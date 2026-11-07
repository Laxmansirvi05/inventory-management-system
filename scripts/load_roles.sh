#!/bin/sh
# load_roles.sh : create the DCL demo accounts (ims_admin, ims_employee) for a database.
# Usage: scripts/load_roles.sh [database_name]   default name: ims   (same MYSQL_* variables as load_db.sh)
# Optional: ROLE_ADMIN_PASSWORD and ROLE_EMPLOYEE_PASSWORD replace the demo passwords written in sql/roles.sql.
# Needs a MySQL account that may create users and grant privileges (Aiven's admin account may be restricted).
set -e
DB="${1:-ims}"
FILE="$(cd "$(dirname "$0")/../sql" && pwd)/roles.sql"

set -- -u "${MYSQL_USER:-root}"
[ -n "$MYSQL_HOST" ] && set -- "$@" -h "$MYSQL_HOST" -P "${MYSQL_PORT:-3306}"
[ -n "$MYSQL_SSL_CA" ] && set -- "$@" --ssl-mode=VERIFY_CA --ssl-ca="$MYSQL_SSL_CA"

sed -e "s/ ims\\./ $DB./g" \
    -e "s/Admin#Demo2026/${ROLE_ADMIN_PASSWORD:-Admin#Demo2026}/" \
    -e "s/Emp#Demo2026/${ROLE_EMPLOYEE_PASSWORD:-Emp#Demo2026}/" "$FILE" | mysql "$@"
echo "Roles created for database '$DB'."
