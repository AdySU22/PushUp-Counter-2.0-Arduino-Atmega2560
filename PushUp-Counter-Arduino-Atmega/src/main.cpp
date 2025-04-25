#include <stdfix.h>

#include "timer.h"
#include "reset_switch.h"
#include "lcd.h"
#include "infrared_sensor.h"
#include "touch_sensor.h"
#include "spi.h"

#define RESET_SWITCH_BEFORE_PRESS 0
#define RESET_SWITCH_PRESSED 1
#define RESET_SWITCH_RELEASED 2
#define RESET_SWITCH_AFTER_RELEASE 3

// Arms straight
#define UP_POSITION 0
// Downward movement
#define MOVING_DOWN 1
// Arms bent
#define DOWN_POSITION 2 
// Upward movement
#define MOVING_UP 3

volatile uint8_t switch_state = RESET_SWITCH_BEFORE_PRESS, ir_sensor_state = UP_POSITION; 
volatile bool touch_sensors_triggered = 0;
volatile uint16_t push_up_count = 0;

int16_t main(void) {
	init_timer0();
	init_timer1();
	init_timer3();
	init_reset_switch_pin();
	init_touch_sensor_pins();
	init_infrared_sensor_pin();
	init_spi(SPI_ASYNC);
	init_i2c();
	sei();
	init_lcd();

	while (1) {
		switch (switch_state) {
			case RESET_SWITCH_PRESSED: {
				switch_state = RESET_SWITCH_RELEASED;
			} break;
			case RESET_SWITCH_AFTER_RELEASE: {
				switch_state = RESET_SWITCH_BEFORE_PRESS;
				// TODO: SEND DATA TO SERVER
				clear_screen();
				write_str("Resetting...");
				delay_s(5);
				clear_screen();
				write_str("Ready!");
				delay_s(3);
				clear_screen();
			} break;
			default:
				break;
		}
		switch (ir_sensor_state) {
			case MOVING_DOWN: {
				ir_sensor_state = DOWN_POSITION;
			} break;
			case MOVING_UP: {
				ir_sensor_state = UP_POSITION;
				move_cursor(1, 0);
				write_char('0');
				write_char('0');
			} break;
			default:
				break;
		}
		// write_str("HELLO FROM I2C");
		// delay_s(2);
		// clear_screen();
		// move_cursor(1, 0);
		// write_str("HELLO FROM I2C");
		// delay_s(2);
		// move_cursor(0, 0);
		// clear_screen();
	}
	return 0;
}

ISR(RESET_SWITCH_PCINT_VECTOR) {
	switch (switch_state) {
		case RESET_SWITCH_BEFORE_PRESS: {
			switch_state = RESET_SWITCH_PRESSED;
		} break;
		case RESET_SWITCH_RELEASED: {
			switch_state = RESET_SWITCH_BEFORE_PRESS;
		} break;
		default: break;
	}
}

ISR(INFRARED_SENSOR_PCINT_VECTOR) {
	switch (ir_sensor_state) {
		case UP_POSITION: {
			ir_sensor_state = MOVING_DOWN;
		} break;
		case DOWN_POSITION: {
			ir_sensor_state = MOVING_UP;
		} break;
		default: break;
	}
}

#ifdef TOUCH_SENSORS_PCIE_NUMBER
ISR(TOUCH_SENSORS_PCINT_VECTOR) {
	touch_sensors_triggered = (TOUCH_SENSORS_PORT & TOUCH_SENSORS_PIN_MASK) == TOUCH_SENSORS_PIN_MASK;
}
#else
#warning "Don't forget to write an ISR for the touch sensors."
#endif // TOUCH_SENSORS_PCIE_NUMBER