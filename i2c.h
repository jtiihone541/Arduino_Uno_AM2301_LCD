/*
 * i2c.h
 *
 * 
 */ 


#ifndef I2C_H_
#define I2C_H_

/*
 * TWI status codes
 *
 */

#define TWI_MSS_START_TRANSMITTED 0x08
#define TWI_MSS_REPEATED_START_TRANSMITTED 0x10
#define TWI_MSS_SLA_W_TRANSMITTED_ACK_RECEIVED 0x18
#define TWI_MSS_SLA_W_TRANSMITTED_NO_ACK_RECEIVED 0x20
#define TWI_MSS_DATA_TRANSMITTED_ACK_RECEIVED 0x28
#define TWI_MSS_DATA_TRANMITTED_NO_ACK_RECEIVED 0x30
#define TWI_MSS_DATA_TRANSMITTED_ARBITRATION_LOST 0x38

/*
 * I2C read is not complete...
 *
 */

typedef enum
{
    WR_START_SENDING = 0,
    WR_SLA_SENDING,
    WR_DATA_SENDING,
    WR_STOP_SENDING,
    WR_ERROR,
    RD_START_SENDING,
    RD_SLA_SENDING,
    RD_DATA_RECEIVING
}twi_i2c_isr_states_t;

typedef struct
{
    volatile twi_i2c_isr_states_t state;
    uint8_t address;
    uint8_t data_length;
    uint8_t *data_ptr;
    uint8_t status; /* This is relevant only if an error occured */
} twi_i2c_state_t;

typedef enum
{
    INTERFACE_4_BITS,
    INTERFACE_8_BITS
} lcd_interface_len_t;


void init_twi();
void twi_send_command(uint8_t address, uint8_t length, uint8_t *data);
void poll_for_twi_transmitted();


#endif /* I2C_H_ */