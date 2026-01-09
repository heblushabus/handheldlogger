/*
 * common_data.h
 *
 *  Created on: Jan 8, 2026
 *      Author: USER
 */

#ifndef COMMON_DATA_H_
#define COMMON_DATA_H_


#include <stdbool.h>
// Unified structure to hold latest sensor data
typedef struct
{
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
    bool valid;             // Flag to indicate if data is populated
} latest_data_t;

// Declare global instance if needed, or just the type
// For this request, we'll define the type here and usage in managers.
extern latest_data_t latest_data;

#endif /* COMMON_DATA_H_ */
