#!/usr/bin/env bash
# Step 8b: PostgreSQL backup. Run from the directory that holds compose.yaml and .env
# (infra/ on the VPS). Writes a compressed dump to ./backups and keeps the last 14 days.
#
# Assumes the compose service is named "db" and .env defines POSTGRES_USER and POSTGRES_DB
# (see .env.example). Adapt SERVICE if Alae names it differently.
#
# Cron example, every night at 03:15:
#   15 3 * * * cd /opt/smart-football/infra && ./backup-postgres.sh >> backups/backup.log 2>&1
#
# Restore:
#   gunzip -c backups/smartfootball_YYYYmmdd_HHMMSS.sql.gz | \
#     docker compose exec -T db psql -U "$POSTGRES_USER" -d "$POSTGRES_DB"
set -euo pipefail

SERVICE="${SERVICE:-db}"
BACKUP_DIR="${BACKUP_DIR:-./backups}"
KEEP_DAYS="${KEEP_DAYS:-14}"

if [ -f .env ]; then
  set -a
  # shellcheck disable=SC1091
  . ./.env
  set +a
fi

: "${POSTGRES_USER:?POSTGRES_USER is not set}"
: "${POSTGRES_DB:?POSTGRES_DB is not set}"

mkdir -p "$BACKUP_DIR"
chmod 700 "$BACKUP_DIR"

stamp="$(date +%Y%m%d_%H%M%S)"
target="$BACKUP_DIR/smartfootball_${stamp}.sql.gz"

docker compose exec -T "$SERVICE" pg_dump -U "$POSTGRES_USER" -d "$POSTGRES_DB" | gzip > "$target"

# An empty dump means pg_dump failed: do not keep it.
if [ ! -s "$target" ] || [ "$(gzip -dc "$target" | wc -c)" -eq 0 ]; then
  rm -f "$target"
  echo "Backup failed: empty dump." >&2
  exit 1
fi

find "$BACKUP_DIR" -name 'smartfootball_*.sql.gz' -mtime +"$KEEP_DAYS" -delete
echo "Backup written: $target"
