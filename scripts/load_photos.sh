#!/bin/sh
# load_photos.sh : store the 60 product pictures (sql/photos.sql) in an existing database, without reloading anything else.
# Usage: scripts/load_photos.sh [database_name]      default name: ims
# Same connection settings as load_db.sh: MYSQL_USER, MYSQL_PWD (environment), and for a remote server
# MYSQL_HOST, MYSQL_PORT, MYSQL_SSL_CA. Restart the app (or press "Reload from database") afterwards so it sees the photos.
set -e
DB="${1:-ims}"
DIR="$(cd "$(dirname "$0")/../sql" && pwd)"
set -- -u "${MYSQL_USER:-root}"
[ -n "$MYSQL_HOST" ] && set -- "$@" -h "$MYSQL_HOST" -P "${MYSQL_PORT:-3306}"
[ -n "$MYSQL_SSL_CA" ] && set -- "$@" --ssl-mode=VERIFY_CA --ssl-ca="$MYSQL_SSL_CA"
mysql "$@" "$DB" < "$DIR/photos.sql"
mysql "$@" "$DB" -e "SELECT COUNT(*) AS products_with_photo FROM Products WHERE photo IS NOT NULL"
