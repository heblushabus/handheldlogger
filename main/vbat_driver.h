#ifndef VBAT_DRIVER_H
#define VBAT_DRIVER_H

#include "esp_err.h"
#include "common_data.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Initialize the ADC driver for battery voltage monitoring (GPIO 1)
     *
     * Handles internal handles for one-shot unit and calibration.
     *
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t vbat_driver_init(void);

    /**
     * @brief Read battery voltage directly into latest_data_t struct
     *
     * @param data Pointer to the unified data struct
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t vbat_driver_read();

    /**
     * @brief Read battery voltage in millivolts (legacy)
     *
     * @param voltage_mv Pointer to store the result
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t vbat_driver_read_voltage(int *voltage_mv);

    /**
     * @brief Read the raw ADC data
     *
     * @param raw_out Pointer to store the result
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t vbat_driver_read_raw(int *raw_out);

#ifdef __cplusplus
}
#endif

#endif // ADC_DRIVER_H
