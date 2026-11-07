#!/bin/bash
# load_aiven.sh : load the database into a remote MySQL (Aiven) in one step.
# Usage: scripts/load_aiven.sh <host> <port> [database] [path-to-ca.pem]
#        defaults: database defaultdb, certificate ~/Downloads/ca.pem, user avnadmin (override with MYSQL_USER)
#
# It asks for three passwords itself (typing is hidden): the Aiven database password and the new web passwords of
# 'admin' and 'employee'. They are never printed, never typed on a command line and never saved in shell history.
# It must run in a real terminal (the Terminal panel), so it refuses to start anywhere else, before changing anything.
set -e
HOST="$1"; PORT="$2"; DB="${3:-defaultdb}"; CA="${4:-$HOME/Downloads/ca.pem}"

if [ -z "$HOST" ] || [ -z "$PORT" ]; then
    echo "Usage: scripts/load_aiven.sh <host> <port> [database] [path-to-ca.pem]" >&2
    exit 1
fi
if [ ! -t 0 ]; then
    echo "This script has to ask for passwords, so it needs a real terminal." >&2
    echo "Run it in the Terminal panel (not in the chat box)." >&2
    exit 1
fi
if [ ! -f "$CA" ]; then
    echo "CA certificate not found: $CA" >&2
    exit 1
fi

# ask "prompt" VARIABLE : read a hidden value into VARIABLE
ask() { printf '%s' "$1" >&2; read -rs "$2"; echo >&2; }

ask "Aiven database password: " MYSQL_PWD
ask "New password for the web login 'admin' (8+ characters): " IMS_ADMIN_PASSWORD
ask "New password for the web login 'employee' (8+ characters): " IMS_EMPLOYEE_PASSWORD
if [ -z "$MYSQL_PWD" ]; then echo "The Aiven password was empty. Nothing was changed." >&2; exit 1; fi

export MYSQL_HOST="$HOST" MYSQL_PORT="$PORT" MYSQL_USER="${MYSQL_USER:-avnadmin}" MYSQL_SSL_CA="$CA"
export MYSQL_PWD IMS_ADMIN_PASSWORD IMS_EMPLOYEE_PASSWORD
exec "$(dirname "$0")/load_db.sh" "$DB"
