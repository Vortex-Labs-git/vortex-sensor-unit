#ifndef SENSOR_PROCESS_H
#define SENSOR_PROCESS_H


#include "aht10_sensor.h"
#include "external_sensor.h"
#include "led_indicators.h"


extern LedIndicator redLED;
extern LedIndicator greenLED;
extern AHT10 aht10;
extern ExternalSensorlist externalSensorList;

void init_sensor_unit(void);

void aht10_sensor_task(void *pvParameters);
void external_sensor_task(void *pvParameters);

#endif // SENSOR_PROCESS_H
