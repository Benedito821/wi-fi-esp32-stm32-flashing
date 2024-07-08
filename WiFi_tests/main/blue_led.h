#ifndef BLUE_LED_H_
#define BLUE_LED_H_
#include <stdint.h>

#define BLUE_LED_GPIO                    2

#define WIFI_DISCONNECTED_DUTY_CYCLE     10
#define WIFI_DISCONNECTED_FREQ           5
#define WIFI_CONNECTED_DUTY_CYCLE        50
#define WIFI_CONNECTED_FREQ              100

typedef struct 
{
    uint8_t      channel,
                gpio_num,
                    mode,
             timer_index;

}ledc_info_t;

/**
 * Initialize the PWM generration over the GPIO
*/
void blue_led_pwm_init(void);

/**
 * duty cycle to indicate there is still no Wifi connection
*/
void blue_led_light_wifi_disconnected(void);

/**
 * duty cycle to indicate  Wifi is connected
*/
void blue_led_light_wifi_connected(void);
#endif