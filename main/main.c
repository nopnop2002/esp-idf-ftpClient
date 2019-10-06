/* SPIFFS filesystem example.
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "nvs_flash.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "sdmmc_cmd.h"
#include "esp_spiffs.h" 

#include "lwip/err.h"
#include "lwip/sys.h"

#include "FtpClient.h"

/* The examples use WiFi configuration that you can set via project configuration menu

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define EXAMPLE_ESP_WIFI_SSID	   CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS	   CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_ESP_MAXIMUM_RETRY  CONFIG_ESP_MAXIMUM_RETRY
#define EXAMPLE_FTP_SERVER		   CONFIG_FTP_SERVER
#define EXAMPLE_FTP_USER		   CONFIG_FTP_USER	
#define EXAMPLE_FTP_PASSWORD	   CONFIG_FTP_PASSWORD

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about one event
 * - are we connected to the AP with an IP? */
const int WIFI_CONNECTED_BIT = BIT0;

static const char *TAG = "ftpClient";

//#define CONFIG_SPIFFS 1
//#define CONFIG_FATFS	1
//#define CONFIG_SPI_SDCARD  1
//#define CONFIG_MMC_SDCARD  1

#if CONFIG_SPI_SDCARD
// Pin mapping when using SPI mode.
// With this mapping, SD card can be used both in SPI and 1-line SD mode.
// Note that a pull-up on CS line is required in SD mode.
#define PIN_NUM_MISO 2
#define PIN_NUM_MOSI 15
#define PIN_NUM_CLK  14
#define PIN_NUM_CS	 13
#endif 

static int s_retry_num = 0;

static void event_handler(void* arg, esp_event_base_t event_base,
								int32_t event_id, void* event_data)
{
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
		esp_wifi_connect();
	} else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
		if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
			esp_wifi_connect();
			xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
			s_retry_num++;
			ESP_LOGI(TAG, "retry to connect to the AP");
		}
		ESP_LOGE(TAG,"connect to the AP fail");
	} else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
		ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
		ESP_LOGI(TAG, "got ip:%s",
				 ip4addr_ntoa(&event->ip_info.ip));
		s_retry_num = 0;
		xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
	}
}

void wifi_init_sta(void)
{
	s_wifi_event_group = xEventGroupCreate();

	tcpip_adapter_init();

	ESP_ERROR_CHECK(esp_event_loop_create_default());

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

	wifi_config_t wifi_config = {
		.sta = {
			.ssid = EXAMPLE_ESP_WIFI_SSID,
			.password = EXAMPLE_ESP_WIFI_PASS
		},
	};
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
	ESP_ERROR_CHECK(esp_wifi_start() );

	xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
	ESP_LOGI(TAG, "wifi_init_sta finished.");
	ESP_LOGI(TAG, "connect to ap SSID:%s", EXAMPLE_ESP_WIFI_SSID);
}

#if CONFIG_SPIFFS 
esp_err_t mountSPIFFS(char * partition_label, char * base_path) {
	ESP_LOGI(TAG, "Initializing SPIFFS file system");

	esp_vfs_spiffs_conf_t conf = {
	  .base_path = base_path,
	  .partition_label = partition_label,
	  .max_files = 5,
	  .format_if_mount_failed = true
	};

	// Use settings defined above to initialize and mount SPIFFS filesystem.
	// Note: esp_vfs_spiffs_register is an all-in-one convenience function.
	esp_err_t ret = esp_vfs_spiffs_register(&conf);

	if (ret != ESP_OK) {
		if (ret == ESP_FAIL) {
			ESP_LOGE(TAG, "Failed to mount or format filesystem");
		} else if (ret == ESP_ERR_NOT_FOUND) {
			ESP_LOGE(TAG, "Failed to find SPIFFS partition");
		} else {
			ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
		}
		return ret;
	}

	size_t total = 0, used = 0;
	ret = esp_spiffs_info(partition_label, &total, &used);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
	} else {
		ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
	}
	ESP_LOGI(TAG, "Mount SPIFFS filesystem");
	return ret;
}
#endif

#if CONFIG_FATFS
wl_handle_t mountFATFS(char * partition_label, char * base_path) {
	ESP_LOGI(TAG, "Initializing FAT file system");
	// To mount device we need name of device partition, define base_path
	// and allow format partition in case if it is new one and was not formated before
	const esp_vfs_fat_mount_config_t mount_config = {
			.max_files = 4,
			.format_if_mount_failed = true,
			.allocation_unit_size = CONFIG_WL_SECTOR_SIZE
	};
	wl_handle_t s_wl_handle;
	esp_err_t err = esp_vfs_fat_spiflash_mount(base_path, partition_label, &mount_config, &s_wl_handle);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Failed to mount FATFS (%s)", esp_err_to_name(err));
		return -1;
	}
	ESP_LOGI(TAG, "Mount FAT filesystem");
	ESP_LOGI(TAG, "s_wl_handle=%d",s_wl_handle);
	return s_wl_handle;
}
#endif

#if CONFIG_SPI_SDCARD || CONFIG_MMC_SDCARD
esp_err_t mountSDCARD(char * base_path) {

#if CONFIG_MMC_SDCARD
	ESP_LOGI(TAG, "Initializing SDMMC peripheral");
	sdmmc_host_t host = SDMMC_HOST_DEFAULT();

	// This initializes the slot without card detect (CD) and write protect (WP) signals.
	// Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
	sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

	// To use 1-line SD mode, uncomment the following line:
	// slot_config.width = 1;

	// GPIOs 15, 2, 4, 12, 13 should have external 10k pull-ups.
	// Internal pull-ups are not sufficient. However, enabling internal pull-ups
	// does make a difference some boards, so we do that here.
	gpio_set_pull_mode(15, GPIO_PULLUP_ONLY);	// CMD, needed in 4- and 1- line modes
	gpio_set_pull_mode(2, GPIO_PULLUP_ONLY);	// D0, needed in 4- and 1-line modes
	gpio_set_pull_mode(4, GPIO_PULLUP_ONLY);	// D1, needed in 4-line mode only
	gpio_set_pull_mode(12, GPIO_PULLUP_ONLY);	// D2, needed in 4-line mode only
	gpio_set_pull_mode(13, GPIO_PULLUP_ONLY);	// D3, needed in 4- and 1-line modes
#endif

#if CONFIG_SPI_SDCARD
	ESP_LOGI(TAG, "Initializing SPI peripheral");
	sdmmc_host_t host = SDSPI_HOST_DEFAULT();
	sdspi_slot_config_t slot_config = SDSPI_SLOT_CONFIG_DEFAULT();
	slot_config.gpio_miso = PIN_NUM_MISO;
	slot_config.gpio_mosi = PIN_NUM_MOSI;
	slot_config.gpio_sck  = PIN_NUM_CLK;
	slot_config.gpio_cs   = PIN_NUM_CS;
	// This initializes the slot without card detect (CD) and write protect (WP) signals.
	// Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
#endif

	// Options for mounting the filesystem.
	// If format_if_mount_failed is set to true, SD card will be partitioned and
	// formatted in case when mounting fails.
	esp_vfs_fat_sdmmc_mount_config_t mount_config = {
		.format_if_mount_failed = false,
		.max_files = 5,
		.allocation_unit_size = 16 * 1024
	};

	// Use settings defined above to initialize SD card and mount FAT filesystem.
	// Note: esp_vfs_fat_sdmmc_mount is an all-in-one convenience function.
	// Please check its source code and implement error recovery when developing
	// production applications.
	sdmmc_card_t* card;
	esp_err_t ret = esp_vfs_fat_sdmmc_mount(base_path, &host, &slot_config, &mount_config, &card);

	if (ret != ESP_OK) {
		if (ret == ESP_FAIL) {
			ESP_LOGE(TAG, "Failed to mount filesystem. "
				"If you want the card to be formatted, set format_if_mount_failed = true.");
		} else {
			ESP_LOGE(TAG, "Failed to initialize the card (%s). "
				"Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
		}
		return ret;
	}

	// Card has been initialized, print its properties
	sdmmc_card_print_info(stdout, card);
	ESP_LOGI(TAG, "Mounte SD card");
	return ret;
}
#endif

void app_main(void)
{
	//Initialize NVS
	esp_err_t ret;
	ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
	  ESP_ERROR_CHECK(nvs_flash_erase());
	  ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

	ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
	wifi_init_sta();

#if CONFIG_SPIFFS 
	char *partition_label = "storage0";
	char *base_path = "/spiffs"; 
	ret = mountSPIFFS(partition_label, base_path);
	if (ret != ESP_OK) return;
#endif

#if CONFIG_FATFS
	char *partition_label = "storage1";
	char *base_path = "/spiflash";
	wl_handle_t s_wl_handle = mountFATFS(partition_label, base_path);
	if (s_wl_handle < 0) return;
#endif 

#if CONFIG_SPI_SDCARD || CONFIG_MMC_SDCARD
	char *base_path = "/sdcard";
	ret = mountSDCARD(base_path);
	if (ret != ESP_OK) return;
#endif 

	// Use POSIX and C standard library functions to work with files.
	// First create a file
	char srcFileName[64];
	char dstFileName[64];
	char outFileName[64];
	sprintf(srcFileName, "%s/hello.txt", base_path);
	sprintf(dstFileName, "hello.txt");
	sprintf(outFileName, "%s/out.txt", base_path);
	ESP_LOGI(TAG, "Opening file");
	FILE* f = fopen(srcFileName, "w");
	if (f == NULL) {
		ESP_LOGE(TAG, "Failed to open file for writing");
		return;
	}
	fprintf(f, "Hello World!\n");
	fclose(f);
	ESP_LOGI(TAG, "File written");

	// Put to ftp serve
	ESP_LOGI(TAG, "ftp server:%s", EXAMPLE_FTP_SERVER);
	ESP_LOGI(TAG, "ftp user  :%s", EXAMPLE_FTP_USER);
	static NetBuf_t* ftpClientNetBuf = NULL;
	FtpClient* ftpClient = getFtpClient();
	int connect = ftpClient->ftpClientConnect(EXAMPLE_FTP_SERVER, 21, &ftpClientNetBuf);
	ESP_LOGI(TAG, "connect=%d", connect);
	if (connect == 0) {
		ESP_LOGE(TAG, "FTP server connect fail");
		return;
	}

	int login = ftpClient->ftpClientLogin(EXAMPLE_FTP_USER, EXAMPLE_FTP_PASSWORD, ftpClientNetBuf);
	ESP_LOGI(TAG, "login=%d", login);
	if (login == 0) {
		ESP_LOGE(TAG, "FTP server login fail");
		return;
	}

	// Remore Directory
	char line[128];
	ftpClient->ftpClientDir(outFileName, "/", ftpClientNetBuf);
	f = fopen(outFileName, "r");
	if (f == NULL) {
		ESP_LOGE(TAG, "Failed to open file for reading");
		return;
	}
	while (fgets(line, sizeof(line), f) != NULL) {
		int len = strlen(line);
		line[len-1] = 0;
		ESP_LOGI(TAG, "%s", line);
	}
	fclose(f);

	// Put to FTP server
	ftpClient->ftpClientPut(srcFileName, dstFileName, FTP_CLIENT_TEXT, ftpClientNetBuf);
	ESP_LOGI(TAG, "ftpClientPut %s ---> %s", srcFileName, dstFileName);

	// Delete file
	unlink(srcFileName);
	ESP_LOGI(TAG, "File removed");

	// Get from FTP server
	ftpClient->ftpClientGet(srcFileName, dstFileName, FTP_CLIENT_TEXT, ftpClientNetBuf);
	ESP_LOGI(TAG, "ftpClientGet %s <--- %s", srcFileName, dstFileName);

	// Open file for reading
	ESP_LOGI(TAG, "Reading file");
	f = fopen(srcFileName, "r");
	if (f == NULL) {
		ESP_LOGE(TAG, "Failed to open file for reading");
		return;
	}
	fgets(line, sizeof(line), f);
	fclose(f);
	// strip newline
	char* pos = strchr(line, '\n');
	if (pos) {
		*pos = '\0';
	}
	ESP_LOGI(TAG, "Read from file: '%s'", line);

	ftpClient->ftpClientQuit(ftpClientNetBuf);

#if CONFIG_SPIFFS
	esp_vfs_spiffs_unregister(NULL);
	ESP_LOGI(TAG, "SPIFFS unmounted");
#endif

#if CONFIG_FATFS
	esp_vfs_fat_spiflash_unmount(base_path, s_wl_handle);
	ESP_LOGI(TAG, "FATFS unmounted");
#endif 

#if CONFIG_SPI_SDCARD || CONFIG_MMC_SDCARD
	esp_vfs_fat_sdmmc_unmount();
	ESP_LOGI(TAG, "SDCARD unmounted");
#endif 
}
