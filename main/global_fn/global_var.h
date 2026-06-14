#ifndef GLOBAL_VAR_H
#define GLOBAL_VAR_H



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
    char sensor_id[5];
    sensor_type_t type;
} sensor_map_t;

typedef struct {
    sensor_map_t sensors[6];
} GetSensors;



// Define the AHT10 sensor data structure
typedef struct {
    float temperature;
    float humidity;
    char error_msg[100];
} AHT10Sensor;


// Define External sensor data structure
typedef struct {
    int raw;
    float value;
} SensorData;
typedef struct {
    bool available;
    char sensor_id[5];
    sensor_type_t type;

    char sensor_name[20];
    SensorData data;

    char error_msg[100];
} SensorS;

typedef struct {
    SensorS sensorS[6];
} SensorMap;


// Declare the global variables
extern GetWifi wifiStaData;
extern GetSensors UnitSensorConfig;

extern AHT10Sensor aht10Sensor;

extern SensorMap sensorMap;

#endif // GLOBAL_VAR_H