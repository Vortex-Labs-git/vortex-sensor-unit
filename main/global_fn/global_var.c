
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
    .set_wifi  = false
};



/* ======================================================================== */
/* ========================= Sensor Config ================================ */
/* ======================================================================== */

GetSensors UnitSensorConfig = {
    .sensors = {
        {"S02", SENSOR_TEMP},
        {"S03", SENSOR_MOISTURE},
        {"S04", SENSOR_NONE},
        {"S05", SENSOR_NONE},
        {"S06", SENSOR_NONE},
        {"S07", SENSOR_NONE},
    }
};