#ifndef DRIVER_NVS_H
#define DRIVER_NVS_H


// add nvs
#include <stdio.h>
#include "nvs.h"
#include "nvs_flash.h"

void driver_nvs_init();
void driver_nvs_open();
void driver_nvs_write_i32(int32_t value_i32, char* key);
void driver_nvs_read_i32(int32_t* read_value, char* key);
void driver_nvs_commit();
void driver_nvs_close();

#endif // DRIVER_NVS_H