/*
 * timer.c
 *
 * 
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include "timer.h"


volatile uint32_t system_clock;

void configure_sleep_mode();

/* Initialise timer service (timer1) with following parameters
 *
 * - timer is clocked with 2MHz, i.e. 16MHz xtal clock divided by 2
 * - timer is configured into CTC mode, and OCRA is set to 20000, providing 100Hz "systick"
 * - input compare is preconfigured, but interrupt is kept disabled
 */
void init_timer(void)
{
    TCCR1A = (0 << COM1A1) | (0 << COM1A0) | (0 << WGM11) | (0 << WGM10);
    TCCR1B = (1 << ICNC1) | (0 << ICES1) | (0 << WGM13) | (1 << WGM12) | (0 << CS12) | (1 << CS11) | (0 << CS10);
    TCNT1 = 0; /* Note: C-compiler handles the correct ordering of 2 8 bits writes into 16bit register */
    OCR1A = OCR_LIMIT; /* Same as for TCNT1. 2000000/20000 = 100 */
    TIMSK1 = (1 << OCIE1A);
    system_clock = 0;
    configure_sleep_mode();
    return;
}

uint32_t get_system_clock(void)
{
    return system_clock;
}

void configure_sleep_mode()
{
    set_sleep_mode(SLEEP_MODE_IDLE);
}

void delay_seconds(uint32_t delay_value)
{
    uint32_t expiration_time, current_system_clock;

    current_system_clock = system_clock;
    expiration_time = (delay_value * 100) + current_system_clock;
    if (0 == delay_value)
    {
        /* Someone will try and test how this would work :) */
        return;
    }
    while (1)
    {
        if (system_clock == expiration_time) break;
        
        /* Put CPU into sleep mode. It is awakened by some interrupt: */
        /* timer interrput -> system_clock has advanced */
        /* Any other interrupt -> system_clock has not advanced */
        /* Any way, wake up from sleep causes timer expiration checking, and either exit or re-sleep... */
        sleep_mode();
    }
    return;
}

void delay_microseconds(uint16_t delay_value)
{
    uint16_t expiration_time, counter_value, expiration_time_upper_limit;
    counter_value = TCNT1;
    expiration_time = counter_value + 2*delay_value;
    expiration_time_upper_limit = expiration_time + 50; /* Hat constant :) */
    
    /* Because interrupts may occur during busy loop, it is possible that timer counter */
    /* seems to increment more than one in consecutive checking, there must be enough */
    /* "overtime window" to allow busy loop terminated */
    
    if (expiration_time_upper_limit > expiration_time)
    {
        /* Normal "unwrapped" situation */
        /* Timer is expired, when TCNT1 is within "time - time+50" */
        while(1)
        {
            if ((TCNT1 >= expiration_time) && (TCNT1 < expiration_time_upper_limit)) break;
        }
    }
    else
    {
        /* Wrapped situation */
        /* Timer is expired, when TCNT1 is outside of "time+50 - time" */
        while(1)
        {
            if ((TCNT1 < expiration_time_upper_limit) || (TCNT1 >= expiration_time_upper_limit)) break;
        }
    }
    return;
}

/*
 * Timer compare interrupt, this is the periodical system tick, called by 100Hz frequency
 *
 */
ISR(TIMER1_COMPA_vect)
{
    system_clock++;
    return;
}

