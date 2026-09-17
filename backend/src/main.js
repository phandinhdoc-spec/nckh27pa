import { createApp } from './app.js';
const app = createApp();
try {
  const address = await app.listen();
  console.log(`FallSafe listening on http://${address.address}:${address.port}`);
} catch (error) { await app.close(); throw error; }
for (const signal of ['SIGINT', 'SIGTERM']) process.once(signal, async () => { await app.close(); });
