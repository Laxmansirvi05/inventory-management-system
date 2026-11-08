#!/bin/bash
# migrate_aiven.sh : bring the Aiven database up to date in one step, keeping its data. Asks for the password itself.
# Usage: scripts/migrate_aiven.sh <host> <port> [database] [path-to-ca.pem]     (run in a real terminal)
set -e
HOST="$1"; PORT="$2"; DB="${3:-defaultdb}"; CA="${4:-$HOME/Downloads/ca.pem}"
[ -n "$HOST" ] && [ -n "$PORT" ] || { echo "Usage: scripts/migrate_aiven.sh <host> <port> [database] [path-to-ca.pem]" >&2; exit 1; }
[ -t 0 ] || { echo "This script asks for a password: run it in the Terminal panel." >&2; exit 1; }
[ -f "$CA" ] || { echo "CA certificate not found: $CA" >&2; exit 1; }
printf 'Aiven database password: ' >&2; read -rs MYSQL_PWD; echo >&2
[ -n "$MYSQL_PWD" ] || { echo "Empty password: nothing was changed." >&2; exit 1; }
export MYSQL_HOST="$HOST" MYSQL_PORT="$PORT" MYSQL_USER="${MYSQL_USER:-avnadmin}" MYSQL_SSL_CA="$CA" MYSQL_PWD
exec "$(dirname "$0")/migrate_db.sh" "$DB"
