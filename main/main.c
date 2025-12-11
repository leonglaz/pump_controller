#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include <esp_http_server.h>
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "driver/gpio.h"
#include <lwip/sockets.h>
#include <lwip/sys.h>
#include <lwip/api.h>
#include <lwip/netdb.h>
#include "driver/uart.h"

static const char *TAG = "ESP32_WebServer";

// Конфигурация WiFi
#define EXAMPLE_ESP_WIFI_SSID "ESP32_UART_Server"
#define EXAMPLE_ESP_WIFI_PASS "12345678"
#define EXAMPLE_ESP_WIFI_CHANNEL 1
#define EXAMPLE_MAX_STA_CONN 5

#define GPIO_LED1 5
#define GPIO_LED2 15
#define GPIO_ERROR1_IN  19
#define GPIO_ERROR2_IN  21
#define GPIO_PLUS_ERRORS 22

int led_state1 = 0;
int led_state2 = 0;

bool led1_enable=false;
bool led2_enable=false;

bool blink_loop_enable=0;
uint16_t blink_time_stop=1000;

bool semafore=true;

SemaphoreHandle_t semaphore_loop;
SemaphoreHandle_t semaphore_one;


bool web_led1=true;
bool web_led2=true;

// HTML шаблоны
char html_template[] = 
"<!DOCTYPE html>"
"<html>"
    "<head>"
    "<meta charset=\"UTF-8\">"
    "<title>ESP32 UART Web Server</title>"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
    "<style>"
        "html {"
        "  font-family: Arial;"
        "  display: inline-block;"
        "  margin: 0px auto;"
        "  text-align: center;"
        "}"
        "h1 {"
        "  color: #070812;"
        "  padding: 2vh;"
        "}"
        ".button {"
        "  background-color: #b30000;"
        "}"
        ".button2 {"
        "  background-color: #364cf4;"
        "}"
        ".button3 {"
        "  background-color:rgb(8, 210, 15);"
        "}"
        ".button4 {"
        "  background-color:rgb(255, 72, 0);"
        "}"
        ".content {"
        "  padding: 50px;"
        "}"
        ".card-grid {"
        "  max-width: 800px;"
        "  margin: 0 auto;"
        "  display: grid;"
        "  grid-gap: 2rem;"
        "  grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));"
        "}"
        ".card {"
        "  background-color: white;"
        "  box-shadow: 2px 2px 12px 1px rgba(140,140,140,.5);"
        "  padding: 20px;"
        "}"
        ".card-title {"
        "  font-size: 1.2rem;"
        "  font-weight: bold;"
        "  color: #034078;"
        "}"
    "</style>"
"</head>"
"<body>"
"  <h1>ESP32 Web Server</h1>"
"  <div class=\"content\">"
"    <div class=\"card-grid\">"
"      <div class=\"card\">"
"        <h2>Состояние LED1: <strong>%s</strong></h2>"           
"           <a href=\"/ledon\"><button class=\"button button3\">ВКЛ</button></a>"
"           <a href=\"/ledoff\"><button class=\"button button4\">ВЫКЛ</button></a>"
"             <p>"
"           <a href=\"/refresh\"><button class=\"button button4\">ОТПРАВИТЬ ТАЙМЕР</button></a>"
"             </p>"
"      </div>"
"      <div class=\"card\">"
"        <h2>Управление LED2</h2>"
"        <p>Состояние LED2: <strong>%s</strong></p>"
"        <p>"
"          <a href=\"/ledon2\"><button class=\"button\">ВКЛ</button></a>"
"          <a href=\"/ledoff2\"><button class=\"button button2\">ВЫКЛ</button></a>"
"        </p>"
"      </div>"
"       <div class=\"card\">"
"       </div>"
"    </div>"
"  </div>"
"</body>"
"</html>";



// Обработчик WiFi событий
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "Подключилось устройство: "MACSTR", AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "Отключилось устройство: "MACSTR", AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}

// Инициализация WiFi в режиме точки доступа
void wifi_init_softap(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
            .channel = EXAMPLE_ESP_WIFI_CHANNEL,
            .password = EXAMPLE_ESP_WIFI_PASS,
            .max_connection = EXAMPLE_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .required = true,
            },
        },
    };

    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Точка доступа запущена. SSID:%s, Канал:%d",
             EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_CHANNEL);
}

// Генерация HTML страницы
esp_err_t send_web_page(httpd_req_t *req)
{
    char html_response[2048];
    const char* led_status1 = (led_state1 == 1) ? "ВКЛ" : "ВЫКЛ";
    const char* led_status2 = (led_state2 == 1) ? "ВКЛ" : "ВЫКЛ";
    // Формируем HTML страницу с текущими данными
    snprintf(html_response, sizeof(html_response), html_template, led_status1, led_status2);
    
    return httpd_resp_send(req, html_response, HTTPD_RESP_USE_STRLEN);
}

// Обработчик главной страницы
esp_err_t get_req_handler(httpd_req_t *req)
{
    return send_web_page(req);
}

// Обработчик обновления страницы
esp_err_t refresh_handler(httpd_req_t *req)
{
    // Просто возвращаем обновленную страницу
    return send_web_page(req);
}

// Обработчик включения LED
esp_err_t led_on_handler(httpd_req_t *req)
{
    web_led1=true;
    led1_enable = true;
    ESP_LOGI(TAG, "LED1 включен");
    return send_web_page(req);
}

// Обработчик выключения LED
esp_err_t led_off_handler(httpd_req_t *req)
{
    web_led1=false;
    led1_enable = false;
    ESP_LOGI(TAG, "LED1 выключен");
    return send_web_page(req);
}

esp_err_t led_on_handler2(httpd_req_t *req)
{
    web_led2=true;
    led1_enable = true;
    ESP_LOGI(TAG, "LED2 включен");
    return send_web_page(req);
}

// Обработчик выключения LED
esp_err_t led_off_handler2(httpd_req_t *req)
{
    web_led2=false;
    led1_enable = false;
    ESP_LOGI(TAG, "LED2 выключен");
    return send_web_page(req);
}

// Настройка URI handlers
httpd_uri_t uri_get = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = get_req_handler,
    .user_ctx = NULL
};

httpd_uri_t uri_refresh = {
    .uri = "/refresh",
    .method = HTTP_GET,
    .handler = refresh_handler,
    .user_ctx = NULL
    
};

httpd_uri_t uri_ledon = {
    .uri = "/ledon",
    .method = HTTP_GET,
    .handler = led_on_handler,
    .user_ctx = NULL
};

httpd_uri_t uri_ledoff = {
    .uri = "/ledoff",
    .method = HTTP_GET,
    .handler = led_off_handler,
    .user_ctx = NULL
};

httpd_uri_t uri_ledon2 = {
    .uri = "/ledon2",
    .method = HTTP_GET,
    .handler = led_on_handler2,
    .user_ctx = NULL
};

httpd_uri_t uri_ledoff2 = {
    .uri = "/ledoff2",
    .method = HTTP_GET,
    .handler = led_off_handler2,
    .user_ctx = NULL
};

// Запуск HTTP сервера
httpd_handle_t setup_server(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &uri_get);
        httpd_register_uri_handler(server, &uri_refresh);
        httpd_register_uri_handler(server, &uri_ledon);
        httpd_register_uri_handler(server, &uri_ledoff);
        httpd_register_uri_handler(server, &uri_ledon2);
        httpd_register_uri_handler(server, &uri_ledoff2);
        ESP_LOGI(TAG, "HTTP сервер запущен");
    } else {
        ESP_LOGE(TAG, "Ошибка запуска HTTP сервера");
    }

    return server;
}


void blink_loop()
{

     while (1)
    {   

            if(semafore)
            {
                
                if(led_state1)
                {
                    gpio_set_level(GPIO_LED1, 1);
                    gpio_set_level(GPIO_LED2, 0);
                }else    
                        {   
                            gpio_set_level(GPIO_LED1, 0);
                            gpio_set_level(GPIO_LED2, 1);
                        }
                led_state1=!led_state1;
            }
        vTaskDelay(blink_time_stop/ portTICK_PERIOD_MS);
    }
    
}

void one_blink()
{

    while (1)
    {   

            if(!semafore)
            {
                if(led1_enable)
                {
                    gpio_set_level(GPIO_LED1, 1);
                    gpio_set_level(GPIO_LED2, 0);
                }else if(led2_enable)
                            {
                                gpio_set_level(GPIO_LED1, 0);
                                gpio_set_level(GPIO_LED2, 1);
                            } else  {
                                        gpio_set_level(GPIO_LED1, 0);
                                        gpio_set_level(GPIO_LED2, 0);
                                    }

            }
        
        

        vTaskDelay(10/ portTICK_PERIOD_MS);
        

                
        
    }
    
}

void check_leds()
{
    while (1)
    {   
        
        if(!gpio_get_level(GPIO_ERROR1_IN))
        {
            
            led1_enable=true;
            //gpio_set_level(GPIO_LED1, 1);
        }
        else 
                {
                    led1_enable=false;
                    //gpio_set_level(GPIO_LED1, 0);
                }

        if(!gpio_get_level(GPIO_ERROR2_IN))
        {
            led2_enable=true;
            //gpio_set_level(GPIO_LED2, 0);
        }
        else    {
                    led2_enable=false;
                    //gpio_set_level(GPIO_LED2, 1);
                }

        
        if(led1_enable&&led2_enable)
        {
            semafore=true;
        
        }
        else 
            {
                semafore=false;
    
            }

/*         if (gpio_get_level(GPIO_ERROR1_IN))
        {
            gpio_set_level(GPIO_LED1,1);
        } else  {
                    gpio_set_level(GPIO_LED1,0);
                }
        
        if (gpio_get_level(GPIO_ERROR2_IN))
        {
            gpio_set_level(GPIO_LED2,1);
        } else  {
                    gpio_set_level(GPIO_LED2,0);
                } */
        

        vTaskDelay(10/ portTICK_PERIOD_MS);
    }
    
}

void app_main()
{
    // Инициализация NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);


    // Инициализация GPIO
    gpio_reset_pin(GPIO_LED1);
    gpio_set_direction(GPIO_LED1, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_LED1, 0);

    gpio_reset_pin(GPIO_LED2);
    gpio_set_direction(GPIO_LED2, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_LED2, 0);

    gpio_reset_pin(GPIO_ERROR1_IN);
    gpio_set_direction(GPIO_ERROR1_IN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(GPIO_ERROR1_IN, GPIO_FLOATING);

    gpio_reset_pin(GPIO_ERROR2_IN);
    gpio_set_direction(GPIO_ERROR2_IN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(GPIO_ERROR2_IN, GPIO_FLOATING);

    gpio_reset_pin(GPIO_PLUS_ERRORS);
    gpio_set_direction(GPIO_PLUS_ERRORS, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_PLUS_ERRORS, 1);


    


    // Инициализация WiFi
    wifi_init_softap();

    
    // Запуск HTTP сервера
    setup_server();

    xTaskCreate(check_leds, "check_leds", 4096, NULL, 7, NULL);

    xTaskCreate(blink_loop, "blink_loop", 4096, NULL, 5, NULL);

    xTaskCreate(one_blink,"one blink",4096, NULL, 5, NULL);
    

    ESP_LOGI(TAG, "Система запущена. Подключитесь к WiFi: %s", EXAMPLE_ESP_WIFI_SSID);
    ESP_LOGI(TAG, "Откройте браузер и перейдите по адресу: http://192.168.4.1");
}