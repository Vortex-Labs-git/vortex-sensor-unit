
#include "sdkconfig.h" 
#include "esp_log.h"
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"


#include "global_fn/global_var.h"
#include "led_indicators.h"
#include "sensor_process.h"



/* =================== PIN CONFIGURATION =================== */
#define RED_LED_PIN         CONFIG_RED_LED_PIN
#define GREEN_LED_PIN       CONFIG_GREEN_LED_PIN


/* =================== HARDWARE OBJECTS =================== */
LedIndicator redLED = { RED_LED_PIN };
LedIndicator greenLED = { GREEN_LED_PIN };



static const char *TAG = "SENSOR_PROCESS";



void init_sensor_unit(void) {
    led_init(&redLED);
    led_init(&greenLED);

    led_on(&redLED);
    led_on(&greenLED);

    vTaskDelay(pdMS_TO_TICKS(2000));

    led_off(&redLED);
    led_off(&greenLED);


    ESP_LOGI(TAG, "Sensor system initialized");
}


