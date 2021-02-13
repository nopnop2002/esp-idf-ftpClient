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
|:-:|:-:|:--|
|GPIO14|SCK|10k pull up if can't mount|
|GPIO15|MOSI|10k pull up if can't mount|
|GPIO2|MISO|10k pull up if can't mount|
|GPIO13|CS|10k pull up if can't mount|
|3.3V|VCC|Don't use 5V supply|
|GND|GND||

|ESP32-S2 pin|SPI pin|Notes|
|:-:|:-:|:--|
|GPIO14|SCK|10k pull up if can't mount|
|GPIO15|MOSI|10k pull up if can't mount|
|GPIO2|MISO|10k pull up if can't mount|
|GPIO13|CS|10k pull up if can't mount|
|3.3V|VCC|Don't use 5V supply|
|GND|GND||

Note: This example doesn't utilize card detect (CD) and write protect (WP) signals from SD card slot.

# Using FAT file system on SDMMC peripheral SDCARD
__Must be formatted with FAT32 before use__

|ESP32 pin|SD card pin|Notes|
|:-:|:-:|:--|
|GPIO14|CLK|10k pullup|
|GPIO15|CMD|10k pullup|
|GPIO2|D0|10k pullup|
|GPIO4|D1|not used in 1-line SD mode; 10k pullup in 4-line SD mode|
|GPIO12|D2|not used in 1-line SD mode; 10k pullup in 4-line SD mode|
|GPIO13|D3|not used in 1-line SD mode, but card's D3 pin must have a 10k pullup
|N/C|CD|optional, not used in the example|
|N/C|WP|optional, not used in the example|
|3.3V|VCC|Don't use 5V supply|
|GND|GND||

|ESP32-S2 pin|SD card pin|Notes|
|:-:|:-:|:--|
|GPIO14|CLK|10k pullup|
|GPIO15|CMD|10k pullup|
|GPIO2|D0|10k pullup|
|GPIO13|D3|not used in 1-line SD mode, but card's D3 pin must have a 10k pullup
|N/C|CD|optional, not used in the example|
|N/C|WP|optional, not used in the example|
|3.3V|VCC|Don't use 5V supply|
|GND|GND||

Note: that ESP32-S2 doesn't include SD Host peripheral and only supports SD over SPI. Therefore only SCK, MOSI, MISO, CS and ground pins need to be connected.

# Note about GPIO2 (ESP32 only)   
GPIO2 pin is used as a bootstrapping pin, and should be low to enter UART download mode.   
One way to do this is to connect GPIO0 and GPIO2 using a jumper, and then the auto-reset circuit on most development boards will pull GPIO2 low along with GPIO0, when entering download mode.

# Note about GPIO12 (ESP32 only)   
GPIO12 is used as a bootstrapping pin to select output voltage of an internal regulator which powers the flash chip (VDD_SDIO).   
This pin has an internal pulldown so if left unconnected it will read low at reset (selecting default 3.3V operation).   
When adding a pullup to this pin for SD card operation, consider the following:
- For boards which don't use the internal regulator (VDD_SDIO) to power the flash, GPIO12 can be pulled high.
- For boards which use 1.8V flash chip, GPIO12 needs to be pulled high at reset. This is fully compatible with SD card operation.
- On boards which use the internal regulator and a 3.3V flash chip, GPIO12 must be low at reset. This is incompatible with SD card operation.
    * In most cases, external pullup can be omitted and an internal pullup can be enabled using a `gpio_pullup_en(GPIO_NUM_12);` call. Most SD cards work fine when an internal pullup on GPIO12 line is enabled. Note that if ESP32 experiences a power-on reset while the SD card is sending data, high level on GPIO12 can be latched into the bootstrapping register, and ESP32 will enter a boot loop until external reset with correct GPIO12 level is applied.
    * Another option is to burn the flash voltage selection efuses. This will permanently select 3.3V output voltage for the internal regulator, and GPIO12 will not be used as a bootstrapping pin. Then it is safe to connect a pullup resistor to GPIO12. This option is suggested for production use.

# Screen Shot
![ScrrenShot](https://user-images.githubusercontent.com/6020549/107837485-5f133f00-6de4-11eb-9fe8-775443c6836d.jpg)
