#include <string.h>
#include <stdbool.h>
#include "cJSON.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include "time_fn/time_func.h"
#include "global_fn/global_var.h"
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

    cJSON *json = cJSON_CreateObject();

    char timestamp[20];
    get_current_timestamp(timestamp, sizeof(timestamp));

    cJSON_AddStringToObject(json, "event", "sensor_unit_info");
    cJSON_AddStringToObject(json, "timestamp", timestamp);
    cJSON_AddStringToObject(json, "device_id", DEVICE_ID);


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


    return json;
}

