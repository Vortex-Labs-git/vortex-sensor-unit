#ifndef GLOBAL_VAR_H
#define GLOBAL_VAR_H

#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"





// Define the structure for get_wifi
typedef struct {
    char ssid[32];
    char password[64];
    bool set_wifi;
} GetWifi;




// Declare the global variables
extern GetWifi wifiStaData;


#endif // GLOBAL_VAR_H