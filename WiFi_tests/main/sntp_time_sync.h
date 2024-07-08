#ifndef SNTP_TIME_SYNC_H
#define SNTP_TIME_SYNC_H

/**
 * Starts the NTP server synchronization task.
 */
void sntp_time_sync_task_start(void);

/**
 * Returns local time if set.
 * @return local time buffer.
 */
char* sntp_time_sync_get_time(void);

#endif