#ifndef DRIVER_NVS_H
#define DRIVER_NVS_H


// add nvs
#include <stdio.h>
#include "nvs.h"
#include "nvs_flash.h"

void driver_nvs_init();
void driver_nvs_open();
void driver_nvs_write_u32(uint32_t value_u32, char* key);
void driver_nvs_read_u32(uint32_t* read_value, char* key);
void driver_nvs_commit();
void driver_nvs_close();
void driver_nvs_write_u8(uint8_t value_u8, char* key);
void driver_nvs_read_u8(uint8_t* read_value, char* key);

#endif // DRIVER_NVS_H