/*
 * common_data.h
 *
 *  Created on: Jan 8, 2026
 *      Author: USER
 */

#ifndef COMMON_DATA_H_
#define COMMON_DATA_H_

#include "esp_attr.h"
#include <stdbool.h>
#include <stdint.h>
// Unified structure to hold latest sensor data
typedef struct {
  float temperature;
  float humidity;
  float pressure;
  float iaq;
  int iaq_accuracy;
  float gas_resistance;
  int stabilization_status;
  int run_in_status;
  int battery_voltage_mv; // Battery voltage in millivolts
  bool is_bsec;
  bool valid; // Flag to indicate if data is populated
} latest_data_t;

// Declare global instance if needed, or just the type
// For this request, we'll define the type here and usage in managers.
RTC_NOINIT_ATTR extern latest_data_t latest_data;

// Magic number to check if RTC memory contains valid BSEC state
#define RTC_BSEC_MAGIC 0x42534543 // "BSEC" in hex
#define BSEC_STATE_LEN 238

typedef struct {
  uint32_t magic;                // 4 bytes magic number
  uint8_t state[BSEC_STATE_LEN]; // BSEC state blob
} rtc_bsec_state_container_t;

RTC_NOINIT_ATTR extern rtc_bsec_state_container_t rtc_bsec_state;

extern bool nvs_active;

typedef struct __attribute__((packed)) {
  uint32_t timestamp : 32; // unix time in secs

  uint32_t pressure : 17;
  uint32_t temp : 13;    // mem:0-8191 real: -20,95-60,95
  uint32_t accuracy : 2; // 1 byte: 0-3

  uint32_t humidity : 10; // mem:0-1024 real: 0,0-102,4
  uint32_t iaq : 9;       // real:0-512
  uint32_t co2 : 13;      // real: 0-8192

  uint32_t battery : 9; // mem:0-512 real 0,00-5,12
  uint32_t _pad1 : 23;
} buf_ulp_item;

RTC_FAST_ATTR extern buf_ulp_item buf_ulp[110];

typedef struct __attribute__((packed)) {
  uint32_t pressure : 17;
  uint32_t temp : 13;    // mem:0-8191 real: -20,95-60,95
  uint32_t accuracy : 2; // 1 byte: 0-3

  uint32_t humidity : 10; // mem:0-1024 real: 0,0-102,4
  uint32_t iaq : 9;       // real:0-512
  uint32_t co2 : 13;      // real: 0-8192

  uint32_t battery : 9; // mem:0-512 real 0,00-5,12
  uint32_t _pad1 : 23;
} buf_lp_item;

extern buf_lp_item buf_lp[110];
extern buf_lp_item buf_lp10[110];

#endif /* COMMON_DATA_H_ */
