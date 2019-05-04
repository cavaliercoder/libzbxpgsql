# libzbxpgsql-streaming
Monitoring Add-On for libzbxpgsql v1.1 to monitor PostgreSQL Streaming Replication on Zabbix

Ref: https://github.com/robbrucks/libzbxpgsql-streaming

## Setup

*This Document assumes you have the libzbxpgsql v1.1 module installed, configured, and already succesfully monitoring your postgres database clusters (instances)*

If you have set up Streaming Replication as in https://wiki.postgresql.org/wiki/Streaming_Replication #37 you can also add following Templates and configuration to the host.
	
	* Main Template called `Template_PostgreSQL_Server_3.0_Streaming.xml`
	* Secondary Template called `Template_PostgreSQL_Server_3.0_Streaming_Secondary.xml`
	
	In order to use the Secondary one it is necessary to prepare it:

	* Variable `@Secondary@` is for UI Names
	* Variable `@SECONDARY@` is for separating `PG_CONN` from main template - #112, #107

	sed -e 's/@Secondary@/SomeNiceName/g; s/@SECONDARY@/INSTANCENAME/g;' Template_PostgreSQL_Server_3.0_Streaming_Secondary.xml > Template_PostgreSQL_Server_3.0_Streaming_SomeNiceName.xml

This will distinguish instances running on same host but different ports.

1. Copy the `libzbxpgsql-streaming.conf` file as `libzbxpgsql.conf` into the `/etc/zabbix` directory on your master and slave DB servers
1. Execute the SQL script `sql/replication_pump_func.sql` on each *master* DB cluster against the same database as defined in your {$PG\_DB} macro in Zabbix (the `postgres` database by default)
    ```
    psql -f replication_pump_func.sql -U postgres -d postgres
    ```
1. If you will be using a DB user other than `postgres` to connect to the DB from the Zabbix agent, you will need to grant execute on the function to that user:
    ```
    psql -c 'GRANT EXECUTE ON FUNCTION replication_pump() TO your_zabbix_user;' -U postgres postgres
    ```
1. Link the `Template App PostgreSQL Streaming` template to your master and each of your slave DB hosts via the Zabbix UI
1. Restart the zabbix-agent on your DB servers

## What is monitored?
* Count of WAL log bytes waiting to be applied on each _connected_ slave ("lag bytes"; measured on the master)
* Number of seconds a slave is behind the master ("lag seconds"; measured on each slave)
* Whether or not replication has been paused on a slave

## What does it alert on?
* If "lag bytes" exceeds the value of Zabbix macro variable "{$PG\_ALRT\_SLAVE\_LAG\_BYTES}" (default 100mb)
* If "lag seconds" exceeds the value of Zabbix macro variable "{$PG\_ALRT\_SLAVE\_LAG\_SECS}" (default 300 seconds)
* If replication is manually paused on a slave

## What's up with the "Replication Master Log Pump" thingy?
Get ready for a lengthy explanation...

On a master/slave setup using streaming replication, selecting from the `pg_last_xact_replay_timestamp()` function on the slaves will report the timestamp of the last update replayed.  This works fine when the master has constant update activity, but if there is a period of time where there are no updates on the master then the replay timestamp will not get updated on the slaves (since there are no changes to stream). This can cause the slave to _appear_ to be significantly behind the master despite actually being up to date.

I discovered that issuing a simple `NOTIFY` command on the master will cause the notification to be streamed to the slaves. The PostgreSQL documentation indicates that the notification is discarded if there are no corresponding listeners, so this appears to be a relatively harmless and lightweight method to force the replay timestamp to be updated on the slaves.

The `NOTIFY` command does not make any changes to data or schemas in the database and it does not require any special permissions to execute.

Unfortunately libzbxpgsql cannot issue a `NOTIFY` command directly, so I had to implement it using a function.

So the `PostgreSQL Streaming Replication Master Log Pump` item runs this function every 30 seconds to execute a `NOTIFY` and "pumps" the WAL log stream.

By issuing the `NOTIFY` every 30 seconds I can ensure that the replay timestamp on the slaves is updated at least that frequently, even if a master goes "quiet". Then if the timestamp fails to get updated for longer than 30 seconds I can alert that there is truly a problem with replication.

This seemed a far more elegant solution than creating a single-row table with a timestamp, regularly updating it, and watching for the timestamp update on the slaves. It eliminates the need for a table, permissions, and frequent vacuums of the table.

## But I can alert on lag bytes...
Yes, this template also measures the lag bytes as reported by the master in the `pg_stat_replication` view, and it will alert if lag bytes becomes high. But the `pg_stat_replication` view has a critical weakness: if communication with the slave is lost, the corresponding row in `pg_stat_replication` for that slave is immediately deleted and you will not know how far behind replication is. Since that row is gone, Zabbix can't measure any lag and can't alert that the slave is falling behind.

## What am I trying to solve here?
I'm trying to solve one of the more commonly encountered problems with replication: How can I tell that a communication issue has stalled streaming replication?

I think I've solved it - but please let me know if I've got something wrong...
