#ifndef DRIVER_WIFI_H
#define DRIVER_WIFI_H

#include <stdio.h>
#include "esp_system.h"
#include <esp_http_server.h> 
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "../../main/struct_pump.h"

void wifi_init_softap(void);
void start_webserver(void);
void wifi_set_struct_pump(struct_pump_t* ptr_struct_t);
#endif // DRIVER_WIFI_H