#ifndef MQTT_STATE_FN_H
#define MQTT_STATE_FN_H

#include "mqtt_client_fn.h"


cJSON* create_sensorunit_status();
cJSON* create_sensorunit_state_data();
cJSON* create_sensorunit_error();

#endif