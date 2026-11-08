#!/bin/sh
# warmup.sh : wake the live site (free Render instances sleep when idle) and wait until BOTH the web app and its
# database answer. Run it a few minutes before a demo.
# Usage: scripts/warmup.sh <base-url> [max-minutes]      e.g. scripts/warmup.sh https://inventory-8fp8.onrender.com 8
URL="${1:?Usage: scripts/warmup.sh https://your-app.onrender.com [max-minutes]}"
MAX="${2:-8}"
OUT="$(mktemp)"
trap 'rm -f "$OUT"' EXIT
start="$(date +%s)"
deadline=$((start + MAX * 60))
n=0
echo "Waking $URL (waiting up to $MAX minutes) ..."
while [ "$(date +%s)" -lt "$deadline" ]; do
    n=$((n + 1))
    code="$(curl -s -m 25 -o "$OUT" -w '%{http_code}' "$URL/api/health?deep=1" || true)"
    if [ "$code" = "200" ] && grep -q '"database":"ok"' "$OUT"; then
        echo "READY after $(( $(date +%s) - start )) s: the web app and the database both answer."
        exit 0
    fi
    printf '  try %s: HTTP %s after %s s\n' "$n" "${code:-000}" "$(( $(date +%s) - start ))"
    sleep 5
done
echo "NOT READY after $MAX minutes. Look at the Render and Aiven dashboards." >&2
exit 1
