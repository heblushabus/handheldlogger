#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize Wi-Fi (Station Mode) and synchronize time via SNTP
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t wifi_time_manager_init(void);

#ifdef __cplusplus
}
#endif
