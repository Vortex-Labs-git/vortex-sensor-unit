
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
