#!/bin/sh
# backup_db.sh : save a whole database (tables, data, triggers, views) into one .sql file, to keep a copy or to move to
# another MySQL (for example when the Aiven trial ends).
# Usage: scripts/backup_db.sh [database] [output-file]      default: ims, backup-<database>-<date>.sql
# Same MYSQL_* variables as load_db.sh (MYSQL_HOST, MYSQL_PORT, MYSQL_USER, MYSQL_SSL_CA, MYSQL_PWD).
# The file contains the password hashes of your users: keep it private (backup-*.sql is ignored by git).
# Restore into an empty database:  mysql <same connection options> <database> < <file>
set -e
DB="${1:-ims}"
OUT="${2:-backup-$DB-$(date +%Y%m%d-%H%M%S).sql}"
set -- -u "${MYSQL_USER:-root}"
[ -n "$MYSQL_HOST" ] && set -- "$@" -h "$MYSQL_HOST" -P "${MYSQL_PORT:-3306}"
[ -n "$MYSQL_SSL_CA" ] && set -- "$@" --ssl-mode=VERIFY_CA --ssl-ca="$MYSQL_SSL_CA"
umask 077
mysqldump "$@" --single-transaction --routines --triggers --no-tablespaces --set-gtid-purged=OFF "$DB" > "$OUT"
echo "Saved $OUT ($(wc -c < "$OUT") bytes)."
