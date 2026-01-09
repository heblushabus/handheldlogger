idf_component_register(SRCS "wifi_time_manager.c" "main.c" "vbat_driver.c" "u8g2_manager.c" "bme680_manager.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES esp_adc log u8g2 driver bme68x_lib bsec2 nvs_flash esp_wifi esp_netif esp_event)
