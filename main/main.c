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
#include <time.h>
#include "driver/uart.h"

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

static const char *TAG = "Pump_Conroller";



// Конфигурация WiFi
#define EXAMPLE_ESP_WIFI_SSID "ESP32_UART_Server"
#define EXAMPLE_ESP_WIFI_PASS "12345678"
#define EXAMPLE_ESP_WIFI_CHANNEL 1
#define EXAMPLE_MAX_STA_CONN 4

#define GPIO_LED1 5
#define GPIO_LED2 15
#define GPIO_ERROR1_IN  19
#define GPIO_ERROR2_IN  21
#define GPIO_PLUS_ERRORS 22
#define GPIO_BUTTON_POWER 17

// Глобальные переменные
static int led_state1 = 0;
static int led_state2 = 0;
static bool led1_enable = false;
static bool led2_enable = false;
static uint8_t hours = 20;
bool semafore=true;
bool task_check_power_ready=false;

uint16_t led1_work_time=1;
uint16_t led2_work_time=1;


bool blink_loop_work=false;
// HTML шаблон - минимальный
static const char html_template[] = 
"<!DOCTYPE html><html><head>"
"<meta charset=UTF-8><title>ESP32</title><meta name=viewport content='width=device-width,initial-scale=1'>"
"<style>"
"body{font-family:Arial;text-align:center;margin:20px}"
".btn{padding:10px 20px;margin:5px;border:none;border-radius:4px;color:white;text-decoration:none;display:inline-block}"
".on{background:#0a0}"
".off{background:#a00}"
".led2{background:#00a}"
"</style>"
"</head>"
"<body>"
"<h1>ESP32 Web Server</h1>"
"<div><h2>LED1: <strong>%s</strong></h2>"
"<a href=/ledon class='btn on'>ВКЛ</a>"
"<a href=/ledoff class='btn off'>ВЫКЛ</a></div>"
"<div><h2>LED2: <strong>%s</strong></h2>"
"<a href=/ledon2 class='btn led2'>ВКЛ</a>"
"<a href=/ledoff2 class='btn off'>ВЫКЛ</a></div>"
"<div><h2>Часы: <strong>%d</strong></h2>"
"<form method='POST' action='/'>"
"<input type='number' name='hours' min='1' max='100' value='%d'>"
"<input type='submit' class='btn off' value='Сохранить'>"
"</form></div>"
"</body></html>";

// Обработчик WiFi событий
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        ESP_LOGI(TAG, "Устройство подключено");
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        ESP_LOGI(TAG, "Устройство отключено");
    }
}

// Инициализация WiFi
static void wifi_init_softap(void)
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
            .channel = EXAMPLE_ESP_WIFI_CHANNEL,
            .password = EXAMPLE_ESP_WIFI_PASS,
            .max_connection = EXAMPLE_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA2_PSK
        },
    };

    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi AP запущен: %s", EXAMPLE_ESP_WIFI_SSID);
}

// Обработчик POST запросов - ИСПРАВЛЕННЫЙ
static esp_err_t post_handler(httpd_req_t *req)
{
    
    char buf[64];
    int ret;
    int remaining = req->content_len;
    int total_read = 0;
    
    // Читаем тело POST запроса
    while (remaining > 0 && total_read < sizeof(buf) - 1) {
        ret = httpd_req_recv(req, buf + total_read, 
                            MIN(remaining, sizeof(buf) - total_read - 1));
        if (ret <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            break;
        }
        total_read += ret;
        remaining -= ret;
    }
    buf[total_read] = '\0';
    
    ESP_LOGI(TAG, "Получено POST: %s", buf);
    int new_hours=1;
    // Парсим значение hours из формы application/x-www-form-urlencoded
    char *hours_start = strstr(buf, "hours=");
    if (hours_start) {
        hours_start += 6; // Пропускаем "hours="
        new_hours = atoi(hours_start);
        if (new_hours >= 1) {
            
            /* if(blink_loop_work)
            {
                if (xHandle_blink_loop) vTaskSuspend(xHandle_blink_loop);
                hours = new_hours;
                if (xHandle_blink_loop) vTaskResume(xHandle_blink_loop);
                
            }   else hours = new_hours; */
            hours = new_hours;
            ESP_LOGI(TAG, "Новое время смены насосов: %d", hours);
        }
    }

    
    // Редирект на главную
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

// Обработчик GET запросов (главная страница)
static esp_err_t get_handler(httpd_req_t *req)
{
    char response[1024];
    const char* led1_status = led_state1 ? "ВКЛ" : "ВЫКЛ";
    const char* led2_status = led_state2 ? "ВКЛ" : "ВЫКЛ";
    
    int len = snprintf(response, sizeof(response), html_template, 
                      led1_status, led2_status, hours, hours);
    
    if (len > 0 && len < sizeof(response)) {
        httpd_resp_set_type(req, "text/html");
        return httpd_resp_send(req, response, len);
    }
    
    return ESP_FAIL;
}

// Функция редиректа
static esp_err_t redirect_to_home(httpd_req_t *req)
{
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

// Обработчики LED
static esp_err_t led_on_handler(httpd_req_t *req)
{
    led_state1 = 1;
    led1_enable = true;
    return redirect_to_home(req);
}

static esp_err_t led_off_handler(httpd_req_t *req)
{
    led_state1 = 0;
    led1_enable = false;
    return redirect_to_home(req);
}

static esp_err_t led_on_handler2(httpd_req_t *req)
{
    led_state2 = 1;
    led2_enable = true;
    return redirect_to_home(req);
}

static esp_err_t led_off_handler2(httpd_req_t *req)
{
    led_state2 = 0;
    led2_enable = false;
    return redirect_to_home(req);
}

// URI handlers
static const httpd_uri_t root = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = get_handler,
    .user_ctx = NULL
};

static const httpd_uri_t post = {
    .uri = "/",
    .method = HTTP_POST,
    .handler = post_handler,
    .user_ctx = NULL
};

static const httpd_uri_t ledon = {
    .uri = "/ledon",
    .method = HTTP_GET,
    .handler = led_on_handler,
    .user_ctx = NULL
};

static const httpd_uri_t ledoff = {
    .uri = "/ledoff",
    .method = HTTP_GET,
    .handler = led_off_handler,
    .user_ctx = NULL
};

static const httpd_uri_t ledon2 = {
    .uri = "/ledon2",
    .method = HTTP_GET,
    .handler = led_on_handler2,
    .user_ctx = NULL
};

static const httpd_uri_t ledoff2 = {
    .uri = "/ledoff2",
    .method = HTTP_GET,
    .handler = led_off_handler2,
    .user_ctx = NULL
};

// Инициализация HTTP сервера
static void start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192;
    config.task_priority = tskIDLE_PRIORITY + 3;
    config.max_uri_handlers = 8;
    config.lru_purge_enable = true;
    config.recv_wait_timeout = 10;
    config.send_wait_timeout = 10;
    
    ESP_LOGI(TAG, "Запуск HTTP сервера на порту: %d", config.server_port);
    
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK) {
        // Регистрация обработчиков
        httpd_register_uri_handler(server, &root);
        httpd_register_uri_handler(server, &post);
        httpd_register_uri_handler(server, &ledon);
        httpd_register_uri_handler(server, &ledoff);
        httpd_register_uri_handler(server, &ledon2);
        httpd_register_uri_handler(server, &ledoff2);
        
        ESP_LOGI(TAG, "HTTP сервер успешно запущен");
    } else {
        ESP_LOGE(TAG, "Ошибка запуска HTTP сервера");
    }
}


bool led1_work;
bool led2_work;

void blink_loop()
{
    TickType_t prevWakeup = 0;
    uint16_t i=0;
    uint16_t work_time=0;

/*     task_check_power_ready=true;
    semafore=true;
    led_state1=true; */
    
    while (1)
    {   
        if(task_check_power_ready)
        {
            if(semafore)
            {
                
                if(led_state1)
                {
                    gpio_set_level(GPIO_LED1, 1);
                    gpio_set_level(GPIO_LED2, 0);
                    ESP_LOGI(TAG, "LED1 горит %d, time %d", i,  led1_work_time);
                    led1_work=true;
                    led1_work=true;
                    led2_work=false;
                }else    
                        {   
                            gpio_set_level(GPIO_LED1, 0);
                            gpio_set_level(GPIO_LED2, 1);
                            ESP_LOGI(TAG, "LED2 горит %d, time %d", i,  led2_work_time);
                            led1_work=false;
                            led2_work=true;
                        }
                
                if(led1_work)
                work_time=led1_work_time;
                if (led2_work)
                work_time=led2_work_time;

                if  (work_time>=(hours))            
                {
                    led_state1=!led_state1;
                    i=0;
                }
                i++;
                vTaskDelayUntil ( &prevWakeup, pdMS_TO_TICKS ( 1000 )) ;
            }
        }
        vTaskDelay(10/ portTICK_PERIOD_MS);
    }
    
}
bool led1_reserve;
bool led2_reserve;
bool leds_crush;

void one_blink()
{
    led1_reserve=true;
    led2_reserve=true;
    leds_crush=true;

    while (1)
    {   
        if(task_check_power_ready)
        {
            if(!semafore)
            {
            
                if(led1_enable)
                {
                    if(led1_reserve)
                    {
                        gpio_set_level(GPIO_LED1, 1);
                        gpio_set_level(GPIO_LED2, 0);
                        ESP_LOGI(TAG, "LED1 горит резерв");
                        led1_reserve=false;
                        led2_reserve=true;
                        leds_crush=true;
                        
                        led1_work=true;
                        led2_work=true;
                    }
                    
                }else if(led2_enable)
                            {
                                if(led2_reserve)
                                {
                                    gpio_set_level(GPIO_LED1, 0);
                                    gpio_set_level(GPIO_LED2, 1);
                                    ESP_LOGI(TAG, "LED2 горит резерв");
                                    led1_reserve=true;
                                    led2_reserve=false;
                                    leds_crush=true;
                                    led1_work=false;
                                    led2_work=true;
                                }
                            } else  {
                                        if(leds_crush)
                                        {
                                            gpio_set_level(GPIO_LED1, 0);
                                            gpio_set_level(GPIO_LED2, 0);
                                            ESP_LOGI(TAG, "сломаны");
                                            led1_reserve=true;
                                            led2_reserve=true;
                                            leds_crush=false;

                                            led1_work=false;
                                            led2_work=false;
                                        }
                                    }
            }
        //ESP_LOGI(TAG, "Задача blink one работает");
        }
        vTaskDelay(10/ portTICK_PERIOD_MS);
    }
    
}

bool normal_work;

void check_leds()
{
    normal_work=true;
    
    while (1)
    {   
        if(task_check_power_ready)
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
                if(normal_work)
                {
                    ESP_LOGI(TAG, "Разрешена смена светодиодов");
                    semafore=true;
                    normal_work=false;
                }
                    
                
                
            }
            else 
                {   
                    if(!normal_work)
                    {
                        
                        ESP_LOGE(TAG, "Запрещена смена светодиодов");
                        semafore=false;
                        normal_work=true;
                    }
                }
            //ESP_LOGI(TAG, "Задача check leds работает");
        } 
        

        

        vTaskDelay(10/ portTICK_PERIOD_MS);
    }
    
}

static bool tasks_suspended = true;

void check_power(void *pvParameters)
{
    

    gpio_set_level(GPIO_LED1, 0);
    gpio_set_level(GPIO_LED2, 0);

    tasks_suspended = true;
    
    while (1)
    {   

        if(gpio_get_level(GPIO_BUTTON_POWER))
        {
            if(tasks_suspended)
            {
             
                task_check_power_ready=true;
                ESP_LOGI(TAG, "Задачи возобновлены");
                tasks_suspended=false;
                
            }
                
            
        }
        else 
        {
            if(!tasks_suspended)
            {
                task_check_power_ready=false;
                gpio_set_level(GPIO_LED1, 0);
                gpio_set_level(GPIO_LED2, 0);
                ESP_LOGE(TAG, "Задачи приостановлены");
                blink_loop_work=false;
                tasks_suspended=true;
            }
        }
        //ESP_LOGI(TAG, "Задача check power работает");
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void check_time(void *pvParameters)
{
    TickType_t wake_check_time = 0;
    while (1)
    {  
        if(task_check_power_ready)
        {
            if(led1_work)
            {
                led1_work_time++;
                if(led1_work_time>hours)
                led1_work_time=0;
            }      
            if (led2_work)
            {
                led2_work_time++;
                if(led2_work_time>hours)
                led2_work_time=0;
            }
        }
        vTaskDelayUntil ( &wake_check_time, pdMS_TO_TICKS (1000 )) ;
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

    gpio_reset_pin(GPIO_BUTTON_POWER);
    gpio_set_direction(GPIO_BUTTON_POWER, GPIO_MODE_INPUT);
    gpio_set_pull_mode(GPIO_BUTTON_POWER, GPIO_FLOATING);


    


    // Инициализация WiFi
    wifi_init_softap();

    
    // Запуск HTTP сервера
    start_webserver();

    ESP_LOGI(TAG, "Система запущена. Подключитесь к WiFi: %s", EXAMPLE_ESP_WIFI_SSID);
    ESP_LOGI(TAG, "Откройте браузер и перейдите по адресу: http://192.168.4.1");

    vTaskDelay(100 / portTICK_PERIOD_MS);

    xTaskCreate(check_leds, "check_leds", 4096, NULL, 5, NULL);

    xTaskCreate(check_time, "check_time", 4096, NULL, 5, NULL);

    xTaskCreate(blink_loop, "blink_loop", 4096, NULL, 5, NULL);

    

    xTaskCreate(one_blink,"one blink",4096, NULL, 5, NULL);

    xTaskCreate(check_power, "check_power", 4096, NULL, 7, NULL);

    

} 

