# Inventory Management System (ADS + DBMS)
C++ backend · HTML/CSS/JS frontend · MySQL database (local or Aiven) · deployed on Render.

> **Status:** database scripts, C++ core, database layer, REST API and the web frontend are built and tested (phases 0-6). Deployment files are ready and tested in Docker; the Aiven and Render steps need your accounts (see the deployment guide). See `docs/`.

## Features
Login (Admin/Employee) · product CRUD, search, sort, photos · purchases and sales with stock triggers · FIFO order queue · low-stock priority alerts · undo · reports · printable bills · user management · **Live SQL panel** (the SQL, results, trigger notes and before/after rows behind every action) · Database Model page · runnable syllabus demo (`sql/syllabus_demo.sql`).

## Documents
1. [PRD](docs/01-PRD.md) 2. [TRD](docs/02-TRD.md) 3. [API spec](docs/03-API-Spec.md) 4. [Database design](docs/04-Database-Design.md) 5. [Database architecture & workflow](docs/05-Database-Architecture-and-Workflow.md) 6. [Test plan](docs/06-Test-Plan.md) 7. [Syllabus mapping](docs/07-Syllabus-Mapping.md) 8. [Implementation plan](docs/08-Implementation-Plan.md) 9. [Frontend plan](docs/09-Frontend-Plan.md) 10. [Deployment guide](docs/10-Deployment.md) 11. [Audit report](docs/11-Audit-Report.md) 12. [Syllabus demo output](docs/12-Syllabus-Demo-Output.md) 13. [Viva and demo guide](docs/13-Viva-Guide.md)

## Run locally
1. Install MySQL 8 (includes the `libmysqlclient` C library), a C++17 compiler and CMake.
2. Create the database: `scripts/load_db.sh ims` (runs `schema.sql`, `triggers.sql`, `views.sql`, `seed.sql`, `photos.sql` (a picture for each of the 60 products; `scripts/load_photos.sh` adds them to an existing database, `scripts/photos_aiven.sh <host> <port>` to the Aiven one); `roles.sql` is optional and creates server-wide accounts). An older copy: `scripts/migrate_db.sh ims`.
3. Set environment variables (all optional locally; defaults are `127.0.0.1:3306`, user `root`, database `ims`, port `8080`): `DB_HOST DB_PORT DB_USER DB_PASSWORD DB_NAME DB_SSL_CA PORT PUBLIC_DIR SHOP_NAME COOKIE_SECURE`.
4. `cmake -B build && cmake --build build` then run `build/ims` and open `http://localhost:8080`. The landing page is at `/`; signing in leads to `dashboard.html`.

## Demo without a server
Open any page on **localhost** with `?mock=1` (for example `http://localhost:8080/login.html?mock=1`) to use the sample data in `public/mock/` instead of the API; nothing is saved. Use `?mock=0` to leave. Mock mode is ignored on any other host. Refresh the sample files from a running test server with `python3 scripts/make_mock.py http://127.0.0.1:8099`.

## Tests
All of these write data, so they run only against a database whose name ends in `_test`; reload it (`scripts/load_db.sh ims_test`) before each suite, because every suite expects the original seed data.
- `build/core_tests`: data structures, stock rules, whole-document undo, the fair lock (no database).
- `build/auth_tests`: password hashing (PBKDF2 and the older format), sessions, login throttle (no database).
- `DB_NAME=ims_test build/db_tests`: the database layer on real MySQL (transactions, triggers, constraints, undo, lost-connection recovery).
- API and Live SQL: start `DB_NAME=ims_test PORT=8099 build/ims`, then `python3 tests/api_smoke.py http://127.0.0.1:8099`, reload `ims_test`, restart the server, then `python3 tests/livesql_coverage.py http://127.0.0.1:8099`. The second script also checks every statement the Live SQL panel shows against MySQL itself: each SELECT is run again and its columns, row count and rows must match; each INSERT/UPDATE/DELETE must pass `EXPLAIN`; every transaction must end.
- Everything on Aiven, in a throwaway database that is dropped afterwards: `scripts/test_on_aiven.sh <host> <port>` (asks for the password).

## Upgrading a database made by an earlier version
`scripts/migrate_db.sh <database>` (or `scripts/migrate_aiven.sh <host> <port>`) updates the schema and keeps the data; it is safe to run twice. A newer server refuses to start on an older schema and says so. On an existing deployment migrate the database first, then deploy (see `docs/10-Deployment.md`).

## Deploy
Aiven MySQL for the database, a Render Docker web service for the app. Step-by-step guide, tested parts, known risks and the wake-up checklist: [docs/10-Deployment.md](docs/10-Deployment.md). Free tiers sleep when idle: open both before a demo.

