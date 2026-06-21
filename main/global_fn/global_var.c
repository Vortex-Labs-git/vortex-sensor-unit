
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "global_fn/global_var.h"





/* ======================================================================== */
/* ============================= WIFI DATA ================================ */
/* ======================================================================== */

/**
 * @brief Stored WiFi Station credentials
 *
 * Used by WiFi STA initialization.
 * Typically loaded from NVS during boot.
 */
GetWifi wifiStaData = {
    .ssid       = "",
    .password   = "",
    .set_wifi  = false,
};



/* ======================================================================== */
/* ========================= Sensor Config ================================ */
/* ======================================================================== */

GetSensors UnitSensorConfig = {
    .sensors = {
        {"S02", SENSOR_NONE, ""},
        {"S03", SENSOR_NONE, ""},
        {"S04", SENSOR_NONE, ""},
        {"S05", SENSOR_NONE, ""},
        {"S06", SENSOR_NONE, ""},
        {"S07", SENSOR_NONE, ""},
    }
};


AHT10Sensor aht10Sensor = {
    .temperature = 0.0,
    .humidity = 0.0,
    .error_msg = "",
};

SensorMap sensorMap = {
    .sensorS = {
        {
            .available = false,
            .sensor_id = "S02",
            .type = SENSOR_NONE,
            .sensor_name = "",
            .data = { .raw = 0, .value = 0.0f },
            .error_msg = ""
        },
        {
            .available = false,
            .sensor_id = "S03",
            .type = SENSOR_NONE,
            .sensor_name = "",
            .data = { .raw = 0, .value = 0.0f },
            .error_msg = ""
        },
        {
            .available = false,
            .sensor_id = "S04",
            .type = SENSOR_NONE,
            .sensor_name = "",
            .data = { .raw = 0, .value = 0.0f },
            .error_msg = ""
        },
        {
            .available = false,
            .sensor_id = "S05",
            .type = SENSOR_NONE,
            .sensor_name = "",
            .data = { .raw = 0, .value = 0.0f },
            .error_msg = ""
        },
        {
            .available = false,
            .sensor_id = "S06",
            .type = SENSOR_NONE,
            .sensor_name = "",
            .data = { .raw = 0, .value = 0.0f },
            .error_msg = ""
        },
        {
            .available = false,
            .sensor_id = "S07",
            .type = SENSOR_NONE,
            .sensor_name = "",
            .data = { .raw = 0, .value = 0.0f },
            .error_msg = ""
        }
    }
};