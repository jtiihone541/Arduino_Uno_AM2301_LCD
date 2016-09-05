/*
 * i2c.c
 *
 * 
 * Write-only implementation to get I2C working with LCD.
 * LCD requires only write operation (although reading is also possible in theory), so very simple implementation is enough.
 *
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>

#include "i2c.h"

uint8_t i2c_byte = 0;
twi_i2c_state_t i2c_state;

void twi_error(uint8_t errorcode)
{
    
    i2c_state.status = errorcode;
    return;
}

void init_twi()
{
    /* Let's assume we are using 16MHz clock, set TWI bit rate close to 100kbit/s */
    /* means that 2*TWBR*prescaler should be 160-16 -> prescaler = 1 and TWBR=72 */
    
    TWBR = 72;
    TWSR = TWSR & 0xFC;
    TWCR = (1 << TWIE);
    return;
}

void twi_send_command(uint8_t address, uint8_t length, uint8_t *data)
{
    i2c_state.address = address;
    i2c_state.data_length = length;
    i2c_state.data_ptr = data;
    i2c_state.state = WR_START_SENDING;
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWIE) | (1 << TWEN);
    return;
}

void poll_for_twi_transmitted()
{
    while ((i2c_state.state != WR_STOP_SENDING));
    return;
}

/*
 * The Two Wire Interface Interrupt handler for I2C master mode.
 *
 * Thanks to TWI implementation of ATMEGA328P, this is quite simple!
 *
 * I2C procedure is following:
 * 1. main prb configures I2C
 * 2. main prb writes I2C command into buffer
 * 3. main prb starts I2C operation, i.e. start sending
 * 4. following parts are handled by ISR
 * 5. ISR indicates main prb about completion of the procedure (actually main prb polls I2C state...)
 *
 */
ISR(TWI_vect)
{
    uint8_t errorcode;
    switch(i2c_state.state)
    {
        case    WR_START_SENDING:
        if ( ((TWSR & 0xF8) != TWI_MSS_START_TRANSMITTED) && ((TWSR & 0xF8) != TWI_MSS_REPEATED_START_TRANSMITTED))
        {
            /* Error occured */
            errorcode = TWSR & 0xF8;
            twi_error(errorcode);
            break;
        }
        TWDR = (i2c_state.address << 1);
        TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWIE);
        i2c_state.state = WR_SLA_SENDING;
        break;
        
        case    WR_SLA_SENDING:
        if ((TWSR & 0xF8) != TWI_MSS_SLA_W_TRANSMITTED_ACK_RECEIVED)
        {
            /* Error occured */
            errorcode = TWSR & 0xF8;
            twi_error(errorcode);
            break;
        }
        TWDR = *i2c_state.data_ptr;
        i2c_state.data_ptr++;
        i2c_state.data_length--;
        TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWIE);
        i2c_state.state = WR_DATA_SENDING;
        break;
        
        case    WR_DATA_SENDING:
        if ((TWSR & 0xF8) != TWI_MSS_DATA_TRANSMITTED_ACK_RECEIVED)
        {
            /* Error occured */
            errorcode = TWSR & 0xF8;
            twi_error(errorcode);
            break;
        }
        /* Data byte sent successfully. Check if more data to be sent */
        if (i2c_state.data_length > 0)
        {
            /* Still more data to be sent -> no need to change state!!! */
            TWDR = *i2c_state.data_ptr;
            TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWIE);
            i2c_state.data_ptr++;
            i2c_state.data_length--;
        }
        else
        {
            /* No more data */
            TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWSTO);
            
            /* Acknowledge to upper layer transmission complete! */
            i2c_state.state = WR_STOP_SENDING;
        }
        break;
        
        case    WR_STOP_SENDING:
        break;
        
        case    WR_ERROR:
        /* Maybe something should be done here... */
        break;
        
        default:
        break;
    }
    return;
}