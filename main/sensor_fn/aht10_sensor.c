#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "aht10_sensor.h"

#define AHT10_CMD_INIT             0xE1
#define AHT20_CMD_INIT             0xBE
#define AHT10_CMD_TRIGGER_MEASURE  0xAC
#define AHT10_CMD_SOFT_RESET       0xBA

#define AHT10_STATUS_BUSY          0x80
#define AHT10_STATUS_CALIBRATED    0x08

#define AHT10_POWER_ON_DELAY_MS    40
#define AHT10_MEASURE_DELAY_MS     80
#define AHT10_RESET_DELAY_MS       20
#define AHT10_CALIBRATION_RETRIES  3

static const char *TAG = "AHT10_SENSOR";

static i2c_port_t aht10_i2c_port = I2C_NUM_MAX;
static bool aht10_ready = false;

static void aht10_set_error(AHT10Sensor *sensor, const char *message)
{
    if (sensor == NULL) {
        return;
    }

    snprintf(sensor->error_msg, sizeof(sensor->error_msg), "%s", message);
}

static esp_err_t aht10_write_bytes(const uint8_t *data, size_t length)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (cmd == NULL) {
        return ESP_ERR_NO_MEM;
    }

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (AHT10_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, (uint8_t *)data, length, true);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(
        aht10_i2c_port,
        cmd,
        pdMS_TO_TICKS(AHT10_I2C_TIMEOUT_MS)
    );

    i2c_cmd_link_delete(cmd);
    return ret;
}

static esp_err_t aht10_read_bytes(uint8_t *data, size_t length)
{
    if (length == 0) {
        return ESP_OK;
    }

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (cmd == NULL) {
        return ESP_ERR_NO_MEM;
    }

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (AHT10_I2C_ADDRESS << 1) | I2C_MASTER_READ, true);

    if (length > 1) {
        i2c_master_read(cmd, data, length - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, data + length - 1, I2C_MASTER_NACK);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(
        aht10_i2c_port,
        cmd,
        pdMS_TO_TICKS(AHT10_I2C_TIMEOUT_MS)
    );

    i2c_cmd_link_delete(cmd);
    return ret;
}

static esp_err_t aht10_soft_reset(void)
{
    const uint8_t reset_cmd[] = { AHT10_CMD_SOFT_RESET };
    esp_err_t ret = aht10_write_bytes(reset_cmd, sizeof(reset_cmd));
    if (ret == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(AHT10_RESET_DELAY_MS));
    }
    return ret;
}

static esp_err_t aht10_read_status(uint8_t *status)
{
    return aht10_read_bytes(status, sizeof(*status));
}

static esp_err_t aht10_run_calibration_command(uint8_t command, uint8_t *status)
{
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

        ESP_LOGI(TAG, "AHT init command 0x%02X status: 0x%02X", command, *status);
        if ((*status & AHT10_STATUS_CALIBRATED) != 0) {
            return ESP_OK;
        }
    }

    return ESP_ERR_INVALID_STATE;
}

static esp_err_t aht10_calibrate(void)
{
    uint8_t status = 0;

    esp_err_t ret = aht10_read_status(&status);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "AHT power-on status: 0x%02X", status);
        if ((status & AHT10_STATUS_CALIBRATED) != 0) {
            return ESP_OK;
        }
    }

    ret = aht10_run_calibration_command(AHT10_CMD_INIT, &status);
    if (ret == ESP_OK) {
        return ESP_OK;
    }

    ESP_LOGW(TAG, "AHT10 init command failed to calibrate, trying AHT20 command");

    ret = aht10_run_calibration_command(AHT20_CMD_INIT, &status);
    if (ret != ESP_OK) {
        snprintf(
            aht10Sensor.error_msg,
            sizeof(aht10Sensor.error_msg),
            "AHT calibration failed, status 0x%02X",
            status
        );
    }

    return ret;
}

esp_err_t aht10_init(i2c_port_t i2c_port, gpio_num_t sda_pin, gpio_num_t scl_pin)
{
    i2c_config_t config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = sda_pin,
        .scl_io_num = scl_pin,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = AHT10_I2C_MASTER_FREQ_HZ,
        .clk_flags = 0,
    };

    esp_err_t ret = i2c_param_config(i2c_port, &config);
    if (ret != ESP_OK) {
        aht10_set_error(&aht10Sensor, "Failed to configure I2C");
        return ret;
    }

    ret = i2c_driver_install(i2c_port, I2C_MODE_MASTER, 0, 0, 0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        aht10_set_error(&aht10Sensor, "Failed to install I2C driver");
        return ret;
    }

    aht10_i2c_port = i2c_port;
    vTaskDelay(pdMS_TO_TICKS(AHT10_POWER_ON_DELAY_MS));

    ret = aht10_soft_reset();
    if (ret != ESP_OK) {
        aht10_set_error(&aht10Sensor, "AHT10 soft reset failed");
        return ret;
    }

    ret = aht10_calibrate();
    if (ret != ESP_OK) {
        if (aht10Sensor.error_msg[0] == '\0') {
            aht10_set_error(&aht10Sensor, "AHT10 calibration failed");
        }
        return ret;
    }

    aht10_ready = true;
    aht10_set_error(&aht10Sensor, "");
    ESP_LOGI(TAG, "AHT10 initialized on I2C port %d", i2c_port);

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

    ret = aht10_read_bytes(data, sizeof(data));
    if (ret != ESP_OK) {
        return ret;
    }

    if ((data[0] & AHT10_STATUS_BUSY) != 0) {
        return ESP_ERR_TIMEOUT;
    }

    uint32_t raw_humidity =
        ((uint32_t)data[1] << 12) |
        ((uint32_t)data[2] << 4) |
        ((uint32_t)data[3] >> 4);

    uint32_t raw_temperature =
        (((uint32_t)data[3] & 0x0F) << 16) |
        ((uint32_t)data[4] << 8) |
        (uint32_t)data[5];

    *humidity = ((float)raw_humidity * 100.0f) / 1048576.0f;
    *temperature = (((float)raw_temperature * 200.0f) / 1048576.0f) - 50.0f;

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
        aht10_set_error(sensor, esp_err_to_name(ret));
        return ret;
    }

    sensor->temperature = temperature;
    sensor->humidity = humidity;
    aht10_set_error(sensor, "");

    return ESP_OK;
}
