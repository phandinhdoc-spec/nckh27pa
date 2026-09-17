STATUS: DONE
OWNER: Codex
FILES: backend/**
GOAL: Phase 3 canonical SQLite/HTTP backend
RESULT: All required behavior implemented and verified using real HTTP and SQLite.
NEXT_ACTION: Owner review; no pending implementation blocker.

All counts are from `npm test` (`npm test --prefix backend` when invoked at repository root).

| Slice | RED evidence | GREEN evidence |
| --- | --- | --- |
| Initial database/HTTP/envelopes | Missing app module: 1 test, 0 pass, 1 fail. Initial socket attempt hit EPERM. | Host capability restored; locally rerun: 1 test, 1 pass, 0 fail. |
| Device heartbeat/staleness/low battery | Missing routes: 3 tests, 1 pass, 2 fail. | 3 tests, 3 pass, 0 fail. |
| Sensor ingest/latest/dedup/validation | Missing routes: 5 tests, 3 pass, 2 fail. | 5 tests, 5 pass, 0 fail. |
| Events ingest/read/filter/validation | Missing routes: 7 tests, 5 pass, 2 fail. | 7 tests, 7 pass, 0 fail. |
| Contacts CRUD/primary/toggle/isolation/validation | Missing routes: 10 tests, 7 pass, 3 fail. | 10 tests, 10 pass, 0 fail. |
| Active alerts/cancellation | Missing routes: 12 tests, 10 pass, 2 fail. | 12 tests, 12 pass, 0 fail. |
| SOS/manual SOS/transactional outbox | Missing SOS route: 15 tests, 12 pass, 3 fail. | 15 tests, 15 pass, 0 fail. |
| ACK/resolve and SOS rollback | Missing ACK/resolve routes: 18 tests, 16 pass, 2 fail; rollback test already passed. | 18 tests, 18 pass, 0 fail. |
| Durable startup/periodic watchdog and failure recovery | No scanner: 21 tests, 18 pass, 3 fail (overdue state unchanged and polling timed out). | 21 tests, 21 pass, 0 fail. |
| Transport and managed-field validation | Bad URL returned 500; invalid dispatch accepted: 24 tests, 22 pass, 2 fail. | 24 tests, 24 pass, 0 fail. |
| Lifecycle/streaming/config/file runtime | After fixing test cleanup order: 29 tests, 26 pass, 3 fail (shutdown delay, no early size rejection, empty PORT accepted). File restart and npm start probes already passed. | 29 tests, 29 pass, 0 fail. |
| Overdue cancellation race/concurrent retries/late transaction failure | Late cancel returned 200: 32 tests, 31 pass, 1 fail. Concurrent retries and post-insert rollback already passed. | 32 tests, 32 pass, 0 fail. |

Final verification: `npm test` from `backend/`: 32 tests, 32 pass, 0 fail, 0 skipped. Includes `npm start` with file-backed SQLite, real HTTP health, SIGTERM shutdown, and refused connection after shutdown.

## Review correction: nullable gyroscope group

- RED (`npm test` from backend): 34 tests, 33 pass, 1 fail. The new real HTTP PHONE/null sample returned 400 `VALIDATION_ERROR`, message `Invalid gyroXDps`, instead of 200.
- GREEN (`npm test --prefix backend`): 34 tests, 34 pass, 0 fail. Explicit null gyro axes round-trip through SQLite and HTTP; missing, mixed and invalid axes are rejected. Metadata/accelerometer fields remain required/non-null.
- Updated initial pre-release schema directly; no runtime rebuild migration or seed data. Existing repository null mapping required no code change.
- Both documentation JSON checks pass: canonical 32/32, ESP32 11/11; 43 valid blocks, 0 invalid.
- Final correction suite: 34 tests, 34 pass, 0 fail, 0 skipped.
