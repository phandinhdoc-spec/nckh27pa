import { createServer } from 'node:http';
import { loadConfig, BODY_LIMIT_BYTES } from './config.js';
import { openDatabase } from './repositories/db.js';
import { startWatchdog } from './services/alertStateMachine.js';
import { ApiError } from './services/errors.js';
import { controller, respond } from './controllers/controller.js';
import { route } from './routes/router.js';

function readBody(req) {
  return new Promise((resolve, reject) => {
    let size = 0;
    let settled = false;
    const chunks = [];
    function fail(message) {
      if (settled) return;
      settled = true;
      req.removeListener('data', onData);
      req.resume();
      reject(new ApiError(400, 'VALIDATION_ERROR', message));
    }
    function onData(chunk) {
      size += chunk.length;
      if (size > BODY_LIMIT_BYTES) return fail('Request body exceeds 65536 bytes');
      chunks.push(chunk);
    }
    req.on('data', onData);
    req.on('error', () => fail('Request stream interrupted'));
    req.once('aborted', () => fail('Request stream interrupted'));
    req.once('end', () => {
      if (settled) return;
      if (!size) { settled = true; resolve({}); return; }
      try {
        const value = JSON.parse(new TextDecoder('utf-8', { fatal: true }).decode(Buffer.concat(chunks)));
        if (!value || typeof value !== 'object' || Array.isArray(value)) return fail('Expected JSON object');
        settled = true;
        resolve(value);
      } catch { fail('Malformed JSON object'); }
    });
    if (Number(req.headers['content-length']) > BODY_LIMIT_BYTES) fail('Request body exceeds 65536 bytes');
  });
}

export function createApp(options = {}) {
  const config = loadConfig(process.env, options.config);
  const now = options.now ?? Date.now;
  const db = openDatabase(config.DB_PATH);
  const watchdog = startWatchdog({ db, config, now });
  const actions = controller({ db, config, now, started: now(), watchdog });
  const server = createServer(async (req, res) => {
    try {
      const body = await readBody(req);
      const result = await route(req.method, new URL(req.url, 'http://localhost'), actions, body, req.headers);
      respond(res, result.status ?? 200, result.data, now());
    } catch (error) {
      if (res.destroyed) return;
      // End the connection after a rejected body, including an unfinished upload.
      if (!req.complete) res.setHeader('connection', 'close');
      respond(res, error instanceof ApiError ? error.status : 500, {
        code: error instanceof ApiError ? error.code : 'INTERNAL_SERVER_ERROR',
        message: error instanceof ApiError ? error.message : 'Internal server error',
      }, now(), true);
    }
  });
  server.on('clientError', (error, socket) => {
    if (!socket.writable) return socket.destroy();
    const body = JSON.stringify({ success: false, error: { code: 'VALIDATION_ERROR', message: 'Malformed HTTP request' }, timestamp: now() });
    socket.end(`HTTP/1.1 400 Bad Request\r\nContent-Type: application/json; charset=utf-8\r\nContent-Length: ${Buffer.byteLength(body)}\r\nConnection: close\r\n\r\n${body}`);
  });
  let closing;
  function close() {
    if (closing) return closing;
    watchdog.stop();
    closing = (async () => {
      if (server.listening) {
        await new Promise((resolve, reject) => {
          server.close(error => error ? reject(error) : resolve());
          server.closeAllConnections();
        });
      }
      db.close();
    })();
    return closing;
  }
  return {
    db, server, config, close, stop: close,
    listen: () => new Promise((resolve, reject) => {
      if (closing) return reject(new Error('Application is closed'));
      server.once('error', reject);
      server.listen(config.PORT, config.HOST, () => {
        server.removeListener('error', reject);
        resolve(server.address());
      });
    }),
  };
}
