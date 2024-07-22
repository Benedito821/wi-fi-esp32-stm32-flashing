#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_ota_ops.h"
#include "sys/param.h"

#include "esp_http_server.h"
#include  "esp_log.h"
#include  "esp_wifi.h"
#include "http_server.h"
#include "tasks_common.h"
#include "wifi_app.h"
#include "lwip/netdb.h"
#include "esp_timer.h"

#include "driver/uart.h"

#include "driver/i2c.h"
#include "Wifi_app.h"

//Tag used for ESP serial console messages
static const char TAG[] = "http_server";

// Wifi connect status
static int g_wifi_connect_status = NONE;

//Firmware update status
static int g_fw_update_status = OTA_UPDATE_PENDING; 

//OTA update handler request monitor
static bool ota_update_requested = false;

//http server task handle
static httpd_handle_t http_server_handle = NULL;

//http server monitor task handle
static TaskHandle_t task_http_server_monitor = NULL;

//Queue handle used to manipulate the main queue of events
static QueueHandle_t http_server_monitor_queue_handle = NULL;

// Embedded files : JQuery, index.html, app.css, app.js and favicon.ico files
extern const uint8_t jquery_3_3_1_min_js_start[]  asm("_binary_jquery_3_3_1_min_js_start"); 
extern const uint8_t jquery_3_3_1_min_js_end[]    asm("_binary_jquery_3_3_1_min_js_end"); 
extern const uint8_t index_html_start[]           asm("_binary_index_html_start"); 
extern const uint8_t index_html_end[]             asm("_binary_index_html_end"); 
extern const uint8_t app_css_start[]              asm("_binary_app_css_start"); 
extern const uint8_t app_css_end[]                asm("_binary_app_css_end"); 
extern const uint8_t app_js_start[]               asm("_binary_app_js_start"); 
extern const uint8_t app_js_end[]                 asm("_binary_app_js_end"); 
extern const uint8_t favicon_ico_start[]          asm("_binary_favicon_ico_start");
extern const uint8_t favicon_ico_end[]            asm("_binary_favicon_ico_end"); 

/**
 *http server monitor task usedd to track events of the http server
 @param pvParameters parameter which can be passed to the task 
*/
static void http_server_monitor(void* parameter)
{
    http_server_queue_message_t msg;
    while(1)
    {
        if(xQueueReceive(http_server_monitor_queue_handle,&msg,portMAX_DELAY))
        {
            switch(msg.msgID)
            {
                case HTTP_MSG_WIFI_CONNECT_INIT:
                    ESP_LOGI(TAG,"HTTP_MSG_WIFI_CONNECT_INIT");
                    g_wifi_connect_status  = HTTP_WIFI_STATUS_CONNECTING;
                    break;

                case HTTP_MSG_WIFI_CONNECT_SUCCESS:
                    ESP_LOGI(TAG,"HTTP_MSG_WIFI_CONNECT_SUCCESS");
                    g_wifi_connect_status  = HTTP_WIFI_STATUS_CONNECT_SUCCESS;
                    break;

                case HTTP_MSG_WIFI_CONNECT_FAIL:
                    ESP_LOGI(TAG,"HTTP_MSG_WIFI_CONNECT_FAIL");
                    g_wifi_connect_status  = HTTP_WIFI_STATUS_CONNECT_FAILED;
                    break;

                case HTTP_MSG_WIFI_USER_DISCONNECT:
                    ESP_LOGI(TAG,"HTTP_MSG_WIFI_USER_DISCONNECT");
                    g_wifi_connect_status  = HTTP_WIFI_STATUS_DISCONNECTED;
                    break;

                case HTTP_MSG_OTA_UPDATE_SUCCESSFUL:
                    ESP_LOGI(TAG,"HTTP_MSG_OTA_UPDATE_SUCCESSFUL");
                    break;

                case HTTP_MSG_OTA_UPDATE_FAILED:
                    ESP_LOGI(TAG,"HTTP_MSG_OTA_UPDATE_FAILED");
                    break;

                default:
                    break;
            }
        }
    }
}

/**
 * Jquery get handler is requested when accessing the web page
 * @param req HTTP request for which the uri needs to be handled
 * @return ESP_OK
*/
static esp_err_t http_server_jquery_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG,"Jquery requested");
    httpd_resp_set_type(req,"application/javascript");
    httpd_resp_send(req,(const char*)jquery_3_3_1_min_js_start,jquery_3_3_1_min_js_end-jquery_3_3_1_min_js_start);
    return ESP_OK;
}

/**
 * Sends the index.html page
 * @param req HTTP request for which the uri needs to be handled
 * @return ESP_OK
*/
static esp_err_t http_server_index_html_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG,"index.html requested");
    httpd_resp_set_type(req,"text/html");
    httpd_resp_send(req,(const char*)index_html_start,index_html_end-index_html_start);
    return ESP_OK;
}

/**
 * app.css get handdler is requested when accessing the web page
 * @param req HTTP request for which the uri needs to be 
 * @return ESP_OK
*/
static esp_err_t http_server_app_css_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG,"app.css requested");
    httpd_resp_set_type(req,"text/css");
    httpd_resp_send(req,(const char*)app_css_start,app_css_end-app_css_start);
    return ESP_OK;
}

/**
 * app.js get handdler is requested when accessing the web page
 * @param req HTTP request for which the uri needs to be handled
 * @return ESP_OK
*/
static esp_err_t http_server_app_js_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG,"app.js requested");
    httpd_resp_set_type(req,"application/javascript");
    httpd_resp_send(req,(const char*)app_js_start,app_js_end-app_js_start);
    return ESP_OK;
}

/**
 * Sends the .ico (icon) file when accessing the web page
 * @param req HTTP request for which the uri needs to be handled
 * @return ESP_OK
*/
static esp_err_t http_server_favicon_ico_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG,"favicon.ico requested");
    httpd_resp_set_type(req,"image/x-icon");
    httpd_resp_send(req,(const char*)favicon_ico_start,favicon_ico_end-favicon_ico_start);
    return ESP_OK;
}

/**
 * Receives the .bin files via the web page and handles the firmware update
 * @param req http request for which the uri needs to be handled
 * @return ESP_OK , otherwise ESP_FAIL if timeout occurs and the update cannot be started
*/
esp_err_t http_server_OTA_update_handler(httpd_req_t *req)
{
    char ota_buff[1024];
    int content_length = req->content_len;
    int content_received = 0;
    int recv_len;
    bool is_req_body_started = false;
    char uart_temp[10] = {'\0'};
    char ack_arr[6] = {'\0'};

    ota_update_requested = true;

    do
    {
        //read data for the request
        if((recv_len = httpd_req_recv(req,ota_buff,MIN(content_length,sizeof(ota_buff)))) < 0)
        {
            //check if timeout occured
            if(recv_len == HTTPD_SOCK_ERR_TIMEOUT)
            {
                ESP_LOGI(TAG,"http_server_OTA_update_handler: Socket Timeout");
                continue; //retry receiving if  timeout occured
            }
            ESP_LOGI(TAG,"http_server_OTA_update_handler: OTA other error %d",recv_len);
            return ESP_FAIL;
        }
        //is this the first data we are receiving
        //if so, it will have the information in the header that we need
        if (!is_req_body_started)
		{
			is_req_body_started = true;
			// Get the location of the .bin file content (remove the web form data)
			char *body_start_p = strstr(ota_buff, "\r\n\r\n") + 4;

			int body_part_len = recv_len - (body_start_p - ota_buff);

            printf("http_server_OTA_update_handler: body_part_len: %d , recv_len: %d , ota_buff:%p , body_start_p:%p\r\n", body_part_len,recv_len,ota_buff,body_start_p);

            content_length = content_length - (body_start_p - ota_buff);
            
            printf("http_server_OTA_update_handler: OTA file size: %d\r\n", content_length); //we take off the remaining web data

            sprintf(uart_temp,"%d",content_length);

            printf("http_server_OTA_update_handler: Sending OTA file size...\r\n");

            uart_write_bytes(UART_NUM_2,  uart_temp, sizeof(uart_temp));


            uart_read_bytes(UART_NUM_2, ack_arr,sizeof(ack_arr), pdMS_TO_TICKS(1000));

            if(!strcmp(ack_arr,"ACK0"))
            {
                printf("http_server_OTA_update_handler: Slave board connected\r\n");
            }
            else
            {
                printf("http_server_OTA_update_handler: Slave board not connected. Abrorting...\r\n");
                http_server_monitor_send_message(HTTP_MSG_OTA_UPDATE_FAILED); 
                break;
            }
             
            uart_write_bytes(UART_NUM_2, body_start_p,body_part_len);

            content_received += body_part_len;

            printf("first chunk has been sent body_part_len= %d  content_received= %d\r\n",body_part_len,content_received);

            vTaskDelay(pdMS_TO_TICKS(2000)); //delay for synch between boards
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(2000)); //delay for synch between boards

            uart_write_bytes(UART_NUM_2,  ota_buff, recv_len);

            content_received += recv_len;

            printf("next chunk has been sent recv_len = %d  content_received= %d\r\n",recv_len,content_received);
        }
    }while(recv_len > 0 && content_received < content_length);

    if(!strcmp(ack_arr,"ACK0") )
    {
        g_fw_update_status = OTA_UPDATE_SUCCESSFUL;
        http_server_monitor_send_message(HTTP_MSG_OTA_UPDATE_SUCCESSFUL); 
    }
    else
    {
        g_fw_update_status = OTA_UPDATE_FAILED;
        http_server_monitor_send_message(HTTP_MSG_OTA_UPDATE_FAILED); 
        return ESP_FAIL;
    }

	return ESP_OK;
}

/**
 * OTA status handler responds with the firmware update status after the OTA update is started
 * and responds with the compile time/date when the page is first requested
 * @param req HTTP request for which the uri needs to be handled
 * @return ESP_OK
 */
esp_err_t http_server_OTA_status_handler(httpd_req_t* req)
{
	char otaJSON[100];

	ESP_LOGI(TAG, "OTAstatus requested");

    if(ota_update_requested == false)
        g_fw_update_status = OTA_UPDATE_PENDING;

    ota_update_requested = false; //clear the OTA handler request flag

	sprintf(otaJSON, "{\"ota_update_status\":%d,\"compile_time\":\"%s\",\"compile_date\":\"%s\"}", g_fw_update_status, __TIME__, __DATE__);
	httpd_resp_set_type(req, "application/json");
	httpd_resp_send(req, otaJSON, strlen(otaJSON));
	return ESP_OK;
}

/**
 * wifiConnect.json handler is invoked after the connect button is pressed
 * and handles receiving the SSID and password entered by the user
 * @param req HTTP request for which the uri needs to be handled.
 * @return ESP_OK
 */
static esp_err_t http_server_wifi_connect_json_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "/wifiConnect.json requested");
    size_t len_ssid = 0, len_pass = 0;
    char *ssid_str = NULL, *pass_str = NULL;
    // Get SSID header
	len_ssid = httpd_req_get_hdr_value_len(req, "my-connect-ssid") + 1;
    if (len_ssid > 1)
	{
		ssid_str = malloc(len_ssid);
		if (httpd_req_get_hdr_value_str(req, "my-connect-ssid", ssid_str, len_ssid) == ESP_OK)
		{
			ESP_LOGI(TAG, "http_server_wifi_connect_json_handler: Found header => my-connect-ssid: %s", ssid_str);
		}
    }
    // Get Password header
    len_pass = httpd_req_get_hdr_value_len(req, "my-connect-pwd") + 1;
    if (len_pass > 1)
    {
        pass_str = malloc(len_pass);
        if (httpd_req_get_hdr_value_str(req, "my-connect-pwd", pass_str, len_pass) == ESP_OK)
        {
            ESP_LOGI(TAG, "http_server_wifi_connect_json_handler: Found header => my-connect-pwd: %s", pass_str);
        }
    }
	
    // Update the Wifi networks configuration and let the wifi application know
	wifi_config_t* wifi_config = wifi_app_get_wifi_config();
	memset(wifi_config, 0x00, sizeof(wifi_config_t));
	memcpy(wifi_config->sta.ssid, ssid_str, len_ssid);
	memcpy(wifi_config->sta.password, pass_str, len_pass);
	wifi_app_send_message(WIFI_APP_MSG_CONNECTING_FROM_HTTP_SERVER);
    free(ssid_str);
	free(pass_str);
	return ESP_OK;
}

/**
 * wifiConnectStatus handler updates the connection status for the web page.
 * @param req HTTP request for which the uri needs to be handled.
 * @return ESP_OK
 */
static esp_err_t http_server_wifi_connect_status_json_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "/wifiConnectStatus requested");

	char statusJSON[100];

	sprintf(statusJSON, "{\"wifi_connect_status\":%d}", g_wifi_connect_status);

	httpd_resp_set_type(req, "application/json");
	httpd_resp_send(req, statusJSON, strlen(statusJSON));

	return ESP_OK;
}

static esp_err_t http_server_get_wifi_connect_info_json_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "/wifiConnectInfo.json requested");

	char ipInfoJSON[200];
	memset(ipInfoJSON, 0, sizeof(ipInfoJSON));

	char ip[IP4ADDR_STRLEN_MAX];
	char netmask[IP4ADDR_STRLEN_MAX];
	char gw[IP4ADDR_STRLEN_MAX];

	if (g_wifi_connect_status == HTTP_WIFI_STATUS_CONNECT_SUCCESS)
	{
		wifi_ap_record_t wifi_data;
		ESP_ERROR_CHECK(esp_wifi_sta_get_ap_info(&wifi_data));
		char *ssid = (char*)wifi_data.ssid;

		esp_netif_ip_info_t ip_info;
		ESP_ERROR_CHECK(esp_netif_get_ip_info(esp_netif_sta, &ip_info));
		esp_ip4addr_ntoa(&ip_info.ip, ip, IP4ADDR_STRLEN_MAX);
		esp_ip4addr_ntoa(&ip_info.netmask, netmask, IP4ADDR_STRLEN_MAX);
		esp_ip4addr_ntoa(&ip_info.gw, gw, IP4ADDR_STRLEN_MAX);

		sprintf(ipInfoJSON, "{\"ip\":\"%s\",\"netmask\":\"%s\",\"gw\":\"%s\",\"ap\":\"%s\"}", ip, netmask, gw, ssid);
	}

	httpd_resp_set_type(req, "application/json");
	httpd_resp_send(req, ipInfoJSON, strlen(ipInfoJSON));

	return ESP_OK;
}

/**
 * wifiDisconnect.json handler responds by sending a message to the Wifi application to disconnect.
 * @param req HTTP request for which the uri needs to be handled.
 * @return ESP_OK
 */
static esp_err_t http_server_wifi_disconnect_json_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "wifiDisconect.json requested");

	wifi_app_send_message(WIFI_APP_MSG_USER_REQUESTED_STA_DISCONNECT);

	return ESP_OK;
}

/**
 * apSSID.json handler responds by sending the AP SSID.
 * @param req HTTP request for which the uri needs to be handled.
 * @return ESP_OK
 */
static esp_err_t http_server_get_ap_ssid_json_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "/apSSID.json requested");

	char ssidJSON[50];

	wifi_config_t *wifi_config = wifi_app_get_wifi_config();
	esp_wifi_get_config(ESP_IF_WIFI_AP, wifi_config);
	char *ssid = (char*)wifi_config->ap.ssid;

	sprintf(ssidJSON, "{\"ssid\":\"%s\"}", ssid);

	httpd_resp_set_type(req, "application/json");
	httpd_resp_send(req, ssidJSON, strlen(ssidJSON));

	return ESP_OK;
}

/**
 * Sets up the default httpd server configuration
 * @return http server instance handle if successful,NULL otherwise
*/
static httpd_handle_t http_server_configure(void)
{
    //Generate the default configuration
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    //create http server monitor task
    xTaskCreatePinnedToCore(&http_server_monitor,"http_server_monitor",
    HTTP_SERVER_MONITOR_STACK_SIZE,NULL, HTTP_SERVER_MONITOR_PRIORITY,&task_http_server_monitor,HTTP_SERVER_MONITOR_CORE_ID);
    //create the message queue
    http_server_monitor_queue_handle = xQueueCreate(3,sizeof(http_server_queue_message_t));
    // The core that the http server will run on
    config.core_id = HTTP_SERVER_TASK_CORE_ID;
    
    //Adjust the default priority to 1 less than the wifi application task
    config.task_priority = HTTP_SERVER_TASK_PRIORITY;
    
    //Bump up the stack size (default is 4096)
    config.stack_size = HTTP_SERVER_MONITOR_STACK_SIZE;

    //Increase uri handlers
    config.max_uri_handlers = 20;

    //Increase the timeout limits
    config.recv_wait_timeout = 10;
    config.send_wait_timeout = 10;

    ESP_LOGI(TAG,
                "http_server_configure: Starting server on port: '%d' with task priority: '%d'",
                config.server_port,
                config.task_priority);

    //Start the httpd server
    if(httpd_start(&http_server_handle,&config )== ESP_OK)
    {
         ESP_LOGI(TAG,"http_server_configure: Registering URI handlers");

         //Register query handler
         httpd_uri_t jquery_js = {
                .uri = "/jquery-3.3.1.min.js",
                .method = HTTP_GET,
                .handler = http_server_jquery_handler,
                .user_ctx = NULL
         };
         httpd_register_uri_handler(http_server_handle,&jquery_js);

        //Register index.html handler
         httpd_uri_t index_html = {
                .uri = "/",
                .method = HTTP_GET,
                .handler = http_server_index_html_handler,
                .user_ctx = NULL
         };
         httpd_register_uri_handler(http_server_handle,&index_html);

        //Register app.css handler
         httpd_uri_t app_css = {
                .uri = "/app.css",
                .method = HTTP_GET,
                .handler = http_server_app_css_handler,
                .user_ctx = NULL
         };
         httpd_register_uri_handler(http_server_handle,&app_css);

         //Register app.js handler
         httpd_uri_t app_js = {
                .uri = "/app.js",
                .method = HTTP_GET,
                .handler = http_server_app_js_handler,
                .user_ctx = NULL
         };
         httpd_register_uri_handler(http_server_handle,&app_js);

         //Register favicon.ico handler
         httpd_uri_t favicon_ico = {
                .uri = "/favicon.ico",
                .method = HTTP_GET,
                .handler = http_server_favicon_ico_handler,
                .user_ctx = NULL
         };
         httpd_register_uri_handler(http_server_handle,&favicon_ico);

         //register OTAupdate handler
         httpd_uri_t OTA_update = {
                .uri = "/OTAupdate",
                .method = HTTP_POST,
                .handler = http_server_OTA_update_handler,
                .user_ctx = NULL
         };
         httpd_register_uri_handler(http_server_handle,&OTA_update);

         //register OTAstatus handler
         httpd_uri_t OTA_status = {
                .uri = "/OTAstatus",
                .method = HTTP_POST,
                .handler = http_server_OTA_status_handler,
                .user_ctx = NULL
         };
         httpd_register_uri_handler(http_server_handle,&OTA_status);

        // register wifiConnect.json handler
		httpd_uri_t wifi_connect_json = {
				.uri = "/wifiConnect.json",
				.method = HTTP_POST,
				.handler = http_server_wifi_connect_json_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &wifi_connect_json);

        // register wifiConnectStatus.json handler
		httpd_uri_t wifi_connect_status_json = {
				.uri = "/wifiConnectStatus",
				.method = HTTP_POST,
				.handler = http_server_wifi_connect_status_json_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &wifi_connect_status_json);

        // register wifiConnectInfo.json handler
		httpd_uri_t wifi_connect_info_json = {
				.uri = "/wifiConnectInfo.json",
				.method = HTTP_GET,
				.handler = http_server_get_wifi_connect_info_json_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &wifi_connect_info_json);
        
        // register wifiDisconnect.json handler
		httpd_uri_t wifi_disconnect_json = {
				.uri = "/wifiDisconnect.json",
				.method = HTTP_DELETE,
				.handler = http_server_wifi_disconnect_json_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &wifi_disconnect_json);

        // register apSSID.json handler
		httpd_uri_t ap_ssid_json = {
				.uri = "/apSSID.json",
				.method = HTTP_GET,
				.handler = http_server_get_ap_ssid_json_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &ap_ssid_json);

       return http_server_handle;
    }
    return NULL;
}

void http_server_start(void)
{
    if(http_server_handle == NULL)
    {
        http_server_handle = http_server_configure();
    }
        
}

void http_server_stop(void)
{
    if(http_server_handle)
    {
        httpd_stop(http_server_handle);
        ESP_LOGI(TAG,"http_server_stop: stopping HTTP server");
        http_server_handle = NULL;
    }
    if(task_http_server_monitor)
    {
        vTaskDelete(task_http_server_monitor);
        ESP_LOGI(TAG,"http_server_stop: stopping HTTP server monitor");
        task_http_server_monitor = NULL;
    }
        
}

BaseType_t http_server_monitor_send_message(http_server_message_e msgID)
{
    http_server_queue_message_t msg;
    msg.msgID = msgID;
    return xQueueSend(http_server_monitor_queue_handle,&msg,portMAX_DELAY);
}