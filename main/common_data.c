#include "common_data.h"
#include "esp_attr.h"
#include <stdbool.h>

RTC_NOINIT_ATTR latest_data_t latest_data;
RTC_NOINIT_ATTR rtc_bsec_state_container_t rtc_bsec_state;
RTC_FAST_ATTR buf_ulp_item buf_ulp[110];
buf_lp_item buf_lp[110];
buf_lp_item buf_lp10[110];
