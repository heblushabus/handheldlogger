#ifndef U8G2_MANAGER_H
#define U8G2_MANAGER_H

#include "driver/i2c_master.h"
#include "esp_err.h"
#include "u8g2.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Initialize the U8G2 display
     *
     * @return esp_err_t ESP_OK on success
     */
    esp_err_t u8g2_manager_init(void);

    /**
     * @brief Draw the main UI with voltage and sensor data
     *
     * @param voltage_mv Voltage in millivolts
     * @param temp Temperature in Celsius
     * @param humidity Humidity in %
     */
    void u8g2_manager_draw_ui(int voltage_mv, float temp, float humidity);

    /**
     * @brief Print status message to display
     * @param message String to display
     */
    void u8g2_manager_print_status(const char *message);

    /**
     * @brief Get the I2C bus handle
     *
     * @return i2c_master_bus_handle_t
     */
    i2c_master_bus_handle_t u8g2_manager_get_i2c_bus_handle(void);

#ifdef __cplusplus
}
#endif

#endif // U8G2_MANAGER_H
