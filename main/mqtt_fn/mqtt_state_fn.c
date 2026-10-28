#include <string.h>
#include <stdbool.h>
#include "cJSON.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include "time_fn/time_func.h"
#include "global_fn/global_var.h"
#include "ota_fn/ota_update_fn.h"
#include "sensor_fn/external_sensor.h"
#include "mqtt_state_fn.h"



static const char *TAG = "MQTT_STATE";





/*===============================================================
 *              HANDLE BASIC COMMAND DATA (cmd_data)
 *==============================================================*/
void mqtt_handle_cmd_data(const char *data) {
    cJSON *json_cmd_data = cJSON_Parse(data);

    if (json_cmd_data == NULL) {
        ESP_LOGE(TAG, "Invalid JSON received");
        return;
    }

    // Extract top-level fields
    cJSON *event = cJSON_GetObjectItem(json_cmd_data, "event");
    cJSON *device_id = cJSON_GetObjectItem(json_cmd_data, "device_id");

    /*----------------- OTA Update Request -----------------*/
    cJSON *ota_update = cJSON_GetObjectItem(json_cmd_data, "ota_update");
    if (cJSON_IsObject(ota_update)) {
        cJSON *url = cJSON_GetObjectItem(ota_update, "url");
        cJSON *version = cJSON_GetObjectItem(ota_update, "version");

        if (cJSON_IsString(url) && cJSON_IsString(version)) {
            ESP_LOGI(TAG, "OTA request: v%s from %s", version->valuestring, url->valuestring);
            ota_start(url->valuestring, version->valuestring);
        }
    }

    cJSON_Delete(json_cmd_data);
}



/*===============================================================
 *                 GENERIC TOPIC ROUTER
 *==============================================================*/
void mqtt_handle_topic(const char *data) {
    cJSON *json_data = cJSON_Parse(data);

    if (json_data == NULL) {
        ESP_LOGE(TAG, "Invalid JSON received");
        return;
    }

    cJSON *event = cJSON_GetObjectItem(json_data, "event");
    if (cJSON_IsString(event)) {
        ESP_LOGI(TAG, "Received event: %s", event->valuestring);
    } else {
        ESP_LOGW(TAG, "Event field missing or not a string");
    }

    if (strcmp(event->valuestring, "set_valve_basic") == 0) {
        mqtt_handle_cmd_data(data);

    } else {
        ESP_LOGW(TAG, "Unknown event type: %s", event->valuestring);
    }

    cJSON_Delete(json_data);
}




/*===============================================================
 *              CREATE JSON: SENSOR UNIT STATUS
 *==============================================================*/

/**
 * @brief Create JSON object for sensorunit online status
 */
cJSON* create_sensorunit_status() {
    cJSON *json = cJSON_CreateObject();

    char timestamp[32];
    get_current_timestamp(timestamp, sizeof(timestamp));

    cJSON_AddStringToObject(json, "event", "sensor_unit_status");
    cJSON_AddStringToObject(json, "timestamp", timestamp);
    cJSON_AddStringToObject(json, "device_id", deviceIdentity.device_id);
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

    char timestamp[32];
    get_current_timestamp(timestamp, sizeof(timestamp));

    cJSON_AddStringToObject(json, "event", "sensor_unit_info");
    cJSON_AddStringToObject(json, "device_id", deviceIdentity.device_id);
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

        cJSON_AddStringToObject(sensor, "sensor_id", "S00");
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

        cJSON_AddStringToObject(sensor, "sensor_id", "S01");
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

    char timestamp[32];
    get_current_timestamp(timestamp, sizeof(timestamp));

    cJSON_AddStringToObject(json, "event", "sensor_unit_error");
    cJSON_AddStringToObject(json, "timestamp", timestamp);
    cJSON_AddStringToObject(json, "device_id", deviceIdentity.device_id);

    cJSON *error_array = cJSON_CreateArray();

    AHT10Sensor in_snap;
    xSemaphoreTake(InbuildsensorMutex, portMAX_DELAY);
    in_snap = aht10Sensor;
    xSemaphoreGive(InbuildsensorMutex);
    if (strlen(in_snap.error_msg) > 0) {
        /* Built-in Temperature */
        cJSON *err = cJSON_CreateObject();
        cJSON_AddStringToObject(err, "sensor_id", "S00");
        cJSON_AddStringToObject(err, "error", in_snap.error_msg);
        cJSON_AddItemToArray(error_array, err);

        /* Built-in Humidity */
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

        if (strlen(ex_snap.sensorS[i].error_msg) == 0)
            continue;

        cJSON *err = cJSON_CreateObject();

        cJSON_AddStringToObject( err, "sensor_id", ex_snap.sensorS[i].sensor_id);
        cJSON_AddStringToObject( err, "error", ex_snap.sensorS[i].error_msg);
        cJSON_AddItemToArray(error_array, err);
    }

    cJSON_AddItemToObject(json, "error", error_array);

    return json;
}

