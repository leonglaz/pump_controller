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
#include "freertos/timers.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include <time.h>

#include "driver_nvs.h"
#include "driver_wifi.h"
#include "struct_pump.h"

#define EXAMPLE_ESP_WIFI_SSID "ESP32_UART_Server"

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


#define FLAG_POWER (1<<0)
#define FLAG_PUMP1_READY (1<<1)
#define FLAG_PUMP2_READY (1<<2)
#define FLAG_WORKING (1<<3)
#define FLAG_REVERSE (1<<4)
#define FLAG_CRUSH (1<<5)
#define FLAG_ACDIFICATION (1<<6)
#define FLAG_WIFI_CONTROL (1<<7)


struct_pump_t pump_t ={0};
struct_pump_t * ptr_pump_t=&pump_t;

struct_nvs_t nvs_t ={0};
struct_nvs_t * ptr_nvs_t=&nvs_t;

EventGroupHandle_t xEventGroup;

static const char *TAG = "Pump_Conroller";





static bool led1_enable = false;
static bool led2_enable = false;


bool do_change=false;

TimerHandle_t _timer1 = NULL;
TimerHandle_t _timer2 = NULL;
bool acidification=false;






void working()
{
    TickType_t prevWakeup = 0;
    uint16_t i=0;
    
    EventBits_t Bits_working;
    
    while (1)
    {   
        Bits_working = xEventGroupWaitBits(xEventGroup, 
        FLAG_WORKING,
        pdTRUE, pdTRUE,    pdMS_TO_TICKS(portMAX_DELAY));    

        if(Bits_working & (FLAG_POWER) && (Bits_working & FLAG_PUMP1_READY) && (Bits_working & FLAG_PUMP2_READY) && !ptr_pump_t->wifi_control)
        {
           
             
            if  (do_change)            
            {
                if(ptr_nvs_t->change_pumps)
                {
                    ptr_nvs_t->change_pumps=0;
                }else ptr_nvs_t->change_pumps=1;
                
                do_change=false;
                i=0;
            }
            

            if(ptr_nvs_t->change_pumps)
            {
                gpio_set_level(GPIO_LED11, 1);
                gpio_set_level(GPIO_LED12, 0);
                
                
                gpio_set_level(GPIO_RELAY_1, 1);
                
                gpio_set_level(GPIO_RELAY_CRUSH, 0);
                ESP_LOGI(TAG, "LED1 горит %d, time1 %d", i,  ptr_nvs_t->pump1_time_cycle);
                ptr_pump_t->pump1_work=true;
                ptr_pump_t->pump2_work=false;

                if(!acidification)
                {   
                    gpio_set_level(GPIO_RELAY_2, 0);
                    gpio_set_level(GPIO_LED21, 0);
                    gpio_set_level(GPIO_LED22, 0);
                }
                

            }else    
                    {   
                        
                        gpio_set_level(GPIO_LED21, 1);
                        gpio_set_level(GPIO_LED22, 0);
                        
                        gpio_set_level(GPIO_RELAY_2, 1);
                        gpio_set_level(GPIO_RELAY_CRUSH, 0);
                        ESP_LOGI(TAG, "LED2 горит %d, time2 %d", i,  ptr_nvs_t->pump2_time_cycle);
                        ptr_pump_t->pump1_work=false;
                        ptr_pump_t->pump2_work=true;

                        if(!acidification)
                        {
                            gpio_set_level(GPIO_RELAY_1, 0);
                            gpio_set_level(GPIO_LED11, 0);
                            gpio_set_level(GPIO_LED12, 0);
                        }
                        
                    }
            i++;
            vTaskDelayUntil ( &prevWakeup, pdMS_TO_TICKS ( 1000 )) ;
             
        }
        else if(!(Bits_working & (FLAG_POWER)) && (Bits_working & FLAG_PUMP1_READY) && (Bits_working & FLAG_PUMP2_READY))
        {
            gpio_set_level(GPIO_RELAY_CRUSH, 0);
            ESP_LOGI(TAG, "working нет питания");
            ptr_pump_t->pump1_work=false;
            ptr_pump_t->pump2_work=false;

            if(!acidification)
            {
                gpio_set_level(GPIO_RELAY_1, 0);
                gpio_set_level(GPIO_LED11, 0);
                gpio_set_level(GPIO_LED12, 0);

                gpio_set_level(GPIO_LED21, 0);
                gpio_set_level(GPIO_LED22, 0);
                gpio_set_level(GPIO_RELAY_2, 0);
            }
        }
        vTaskDelay(100/ portTICK_PERIOD_MS);
    }
    
}

void reverse()
{
    EventBits_t Bits_reverse;

    while (1)
    { 
        Bits_reverse = xEventGroupWaitBits(xEventGroup, 
        FLAG_REVERSE,
        pdTRUE, pdTRUE,    pdMS_TO_TICKS(portMAX_DELAY));

        if((Bits_reverse & FLAG_PUMP1_READY) && !(Bits_reverse & FLAG_PUMP2_READY))
        {
                if(Bits_reverse & FLAG_POWER)
                    {
                        gpio_set_level(GPIO_LED11, 1);
                        gpio_set_level(GPIO_RELAY_1, 1);
                        ESP_LOGE(TAG, "питание есть  в резерве");
                    }else 
                    {
                        gpio_set_level(GPIO_LED11, 0);
                        gpio_set_level(GPIO_RELAY_1, 0);
                        ESP_LOGE(TAG, "питание нет в резерве");
                    }
                
                gpio_set_level(GPIO_LED12, 0);
                

                gpio_set_level(GPIO_RELAY_2, 0);
                gpio_set_level(GPIO_LED21, 0);
                gpio_set_level(GPIO_LED22, 1);
                
                gpio_set_level(GPIO_RELAY_CRUSH, 0);

                ESP_LOGI(TAG, "насос 1 работает  в резерве");
       

                ptr_pump_t->pump1_work=true;
                ptr_pump_t->pump2_work=false;
                           
        }else  if(!(Bits_reverse & FLAG_PUMP1_READY) && (Bits_reverse & FLAG_PUMP2_READY))
                {   
                    gpio_set_level(GPIO_RELAY_1, 0);
                    gpio_set_level(GPIO_LED11, 0);
                    gpio_set_level(GPIO_LED12, 1);

                    if((xEventGroupGetBits(xEventGroup) & FLAG_POWER))
                    {
                        gpio_set_level(GPIO_LED21, 1);
                        gpio_set_level(GPIO_RELAY_2, 1);
                        ESP_LOGE(TAG, "питание есть в резерве");
                    }else 
                    {
                        gpio_set_level(GPIO_LED21, 0);
                        gpio_set_level(GPIO_RELAY_2, 0);
                        ESP_LOGE(TAG, "питание нет в резерве");
                    }

                    gpio_set_level(GPIO_LED22, 0);

                    gpio_set_level(GPIO_RELAY_CRUSH, 0);
                    ESP_LOGI(TAG, "насос 2 работает  в резерве");
                    
                    ptr_pump_t->pump1_work=false;
                    ptr_pump_t->pump2_work=true;
                }
            
            
        

        vTaskDelay(500/ portTICK_PERIOD_MS);
    }
    
}

void crush()
{
    EventBits_t Bits_crush;

    while (1)
    {   

        Bits_crush = xEventGroupWaitBits(xEventGroup, 
        FLAG_CRUSH,
        pdTRUE, pdTRUE,    pdMS_TO_TICKS(portMAX_DELAY));
        if(!(Bits_crush & FLAG_PUMP1_READY) && !(Bits_crush & FLAG_PUMP2_READY))
        {
            gpio_set_level(GPIO_RELAY_1, 0);
            gpio_set_level(GPIO_LED11, 0);
            gpio_set_level(GPIO_LED12, 1);

            gpio_set_level(GPIO_RELAY_2, 0);
            gpio_set_level(GPIO_LED21, 0);
            gpio_set_level(GPIO_LED22, 1);

            
            ESP_LOGI(TAG, "оба насоса сломаны");

            ptr_pump_t->pump1_work=false;
            ptr_pump_t->pump2_work=false;           
            
            if((Bits_crush & FLAG_POWER))
            {
                gpio_set_level(GPIO_RELAY_CRUSH, 1);
            }
            else gpio_set_level(GPIO_RELAY_CRUSH, 0);
        }

        vTaskDelay(500/ portTICK_PERIOD_MS);
    }
    
}





void check_pumps()
{
    
    while (1)
    {   

            if(gpio_get_level(GPIO_VOLTAGE))
            {
                    //ESP_LOGE(TAG, "Напряжение 380");
                    if (gpio_get_level(GPIO_PHASE_11)||gpio_get_level(GPIO_PHASE_12)||gpio_get_level(GPIO_PHASE_13)||gpio_get_level(GPIO_OVERHEAT1))
                    {
                        vTaskDelay(100);
                    }

                    if (gpio_get_level(GPIO_PHASE_11)||gpio_get_level(GPIO_PHASE_12)||gpio_get_level(GPIO_PHASE_13)||gpio_get_level(GPIO_OVERHEAT1))
                    {
                        xEventGroupClearBits(xEventGroup, FLAG_PUMP1_READY);
                        ESP_LOGE(TAG, "Фазы 1 насоса сломаны или насос перегрет");
                        
                    }else   xEventGroupSetBits(xEventGroup, FLAG_PUMP1_READY);

                    if (gpio_get_level(GPIO_PHASE_21)||gpio_get_level(GPIO_PHASE_22)||gpio_get_level(GPIO_PHASE_23)||gpio_get_level(GPIO_OVERHEAT1))
                    {
                        vTaskDelay(100);
                    }

                    if (gpio_get_level(GPIO_PHASE_21)||gpio_get_level(GPIO_PHASE_22)||gpio_get_level(GPIO_PHASE_23)||gpio_get_level(GPIO_OVERHEAT1))
                    {
                        
                        ESP_LOGE(TAG, "Фазы 2 насоса сломаны или насос перегрет");
                        xEventGroupClearBits(xEventGroup, FLAG_PUMP2_READY);
                        
                    }else xEventGroupSetBits(xEventGroup, FLAG_PUMP2_READY);
            }else   
                {
                    //ESP_LOGE(TAG, "Напряжение 220");
                    if(gpio_get_level(GPIO_OVERHEAT1))
                    {
                        vTaskDelay(100);
                    }

                    if(gpio_get_level(GPIO_OVERHEAT1))
                    {
                        xEventGroupClearBits(xEventGroup, FLAG_PUMP1_READY);
                        ESP_LOGE(TAG, "led1 перегрев");
                    }
                    else 
                        {
                            xEventGroupSetBits(xEventGroup, FLAG_PUMP1_READY);
                        }
                    
                    if(gpio_get_level(GPIO_OVERHEAT2))
                    {
                        vTaskDelay(100);
                    }

                    if(gpio_get_level(GPIO_OVERHEAT2))
                    {
                        xEventGroupClearBits(xEventGroup, FLAG_PUMP2_READY);
                        ESP_LOGE(TAG, "led2 перегрев");
                        
                    }
                    else 
                        {
                            xEventGroupSetBits(xEventGroup, FLAG_PUMP2_READY);
                        }
                    
                }

            if((xEventGroupGetBits(xEventGroup) & FLAG_PUMP1_READY) && (xEventGroupGetBits(xEventGroup) & FLAG_PUMP2_READY))
            {
                xEventGroupSetBits(xEventGroup, FLAG_WORKING);
            }

            if((!(xEventGroupGetBits(xEventGroup) & FLAG_PUMP1_READY) && (xEventGroupGetBits(xEventGroup) & FLAG_PUMP2_READY))||((xEventGroupGetBits(xEventGroup) & FLAG_PUMP1_READY) && !((xEventGroupGetBits(xEventGroup) & FLAG_PUMP2_READY))))
            {
                xEventGroupSetBits(xEventGroup, FLAG_REVERSE);
            }

            if(!(xEventGroupGetBits(xEventGroup) & FLAG_PUMP1_READY) && !(xEventGroupGetBits(xEventGroup) & FLAG_PUMP2_READY))
            {
                xEventGroupSetBits(xEventGroup, FLAG_CRUSH);
            }

            if((xEventGroupGetBits(xEventGroup) & FLAG_PUMP1_READY) && (xEventGroupGetBits(xEventGroup) & FLAG_PUMP2_READY) && ptr_pump_t->wifi_control)
            {
                xEventGroupSetBits(xEventGroup, FLAG_WIFI_CONTROL);
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
             
                xEventGroupSetBits(xEventGroup, FLAG_POWER);
                ESP_LOGI(TAG, "контроллер включен");
                tasks_suspended=false;
                
            }
        }
        else 
            {
                if(!tasks_suspended)
                {
                    xEventGroupClearBits(xEventGroup, FLAG_POWER);
                    xEventGroupClearBits(xEventGroup, FLAG_PUMP1_READY);
                    xEventGroupClearBits(xEventGroup, FLAG_PUMP2_READY);
                    gpio_set_level(GPIO_LED11, 0);
                    gpio_set_level(GPIO_LED12, 0);
                    gpio_set_level(GPIO_RELAY_1, 0);
                    gpio_set_level(GPIO_RELAY_2, 0);
                    gpio_set_level(GPIO_LED21, 0);
                    gpio_set_level(GPIO_LED22, 0);
                    gpio_set_level(GPIO_RELAY_CRUSH, 0);
                    
                    ESP_LOGE(TAG, "контроллер выключен");
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
    ptr_pump_t->pump1_work=false;
    ptr_pump_t->pump2_work=false;
    EventBits_t Bits_time;
    while (1)
    {  
        Bits_time = xEventGroupWaitBits(xEventGroup, FLAG_POWER, pdFALSE,  pdFALSE,    pdMS_TO_TICKS(portMAX_DELAY));    
        if(Bits_time & (FLAG_POWER))
        {
            if(ptr_pump_t->pump1_work)
            {
               
                ptr_nvs_t->pump1_time_cycle++;
                ptr_nvs_t->pump1_operation_time++;

                if(ptr_nvs_t->pump1_time_cycle>=ptr_nvs_t->change_time)
                    {
                        ESP_LOGE(TAG, "pump1_work_time %d hours %d", ptr_nvs_t->pump1_time_cycle, ptr_nvs_t->change_time);
                        if(!do_change)
                        {
                            do_change=true;
                        }
                        ptr_nvs_t->pump1_time_cycle=0;
                        
                    }
                //ESP_LOGE(TAG, "pump1_work_time %d", pump1_work_time);
                if(ptr_nvs_t->pump1_time_cycle%ptr_nvs_t->time_save_nvs==0)
                {
                    driver_nvs_write_blob(nvs_t);
                    ESP_LOGE(TAG, "насос1: часы работы %d, цикл %d, номер насоса %d", ptr_nvs_t->pump1_operation_time, ptr_nvs_t->pump1_time_cycle, ptr_nvs_t->change_pumps);
                    ESP_LOGE(TAG, "насос2: часы работы %d, цикл %d, номер насоса %d", ptr_nvs_t->pump2_operation_time, ptr_nvs_t->pump2_time_cycle, ptr_nvs_t->change_pumps);
                }
                
                
                
            }      
            if (ptr_pump_t->pump2_work)
            {
                ptr_nvs_t->pump2_time_cycle++;
                ptr_nvs_t->pump2_operation_time++;
                
                if(ptr_nvs_t->pump2_time_cycle>=ptr_nvs_t->change_time)
                {
                    
                    if(!do_change)
                    {
                        do_change=true;
                    }
                    ptr_nvs_t->pump2_time_cycle=0;
                    
                }

                if (ptr_nvs_t->pump2_time_cycle%ptr_nvs_t->time_save_nvs==0)
                {
                    
                    driver_nvs_write_blob(nvs_t);
                    ESP_LOGE(TAG, "насос1: часы работы %d, цикл %d, номер насоса %d", ptr_nvs_t->pump1_operation_time, ptr_nvs_t->pump1_time_cycle, ptr_nvs_t->change_pumps);
                    ESP_LOGE(TAG, "насос2: часы работы %d, цикл %d, номер насоса %d", ptr_nvs_t->pump2_operation_time, ptr_nvs_t->pump2_time_cycle, ptr_nvs_t->change_pumps);
                    
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
    if(!ptr_pump_t->pump1_work)
    {
        gpio_set_level(GPIO_RELAY_1, 0);
        gpio_set_level(GPIO_LED11, 0);
        gpio_set_level(GPIO_LED12, 0);
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
    if(!ptr_pump_t->pump2_work)
    {
        gpio_set_level(GPIO_RELAY_2, 0);
        gpio_set_level(GPIO_LED21, 0);
        gpio_set_level(GPIO_LED22, 0);
    }
        
    acidification=false;
}

void timer3_callback(TimerHandle_t pxTimer)
{
    acidification=true;
    ESP_LOGW("timer2", "timer2 acidification on ");
    gpio_set_level(GPIO_RELAY_2, 1);
    gpio_set_level(GPIO_LED21, 1);
    gpio_set_level(GPIO_LED22, 0);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    if(!ptr_pump_t->pump2_work)
    {
        gpio_set_level(GPIO_RELAY_2, 0);
        gpio_set_level(GPIO_LED21, 0);
        gpio_set_level(GPIO_LED22, 0);
    }
        
    acidification=false;
}


void check_acidification(void *pvParameters)
{
    _timer1 = xTimerCreate("Timer1", pdMS_TO_TICKS(ptr_nvs_t->timer_acidification*1000),  pdTRUE, NULL, timer1_callback);
    // Запускаем таймер
    if (xTimerStart( _timer1, 0) == pdPASS) {
        ESP_LOGI("main", "Software FreeRTOS timer1 stated");
    };

    _timer2 = xTimerCreate("Timer2", pdMS_TO_TICKS(ptr_nvs_t->timer_acidification*1000),  pdTRUE, NULL, timer2_callback);
    // Запускаем таймер
    if (xTimerStart( _timer2, 0) == pdPASS) {
        ESP_LOGI("main", "Software FreeRTOS timer2 stated");
    };

    uint32_t last_timer_acidification=ptr_nvs_t->timer_acidification;

    EventBits_t Bits_acidification;
    while (1)
    {  
        
        if(ptr_pump_t->pump1_work||!(xEventGroupGetBits(xEventGroup) & FLAG_PUMP1_READY)) 
        {
            xTimerReset(_timer1, 5);
        }
                

        if(ptr_pump_t->pump2_work||!(xEventGroupGetBits(xEventGroup) & FLAG_PUMP1_READY))
        {
            xTimerReset(_timer2, 5);
        }
                
        if (last_timer_acidification!=ptr_nvs_t->timer_acidification)
        {
            xTimerChangePeriod(_timer1, pdMS_TO_TICKS(ptr_nvs_t->timer_acidification*1000),5);
            xTimerChangePeriod(_timer2, pdMS_TO_TICKS(ptr_nvs_t->timer_acidification*1000),5);
            last_timer_acidification=ptr_nvs_t->timer_acidification;
            
        }
        
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}

void wifi_conrol()
{
    
    
    uint16_t i=0;
    uint16_t j=0;
    EventBits_t Bits_wifi_control;
    
    while (1)
    {   
        Bits_wifi_control = xEventGroupWaitBits(xEventGroup, 
        FLAG_WIFI_CONTROL,
        pdTRUE, pdTRUE,    pdMS_TO_TICKS(portMAX_DELAY));    

        if(Bits_wifi_control & (FLAG_POWER) && (Bits_wifi_control & FLAG_PUMP1_READY) && (Bits_wifi_control & FLAG_PUMP2_READY)&& (Bits_wifi_control & FLAG_WIFI_CONTROL))
        {
           
            

            if(ptr_pump_t->pump1_go)
            {
                gpio_set_level(GPIO_LED11, 1);
                gpio_set_level(GPIO_LED12, 0);
                gpio_set_level(GPIO_RELAY_1, 1);                
                gpio_set_level(GPIO_RELAY_CRUSH, 0);
                ESP_LOGI(TAG, "PUMP1 горит в wifi режиме %d, time1 %d", i,  ptr_nvs_t->pump1_time_cycle);
                ptr_pump_t->pump1_work=true;

                
            } else  {
                        if(!acidification)
                        {
                        gpio_set_level(GPIO_LED11, 0);
                        gpio_set_level(GPIO_LED12, 0);
                        gpio_set_level(GPIO_RELAY_1, 0);                
                        }
                        gpio_set_level(GPIO_RELAY_CRUSH, 0);
                        ptr_pump_t->pump1_work=false;
                        i=0;
                    }
            
            if(ptr_pump_t->pump2_go)
            {   
                
                gpio_set_level(GPIO_LED21, 1);
                gpio_set_level(GPIO_LED22, 0);
                gpio_set_level(GPIO_RELAY_2, 1);
                gpio_set_level(GPIO_RELAY_CRUSH, 0);
                ESP_LOGI(TAG, "PUMP2 горит в wifi режиме %d, time2 %d", j,  ptr_nvs_t->pump2_time_cycle);
                ptr_pump_t->pump2_work=true;

                
                
                
            } else  {
                        if(!acidification)
                        {
                            gpio_set_level(GPIO_LED21, 0);
                            gpio_set_level(GPIO_LED22, 0);
                            gpio_set_level(GPIO_RELAY_2, 0);
                           
                        }
                        gpio_set_level(GPIO_RELAY_CRUSH, 0);
                        ptr_pump_t->pump2_work=false;
                        j=0;
                    }
            i++;
            j++;
             
        }
        
        vTaskDelay(100/ portTICK_PERIOD_MS);
    }
    
}


void app_main()
{


    ptr_pump_t->pump1_work=false;
    ptr_pump_t->pump2_work=false;
    // initialization NVS
    driver_nvs_init();

/*     struct_nvs_t test_nvs_t;
    test_nvs_t.pump1_operation_time=0;
    test_nvs_t.pump1_time_cycle=0;
    test_nvs_t.pump2_operation_time=0;
    test_nvs_t.pump2_time_cycle=0;
    test_nvs_t.change_time=30;
    test_nvs_t.change_pumps=0;
    test_nvs_t.time_save_nvs=20;
    test_nvs_t.timer_acidification=10;
    driver_nvs_write_blob(test_nvs_t);

    driver_nvs_read_blob(ptr_nvs_t); */



    ESP_LOGE(TAG, "минуты работы насоса1 %d", ptr_nvs_t->pump1_operation_time);
    ESP_LOGE(TAG, "минуты работы насоса2 %d", ptr_nvs_t->pump2_operation_time);



    ESP_LOGE(TAG, "работа цикла насоса1 %d", ptr_nvs_t->pump1_time_cycle);
    ESP_LOGE(TAG, "работа цикла насоса2 %d", ptr_nvs_t->pump2_time_cycle);



    ESP_LOGE(TAG, "работающий насос %d", ptr_nvs_t->change_pumps);

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
    gpio_set_direction(GPIO_BUTTON_POWER, GPIO_MODE_INPUT);
    gpio_set_pull_mode(GPIO_BUTTON_POWER, GPIO_FLOATING);
    gpio_reset_pin(GPIO_RELAY_CRUSH);
    gpio_set_direction(GPIO_RELAY_CRUSH, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_RELAY_CRUSH, 0);
    gpio_set_direction(GPIO_VOLTAGE, GPIO_MODE_INPUT);
    gpio_set_pull_mode(GPIO_VOLTAGE, GPIO_FLOATING);


    


    // Инициализация WiFi
    wifi_init_softap();
    wifi_set_struct_pump(ptr_pump_t, ptr_nvs_t);
    
    // Запуск HTTP сервера
    start_webserver();

    ESP_LOGI(TAG, "Система запущена. Подключитесь к WiFi: %s", EXAMPLE_ESP_WIFI_SSID);
    ESP_LOGI(TAG, "Откройте браузер и перейдите по адресу: http://192.168.4.1");


    xEventGroup = xEventGroupCreate();

    vTaskDelay(1000 / portTICK_PERIOD_MS);

    

    xTaskCreate(check_pumps, "check_pumps", 4096, NULL, 5, NULL);


    xTaskCreate(check_power, "check_power", 4096, NULL, 7, NULL);

    xTaskCreate(working, "working", 4096, NULL, 5, NULL);

    xTaskCreate(reverse,"one blink",4096, NULL, 5, NULL);

    xTaskCreate(crush,"crush",4096, NULL, 5, NULL);

    xTaskCreate(check_time, "check_time", 4096, NULL, 5, NULL);

    xTaskCreate(check_acidification, "check_acidification", 4096, NULL, 5, NULL);

    xTaskCreate(wifi_conrol, "wifi_conrol", 4096, NULL, 5, NULL);
} 

