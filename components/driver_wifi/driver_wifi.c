#include "driver_wifi.h"


#define EXAMPLE_ESP_WIFI_SSID "ESP32_UART_Server"
#define EXAMPLE_ESP_WIFI_PASS "12345678"
#define EXAMPLE_ESP_WIFI_CHANNEL 1
#define EXAMPLE_MAX_STA_CONN 4

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

static const char *TAG_WIFI = "driver_wifi";
struct_pump_t* wifi_ptr_pump;
uint32_t wifi_hours=60;

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
/*         "<div><h2>Время для антизакисания: <strong>%ld</strong></h2>"
        "<form method='POST' action='/'>"
            "<input type='number' name='hours' min='1' max='100' value='%ld'>"
            "<input type='submit' class='btn off' value='Сохранить'>"
        "</form></div>" */
    "</body></html>";

// Обработчик WiFi событий
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        ESP_LOGI(TAG_WIFI, "Устройство подключено");
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        ESP_LOGI(TAG_WIFI, "Устройство отключено");
    }
}

// Инициализация WiFi
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

    ESP_LOGI(TAG_WIFI, "WiFi AP запущен: %s", EXAMPLE_ESP_WIFI_SSID);
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
    
    ESP_LOGI(TAG_WIFI, "Получено POST: %s", buf);
    int new_hours=1;

    char *hours_start = strstr(buf, "hours=");
    if (hours_start) {
        hours_start += 6; // Пропускаем "hours="
        new_hours = atoi(hours_start);
        if (new_hours >= 1) {
            
            
            wifi_ptr_pump->hours = new_hours;
            ESP_LOGI(TAG_WIFI, "Новое время смены насосов: %d",wifi_ptr_pump->hours);
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
    const char* led1_status =  wifi_ptr_pump->led1_work ? "ВКЛ" : "ВЫКЛ";
    const char* led2_status = wifi_ptr_pump->led2_work ? "ВКЛ" : "ВЫКЛ";
    
    int len = snprintf(response, sizeof(response), html_template, 
                      led1_status, led2_status, wifi_ptr_pump->hours, wifi_ptr_pump->hours);
    
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
    //led_state1 = 1;
    //led1_enable = true;
    return redirect_to_home(req);
}

static esp_err_t led_off_handler(httpd_req_t *req)
{
    //led_state1 = 0;
    //led1_enable = false;
    return redirect_to_home(req);
}

static esp_err_t led_on_handler2(httpd_req_t *req)
{
    
    //led2_enable = true;
    return redirect_to_home(req);
}

static esp_err_t led_off_handler2(httpd_req_t *req)
{
    
    //led2_enable = false;
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
void start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192;
    config.task_priority = tskIDLE_PRIORITY + 3;
    config.max_uri_handlers = 8;
    config.lru_purge_enable = true;
    config.recv_wait_timeout = 10;
    config.send_wait_timeout = 10;
    
    ESP_LOGI(TAG_WIFI, "Запуск HTTP сервера на порту: %d", config.server_port);
    
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK) {
        // Регистрация обработчиков
        httpd_register_uri_handler(server, &root);
        httpd_register_uri_handler(server, &post);
        httpd_register_uri_handler(server, &ledon);
        httpd_register_uri_handler(server, &ledoff);
        httpd_register_uri_handler(server, &ledon2);
        httpd_register_uri_handler(server, &ledoff2);
        
        ESP_LOGI(TAG_WIFI, "HTTP сервер успешно запущен");
    } else {
        ESP_LOGE(TAG_WIFI, "Ошибка запуска HTTP сервера");
    }
}

void wifi_set_struct_pump(struct_pump_t* ptr_struct_t)
{
    wifi_ptr_pump=ptr_struct_t;
}