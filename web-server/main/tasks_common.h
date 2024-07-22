#ifndef TASKS_COMMON_H_
#define TASKS_COMMON_H_

//Wfif app tasks
#define WIFI_APP_TASK_STACK_SIZE            4096
#define WIFI_APP_TASK_PRIORITY              5
#define WIFI_APP_TASK_CORE_ID               0

//Http server tasks
#define HTTP_SERVER_TASK_STACK_SIZE         8192
#define HTTP_SERVER_TASK_PRIORITY           4
#define HTTP_SERVER_TASK_CORE_ID            0

// Wifi Reset Button task
#define WIFI_RESET_BUTTON_TASK_STACK_SIZE	2048
#define WIFI_RESET_BUTTON_TASK_PRIORITY		6
#define WIFI_RESET_BUTTON_TASK_CORE_ID		0

//Http server monitor task
#define HTTP_SERVER_MONITOR_STACK_SIZE      4096
#define HTTP_SERVER_MONITOR_PRIORITY        3
#define HTTP_SERVER_MONITOR_CORE_ID         0

#endif