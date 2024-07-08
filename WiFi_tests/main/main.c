#include "nvs_flash.h"
#include "wifi_app.h"
#include "blue_led.h"
#include "mpu6050.h"
#include "driver/i2c.h"
#include "wifi_reset_button.h"
#include "esp_log.h"

#include "driver/uart.h"

#include <string.h>

#include "sntp_time_sync.h"
#include "aws_iot.h"

static const char TAG[] = "main";

void wifi_application_connected_events(void)
{
	ESP_LOGI(TAG, "WiFi Application Connected!!");
	// sntp_time_sync_task_start();
    // aws_iot_start();
}

// static void uart_task(void *arg)
// {
//     uart_config_t uart_config = {
//         .baud_rate = 115200,
//         .data_bits = UART_DATA_8_BITS,
//         .parity    = UART_PARITY_DISABLE,
//         .stop_bits = UART_STOP_BITS_1,
//         .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
//         .source_clk = UART_SCLK_DEFAULT,
//     };

//     ESP_ERROR_CHECK(uart_param_config(UART_NUM_0, &uart_config));
//     ESP_ERROR_CHECK(uart_set_pin(UART_NUM_0, 35, 34, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
//     ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, 1024, 1024, 0, NULL, 0));

//     const char* uartBuf = "Hi from ESP32\n"; 

//     for(;;)
//     {
//         uart_write_bytes(UART_NUM_0,  uartBuf, strlen(uartBuf));

//         vTaskDelay(1000 / portTICK_PERIOD_MS);
        
//     }
// }



void app_main(void)
{
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_param_config(UART_NUM_2, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_2, 17, 16, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_2, 1024, 1024, 0, NULL, 0));

    ESP_ERROR_CHECK(uart_param_config(UART_NUM_0, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_0, 4, 5, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, 1024, 1024, 0, NULL, 0));

    //initialize NVS
    esp_err_t ret = nvs_flash_init();
    if(ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    // configure wifi reset button
    wifi_reset_button_config();
    mpu6050_init(I2C_NUM_0);
    // configure the blue LED
    blue_led_pwm_init();
    //Start the Wifi
    wifi_app_start();
    // Set connected event callback
	wifi_app_set_callback(&wifi_application_connected_events);

    // xTaskCreate(uart_task, "uart_task", 1024, NULL, 10, NULL);
}
