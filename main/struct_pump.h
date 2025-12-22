#ifndef STRUCT_PUMP
#define STRUCT_PUMP

typedef struct{
    uint32_t hours;
    uint32_t period_save_nvs;
    uint32_t timer_acidification;
    bool pump1_work;
    bool pump2_work;
    bool wifi_control;
    bool pump1_go;
    bool pump2_go;
}   struct_pump_t;



#endif // STRUCT_PUMP_H