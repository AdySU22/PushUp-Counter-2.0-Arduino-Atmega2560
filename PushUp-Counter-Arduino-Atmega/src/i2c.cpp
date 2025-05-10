#include "i2c.h"

// --- Internal State Variables ---
static bool send_stop;                // Flag to indicate if STOP should be sent at the end
static bool sent;
static uint8_t tx_data; 								// Temporary data storage variable
volatile uint8_t i2c_status = 0xFF;

// TWCR commands (makes code more readable)
#define I2C_CMD_START      ((1 << TWINT) | (1 << TWSTA) | (1 << TWEN) | (1 << TWIE))
#define I2C_CMD_STOP       ((1 << TWINT) | (1 << TWSTO) | (1 << TWEN)) // No Interrupt for STOP
#define I2C_CMD_TRANSMIT   ((1 << TWINT) | (1 << TWEN) | (1 << TWIE))
#define I2C_CMD_RECEIVE_ACK ((1 << TWINT) | (1 << TWEA) | (1 << TWEN) | (1 << TWIE))
#define I2C_CMD_RECEIVE_NACK ((1 << TWINT) | (1 << TWEN) | (1 << TWIE)) // No TWEA for NACK
#define I2C_CMD_CLEAR_TWINT ((1 << TWINT) | (1 << TWEN) | (1 << TWIE)) // Used sometimes just to clear flag

// --- Helper Functions ---

/**
 * @brief Resets the internal I2C state variables.
 * @param state The state to set after reset (usually IDLE or an ERROR state)
 */
static void i2c_reset_state(uint8_t state) {
	DEBUG_PRINT_VERBOSE("Resetting state to ");
	DEBUG_PRINTLN_VERBOSE(state);
    i2c_status = state;
    send_stop = true; // Default to sending stop
}

// --- Public Functions ---

void init_i2c(void) {
    // Reset state
    i2c_reset_state(I2C_IDLE);

    // Set TWI clock frequency
    // Formula: SCL_freq = CPU_freq / (16 + 2 * TWBR * Prescaler)
    // We use Prescaler = 1 (TWPS0=0, TWPS1=0)
    TWSR &= ~((1 << TWPS0) | (1 << TWPS1)); // Clear prescaler bits -> Prescaler = 1
    TWBR = 72;

    // Enable TWI peripheral and TWI interrupt
    TWCR = (1 << TWEN) | (1 << TWIE);
    // Note: Global interrupts must be enabled separately in main
}

void i2c_deinit(void) {
    // Disable TWI peripheral and interrupt
    TWCR = 0;
    i2c_reset_state(I2C_IDLE);
}

bool send_byte(const uint8_t data, bool send_stop) {
    if (i2c_status != I2C_IDLE) {
		DEBUG_PRINT("i2c status: ");
		DEBUG_PRINTLN(i2c_status);
        return false; // Bus is busy
    }
    
    i2c_status = I2C_BUSY;
    send_stop = send_stop;

	sent = false;
    tx_data = data;

    // Send START condition
    TWCR = I2C_CMD_START;

    return true;
}

bool i2c_is_busy(void) {
    // Consider BUSY state as busy. Other states (IDLE, ERROR) mean not actively transferring.
    return (i2c_status == I2C_BUSY);
}

void stop_i2c(void) {
    // Send STOP condition
    TWCR = I2C_CMD_STOP;

    // Wait for TWSTO bit to clear (STOP condition is finished)
    // This is a small blocking delay, unavoidable for STOP.
    // Add a timeout to prevent infinite loop in case of hardware issues
    uint16_t timeout = 0xFFFF; // Adjust timeout counter as needed
    while((TWCR & (1 << TWSTO)) && --timeout) {};

    // Reset state after stop is sent or timeout
    if (timeout == 0) {
        i2c_reset_state(I2C_ERROR_OTHER); // Indicate potential issue
    } else if (i2c_status == I2C_BUSY) { // Only transition from BUSY to IDLE
		i2c_reset_state(I2C_IDLE);
    }
    // If it was already in an error state, leave it there.
}

// --- I2C Interrupt Service Routine ---
ISR(TWI_vect) {
    uint8_t status = I2C_STATUS;
	DEBUG_PRINT_VERBOSE("Status: ");
	DEBUG_PRINTLN_VERBOSE(status);
    switch (status) {
        // --- Master Transmitter ---
        case I2C_START:      // 0x08: A START condition has been transmitted
        case I2C_REP_START:  // 0x10: A repeated START condition has been transmitted
            // Send slave address with R/W bit
            TWDR = LCD_ADDRESS << 1;
			DEBUG_PRINT_VERBOSE("Sending address.\n");
            TWCR = I2C_CMD_TRANSMIT; // Clear TWINT, proceed
            break;

        case I2C_MT_SLA_ACK: // 0x18: SLA+W has been transmitted; ACK received
            // Start sending the byte
        case I2C_MT_DATA_ACK: // 0x28: Data byte transmitted; ACK received
            if (!sent) {
				DEBUG_PRINT_VERBOSE("Sending next data byte.\n");
                // Send next data byte
                TWDR = tx_data;
                TWCR = I2C_CMD_TRANSMIT;
				sent = true;
            } else {
                // Transmission complete
                if (send_stop) {
					DEBUG_PRINT_VERBOSE("Sending stop bit.\n");
                    stop_i2c(); // Generates STOP, waits, and sets state to IDLE
                } else {
					DEBUG_PRINT_VERBOSE("Sending repeat start bit.\n");
                    // No STOP requested (e.g., Rep START follows)
                    i2c_reset_state(I2C_IDLE); // Transaction part finished, ready for next step
                    // Need to clear TWINT without starting/stopping anything yet
                    TWCR = I2C_CMD_CLEAR_TWINT;
                }
            }
            break;
			
		case I2C_MT_SLA_NACK:  // 0x20: SLA+W transmitted; NACK received
			DEBUG_PRINT_VERBOSE("Slave address NACK'ed.\n");
            i2c_reset_state(I2C_ERROR_NACK_ADDR);
			if(send_stop) stop_i2c(); // Attempt to release bus
			else TWCR = I2C_CMD_CLEAR_TWINT; // Clear interrupt if no stop needed
            break;
		case I2C_MT_DATA_NACK: // 0x30: Data transmitted; NACK received
			DEBUG_PRINT_VERBOSE("Data NACK'ed.\n");
			i2c_reset_state(I2C_ERROR_NACK_DATA);
			if(send_stop) stop_i2c(); // Attempt to release bus
			else TWCR = I2C_CMD_CLEAR_TWINT; // Clear interrupt if no stop needed
            break;
			
		case I2C_BUS_ERROR:    // 0x00: Bus error due to illegal START or STOP
			DEBUG_PRINT_VERBOSE("I2C Bus error.\n");
			i2c_reset_state(I2C_ERROR_BUS_ERROR);
			// Try to recover by resetting TWI hardware? Sending STOP might be problematic here.
			TWCR = (1 << TWINT) | (1 << TWEN); // Try to just clear flag, keep TWI enabled but no interrupt?
			// Or maybe call init_i2c() again? Requires careful thought.
			// For simplicity, just set error state and clear flag.
			break;
			
			// --- Other Status Codes (Should not happen in Master Mode if logic is correct) ---
		case I2C_NO_INFO:      // 0xF8: No relevant state info; TWINT=0 (shouldn't trigger ISR)
			DEBUG_PRINT_VERBOSE("I2C No info.\n");
		default:
            // Handle unexpected status codes - Treat as an error
			i2c_reset_state(I2C_ERROR_OTHER);
			if(send_stop) stop_i2c(); // Attempt to release bus
			else TWCR = I2C_CMD_CLEAR_TWINT; // Clear interrupt if no stop needed
            break;
    }
}