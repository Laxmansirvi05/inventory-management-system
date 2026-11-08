#!/bin/bash
# test_on_aiven.sh : run the whole test suite against a THROWAWAY database on the remote MySQL (Aiven), so the real
# data is never touched. It creates ims_aiven_test, loads it, starts a local copy of the server on it (over TLS),
# runs the database-layer, API and Live SQL tests, then drops ims_aiven_test again.
# Usage: scripts/test_on_aiven.sh <host> <port> [path-to-ca.pem]       (run it in a real terminal; it asks for the password)
# Needs the project built (build/ims, build/db_tests; override the folder with IMS_BUILD_DIR) and python3.
set -e
HOST="$1"; PORT="$2"; CA="${3:-$HOME/Downloads/ca.pem}"
BUILD="${IMS_BUILD_DIR:-build}"
SCRATCH="ims_aiven_test"                       # the ONLY database this script creates or drops
WEBPORT=8097
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

[ -n "$HOST" ] && [ -n "$PORT" ] || { echo "Usage: scripts/test_on_aiven.sh <host> <port> [path-to-ca.pem]" >&2; exit 1; }
[ -t 0 ] || { echo "This script asks for a password: run it in the Terminal panel." >&2; exit 1; }
[ -f "$CA" ] || { echo "CA certificate not found: $CA" >&2; exit 1; }
for f in "$BUILD/ims" "$BUILD/db_tests"; do [ -x "$ROOT/$f" ] || [ -x "$f" ] || { echo "Missing $f: build the project first (cmake -B build && cmake --build build)." >&2; exit 1; }; done
BIN_DIR="$ROOT/$BUILD"; [ -x "$BIN_DIR/ims" ] || BIN_DIR="$BUILD"

printf 'Aiven database password: ' >&2; read -rs MYSQL_PWD; echo >&2
[ -n "$MYSQL_PWD" ] || { echo "Empty password: nothing was changed." >&2; exit 1; }
export MYSQL_HOST="$HOST" MYSQL_PORT="$PORT" MYSQL_USER="${MYSQL_USER:-avnadmin}" MYSQL_SSL_CA="$CA" MYSQL_PWD
export IMS_ADMIN_PASSWORD="Aa1-$(openssl rand -hex 8)" IMS_EMPLOYEE_PASSWORD="Ee1-$(openssl rand -hex 8)"   # random, used only by this run
MYSQL_ARGS="-u $MYSQL_USER -h $MYSQL_HOST -P $MYSQL_PORT --ssl-mode=VERIFY_CA --ssl-ca=$MYSQL_SSL_CA"

SERVER_PID=""
cleanup() {
    [ -n "$SERVER_PID" ] && kill "$SERVER_PID" 2>/dev/null || true
    mysql $MYSQL_ARGS -e "DROP DATABASE IF EXISTS $SCRATCH" 2>/dev/null && echo "Scratch database $SCRATCH dropped." || echo "Could not drop $SCRATCH: remove it by hand." >&2
}
trap cleanup EXIT

echo "== creating $SCRATCH on $MYSQL_HOST and loading the project into it"
mysql $MYSQL_ARGS -e "DROP DATABASE IF EXISTS $SCRATCH; CREATE DATABASE $SCRATCH CHARACTER SET utf8mb4"
"$ROOT/scripts/load_db.sh" "$SCRATCH" | tail -3

export DB_HOST="$HOST" DB_PORT="$PORT" DB_USER="$MYSQL_USER" DB_PASSWORD="$MYSQL_PWD" DB_NAME="$SCRATCH" DB_SSL_CA="$CA"
echo "== database layer tests (real transactions, triggers, lost-connection recovery)"
"$BIN_DIR/db_tests" | tail -1
"$ROOT/scripts/load_db.sh" "$SCRATCH" >/dev/null           # db_tests leaves a product behind: start the API tests clean

echo "== starting the server on $SCRATCH (TLS, certificate checked)"
PORT=$WEBPORT "$BIN_DIR/ims" > "${TMPDIR:-/tmp}/ims-aiven-test.log" 2>&1 &
SERVER_PID=$!
for i in $(seq 1 30); do curl -s "http://127.0.0.1:$WEBPORT/api/health?deep=1" | grep -q '"database":"ok"' && break; sleep 1; done
curl -s "http://127.0.0.1:$WEBPORT/api/health?deep=1" | grep -q '"database":"ok"' || { echo "Server did not come up; see ${TMPDIR:-/tmp}/ims-aiven-test.log" >&2; exit 1; }

echo "== API tests"
python3 "$ROOT/tests/api_smoke.py" "http://127.0.0.1:$WEBPORT" | tail -1
"$ROOT/scripts/load_db.sh" "$SCRATCH" >/dev/null
kill "$SERVER_PID" 2>/dev/null; wait "$SERVER_PID" 2>/dev/null || true
PORT=$WEBPORT "$BIN_DIR/ims" > "${TMPDIR:-/tmp}/ims-aiven-test.log" 2>&1 &
SERVER_PID=$!
for i in $(seq 1 30); do curl -s "http://127.0.0.1:$WEBPORT/api/health" | grep -q '"status":"up"' && break; sleep 1; done
echo "== Live SQL coverage"
python3 "$ROOT/tests/livesql_coverage.py" "http://127.0.0.1:$WEBPORT" | tail -1
echo "== done: every number above should read all-passed / 0 problems"
