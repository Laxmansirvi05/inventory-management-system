# Inventory Management System (ADS + DBMS)
C++ backend · HTML/CSS/JS frontend · MySQL database (local or Aiven) · deployed on Render.

> **Status:** database scripts, C++ core, database layer, REST API and the web frontend are built and tested (phases 0-6). Deployment files are ready and tested in Docker; the Aiven and Render steps need your accounts (see the deployment guide). See `docs/`.

## Features (planned)
Login (Admin/Employee) · product CRUD, search, sort · purchases and sales with stock triggers · FIFO order queue · low-stock priority alerts · undo · reports · **DBMS Lab** (ER model, query explorer, live SQL panel, trigger/transaction/constraint/DCL/index/BLOB demos).

## Documents
1. [PRD](docs/01-PRD.md) 2. [TRD](docs/02-TRD.md) 3. [API spec](docs/03-API-Spec.md) 4. [Database design](docs/04-Database-Design.md) 5. [Database architecture & workflow](docs/05-Database-Architecture-and-Workflow.md) 6. [Test plan](docs/06-Test-Plan.md) 7. [Syllabus mapping](docs/07-Syllabus-Mapping.md) 8. [Implementation plan](docs/08-Implementation-Plan.md) 9. [Frontend plan](docs/09-Frontend-Plan.md) 10. [Deployment guide](docs/10-Deployment.md)

## Run locally
1. Install MySQL 8 (includes the `libmysqlclient` C library), a C++17 compiler and CMake.
2. Create the database: `scripts/load_db.sh ims` (runs `schema.sql`, `triggers.sql`, `views.sql`, `seed.sql`; `roles.sql` is optional and creates server-wide accounts).
3. Set environment variables (all optional locally; defaults are `127.0.0.1:3306`, user `root`, database `ims`, port `8080`): `DB_HOST DB_PORT DB_USER DB_PASSWORD DB_NAME DB_SSL_CA PORT PUBLIC_DIR SHOP_NAME COOKIE_SECURE`.
4. `cmake -B build && cmake --build build` then run `build/ims` and open `http://localhost:8080`.

## Demo without a server
Open any page with `?mock=1` (for example `/login.html?mock=1`) to use the sample data in `public/mock/` instead of the API; nothing is saved. Use `?mock=0` to leave. Regenerate the sample files from a running test server with `python3 scripts/make_mock.py http://127.0.0.1:8099`.

## Tests
- `build/core_tests`: data structures and stock logic (no database).
- `scripts/load_db.sh ims_test && DB_NAME=ims_test build/db_tests`: database layer.
- API: start `DB_NAME=ims_test PORT=8099 build/ims`, then `python3 tests/api_smoke.py http://127.0.0.1:8099`.
These write data, so they must only run against a database whose name ends in `_test`. Reload it (`scripts/load_db.sh ims_test`) before each suite, because every suite expects the original seed data.

## Deploy
Aiven MySQL for the database, a Render Docker web service for the app. Step-by-step guide, tested parts, known risks and the wake-up checklist: [docs/10-Deployment.md](docs/10-Deployment.md). Free tiers sleep when idle: open both before a demo.

## Team
D. Yashwin (25R11A66U2) · K. Krithik (25R11A66V2) · Lakshman (25R11A66V5) — Geethanjali College of Engineering and Technology, CSE.
