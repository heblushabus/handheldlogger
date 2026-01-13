#include "common_data.h"
#include "u8g2_manager.h" // Include header for display access

#include "esp_attr.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "nvs_flash.h"
// #include "protocol_examples_common.h" -- NOT AVAILABLE
#include <string.h>
#include <sys/time.h>
#include <time.h>
// Note: protocol_examples_common is usually for examples.
// We will implement simple wifi connect manually to avoid dependency issues if
// it's missing.

static const char *TAG = "WIFI_TIME_MGR";

#define WIFI_SSID "thePlekumat"
#define WIFI_PASS "170525ANee"
#define MAXIMUM_RETRY 5

static int s_retry_num = 0;
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data) {
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    u8g2_manager_print_status("Connecting WiFi...");
    esp_wifi_connect();
  } else if (event_base == WIFI_EVENT &&
             event_id == WIFI_EVENT_STA_DISCONNECTED) {
    if (s_retry_num < MAXIMUM_RETRY) {
      char buf[32];
      snprintf(buf, sizeof(buf), "Retrying WiFi (%d)", s_retry_num + 1);
      u8g2_manager_print_status(buf);
      esp_wifi_connect();
      s_retry_num++;
      ESP_LOGW(TAG, "retry to connect to the AP");
    } else {
      u8g2_manager_print_status("WiFi Failed!");
      xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
    }
    ESP_LOGW(TAG, "connect to the AP fail");
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
    s_retry_num = 0;
    u8g2_manager_print_status("WiFi Connected!");
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
  }
}

// ...

static void wifi_init_sta(void) {
  s_wifi_event_group = xEventGroupCreate();

  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  esp_event_handler_instance_t instance_any_id;
  esp_event_handler_instance_t instance_got_ip;
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));

  wifi_config_t wifi_config = {
      .sta =
          {
              .ssid = WIFI_SSID,
              .password = WIFI_PASS,
              .threshold.authmode = WIFI_AUTH_WPA2_PSK,
          },
  };
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());

  // Reduce TX power to avoid brownout
  ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(52)); // 52 * 0.25 = 13dBm

  ESP_LOGD(TAG, "wifi_init_sta finished.");

  EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                         WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                         pdFALSE, pdFALSE, portMAX_DELAY);

  if (bits & WIFI_CONNECTED_BIT) {
    ESP_LOGI(TAG, "connected to ap SSID:%s password:%s", WIFI_SSID, WIFI_PASS);
  } else if (bits & WIFI_FAIL_BIT) {
    ESP_LOGD(TAG, "Failed to connect to SSID:%s, password:%s", WIFI_SSID,
             WIFI_PASS);
  } else {
    ESP_LOGE(TAG, "UNEXPECTED EVENT");
  }
}

static void initialize_sntp(void) {
  ESP_LOGI(TAG, "Initializing SNTP");
  esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
  esp_sntp_setservername(0, "pool.ntp.org");
  esp_sntp_init();
}

static void obtain_time(void) {
  u8g2_manager_print_status("Syncing Time...");
  // Wait for time to be set
  time_t now = 0;
  struct tm timeinfo = {0};
  int retry = 0;
  const int retry_count = 10;
  while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET &&
         ++retry < retry_count) {
    ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry,
             retry_count);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
  }
  if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
    u8g2_manager_print_status("Time Synced!");
  } else {
    u8g2_manager_print_status("Time Sync Failed");
  }
  time(&now);
  localtime_r(&now, &timeinfo);
}

esp_err_t wifi_time_manager_init(void) {
  // Check if we already have valid time (RTC maintained)
  time_t now;
  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);

  // If year is > 2024, we assume time is valid and we can skip WiFi.
  // tm_year is years since 1900. 2025 - 1900 = 125.
  if (timeinfo.tm_year > (2024 - 1900)) {
    ESP_LOGI(TAG, "RTC time is valid (Year: %d). Skipping WiFi Sync.",
             timeinfo.tm_year + 1900);
    setenv("TZ", "TRT-3", 1);
    tzset();
    return ESP_OK;
  }

  ESP_LOGI(TAG, "RTC time invalid (Year: %d). Starting WiFi Sync...",
           timeinfo.tm_year + 1900);

  // Initialize NVS (Required for WiFi)
  if (!nvs_active) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_LOGE(TAG, "NVS ERROR in wifi manager");
      while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
      }
    }
    ESP_ERROR_CHECK(ret);
  }
  // Connect to WiFi
  wifi_init_sta();

  // Init SNTP
  initialize_sntp();

  // Wait a bit for sync
  obtain_time();

  // Set timezone
  setenv("TZ", "TRT-3", 1);
  tzset();

  // Kill all WiFi to save power
  ESP_LOGI(TAG, "Shutting down WiFi to save power...");
  esp_wifi_stop();
  esp_wifi_deinit();
  // esp_netif_deinit not trivial, but stopping wifi is key.

  return ESP_OK;
}
