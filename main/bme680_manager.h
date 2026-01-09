#ifndef BME680_MANAGER_H
#define BME680_MANAGER_H

#include "bsec2.h"
#include "driver/i2c_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Initialize the BME680 sensor with BSEC
     *
     * @param bus_handle The I2C bus handle to use
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t bme680_manager_init(i2c_master_bus_handle_t bus_handle);

    /**
     * @brief Set the BSEC configuration from header file
     *
     * @param bsec The BSEC instance
     * @return esp_err_t ESP_OK on success, ESP_FAIL otherwise
     */
    esp_err_t bme68x_set_config(bsec2_t *bsec);

#include "common_data.h"

    /**
     * @brief Read data from the BME680 sensor (BSEC processed)
     *
     * @param data Pointer to a latest_data_t structure to store the formatted data
     * @return esp_err_t ESP_OK on success, ESP_FAIL otherwise
     */
    esp_err_t bme680_manager_read();

    /**
     * @brief Load BSEC state from NVS
     */
    void bme68x_load_state(bsec2_t *bsec);

    /**
     * @brief Save BSEC state to NVS
     */
    void bme68x_save_state(bsec2_t *bsec);

    /**
     * @brief Force save BSEC state (Wrapper for internal BSEC instance)
     */
    void bme680_manager_save_state(void);

    /**
     * @brief Run the BSEC algorithm (poll sensor)
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t bme680_manager_run();

    /**
     * @brief Get the next BSEC call timestamp in milliseconds
     * @return int64_t Timestamp in ms
     */
    int64_t bme680_manager_get_next_call_ms(void);

    /**
     * @brief Get the next BSEC call timestamp in nanoseconds
     * @return int64_t Timestamp in ns
     */
    int64_t bme680_manager_get_next_call_ns(void);

#ifdef __cplusplus
}
#endif

#endif // BME680_MANAGER_H
