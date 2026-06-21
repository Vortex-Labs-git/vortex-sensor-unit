
#include <string.h>
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"

#include "sensor_config.h"
#include "global_fn/global_var.h"

#define SENSOR_NVS_NAMESPACE   "sensor_cfg"
#define SENSOR_NVS_KEY         "map"

#define DEFAULT_S02_TYPE  CONFIG_SENSOR_02_TYPE
#define DEFAULT_S03_TYPE  CONFIG_SENSOR_03_TYPE
#define DEFAULT_S04_TYPE  CONFIG_SENSOR_04_TYPE
#define DEFAULT_S05_TYPE  CONFIG_SENSOR_05_TYPE
#define DEFAULT_S06_TYPE  CONFIG_SENSOR_06_TYPE
#define DEFAULT_S07_TYPE  CONFIG_SENSOR_07_TYPE

static const char *TAG_SENSOR = "sensor_storage";



esp_err_t sensor_config_load(void) {
    nvs_handle_t handle;
    esp_err_t err;
    size_t size = sizeof(GetSensors);

    err = nvs_open(SENSOR_NVS_NAMESPACE, NVS_READONLY, &handle);

    if (err != ESP_OK) {
        ESP_LOGW(TAG_SENSOR, "No stored sensor config");
        return err;
    }

    err = nvs_get_blob(handle, SENSOR_NVS_KEY, &UnitSensorConfig, &size);

    nvs_close(handle);

    if (err == ESP_OK) {
        ESP_LOGI(TAG_SENSOR, "Sensor config loaded");
        for (int i = 0; i < 6; i++) {
            ESP_LOGI(TAG_SENSOR, "Sensor[%d]: id=%s, type=%d, name=%s", i, UnitSensorConfig.sensors[i].sensor_id, UnitSensorConfig.sensors[i].type, UnitSensorConfig.sensors[i].sensor_name);
        }
        return ESP_OK;
    }

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG_SENSOR, "No sensor config found, loading defaults");

        sensor_config_restore_default();
        return ESP_OK;
    }

    ESP_LOGE(TAG_SENSOR, "Failed to read sensor config (%s)", esp_err_to_name(err));

    return err;
}



esp_err_t sensor_config_save(void) {
    nvs_handle_t handle;
    esp_err_t err;

    err = nvs_open(SENSOR_NVS_NAMESPACE, NVS_READWRITE, &handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG_SENSOR, "Failed to open NVS (%s)", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_blob(handle, SENSOR_NVS_KEY, &UnitSensorConfig, sizeof(GetSensors));

    if (err != ESP_OK) {
        ESP_LOGE(TAG_SENSOR, "Failed to write sensor config (%s)", esp_err_to_name(err));

        nvs_close(handle);
        return err;
    }

    err = nvs_commit(handle);

    if (err == ESP_OK) {
        ESP_LOGI(TAG_SENSOR, "Sensor config saved");
        for (int i = 0; i < 6; i++) {
            ESP_LOGI(TAG_SENSOR, "Sensor[%d]: id=%s, type=%d, name=%s", i, UnitSensorConfig.sensors[i].sensor_id, UnitSensorConfig.sensors[i].type, UnitSensorConfig.sensors[i].sensor_name);
        }
    } else {
        ESP_LOGE(TAG_SENSOR, "Commit failed (%s)", esp_err_to_name(err));
    }

    nvs_close(handle);

    return err;
}



void sensor_config_restore_default(void) {
    memset(&UnitSensorConfig, 0, sizeof(GetSensors));

    strcpy(UnitSensorConfig.sensors[0].sensor_id, "S02");
    UnitSensorConfig.sensors[0].type = (sensor_type_t)DEFAULT_S02_TYPE;

    strcpy(UnitSensorConfig.sensors[1].sensor_id, "S03");
    UnitSensorConfig.sensors[1].type = (sensor_type_t)DEFAULT_S03_TYPE;

    strcpy(UnitSensorConfig.sensors[2].sensor_id, "S04");
    UnitSensorConfig.sensors[2].type = (sensor_type_t)DEFAULT_S04_TYPE;

    strcpy(UnitSensorConfig.sensors[3].sensor_id, "S05");
    UnitSensorConfig.sensors[3].type = (sensor_type_t)DEFAULT_S05_TYPE;

    strcpy(UnitSensorConfig.sensors[4].sensor_id, "S06");
    UnitSensorConfig.sensors[4].type = (sensor_type_t)DEFAULT_S06_TYPE;

    strcpy(UnitSensorConfig.sensors[5].sensor_id, "S07");
    UnitSensorConfig.sensors[5].type = (sensor_type_t)DEFAULT_S07_TYPE;

    sensor_config_save();

    ESP_LOGI(TAG_SENSOR, "Sensor configuration restored to defaults");
}