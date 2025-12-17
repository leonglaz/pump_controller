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
#include "driver_nvs.h"
#include "freertos/timers.h"

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

static const char *TAG = "Pump_Conroller";

uint32_t pump1_work_minutes=0;
uint32_t pump2_work_minutes=0;

// Конфигурация WiFi
#define EXAMPLE_ESP_WIFI_SSID "ESP32_UART_Server"
#define EXAMPLE_ESP_WIFI_PASS "12345678"
#define EXAMPLE_ESP_WIFI_CHANNEL 1
#define EXAMPLE_MAX_STA_CONN 4




#define GPIO_LED11 14
#define GPIO_LED12 27
#define GPIO_LED21 15
#define GPIO_LED22 22

#define GPIO_OVERHEAT1  35
#define GPIO_OVERHEAT2  34
#define GPIO_VOLTAGE 39
#define GPIO_BUTTON_POWER 36

#define GPIO_RELAY_CRUSH 13
#define GPIO_RELAY_1 12
#define GPIO_RELAY_2 2

#define GPIO_PHASE_11 26
#define GPIO_PHASE_12 25
#define GPIO_PHASE_13 33

#define GPIO_PHASE_21 18
#define GPIO_PHASE_22 19
#define GPIO_PHASE_23 21

// Глобальные переменные
static uint8_t led_state1 = 0;
static int led_state2 = 0;
static bool led1_enable = false;
static bool led2_enable = false;
static uint32_t hours = 60*1;
static uint32_t period_save_nvs=60*0.5;

uint32_t timer1_acidification=20;
uint32_t timer2_acidification=20;
bool semafore=true;
bool task_check_power_ready=false;


bool phase_pump1=false;
bool phase_pump2=false;
bool voltage_work=true;

uint32_t led1_work_time=0;
uint32_t led2_work_time=0;
bool blink_loop_work=false;
bool do_change=false;

TimerHandle_t _timer1 = NULL;
TimerHandle_t _timer2 = NULL;
bool acidification=false;

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
"<div><h2>Часы: <strong>%ld</strong></h2>"
"<form method='POST' action='/'>"
"<input type='number' name='hours' min='1' max='100' value='%ld'>"
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

    char *hours_start = strstr(buf, "hours=");
    if (hours_start) {
        hours_start += 6; // Пропускаем "hours="
        new_hours = atoi(hours_start);
        if (new_hours >= 1) {
            
            
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


bool led1_work=false;
bool led2_work=false;

void blink_loop()
{
    TickType_t prevWakeup = 0;
    uint16_t i=0;
    uint16_t work_time=0;


    
    while (1)
    {   
        if(task_check_power_ready)
        {
           
             if(semafore)
            {
               
                
           
                if(voltage_work)
                {   
                    if  (do_change)            
                    {
                        if(led_state1)
                        {
                            led_state1=0;
                        }else led_state1=1;
                        
                        do_change=false;
                        i=0;
                    }
                    

                    if(led_state1)
                    {
                        gpio_set_level(GPIO_LED11, 1);
                        gpio_set_level(GPIO_LED12, 0);
                        
                        
                        gpio_set_level(GPIO_RELAY_1, 1);
                        
                        gpio_set_level(GPIO_RELAY_CRUSH, 0);
                        ESP_LOGI(TAG, "LED1 горит %d, time1 %d", i,  led1_work_time);
                        led1_work=true;
                        led2_work=false;

                        if(!acidification)
                        {   
                            gpio_set_level(GPIO_RELAY_2, 0);
                            gpio_set_level(GPIO_LED21, 0);
                            gpio_set_level(GPIO_LED22, 1);
                        }
                        

                    }else    
                            {   
                                
                                gpio_set_level(GPIO_LED21, 1);
                                gpio_set_level(GPIO_LED22, 0);
                               
                                gpio_set_level(GPIO_RELAY_2, 1);
                                gpio_set_level(GPIO_RELAY_CRUSH, 0);
                                ESP_LOGI(TAG, "LED2 горит %d, time2 %d", i,  led2_work_time);
                                led1_work=false;
                                led2_work=true;

                                if(!acidification)
                                {
                                    gpio_set_level(GPIO_RELAY_1, 0);
                                    gpio_set_level(GPIO_LED11, 0);
                                    gpio_set_level(GPIO_LED12, 1);
                                }
                                
                            }
                            
                    
/*                     if(led1_work)
                    work_time=led1_work_time;
                    if (led2_work)
                    work_time=led2_work_time; */

                    i++;
                    vTaskDelayUntil ( &prevWakeup, pdMS_TO_TICKS ( 1000 )) ;
                }
               
            } 
        }
        vTaskDelay(100/ portTICK_PERIOD_MS);
    }
    
}
bool led1_reserve;
bool led2_reserve;
bool leds_crush;

bool repeat_led1=true;
bool repeat_led2=true;
bool repeat_crush=true;

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
                    if(led1_enable&&phase_pump1)
                    {
                        if(repeat_led1)
                        {
                            gpio_set_level(GPIO_LED11, 1);
                            gpio_set_level(GPIO_LED12, 0);
                            gpio_set_level(GPIO_RELAY_1, 1);

                            gpio_set_level(GPIO_RELAY_2, 0);
                            gpio_set_level(GPIO_LED21, 0);
                            gpio_set_level(GPIO_LED22, 1);
                            
                            gpio_set_level(GPIO_RELAY_CRUSH, 0);

                            ESP_LOGI(TAG, "LED1 горит резерв");
                            led1_reserve=false;
                            led2_reserve=true;
                            leds_crush=true;

                            led1_work=true;
                            led2_work=false;

                            repeat_led1=false;
                            repeat_led2=true;
                            repeat_crush=true;
                        }
                                
                    }else if(led2_enable&&phase_pump2)
                            {   
                                if(repeat_led2)
                                {
                                    gpio_set_level(GPIO_RELAY_1, 0);
                                    gpio_set_level(GPIO_LED11, 0);
                                    gpio_set_level(GPIO_LED12, 1);

                                    gpio_set_level(GPIO_RELAY_2, 1);
                                    gpio_set_level(GPIO_LED21, 1);
                                    gpio_set_level(GPIO_LED22, 0);

                                    gpio_set_level(GPIO_RELAY_CRUSH, 0);
                                    ESP_LOGI(TAG, "LED2 горит резерв");
                                    led1_reserve=true;
                                    led2_reserve=false;
                                    leds_crush=true;
                                    led1_work=false;
                                    led2_work=true;

                                    repeat_led1=true;
                                    repeat_led2=false;
                                    repeat_crush=true;
                                }

                            } else  
                                    {
                                        if(repeat_crush)
                                        {
                                            gpio_set_level(GPIO_RELAY_1, 0);
                                            gpio_set_level(GPIO_LED11, 0);
                                            gpio_set_level(GPIO_LED12, 1);

                                            gpio_set_level(GPIO_RELAY_2, 0);
                                            gpio_set_level(GPIO_LED21, 0);
                                            gpio_set_level(GPIO_LED22, 1);

                                            gpio_set_level(GPIO_RELAY_CRUSH, 1);
                                            ESP_LOGI(TAG, "оба насоса сломаны");
                                            led1_reserve=true;
                                            led2_reserve=true;
                                            leds_crush=false;

                                            led1_work=false;
                                            led2_work=false;


                                            
                                            repeat_led1=true;
                                            repeat_led2=true;
                                            repeat_crush=false;
                                        }
                                        
                                        
                                    }   
            
            }
        }     

        vTaskDelay(500/ portTICK_PERIOD_MS);
    }
    
}


bool repeat_overheat1=true;
bool repeat_overheat2=true;
bool repeat_check=true;


void check_leds()
{
    
    
    while (1)
    {   
        if(task_check_power_ready)
        {   
            
                        if(!gpio_get_level(GPIO_OVERHEAT1))
                        {
                            if(!led1_enable)
                            {
                                led1_enable=true;
                            }
                        }
                        else 
                            {
                                if(led1_enable)
                                {
                                    
                                    ESP_LOGE(TAG, "led1 перегрев");
                                    led1_enable=false;
                                    
                                }
                            }
                        
                        if(!gpio_get_level(GPIO_OVERHEAT2))
                        {
                            if(repeat_overheat2)
                            {
                                led2_enable=true;
                                repeat_overheat2=false;
                            }
                            
                            //gpio_set_level(GPIO_LED1, 1);
                        }
                        else 
                            {
                                if(!repeat_overheat2)
                                {
                                    repeat_overheat2=true;
                                    ESP_LOGE(TAG, "led2 перегрев");
                                    led2_enable=false;
                                    
                                }
                                
                                //gpio_set_level(GPIO_LED1, 0);
                            }
                                
                        if(gpio_get_level(GPIO_VOLTAGE))
                        {
                            

                                //ESP_LOGE(TAG, "Напряжение 380");

                                if (gpio_get_level(GPIO_PHASE_11)||gpio_get_level(GPIO_PHASE_12)||gpio_get_level(GPIO_PHASE_13))
                                {
                                    if(phase_pump1)
                                    {
                                        phase_pump1=false;
                                        ESP_LOGE(TAG, "Фазы 1 насоса сломаны");
                                    }
                                }else   if(!phase_pump1) phase_pump1=true;

                                if (gpio_get_level(GPIO_PHASE_21)||gpio_get_level(GPIO_PHASE_22)||gpio_get_level(GPIO_PHASE_23))
                                {
                                    if(phase_pump2)
                                    {
                                        phase_pump2=false;
                                        ESP_LOGE(TAG, "Фазы 2 насоса сломаны");
                                    }
                                    
                                }else if(!phase_pump2) phase_pump2=true;

                                if(phase_pump1&&phase_pump2)
                                {
                                    voltage_work=true;
                                    //ESP_LOGE(TAG, "voltage_work=true");
                                }else 
                                    {
                                        //ESP_LOGE(TAG, "voltage_work=false");
                                        voltage_work=false;
                                    }
                                    
                

                        }else   
                            {
                                    

                                //ESP_LOGE(TAG, "Напряжение 220");
                                voltage_work=true;
                                phase_pump1=true;
                                phase_pump2=true;
                                    
                            }

                                
            
            

            
            

            if(led1_enable&&led2_enable&&voltage_work)
            {
                //ESP_LOGI(TAG, "Разрешена смена светодиодов");
                semafore=true;
            }
            else 
                {   
                    //ESP_LOGE(TAG, "Запрещена смена светодиодов");
                    semafore=false;
                }
           
        } 
        vTaskDelay(500/ portTICK_PERIOD_MS);
    }
    
}

static bool tasks_suspended = true;

void check_power(void *pvParameters)
{
    

    gpio_set_level(GPIO_LED11, 0);
    gpio_set_level(GPIO_LED12, 0);
    gpio_set_level(GPIO_RELAY_1, 0);
    gpio_set_level(GPIO_RELAY_2, 0);
    gpio_set_level(GPIO_LED21, 0);
    gpio_set_level(GPIO_LED22, 0);
    gpio_set_level(GPIO_RELAY_CRUSH, 0);

    tasks_suspended = true;
    
    while (1)
    {   

        if(!gpio_get_level(GPIO_BUTTON_POWER))
        {
            vTaskDelay(50);
        }

        if(!gpio_get_level(GPIO_BUTTON_POWER))
        {
            if(tasks_suspended)
            {
             
                task_check_power_ready=true;
                ESP_LOGI(TAG, "контроллер включен");
                tasks_suspended=false;
                
            }
                
            
        }
        else 
            {
                if(!tasks_suspended)
                {
                    task_check_power_ready=false;
                    gpio_set_level(GPIO_LED11, 0);
                    gpio_set_level(GPIO_LED12, 0);
                    gpio_set_level(GPIO_RELAY_1, 0);
                    gpio_set_level(GPIO_RELAY_2, 0);
                    gpio_set_level(GPIO_LED21, 0);
                    gpio_set_level(GPIO_LED22, 0);
                    gpio_set_level(GPIO_RELAY_CRUSH, 0);
                    
                    ESP_LOGE(TAG, "контроллер выключен");
                    blink_loop_work=false;
                    tasks_suspended=true;
                }
            }
        //ESP_LOGI(TAG, "Задача check power работает");
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}

void check_time(void *pvParameters)
{
    TickType_t wake_check_time = 0;
    led1_work=false;
    led2_work=false;

    while (1)
    {  
        if(task_check_power_ready)
        {
            if(led1_work)
            {
               
                led1_work_time++;
                pump1_work_minutes++;
                //ESP_LOGE(TAG, "led1_work_time %d", led1_work_time);
                if(led1_work_time%period_save_nvs==0)
                {
                    if(led1_work_time>=hours)
                    {
                        ESP_LOGE(TAG, "led1_work_time %d hours %d", led1_work_time, hours);
                        if(!do_change)
                        {
                            do_change=true;
                        }
                        led1_work_time=0;
                        
                    }
                    driver_nvs_write_u32(pump1_work_minutes,"pump1"); 
                    driver_nvs_write_u32(led1_work_time, "change1");
                    driver_nvs_write_u8(led_state1, "active_pump");
                    ESP_LOGE(TAG, "насос1: часы работы %d, цикл %d, номер насоса %d", pump1_work_minutes, led1_work_time, led_state1);
                    ESP_LOGE(TAG, "насос2: часы работы %d, цикл %d, номер насоса %d", pump2_work_minutes, led2_work_time, led_state1);
                }
                
                
                
            }      
            if (led2_work)
            {
                led2_work_time++;
                pump2_work_minutes++;
                
                if (led2_work_time%period_save_nvs==0)
                {
                    if(led2_work_time>=hours)
                    {
                        ESP_LOGE(TAG, "led2_work_time %d hours %d", led2_work_time, hours);
                        if(!do_change)
                        {
                            do_change=true;
                        }
                        led2_work_time=0;
                        
                    }
                    driver_nvs_write_u32(pump2_work_minutes,"pump2");
                    driver_nvs_write_u32(led2_work_time, "change2"); 
                    driver_nvs_write_u8(led_state1, "active_pump");
                    ESP_LOGE(TAG, "насос1: часы работы %d, цикл %d, номер насоса %d", pump1_work_minutes, led1_work_time, led_state1);
                    ESP_LOGE(TAG, "насос2: часы работы %d, цикл %d, номер насоса %d", pump2_work_minutes, led2_work_time, led_state1);
                    
                }

               
                
                
            }
        }
        vTaskDelayUntil ( &wake_check_time, pdMS_TO_TICKS (1000 )) ;
    }
}



void timer1_callback(TimerHandle_t pxTimer)
{
    acidification=true;
    ESP_LOGW("timer1", "timer1 acidification on ");
    gpio_set_level(GPIO_RELAY_1, 1);
    gpio_set_level(GPIO_LED11, 1);
    gpio_set_level(GPIO_LED12, 0);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    if(led_state1!=1)
    {
        gpio_set_level(GPIO_RELAY_1, 0);
        gpio_set_level(GPIO_LED11, 0);
        gpio_set_level(GPIO_LED21, 1);
    }
    
    acidification=false;
}

void timer2_callback(TimerHandle_t pxTimer)
{
    acidification=true;
    ESP_LOGW("timer2", "timer2 acidification on ");
    gpio_set_level(GPIO_RELAY_2, 1);
    gpio_set_level(GPIO_LED21, 1);
    gpio_set_level(GPIO_LED22, 0);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    if(led_state1!=0)
    {
        gpio_set_level(GPIO_RELAY_2, 0);
        gpio_set_level(GPIO_LED21, 0);
        gpio_set_level(GPIO_LED22, 1);
    }
        
    acidification=false;
}

void check_acidification(void *pvParameters)
{
    _timer1 = xTimerCreate("Timer1", pdMS_TO_TICKS(1000*timer1_acidification),  pdTRUE, NULL, timer1_callback);
    // Запускаем таймер
    if (xTimerStart( _timer1, 0) == pdPASS) {
        ESP_LOGI("main", "Software FreeRTOS timer1 stated");
    };

    _timer2 = xTimerCreate("Timer2", pdMS_TO_TICKS(1000*timer2_acidification),  pdTRUE, NULL, timer2_callback);
    // Запускаем таймер
    if (xTimerStart( _timer2, 0) == pdPASS) {
        ESP_LOGI("main", "Software FreeRTOS timer2 stated");
    };


    while (1)
    {  
        if(task_check_power_ready)
        {
            if(led1_work||!led1_enable||!phase_pump1) 
                xTimerReset(_timer1, 5);

            if(led2_work||!led2_enable||!phase_pump2)
                xTimerReset(_timer2, 5);
            
        }else 
            {
                xTimerReset(_timer1, 5);
                xTimerReset(_timer2, 5);
            }

        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}


void app_main()
{
    // initialization NVS
    driver_nvs_init();

/*     uint32_t a=1000;
    driver_nvs_write_u32(a, "pump1");
    driver_nvs_write_u32(a, "pump2"); */

    driver_nvs_read_u32(&pump1_work_minutes, "pump1");
    driver_nvs_read_u32(&pump2_work_minutes, "pump2");

    ESP_LOGE(TAG, "минуты работы насоса1 %d", pump1_work_minutes);
    ESP_LOGE(TAG, "минуты работы насоса2 %d", pump2_work_minutes);

/*     uint32_t a=0;
    driver_nvs_write_u32(a, "change1");
    driver_nvs_write_u32(a, "change2"); */
    
    driver_nvs_read_u32(&led1_work_time, "change1");
    driver_nvs_read_u32(&led2_work_time, "change2");

    ESP_LOGE(TAG, "работа цикла насоса1 %d", led1_work_time);
    ESP_LOGE(TAG, "работа цикла насоса2 %d", led2_work_time);

/*     uint8_t active_pump=1; 
    driver_nvs_write_u8(active_pump, "active_pump");*/

    driver_nvs_read_u8(&led_state1, "active_pump");
    ESP_LOGE(TAG, "работающий насос %d", led_state1);

    // initialization GPIO pump1
    gpio_reset_pin(GPIO_RELAY_1);
    gpio_set_direction(GPIO_RELAY_1, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_RELAY_1, 0);
    gpio_reset_pin(GPIO_LED11);
    gpio_set_direction(GPIO_LED11, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_LED11, 0);
    gpio_reset_pin(GPIO_LED12);
    gpio_set_direction(GPIO_LED12, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_LED12, 0);
    //gpio_reset_pin(GPIO_OVERHEAT1);
    gpio_set_direction(GPIO_OVERHEAT1, GPIO_MODE_INPUT);
    gpio_set_pull_mode(GPIO_OVERHEAT1, GPIO_FLOATING);
    gpio_reset_pin(GPIO_PHASE_11);
    gpio_set_direction(GPIO_PHASE_11, GPIO_MODE_INPUT);
    gpio_set_pull_mode(GPIO_PHASE_11, GPIO_FLOATING);
    gpio_reset_pin(GPIO_PHASE_12);
    gpio_set_direction(GPIO_PHASE_12, GPIO_MODE_INPUT);
    gpio_set_pull_mode(GPIO_PHASE_12, GPIO_FLOATING);
    gpio_reset_pin(GPIO_PHASE_13);
    gpio_set_direction(GPIO_PHASE_13, GPIO_MODE_INPUT);
    gpio_set_pull_mode(GPIO_PHASE_13, GPIO_FLOATING);


    
    // initialization GPIO pump2
    gpio_reset_pin(GPIO_RELAY_2);
    gpio_set_direction(GPIO_RELAY_2, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_RELAY_2, 0);
    gpio_reset_pin(GPIO_LED21);
    gpio_set_direction(GPIO_LED21, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_LED21, 0);
    gpio_reset_pin(GPIO_LED22);
    gpio_set_direction(GPIO_LED22, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_LED22, 0);
    //gpio_reset_pin(GPIO_OVERHEAT2);
    gpio_set_direction(GPIO_OVERHEAT2, GPIO_MODE_INPUT);
    gpio_set_pull_mode(GPIO_OVERHEAT2, GPIO_FLOATING);
    gpio_reset_pin(GPIO_PHASE_21);
    gpio_set_direction(GPIO_PHASE_21, GPIO_MODE_INPUT);
    gpio_set_pull_mode(GPIO_PHASE_21, GPIO_FLOATING);
    gpio_reset_pin(GPIO_PHASE_22);
    gpio_set_direction(GPIO_PHASE_22, GPIO_MODE_INPUT);
    gpio_set_pull_mode(GPIO_PHASE_22, GPIO_FLOATING);
    gpio_reset_pin(GPIO_PHASE_23);
    gpio_set_direction(GPIO_PHASE_23, GPIO_MODE_INPUT);
    gpio_set_pull_mode(GPIO_PHASE_23, GPIO_FLOATING);

   
    // initialization GPIO general
    //gpio_reset_pin(GPIO_BUTTON_POWER);
    gpio_set_direction(GPIO_BUTTON_POWER, GPIO_MODE_INPUT);
    gpio_set_pull_mode(GPIO_BUTTON_POWER, GPIO_FLOATING);
    gpio_reset_pin(GPIO_RELAY_CRUSH);
    gpio_set_direction(GPIO_RELAY_CRUSH, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_RELAY_CRUSH, 0);
    //gpio_reset_pin(GPIO_VOLTAGE);
    gpio_set_direction(GPIO_VOLTAGE, GPIO_MODE_INPUT);
    gpio_set_pull_mode(GPIO_VOLTAGE, GPIO_FLOATING);


    


    // Инициализация WiFi
    wifi_init_softap();

    
    // Запуск HTTP сервера
    start_webserver();

    ESP_LOGI(TAG, "Система запущена. Подключитесь к WiFi: %s", EXAMPLE_ESP_WIFI_SSID);
    ESP_LOGI(TAG, "Откройте браузер и перейдите по адресу: http://192.168.4.1");

    vTaskDelay(1000 / portTICK_PERIOD_MS);

    xTaskCreate(check_leds, "check_leds", 4096, NULL, 5, NULL);


    

    xTaskCreate(blink_loop, "blink_loop", 4096, NULL, 5, NULL);

    xTaskCreate(one_blink,"one blink",4096, NULL, 5, NULL);

    xTaskCreate(check_power, "check_power", 4096, NULL, 7, NULL);

    xTaskCreate(check_time, "check_time", 4096, NULL, 5, NULL);

    xTaskCreate(check_acidification, "check_acidification", 4096, NULL, 5, NULL);
} 

