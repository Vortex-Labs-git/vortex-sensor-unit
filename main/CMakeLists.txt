idf_component_register(SRCS 
                        "main.c"
                        "wifi_fn/vortex_wifi.c"
                        "global_fn/global_var.c"
                        "eeprom_fn/wifi_storage.c"
                        "time_fn/time_func.c"
                        "sensor_fn/led_indicators.c"
                        "sensor_fn/sensor_process.c"
                    INCLUDE_DIRS ".")
