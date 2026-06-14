#ifndef GLOBAL_VAR_H
#define GLOBAL_VAR_H

#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"





// Define the structure for get_wifi
typedef struct {
    char ssid[32];
    char password[64];
    bool set_wifi;
} GetWifi;


// Define the structure for get_sensor_config
typedef enum {
    SENSOR_NONE = 0,
    SENSOR_TEMP,
    SENSOR_MOISTURE,
    SENSOR_HUMIDITY,
    SENSOR_PH,
} sensor_type_t;

typedef struct {
    char port_id[5];
    sensor_type_t type;
} sensor_map_t;

typedef struct {
    sensor_map_t sensors[6];
} GetSensors;



// Declare the global variables
extern GetWifi wifiStaData;
extern GetSensors UnitSensorConfig;


#endif // GLOBAL_VAR_H