/*
 * am2301.c
 *
 * 
 * Procedures for AM2301 temperature sensor usage, including:
 * - configuring I/O-port (it can be hardcoded because "input capture" is supported only one pin in ATMEGA328P)
 * - ISR for "input capture"
 * - conversion of data from AM2301 format into more readable format
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>
#include <string.h>
#include <stdio.h>
#include "am2301.h"
#include "timer.h"


am2301_interrupt_data_t interrupt_data;

void set_am2301_pin_output(uint8_t signal_state)
{
    DDRB |= 1; /* B0 as output */
    if ((signal_state & 1) == 1)
    {
        PORTB |= (signal_state & 1);
    }
    else
    {
        PORTB &= 0xfe;
    }
    return;
}

void set_am2301_pin_input()
{
    DDRB &= 0xfe; /* B0 as input */
    PORTB |= 1; /* Pull up enabled */
    return;
}

void enable_am2301_input_capture_interrupt()
{
    TIFR1 |= (1 << ICF1);
    TIMSK1 |= (1 << ICIE1);
    return;
}

void disable_am2301_input_capture_interrupt()
{
    TIMSK1 &= ~(1 << ICIE1);
    return;
}

/*
 * Make a dummy measurement request (data is not received) to wake up AM2301. Usually AM2301 gives only trash data
 * from first measurement, but later ones are successful.
 *
 */
void initial_am2301_wakeup()
{
    uint32_t delay_loop;
    set_am2301_pin_output(0); /* Atmel recommendation not to go directly from tri-state to output high */
    set_am2301_pin_output(1);
    for (delay_loop = 0; delay_loop < 20000; delay_loop++);
    set_am2301_pin_output(0);
    for (delay_loop = 0; delay_loop < 20000; delay_loop++);
    set_am2301_pin_output(1);
    
    return;
}

void stop_am2301_measurement()
{
    set_am2301_pin_output(0);
    set_am2301_pin_output(1);
    disable_am2301_input_capture_interrupt();
    
    return;
}

/*
 * Start AM2301 measurement by pulling data line low for some time, then pulling it back to high, and finally configure
 * pin as input. Initialise the data structure and enable input capture interrupt from timer
 *
 */
void start_am2301_measurement()
{
    uint32_t stop;
    memset(&interrupt_data, 0, sizeof(interrupt_data));
    set_am2301_pin_output(0);
    for (stop = 0; stop < 20000; stop++);
    set_am2301_pin_input();
    enable_am2301_input_capture_interrupt();
    interrupt_data.zero_bit_limit = 180;
    return;
}

/*
 * Timer input capture interrupt, this is called when "input signal change" is
 * detected, and "timestamp" is automatically saved by HW to be read later - here in ISR.
 * Because "timestamp" is saved by HW, timing can be very accurate because interrupt latency does not affect timing.
 * 
 * In this application, however, "input capture" is a bit "too accurate" (0,5us resolution). AM2301 sends falling edges
 * at periods less than 85us (zero bits) and more than 116us (one bits). Difference is more than 30us - far more than
 * typical interrupt latency, e.g. if using pin change interrupts.
 * 
 * ISR keeps count on how many falling edges are detected, lengths of bits (from falling edge to next falling edge)
 * are calculated from previous time stamp and current one.
 *
 * Two first falling edges are discarded, because they are not "databits", but "handshaking bits".
 * Actual "bit lengths" are stored into array until all 40 bits are received - remainder of bits are discarded.
 * For some reason, this AM2301 sends 65 databits for some reason - extra bits are zeroes...
 *
 */
ISR(TIMER1_CAPT_vect)
{
    register uint16_t timestamp, time_difference;

    interrupt_data.bitcounter++;

    /* There is two unnecessary "falling edges" at beginning, discard them */
    if (interrupt_data.bitcounter < 3)
    {
        interrupt_data.last_timestamp = ICR1;
        return;
    }
    if (interrupt_data.bitcounter > 42)
    {
        return;
    }

    /* Now handle rest of bits */
    timestamp = ICR1;

    if (timestamp > interrupt_data.last_timestamp)
    {
        /* timer has not wrapped */
        time_difference = timestamp - interrupt_data.last_timestamp;
    }
    else
    {
        /* timer has wrapped */
        time_difference = 20000 - (interrupt_data.last_timestamp - timestamp);
    }
    interrupt_data.last_timestamp = timestamp;
    interrupt_data.timestamps[interrupt_data.bitcounter -3] = time_difference;
    return;
}

void calculate_am2301_data(am2301_interrupt_data_t *data)
{
    uint8_t i, parity, temp_parity;
    uint16_t conversion;
    
    /* Very first, checking - is there enough databits? If not, then the AM2301 may not have responded properly */
    if (data->bitcounter < 43)
    {
        data->data_validity = DATA_INCOMPLETE_DATA;
        return;
    }
    conversion = 0;
    for(i = 0; i < 16; i++)
    {
        if (data->timestamps[i] > data->zero_bit_limit)
        {
            /* Bit is longer than zero bit time -> it is '1' */
            conversion = (conversion << 1) | 1;
        }
        else
        {
            /* Bit time is shorter than zero bit time -> it is '0' */
            conversion = conversion << 1;
        }
    }
    data->humidity_int = conversion;
    
    conversion = 0;
    
    for (i = 16; i < 32; i++)
    {
        if (data->timestamps[i] > data->zero_bit_limit)
        {
            /* Bit is longer than zero bit time -> it is '1' */
            conversion = (conversion << 1) | 1;
        }
        else
        {
            /* Bit time is shorter than zero bit time -> it is '0' */
            conversion = conversion << 1;
        }
    }
    data->temperature_int = conversion;
    
    parity = 0;
    
    for (i = 32; i < 40; i++)
    {
        if (data->timestamps[i] > data->zero_bit_limit)
        {
            /* Bit is longer than zero bit time -> it is '1' */
            conversion = (conversion << 1) | 1;
        }
        else
        {
            /* Bit time is shorter than zero bit time -> it is '0' */
            conversion = conversion << 1;
        }
    }
    return;
    
    /* Calculate parity: It is 8lowmost bits of sum of all 4 databytes */
    
    temp_parity = (data->humidity_int >> 8) + (data->humidity_int & 0xff) + (data->temperature_int >> 8) + (data->temperature_int & 0xff);
    if (temp_parity == parity)
    {
        data->data_validity = DATA_VALID;
    }
    else
    {
        data->data_validity = DATA_PARITY_ERROR;
    }
    
    
}
void get_am2301_temperature(char *ptr, uint8_t maxlen)
{
    calculate_am2301_data(&interrupt_data);
    
    switch (interrupt_data.data_validity)
    {
        case    DATA_VALID:
                /* Temperature may be negative, this is indicated by the MSB set '1' */
                if ((interrupt_data.temperature_int & 0x8000) == 0x8000)
                {
                    /* Negative temperature, make it negative by using negative divider */
                    snprintf(ptr, maxlen, "Temp: %.1f %c%c   ", (float)(interrupt_data.temperature_int & 0x7fff)/-10.0, 0xdf, 0x43);
                }
                else
                {
                    /* Positive temperature */
                    snprintf(ptr, maxlen, "Temp: %.1f %c%c  ", (float)(interrupt_data.temperature_int)/10.0, 0xdf, 0x43);
                }                    
                break;
                
        case    DATA_PARITY_ERROR:
                snprintf(ptr, maxlen, "Temp: <parity>");
                break;
                
        default:
                snprintf(ptr, maxlen, "Temp: <no data>");
                break;
    }
    return;
}

void get_am2301_humidity(char *ptr, uint8_t maxlen)
{
    calculate_am2301_data(&interrupt_data);
    switch (interrupt_data.data_validity)
    {
        case    DATA_VALID:
                snprintf(ptr, maxlen, "Hum : %.1f %s   ", (interrupt_data.humidity_int)/10.0, "%");
                break;
        
        case    DATA_PARITY_ERROR:
                snprintf(ptr, maxlen, "Hum : <parity>");
                break;
        
        default:
                snprintf(ptr, maxlen, "Hum : <no data>");
                break;
    }
    return;
}

