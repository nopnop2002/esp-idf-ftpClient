# esp-idf-ftpClient
FTP Client for esp-idf

I ported from [here](https://github.com/JohnnyB1290/ESP32-FTP-Client).   

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

```
git clone https://github.com/nopnop2002/esp-idf-ftpClient
cd esp-idf-ftpClient/
make menuconfig
make flash monitor
```

ESP32 supports the following file systems.   
You can select any one using menuconfig.   
- SPIFFS file system on FLASH   
- FAT file system on FLASH   
- FAT file system on SPI peripheral SDCARD   
- FAT file system on SDMMC peripheral SDCARD   

![ftpClient-config](https://user-images.githubusercontent.com/6020549/65889407-2b489c80-e3dc-11e9-9e6c-acae8f69880f.jpg)
![ftpClienc-config-filesystem](https://user-images.githubusercontent.com/6020549/65959000-c2b8f880-e48b-11e9-99d2-7cdf1cd7efc8.jpg)

---

# Using SPIFFS file system

![ftpClient-config-SPIFFS](https://user-images.githubusercontent.com/6020549/65889414-2f74ba00-e3dc-11e9-9358-91db0a9f536a.jpg)![ftpClient-SPIFFS](https://user-images.githubusercontent.com/6020549/65889527-5fbc5880-e3dc-11e9-96be-123e43268388.jpg)

---

# Using FAT file system on FLASH

![ftpClient-config-FATFS](https://user-images.githubusercontent.com/6020549/65889428-326faa80-e3dc-11e9-9aca-6d37030ddf47.jpg)
![ftpClient-FATFS](https://user-images.githubusercontent.com/6020549/65889548-64810c80-e3dc-11e9-96e0-8207b9d3fe9e.jpg)

---

# Using FAT file system on SPI peripheral SDCARD

![ftpClient-config-SPI_SDCARD](https://user-images.githubusercontent.com/6020549/65959025-dd8b6d00-e48b-11e9-9f79-6a4aa50c07b3.jpg)
![ftpClient-SPI_SDCARD](https://user-images.githubusercontent.com/6020549/65959028-e2502100-e48b-11e9-8da4-79bbb0c0a8ec.jpg)

__Must be formatted with FAT32 before use__

|ESP32 pin|SPI pin|Notes|
|:-:|:-:|:-:|
|GPIO14(MTMS)|SCK||
|GPIO15(MTDO)|MOSI|10k pull up if can't mount|
|GPIO2|MISO||
|GPIO13(MTCK)|CS|| 
|3.3V|VCC|Can't use 5V supply|
|GND|GND||

---

# Using FAT file system on SDMMC peripheral SDCARD

![ftpClient-config-MMC_SDCARD](https://user-images.githubusercontent.com/6020549/65959060-f3009700-e48b-11e9-8fa8-d3f9572e7c08.jpg)

![ftpClient-MMC_SDCARD](https://user-images.githubusercontent.com/6020549/65959066-f72cb480-e48b-11e9-96df-a7cc0dbdba32.jpg)

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

