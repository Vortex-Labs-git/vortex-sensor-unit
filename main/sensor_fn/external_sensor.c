#include <string.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_log.h"

#include "global_fn/global_var.h"
#include "external_sensor.h"

static const char *TAG_EXTERNAL = "EXTERNAL_SENSOR";


sensor_type_t get_sensor_type(const char *id) {
    for (int i = 0; i < 6; i++) {
        if (strcmp(UnitSensorConfig.sensors[i].sensor_id, id) == 0) {
            return UnitSensorConfig.sensors[i].type;
        }
    }
    return SENSOR_NONE;
}
const char *get_sensor_name(const char *id) {
    for (int i = 0; i < 6; i++) {
        if (strcmp(UnitSensorConfig.sensors[i].sensor_id, id) == 0) {
            return UnitSensorConfig.sensors[i].sensor_name;
        }
    }

    return "";
}


static adc1_channel_t gpio_to_adc(uint8_t gpio) {
    switch (gpio) {
        case 32: return ADC1_CHANNEL_4;
        case 33: return ADC1_CHANNEL_5;
        case 34: return ADC1_CHANNEL_6;
        case 35: return ADC1_CHANNEL_7;
        case 36: return ADC1_CHANNEL_0;
        case 39: return ADC1_CHANNEL_3;
        default: return ADC1_CHANNEL_MAX;
    }
}


void external_sensors_init(ExternalSensorlist *sensorList) {
    adc1_config_width(ADC_WIDTH_BIT_12);

    for(int sensor_index = 0; sensor_index < 6; sensor_index++) {
        ExternalSensor *sensor = &sensorList->externalSensor[sensor_index];
        sensor_type_t type = get_sensor_type(sensor->sensor_id);
        const char *name = get_sensor_name(sensor->sensor_id);

        if (type == SENSOR_NONE) {
            ESP_LOGI(TAG_EXTERNAL, "Sensor not config on: %s",  sensor->sensor_id);
            sensorMap.sensorS[sensor_index].available = false;
            continue;
        }

        sensorMap.sensorS[sensor_index].available = true;
        sensorMap.sensorS[sensor_index].type = type;
        strcpy(sensorMap.sensorS[sensor_index].sensor_name, name);
        adc1_channel_t ch = gpio_to_adc(sensor->pin_A0);

        if (ch == ADC1_CHANNEL_MAX) {
            ESP_LOGE(TAG_EXTERNAL, "Invalid GPIO for ADC: %d", sensor->pin_A0);
            sensorMap.sensorS[sensor_index].available = false;
            continue;
        }
        sensor->channel = ch;

        adc1_config_channel_atten(sensor->channel, ADC_ATTEN_DB_11);
    }

}


void external_sesnor_read(int sensor_index, ExternalSensor *hw) {
    if (hw->channel == ADC1_CHANNEL_MAX) {
        ESP_LOGE(TAG_EXTERNAL, "Invalid ADC channel");
        return;
    }

    int raw = adc1_get_raw(hw->channel);

    xSemaphoreTake(ExternalsensorMutex, portMAX_DELAY);
    sensorMap.sensorS[sensor_index].data.raw = raw;
    xSemaphoreGive(ExternalsensorMutex);

}


const char *sensor_type_to_string(sensor_type_t type) {
    switch (type)
    {
        case SENSOR_TEMP:
            return "Temperature";

        case SENSOR_HUMIDITY:
            return "Humidity";

        case SENSOR_MOISTURE:
            return "Moisture";

        case SENSOR_PH:
            return "pH";

        default:
            return "Unknown";
    }
}

sensor_type_t string_to_sensor_type(const char *type) {
    if (strcmp(type, "Temperature") == 0) return SENSOR_TEMP;
    if (strcmp(type, "Humidity") == 0)    return SENSOR_HUMIDITY;
    if (strcmp(type, "Moisture") == 0)    return SENSOR_MOISTURE;
    if (strcmp(type, "pH") == 0)          return SENSOR_PH;

    return SENSOR_NONE;
}