/*
 * timer.h
 *
 * 
 */ 


#ifndef TIMER_H_
#define TIMER_H_

#define OCR_LIMIT 20000

void init_timer(void);
void delay_seconds(uint32_t delay_value);
void delay_microseconds(uint16_t delay_value);
uint32_t get_system_clock(void);

#endif /* TIMER_H_ */