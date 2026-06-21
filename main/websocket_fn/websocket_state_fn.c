#include <string.h>
#include <stdbool.h>
#include "esp_log.h"
#include "cJSON.h"
#include "sdkconfig.h" 

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "global_fn/global_var.h"
#include "sensor_fn/external_sensor.h"
#include "websocket_state_fn.h"
#include "time_fn/time_func.h"
#include "eeprom_fn/sensor_config.h"
#include "eeprom_fn/wifi_storage.h"
#include "websocket_server_fn.h"


/*---------------------------------------------------------------
 * Configuration
 *--------------------------------------------------------------*/

// Device ID from menuconfig
#define DEVICE_ID CONFIG_SENSOR_UNIT_ID

// WebSocket authentication passkey (menuconfig)
#define PASSKEY_VALUE CONFIG_WS_PASSKEY_VALUE

static const char *WEB_STATE_TAG = "STATE UPDATE OFFLINE";



/*===============================================================
 *                  SEND DEVICE BASIC INFO
 *==============================================================*/

/**
 * @brief Send device identification information via WebSocket.
 * 
 * This is typically sent after successful authentication.
 */
void send_device_info(void) {
    if (esp_server == NULL) return;

    char timestamp[20];
    get_current_timestamp(timestamp, sizeof(timestamp));

    // Create the JSON object
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "event", "device_info");
    cJSON_AddStringToObject(json, "timestamp", timestamp);
    cJSON_AddStringToObject(json, "device_id", DEVICE_ID);

    // Convert the JSON object to string (allocate memory)
    char *json_string = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);

    // Send via WebSocket
    if (httpd_queue_work(esp_server, websocket_async_send, json_string) != ESP_OK) {
        ESP_LOGE(WEB_STATE_TAG, "Failed to queue work");
        free(json_string);
    }
}


void send_sensorunit_data(void) {
    if (esp_server == NULL) return;

    char timestamp[20];
    get_current_timestamp(timestamp, sizeof(timestamp));

    // Create the JSON object
    cJSON *json = cJSON_CreateObject();

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


    // Convert the JSON object to string (allocate memory)
    char *json_string = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);

    // Send via WebSocket
    if (httpd_queue_work(esp_server, websocket_async_send, json_string) != ESP_OK) {
        ESP_LOGE(WEB_STATE_TAG, "Failed to queue work");
        free(json_string);
    }
}



void get_sensorunit_config(void) {
    if (esp_server == NULL) return;

    char timestamp[20];
    get_current_timestamp(timestamp, sizeof(timestamp));

    // Create the JSON object
    cJSON *json = cJSON_CreateObject();

    cJSON_AddStringToObject(json, "event", "get_sensor_config");
    cJSON_AddStringToObject(json, "device_id", DEVICE_ID);
    cJSON_AddStringToObject(json, "timestamp", timestamp);

    cJSON *sensor_array = cJSON_CreateArray();
    int sensor_count = 0;

    for (int i = 0; i < 6; i++)
    {
        if (UnitSensorConfig.sensors[i].type == SENSOR_NONE)
            continue;

        cJSON *sensor = cJSON_CreateObject();

        cJSON_AddStringToObject( sensor, "sensor_id", UnitSensorConfig.sensors[i].sensor_id);
        cJSON_AddStringToObject( sensor, "sensor_type", sensor_type_to_string( UnitSensorConfig.sensors[i].type));
        cJSON_AddStringToObject( sensor, "sensor_name", UnitSensorConfig.sensors[i].sensor_name);

        cJSON_AddItemToArray(sensor_array, sensor);

        sensor_count++;
    }

    cJSON_AddNumberToObject(json, "no_sensors", sensor_count);
    cJSON_AddItemToObject(json, "sensor_data", sensor_array);

    // Convert the JSON object to string (allocate memory)
    char *json_string = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);

    // Send via WebSocket
    if (httpd_queue_work(esp_server, websocket_async_send, json_string) != ESP_OK) {
        ESP_LOGE(WEB_STATE_TAG, "Failed to queue work");
        free(json_string);
    }
}




esp_err_t set_sensorunit_config(cJSON *sensor_data, cJSON *no_sensors)
{
    if (!cJSON_IsArray(sensor_data)) {
        return ESP_ERR_INVALID_ARG;
    }

    int count = 0;

    if (cJSON_IsNumber(no_sensors)) {
        count = no_sensors->valueint;
    }
    else if (cJSON_IsString(no_sensors)) {
        count = atoi(no_sensors->valuestring);
    }
    else {
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *sensor = NULL;
    int updated_count = 0;
    int j = 0;

    cJSON_ArrayForEach(sensor, sensor_data)
    {
        if (j >= count) {
            break;
        }

        cJSON *sensor_id   = cJSON_GetObjectItem(sensor, "sensor_id");
        cJSON *sensor_type = cJSON_GetObjectItem(sensor, "sensor_type");
        cJSON *sensor_name = cJSON_GetObjectItem(sensor, "sensor_name");

        if (!cJSON_IsString(sensor_id) || !cJSON_IsString(sensor_type) || !cJSON_IsString(sensor_name)) {
            continue;
        }

        for (int i = 0; i < 6; i++) {
            if (strcmp(UnitSensorConfig.sensors[i].sensor_id, sensor_id->valuestring) == 0) {
                UnitSensorConfig.sensors[i].type = string_to_sensor_type(sensor_type->valuestring);

                strncpy( UnitSensorConfig.sensors[i].sensor_name, sensor_name->valuestring, sizeof(UnitSensorConfig.sensors[i].sensor_name) - 1);

                UnitSensorConfig.sensors[i].sensor_name[sizeof(UnitSensorConfig.sensors[i].sensor_name) - 1] = '\0';

                updated_count++;

                break;
            }
        }
        j++;
    }

    if (updated_count == 0) {
        return ESP_ERR_NOT_FOUND;
    }

    return ESP_OK;
}






/*===============================================================
 *                OFFLINE EVENT HANDLER
 *==============================================================*/

/**
 * @brief Handles WebSocket events after authentication.
 * 
 * Supported events:
 *   - device_basic_info
 */
void offline_data(cJSON *event, cJSON *json) {

    /*----------------- DEVICE BASIC INFO REQUEST -----------------*/
    if ( strcmp(event->valuestring, "device_basic_info") == 0) {
        ESP_LOGI(WEB_STATE_TAG, "Event matched: device_basic_info");

        cJSON *data = cJSON_GetObjectItem(json, "data");
        if (data != NULL && cJSON_IsObject(data)) {

            cJSON *user_id = cJSON_GetObjectItem(data, "user_id");
            cJSON *device_id = cJSON_GetObjectItem(data, "device_id");
            if (device_id != NULL && cJSON_IsString(device_id)) {
                if (strcmp(device_id->valuestring, DEVICE_ID) == 0) {
                    ESP_LOGI(WEB_STATE_TAG, "User send the correct device ID %s, user ID %s", device_id->valuestring, user_id->valuestring);
                    send_sensorunit_data();
                    ESP_LOGI(WEB_STATE_TAG, "Send send_sensorunit_data");
                } else {
                    ESP_LOGW(WEB_STATE_TAG, "User dont send the correct device ID");
                }
            } else {
                ESP_LOGW(WEB_STATE_TAG, "\"device_id\" is false or missing");
            }
        } else {
            ESP_LOGW(WEB_STATE_TAG, "\"data\" is false or missing");
        }
    }

    else if ( strcmp(event->valuestring, "get_sensor_config") == 0 ) {
        ESP_LOGI(WEB_STATE_TAG, "Event matched: get_sensor_config");

        cJSON *device_id = cJSON_GetObjectItem(json, "device_id");
        if (device_id != NULL && cJSON_IsString(device_id)) {
            if (strcmp(device_id->valuestring, DEVICE_ID) == 0) {
                ESP_LOGI(WEB_STATE_TAG, "User send the correct device ID %s", device_id->valuestring);
                get_sensorunit_config();
                ESP_LOGI(WEB_STATE_TAG, "Send get_sensorunit_config");
            } else {
                ESP_LOGW(WEB_STATE_TAG, "User dont send the correct device ID");
            }
        } else {
            ESP_LOGW(WEB_STATE_TAG, "\"device_id\" is false or missing");
        }
    }

    else if ( strcmp(event->valuestring, "set_sensor_config") == 0 ) {
        ESP_LOGI(WEB_STATE_TAG, "Event matched: set_sensor_config");

        cJSON *device_id = cJSON_GetObjectItem(json, "device_id");
        if (device_id != NULL && cJSON_IsString(device_id)) {
            if (strcmp(device_id->valuestring, DEVICE_ID) == 0) {
                ESP_LOGI(WEB_STATE_TAG, "User send the correct device ID %s", device_id->valuestring);
                
                cJSON *sensor_data = cJSON_GetObjectItem(json, "sensor_data");
                cJSON *no_sensors = cJSON_GetObjectItem(json, "no_sensors");

                if (sensor_data && cJSON_IsArray(sensor_data)) {
                    esp_err_t err = set_sensorunit_config(sensor_data, no_sensors);
                    if (err == ESP_OK) {
                        err = sensor_config_save();
                        if (err == ESP_OK) {
                            ESP_LOGI(WEB_STATE_TAG,"Sensor configuration updated and saved, Restating ....");
                            esp_restart();
                        } else {
                            ESP_LOGE(WEB_STATE_TAG, "Failed to save configuration");
                        }
                    } else {
                        ESP_LOGE(WEB_STATE_TAG, "Failed to update configuration (%s)", esp_err_to_name(err));
                    }
                } else {
                    ESP_LOGW(WEB_STATE_TAG, "\"sensor_data\" missing or invalid");
                }

                ESP_LOGW(WEB_STATE_TAG, "Send set_sensorunit_config");
            } else {
                ESP_LOGW(WEB_STATE_TAG, "User dont send the correct device ID");
            }
        } else {
            ESP_LOGW(WEB_STATE_TAG, "\"device_id\" is false or missing");
        }
    }


    /*----------------- UPDATE WIFI CREDENTIALS -----------------*/
    else if ( strcmp(event->valuestring, "set_device_wifi") == 0) {
        ESP_LOGI(WEB_STATE_TAG, "Event matched: set_device_wifi");

        cJSON *wifi_data = cJSON_GetObjectItem(json, "wifi_data");
        if (wifi_data != NULL && cJSON_IsObject(wifi_data)) {
            cJSON *ssid = cJSON_GetObjectItem(wifi_data, "ssid");
            cJSON *password = cJSON_GetObjectItem(wifi_data, "password");
            ESP_LOGI(WEB_STATE_TAG, "ssid: %s, password: %s", ssid->valuestring, password->valuestring);

            if (!cJSON_IsString(ssid) || !cJSON_IsString(password)) {
                ESP_LOGE(WEB_STATE_TAG, "Invalid WiFi JSON format");
            }

            if (strcmp(wifiStaData.ssid, ssid->valuestring) != 0 || strcmp(wifiStaData.password, password->valuestring) != 0) {
                memset(wifiStaData.ssid, 0, sizeof(wifiStaData.ssid));
                memset(wifiStaData.password, 0, sizeof(wifiStaData.password));

                strncpy(wifiStaData.ssid, ssid->valuestring, sizeof(wifiStaData.ssid) - 1);
                strncpy(wifiStaData.password, password->valuestring, sizeof(wifiStaData.password) - 1);

                wifiStaData.set_wifi = true;

                wifi_storage_save();

                ESP_LOGI(WEB_STATE_TAG, "WiFi updated. Restarting...");
                esp_restart();

            } else {
                ESP_LOGI(WEB_STATE_TAG, "WiFi data unchanged. No action taken.");
            }

        } else {
            ESP_LOGW(WEB_STATE_TAG, "\"wifi_data\" field is missing or not an object");
        }
    }
    else {
        ESP_LOGW(WEB_STATE_TAG, "Event type does not match: %s", event->valuestring);
    }
}




/*===============================================================
 *                  PROCESS WEBSOCKET MESSAGE
 *==============================================================*/

/**
 * @brief Entry point for WebSocket JSON message processing.
 * 
 * Handles:
 *   - Authentication (passkey validation)
 *   - Routing to offline_data after authorization
 */
void process_message(const char *payload, bool *connection_authorized) {

    // Parse the JSON string into a cJSON object
    cJSON *json = cJSON_Parse(payload);
    if (json == NULL) {
        ESP_LOGE(WEB_STATE_TAG, "Failed to parse JSON");
        return;
    }

    // Extract the "event" field
    cJSON *event = cJSON_GetObjectItem(json, "event");
    if (event == NULL || !cJSON_IsString(event)) {
        ESP_LOGW(WEB_STATE_TAG, "\"event\" field is missing in the JSON message");
        cJSON_Delete(json);
        return;
    }


    /*----------------- If Already Authorized -----------------*/
    if ( *connection_authorized) {
        offline_data( event, json);
    } 
    /*----------------- Authentication Phase -----------------*/
    else {
        if (strcmp(event->valuestring, "request_device_info") == 0) {
            ESP_LOGI(WEB_STATE_TAG, "Event matched: request_device_info");

            cJSON *passkey   = cJSON_GetObjectItem(json, "passkey");
            if ( cJSON_IsString(passkey) && (strcmp(passkey->valuestring, PASSKEY_VALUE) == 0)) {
                *connection_authorized = true;
                ESP_LOGI(WEB_STATE_TAG, "Passkey accept");

                send_device_info();
                ESP_LOGI(WEB_STATE_TAG, "Send Device info");
            } else {
                *connection_authorized = false;
                ESP_LOGI(WEB_STATE_TAG, "Passkey not accept");
            }
        } else {
            ESP_LOGW(WEB_STATE_TAG, "Connection not authorized");
        }
    }

    // Free memory to prevent leaks
    cJSON_Delete(json);
}

