#include "driver/ledc.h"
#include "blue_led.h"
#include "esp_log.h"

ledc_info_t blue_led_info;

/**
 * Initializes the blue led channel and timer settings
*/
void blue_led_pwm_init(void)
{
    blue_led_info.channel             = LEDC_CHANNEL_0;
    blue_led_info.gpio_num      =        BLUE_LED_GPIO;
    blue_led_info.mode           = LEDC_LOW_SPEED_MODE;
    blue_led_info.timer_index           = LEDC_TIMER_0;

    //configure timer 0
    ledc_timer_config_t ledc_timer = 
    {
        .duty_resolution = LEDC_TIMER_8_BIT,
        .freq_hz         = 5,
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .timer_num       = LEDC_TIMER_0
    };
    ledc_timer_config(&ledc_timer);

    //configure the channel
    ledc_channel_config_t ledc_channel = 
    {
        .channel    = blue_led_info.channel,
        .duty       = 0,
        .hpoint     = 0,
        .gpio_num   = blue_led_info.gpio_num,
        .intr_type  = LEDC_INTR_DISABLE,
        .speed_mode = blue_led_info.mode,
        .timer_sel  = blue_led_info.timer_index 
    };
    ledc_channel_config(&ledc_channel);
}

void blue_led_light_wifi_disconnected(void)
{
    ledc_set_freq(blue_led_info.mode,blue_led_info.timer_index,(uint32_t)WIFI_DISCONNECTED_FREQ);
    ledc_set_duty(blue_led_info.mode,blue_led_info.channel,(uint32_t)WIFI_DISCONNECTED_DUTY_CYCLE);
    ledc_update_duty(blue_led_info.mode,blue_led_info.channel);
}

void blue_led_light_wifi_connected(void)
{
    ledc_set_duty(blue_led_info.mode,blue_led_info.channel,(uint32_t)WIFI_CONNECTED_DUTY_CYCLE);
    ledc_update_duty(blue_led_info.mode,blue_led_info.channel);
    ledc_set_freq(blue_led_info.mode,blue_led_info.timer_index,(uint32_t)WIFI_CONNECTED_FREQ);
}