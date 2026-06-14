
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif_net_stack.h"
#include "esp_netif.h"
#include "nvs_flash.h"


#include "eeprom_fn/wifi_storage.h"
#include "eeprom_fn/sensor_config.h"
#include "wifi_fn/vortex_wifi.h"
#include "sensor_fn/external_sensor.h"
#include "sensor_fn/sensor_process.h" 




/* ========================== GLOBAL VARIABLES ========================== */

/**
 * @brief Mutex to protect valve-related operations
 */
SemaphoreHandle_t valveMutex = NULL;
/**
 * @brief Mutex to protect web server operations
 */
SemaphoreHandle_t serverMutex = NULL;





/* ======================================================================== */
/* ================================ MAIN ================================== */
/* ======================================================================== */

void app_main(void)
{

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // NVS Init
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);


#if CONFIG_ESP_WIFI_STA_MODE_RESET
    wifi_storage_restore_default();
#endif

#if CONFIG_SENSOR_CONFIG_RESET
    wifi_storage_restore_default();
#endif

    wifi_storage_load();
    sensor_config_load();

    // serverMutex = xSemaphoreCreateMutex();
    // if (serverMutex == NULL) {
    //     ESP_LOGE(TAG_MAIN, "Failed to create serverMutex");
    //     return;
    // }

    // load_eeprom_calibration();

    // time_module_init();
    
    init_sensor_unit();

    // wifi_init_smart_mode();


    xTaskCreate( external_sensor_task, "external_sensor_task", 4096, NULL, 5, NULL);

}