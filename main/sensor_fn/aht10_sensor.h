#ifndef AHT10_SENSOR_H
#define AHT10_SENSOR_H


#include "driver/gpio.h"
#include "driver/i2c.h"

typedef struct {
    const int I2C_port;
    const uint8_t SDA_pin;
    const uint8_t SCL_pin;

    const uint8_t I2C_add;

    const int Interval;
} AHT10;


esp_err_t aht10_sensor_init(AHT10 *aht10);
esp_err_t aht10_read(float *temperature, float *humidity);
esp_err_t aht10_read_sensor(AHT10Sensor *sensor);


#endif // AHT10_SENSOR_H
