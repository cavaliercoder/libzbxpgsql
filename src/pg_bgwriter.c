/*
** libzbxpgsql - A PostgreSQL monitoring module for Zabbix
** Copyright (C) 2015 - Ryan Armstrong <ryan@cavaliercoder.com>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
**/

#include "libzbxpgsql.h"

/*
 * Custom keys pg.* (for each field in pg_stat_bgwriter)
 *
 * Returns the requested global statistic for the PostgreSQL server
 *
 * Parameters:
 *   0:  connection string
 *   1:  connection database
 *
 * Returns: u
 */
int    PG_STAT_BGWRITER(AGENT_REQUEST *request, AGENT_RESULT *result)
{
    int         ret = SYSINFO_RET_FAIL;                 // Request result code
    const char  *__function_name = "PG_STAT_BGWRITER";  // Function name for log file

    const char  *pgsql_bgwriter_stat = "SELECT %s FROM pg_stat_bgwriter;";
    
    char        *field;
    char        query[MAX_STRING_LEN];
    
    zabbix_log(LOG_LEVEL_DEBUG, "In %s()", __function_name);
    
    // Get stat field from requested key name "pb.table.<field>"
    field = &request->key[3];
    
    // Build query
    zbx_snprintf(query, sizeof(query), pgsql_bgwriter_stat, field);
    
    // Get field value
    if(0 == strncmp(field, "checkpoint_", 11))
        ret = pg_get_dbl(request, result, query, NULL);

    else if(0 == strncmp(field, "stats_reset", 11))
        ret = pg_get_string(request, result, query, NULL);
    
    else
        ret = pg_get_int(request, result, query, NULL);
    
    zabbix_log(LOG_LEVEL_DEBUG, "End of %s()", __function_name);
    return ret;
}

/*
 * Custom key pg.stats_reset_interval
 *
 * Returns the interval in seconds since the BG writer stats were last reset.
 *
 * Parameters:
 *   0:  connection string
 *   1:  connection database
 *
 * Returns: i
 */
int     PG_BG_STATS_RESET_INTERVAL(AGENT_REQUEST *request, AGENT_RESULT *result)
{
    int         ret = SYSINFO_RET_FAIL;                             // Request result code
    const char  *__function_name = "PG_BG_STATS_RESET_INTERVAL";    // Function name for log file

    const char  *pgsql_stats_reset_interval = 
        "SELECT EXTRACT(EPOCH FROM NOW() - stats_reset) from pg_stat_bgwriter;";
    
    zabbix_log(LOG_LEVEL_DEBUG, "In %s()", __function_name);
    
    ret = pg_get_int(request, result, pgsql_stats_reset_interval, NULL);
    
    zabbix_log(LOG_LEVEL_DEBUG, "End of %s()", __function_name);
    return ret;    
}

/*
 * Custom key pg.checkpoint_avg_interval
 *
 * Returns the average interval in seconds between all checkpoints that have
 * run since statistics were reset.
 *
 * Parameters:
 *   0:  connection string
 *   1:  connection database
 *
 * Returns: d
 */
int     PG_BG_AVG_INTERVAL(AGENT_REQUEST *request, AGENT_RESULT *result)
{
    int         ret = SYSINFO_RET_FAIL;                 // Request result code
    const char  *__function_name = "PG_BG_AVG_INTERVAL";  // Function name for log file
    
    const char  *pgsql_bg_avg_interval = "\
SELECT \
    CASE checkpoints_timed + checkpoints_req \
        WHEN 0 THEN 0 \
        ELSE EXTRACT(EPOCH FROM (NOW() - stats_reset)) / (checkpoints_timed + checkpoints_req) \
    END \
FROM pg_stat_bgwriter;";

    zabbix_log(LOG_LEVEL_DEBUG, "In %s()", __function_name);

	ret = pg_get_dbl(request, result, pgsql_bg_avg_interval, NULL);

    zabbix_log(LOG_LEVEL_DEBUG, "End of %s()", __function_name);
    return ret;
}

/*
 * Custom key pg.checkpoint_time_ratio
 *
 * Returns the percentage of time spent writing or syncing checkpoints since
 * statistics were reset.
 *
 * Parameters:
 *   0:  connection string
 *   1:  connection database
 *   2:  action: all (default) | write | sync
 *
 * Returns: d
 */
int     PG_BG_TIME_RATIO(AGENT_REQUEST *request, AGENT_RESULT *result)
{
    int         ret = SYSINFO_RET_FAIL;                 // Request result code
    const char  *__function_name = "PG_BG_TIME_RATIO";   // Function name for log file

    const char  *pgsql_bg_time_ratio = "\
SELECT \
    (%s / 1000) / EXTRACT(EPOCH FROM NOW() - stats_reset) \
FROM pg_stat_bgwriter;";

    char        query[MAX_STRING_LEN];
    char        *action = NULL, *field = NULL;
    
    zabbix_log(LOG_LEVEL_DEBUG, "In %s()", __function_name);

    // parse action parameter
    action = get_rparam(request, PARAM_FIRST);
    if (strisnull(action) || 0 == strcmp(action, "all"))
        field = "(checkpoint_write_time + checkpoint_sync_time)";
    else if (0 == strcmp(action, "write"))
        field = "checkpoint_write_time";
    else if (0 == strcmp(action, "sync"))
        field = "checkpoint_sync_time";
    else {
        set_err_result(result, "Invalid action parameter: %s", action);
        return ret;
    }

    // build query
    zbx_snprintf(query, sizeof(query), pgsql_bg_time_ratio, field);

    // get result
    ret = pg_get_dbl(request, result, query, NULL);

    zabbix_log(LOG_LEVEL_DEBUG, "End of %s()", __function_name);
    return ret;
}
