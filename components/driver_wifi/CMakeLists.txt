idf_component_register(SRCS 
                        "driver_wifi.c"
                    INCLUDE_DIRS 
                        "." 
                    REQUIRES 
                        driver 
                        freertos 
                        esp_wifi
                        esp_http_server
                        )

