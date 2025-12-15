idf_component_register(SRCS 
                        "driver_nvs.c"
                    INCLUDE_DIRS 
                        "." 
                    REQUIRES 
                        driver 
                        freertos 
                        nvs_flash
                        )

# add nvs