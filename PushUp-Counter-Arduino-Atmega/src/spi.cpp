#include "spi.h"

// State variables
static volatile uint8_t tx_buffer[BUFFER_LENGTH], tx_buffer_start, tx_buffer_end;
static volatile uint8_t *rx_buffer, rx_buffer_start, rx_buffer_end;
volatile uint8_t spi_status = SPI_STATE_UNINITIALIZED;

#define SPI_DUMMY_BYTE 0

void init_spi(const bool async_mode) {
	// Set MOSI pin as output and MISO pin as input
	SPI_DDR = (SPI_DDR & ~SPI_MISO_PIN_MASK) | (SPI_MOSI_PIN_MASK) | (SPI_SCK_PIN_MASK) | (SPI_CS_PIN_MASK);
	SPI_PORT |= (SPI_CS_PIN_MASK);
	// Enable SPI, Master, set clock rate fck/16
	SPCR = (1 << MSTR) | (1 << SPR0);
	SPCR |= (1 << SPE) | (async_mode << SPIE);
	spi_status = SPI_STATE_IDLE_SYNC | async_mode;
}

bool set_spi_async(const bool set_async) {
	// Only allow changing mode when idle
    if (spi_status & SPI_STATE_BUS_ACTIVE_BIT_MASK) {
        return false; // Cannot change mode while busy
    }
	uint8_t current_spcr = SPCR;
    if (set_async) {
        current_spcr |= (1 << SPIE);
        spi_status = SPI_STATE_IDLE_ASYNC;
    } else {
        current_spcr &= ~(1 << SPIE);
        spi_status = SPI_STATE_IDLE_SYNC;
    }
    SPCR = current_spcr;
    return true;
}

bool master_transmit_sync(const uint8_t *data, const uint8_t len) {
	if (spi_status != SPI_STATE_IDLE_SYNC) return false;
	spi_status = SPI_STATE_BUSY_SYNC_TX;
	SPI_PORT &= ~SPI_CS_PIN_MASK;
	for (uint8_t i = 0; i < len; i++) {
		SPDR = data[i];
		// Poll for every byte
		while (!(SPSR & (1 << SPIF))) {}
		volatile uint8_t temp __attribute__((unused)) = SPSR; // Read SPSR first per datasheet
        temp = SPDR; // Then read SPDR to clear SPIF
	}
	SPI_PORT |= SPI_CS_PIN_MASK;
	return true;
}

bool master_transmit_async(const uint8_t *data, const uint8_t len) {
	if (spi_status != SPI_STATE_IDLE_ASYNC) return false;
	spi_status = SPI_STATE_BUSY_ASYNC_TX;
	
	for (uint8_t i = 0; i < len; ++i) {
		tx_buffer[tx_buffer_end++] = data[i];
	}
	
	SPI_PORT &= ~SPI_CS_PIN_MASK;
	SPDR = tx_buffer[tx_buffer_start++];
	return true;
}

bool master_receive_sync(uint8_t *data, const uint8_t len) {
	if (spi_status != SPI_STATE_IDLE_SYNC) return false;
	spi_status = SPI_STATE_BUSY_SYNC_RX;
	SPI_PORT &= ~SPI_CS_PIN_MASK;

	for (uint8_t i = 0; i < len; i++) {
		// Wait for byte transfer
		SPDR = SPI_DUMMY_BYTE;
		while(!(SPSR & (1 << SPIF))) {};
		volatile uint8_t tmp __attribute__((unused)) = SPSR;
		data[i] = SPDR;
	}

	SPI_PORT |= SPI_CS_PIN_MASK;
	spi_status = SPI_STATE_IDLE_SYNC;
	return true;
}

bool master_receive_async(volatile uint8_t *data, const uint8_t len) {
	if (spi_status != SPI_STATE_IDLE_ASYNC) return false;
	spi_status = SPI_STATE_BUSY_ASYNC_RX;
	
	rx_buffer = data;
	rx_buffer_end = len;
	rx_buffer_start = 0;
	SPI_PORT &= ~SPI_CS_PIN_MASK;
	SPDR = SPI_DUMMY_BYTE;
	// TODO: signal wifi module to reply
	return true;
}

// SPI Transfer complete interrupt / handler
ISR(SPI_STC_vect) {
	volatile uint8_t tmp __attribute__((unused)) = SPSR;
	uint8_t recv_byte = SPDR;
	switch (spi_status) {
		case SPI_STATE_BUSY_ASYNC_TX: {
			if (tx_buffer_start != tx_buffer_end) {
				SPDR = tx_buffer[tx_buffer_start++];
			} else {
				// disable SPI on slave
				SPI_PORT |= SPI_CS_PIN_MASK; 
				// Signal completion and reset status to idle
                spi_status = SPI_STATE_IDLE_ASYNC;   
			}
		} break;
		case SPI_STATE_BUSY_ASYNC_RX: {
			if (rx_buffer_start != rx_buffer_end) {
				rx_buffer[rx_buffer_start++] = recv_byte;
			} 
			if (rx_buffer_start != rx_buffer_end) {
				// Send next dummy byte to clock in next byte
				SPDR = SPI_DUMMY_BYTE; 
			} else {
				// disable SPI on slave
				SPI_PORT |= SPI_CS_PIN_MASK;
				// Signal completion and reset status to idle
				spi_status = SPI_STATE_IDLE_ASYNC;  
			}
		} break; 
		default:
			SPI_PORT |= SPI_CS_PIN_MASK;
			DEBUG_PRINT("Got error");
			break;
	}
}