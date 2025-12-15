#include "driver_nvs.h"
#include <inttypes.h>
#include "esp_log.h"
//add nvs
esp_err_t err;
nvs_handle_t my_handle;
static const char *TAG = "driver_nvs";

void driver_nvs_init()
{
    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

void driver_nvs_open()
{
    ESP_LOGI(TAG, "\nOpening Non-Volatile Storage (NVS) handle...");

    err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
        return;
    }
}

void driver_nvs_write_i32(int32_t value_i32, char* key)
{   
    driver_nvs_open();
    ESP_LOGI(TAG, "\nWriting counter to NVS...");
    err = nvs_set_i32(my_handle, key, value_i32);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write counter!");
    }
    driver_nvs_commit();
    driver_nvs_close();
}

void driver_nvs_read_i32(int32_t* read_value, char* key)
{
    driver_nvs_open();
    ESP_LOGI(TAG, "\nReading counter from NVS...");
    err = nvs_get_i32(my_handle, key, read_value);
    switch (err) {
        case ESP_OK:
            ESP_LOGI(TAG, "Read counter = %" PRIu8, *read_value);
            break;
        case ESP_ERR_NVS_NOT_FOUND:
            ESP_LOGW(TAG, "The value is not initialized yet!");
            break;
        default:
            ESP_LOGE(TAG, "Error (%s) reading!", esp_err_to_name(err));
    }
    driver_nvs_close();
}

void driver_nvs_commit()
{
    ESP_LOGI(TAG, "\nCommitting updates in NVS...");
    err = nvs_commit(my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS changes!");
    }

}
void driver_nvs_close()
{
    nvs_close(my_handle);
    ESP_LOGI(TAG, "NVS handle closed.");
}



   