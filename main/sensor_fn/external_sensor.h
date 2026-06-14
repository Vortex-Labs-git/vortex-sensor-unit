#ifndef EXTERNAL_SENSOR_H
#define EXTERNAL_SENSOR_H

#include "driver/adc.h"

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

#endif // EXTERNAL_SENSOR_H
