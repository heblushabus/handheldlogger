# BSEC Integration Guide (BME680 / IAQ Edition)

## 1. Overview of BME Family Sensors

The BME sensor family has been designed to enable pressure, temperature, humidity, and gas measurements. The sensors can be operated in different modes specified in supplied header files. In general, a higher data rate corresponds to higher power consumption.

### Temperature Sensor

The integrated temperature sensor has been optimized for very low noise and high resolution. It is primarily used for estimating ambient temperature and for temperature compensation of the other sensors present.

### Pressure Sensor

The pressure sensor is an absolute barometric pressure sensor featuring exceptionally high accuracy and resolution at very low noise.

### Relative Humidity Sensor

The humidity sensor measures relative humidity from 0 to 100 percent across a temperature range from -40 to +85 degrees centigrade.

### Gas Sensor

The gas sensor can detect Volatile Organic Compounds (VOCs), Volatile Sulfur Compounds (VSCs), and other gases such as carbon monoxide and hydrogen in the part per billion (ppb) range.

---

## 2. The Environmental Fusion Library BSEC

### General Description

BSEC fusion library has been conceptualized to provide higher-level signal processing and fusion for the BME sensor. The library receives compensated sensor values from the sensor API. It processes the BME sensor signals (in combination with optional device sensors) to provide the requested virtual sensor outputs.

### BSEC Library Solutions: IAQ

The IAQ solution is a memory-efficient solution for use cases which require Low Power (LP) or Ultra Low Power (ULP) mode outputs.

### BSEC Configuration Settings

BSEC offers the flexibility to configure the solution based on customer-specific needs. The configuration can be loaded to BSEC via `bsec_set_configuration()`.

**Supported Power Modes:**

* **Ultra Low Power (ULP) mode:** Designed for battery-powered devices. Features an update rate of 300 seconds and an average current consumption of <0.1 mA.


* 
**Low Power (LP) mode:** Designed for interactive applications where the air quality is tracked at a higher update rate of 3 seconds with a current consumption of <1 mA.


* **Continuous (CONT) mode:** Provides an update rate of 1 Hz. This mode has an average current consumption of <12 mA.



**Background Calibration History:**

* 
**4days:** BSEC will consider the last 4 days of operation for the automatic background calibration.


* 
**28days:** BSEC will consider the last 28 days of operation for the automatic background calibration.



**Available BME680 Configurations:**
The following configuration sets are available for the IAQ Solution:

| Configuration | Sensor | Supply Voltage | Max Data Rate | Calibration Time |
| --- | --- | --- | --- | --- |
| `bme680_iaq_33v_3s_28d` | BME680 | 3.3 V | 3s (LP) | 28 days |
| `bme680_iaq_33v_3s_4d` | BME680 | 3.3 V | 3s (LP) | 4 days |
| `bme680_iaq_18v_3s_28d` | BME680 | 1.8 V | 3s (LP) | 28 days |
| `bme680_iaq_18v_3s_4d` | BME680 | 1.8 V | 3s (LP) | 4 days |
| `bme680_iaq_33v_300s_28d` | BME680 | 3.3 V | 300s (ULP) | 28 days |
| `bme680_iaq_33v_300s_4d` | BME680 | 3.3 V | 300s (ULP) | 4 days |
| `bme680_iaq_18v_300s_28d` | BME680 | 1.8 V | 300s (ULP) | 28 days |
| `bme680_iaq_18v_300s_4d` | BME680 | 1.8 V | 300s (ULP) | 4 days |
| 

 |  |  |  |  |

### Key Features

* Precise calculation of sensor heat compensated temperature outside the device.


* Precise calculation of sensor heat compensated relative humidity outside the device.


* Precise calculation of atmospheric pressure outside the device.


* Precise calculation of Index for Air Quality (IAQ) level outside the device.


* A power-on conditioning mechanism with LP and ULP modes to avoid prolonged Run-in time.



### Supported Virtual Sensor Output Signals

The sample rate of ULP, LP, and CONT modes are 1/300Hz, 1/3Hz, and 1Hz respectively.

| Signal Name | Unit | Accuracy Status | Supported Modes |
| --- | --- | --- | --- |
| `BSEC_OUTPUT_RAW_PRESSURE` | Pa | no | ULP, LP, CONT |
| `BSEC_OUTPUT_RAW_TEMPERATURE` | deg C | no | ULP, LP, CONT |
| `BSEC_OUTPUT_RAW_HUMIDITY` | % | no | ULP, LP, CONT |
| `BSEC_OUTPUT_RAW_GAS` | Ohm | no | ULP, LP, CONT |
| `BSEC_OUTPUT_IAQ` | 0-500 | yes | ULP, LP, CONT |
| `BSEC_OUTPUT_STATIC_IAQ` | 0-500 | yes | ULP, LP, CONT |
| `BSEC_OUTPUT_CO2_EQUIVALENT` | ppm | yes | ULP, LP, CONT |
| `BSEC_OUTPUT_COMPENSATED_GAS` | Ohm | no | ULP, LP, CONT |
| `BSEC_OUTPUT_BREATH_VOC_EQUIVALENT` | ppm | yes | ULP, LP, CONT |
| `BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE` | deg C | no | ULP, LP, CONT |
| `BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY` | % | no | ULP, LP, CONT |
| `BSEC_OUTPUT_STABILIZATION_STATUS` | Bool | no | ULP, LP, CONT |
| `BSEC_OUTPUT_RUN_IN_STATUS` | Bool | no | ULP, LP, CONT |
| `BSEC_OUTPUT_GAS_PERCENTAGE` | % | yes | ULP, LP, CONT |
| 

 |  |  |  |

**Note on Accuracy:** The IAQ accuracy indicator will notify the user when they should initiate a calibration process. Calibration is performed in the background if the sensor is exposed to clean or polluted air for approximately 30 minutes each.

---

## 3. Integration Guidelines

### Hardware & Software Requirements

BSEC is designed to be used exclusively together with sensors of the BMExxx family. To ensure consistent performance, the sensors shall be configured by BSEC itself by the use of the `bsec_sensor_control()` interface.

The software framework must provide the sample rates requested by the user to BSEC via `bsec_update_subscription()`. The framework must then use `bsec_sensor_control()` periodically to configure the sensor. After every call, the next call to `bsec_sensor_control()` should be scheduled as specified in the returned sensor settings structure.

### Integration Steps

1. 
**Initialization of Sensor:** `bme68x_init()` (Refer to sensor API).


2. 
**Initialization of BSEC:** `bsec_init()`.


3. 
**Update Configuration (Optional):** `bsec_set_configuration()`.


4. 
**Restore State (Optional):** `bsec_set_state()`.


5. 
**Subscribe Outputs:** `bsec_update_subscription()`.


6. **Loop:**
* Call `bsec_sensor_control()` to get sensor settings.


* Configure sensor and trigger measurement.
* Read sensor data.
* Call `bsec_do_steps()` to process data.




7. 
**Save State (On Power Cycle):** `bsec_get_state()`.



---

## 4. FAQ & Troubleshooting

### IAQ output does not change or accuracy remains 0

**Possible reason:** Timing of gas measurements is not within 6.25% of the target values. For example, in Low Power (LP) mode the intended sample period is 3s. The difference between two consecutive measurements must not exceed 106.25% of 3s (3.1875s).
**Correction:** Ensure accurate timestamps with nanosecond resolution and strictly follow the timing returned by `bsec_sensor_control()` in the `next_call` field.

### Error Codes

* 
`BSEC_E_DOSTEPS_VALUELIMITS`: Input sensor signal is out of valid range (e.g., Gas resistor 170 to 103000000 Ohm).


* 
`BSEC_E_SU_WRONGDATARATE`: Requested sample rate in `bsec_update_subscription` does not match defined rates (e.g., must use `BSEC_SAMPLE_RATE_LP` or `BSEC_SAMPLE_RATE_ULP`).


* 
`BSEC_W_SC_CALL_TIMING_VIOLATION`: The timestamp at which `bsec_sensor_control` is called differs from the target timestamp by more than 6.25%.



---

## 5. Relevant Enumerations and Structures

### bsec_virtual_sensor_t (Outputs)

* 
`BSEC_OUTPUT_IAQ`: Indoor-air-quality estimate [0-500].


* 
`BSEC_OUTPUT_STATIC_IAQ`: Unscaled indoor-air-quality estimate.


* 
`BSEC_OUTPUT_CO2_EQUIVALENT`: CO2 equivalent estimate [ppm].


* 
`BSEC_OUTPUT_BREATH_VOC_EQUIVALENT`: Breath VOC concentration estimate [ppm].


* 
`BSEC_OUTPUT_RAW_TEMPERATURE`: Temperature directly measured by sensor.


* 
`BSEC_OUTPUT_RAW_PRESSURE`: Pressure directly measured by sensor.


* 
`BSEC_OUTPUT_RAW_HUMIDITY`: Relative humidity directly measured by sensor.


* 
`BSEC_OUTPUT_RAW_GAS`: Gas resistance measured directly in Ohm.


* 
`BSEC_OUTPUT_STABILIZATION_STATUS`: Indicates if stabilization is finished (1) or ongoing (0).


* 
`BSEC_OUTPUT_RUN_IN_STATUS`: Tracks power-on stabilization.


* 
`BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE`: Temperature compensated for sensor/device heating.


* 
`BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY`: Humidity compensated for sensor/device heating.


* 
`BSEC_OUTPUT_COMPENSATED_GAS`: Gas resistance compensated for temperature/humidity influences.


* 
`BSEC_OUTPUT_GAS_PERCENTAGE`: Percentage of min/max filtered gas value.



### bsec_bme_settings_t

Structure returned by `bsec_sensor_control()` to configure the sensor.

* 
`next_call`: Time stamp of the next call of the sensor_control.


* 
`process_data`: Bit field describing which data is to be passed to `bsec_do_steps()`.


* 
`heater_temperature`: Heater temperature [degrees Celsius].


* 
`heater_duration`: Heater duration [ms].


* 
`run_gas`: Enable gas measurements [0/1].


* 
`pressure_oversampling`: Pressure oversampling settings [0-5].


* 
`temperature_oversampling`: Temperature oversampling settings [0-5].


* 
`humidity_oversampling`: Humidity oversampling settings [0-5].


* 
`trigger_measurement`: Trigger a forced measurement with these settings now [0/1].


* 
`op_mode`: Sensor operation mode [0/1].



### bsec_output_t

* 
`time_stamp`: Time stamp in nanosecond resolution.


* 
`signal`: Signal sample value.


* 
`sensor_id`: Identifier of virtual sensor.


* 
`accuracy`: Accuracy status 0-3.



**IAQ Accuracy Description:**

* 
**0:** Stabilization/run-in ongoing.


* **1:** Low accuracy. Expose sensor to good air (outdoor) and bad air (box with exhaled breath) for auto-trimming.


* 
**2:** Medium accuracy: auto-trimming ongoing.


* 
**3:** High accuracy.