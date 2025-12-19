#ifndef STRUCT_PUMP
#define STRUCT_PUMP

typedef struct{
    uint32_t hours;
    uint32_t period_save_nvs;
    uint32_t timer_acidification;
    bool led1_work;
    bool led2_work;
}   struct_pump_t;



#endif // STRUCT_PUMP_H