#ifndef STRUCT_PUMP
#define STRUCT_PUMP

typedef struct{
    bool pump1_work;
    bool pump2_work;
    bool wifi_control;
    bool pump1_go;
    bool pump2_go;
}   struct_pump_t;

typedef struct {
    uint32_t pump1_operation_time;
    uint32_t pump2_operation_time;
    uint16_t pump1_time_cycle;
    uint16_t pump2_time_cycle;
    uint8_t  change_pumps;
    uint16_t change_time;
    uint16_t time_save_nvs;
    uint16_t timer_acidification;
} struct_nvs_t;


#endif // STRUCT_PUMP_H