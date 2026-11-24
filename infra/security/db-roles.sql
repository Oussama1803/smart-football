-- Step 8b: least-privilege PostgreSQL roles for Smart Football.
-- Run once, as the superuser created by the postgres image (POSTGRES_USER), against POSTGRES_DB.
-- Passwords are passed as psql variables, never written in this file:
--   docker compose exec -T db psql -U "$POSTGRES_USER" -d "$POSTGRES_DB" \
--     -v ON_ERROR_STOP=1 -v app_password="$DB_APP_PASSWORD" -v backup_password="$DB_BACKUP_PASSWORD" \
--     < ../infra/security/db-roles.sql
--
-- Roles:
--   sf_owner   owns the tables, NOLOGIN. Schema changes are made with SET ROLE sf_owner.
--   sf_app     used by the FastAPI backend: read and insert only, no UPDATE/DELETE/DDL.
--   sf_backup  used by backup-postgres.sh: read only.

\set ON_ERROR_STOP on

CREATE ROLE sf_owner NOLOGIN;
CREATE ROLE sf_app LOGIN PASSWORD :'app_password';
CREATE ROLE sf_backup LOGIN PASSWORD :'backup_password';

-- Nobody but the owner creates objects in the public schema.
REVOKE CREATE ON SCHEMA public FROM PUBLIC;
GRANT USAGE, CREATE ON SCHEMA public TO sf_owner;
GRANT USAGE ON SCHEMA public TO sf_app, sf_backup;

REVOKE ALL ON DATABASE :"DBNAME" FROM PUBLIC;
GRANT CONNECT ON DATABASE :"DBNAME" TO sf_app, sf_backup;

-- Tables that already exist (created by the backend's init_db before this script).
DO $$
DECLARE t record;
BEGIN
  FOR t IN SELECT tablename FROM pg_tables WHERE schemaname = 'public' LOOP
    EXECUTE format('ALTER TABLE public.%I OWNER TO sf_owner', t.tablename);
  END LOOP;
END $$;

DO $$
DECLARE s record;
BEGIN
  FOR s IN SELECT sequencename FROM pg_sequences WHERE schemaname = 'public' LOOP
    EXECUTE format('ALTER SEQUENCE public.%I OWNER TO sf_owner', s.sequencename);
  END LOOP;
END $$;

GRANT SELECT, INSERT ON ALL TABLES IN SCHEMA public TO sf_app;
GRANT USAGE ON ALL SEQUENCES IN SCHEMA public TO sf_app;
GRANT SELECT ON ALL TABLES IN SCHEMA public TO sf_backup;
GRANT SELECT ON ALL SEQUENCES IN SCHEMA public TO sf_backup;

-- Tables created later by sf_owner get the same rights automatically.
ALTER DEFAULT PRIVILEGES FOR ROLE sf_owner IN SCHEMA public GRANT SELECT, INSERT ON TABLES TO sf_app;
ALTER DEFAULT PRIVILEGES FOR ROLE sf_owner IN SCHEMA public GRANT USAGE ON SEQUENCES TO sf_app;
ALTER DEFAULT PRIVILEGES FOR ROLE sf_owner IN SCHEMA public GRANT SELECT ON TABLES TO sf_backup;
ALTER DEFAULT PRIVILEGES FOR ROLE sf_owner IN SCHEMA public GRANT SELECT ON SEQUENCES TO sf_backup;
