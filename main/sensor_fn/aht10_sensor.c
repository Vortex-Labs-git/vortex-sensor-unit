#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"

#include "global_fn/global_var.h"
#include "aht10_sensor.h"



#define AHT10_CMD_INIT             0xE1
#define AHT20_CMD_INIT             0xBE
#define AHT10_CMD_TRIGGER_MEASURE  0xAC
#define AHT10_CMD_SOFT_RESET       0xBA

#define AHT10_STATUS_BUSY          0x80
#define AHT10_STATUS_CALIBRATED    0x08
#define AHT10_STATUS_INVALID       0xFF

#define AHT10_POWER_ON_DELAY_MS    40
#define AHT10_MEASURE_DELAY_MS     80
#define AHT10_RESET_DELAY_MS       20
#define AHT10_CALIBRATION_RETRIES  3
#define AHT10_MEASURE_RETRIES      4
#define AHT10_MEASURE_POLL_MS      20

#define AHT10_I2C_ADDRESS          0x38
#define AHT10_I2C_ADDRESS_ALT      0x39
#define AHT10_I2C_MASTER_FREQ_HZ   100000
#define AHT10_I2C_TIMEOUT_MS       1000

/* Both readings are 20 bit, so 0xFFFFF is the all-ones pattern and not a
 * value a working sensor reports. Humidity 0xFFFFF is what an uncalibrated
 * AHT10 returns, and it converts to a permanent 100.00 %. */
#define AHT10_RAW_FULL_SCALE       1048576.0f
#define AHT10_RAW_ALL_ONES         0xFFFFF


static const char *TAG_AHT10 = "AHT10_SENSOR";

static i2c_port_t aht10_i2c_port = I2C_NUM_MAX;
static uint8_t aht10_i2c_address = AHT10_I2C_ADDRESS;
static bool aht10_ready = false;



static esp_err_t aht10_write_bytes(const uint8_t *data, size_t length)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (cmd == NULL) {
        return ESP_ERR_NO_MEM;
    }

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (aht10_i2c_address << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, (uint8_t *)data, length, true);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin( aht10_i2c_port, cmd, pdMS_TO_TICKS(AHT10_I2C_TIMEOUT_MS));

    i2c_cmd_link_delete(cmd);
    return ret;
}

static esp_err_t aht10_read_bytes(uint8_t *data, size_t length) {
    if (length == 0) {
        return ESP_OK;
    }

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (cmd == NULL) {
        return ESP_ERR_NO_MEM;
    }

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (aht10_i2c_address << 1) | I2C_MASTER_READ, true);

    if (length > 1) {
        i2c_master_read(cmd, data, length - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, data + length - 1, I2C_MASTER_NACK);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin( aht10_i2c_port, cmd, pdMS_TO_TICKS(AHT10_I2C_TIMEOUT_MS));

    i2c_cmd_link_delete(cmd);
    return ret;
}

static esp_err_t aht10_soft_reset(void) {
    const uint8_t reset_cmd[] = { AHT10_CMD_SOFT_RESET };
    esp_err_t ret = aht10_write_bytes(reset_cmd, sizeof(reset_cmd));
    if (ret == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(AHT10_RESET_DELAY_MS));
    }
    return ret;
}

static esp_err_t aht10_read_status(uint8_t *status) {
    return aht10_read_bytes(status, sizeof(*status));
}

/* A status byte of 0xFF is what an idle / pulled-up bus reads back when the
 * sensor does not drive SDA. It has the calibrated bit set, so it must be
 * rejected explicitly or a dead sensor looks calibrated. */
static bool aht10_status_is_calibrated(uint8_t status) {
    return (status != AHT10_STATUS_INVALID) && ((status & AHT10_STATUS_CALIBRATED) != 0);
}

static esp_err_t aht10_run_calibration_command(uint8_t command, uint8_t *status) {
    const uint8_t init_cmd[] = { command, 0x08, 0x00 };

    for (int retry = 0; retry < AHT10_CALIBRATION_RETRIES; retry++) {
        esp_err_t ret = aht10_write_bytes(init_cmd, sizeof(init_cmd));
        if (ret != ESP_OK) {
            return ret;
        }

        vTaskDelay(pdMS_TO_TICKS(AHT10_POWER_ON_DELAY_MS));

        ret = aht10_read_status(status);
        if (ret != ESP_OK) {
            return ret;
        }

        ESP_LOGI(TAG_AHT10, "AHT init command 0x%02X status: 0x%02X", command, *status);
        if (aht10_status_is_calibrated(*status)) {
            return ESP_OK;
        }
    }

    return ESP_ERR_INVALID_STATE;
}

static esp_err_t aht10_sensor_calibrate(void) {
    uint8_t status = 0;

    esp_err_t ret = aht10_read_status(&status);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG_AHT10, "AHT power-on status: 0x%02X", status);
    }

    /* The power-on status is only a hint, never a reason to skip the init
     * command: the soft reset above drops the factory calibration, and a
     * silent bus reads back 0xFF with the calibrated bit set. Skipping the
     * init here is what leaves the sensor reporting a raw 0xFFFFF humidity,
     * i.e. a constant 100 %. */
    ret = aht10_run_calibration_command(AHT10_CMD_INIT, &status);
    if (ret == ESP_OK) {
        return ESP_OK;
    }

    ESP_LOGW(TAG_AHT10, "AHT10 init command failed to calibrate, trying AHT20 command");

    ret = aht10_run_calibration_command(AHT20_CMD_INIT, &status);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG_AHT10,"AHT calibration failed, status 0x%02X", status);
    }

    return ret;
}

esp_err_t aht10_sensor_init(AHT10 *aht10) {

    if (aht10 == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (aht10->I2C_add == AHT10_I2C_ADDRESS || aht10->I2C_add == AHT10_I2C_ADDRESS_ALT) {
        aht10_i2c_address = aht10->I2C_add;
    } else {
        ESP_LOGW(TAG_AHT10, "Configured I2C address 0x%02X is not an AHT10 address, using 0x%02X",
                 aht10->I2C_add, AHT10_I2C_ADDRESS);
        aht10_i2c_address = AHT10_I2C_ADDRESS;
    }

    i2c_config_t config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = aht10->SDA_pin,
        .scl_io_num = aht10->SCL_pin,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = AHT10_I2C_MASTER_FREQ_HZ,
        .clk_flags = 0,
    };

    esp_err_t ret = i2c_param_config(aht10->I2C_port, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG_AHT10, "Failed to configure I2C");
        return ret;
    }

    ret = i2c_driver_install(aht10->I2C_port, I2C_MODE_MASTER, 0, 0, 0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG_AHT10, "Failed to install I2C driver");
        return ret;
    }

    aht10_i2c_port = aht10->I2C_port;
    vTaskDelay(pdMS_TO_TICKS(AHT10_POWER_ON_DELAY_MS));

    ret = aht10_soft_reset();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG_AHT10, "AHT10 soft reset failed");
        return ret;
    }

    ret = aht10_sensor_calibrate();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG_AHT10, "AHT10 calibration failed");
        return ret;
    }

    aht10_ready = true;
    ESP_LOGI(TAG_AHT10, "AHT10 initialized on I2C port %d, address 0x%02X",
             aht10->I2C_port, aht10_i2c_address);

    return ESP_OK;
}






esp_err_t aht10_read(float *temperature, float *humidity)
{
    if (temperature == NULL || humidity == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!aht10_ready || aht10_i2c_port == I2C_NUM_MAX) {
        return ESP_ERR_INVALID_STATE;
    }

    const uint8_t measure_cmd[] = { AHT10_CMD_TRIGGER_MEASURE, 0x33, 0x00 };
    uint8_t data[6] = { 0 };

    esp_err_t ret = aht10_write_bytes(measure_cmd, sizeof(measure_cmd));
    if (ret != ESP_OK) {
        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(AHT10_MEASURE_DELAY_MS));

    /* The conversion is not always done after the fixed delay, so give the
     * sensor a few more polls instead of failing the whole cycle. */
    for (int retry = 0; ; retry++) {
        ret = aht10_read_bytes(data, sizeof(data));
        if (ret != ESP_OK) {
            return ret;
        }

        if ((data[0] & AHT10_STATUS_BUSY) == 0) {
            break;
        }

        if (retry >= AHT10_MEASURE_RETRIES) {
            ESP_LOGW(TAG_AHT10, "AHT10 still busy, status 0x%02X", data[0]);
            return ESP_ERR_TIMEOUT;
        }

        vTaskDelay(pdMS_TO_TICKS(AHT10_MEASURE_POLL_MS));
    }

    /* A sensor that browned out or was reset behind our back comes back
     * uncalibrated and then streams a raw 0xFFFFF humidity forever. Reload
     * the factory calibration instead of publishing that as 100 %. */
    if (!aht10_status_is_calibrated(data[0])) {
        ESP_LOGW(TAG_AHT10, "AHT10 lost calibration, status 0x%02X, re-running init", data[0]);
        aht10_sensor_calibrate();
        return ESP_ERR_INVALID_STATE;
    }

    uint32_t raw_humidity =
        ((uint32_t)data[1] << 12) |
        ((uint32_t)data[2] << 4) |
        ((uint32_t)data[3] >> 4);

    uint32_t raw_temperature =
        (((uint32_t)data[3] & 0x0F) << 16) |
        ((uint32_t)data[4] << 8) |
        (uint32_t)data[5];

    if (raw_humidity == AHT10_RAW_ALL_ONES || (raw_humidity == 0 && raw_temperature == 0)) {
        ESP_LOGW(TAG_AHT10, "AHT10 returned no usable sample, raw humidity 0x%05lX, raw temperature 0x%05lX",
                 (unsigned long)raw_humidity, (unsigned long)raw_temperature);
        return ESP_ERR_INVALID_RESPONSE;
    }

    *humidity = ((float)raw_humidity * 100.0f) / AHT10_RAW_FULL_SCALE;
    *temperature = (((float)raw_temperature * 200.0f) / AHT10_RAW_FULL_SCALE) - 50.0f;

    return ESP_OK;
}

esp_err_t aht10_read_sensor(AHT10Sensor *sensor)
{
    if (sensor == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    float temperature = 0.0f;
    float humidity = 0.0f;

    esp_err_t ret = aht10_read(&temperature, &humidity);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG_AHT10, "AHT10 read fail %s", esp_err_to_name(ret));
    }

    xSemaphoreTake(InbuildsensorMutex, portMAX_DELAY);
    if (ret == ESP_OK) {
        sensor->temperature = temperature;
        sensor->humidity = humidity;
        sensor->error_msg[0] = '\0';
    } else {
        strlcpy(sensor->error_msg, esp_err_to_name(ret), sizeof(sensor->error_msg));
    }
    xSemaphoreGive(InbuildsensorMutex);

    return ret;
}
