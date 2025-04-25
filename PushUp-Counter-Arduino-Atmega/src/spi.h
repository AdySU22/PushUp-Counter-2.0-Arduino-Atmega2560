#ifndef SPI_H__
#define SPI_H__

#include "common.h"
#include "timer.h"
#include <avr/interrupt.h>

#define SPI_SYNC 0
#define SPI_ASYNC 1

#define SPI_STATE_SYNC_BIT_MASK 		0x1 // SPI transaction mode (0 = sync, 1 = async)
#define SPI_STATE_BUS_ACTIVE_BIT_MASK 	0x2 // SPI bus is active/sending (0 = idle, 1 = busy)
#define SPI_STATE_DATA_DIR_BIT_MASK 	0x4 // SPI data direction (0 = tx, 1 = rx)

#define SPI_STATE_UNINITIALIZED     	0xFF	// Uninitialized
#define SPI_STATE_IDLE_SYNC     		0x0		// Idle, ready for synchronous operations
#define SPI_STATE_IDLE_ASYNC    		0x1 	// Idle, ready for asynchronous operations (ISR enabled)
#define SPI_STATE_BUSY_SYNC_TX   		0x2		// Busy with synchronous transmit (polling)
#define SPI_STATE_BUSY_SYNC_RX   		0x6		// Busy with synchronous receive (polling)
#define SPI_STATE_BUSY_ASYNC_TX 		0x3		// Busy with asynchronous transmit (ISR active)
#define SPI_STATE_BUSY_ASYNC_RX 		0x7		// Busy with asynchronous receive (ISR active)

extern volatile uint8_t spi_status;
#define BUFFER_LENGTH 256

void init_spi(const bool async);

void set_async_spi(const bool set_async);

bool master_transmit_sync(const uint8_t *data, const uint8_t len);
bool master_transmit_async(const uint8_t *data, const uint8_t len);
bool master_receive_sync(volatile uint8_t *data, const uint8_t len);
bool master_receive_async(volatile uint8_t *data, const uint8_t len);

#endif //SPI_H__