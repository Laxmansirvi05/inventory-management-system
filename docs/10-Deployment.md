# 10 · Deployment Guide (GitHub → Aiven MySQL → Render)

Everything below the line "Steps" needs **your** accounts (GitHub, Aiven, Render). The files they use are in the repository: `Dockerfile`, `render.yaml`, `scripts/load_db.sh`, `scripts/load_roles.sh`.

## What has been verified, and what has not
| Item | Status |
|---|---|
| Docker image builds (Ubuntu 24.04, 156 MB), runs as a non-root user | **Tested** |
| App in the image against MySQL 8.0.46 in a container, restricted (non-SUPER) account, TLS with CA verification (`TLS_AES_256_GCM_SHA384`) | **Tested** |
| Full API suite (100 checks) against that container | **Tested, 100/100** |
| CA certificate passed as text in `DB_SSL_CA_PEM` | **Tested** |
| `scripts/load_db.sh` in remote mode over SSL | **Tested** |
| Session cookie carries `Secure` when `COOKIE_SECURE=1` | **Tested** |
| Loading the scripts into **Aiven** itself (trigger privilege, see step 3) | **Not tested** (needs your account) |
| Deploying on **Render** (build time and memory on the free plan) | **Not tested** (needs your account) |
| `scripts/load_roles.sh` (GRANT/REVOKE) on Aiven | **Not tested** |
| `scripts/test_on_aiven.sh` (full suites on a throwaway Aiven database) | Tested against local MySQL; **not run on Aiven** (needs your password) |
| PBKDF2 passwords, document-level undo, reload, fair lock, origin check | Tested (core, auth, database, API suites and the Linux image) |

## Steps

### 1. Put the code on GitHub
```bash
git init && git add -A && git commit -m "Inventory Management System"
```
`.gitignore` already excludes `build/`, `.env` and `*.pem`. The demo logins written in `sql/seed.sql` and `tests/api_smoke.py` are for **local use only**: loading a remote database refuses to run without your own passwords (step 3), so the demo passwords never exist on the deployed site.

### 2. Create the Aiven MySQL service
1. Aiven console → create a MySQL service (free plan if available). Wait until it shows **Running**.
2. Note **host, port, user, password, database name** from the service overview (the default database is usually `defaultdb`).
3. Download the **CA certificate** (`ca.pem`) from the same page.
4. Allowed IP addresses: for a demo, allow all (`0.0.0.0/0`); Render's free web services have no fixed outgoing address.

### 3. Load the database
Run this in the **Terminal panel** (a real terminal, not the chat box), from the project folder:
```bash
scripts/load_aiven.sh <aiven host> <aiven port>
```
It asks for three passwords with hidden typing: the Aiven database password, then the new web passwords for `admin` and `employee` (8+ characters each). Nothing is echoed, printed, typed on a command line or saved in shell history. It refuses to start without a real terminal, and the certificate is expected at `~/Downloads/ca.pem` (a fourth argument changes that).

Under the hood it calls `scripts/load_db.sh`: it keeps the database and recreates only the IMS tables, triggers and views, inserts the seed data, then replaces the demo passwords with the two you typed (salted and hashed, like every password in the app). A real remote load starts with the line `Remote server <host>: using existing database ...`; if that line is missing, the settings did not reach the script (it ran against local MySQL instead).

**If it stops with `ERROR 1419 ... SUPER privilege and binary logging is enabled`:** the account may not create triggers. On MySQL 8.0.46 in a container, with a non-SUPER account, both of these worked:
- the server setting `log_bin_trust_function_creators = 1`, or
- granting the account the privilege `SET_USER_ID` (**not usable on newer servers**: a local MySQL 26.x rejected it, and Aiven runs 8.4, so do not count on it).

Whether Aiven lets you do either was **not** checked. Look in the service's advanced settings or ask Aiven support. If neither is possible, the plan's fallback applies (`08-Implementation-Plan.md`, risks): run the demo on local MySQL and show the Aiven part with screenshots.

Optional DCL accounts (needs permission to create users): `ROLE_ADMIN_PASSWORD=... ROLE_EMPLOYEE_PASSWORD=... scripts/load_roles.sh defaultdb`.

### 4. Create the Render web service
1. Render dashboard → **New → Blueprint**, pick the repository (it reads `render.yaml`), or create a Docker **Web Service** by hand with the same variables.
2. Enter the values Render asks for:

| Variable | Value |
|---|---|
| `DB_HOST`, `DB_PORT`, `DB_USER`, `DB_PASSWORD`, `DB_NAME` | from the Aiven overview |
| `DB_SSL_CA_PEM` | the **text** of `ca.pem` (all lines, including BEGIN/END). A single line with `\n` between the lines also works |
| `COOKIE_SECURE` | `1` (already set in `render.yaml`) |
| `SHOP_NAME` | printed on the bill (optional) |

`render.yaml` sets the region to Singapore so the app sits next to an Aiven service in Singapore; if your database is elsewhere, change `region` to match. `PORT` is set by Render itself. The health check is `GET /api/health`.
3. First build compiles the C++ code and can take several minutes. If the build runs out of memory on the free plan, change `-j2` to `-j1` in the `Dockerfile`.

### Upgrading a service that is already deployed (needed once for this release)
This release adds triggers and stricter foreign keys. **Migrate first, then deploy:**
1. `scripts/migrate_aiven.sh <aiven host> <aiven port>` in the Terminal panel. It keeps all data. The version that is live today keeps working on the migrated database (tested with its own suite, 119/119).
2. Render → **Manual Deploy** (`autoDeploy` is off). The new server checks the schema at start; if you deploy first it refuses to start with a clear message and the old version stays live until you migrate.
3. Optional: `scripts/test_on_aiven.sh <host> <port>` runs every suite on a throwaway database on Aiven.
Passwords of existing users are upgraded to PBKDF2 automatically at their next sign-in.

### 5. Smoke-test the live URL
Log in as admin → Dashboard shows 60 products → open **Live SQL** → add a product → make a sale → try a sale with a too-large quantity (admin checkbox) and see error 1644 with ROLLBACK → Reports → Database Model.

### 6. Passwords later
The two web logins are the passwords you chose in step 3. Change them any time under **Users → Reset password**.

## Wake-up checklist (free tiers sleep when idle)
Do this **10 minutes before a demo**:
1. Aiven console: the service says **Running** (power it on if it does not).
2. Run `scripts/warmup.sh https://<your-app>.onrender.com`. It wakes the site and waits until the web app **and** the database both answer (up to 8 minutes), then prints `READY`.
3. Log in, open **Live SQL**, make one sale, undo it.
4. Have the local copy ready as a backup: `scripts/load_db.sh ims`, then `build/ims` and open `http://localhost:8080`.
5. Do not push to GitHub during the demo. `render.yaml` sets `autoDeploy: false`, so a push does not redeploy; to deploy a new version, use Render → Manual Deploy.

## Tools in `scripts/`
| Script | Use |
|---|---|
| `load_db.sh`, `load_aiven.sh` | create the tables, triggers, views and seed data (the second asks for the three passwords itself) |
| `warmup.sh <url>` | wake the live site and wait until app and database answer |
| `backup_db.sh [db] [file]` | save tables, data, triggers and views to one `.sql` file (move to another MySQL, or keep a copy) |
| `test_on_aiven.sh <host> <port>` | run all test suites against a throwaway database on Aiven (asks for your password; drops it afterwards) |
| `run_syllabus_demo.sh [db]` | run `sql/syllabus_demo.sql` |
| `make_mock.py <url>` | refresh the sample data of `?mock=1` (localhost only) |

## If the database is changed outside the app
The server keeps a copy of the products in memory. If you edit the database directly (the Aiven console, a `mysql` session, re-running `load_db.sh`), the dashboard shows a banner saying so, with a **Reload from database** button (admin). Reloading also clears the undo history.

## When the Aiven trial ends (about 6–7 December 2026)
Run `scripts/backup_db.sh defaultdb` against Aiven (same `MYSQL_*` variables as the load script) to save everything, then either upgrade the service plan, or restore the file into any MySQL 8 (`mysql ... <database> < backup.sql`) and point `DB_HOST`, `DB_PORT`, `DB_USER`, `DB_PASSWORD`, `DB_NAME` and `DB_SSL_CA_PEM` of the Render service at it.

## Fallback: run everything locally
```bash
scripts/load_db.sh ims && cmake -B build && cmake --build build && build/ims
```

## How the image was tested (to repeat it)
```bash
docker network create ims-net
docker run -d --name ims-mysql-test --network ims-net -p 127.0.0.1:3307:3306 -e MYSQL_ROOT_PASSWORD=<test password> -e MYSQL_DATABASE=ims mysql:8.0
docker cp ims-mysql-test:/var/lib/mysql/ca.pem ./test-ca.pem          # the container's own CA
# create a non-SUPER user, load with scripts/load_db.sh (remote mode), then:
docker build -t ims-app .
docker run -d --name ims-app-test --network ims-net -p 127.0.0.1:8098:8080 -e DB_HOST=ims-mysql-test -e DB_USER=<user> \
  -e DB_PASSWORD=<password> -e DB_NAME=ims -e DB_SSL_CA_PEM="$(cat test-ca.pem)" -e COOKIE_SECURE=0 ims-app
python3 tests/api_smoke.py http://127.0.0.1:8098
```
(`COOKIE_SECURE=0` only because the test client uses plain HTTP.)
