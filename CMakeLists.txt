idf_component_register(SRCS "src/FtpClient.c"
                    INCLUDE_DIRS "include"
                    REQUIRES 
                    #"esp_wifi" "nvs_flash" 
                    "driver"
#                    "module_mgt" "esp_psram"
                             #"esp_modem"
                             )
