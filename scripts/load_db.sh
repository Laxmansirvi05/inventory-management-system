#!/bin/sh
# load_db.sh : load schema, triggers, views and seed data into a database.
# Usage: scripts/load_db.sh [database_name]      default name: ims
#
# Local MySQL: the database is dropped and created again. Set MYSQL_USER (default root) and MYSQL_PWD if needed.
# Remote MySQL (for example Aiven): set MYSQL_HOST (+ MYSQL_PORT, MYSQL_USER, MYSQL_SSL_CA = path to the CA certificate)
#   and the password in MYSQL_PWD (an environment variable, never on the command line). The database must already
#   exist and is NOT dropped; only the IMS tables, triggers and views in it are recreated, then the seed data is inserted.
# Passwords of the two login accounts (as environment variables, never on the command line):
#   IMS_ADMIN_PASSWORD and IMS_EMPLOYEE_PASSWORD (8+ characters). They replace the demo passwords from sql/seed.sql.
#   Optional for a local database; REQUIRED for a remote one, so the public demo passwords never exist online.
# roles.sql is separate: see scripts/load_roles.sh.
set -e
DB="${1:-ims}"
DIR="$(cd "$(dirname "$0")/../sql" && pwd)"

# Check the passwords before touching the database.
if [ -n "$MYSQL_HOST" ] && { [ -z "$IMS_ADMIN_PASSWORD" ] || [ -z "$IMS_EMPLOYEE_PASSWORD" ]; }; then
    echo "Refusing to load a remote database with the public demo passwords." >&2
    echo "Set IMS_ADMIN_PASSWORD and IMS_EMPLOYEE_PASSWORD (8+ characters each) and run again." >&2
    exit 1
fi
for v in "$IMS_ADMIN_PASSWORD" "$IMS_EMPLOYEE_PASSWORD"; do
    if [ -n "$v" ] && [ "${#v}" -lt 8 ]; then echo "Passwords must have at least 8 characters." >&2; exit 1; fi
done

# "salt$sha256hex(salt+password)": the same format the server checks at login.
hash_pw() {
    salt="$(openssl rand -hex 8)"
    printf '%s$%s' "$salt" "$(printf '%s%s' "$salt" "$1" | openssl dgst -sha256 | awk '{print $NF}')"
}

set -- -u "${MYSQL_USER:-root}"
[ -n "$MYSQL_HOST" ] && set -- "$@" -h "$MYSQL_HOST" -P "${MYSQL_PORT:-3306}"
[ -n "$MYSQL_SSL_CA" ] && set -- "$@" --ssl-mode=VERIFY_CA --ssl-ca="$MYSQL_SSL_CA"

if [ -z "$MYSQL_HOST" ]; then
    mysql "$@" -e "DROP DATABASE IF EXISTS \`$DB\`; CREATE DATABASE \`$DB\` CHARACTER SET utf8mb4;"
else
    echo "Remote server $MYSQL_HOST: using existing database '$DB' (tables are recreated, the database is kept)."
fi
for f in schema triggers views seed; do
    mysql "$@" "$DB" < "$DIR/$f.sql"
    echo "  loaded $f.sql"
done
if [ -n "$IMS_ADMIN_PASSWORD" ]; then
    mysql "$@" "$DB" -e "UPDATE Users SET password_hash = '$(hash_pw "$IMS_ADMIN_PASSWORD")' WHERE username = 'admin'"
    echo "  admin password set"
fi
if [ -n "$IMS_EMPLOYEE_PASSWORD" ]; then
    mysql "$@" "$DB" -e "UPDATE Users SET password_hash = '$(hash_pw "$IMS_EMPLOYEE_PASSWORD")' WHERE username = 'employee'"
    echo "  employee password set"
fi
echo "Database '$DB' loaded."
