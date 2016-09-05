/*
 * lcd_with_i2c.c
 *
 * 
 *
 * 
 * Note the I2C byte -> LCD mapping!
 * This mapping is done in "I2C->parallel 4bit HD44780" interface circuit,
 * so map I2C data octet bits according to specification.
 *
 * Bit 7 - D7
 * Bit 6 - D6
 * Bit 5 - D5
 * Bit 4 - D4
 * Bit 3 - backlight control
 * Bit 2 - EN
 * Bit 1 - RW
 * Bit 0 - RS
 */

#include <avr/io.h>
#include <avr/interrupt.h>

#include "lcd_with_i2c.h"
#include "i2c.h"

void send_i2c_lcd_command_8bit_mode(uint8_t address, uint8_t rs, uint8_t data);
void send_i2c_lcd_command_4bit_mode(uint8_t address, uint8_t rs, uint8_t data);


uint8_t lcd_i2c_byte;
uint8_t lcd_backlight_command;
uint8_t lcd_backlight = LCD_BACKLIGHT;

i2c_lcd_data_t i2c_lcd_data = {0x3f, 2, 16};
    
lcd_command_table_t lcd_commands[13] = { \
    {SCREEN_CLEAR, 0, 0, 0x01, 1640},\
    {CURSOR_RETURN, 0, 0, 0x02, 1640},\
    {INPUT_SET, 0, 0, 0x04, 40},\
    {DISPLAY_SWITCH, 0, 0, 0x08, 40},\
    {SHIFT, 0, 0, 0x10, 40},\
    {FUNCTION_SET, 0, 0, 0x20, 40},\
    {CGRAM_AD_SET, 0, 0, 0x40, 40},\
    {DDRAM_AD_SET, 0, 0, 0x80, 40},\
    {BUSY_AD_READ_CT, 0, 1, 0x00, 40},\
    {CGRAM_DATA_WRITE, 1, 0, 0x00, 40},\
    {DDRAM_DATA_WRITE, 1, 0, 0x00, 40},\
    {CGRAM_DATA_READ, 1, 1, 0x00, 40},\
    {DDRAM_DATA_READ, 1, 1, 0x00, 40}
};

/*
 * LCD backlight can be set separately from LCD commands, just be sure not to set EN signal
 * so the LCD does not read these databits.
 *
 */
void change_lcd_backlight(uint8_t new_state)
{
    lcd_backlight = new_state & 1;
    /* Update state immediately */
    lcd_backlight_command = (lcd_backlight << 3);
    twi_send_command(0x3f, 1, &lcd_backlight_command);
    poll_for_twi_transmitted();

    return;
}

void lcd_write_command(lcd_command_name_t command, uint8_t parameter)
{
    uint8_t cpc;
    uint8_t rs;
    uint32_t i;
    
    cpc = lcd_commands[command].command_binary_code | parameter; /* Command and Parameter Combined... */
    rs = lcd_commands[command].rs;

    send_i2c_lcd_command_4bit_mode(0x3f, rs, cpc);
    
    /* Execute delay according to commands' delay value */
    for (i = 0; i < 5 * lcd_commands[command].execution_time_us; i++);
    return;
}
void lcd_write_character(uint8_t chr)
{
    lcd_write_command(DDRAM_DATA_WRITE, chr);
}

void lcd_clear_screen(void)
{
    lcd_write_command(SCREEN_CLEAR, 0);
    return;
}

void lcd_write_string(uint8_t row, uint8_t column, const char *ptr)
{
    /* "row" and "column" start from zero */
    
    uint8_t lcd_screen_address = 0, row_base = 0;
    uint8_t i;
    
    switch (row)
    {
        case 0:
        row_base = 0;
        break;
        
        case 1:
        row_base = 0x40;
        break;
        
        default:
        row_base = 0;
        break;
    }
    lcd_screen_address = row_base + column;
    lcd_write_command(DDRAM_AD_SET, lcd_screen_address);

    /* Data address set, send string char by char */
    i = 0;
    for (i = 0; i < 16; i++)
    {
        if (ptr[i] == 0) break;
        lcd_write_character(ptr[i]);
    }

    return;
}
void unaccurate_delay(uint8_t milliseconds)
{
    /* A rough accuracy delay */
    
    uint32_t i;
    
    for (i = 0; i < (16000 * milliseconds); i++);
    return;
}

/*
 * Although I2C interface LCD uses 4bit mode - it must be initially configured in "8-bit mode".
 *
 */
void init_lcd()
{
    unaccurate_delay(100);
    send_i2c_lcd_command_8bit_mode(0x3f, 0, 0x30);
    unaccurate_delay(20);
    send_i2c_lcd_command_8bit_mode(0x3f, 0, 0x30);
    unaccurate_delay(10);
    send_i2c_lcd_command_8bit_mode(0x3f, 0, 0x30);
    unaccurate_delay(1);
    send_i2c_lcd_command_8bit_mode(0x3f, 0, 0x20);  /* Switch to 4bit command mode */
    unaccurate_delay(2);

    lcd_write_command(FUNCTION_SET, FUNCTION_SET_4D | FUNCTION_SET_2R | FUNCTION_SET_5X7);
    lcd_write_command(DISPLAY_SWITCH, DISPLAY_SWITCH_DISPLAY_OFF);
    lcd_write_command(SCREEN_CLEAR, 0);
    lcd_write_command(INPUT_SET, INPUT_SET_INCREMENT_MODE|INPUT_SET_NO_SHIFT);
    lcd_write_command(DISPLAY_SWITCH, DISPLAY_SWITCH_DISPLAY_ON);
    return;
}



/*
 * In the beginning on LCD initialisation, LCD is in 8bit mode.
 * Although physical interface in only 4bits, Some "8bit" commands can be
 * sent by writing only 4MSB bits of 8bit command.
 * Therefore data is written only once
 *
 */
void send_i2c_lcd_command_8bit_mode(uint8_t address, uint8_t rs, uint8_t data)
{
    lcd_i2c_byte = data & 0xF0; /* Leave 4 MSBs, they are already at correct place! */
    lcd_i2c_byte |= (lcd_backlight << 3);
    lcd_i2c_byte |= (rs & 1);
    lcd_i2c_byte |= (1 << 2); /* Set EN */
    
    twi_send_command(address, 1, &lcd_i2c_byte);
    poll_for_twi_transmitted();
    lcd_i2c_byte ^= 0x4; /* Clear EN */
    twi_send_command(address, 1, &lcd_i2c_byte);
    poll_for_twi_transmitted();
    return;
}

/*
 * Because LCD interface is 4bit-mode after I2C, normal commands must be written in 
 * two parts, same way as with direct parallel mode interface
 *
 */
void send_i2c_lcd_command_4bit_mode(uint8_t address, uint8_t rs, uint8_t data)
{
    
    /* Send 8bit data in two 4-bit pieces over I2C to display */
    
    lcd_i2c_byte = data & 0xF0; /* Leave 4 MSBs, they are already at correct place! */
    lcd_i2c_byte |= (lcd_backlight << 3);
    lcd_i2c_byte |= (rs & 1);
    lcd_i2c_byte |= (1 << 2); /* Set EN */
    twi_send_command(address, 1, &lcd_i2c_byte);
    poll_for_twi_transmitted();
    lcd_i2c_byte ^= 0x4; /* Clear EN */
    twi_send_command(address, 1, &lcd_i2c_byte);
    poll_for_twi_transmitted();
    lcd_i2c_byte = (data << 4) & 0xf0;
    lcd_i2c_byte |= (lcd_backlight << 3);
    lcd_i2c_byte |= (rs & 1);
    lcd_i2c_byte |= 0x4; /* Set EN */
    twi_send_command(address, 1, &lcd_i2c_byte);
    poll_for_twi_transmitted();
    lcd_i2c_byte ^= 0x4; /* Clear EN */
    twi_send_command(address, 1, &lcd_i2c_byte);
    poll_for_twi_transmitted();
    return;
}

