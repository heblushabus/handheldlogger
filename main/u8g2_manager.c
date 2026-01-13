#include "u8g2_manager.h"
// #include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h" // IWYU pragma: keep
#include "freertos/task.h"
// #include "sdkconfig.h"
#include "u8g2.h"
#include <sys/time.h>
#include <time.h>

static const char *TAG = "U8G2_MGR";

/* I2C Configuration defaults if not in menuconfig */
#define I2C_MASTER_SDA_IO 20
#define I2C_MASTER_SCL_IO 19
#define I2C_FREQ_HZ 400000
#define I2C_DISPLAY_ADDRESS 0x3C
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_TIMEOUT_MS 1000

static i2c_master_bus_handle_t i2c_bus_handle = NULL;
static i2c_master_dev_handle_t display_dev_handle = NULL;
static u8g2_t u8g2;

/**
 * @brief U8X8 I2C communication callback
 */
static uint8_t u8x8_byte_i2c_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int,
                                void *arg_ptr) {
  static uint8_t buffer[132];
  static uint8_t buf_idx;

  switch (msg) {
  case U8X8_MSG_BYTE_INIT: {
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = I2C_DISPLAY_ADDRESS,
        .scl_speed_hz = I2C_FREQ_HZ,
    };
    esp_err_t ret = i2c_master_bus_add_device(i2c_bus_handle, &dev_config,
                                              &display_dev_handle);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "I2C master driver initialized failed");
      return 0;
    }
    break;
  }
  case U8X8_MSG_BYTE_START_TRANSFER:
    buf_idx = 0;
    break;
  case U8X8_MSG_BYTE_SET_DC:
    break;
  case U8X8_MSG_BYTE_SEND:
    for (size_t i = 0; i < arg_int; ++i) {
      buffer[buf_idx++] = *((uint8_t *)arg_ptr + i);
    }
    break;
  case U8X8_MSG_BYTE_END_TRANSFER:
    if (buf_idx > 0 && display_dev_handle != NULL) {
      esp_err_t ret = i2c_master_transmit(display_dev_handle, buffer, buf_idx,
                                          I2C_TIMEOUT_MS);
      if (ret != ESP_OK)
        return 0;
    }
    break;
  default:
    return 0;
  }
  return 1;
}

/**
 * @brief U8X8 GPIO control and delay callback
 */
static uint8_t u8x8_gpio_delay_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int,
                                  void *arg_ptr) {
  switch (msg) {
  case U8X8_MSG_GPIO_AND_DELAY_INIT:
    break;
  case U8X8_MSG_DELAY_MILLI:
    vTaskDelay(pdMS_TO_TICKS(arg_int));
    break;
  case U8X8_MSG_DELAY_10MICRO:
    esp_rom_delay_us(arg_int * 10);
    break;
  case U8X8_MSG_DELAY_100NANO:
    __asm__ __volatile__("nop");
    break;
  case U8X8_MSG_DELAY_I2C:
    esp_rom_delay_us(5 / arg_int);
    break;
  default:
    return 0;
  }
  return 1;
}

esp_err_t i2c_init(void) {
  i2c_master_bus_config_t bus_config = {
      .i2c_port = I2C_MASTER_NUM,
      .sda_io_num = I2C_MASTER_SDA_IO,
      .scl_io_num = I2C_MASTER_SCL_IO,
      .clk_source = I2C_CLK_SRC_DEFAULT,
      .glitch_ignore_cnt = 7,
      .flags.enable_internal_pullup = true,
  };
  ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_config, &i2c_bus_handle), TAG,
                      "Failed to create I2C bus");

  return ESP_OK;
}

esp_err_t u8g2_manager_init(void) {
  i2c_init();

  u8g2_Setup_ssd1306_i2c_128x64_noname_f(&u8g2, U8G2_R0, u8x8_byte_i2c_cb,
                                         u8x8_gpio_delay_cb);

  u8g2_InitDisplay(&u8g2);
  u8g2_SetPowerSave(&u8g2, 0);
  u8g2_ClearBuffer(&u8g2);
  u8g2_SendBuffer(&u8g2);

  ESP_LOGI(TAG, "U8G2 initialized successfully");
  return ESP_OK;
}

void u8g2_manager_draw_ui(int voltage_mv, float temp, float humidity, float iaq,
                          int iaq_accuracy) {
  u8g2_ClearBuffer(&u8g2);
  char buf[32];

  u8g2_SetFont(&u8g2, u8g2_font_luRS08_tr);

  // Draw Voltage
  snprintf(buf, sizeof(buf), "Bat: %.2fV", voltage_mv / 1000.0);
  u8g2_DrawStr(&u8g2, 0, 8, buf);

  // Draw Temperature
  snprintf(buf, sizeof(buf), "Tmp: %.1f C", temp);
  u8g2_DrawStr(&u8g2, 0, 16, buf);

  // Draw Humidity
  snprintf(buf, sizeof(buf), "Hum: %.1f %%", humidity);
  u8g2_DrawStr(&u8g2, 0, 24, buf);

  // Draw Time
  time_t now;
  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);
  strftime(buf, sizeof(buf), "%H:%M:%S", &timeinfo);
  u8g2_DrawStr(&u8g2, 0, 32, buf);

  // Draw IAQ
  snprintf(buf, sizeof(buf), "IAQ: %.1f (%d)", iaq, iaq_accuracy);
  u8g2_DrawStr(&u8g2, 0, 48, buf);

  u8g2_SendBuffer(&u8g2);
}

void u8g2_manager_print_status(const char *message) {
  u8g2_ClearBuffer(&u8g2);
  u8g2_SetFont(&u8g2, u8g2_font_luRS08_tr);
  u8g2_DrawStr(&u8g2, 0, 30, message);
  u8g2_SendBuffer(&u8g2);
}

i2c_master_bus_handle_t u8g2_manager_get_i2c_bus_handle(void) {
  return i2c_bus_handle;
}
