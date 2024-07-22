#ifndef WIFI_APP_H_
#define WIFI_APP_H_

#include "esp_netif.h"
#include "esp_wifi_types.h"
#include "freertos/FreeRTOS.h"

//callback typedef
typedef void (*wifi_connected_event_callback_t)(void);

//wi-fi application settings
#define WIFI_AP_SSID                "stota"
#define WIFI_AP_PASSWORD            "Bjohn513"
#define WIFI_AP_CHANNEL             1
#define WIFI_AP_SSID_HIDDEN         0
#define WIFI_APP_MAX_CONNECTIONS    5
#define WIFI_AP_BEACON_INTERVAL     100
#define WIFI_AP_IP                 "192.168.0.1"
#define WIFI_AP_GATEWAY            "192.168.0.1"
#define WIFI_AP_NETMASK            "255.255.255.0"
#define WIFI_AP_IP                 "192.168.0.1"
#define WIFI_AP_BANDWIDTH          WIFI_BW_HT20   
#define WIFI_STA_POWER_SAVE        WIFI_PS_NONE
#define MAX_SSID_LENGTH            32
#define MAX_PASSWORD_LENGTH        64
#define MAX_CONNECTION_RETRIES     5

//netif object for the Station and Access Point
extern esp_netif_t* esp_netif_sta;
extern esp_netif_t* esp_netif_ap;

/**
 * Message IDs for the WiFi application task
 * @note Expand this based on your apllication requirements
*/
typedef enum wifi_app_message
{
    WIFI_APP_MSG_START_HTTP_SERVER = 0,
    WIFI_APP_MSG_CONNECTING_FROM_HTTP_SERVER,
    WIFI_APP_MSG_STA_CONNECTED_GOT_IP,
    WIFI_APP_MSG_USER_REQUESTED_STA_DISCONNECT,
    WIFI_APP_MSG_LOAD_SAVED_CREDENTIALS,
    WIFI_APP_MSG_STA_DISCONNECTED
}wifi_app_message_e;

/**
 * Gets the wifi configuration
 */
wifi_config_t* wifi_app_get_wifi_config(void);

/**
 * Structure for the message queue
 * @note Expand this based on application requirements e.g. add another type and parameter as required
*/
typedef struct wifi_app_queue_message
{
    wifi_app_message_e msgID;
}wifi_app_queue_message_t;

/**
 * Sends a message to the queue
 * @param msgID message ID from the wifi_app_message_e enum
 * @return pdTRUE if an item successfully sent tothe queue, otherwise pdFALSE
 * @note Expand the parameter list based on your requirements e.g. how you've expanded the wifi_app_queue_message_t
*/
BaseType_t wifi_app_send_message(wifi_app_message_e msgID);

/**
 * Starts the WiFi RTOS task
*/
void wifi_app_start(void);

/**
 * Sets the callback function.
 */
void wifi_app_set_callback(wifi_connected_event_callback_t cb);

/**
 * Calls the callback function.
 */
void wifi_app_call_callback(void);

int8_t wifi_app_get_rssi(void);

#endif