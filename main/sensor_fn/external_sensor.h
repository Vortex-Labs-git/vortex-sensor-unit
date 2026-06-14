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

extern ExternalSensorlist externalSensorList;


void external_sensors_init(ExternalSensorlist *sensorList);
void external_sensor_task(void *pvParameters);

#endif // EXTERNAL_SENSOR_H
