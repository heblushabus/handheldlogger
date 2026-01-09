#include "vbat_driver.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "common_data.h"
static const char *TAG = "adc_driver";

// Single instance handles managed by the driver
static adc_oneshot_unit_handle_t adc1_handle = NULL;
static adc_cali_handle_t adc1_cali_handle = NULL;
static bool is_calibrated = false;

// Fixed configuration
#define VBAT_ADC_UNIT ADC_UNIT_1
#define VBAT_ADC_CHANNEL ADC_CHANNEL_1 // GPIO 1 on ESP32-C6
#define VBAT_ADC_ATTEN ADC_ATTEN_DB_12

static bool adc_calibration_init(adc_unit_t unit, adc_channel_t channel,
                                 adc_atten_t atten,
                                 adc_cali_handle_t *out_handle);

esp_err_t vbat_driver_init(void)
{
  if (adc1_handle != NULL)
  {
    ESP_LOGW(TAG, "ADC driver already initialized");
    return ESP_OK;
  }

  //-------------ADC1 Init---------------//
  adc_oneshot_unit_init_cfg_t init_config1 = {
      .unit_id = VBAT_ADC_UNIT,
      .ulp_mode = ADC_ULP_MODE_DISABLE,
  };
  ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

  //-------------ADC1 Config---------------//
  adc_oneshot_chan_cfg_t config = {
      .atten = VBAT_ADC_ATTEN,
      .bitwidth = ADC_BITWIDTH_DEFAULT,
  };
  ESP_ERROR_CHECK(
      adc_oneshot_config_channel(adc1_handle, VBAT_ADC_CHANNEL, &config));

  //-------------ADC1 Calibration Init---------------//
  is_calibrated = adc_calibration_init(VBAT_ADC_UNIT, VBAT_ADC_CHANNEL,
                                       VBAT_ADC_ATTEN, &adc1_cali_handle);

  return ESP_OK;
}

esp_err_t vbat_driver_read_raw(int *raw_out)
{
  if (adc1_handle == NULL)
  {
    return ESP_ERR_INVALID_STATE;
  }
  return adc_oneshot_read(adc1_handle, VBAT_ADC_CHANNEL, raw_out);
}

esp_err_t vbat_driver_read_voltage(int *voltage_mv)
{
  int raw;
  esp_err_t ret = vbat_driver_read_raw(&raw);
  if (ret != ESP_OK)
    return ret;

  if (is_calibrated)
  {
    return adc_cali_raw_to_voltage(adc1_cali_handle, raw, voltage_mv);
  }
  else
  {
    // Fallback or handle uncalibrated state
    // On ESP32-C6, 12dB attenuation covers up to ~3.3V
    *voltage_mv = (raw * 3300) / 4095;
    return ESP_OK;
  }
}

esp_err_t vbat_driver_read()
{
  int voltage_mv = 0;
  esp_err_t ret = vbat_driver_read_voltage(&voltage_mv);
  if (ret == ESP_OK)
  {
    latest_data.battery_voltage_mv = voltage_mv * 2; // Apply voltage divider scaling
  }
  return ret;
}

static bool adc_calibration_init(adc_unit_t unit, adc_channel_t channel,
                                 adc_atten_t atten,
                                 adc_cali_handle_t *out_handle)
{
  adc_cali_handle_t handle = NULL;
  esp_err_t ret = ESP_FAIL;
  bool calibrated = false;

  if (!calibrated)
  {
    ESP_LOGI(TAG, "Calibration scheme: Curve Fitting");
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = unit,
        .chan = channel,
        .atten = atten,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
    if (ret == ESP_OK)
    {
      calibrated = true;
    }
  }

  *out_handle = handle;
  if (ret == ESP_OK)
  {
    ESP_LOGI(TAG, "Calibration Success");
  }
  else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated)
  {
    ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
  }
  else
  {
    ESP_LOGE(TAG, "Invalid arg or no memory");
  }

  return calibrated;
}
