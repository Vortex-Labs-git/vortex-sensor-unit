#include <string.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_log.h"

// 1-Wire RMT / DS18B20 Headers
#include "onewire_bus.h"
#include "ds18b20.h"

#include "global_fn/global_var.h"
#include "external_sensor.h"

#define SOIL_DRY_VALUE    2650
#define SOIL_WET_VALUE    1500

static const char *TAG_EXTERNAL = "EXTERNAL_SENSOR";

// Dynamic storage for DS18B20 handles for up to 6 slots
static ds18b20_device_handle_t ds18b20_handles[6] = {NULL};
static onewire_bus_handle_t    onewire_buses[6]   = {NULL};

int soil_sensor_map(int raw, int wet, int dry) {
    if (dry == wet) {
        return 0;
    }

    int moisture = 100 - ((raw - wet) * 100 / (dry - wet));

    if (moisture > 100) moisture = 100;
    if (moisture < 0)   moisture = 0;

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

    for (int sensor_index = 0; sensor_index < 6; sensor_index++) {
        ExternalSensor *sensor = &sensorList->externalSensor[sensor_index];
        sensor_type_t type = get_sensor_type(sensor->sensor_id);
        const char *name = get_sensor_name(sensor->sensor_id);

        SensorS *slot = &sensorMap.sensorS[sensor_index];
        strlcpy(slot->sensor_id, sensor->sensor_id, sizeof(slot->sensor_id));

        if (type == SENSOR_NONE) {
            ESP_LOGI(TAG_EXTERNAL, "Sensor not config on: %s", sensor->sensor_id);
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

            sensor->channel = ADC1_CHANNEL_MAX;

            if (type == SENSOR_TEMP) {
                // Initialize DS18B20 via OneWire RMT driver
                onewire_bus_config_t bus_config = {
                    .bus_gpio_num = sensor->pin_D0,
                };
                onewire_bus_rmt_config_t rmt_config = {
                    .max_rx_bytes = 10,
                };

                esp_err_t err = onewire_new_bus_rmt(&bus_config, &rmt_config, &onewire_buses[sensor_index]);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG_EXTERNAL, "Failed to init OneWire bus on GPIO %d: %s", sensor->pin_D0, esp_err_to_name(err));
                    slot->available = false;
                    strlcpy(slot->error_msg, "onewire bus init fail", sizeof(slot->error_msg));
                    continue;
                }

                // Search for device on the bus
                onewire_device_iter_handle_t iter = NULL;
                err = onewire_new_device_iter(onewire_buses[sensor_index], &iter);
                onewire_device_t device;

                if (err == ESP_OK && onewire_device_iter_get_next(iter, &device) == ESP_OK) {
                    ds18b20_config_t ds18b20_config = {
                        .resolution = DS18B20_RESOLUTION_12B,
                    };

                    err = ds18b20_new_device(&device, &ds18b20_config, &ds18b20_handles[sensor_index]);
                    if (err == ESP_OK) {
                        slot->available = true;
                        ESP_LOGI(TAG_EXTERNAL, "Sensor %s: DS18B20 initialized on GPIO %d (Addr: %016llX)", 
                                 sensor->sensor_id, sensor->pin_D0, device.address);
                    } else {
                        ESP_LOGE(TAG_EXTERNAL, "Failed to create DS18B20 device instance: %s", esp_err_to_name(err));
                        slot->available = false;
                        strlcpy(slot->error_msg, "ds18b20 device fail", sizeof(slot->error_msg));
                    }
                } else {
                    ESP_LOGE(TAG_EXTERNAL, "No DS18B20 found on GPIO %d", sensor->pin_D0);
                    slot->available = false;
                    strlcpy(slot->error_msg, "no ds18b20 found", sizeof(slot->error_msg));
                }

                if (iter) {
                    onewire_del_device_iter(iter);
                }
            } else {
                // Generic digital config (e.g., DHT22)
                gpio_config_t io = {
                    .pin_bit_mask = (1ULL << sensor->pin_D0),
                    .mode = GPIO_MODE_INPUT,
                    .pull_up_en = GPIO_PULLUP_ENABLE,
                    .pull_down_en = GPIO_PULLDOWN_DISABLE,
                    .intr_type = GPIO_INTR_DISABLE
                };
                gpio_config(&io);
                slot->available = true;
                ESP_LOGI(TAG_EXTERNAL, "Sensor %s: digital (DHT22) on GPIO %d", sensor->sensor_id, sensor->pin_D0);
            }

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

        case SENSOR_TEMP: {
            ds18b20_device_handle_t ds_handle = ds18b20_handles[sensor_index];
            if (ds_handle == NULL) {
                err = ESP_ERR_INVALID_STATE;
                err_str = "ds18b20 not initialized";
                break;
            }

            err = ds18b20_trigger_temperature_conversion(ds_handle);
            if (err == ESP_OK) {
                // 12-bit conversion takes up to 750ms; wait 800ms before reading
                vTaskDelay(pdMS_TO_TICKS(800));

                float temp_c = 0.0f;
                err = ds18b20_get_temperature(ds_handle, &temp_c);
                if (err == ESP_OK) {
                    value = temp_c;
                    raw = (int)(temp_c * 16.0f); // Fixed-point conversion (1/16th degree C)
                } else {
                    err_str = "temp read failed";
                }
            } else {
                err_str = "conversion trigger failed";
            }
            break;
        }

        case SENSOR_HUMIDITY:
            // DHT22 logic here...
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
    switch (type) {
        case SENSOR_TEMP:     return "Temperature";
        case SENSOR_HUMIDITY: return "Humidity";
        case SENSOR_MOISTURE: return "Moisture";
        case SENSOR_PH:       return "pH";
        default:              return "Unknown";
    }
}

sensor_type_t string_to_sensor_type(const char *type) {
    if (strcmp(type, "Temperature") == 0) return SENSOR_TEMP;
    if (strcmp(type, "Humidity") == 0)    return SENSOR_HUMIDITY;
    if (strcmp(type, "Moisture") == 0)    return SENSOR_MOISTURE;
    if (strcmp(type, "pH") == 0)          return SENSOR_PH;

    return SENSOR_NONE;
}