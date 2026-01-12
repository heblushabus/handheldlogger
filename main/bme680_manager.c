#include "bme680_manager.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h" // IWYU pragma: keep
#include "freertos/task.h"
// #include "nvs_flash.h"
#include "bsec_config.h"
#include "common_data.h"
#include "nvs.h"
#include <string.h>
#include <sys/time.h>
#include <time.h>

#define BSEC_STATE_SAVE_INTERVAL 1000 * 60 * 1 // Save every 1 minute for test

static const char *TAG = "BME680_MGR";
static bsec2_t bsec;
static bsec_outputs_t latest_outputs;
static bool new_outputs_available = false;

static void bsec_callback(const bme68x_data_t data,
                          const bsec_outputs_t outputs, bsec2_t bsec2) {
  if (outputs.n_outputs > 0) {
    latest_outputs = outputs;
    if (bme680_manager_read() == ESP_OK) {
      ESP_LOGI(TAG, "BSEC Data Acquired");
    } else
      ESP_LOGW(TAG, "BSEC read error");
  } else {
    // If BSEC has no new outputs (e.g. during stabilization), use raw data
    // directly Assuming BME68X_USE_FPU is NOT defined globally, these are
    // scaled integers
    latest_data.temperature = data.temperature;
    latest_data.humidity = data.humidity;
    latest_data.pressure = data.pressure; // Already converted to hPa in bsec2.c
    latest_data.gas_resistance = data.gas_resistance;
    latest_data.iaq = 0; // Not available
    latest_data.iaq_accuracy = 0;
    latest_data.valid = true;
    latest_data.is_bsec = false;

    ESP_LOGI(TAG, "Using raw data from callback (Temp: %.2f, Hum: %.2f)",
             latest_data.temperature, latest_data.humidity);
  }
}

esp_err_t bme680_manager_init(i2c_master_bus_handle_t bus_handle) {
  if (bus_handle == NULL) {
    ESP_LOGE(TAG, "Invalid I2C bus handle");
    return ESP_ERR_INVALID_ARG;
  }

  ESP_LOGI(TAG, "Initializing BSEC2...");

  // Initialize the library with the I2C bus handle
  if (!bsec2_init(&bsec, (void *)bus_handle, BME68X_I2C_INTF)) {
    ESP_LOGE(TAG, "BSEC2 initialization failed");
    return ESP_FAIL;
  }

  bme68x_set_config(&bsec);

  bsec2_attach_callback(&bsec, bsec_callback);

  bme68x_load_state(&bsec);

  bsec_sensor_t sensor_list[] = {
      BSEC_OUTPUT_IAQ,
      BSEC_OUTPUT_RAW_TEMPERATURE,
      BSEC_OUTPUT_RAW_PRESSURE,
      BSEC_OUTPUT_RAW_HUMIDITY,
      BSEC_OUTPUT_RAW_GAS,
      BSEC_OUTPUT_STABILIZATION_STATUS,
      BSEC_OUTPUT_RUN_IN_STATUS,
      BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
      BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
  };

  if (!bsec2_update_subscription(&bsec, sensor_list,
                                 sizeof(sensor_list) / sizeof(sensor_list[0]),
                                 BSEC_SAMPLE_RATE_ULP)) {
    ESP_LOGE(TAG, "BSEC2 subscription failed. Status: %d", bsec.status);
    return ESP_FAIL;
  }

  ESP_LOGI(TAG, "BSEC2 initialized successfully");

  return ESP_OK;
}

esp_err_t bme680_manager_run() {
  if (!bsec2_run(&bsec)) {
    ESP_LOGW(TAG, "BSEC2 run failed");
    ESP_LOGW(TAG, "BSEC2 run error: %d", bsec.status);
    return ESP_FAIL;
  }
  if (bsec.status != BSEC_OK) {
    ESP_LOGW(TAG, "BSEC2 run status: %d", bsec.status);
    // return ESP_FAIL;
  }
  return ESP_OK;
}

esp_err_t bme680_manager_read() {
  // Populate BSEC fields from latest_outputs (don't memset - preserves battery
  // voltage)
  latest_data.valid = true;
  latest_data.is_bsec = true;

  for (int i = 0; i < latest_outputs.n_outputs; i++) {
    const bsec_data_t *output = &latest_outputs.output[i];
    switch (output->sensor_id) {
    case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE:
    case BSEC_OUTPUT_RAW_TEMPERATURE: // Fallback if heat compensated not
                                      // available
      // Prefer heat compensated if we subscribed to it
      if (output->sensor_id ==
              BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE ||
          latest_data.temperature == 0)
        latest_data.temperature = output->signal;
      break;
    case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY:
    case BSEC_OUTPUT_RAW_HUMIDITY:
      if (output->sensor_id == BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY ||
          latest_data.humidity == 0)
        latest_data.humidity = output->signal;
      break;
    case BSEC_OUTPUT_RAW_PRESSURE:
      latest_data.pressure = output->signal;
      break;
    case BSEC_OUTPUT_IAQ:
      latest_data.iaq = output->signal;
      latest_data.iaq_accuracy = output->accuracy;
      break;
    case BSEC_OUTPUT_RAW_GAS:
      latest_data.gas_resistance = output->signal;
      break;
    case BSEC_OUTPUT_STABILIZATION_STATUS:
      latest_data.stabilization_status = (int)output->signal;
      break;
    case BSEC_OUTPUT_RUN_IN_STATUS:
      latest_data.run_in_status = (int)output->signal;
      break;
    default:
      break;
    }
    // logging
  }

  new_outputs_available = false;
  return ESP_OK;
}

esp_err_t bme68x_set_config(bsec2_t *bsec) {
  if (!bsec2_set_config(bsec, bsec_config_iaq)) {
    ESP_LOGE(TAG, "Failed to set BSEC configuration");
    return ESP_FAIL;
  } else {
    ESP_LOGI(TAG, "BSEC configuration applied");
    return ESP_OK;
  }
}

// Helper to check time validity
static bool is_time_synced(void) {
  time_t now;
  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);
  return (timeinfo.tm_year > (2024 - 1900));
}

void bme68x_load_state(bsec2_t *bsec) {
  if (!is_time_synced()) {
    ESP_LOGW(TAG, "Time not synced (Year <= 2024). Skipping BSEC state load.");
    return;
  }

  nvs_handle_t my_handle;
  esp_err_t err;

  // Open
  err = nvs_open("bsec_storage", NVS_READWRITE, &my_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(err));
    return;
  }

  // Read blob
  uint8_t bsec_state[BSEC_MAX_STATE_BLOB_SIZE];
  size_t required_size = sizeof(bsec_state);

  err = nvs_get_blob(my_handle, "bsec_state", bsec_state, &required_size);
  if (err == ESP_OK && required_size > 0) {
    if (!bsec2_set_state(bsec, bsec_state)) {
      ESP_LOGE(TAG, "Failed to set BSEC state");
    } else {
      ESP_LOGI(TAG, "BSEC state loaded from NVS (Size: %d)", required_size);
    }
  } else {
    ESP_LOGW(TAG, "NVS state not found or invalid size (Error: %s, Size: %d)",
             esp_err_to_name(err), required_size);
  }

  nvs_close(my_handle);
}

void bme68x_save_state(bsec2_t *bsec) {
  if (!is_time_synced()) {
    ESP_LOGW(TAG, "Time not synced. Skipping BSEC state save.");
    return;
  }

  nvs_handle_t my_handle;
  esp_err_t err;

  uint8_t bsec_state[BSEC_MAX_STATE_BLOB_SIZE];
  if (!bsec2_get_state(bsec, bsec_state)) {
    ESP_LOGE(TAG, "Failed to get BSEC state");
    return;
  }

  // Open
  err = nvs_open("bsec_storage", NVS_READWRITE, &my_handle);
  if (err != ESP_OK)
    return;

  err = nvs_set_blob(my_handle, "bsec_state", bsec_state,
                     BSEC_MAX_STATE_BLOB_SIZE);
  if (err == ESP_OK) {
    err = nvs_commit(my_handle);
    if (err == ESP_OK) {
      ESP_LOGI(TAG, "BSEC state saved to NVS");
    }
  }
  nvs_close(my_handle);
}

void bme680_manager_save_state(void) { bme68x_save_state(&bsec); }

int64_t bme680_manager_get_next_call_ms(void) {
  return bsec.bme_conf.next_call / 1000000;
}

int64_t bme680_manager_get_next_call_ns(void) {
  return bsec.bme_conf.next_call;
}
