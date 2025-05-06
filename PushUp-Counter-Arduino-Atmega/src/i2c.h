#ifndef I2C_H__
#define I2C_H__

#include "common.h"
#include <avr/interrupt.h>
#include "timer.h"
// --- Configuration ---

/** @brief Maximum size of the I2C transmit/receive buffer */
#define I2C_BUFFER_LENGTH 32

// --- LCD Specific Address ---
// Choose the correct address for your specific LCD backpack module
#define LCD_ADDRESS_DEFAULT     0x27 // Common default
#define LCD_ADDRESS_ALTERNATIVE 0x3F // Common alternative (PCF8574A)
#define LCD_ADDRESS LCD_ADDRESS_DEFAULT
// #define LCD_ADDRESS LCD_ADDRESS_ALTERNATIVE // Example: Using the alternative

// --- I2C Status Codes ---
// (User-provided status codes are kept as is)
#ifndef I2C_STATUS__
#define I2C_STATUS__

// Custom status code (Internal library state)
#define I2C_IDLE 0
#define I2C_BUSY 1
#define I2C_ERROR_START_TIMEOUT 2
#define I2C_ERROR_NACK_ADDR 3
#define I2C_ERROR_NACK_DATA 4
#define I2C_ERROR_ARB_LOST 5
#define I2C_ERROR_BUS_ERROR 6
#define I2C_ERROR_OTHER 7

#define I2C_STATUS_BIT_MASK 0xF8
#define I2C_STATUS (TWSR & I2C_STATUS_BIT_MASK)

// General status codes
#define I2C_START                   0x08
#define I2C_REP_START               0x10
#define I2C_ARB_LOST                0x38
// Master Transmitter Mode Status Codes
#define I2C_MT_SLA_ACK              0x18
#define I2C_MT_SLA_NACK             0x20
#define I2C_MT_DATA_ACK             0x28
#define I2C_MT_DATA_NACK            0x30
// Master Receiver Mode Status Codes
#define I2C_MR_SLA_ACK              0x40
#define I2C_MR_SLA_NACK             0x48
#define I2C_MR_DATA_ACK             0x50
#define I2C_MR_DATA_NACK            0x58
// Slave Transmitter Mode Status Codes (Not implemented in this master-only library)
#define I2C_ST_SLA_ACK              0xA8
#define I2C_ST_ARB_LOST_SLA_ACK     0xB0
#define I2C_ST_DATA_ACK             0xB8
#define I2C_ST_DATA_NACK            0xC0
#define I2C_ST_LAST_DATA_ACK        0xC8
// Slave Receiver Mode Status Codes (Not implemented in this master-only library)
#define I2C_SR_SLA_ACK              0x60
#define I2C_SR_ARB_LOST_SLA_ACK     0x68
#define I2C_SR_GCALL_ACK            0x70
#define I2C_SR_ARB_LOST_GCALL_ACK   0x78
#define I2C_SR_DATA_ACK             0x80
#define I2C_SR_DATA_NACK            0x88
#define I2C_SR_GCALL_DATA_ACK       0x90
#define I2C_SR_GCALL_DATA_NACK      0x98
#define I2C_SR_STOP_REP_START       0xA0
// Miscellaneous Status Codes
#define I2C_BUS_ERROR               0x00
#define I2C_NO_INFO                 0xF8

#endif // I2C_STATUS__

/**
 * @brief Initializes the I2C hardware (TWI) peripheral.
 * Configures SCL frequency, enables TWI, and enables TWI interrupt.
 * Must be called before any other I2C operations.
 * Remember to enable global interrupts (sei()) after calling this.
 */
void init_i2c(void);

/**
 * @brief Disables the I2C hardware (TWI) peripheral and interrupt.
 */
void deinit_i2c(void);

/**
 * @brief Initiates transmitting data to a specific I2C slave device.
 * This function is non-blocking. It sets up the transfer and returns immediately.
 * The actual transmission occurs via interrupts.
 *
 * @param data Data to transmit.
 * @param send_stop If true, a STOP condition will be sent after transmission. If false,
 * a Repeated START might follow (e.g., for read operation after write).
 * @return true if the I2C bus was idle and the transmission was initiated.
 * @return false if the I2C bus is busy or invalid arguments provided.
 */
bool send_byte(const uint8_t data, bool send_stop);

/**
 * @brief Checks if the I2C peripheral is currently busy with a transaction.
 *
 * @return true if busy, false if idle or an error occurred.
 */
bool i2c_is_busy(void);

/**
 * @brief Generates an I2C STOP condition manually.
 * Usually called internally, but can be used to ensure the bus is released
 * if a transfer was started with send_stop=false and no further action followed.
 * Note: This is blocking for a short period while STOP is sent.
 */
void stop_i2c(void);


// Static variables

// Current state of the I2C FSM
extern volatile uint8_t i2c_status;

#endif // I2C_H__

/**
 * TWDR = data;
 * TWCR = (1 << TWEN) | (1 << TWEA) | (1 << TWIE) | [(1 << TWSTA) | (1 << TWSTO)] | (1 << TWINT);
 * while (TWCR & (1 << TWINT));
 * uint8_t status = (TWSR & 0xF8);
 */

/** 
 * 1. Setup
 * set prescaler
 * TWSR = (1 << TWPS0);
 * 
 * set bit rate
 * TWBR = 72;
 * 
 * send start bit
 * TWCR = (1 << TWEN) | (1 << TWEA) | (1 << TWIE) | (1 << TWSTA) | (1 << TWINT);
 *          ENABLE       ENABLE ACK   ENABLE INT     START BIT         SEND
 * 
 * poll / wait until transaction finished
 * while (TWCR & (1 << TWINT));
 * 
 * check status
 * switch (TWSR & 0xF8) {
 * 		...
 * }
 * if START is ACK'ed:
 * 
 * 2. Send SLA + W
 * set/write slave address in TWDR
 * TWDR = SLA+W/R [(0x27 << 1) | 0 / 1]
 *                     SLA       W   R
 * 
 * send slave address
 * TWCR = (1 << TWEN) | (1 << TWEA) | (1 << TWIE) | (1 << TWINT);
 * 
 * poll / wait until transaction finished
 * while (TWCR & (1 << TWINT));
 * 
 * check status
 * switch (TWSR & 0xF8) {
 * 		...
 * }
 * 
 * if START is ACK'ed:
 * 
 * 3. Send data
 * 
 * while (data_is_available) {
 * 		uint8_t data = ...;
 * 		TWDR = data;
 * 		TWCR = (1 << TWEN) | (1 << TWEA) | (1 << TWIE) | (1 << TWINT);
 * 		
 * 		poll / wait until transaction finished
 * 		while (TWCR & (1 << TWINT));
 * 		
 * 		check status
 * 		switch (TWSR & 0xF8) {
 * 				...
 * 		}
 * 		if START is ACK'ed:
 * 		...
 * 		else:
 * 		...
 * }
 * 
 * 4. Terminate connection
 * 
 * TWCR = (1 << TWEN) | (1 << TWEA) | (1 << TWIE) | (1 << TWSTO) | (1 << TWINT);
 *                                                    STOP BIT
 * 
 * poll / wait until stop bit is cleared / set to 0
 * while (TWCR & (1 << TWSTO));
 * 
 * done
 * 
 * else: 
 * retry step 1/2 
 * 
 * else:
 * retry step 1
 */