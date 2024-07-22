# Instructions
OTA flashing an STM32 board using a web server running on an ESP32 board. The system block diagram is presented in the section "Scheme".

The UART parameters are 115200/8/1/N. 

The projected was generated and tested under the STM32WB55 board,but it shloud work for others ST boards as well. The web server runs on as ESP32-WROOM-32 board but it was also tested for an ESP32 Devkit wrover. When powered up, ESP32 launches an access point with the SSID stota, creating a LAN after connecting a web client(a device with a web browser and the bin file stored on it). The password can be found in the wifi_app.h file, under web-server/main. The server address was set to 192.168.0.1.

Please note that some knowledge on designing bootloaders are needed to adapt the project in case something works wrong due to a silicon change. Working with vector tables, linker descriptors, memory management are some of the core skills. 

The project works for a variety of file sizes. However, due to synchronization issues between the server and the slave board the flashing speed was reduced to roughly 512 bytes/s (this can be improved). Thus, a 10K code would take around 19s to be sent and flashed. 

Example .bin files can be found under /bins folder

To get started

1. Flash the slave board with the bootloader project
2. Flash the ESP32 with the web-server project
3. Connect the slave board to the ESP32 via UART according to the scheme
4. On a PC/mobile phone connect to the stota wifi network
5. On the PC or mobile phone go to the browser and browse for 192.168.0.1 (a tab named STM OTA update should open up)
6. On the GUI select the .bin file from the device and click on update firmware 

Additional debugging interfaces through UART are available both for the ESP32 and ST boards.

# System scheme

![](https://github.com/Benedito821/wi-fi-esp32-stm32-flashing/blob/master/scheme.jpg)
