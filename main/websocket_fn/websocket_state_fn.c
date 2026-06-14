#include <string.h>
#include <stdbool.h>
#include "esp_log.h"
#include "cJSON.h"
#include "sdkconfig.h" 

#include "global_fn/global_var.h"
#include "websocket_state_fn.h"
#include "time_fn/time_func.h"
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









/*===============================================================
 *                OFFLINE EVENT HANDLER
 *==============================================================*/

/**
 * @brief Handles WebSocket events after authentication.
 * 
 * Supported events:
 *   - device_basic_info
 *   - set_valve_basic
 *   - set_valve_wifi
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
                    ESP_LOGW(WEB_STATE_TAG, "User send the correct device ID %s, user ID %s", device_id->valuestring, user_id->valuestring);
                    // send_device_data();
                    ESP_LOGW(WEB_STATE_TAG, "Send valve data");
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
    /*----------------- UPDATE WIFI CREDENTIALS -----------------*/
    else if ( strcmp(event->valuestring, "set_valve_wifi") == 0) {
        ESP_LOGI(WEB_STATE_TAG, "Event matched: set_valve_wifi");

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

