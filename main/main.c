#include "bme680_manager.h"
#include "common_data.h"
#include "driver/gpio.h"
// #include "esp_attr.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h" // IWYU pragma: keep
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "u8g2_manager.h"
#include "vbat_driver.h"
#include <stdint.h>
#include <stdio.h>
#include <sys/time.h>

#define BME68X_USE_FPU
static const char *TAG = "main";

#include "esp_pm.h"
#include "esp_sleep.h"
#include "nvs_flash.h"
#include "wifi_time_manager.h"

RTC_FAST_ATTR int64_t next_call_ns;
RTC_FAST_ATTR int8_t loopcnt;
bool nvs_active = 0;

int64_t getCurNs() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return ((int64_t)tv.tv_sec * 1000000000LL + (int64_t)tv.tv_usec * 1000LL);
}

uint64_t sleepDurUs() { return (next_call_ns - getCurNs()) / 1000LL; }

void smartDelay(int64_t duration) {
  if (duration < 0) {
    ESP_LOGE(TAG, "sleep duration is negative");
    return;
  } else if (duration < 10000) { // 10ms
    esp_rom_delay_us(sleepDurUs());
    ESP_LOGI(TAG, "delay");
    return;
  } else if (duration < 10000000) { // 10s
    ESP_LOGI(TAG, "lightsleep %lld us, %d ms", duration, (duration / 1000));
    esp_sleep_enable_timer_wakeup(sleepDurUs());
    esp_light_sleep_start();
    return;
  }
  loopcnt++;
  bme680_manager_save_rtc_state();
  if (loopcnt >= 6) {
    bme680_manager_save_state();
    loopcnt = 0;
  }
  ESP_LOGI(TAG, "deepsleep: (%lld us)", duration);
  esp_sleep_enable_timer_wakeup(sleepDurUs());
  esp_deep_sleep_start();
}

void iterateBsec() {
  int64_t diff;
  latest_data.valid = false;
  latest_data.is_bsec = false;
  diff = getCurNs() - next_call_ns;

  if (diff < 0 && diff > -20000000LL) {
    esp_rom_delay_us((uint32_t)(-diff / 1000));
    ESP_LOGI(TAG, "littlefix");
  }
  if (diff > 2000000) { // greater than 2ms error
    ESP_LOGW(TAG, "timing error of %lld us late, normal with first measurement",
             diff / 1000);
  } else {
    ESP_LOGD(TAG, "diff:%lld us late", diff / 1000);
  }
  ESP_LOGI(TAG, "BSEC run calling");
  bme680_manager_run();
}

void app_main(void) {
  // Initialize Power Management (Auto Light Sleep)
  esp_pm_config_t pm_config = {
      .max_freq_mhz = 80, .min_freq_mhz = 40, .light_sleep_enable = false};
  ESP_ERROR_CHECK(esp_pm_configure(&pm_config));

  // Reduce WiFi driver logs
  esp_log_level_set("wifi", ESP_LOG_WARN);
  esp_log_level_set("wifi_init", ESP_LOG_WARN);
  esp_log_level_set("esp_netif_handlers", ESP_LOG_WARN);
  esp_log_level_set("phy_init", ESP_LOG_WARN);
  esp_log_level_set("pp", ESP_LOG_WARN);
  esp_log_level_set("net80211", ESP_LOG_WARN);
  ESP_LOGI(TAG, "Initializing Display...");

  gpio_reset_pin(15);
  gpio_set_direction(15, GPIO_MODE_OUTPUT);
  gpio_set_level(15, 0);

  // gpio_hold_en(15);
  ESP_LOGI(TAG, "cause: %d", esp_sleep_get_wakeup_cause());

  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER) {

    if (wifi_time_manager_init() != ESP_OK) {
      ESP_LOGE(TAG, "woke but no time - Aborting BSEC run");
      vTaskDelay(pdMS_TO_TICKS(5000));
      // Sleep for a short time to retry later
      esp_sleep_enable_timer_wakeup(10 * 1000000ULL);
      esp_deep_sleep_start();
      return;
    }

    if (i2c_init() != ESP_OK) {
      ESP_LOGE(TAG, "Failed to initialize I2C");
    }

    i2c_master_bus_handle_t bus_handle = u8g2_manager_get_i2c_bus_handle();

    if (bme680_manager_init(bus_handle) != ESP_OK) {
      ESP_LOGE(TAG, "Failed to initialize BME680");
    }
  b:
    iterateBsec();

    if (!latest_data.valid) {
      ESP_LOGW(TAG, "BSEC Data NOT Ready");
    }

    if (latest_data.is_bsec) {
      // display all sensors
      ESP_LOGI(TAG, "IAQ: %f", latest_data.iaq);
      ESP_LOGI(TAG, "IAQ Accuracy: %d", latest_data.iaq_accuracy);
      ESP_LOGI(TAG, "Temperature: %f", latest_data.temperature);
      ESP_LOGI(TAG, "Humidity: %f", latest_data.humidity);
      ESP_LOGI(TAG, "Pressure: %f", latest_data.pressure);
      ESP_LOGI(TAG, "Gas Resistance: %f", latest_data.gas_resistance);
      ESP_LOGI(TAG, "Stabilization Status: %d",
               latest_data.stabilization_status);
      ESP_LOGI(TAG, "Run-in Status: %d", latest_data.run_in_status);
    }
    // Calculate sleep time based on BSEC next call (nanoseconds)
    next_call_ns = bme680_manager_get_next_call_ns();
    smartDelay(sleepDurUs());
    goto b;
    ///////////////////////////////////////////////////////////////////////////////////////////
  } else if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT1) { // gpio

  } else if (esp_sleep_get_wakeup_cause() ==
             ESP_SLEEP_WAKEUP_UNDEFINED) { // reboot
  }

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

  // Initialize BME680
  u8g2_manager_print_status("Init Sensor...");
  i2c_master_bus_handle_t bus_handle = u8g2_manager_get_i2c_bus_handle();

  // Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_LOGE(TAG, "NVS FAIL!");
    while (1) {
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
  }
  ESP_ERROR_CHECK(ret);
  nvs_active = 1;

  if (bme680_manager_init(bus_handle) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize BME680");
  }

  // Read calibrated battery voltage into struct
  vbat_driver_read();
a:
  iterateBsec();

  if (latest_data.valid) {
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
  // Calculate sleep time based on BSEC next call (nanoseconds)
  next_call_ns = bme680_manager_get_next_call_ns();
  smartDelay(sleepDurUs());
  goto a;
}
