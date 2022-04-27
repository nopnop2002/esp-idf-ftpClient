# esp-idf-ftpClient
FTP Client for esp-idf.   
This project use [ESP32-FTP-Client](https://github.com/JohnnyB1290/ESP32-FTP-Client). It's a great job.   

# Installation for 4M Flash like ESP32
```
git clone https://github.com/nopnop2002/esp-idf-ftpClient
cd esp-idf-ftpClient/
cp partitions_example.csv.esp32 partitions_example.csv
idf.py set-target {esp32/esp32s2/esp32s3}
idf.py menuconfig
idf.py flash
```

__If you need more space on FLASH, you need to modify partitions_example.csv.__   

# Installation for 2M Flash like ESP32C3
```
git clone https://github.com/nopnop2002/esp-idf-ftpClient
cd esp-idf-ftpClient/
cp partitions_example.csv.esp32c3 partitions_example.csv
idf.py set-target esp32c3
idf.py menuconfig
idf.py flash
```

__If you need more space on FLASH, you need to modify partitions_example.csv.__   


# Configuration

![config-top](https://user-images.githubusercontent.com/6020549/165466546-f6609f93-679d-4de9-9b45-db11b723d815.jpg)
![config-app](https://user-images.githubusercontent.com/6020549/165466556-7c5770da-e75b-4aa8-a49f-43c975233809.jpg)

## File System Setting   
ESP32 supports the following file systems.   
You can select any one using menuconfig.   
- SPIFFS file system on FLASH   
- FAT file system on FLASH   
- FAT file system on SPI peripheral SDCARD   
- FAT file system on SDMMC peripheral SDCARD(Valid only for ESP32/ESP32S3)   
- FAT file system on External Flash like Winbond W25Q64(Valid only for ESP32)    

![config-file-system-1](https://user-images.githubusercontent.com/6020549/165466672-edf8c8f7-6505-4df7-ad82-c78d63198271.jpg)
![config-file-system-2](https://user-images.githubusercontent.com/6020549/165466686-f8760b46-d93b-4f11-8664-2dfdb37d6f91.jpg)

Note:   
The connection when using SDSPI, SDMMC, and External flash will be described later.   

## Wifi Setting   

![config-wifi](https://user-images.githubusercontent.com/6020549/165469815-dfe6407b-4598-4cc7-8f60-4ceef4618529.jpg)


## FTP Server Setting   

![config-ftp-server](https://user-images.githubusercontent.com/6020549/165469878-2e591cca-a740-4821-b3bf-53fef6dc6dd0.jpg)


# Using FAT file system on SPI peripheral SDCARD

|ESP32|ESP32S2/S3|ESP32C3|SD card pin|Notes|
|:-:|:-:|:-:|:-:|:--|
|GPIO23|GPIO35|GPIO04|MOSI|10k pull up if can't mount|
|GPIO19|GPIO37|GPIO06|MISO|10k pull up if can't mount|
|GPIO18|GPIO36|GPIO05|SCK|10k pull up if can't mount|
|GPIO13|GPIO34|GPIO01|CS|10k pull up if can't mount|
|3.3V|3.3V|3.3V|VCC|Don't use 5V supply|
|GND|GND|GND|GND||

__You can change it to any pin using menuconfig.__   

Note:   
This example doesn't utilize card detect (CD) and write protect (WP) signals from SD card slot.   


# Using FAT file system on SDMMC peripheral SDCARD

|ESP32 pin|SD card pin|Notes|
|:-:|:-:|:--|
|GPIO14|CLK|10k pullup|
|GPIO15|CMD|10k pullup|
|GPIO2|D0|10k pullup|
|GPIO4|D1|not used in 1-line SD mode; 10k pullup in 4-line SD mode|
|GPIO12|D2|not used in 1-line SD mode; 10k pullup in 4-line SD mode|
|GPIO13|D3|not used in 1-line SD mode, but card's D3 pin must have a 10k pullup
|N/C|CD|not used in this project|
|N/C|WP|not used in this project|
|3.3V|VCC|Don't use 5V supply|
|GND|GND||

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


## Note about GPIO2 (ESP32 only)   
GPIO2 pin is used as a bootstrapping pin, and should be low to enter UART download mode.   
One way to do this is to connect GPIO0 and GPIO2 using a jumper, and then the auto-reset circuit on most development boards will pull GPIO2 low along with GPIO0, when entering download mode.

## Note about GPIO12 (ESP32 only)   
GPIO12 is used as a bootstrapping pin to select output voltage of an internal regulator which powers the flash chip (VDD_SDIO).   
This pin has an internal pulldown so if left unconnected it will read low at reset (selecting default 3.3V operation).   
When adding a pullup to this pin for SD card operation, consider the following:
- For boards which don't use the internal regulator (VDD_SDIO) to power the flash, GPIO12 can be pulled high.
- For boards which use 1.8V flash chip, GPIO12 needs to be pulled high at reset. This is fully compatible with SD card operation.
- On boards which use the internal regulator and a 3.3V flash chip, GPIO12 must be low at reset. This is incompatible with SD card operation.
    * In most cases, external pullup can be omitted and an internal pullup can be enabled using a `gpio_pullup_en(GPIO_NUM_12);` call. Most SD cards work fine when an internal pullup on GPIO12 line is enabled. Note that if ESP32 experiences a power-on reset while the SD card is sending data, high level on GPIO12 can be latched into the bootstrapping register, and ESP32 will enter a boot loop until external reset with correct GPIO12 level is applied.
    * Another option is to burn the flash voltage selection efuses. This will permanently select 3.3V output voltage for the internal regulator, and GPIO12 will not be used as a bootstrapping pin. Then it is safe to connect a pullup resistor to GPIO12. This option is suggested for production use.

# Using FAT file system on External Flash
I tested these External Flash.   
https://github.com/nopnop2002/esp-idf-w25q64

|ESP32 pin|SPI bus signal|SPI Flash pin|
|:-:|:-:|:-:|
|GPIO23|MOSI|DI|
|GPIO19|MISO|DO|
|GPIO18|SCK|CLK|
|GPIO5|CS|CMD|
|3.3V||/WP|
|3.3V||/HOLD|
|3.3V||VCC|
|GND||GND|

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

# API
Based on ftplib V4.0-1.   
You can use [working documentation of ftplib](https://nbpfaus.net/~pfau/ftplib/ftplib.html).   

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


# Screen Shot   
![ScrrenShot](https://user-images.githubusercontent.com/6020549/107837485-5f133f00-6de4-11eb-9fe8-775443c6836d.jpg)
- Get remote file list   
- Write local file   
- Put file to server   
- Remove local file   
- Get file from sever   
- Read local file   

# Reference   
- FTP Server using FAT File system.   
Since it uses the FAT file system instead of SPIFFS, directory operations are possible.   
https://github.com/nopnop2002/esp-idf-ftpServer   

- File copy using scp.   
https://github.com/nopnop2002/esp-idf-scp-client

- File copy using smb.   
https://github.com/nopnop2002/esp-idf-smb-client

