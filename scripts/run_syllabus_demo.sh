#!/bin/sh
# run_syllabus_demo.sh : run sql/syllabus_demo.sql and show every statement with its result.
# Usage: scripts/run_syllabus_demo.sh [database]      default: ims   (same MYSQL_* variables as load_db.sh)
# --force keeps going after the statements that are MEANT to fail (constraints, oversell).
# Data is unchanged afterwards: changes are rolled back or made on a temporary table.
set -e
DB="${1:-ims}"
FILE="$(cd "$(dirname "$0")/../sql" && pwd)/syllabus_demo.sql"
set -- -u "${MYSQL_USER:-root}"
[ -n "$MYSQL_HOST" ] && set -- "$@" -h "$MYSQL_HOST" -P "${MYSQL_PORT:-3306}"
[ -n "$MYSQL_SSL_CA" ] && set -- "$@" --ssl-mode=VERIFY_CA --ssl-ca="$MYSQL_SSL_CA"
mysql "$@" --table --force -vv "$DB" < "$FILE" 2>&1
