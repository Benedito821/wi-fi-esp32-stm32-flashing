#ifndef _WIFI_RESET_BUTTON
#define _WIFI_RESET_BUTTON
// Default interrupt flag
#define ESP_INTR_FLAG_DEFAULT	0

// Wifi reset button is the BOOT button on the DevKit
#define WIFI_RESET_BUTTON		0

/**
 * Configures Wifi reset button and interrupt configuration
 */
void wifi_reset_button_config(void);

#endif