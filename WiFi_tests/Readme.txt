This is an IoT application using WiFi and a ESP32-WROOM-32 board to implement different functionalities such as: OTA board flashing, getting sensor payload data, WiFi device in AP and station modes. The interaction with the application is quite intuitive. The connection scheme is shown below, along with a demo of the application. 

Firstly, a client device connects to the esp board as if it was connecting to a common WiFi router. After this the webpage can be accessed using the desired web browser and the gateway address. The credentials can be found on the wifi_app.h file. Further, on the web page, we can get the esp to connect to a WiFi router with internet access. This will allow us to use the SNT protocol. 

Note: for the Firmware upgrade there should be a .bin file.