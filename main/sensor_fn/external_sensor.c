#include <string.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_log.h"

#include "global_fn/global_var.h"
#include "external_sensor.h"


#define SOIL_DRY_VALUE    2650
#define SOIL_WET_VALUE    1500

static const char *TAG_EXTERNAL = "EXTERNAL_SENSOR";



int soil_sensor_map(int raw, int wet, int dry) {

    if (dry == wet) {
        return 0;
    }

    int moisture = 100 - ((raw - wet) * 100 / (dry - wet));

    if (moisture > 100)
        moisture = 100;

    if (moisture < 0)
        moisture = 0;

    return moisture;
}


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

static bool is_digital_type(sensor_type_t type) {
    return (type == SENSOR_TEMP) || (type == SENSOR_HUMIDITY);
}



void external_sensors_init(ExternalSensorlist *sensorList) {
    adc1_config_width(ADC_WIDTH_BIT_12);

    for(int sensor_index = 0; sensor_index < 6; sensor_index++) {
        ExternalSensor *sensor = &sensorList->externalSensor[sensor_index];
        sensor_type_t type = get_sensor_type(sensor->sensor_id);
        const char *name = get_sensor_name(sensor->sensor_id);

        SensorS *slot = &sensorMap.sensorS[sensor_index];
        strlcpy(slot->sensor_id, sensor->sensor_id, sizeof(slot->sensor_id));

        if (type == SENSOR_NONE) {
            ESP_LOGI(TAG_EXTERNAL, "Sensor not config on: %s",  sensor->sensor_id);
            slot->available = false;
            continue;
        }

        slot->type = type;
        strlcpy(slot->sensor_name, name, sizeof(slot->sensor_name));

        if (is_digital_type(type)) {
            if (sensor->pin_D0 >= 34) {
                ESP_LOGE(TAG_EXTERNAL, "Sensor %s: GPIO %d is input-only, cannot drive 1-wire bus", sensor->sensor_id, sensor->pin_D0);
                slot->available = false;
                strlcpy(slot->error_msg, "invalid digital pin (input-only)", sizeof(slot->error_msg));
                continue;
            }

            gpio_config_t io = {
                .pin_bit_mask = (1ULL << sensor->pin_D0),
                .mode = GPIO_MODE_INPUT,
                .pull_up_en = GPIO_PULLUP_ENABLE,
                .pull_down_en = GPIO_PULLDOWN_DISABLE,
                .intr_type = GPIO_INTR_DISABLE
            };
            gpio_config(&io);
 
            sensor->channel = ADC1_CHANNEL_MAX;
            slot->available = true;
 
            ESP_LOGI(TAG_EXTERNAL, "Sensor %s: digital (%s) on GPIO %d", sensor->sensor_id, (type == SENSOR_TEMP) ? "DS18B20" : "DHT22", sensor->pin_D0);


        } else {
            adc1_channel_t ch = gpio_to_adc(sensor->pin_A0);

            if (ch == ADC1_CHANNEL_MAX) {
                ESP_LOGE(TAG_EXTERNAL, "Invalid GPIO for ADC: %d", sensor->pin_A0);
                slot->available = false;
                strlcpy(slot->error_msg, "invalid ADC pin", sizeof(slot->error_msg));
                continue;
            }

            sensor->channel = ch;
            adc1_config_channel_atten(sensor->channel, ADC_ATTEN_DB_11);
            slot->available = true;

            ESP_LOGI(TAG_EXTERNAL, "Sensor %s: analog on GPIO %d (ADC ch %d)", sensor->sensor_id, sensor->pin_A0, ch);
        }

    }

}


void external_sesnor_read(int sensor_index, ExternalSensor *hw) {

    sensor_type_t type = sensorMap.sensorS[sensor_index].type;

    int raw = 0;
    float value = 0.0f;
    esp_err_t err = ESP_OK;
    const char *err_str = "";

    switch (type) {
        case SENSOR_MOISTURE:
            if (hw->channel == ADC1_CHANNEL_MAX) {
                err = ESP_ERR_INVALID_STATE;
                err_str = "invalid ADC channel";
                break;
            }
            raw = adc1_get_raw(hw->channel);
            value = (float)soil_sensor_map(raw, SOIL_WET_VALUE, SOIL_DRY_VALUE);
            break;

        case SENSOR_PH:
            if (hw->channel == ADC1_CHANNEL_MAX) {
                err = ESP_ERR_INVALID_STATE;
                err_str = "invalid ADC channel";
                break;
            }
            raw = adc1_get_raw(hw->channel);
            value = (float)raw;
            break;

        case SENSOR_TEMP:
            float temp_c = 0.0f;
            // err = ds18b20_read_temp(hw->pin_D0, &temp_c);
            // if (err == ESP_OK) {
            //     value = temp_c;
            //     raw = (int)(temp_c * 16.0f);
            // } else {
            //     err_str = esp_err_to_name(err);
            // }
            break;

        case SENSOR_HUMIDITY:
            float hum = 0.0f;
            float temp_unused = 0.0f;
            // err = dht22_read(hw->pin_D0, &temp_unused, &hum);
            // if (err == ESP_OK) {
            //     value = hum;
            //     raw = (int)(hum * 10.0f);
            // } else {
            //     err_str = esp_err_to_name(err);
            // }
            break;
        
        default:
            err = ESP_ERR_NOT_SUPPORTED;
            err_str = "unknown sensor type";
            break;
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG_EXTERNAL, "Sensor %s read failed: %s", sensorMap.sensorS[sensor_index].sensor_id, err_str);
    }


    xSemaphoreTake(ExternalsensorMutex, portMAX_DELAY);
    if (err == ESP_OK) {
        sensorMap.sensorS[sensor_index].data.raw   = raw;
        sensorMap.sensorS[sensor_index].data.value = value;
        sensorMap.sensorS[sensor_index].error_msg[0] = '\0';
    } else {
        strlcpy(sensorMap.sensorS[sensor_index].error_msg, err_str, sizeof(sensorMap.sensorS[sensor_index].error_msg));
    }
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

