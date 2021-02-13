# esp-idf-ftpClient
FTP Client for esp-idf.   
I ported from [here](https://github.com/JohnnyB1290/ESP32-FTP-Client).   

# Installation for ESP32
```
git clone https://github.com/nopnop2002/esp-idf-ftpClient
cd esp-idf-ftpClient/
idf.py set-target esp32
idf.py menuconfig
idf.py flash
```

# Installation for ESP32-S2

```
git clone https://github.com/nopnop2002/esp-idf-ftpClient
cd esp-idf-ftpClient/
idf.py set-target esp32s2
idf.py menuconfig
idf.py flash
```

# Configure
You have to set this config value with menuconfig.   
- CONFIG_FILE_SYSTEM   
See below.
- CONFIG_ESP_WIFI_SSID   
SSID of your wifi.
- CONFIG_ESP_WIFI_PASSWORD   
PASSWORD of your wifi.
- CONFIG_ESP_MAXIMUM_RETRY   
Maximum number of retries when connecting to wifi.
- CONFIG_FTP_SERVER   
IP or MDNS of FTP Server.
- CONFIG_FTP_USER   
Username of FTP Server.
- CONFIG_FTP_PASSWORD   
Password of FTP Server.

![config-main](https://user-images.githubusercontent.com/6020549/107837354-d399ae00-6de3-11eb-9fb3-f221e536a5b9.jpg)
![config-app-1](https://user-images.githubusercontent.com/6020549/107837352-d3011780-6de3-11eb-9ec5-bdb43bfe304d.jpg)

ESP32 supports the following file systems.   
You can select any one using menuconfig.   
- SPIFFS file system on FLASH   
- FAT file system on FLASH   
- FAT file system on SPI peripheral SDCARD   
- FAT file system on SDMMC peripheral SDCARD   

![config-app-2](https://user-images.githubusercontent.com/6020549/107837353-d399ae00-6de3-11eb-8aa5-ff4d7570191c.jpg)

# Using FAT file system on SPI peripheral SDCARD
__Must be formatted with FAT32 before use__

|ESP32 pin|SPI pin|Notes|
|:-:|:-:|:-:|
|GPIO14(MTMS)|SCK||
|GPIO15(MTDO)|MOSI|10k pull up if can't mount|
|GPIO2|MISO||
|GPIO13(MTCK)|CS|| 
|3.3V|VCC|Can't use 5V supply|
|GND|GND||


# Using FAT file system on SDMMC peripheral SDCARD
__Must be formatted with FAT32 before use__

|ESP32 pin|SD card pin|Notes|
|:-:|:-:|:-:|
|GPIO14(MTMS)|CLK|10k pullup|
|GPIO15(MTDO)|CMD|10k pullup|
|GPIO2|D0|10k pullup|
|GPIO4|D1|not used in 1-line SD mode; 10k pullup in 4-line SD mode|
|GPIO12(MTDI)|D2|not used in 1-line SD mode; 10k pullup in 4-line SD mode|
|GPIO13 (MTCK)|D3|not used in 1-line SD mode, but card's D3 pin must have a 10k pullup
|N/C|CD|optional, not used in the example|
|N/C|WP|optional, not used in the example|

# Screen Shot
![ScrrenShot](https://user-images.githubusercontent.com/6020549/107837485-5f133f00-6de4-11eb-9fe8-775443c6836d.jpg)
