
#include "sdkconfig.h" 
#include "esp_log.h"
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"


#include "global_fn/global_var.h"
#include "aht10_sensor.h"
#include "led_indicators.h"
#include "sensor_process.h"



/* =================== PIN CONFIGURATION =================== */
#define RED_LED_PIN         CONFIG_RED_LED_PIN
#define GREEN_LED_PIN       CONFIG_GREEN_LED_PIN
#define AHT10_I2C_PORT      CONFIG_AHT10_I2C_PORT
#define AHT10_SDA_PIN       CONFIG_AHT10_SDA_PIN
#define AHT10_SCL_PIN       CONFIG_AHT10_SCL_PIN
#define AHT10_READ_MS       CONFIG_AHT10_READ_INTERVAL_MS


/* =================== HARDWARE OBJECTS =================== */
LedIndicator redLED = { RED_LED_PIN };
LedIndicator greenLED = { GREEN_LED_PIN };



static const char *TAG = "SENSOR_PROCESS";

static void aht10_sensor_task(void *arg)
{
    while (1) {
        if (aht10_read_sensor(&aht10Sensor) == ESP_OK) {
            ESP_LOGI(
                TAG,
                "AHT10 temperature: %.2fC˚, humidity: %.2f %%",
                aht10Sensor.temperature,
                aht10Sensor.humidity
            );
        } else {
            ESP_LOGE(TAG, "AHT10 read failed: %s", aht10Sensor.error_msg);
        }

        vTaskDelay(pdMS_TO_TICKS(AHT10_READ_MS));
    }
}



void init_sensor_unit(void) {
    led_init(&redLED);
    led_init(&greenLED);

    led_on(&redLED);
    led_on(&greenLED);

    vTaskDelay(pdMS_TO_TICKS(2000));

    led_off(&redLED);
    led_off(&greenLED);

    if (aht10_init(
        (i2c_port_t)AHT10_I2C_PORT,
        (gpio_num_t)AHT10_SDA_PIN,
        (gpio_num_t)AHT10_SCL_PIN
        ) == ESP_OK) {
        ESP_LOGI(
            TAG,
            "AHT10 initialized on I2C port %d, SDA GPIO%d, SCL GPIO%d",
            AHT10_I2C_PORT,
            AHT10_SDA_PIN,
            AHT10_SCL_PIN
        );
        xTaskCreate(aht10_sensor_task, "aht10_sensor_task", 3072, NULL, 5, NULL);
    } else {
        ESP_LOGE(TAG, "AHT10 init failed: %s", aht10Sensor.error_msg);
    }

    ESP_LOGI(TAG, "Sensor system initialized");
}
