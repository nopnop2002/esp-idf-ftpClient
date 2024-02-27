/*
	FTP Client example.
	This example code is in the Public Domain (or CC0 licensed, at your option.)

	Unless required by applicable law or agreed to in writing, this
	software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
	CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <inttypes.h>
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

// External Flash Stuff
#include "esp_flash.h"
#include "esp_flash_spi_init.h"
#include "esp_partition.h"
//#include "soc/spi_pins.h"

// h2 and c2 will not support external flash
//#define EXAMPLE_FLASH_FREQ_MHZ 40

// ESP32 (VSPI)
#ifdef CONFIG_IDF_TARGET_ESP32
#define HOST_ID  SPI3_HOST
#define SPI_DMA_CHAN SPI_DMA_CH_AUTO
#else // Other chips (SPI2/HSPI)
#define HOST_ID  SPI2_HOST
#define SPI_DMA_CHAN SPI_DMA_CH_AUTO
#endif

#include "FtpClient.h"

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0))
#define esp_vfs_fat_spiflash_mount esp_vfs_fat_spiflash_mount_rw_wl
#define esp_vfs_fat_spiflash_unmount esp_vfs_fat_spiflash_unmount_rw_wl
#endif

static const char *TAG = "FTP";
static char *MOUNT_POINT = "/root";

//for test
//#define CONFIG_SPIFFS 1
//#define CONFIG_FATFS	1
//#define CONFIG_SPI_SDCARD  1
//#define CONFIG_MMC_SDCARD  1
//#define CONFIG_SPI_FLASH	 1

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static int s_retry_num = 0;

static void event_handler(void* arg, esp_event_base_t event_base,
								int32_t event_id, void* event_data)
{
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
		esp_wifi_connect();
	} else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
		if (s_retry_num < CONFIG_ESP_MAXIMUM_RETRY) {
			esp_wifi_connect();
			s_retry_num++;
			ESP_LOGI(TAG, "retry to connect to the AP");
		} else {
			xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
		}
		ESP_LOGI(TAG,"connect to the AP fail");
	} else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
		ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
		ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
		s_retry_num = 0;
		xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
	}
} 


esp_err_t wifi_init_sta()
{
	esp_err_t ret_value = ESP_OK;
	s_wifi_event_group = xEventGroupCreate();

	ESP_LOGI(TAG,"ESP-IDF Ver%d.%d", ESP_IDF_VERSION_MAJOR, ESP_IDF_VERSION_MINOR);
	ESP_LOGI(TAG,"ESP_IDF_VERSION %d", ESP_IDF_VERSION);

	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	esp_netif_create_default_wifi_sta();

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));
	ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

	wifi_config_t wifi_config = {
		.sta = {
			.ssid = CONFIG_ESP_WIFI_SSID,
			.password = CONFIG_ESP_WIFI_PASSWORD
		},
	};
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
	ESP_ERROR_CHECK(esp_wifi_start() );

	/* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
	 * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
	EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
		WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
		pdFALSE,			// xClearOnExit
		pdFALSE,			// xWaitForAllBits
		portMAX_DELAY);

	/* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
	 * happened. */
	if (bits & WIFI_CONNECTED_BIT) {
		ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
			 CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD);
	} else if (bits & WIFI_FAIL_BIT) {
		ESP_LOGE(TAG, "Failed to connect to SSID:%s, password:%s",
			 CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD);
		ret_value = ESP_FAIL;
	} else {
		ESP_LOGE(TAG, "UNEXPECTED EVENT");
		ret_value = ESP_ERR_INVALID_STATE;
	}

	ESP_LOGI(TAG, "wifi_init_sta finished.");
	ESP_LOGI(TAG, "connect to ap SSID:%s", CONFIG_ESP_WIFI_SSID);
	vEventGroupDelete(s_wifi_event_group); 
	return ret_value; 
}

#if CONFIG_SPI_FLASH
static esp_flash_t* init_ext_flash(void)
{
	const spi_bus_config_t bus_config = {
		.mosi_io_num = CONFIG_SPI_MOSI,
		.miso_io_num = CONFIG_SPI_MISO,
		.sclk_io_num = CONFIG_SPI_CLK,
		.quadhd_io_num = -1,
		.quadwp_io_num = -1,
	};

	const esp_flash_spi_device_config_t device_config = {
		.host_id = HOST_ID,
		.cs_id = 0,
		.cs_io_num = CONFIG_SPI_CS,
		.io_mode = SPI_FLASH_DIO,
		.freq_mhz = ESP_FLASH_40MHZ,
	};

	ESP_LOGI(TAG, "Initializing external SPI Flash");
	ESP_LOGI(TAG, "Pin assignments:");
	ESP_LOGI(TAG, "MOSI: %2d MISO: %2d SCLK: %2d CS: %2d",
		bus_config.mosi_io_num, bus_config.miso_io_num,
		bus_config.sclk_io_num, device_config.cs_io_num
	);

	// Initialize the SPI bus
	ESP_LOGI(TAG, "DMA CHANNEL: %d", SPI_DMA_CHAN);
	ESP_ERROR_CHECK(spi_bus_initialize(HOST_ID, &bus_config, SPI_DMA_CHAN));

	// Add device to the SPI bus
	esp_flash_t* ext_flash;
	ESP_ERROR_CHECK(spi_bus_add_flash_device(&ext_flash, &device_config));

	// Probe the Flash chip and initialize it
	esp_err_t err = esp_flash_init(ext_flash);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Failed to initialize external Flash: %s (0x%x)", esp_err_to_name(err), err);
		return NULL;
	}

	// Print out the ID and size
	uint32_t id;
	ESP_ERROR_CHECK(esp_flash_read_id(ext_flash, &id));
	ESP_LOGI(TAG, "Initialized external Flash, size=%"PRIu32" KB, ID=0x%"PRIx32, ext_flash->size / 1024, id);

	return ext_flash;
}


static const esp_partition_t* add_partition(esp_flash_t* ext_flash, const char* partition_label)
{
	ESP_LOGI(TAG, "Adding external Flash as a partition, label=\"%s\", size=%"PRIu32" KB", partition_label, ext_flash->size / 1024);
	const esp_partition_t* fat_partition;
	ESP_ERROR_CHECK(esp_partition_register_external(ext_flash, 0, ext_flash->size, partition_label, ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_FAT, &fat_partition));
	return fat_partition;
}
#endif // CONFIG_SPI_FLASH

#if CONFIG_SPIFFS 
esp_err_t mountSPIFFS(char * partition_label, char * mount_point) {
	ESP_LOGI(TAG, "Initializing SPIFFS file system");

	esp_vfs_spiffs_conf_t conf = {
		.base_path = mount_point,
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
		ESP_LOGI(TAG, "Mount SPIFFS filesystem on %s", mount_point);
	}
	//ESP_LOGI(TAG, "Mount SPIFFS filesystem");
	return ret;
}
#endif

#if CONFIG_FATFS || CONFIG_SPI_FLASH
wl_handle_t mountFATFS(char * partition_label, char * mount_point) {
	ESP_LOGI(TAG, "Initializing FAT file system");
	// To mount device we need name of device partition, define base_path
	// and allow format partition in case if it is new one and was not formated before
	const esp_vfs_fat_mount_config_t mount_config = {
		.max_files = 4,
		.format_if_mount_failed = true,
		.allocation_unit_size = CONFIG_WL_SECTOR_SIZE
	};
	wl_handle_t s_wl_handle;
	esp_err_t err = esp_vfs_fat_spiflash_mount(mount_point, partition_label, &mount_config, &s_wl_handle);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Failed to mount FATFS (%s)", esp_err_to_name(err));
		return -1;
	}
	ESP_LOGI(TAG, "Mount FAT filesystem on %s", mount_point);
	ESP_LOGI(TAG, "s_wl_handle=%"PRIi32, s_wl_handle);
	return s_wl_handle;
}
#endif // CONFIG_FATFS || CONFIG_SPI_FLASH

#if CONFIG_SPI_SDCARD || CONFIG_MMC_SDCARD
esp_err_t mountSDCARD(char * mount_point, sdmmc_card_t * card) {

	esp_err_t ret;
	// Options for mounting the filesystem.
	// If format_if_mount_failed is set to true, SD card will be partitioned and
	// formatted in case when mounting fails.
	esp_vfs_fat_sdmmc_mount_config_t mount_config = {
		.format_if_mount_failed = true,
		.max_files = 5,
		.allocation_unit_size = 16 * 1024
	};
	//sdmmc_card_t* card;

#if CONFIG_MMC_SDCARD
	// Use settings defined above to initialize SD card and mount FAT filesystem.
	// Note: esp_vfs_fat_sdmmc/sdspi_mount is all-in-one convenience functions.
	// Please check its source code and implement error recovery when developing
	// production applications.

	ESP_LOGI(TAG, "Initializing SDMMC peripheral");
	sdmmc_host_t host = SDMMC_HOST_DEFAULT();

	// This initializes the slot without card detect (CD) and write protect (WP) signals.
	// Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
	sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

	// Set bus width to use:
#ifdef CONFIG_SDMMC_BUS_WIDTH_4
	ESP_LOGI(TAG, "SDMMC 4 line mode");
	slot_config.width = 4;
#else
	ESP_LOGI(TAG, "SDMMC 1 line mode");
	slot_config.width = 1;
#endif

	// On chips where the GPIOs used for SD card can be configured, set them in
	// the slot_config structure:
#ifdef SOC_SDMMC_USE_GPIO_MATRIX
	ESP_LOGI(TAG, "SOC_SDMMC_USE_GPIO_MATRIX");
	slot_config.clk = CONFIG_SDMMC_CLK; //GPIO_NUM_36;
	slot_config.cmd = CONFIG_SDMMC_CMD; //GPIO_NUM_35;
	slot_config.d0 = CONFIG_SDMMC_D0; //GPIO_NUM_37;
#ifdef CONFIG_SDMMC_BUS_WIDTH_4
	slot_config.d1 = CONFIG_SDMMC_D1; //GPIO_NUM_38;
	slot_config.d2 = CONFIG_SDMMC_D2; //GPIO_NUM_33;
	slot_config.d3 = CONFIG_SDMMC_D3; //GPIO_NUM_34;
#endif // CONFIG_SDMMC_BUS_WIDTH_4
#endif // SOC_SDMMC_USE_GPIO_MATRIX

	// Enable internal pullups on enabled pins. The internal pullups
	// are insufficient however, please make sure 10k external pullups are
	// connected on the bus. This is for debug / example purpose only.
	slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

	ESP_LOGI(TAG, "Mounting filesystem");
	ret = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &card);
	
#if 0
	// GPIOs 15, 2, 4, 12, 13 should have external 10k pull-ups.
	// Internal pull-ups are not sufficient. However, enabling internal pull-ups
	// does make a difference some boards, so we do that here.
	gpio_set_pull_mode(15, GPIO_PULLUP_ONLY);	// CMD, needed in 4- and 1- line modes
	gpio_set_pull_mode(2, GPIO_PULLUP_ONLY);	// D0, needed in 4- and 1-line modes
	gpio_set_pull_mode(4, GPIO_PULLUP_ONLY);	// D1, needed in 4-line mode only
	gpio_set_pull_mode(12, GPIO_PULLUP_ONLY);	// D2, needed in 4-line mode only
	gpio_set_pull_mode(13, GPIO_PULLUP_ONLY);	// D3, needed in 4- and 1-line modes

	ret = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &card);
#endif
#endif // CONFIG_MMC_SDCARD

#if CONFIG_SPI_SDCARD
	ESP_LOGI(TAG, "Initializing SPI peripheral");
	ESP_LOGI(TAG, "SPI_MOSI=%d", CONFIG_SPI_MOSI);
	ESP_LOGI(TAG, "SPI_MISO=%d", CONFIG_SPI_MISO);
	ESP_LOGI(TAG, "SPI_CLK=%d", CONFIG_SPI_CLK);
	ESP_LOGI(TAG, "SPI_CS=%d", CONFIG_SPI_CS);
	ESP_LOGI(TAG, "SPI_POWER=%d", CONFIG_SPI_POWER);

	if (CONFIG_SPI_POWER != -1) {
		//gpio_pad_select_gpio(CONFIG_SPI_POWER);
		gpio_reset_pin(CONFIG_SPI_POWER);
		/* Set the GPIO as a push/pull output */
		gpio_set_direction(CONFIG_SPI_POWER, GPIO_MODE_OUTPUT);
		ESP_LOGI(TAG, "Turning on the peripherals power using GPIO%d", CONFIG_SPI_POWER);
		gpio_set_level(CONFIG_SPI_POWER, 1);
		vTaskDelay(3000 / portTICK_PERIOD_MS);
	}


	sdmmc_host_t host = SDSPI_HOST_DEFAULT();
	spi_bus_config_t bus_cfg = {
		.mosi_io_num = CONFIG_SPI_MOSI,
		.miso_io_num = CONFIG_SPI_MISO,
		.sclk_io_num = CONFIG_SPI_CLK,
		.quadwp_io_num = -1,
		.quadhd_io_num = -1,
		.max_transfer_sz = 4000,
	};
	ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Failed to initialize bus.");
		return ret;
	}
	// This initializes the slot without card detect (CD) and write protect (WP) signals.
	// Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
	sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
	slot_config.gpio_cs = CONFIG_SPI_CS;
	slot_config.host_id = host.slot;

	ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);
#endif // CONFIG_SPI_SDCARD

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
	ESP_LOGI(TAG, "Mounte SD card on %s", mount_point);
	return ret;
}
#endif // CONFIG_SPI_SDCARD || CONFIG_MMC_SDCARD

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
	if (wifi_init_sta() != ESP_OK) {
		ESP_LOGE(TAG, "Connection failed");
		while(1) { vTaskDelay(1); }
	}

#if CONFIG_SPIFFS 
	char *partition_label = "storage0";
	ret = mountSPIFFS(partition_label, MOUNT_POINT);
	if (ret != ESP_OK) return;
#endif

#if CONFIG_FATFS
	char *partition_label = "storage1";
	wl_handle_t s_wl_handle = mountFATFS(partition_label, MOUNT_POINT);
	if (s_wl_handle < 0) return;
#endif 

#if CONFIG_SPI_SDCARD || CONFIG_MMC_SDCARD
	sdmmc_card_t card;
	ret = mountSDCARD(MOUNT_POINT, &card);
	if (ret != ESP_OK) return;
#endif 

#if CONFIG_SPI_FLASH
	// Set up SPI bus and initialize the external SPI Flash chip
	esp_flash_t* flash = init_ext_flash();
	if (flash == NULL) return;
	// Add the entire external flash chip as a partition
	char *partition_label = "storage";
	add_partition(flash, partition_label);
	wl_handle_t s_wl_handle = mountFATFS(partition_label, MOUNT_POINT);
	if (s_wl_handle < 0) return;
#endif

	char srcFileName[64];
	char dstFileName[64];
	char outFileName[64];
	sprintf(srcFileName, "%s/hello.txt", MOUNT_POINT);
	sprintf(dstFileName, "hello.txt");
	sprintf(outFileName, "%s/out.txt", MOUNT_POINT);

	// Open FTP server
	ESP_LOGI(TAG, "ftp server:%s", CONFIG_FTP_SERVER);
	ESP_LOGI(TAG, "ftp user  :%s", CONFIG_FTP_USER);
	static NetBuf_t* ftpClientNetBuf = NULL;
	FtpClient* ftpClient = getFtpClient();
	//int connect = ftpClient->ftpClientConnect(CONFIG_FTP_SERVER, 21, &ftpClientNetBuf);
	//int connect = ftpClient->ftpClientConnect(CONFIG_FTP_SERVER, 2121, &ftpClientNetBuf);
	int connect = ftpClient->ftpClientConnect(CONFIG_FTP_SERVER, CONFIG_FTP_PORT, &ftpClientNetBuf);
	ESP_LOGI(TAG, "connect=%d", connect);
	if (connect == 0) {
		ESP_LOGE(TAG, "FTP server connect fail");
		return;
	}

	// Login FTP server
	int login = ftpClient->ftpClientLogin(CONFIG_FTP_USER, CONFIG_FTP_PASSWORD, ftpClientNetBuf);
	ESP_LOGI(TAG, "login=%d", login);
	if (login == 0) {
		ESP_LOGE(TAG, "FTP server login fail");
		return;
	}

	// Remote Directory
	char line[128];
	//ftpClient->ftpClientDir(outFileName, "/", ftpClientNetBuf);
	ftpClient->ftpClientDir(outFileName, ".", ftpClientNetBuf);
	FILE* f = fopen(outFileName, "r");
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
	ESP_LOGI(TAG, "");

	// Use POSIX and C standard library functions to work with files.
	// Create file
	f = fopen(srcFileName, "w");
	if (f == NULL) {
		ESP_LOGE(TAG, "Failed to open file for writing");
		return;
	}
	fprintf(f, "Hello World!\n");
	fclose(f);
	ESP_LOGI(TAG, "Wrote the text on %s", srcFileName);

	// Put file to FTP server
	ftpClient->ftpClientPut(srcFileName, dstFileName, FTP_CLIENT_TEXT, ftpClientNetBuf);
	ESP_LOGI(TAG, "ftpClientPut %s ---> %s", srcFileName, dstFileName);

	// Delete file
	unlink(srcFileName);
	ESP_LOGI(TAG, "Deleted %s", srcFileName);

	// Get file from FTP server
	ftpClient->ftpClientGet(srcFileName, dstFileName, FTP_CLIENT_TEXT, ftpClientNetBuf);
	ESP_LOGI(TAG, "ftpClientGet %s <--- %s", srcFileName, dstFileName);

	// Open file for reading
	ESP_LOGI(TAG, "Open %s", srcFileName);
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
	ESP_LOGI(TAG, "Read from %s: '%s'", srcFileName, line);

	ftpClient->ftpClientQuit(ftpClientNetBuf);

#if CONFIG_SPIFFS
	esp_vfs_spiffs_unregister(NULL);
	ESP_LOGI(TAG, "SPIFFS unmounted");
#endif

#if CONFIG_FATFS
	esp_vfs_fat_spiflash_unmount(MOUNT_POINT, s_wl_handle);
	ESP_LOGI(TAG, "FATFS unmounted");
#endif 

#if CONFIG_SPI_SDCARD || CONFIG_MMC_SDCARD
	esp_vfs_fat_sdcard_unmount(MOUNT_POINT, &card);
	ESP_LOGI(TAG, "SDCARD unmounted");
#endif 

}
