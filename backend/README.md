# FallSafe backend

Requires Node.js v26.8.2; uses built-in modules only. No npm install is needed.

From `backend/`:

```sh
npm test
npm start
```

`npm start` uses file-backed SQLite at `./data/fallsafe.db` by default. A new database has no seed records. SIGINT/SIGTERM close the server and database.

Environment configuration lives in `src/config.js`:

| Key | Default |
| --- | --- |
| HOST | 0.0.0.0 |
| PORT | 3000 |
| DB_PATH | ./data/fallsafe.db |
| COUNTDOWN_TIMEOUT_MS | 10000 |
| WATCHDOG_GRACE_MS | 5000 |
| HEARTBEAT_STALE_THRESHOLD_MS | 120000 |
| WATCHDOG_SCAN_INTERVAL_MS | 1000 |

Port 0 is supported for tests. Test fixtures use SQLite `:memory:`. The request body limit is 65,536 bytes.

The API follows `../docs/api-contract.md`. Routes call controllers, services, and SQLite repositories. Numbered migrations preserve the canonical tables and add response fields, lifecycle retry records, and indexes. Watchdog deadlines survive restart; recovery runs at startup and periodically. `createApp()` exposes `listen()`, `close()` and `stop()` for embedding and tests.

The outbox is a local sink: one `RECORDED` row per event, with eligible recipients snapshotted in `recipients_json`. Existing recipient columns are populated for a single eligible recipient and empty for zero or multiple recipients. No SMS/call adapter, delivery claim, or external dispatch is implemented. User IDs partition data using the v1 contract; credential authentication is not implemented.

Heartbeat-only low-battery events use `X-User-Id` when supplied, otherwise the device ID as their scope; the canonical heartbeat payload has no user ID. JSON integer inputs must be exactly representable nonnegative JavaScript safe integers. Runtime timing uses epoch milliseconds, as required for persistent watchdog deadlines.

`TDD.md` records actual RED/GREEN runs. The suite includes real HTTP, file persistence/restart, transactional failure injection, concurrent retries, and a spawned `npm start`/SIGTERM check. Temporary databases are removed by test cleanup.
