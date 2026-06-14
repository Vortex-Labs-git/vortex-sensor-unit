#include <stdint.h>
#include "sdkconfig.h" 
#include "esp_log.h"
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"


#include "global_fn/global_var.h"
#include "led_indicators.h"
#include "external_sensor.h"
#include "sensor_process.h"



/* =================== PIN CONFIGURATION =================== */
#define RED_LED_PIN         CONFIG_RED_LED_PIN
#define GREEN_LED_PIN       CONFIG_GREEN_LED_PIN

#define SENSOR_SCL_PIN      CONFIG_SENSOR_SCL_PIN
#define SENSOR_SDA_PIN      CONFIG_SENSOR_SDA_PIN

#define SENSOR_02_Pin_A0    CONFIG_SENSOR_02_Pin_A0
#define SENSOR_02_Pin_D0    CONFIG_SENSOR_02_Pin_D0

#define SENSOR_03_Pin_A0    CONFIG_SENSOR_03_Pin_A0
#define SENSOR_03_Pin_D0    CONFIG_SENSOR_03_Pin_D0

#define SENSOR_04_Pin_A0    CONFIG_SENSOR_04_Pin_A0
#define SENSOR_04_Pin_D0    CONFIG_SENSOR_04_Pin_D0

#define SENSOR_05_Pin_A0    CONFIG_SENSOR_05_Pin_A0
#define SENSOR_05_Pin_D0    CONFIG_SENSOR_05_Pin_D0

#define SENSOR_06_Pin_A0    CONFIG_SENSOR_06_Pin_A0
#define SENSOR_06_Pin_D0    CONFIG_SENSOR_06_Pin_D0

#define SENSOR_07_Pin_A0    CONFIG_SENSOR_07_Pin_A0
#define SENSOR_07_Pin_D0    CONFIG_SENSOR_07_Pin_D0

/* =================== HARDWARE OBJECTS =================== */
LedIndicator redLED = { RED_LED_PIN };
LedIndicator greenLED = { GREEN_LED_PIN };

ExternalSensorlist externalSensorList = {
    .externalSensor = {
        { "S02", SENSOR_02_Pin_A0, ADC1_CHANNEL_MAX, SENSOR_02_Pin_D0 },
        { "S03", SENSOR_03_Pin_A0, ADC1_CHANNEL_MAX, SENSOR_03_Pin_D0 },
        { "S04", SENSOR_04_Pin_A0, ADC1_CHANNEL_MAX, SENSOR_04_Pin_D0 },
        { "S05", SENSOR_05_Pin_A0, ADC1_CHANNEL_MAX, SENSOR_05_Pin_D0 },
        { "S06", SENSOR_06_Pin_A0, ADC1_CHANNEL_MAX, SENSOR_06_Pin_D0 },
        { "S07", SENSOR_07_Pin_A0, ADC1_CHANNEL_MAX, SENSOR_07_Pin_D0 }
    }
};


static const char *TAG = "SENSOR_PROCESS";


void init_sensor_unit(void) {
    led_init(&redLED);
    led_init(&greenLED);

    led_on(&redLED);
    led_on(&greenLED);

    vTaskDelay(pdMS_TO_TICKS(2000));

    led_off(&redLED);
    led_off(&greenLED);

    external_sensors_init(&externalSensorList);

    ESP_LOGI(TAG, "Sensor system initialized");
}


