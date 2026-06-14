#ifndef SENSOR_CONFIG_H
#define SENSOR_CONFIG_H


esp_err_t sensor_config_load(void);
esp_err_t sensor_config_save(void);
void sensor_config_restore_default(void);

#endif /* SENSOR_CONFIG_H */
