#include "bme680_manager.h"
#include "common_data.c"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h" // IWYU pragma: keep
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "u8g2_manager.h"
#include "vbat_driver.h"
#include <stdint.h>
#include <stdio.h>
#include <sys/time.h>
// #include <time.h>

#define BME68X_USE_FPU
static const char *TAG = "main";

#include "esp_pm.h"
#include "esp_sleep.h"
#include "nvs_flash.h"
#include "wifi_time_manager.h"

int64_t getCurNs() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return ((int64_t)tv.tv_sec * 1000000000LL + (int64_t)tv.tv_usec * 1000LL);
}

void app_main(void) {
  // Initialize Power Management (Auto Light Sleep)
  esp_pm_config_t pm_config = {.max_freq_mhz = 80,
                               .min_freq_mhz = 40,
#if CONFIG_FREERTOS_USE_TICKLESS_IDLE
                               .light_sleep_enable = false
#endif
  };
  ESP_ERROR_CHECK(esp_pm_configure(&pm_config));
  ESP_LOGI(TAG, "Initializing Display...");

  gpio_reset_pin(15);
  gpio_set_direction(15, GPIO_MODE_OUTPUT);
  gpio_set_level(15, 0);

  // Keep GPIO 18 high during light sleep
  gpio_hold_en(15);

  // Initialize the Display manager (also initializes I2C)
  if (u8g2_manager_init() != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize display");
  }

  // Initialize WiFi & SNTP
  u8g2_manager_print_status("Checking Time...");
  if (wifi_time_manager_init() != ESP_OK) {
    ESP_LOGE(TAG, "Time Sync Failed - Aborting BSEC run");
    u8g2_manager_print_status("Time Sync Fail!");
    vTaskDelay(pdMS_TO_TICKS(5000));
    // Sleep for a short time to retry later
    esp_sleep_enable_timer_wakeup(10 * 1000000ULL);
    esp_deep_sleep_start();
    return;
  }

  // Initialize the ADC driver
  ESP_ERROR_CHECK(vbat_driver_init());

  // ... rest of the code ... (keeping user's init logic)

  // Initialize BME680
  u8g2_manager_print_status("Init Sensor...");
  i2c_master_bus_handle_t bus_handle = u8g2_manager_get_i2c_bus_handle();

  // Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    u8g2_manager_print_status("NVS FAIL!");
    while (1) {
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
  }
  ESP_ERROR_CHECK(ret);

  if (bme680_manager_init(bus_handle) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize BME680");
  }

  // Read calibrated battery voltage into struct
  vbat_driver_read();

  latest_data.valid = false;
  latest_data.is_bsec = false;

a:
  uint64_t diff = getCurNs() - bme680_manager_get_next_call_ns();

  if (diff > 2000000000LL)
    ESP_LOGW(TAG, "timing error, normal with first measurement");
  else
    ESP_LOGI(TAG, "diff:%lld", diff);
  bme680_manager_run();

  if (latest_data.valid) {
    ESP_LOGI(TAG, "BSEC Data Acquired");
    // Pass unified data to display
    u8g2_manager_draw_ui(latest_data.battery_voltage_mv,
                         latest_data.temperature, latest_data.humidity,
                         latest_data.iaq, latest_data.iaq_accuracy);
  } else {
    ESP_LOGW(TAG, "BSEC Data NOT Ready");
    // Only display battery if sensor fails
    char bat_str[16];
    snprintf(bat_str, sizeof(bat_str), "Bat: %dmV",
             latest_data.battery_voltage_mv);
    u8g2_manager_print_status(bat_str);
  }

  if (latest_data.is_bsec) {
    // display all sensors
    ESP_LOGI(TAG, "IAQ: %f", latest_data.iaq);
    ESP_LOGI(TAG, "IAQ Accuracy: %d", latest_data.iaq_accuracy);
    ESP_LOGI(TAG, "Temperature: %f", latest_data.temperature);
    ESP_LOGI(TAG, "Humidity: %f", latest_data.humidity);
    ESP_LOGI(TAG, "Pressure: %f", latest_data.pressure);
    ESP_LOGI(TAG, "Gas Resistance: %f", latest_data.gas_resistance);
    ESP_LOGI(TAG, "Stabilization Status: %d", latest_data.stabilization_status);
    ESP_LOGI(TAG, "Run-in Status: %d", latest_data.run_in_status);
  }
  //  u8g2_manager_print_status("deepsleep...");
  //  vTaskDelay(pdMS_TO_TICKS(500)); // Short delay to show message

  // Calculate sleep time based on BSEC next call (nanoseconds)
  int64_t next_call_ns = bme680_manager_get_next_call_ns();

  int64_t now_ns = getCurNs();

  // Calculate remaining time in microseconds for esp_sleep
  int64_t sleep_duration_us = (next_call_ns - now_ns) / 1000LL;

  // Safety clamp - if calculated time is invalid or too short, force safe
  // 10sec
  if (sleep_duration_us < 10000000) {
    ESP_LOGI(TAG,
             "Calculated sleep duration too short (%lld us), lightsleep %ds",
             sleep_duration_us, (sleep_duration_us / 1000) + 20);
    vTaskDelay(pdMS_TO_TICKS((sleep_duration_us / 1000) + 33));
    goto a;
  }

  // Save BSEC state before sleeping
  bme680_manager_save_state();

  ESP_LOGI(TAG, "Enabling Timer Wakeup (%lld us)", sleep_duration_us);
  esp_sleep_enable_timer_wakeup(sleep_duration_us);

  esp_deep_sleep_start();
}
