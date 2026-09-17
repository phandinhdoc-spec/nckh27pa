import { DatabaseSync } from 'node:sqlite';
import { readFileSync, mkdirSync, readdirSync } from 'node:fs';
import { dirname } from 'node:path';
export function openDatabase(path) {
  if (path !== ':memory:') mkdirSync(dirname(path), { recursive: true });
  const db = new DatabaseSync(path);
  try {
    db.exec('PRAGMA foreign_keys = ON; PRAGMA busy_timeout = 5000; PRAGMA journal_mode = WAL;');
    db.exec(readFileSync(new URL('../../migrations/001_initial_schema.sql', import.meta.url), 'utf8'));
    db.exec('CREATE TABLE IF NOT EXISTS schema_migrations (name TEXT PRIMARY KEY)');
    const directory = new URL('../../migrations/', import.meta.url);
    for (const name of readdirSync(directory).filter(n => n.endsWith('.sql')).sort()) {
      if (db.prepare('SELECT 1 FROM schema_migrations WHERE name = ?').get(name)) continue;
      transaction(db, () => {
        db.exec(readFileSync(new URL(name, directory), 'utf8'));
        db.prepare('INSERT INTO schema_migrations VALUES (?)').run(name);
      });
    }
    return db;
  } catch (error) { db.close(); throw error; }
}
export function transaction(db, work) {
  db.exec('BEGIN IMMEDIATE');
  try { const result = work(); db.exec('COMMIT'); return result; }
  catch (error) { db.exec('ROLLBACK'); throw error; }
}
