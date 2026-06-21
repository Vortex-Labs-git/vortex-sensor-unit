#include <string.h>
#include <stdbool.h>
#include "cJSON.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include "time_fn/time_func.h"
#include "global_fn/global_var.h"
#include "sensor_fn/external_sensor.h"
#include "mqtt_state_fn.h"


/*---------------------------------------------------------------
 * Configuration
 *--------------------------------------------------------------*/

// Device ID configured from menuconfig
#define DEVICE_ID CONFIG_SENSOR_UNIT_ID

static const char *TAG = "MQTT_STATE";




/*===============================================================
 *              CREATE JSON: SENSOR UNIT STATUS
 *==============================================================*/

/**
 * @brief Create JSON object for sensorunit online status
 */
cJSON* create_sensorunit_status() {
    cJSON *json = cJSON_CreateObject();

    char timestamp[20];
    get_current_timestamp(timestamp, sizeof(timestamp));

    cJSON_AddStringToObject(json, "event", "sensor_unit_status");
    cJSON_AddStringToObject(json, "timestamp", timestamp);
    cJSON_AddStringToObject(json, "device_id", DEVICE_ID);
    cJSON_AddStringToObject(json, "status", "online");

    return json;
}



/*===============================================================
 *              CREATE JSON: SENSOR UNIT STATE DATA
 *==============================================================*/

/**
 * @brief Create JSON object containing:
 *        - sensor unit state
 */
cJSON* create_sensorunit_state_data() {

    // Create the JSON object
    cJSON *json = cJSON_CreateObject();

    char timestamp[20];
    get_current_timestamp(timestamp, sizeof(timestamp));

    cJSON_AddStringToObject(json, "event", "sensor_unit_info");
    cJSON_AddStringToObject(json, "device_id", DEVICE_ID);
    cJSON_AddStringToObject(json, "device_name", "device name");
    cJSON_AddStringToObject(json, "timestamp", timestamp);

    cJSON *data = cJSON_CreateArray();

    int sensor_count = 0;

    AHT10Sensor in_snap;
    xSemaphoreTake(InbuildsensorMutex, portMAX_DELAY);
    in_snap = aht10Sensor;
    xSemaphoreGive(InbuildsensorMutex);
    /* ----------------------------------------------------
     * Sensor 1 : Built-in Temperature (always present)
     * ---------------------------------------------------- */
    {
        cJSON *sensor = cJSON_CreateObject();

        cJSON_AddStringToObject(sensor, "sensor_id", "S01");
        cJSON_AddStringToObject(sensor, "sensor_type", "Temperature");
        cJSON_AddStringToObject(sensor, "sensor_name", "Inbuild Temp");
        cJSON_AddNumberToObject(sensor, "sensor_value", in_snap.temperature);

        cJSON_AddItemToArray(data, sensor);
        sensor_count++;
    }

    /* ----------------------------------------------------
     * Sensor 2 : Built-in Humidity (always present)
     * ---------------------------------------------------- */
    {
        cJSON *sensor = cJSON_CreateObject();

        cJSON_AddStringToObject(sensor, "sensor_id", "S02");
        cJSON_AddStringToObject(sensor, "sensor_type", "Humidity");
        cJSON_AddStringToObject(sensor, "sensor_name", "inbuild humidity");
        cJSON_AddNumberToObject(sensor, "sensor_value", in_snap.humidity);

        cJSON_AddItemToArray(data, sensor);
        sensor_count++;
    }

    /* ----------------------------------------------------
     * External sensors S02-S07
     * Only add available sensors
     * ---------------------------------------------------- */

    SensorMap ex_snap;
    xSemaphoreTake(ExternalsensorMutex, portMAX_DELAY);
    ex_snap = sensorMap;
    xSemaphoreGive(ExternalsensorMutex);
    for (int i = 0; i < 6; i++)
    {
        if (!ex_snap.sensorS[i].available)
            continue;

        cJSON *sensor = cJSON_CreateObject();

        char sensor_id[32];
        snprintf(sensor_id, sizeof(sensor_id), "%s", ex_snap.sensorS[i].sensor_id);
        cJSON_AddStringToObject(sensor, "sensor_id", sensor_id);

        cJSON_AddStringToObject(sensor, "sensor_type", sensor_type_to_string( ex_snap.sensorS[i].type));
        cJSON_AddStringToObject(sensor, "sensor_name", ex_snap.sensorS[i].sensor_name);
        cJSON_AddNumberToObject(sensor, "sensor_value", ex_snap.sensorS[i].data.value);

        cJSON_AddItemToArray(data, sensor);
        sensor_count++;
    }

    cJSON_AddNumberToObject(json, "no_sensors", sensor_count);
    cJSON_AddItemToObject(json, "data", data);


    return json;
}



/*===============================================================
 *              CREATE JSON: SENSOR UNIT ERROR
 *==============================================================*/

/**
 * @brief Create JSON object for error reporting
 */
cJSON* create_sensorunit_error() {

    cJSON *json = cJSON_CreateObject();

    char timestamp[20];
    get_current_timestamp(timestamp, sizeof(timestamp));

    cJSON_AddStringToObject(json, "event", "sensor_unit_error");
    cJSON_AddStringToObject(json, "timestamp", timestamp);
    cJSON_AddStringToObject(json, "device_id", DEVICE_ID);

    cJSON *error_array = cJSON_CreateArray();

    AHT10Sensor in_snap;
    xSemaphoreTake(InbuildsensorMutex, portMAX_DELAY);
    in_snap = aht10Sensor;
    xSemaphoreGive(InbuildsensorMutex);
    /* Built-in Temperature */
    {
        cJSON *err = cJSON_CreateObject();
        cJSON_AddStringToObject(err, "sensor_id", "S00");
        cJSON_AddStringToObject(err, "error", in_snap.error_msg);
        cJSON_AddItemToArray(error_array, err);
    }

    /* Built-in Humidity */
    {
        cJSON *err = cJSON_CreateObject();
        cJSON_AddStringToObject(err, "sensor_id", "S01");
        cJSON_AddStringToObject(err, "error", in_snap.error_msg);
        cJSON_AddItemToArray(error_array, err);
    }


    /* External sensors */
    SensorMap ex_snap;
    xSemaphoreTake(ExternalsensorMutex, portMAX_DELAY);
    ex_snap = sensorMap;
    xSemaphoreGive(ExternalsensorMutex);
    for (int i = 0; i < 6; i++) {
        if (!ex_snap.sensorS[i].available)
            continue;

        cJSON *err = cJSON_CreateObject();

        cJSON_AddStringToObject( err, "sensor_id", ex_snap.sensorS[i].sensor_id);
        cJSON_AddStringToObject( err, "error", ex_snap.sensorS[i].error_msg);
        cJSON_AddItemToArray(error_array, err);
    }

    cJSON_AddItemToObject(json, "error", error_array);

    return json;
}

