/*
 * Arduino_UNO_I2C_LCD_test_1.c
 *
 * 
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>
#include "i2c.h"
#include "lcd_with_i2c.h"
#include "timer.h"
#include "am2301.h"

#define MAX_LINE_LEN 16

int main(void)
{
    char display_str[MAX_LINE_LEN];

    SREG |= 128; /* Enable interrupts */
    init_timer();
    init_twi();
    init_lcd();
    lcd_write_string(0,0,"Initializing");
    lcd_write_string(1,0,"Wait...");
    initial_am2301_wakeup();
    delay_seconds(1);
    while (1) 
    {
        start_am2301_measurement();
        delay_seconds(1);
        stop_am2301_measurement();
        get_am2301_temperature(display_str, MAX_LINE_LEN);
        lcd_write_string(0,0,display_str);
        get_am2301_humidity(display_str, MAX_LINE_LEN);
        lcd_write_string(1,0,display_str);
        delay_seconds(9);
    }
}

