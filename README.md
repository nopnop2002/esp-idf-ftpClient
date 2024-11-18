# esp-idf-ftpClient
FTP Client for esp-idf.   

# Software requirements   
ESP-IDF V5.0 or later.   
ESP-IDF V4.4 release branch reached EOL in July 2024.   

# Installation
```
git clone https://github.com/nopnop2002/esp-idf-ftpClient
cd esp-idf-ftpClient/
idf.py menuconfig
idf.py flash
```

# Configuration

![config-top](https://user-images.githubusercontent.com/6020549/165466546-f6609f93-679d-4de9-9b45-db11b723d815.jpg)
![config-app](https://user-images.githubusercontent.com/6020549/165466556-7c5770da-e75b-4aa8-a49f-43c975233809.jpg)

## File System Selection   
This project supports the following file systems.   
You can select any one using menuconfig.   
- SPIFFS file system on Builtin SPI Flash Menory   
- FAT file system on Builtin SPI Flash Menory   
- LITTLEFS file system on Builtin SPI Flash Menory   
- FAT file system on SPI peripheral SDCARD   
- FAT file system on SDMMC peripheral SDCARD(Valid only for ESP32/ESP32S3)   
- FAT file system on External SPI Flash Memory like Winbond W25Q64    

![config-file-system-builtin-spiffs](https://github.com/user-attachments/assets/b4b2ad4c-f5f0-4994-98c7-8af8ff7b09a5)
![config-file-system-builtin-fatfs](https://github.com/user-attachments/assets/39b74b16-4df5-427e-8285-93e48e41992e)
![config-file-system-builtin-littlefs](https://github.com/user-attachments/assets/72eb1497-680d-475e-ba52-d4ec0852c441)

Note:   
The connection when using SDSPI, SDMMC, and External SPI flash Memory will be described later.   

Note:   
LITTLEFS requires ESP-IDF V5.2 or later.

## Partition table   
Use ```partitions_example_spiffs.csv``` when you select SPIFFS file system on Builtin SPI Flash Menory.   
Use ```partitions_example_fatfs.csv``` when you select FAT file system on Builtin SPI Flash Menory.   
Use ```partitions_example_littlefs.csv``` when you select LITTLEFS file system on Builtin SPI Flash Menory.   

__If you need more storage space in the Builtin SPI Flash Menory, you will need to modify these files.__   

## Wifi Setting   

![config-wifi](https://user-images.githubusercontent.com/6020549/165469815-dfe6407b-4598-4cc7-8f60-4ceef4618529.jpg)


## FTP Server Setting   

![config-ftp-server](https://user-images.githubusercontent.com/6020549/187052709-7873970e-c72e-4ea0-b904-85e1b0afc799.jpg)

# Using FAT file system on SPI peripheral SDCARD

|ESP32|ESP32S2/S3|ESP32C2/C3/C6|SD card pin|Notes|
|:-:|:-:|:-:|:-:|:--|
|GPIO23|GPIO35|GPIO01|MOSI|10k pull up if can't mount|
|GPIO19|GPIO37|GPIO03|MISO||
|GPIO18|GPIO36|GPIO02|SCK||
|GPIO5|GPIO34|GPIO04|CS||
|3.3V|3.3V|3.3V|VCC|Don't use 5V supply|
|GND|GND|GND|GND||

![config-file-system-sdspi](https://user-images.githubusercontent.com/6020549/194224614-703f6ee3-303f-4f81-a25a-4e26b3297f2b.jpg)

__You can change it to any pin using menuconfig.__   

Note:   
This project doesn't utilize card detect (CD) and write protect (WP) signals from SD card slot.   


# Using FAT file system on SDMMC peripheral SDCARD

On ESP32, SDMMC peripheral is connected to specific GPIO pins using the IO MUX.   
__GPIO pins cannot be customized.__   
GPIO2 and GPIO12 cannot be changed.   
So using 4-line SD mode with ESP32 is very tricky.   
Please see the table below for the pin connections.

|ESP32 pin|SD card pin|Notes|
|:-:|:-:|:--|
|GPIO14|CLK|10k pullup|
|GPIO15|CMD|10k pullup|
|GPIO2|D0|10k pullup or connect to GPIO00|
|GPIO4|D1|not used in 1-line SD mode; 10k pullup in 4-line SD mode|
|GPIO12|D2|not used in 1-line SD mode; 10k pullup in 4-line SD mode|
|GPIO13|D3|not used in 1-line SD mode, but card's D3 pin must have a 10k pullup
|N/C|CD|not used in this project|
|N/C|WP|not used in this project|
|3.3V|VCC|Don't use 5V supply|
|GND|GND||

- 1line mode   
![config-file-system-sdmmc-esp32](https://user-images.githubusercontent.com/6020549/165658970-6956c9c4-eb24-44fe-99c2-758045300adc.jpg)

- 4line mode   
![config-file-system-sdmmc-esp32-4line](https://github.com/user-attachments/assets/5821a686-8ca0-40cc-b0c6-ad247ee29c4b)

On ESP32-S3, SDMMC peripheral is connected to GPIO pins using GPIO matrix.   
__This allows arbitrary GPIOs to be used to connect an SD card.__   

|ESP32-S3 pin|SD card pin|Notes|
|:-:|:-:|:--|
|GPIO36|CLK|10k pullup|
|GPIO35|CMD|10k pullup|
|GPIO37|D0|10k pullup|
|GPIO38|D1|not used in 1-line SD mode; 10k pullup in 4-line SD mode|
|GPIO33|D2|not used in 1-line SD mode; 10k pullup in 4-line SD mode|
|GPIO34|D3|not used in 1-line SD mode, but card's D3 pin must have a 10k pullup
|N/C|CD|not used in this project|
|N/C|WP|not used in this project|
|3.3V|VCC|Don't use 5V supply|
|GND|GND||

- 1line mode   
![config-file-system-sdmmc-esp32s3](https://user-images.githubusercontent.com/6020549/165659001-8e767614-39e5-4e19-9e90-8ce0c8e79f57.jpg)

- 4line mode   
![config-file-system-sdmmc-esp32s3-4line](https://github.com/user-attachments/assets/ddb93c01-07a3-41f7-8cf7-07bea2a8014d)

## Note about GPIO2 (ESP32 only)   
GPIO2 pin is used as a bootstrapping pin, and should be low to enter UART download mode. One way to do this is to connect GPIO0 and GPIO2 using a jumper, and then the auto-reset circuit on most development boards will pull GPIO2 low along with GPIO0, when entering download mode.

- Some boards have pulldown and/or LED on GPIO2. LED is usually ok, but pulldown will interfere with D0 signals and must be removed. Check the schematic of your development board for anything connected to GPIO2.

## Note about GPIO12 (ESP32 only)   
GPIO12 is used as a bootstrapping pin to select output voltage of an internal regulator which powers the flash chip (VDD_SDIO).   
This pin has an internal pulldown so if left unconnected it will read low at reset (selecting default 3.3V operation).   
When adding a pullup to this pin for SD card operation, consider the following:
- For boards which don't use the internal regulator (VDD_SDIO) to power the flash, GPIO12 can be pulled high.
- For boards which use 1.8V flash chip, GPIO12 needs to be pulled high at reset. This is fully compatible with SD card operation.
- On boards which use the internal regulator and a 3.3V flash chip, GPIO12 must be low at reset. This is incompatible with SD card operation.
    * In most cases, external pullup can be omitted and an internal pullup can be enabled using a `gpio_pullup_en(GPIO_NUM_12);` call. Most SD cards work fine when an internal pullup on GPIO12 line is enabled. Note that if ESP32 experiences a power-on reset while the SD card is sending data, high level on GPIO12 can be latched into the bootstrapping register, and ESP32 will enter a boot loop until external reset with correct GPIO12 level is applied.
    * Another option is to burn the flash voltage selection efuses. This will permanently select 3.3V output voltage for the internal regulator, and GPIO12 will not be used as a bootstrapping pin. Then it is safe to connect a pullup resistor to GPIO12. This option is suggested for production use.

# Using FAT file system on External SPI Flash Memory
I tested these SPI Flash Memory.   
https://github.com/nopnop2002/esp-idf-w25q64

|#|W25Q64||ESP32|ESP32-S2/S3|ESP32-C2/C3/C6|
|:-:|:-:|:-:|:-:|:-:|:-:|
|1|/CS|--|GPIO5|GPIO10|GPIO4|
|2|MISO|--|GPIO19|GPIO13|GPIO3|
|3|/WP|--|3.3V|3.3V|3.3V|
|4|GND|--|GND|GND|GND|
|5|MOSI|--|GPIO23|GPIO11|GPIO1|
|6|SCK|--|GPIO18|GPIO12|GPIO2|
|7|/HOLD|--|3.3V|3.3V|3.3V|
|8|VCC|--|3.3V|3.3V|3.3V|

![config-file-system-external-fatfs](https://github.com/user-attachments/assets/4f98df6c-1c4a-40a7-84e0-d6e7198f30e0)

Note: You will get an error. It works fine after a few resets. At the moment, it is not stable.   
```
I (2121) FTP: Initializing external SPI Flash
I (2121) FTP: Pin assignments:
I (2121) FTP: MOSI: 23   MISO: 19   SCLK: 18   CS:  5
E (2131) memspi: no response
E (2131) FTP: Failed to initialize external Flash: ESP_ERR_INVALID_RESPONSE (0x108)
```

After reset   
```
I (1621) FTP: Initializing external SPI Flash
I (1621) FTP: Pin assignments:
I (1631) FTP: MOSI: 23   MISO: 19   SCLK: 18   CS:  5
I (1631) spi_flash: detected chip: winbond
I (1641) spi_flash: flash io: dio
I (1641) FTP: Initialized external Flash, size=8192 KB, ID=0xef4017
I (1651) FTP: Adding external Flash as a partition, label="storage", size=8192 KB
I (1661) FTP: Initializing FAT file system
I (1661) FTP: Mount FAT filesystem on /root
```


# Using ESP32-CAM
The ESP32-CAM development board has a micro SD card slot on the board.   
It is connected to the ESP32 by SDMMC with 4-line Mode.   
__No equipment other than the development board is required.__   
It works very stably.   

![ESP32-CAM-1](https://github.com/user-attachments/assets/89317896-e88e-460a-b658-e224ae5b9d83)
![ESP32-CAM-2](https://github.com/user-attachments/assets/911b6683-d93a-4a9b-8f18-3c46515d698b)
![ESP32-CAM-3](https://github.com/user-attachments/assets/287c2e53-f80a-457e-aba3-bfe745c3e2b8)


# Using LilyGo ESP32-S2
The LilyGo ESP32-S2 development board has a micro SD card slot on the board.   
It is connected to the ESP32 by SPI, and the peripheral power is supplied from GPIO14.   
__No equipment other than the development board is required.__   
It works very stably.   

|ESP32 pin|SPI bus signal|
|:-:|:-:|
|GPIO11|MOSI|
|GPIO13|MISO|
|GPIO12|SCK|
|GPIO10|CS|
|GPIO14|POWER|

![LilyGo-esp32-s2-1](https://user-images.githubusercontent.com/6020549/107864770-00f96100-6ea3-11eb-8549-6885ae398111.JPG)
![LilyGo_ESP32-S2-2](https://github.com/user-attachments/assets/69513704-cdd7-47f3-a48a-756524c4b5fb)
![LilyGo_ESP32-S2-3](https://github.com/user-attachments/assets/6d62b8ba-696c-4907-99a2-1fb6cc862efe)




# API
Based on [ftplib](https://nbpfaus.net/~pfau/ftplib/ftplib.html) V4.0-1.   

## Server Connection
- ftpClientConnect() - Connect to a remote server
- ftpClientLogin() - Login to remote machine
- ftpClientQuit() - Disconnect from remote server
- ftpClientSetOptions() - Set Connection Options

## Directory Functions
- ftpClientChangeDir() - Change working directory
- ftpClientMakeDir() - Create a directory
- ftpClientRemoveDir() - Remove a directory
- ftpClientDir() - List a remote directory
- ftpClientNlst() - List a remote directory
- ftpClientChangeDirUp() - Change to parent directory
- ftpClientPwd() - Determine current working directory

## File to File Transfer
- ftpClientGet() - Retreive a remote file
- ftpClientPut() - Send a local file to remote
- ftpClientDelete() - Delete a remote file
- ftpClientRename() - Rename a remote file

## File to Program Transfer
These routines allow programs access to the data streams connected to remote files and directories.   
- ftpClientAccess() - Open a remote file or directory
- ftpClientRead() - Read from remote file or directory
- ftpClientWrite() - Write to remote file
- ftpClientClose() - Close data connection

# Using long file name support   
By default, FATFS file names can be up to 8 characters long.   
If you use filenames longer than 8 characters, you need to change the values below.   
![config_long_file_name_support-1](https://github.com/nopnop2002/esp-idf-ftpServer/assets/6020549/dbae8910-74c9-4702-bdd4-881246e3fb95)
![config_long_file_name_support-2](https://github.com/nopnop2002/esp-idf-ftpServer/assets/6020549/75d1ea99-86ff-40c0-8ffc-8bd30b9fc32e)
![config_long_file_name_support-3](https://github.com/nopnop2002/esp-idf-ftpServer/assets/6020549/6e381627-e6f4-494d-ad40-04a73b49727b)


# Screen Shot   
![ScrrenShot](https://user-images.githubusercontent.com/6020549/107837485-5f133f00-6de4-11eb-9fe8-775443c6836d.jpg)
- Get remote file list   
- Write local file   
- Put file to server   
- Remove local file   
- Get file from sever   
- Read local file   


# Truble shooting   
By changing this, you can see the response from the server:
```
#define FTP_CLIENT_DEBUG                    2
```


# FTP Server using python
https://github.com/nopnop2002/esp-idf-ftpClient/tree/master/python-ftp-server


# Reference   
- FTP Server using FAT File system.   
Since it uses the FAT file system instead of SPIFFS, directory operations are possible.   
https://github.com/nopnop2002/esp-idf-ftpServer   

- File copy using scp.   
https://github.com/nopnop2002/esp-idf-scp-client

- File copy using smb.   
https://github.com/nopnop2002/esp-idf-smb-client

