#!/bin/sh
# migrate_db.sh : update an EXISTING database (made by an earlier version) to the current schema, keeping its data.
# Usage: scripts/migrate_db.sh [database]      default: ims   (same MYSQL_* variables as load_db.sh)
# Run it BEFORE starting a newer server on that database: the server checks the schema at start-up and refuses an old one.
set -e
DB="${1:-ims}"
DIR="$(cd "$(dirname "$0")/../sql" && pwd)"
set -- -u "${MYSQL_USER:-root}"
[ -n "$MYSQL_HOST" ] && set -- "$@" -h "$MYSQL_HOST" -P "${MYSQL_PORT:-3306}"
[ -n "$MYSQL_SSL_CA" ] && set -- "$@" --ssl-mode=VERIFY_CA --ssl-ca="$MYSQL_SSL_CA"
mysql "$@" "$DB" < "$DIR/migrate.sql"
echo "  foreign keys updated (RESTRICT)"
mysql "$@" "$DB" < "$DIR/triggers.sql"
echo "  triggers re-created (7)"
echo "Database '$DB' is up to date."
