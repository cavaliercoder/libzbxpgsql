SET ROLE postgres;

-- If there are slaves and this DB is NOT in recovery,
-- then issue a fake notify command to force the log
-- to stream. This will update pg_last_xact_replay_timestamp
-- on all slaves.

BEGIN;

  CREATE OR REPLACE FUNCTION replication_pump()
    RETURNS void
    AS $$
      DECLARE slavect int;
      BEGIN
        SELECT count(*) INTO slavect FROM pg_stat_replication;
        IF slavect > 0 AND pg_is_in_recovery() = FALSE THEN
          NOTIFY libzbxpgsql_fake_notify;
        END IF;
      END;
    $$
  LANGUAGE plpgsql
  VOLATILE
  ;

  REVOKE ALL ON FUNCTION replication_pump() FROM public;
  GRANT EXECUTE ON FUNCTION replication_pump() TO postgres;

COMMIT;

