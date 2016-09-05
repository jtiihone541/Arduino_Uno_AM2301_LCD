/*
 * am2301.h
 *
 *  
 */ 


#ifndef AM2301_H_
#define AM2301_H_

#define TIMESTAMPS 40
#define DATA_VALID 0
#define DATA_PARITY_ERROR 1
#define DATA_INCOMPLETE_DATA 2

typedef struct
{
    uint8_t bitcounter;
    uint8_t zero_bit_limit;
    uint16_t humidity_int;
    uint16_t temperature_int;
    uint16_t last_timestamp;
    uint8_t parity;
    uint8_t data_validity;
    uint16_t timestamps[TIMESTAMPS];
    uint16_t abs_time[TIMESTAMPS];
} am2301_interrupt_data_t;

void initial_am2301_wakeup();
void stop_am2301_measurement();
void start_am2301_measurement();
void get_am2301_temperature(char *, uint8_t);
void get_am2301_humidity(char *, uint8_t);
#endif /* AM2301_H_ */