idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES "esp-idf-ftpClient" "esp_wifi" "vfs" "nvs_flash" "fatfs" "spiffs" "spi_flash"
                    )
