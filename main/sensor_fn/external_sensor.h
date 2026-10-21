#ifndef EXTERNAL_SENSOR_H
#define EXTERNAL_SENSOR_H

#include <stdbool.h>
#include <stdint.h>
#include "driver/adc.h"

// Define sensor types enum so string_to_sensor_type and sensor_type_to_string know what it is
typedef enum {
    SENSOR_NONE = 0,
    SENSOR_TEMP,
    SENSOR_HUMIDITY,
    SENSOR_MOISTURE,
    SENSOR_PH
} sensor_type_t;

typedef struct {
    char sensor_id[5];
    
    const uint8_t pin_A0;
    adc1_channel_t channel;
    
    const uint8_t pin_D0;
} ExternalSensor;

typedef struct {
    ExternalSensor externalSensor[6];
} ExternalSensorlist;

void external_sensors_init(ExternalSensorlist *sensorList);
void external_sesnor_read(int sensor_index, ExternalSensor *hw);
const char *sensor_type_to_string(sensor_type_t type);
sensor_type_t string_to_sensor_type(const char *type);

#endif // EXTERNAL_SENSOR_H