#ifndef AHT10_SENSOR_H
#define AHT10_SENSOR_H

#include <stdint.h>

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_err.h"

#include "global_fn/global_var.h"

#define AHT10_I2C_ADDRESS          0x38
#define AHT10_I2C_MASTER_FREQ_HZ   100000
#define AHT10_I2C_TIMEOUT_MS       1000

extern AHT10Sensor aht10Sensor;

esp_err_t aht10_init(i2c_port_t i2c_port, gpio_num_t sda_pin, gpio_num_t scl_pin);
esp_err_t aht10_read(float *temperature, float *humidity);
esp_err_t aht10_read_sensor(AHT10Sensor *sensor);

#endif // AHT10_SENSOR_H
