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

volatile uint8_t ir_sensor_state = UP_POSITION; 
volatile bool reset_triggered = false;
volatile uint16_t push_up_count = 0;

char buf[8] = {};

void toggle_interrupts(void) {
	// toggle_touch_sensor();
	toggle_infrared_sensor();
	toggle_reset_switch();
}

int16_t main(void) {
	DEBUG_INIT(9600);
	init_timer0();
	init_timer1();
	init_timer3();
	init_touch_sensor_pins();
	init_spi(SPI_ASYNC);
	init_i2c();
	sei();
	init_lcd();
	write_str("Press both ");
	write_str("buttons to start");
	while (TOUCH_SENSORS_PINS & TOUCH_SENSORS_PIN_MASK) {}
	init_reset_switch_pin();
	init_infrared_sensor_pin();
	clear_screen();
	write_str("Starting...");
	delay_s(5);
	clear_screen();
	write_str("Ready!");
	delay_s(1);
	move_cursor(0, 0);
	write_str("Push-ups done:");
	while (1) {
		DEBUG_PRINT("Touch sensors active: ");
		DEBUG_PRINT(TOUCH_SENSORS_PINS & TOUCH_SENSORS_PIN_MASK);
		DEBUG_PRINT(", Reset switch pressed: ");
		DEBUG_PRINT(reset_triggered);
		DEBUG_PRINT(", Infrared sensor active: ");
		DEBUG_PRINTLN(PINK & INFRARED_SENSOR_PIN_MASK);
		continue;
		switch (ir_sensor_state) {
			case MOVING_DOWN: {
				ir_sensor_state = DOWN_POSITION;
				delay_us_sync(10);
			} break;
			case MOVING_UP: {
				#ifdef TOUCH_SENSORS_DDR
				// Active high / low
				push_up_count += (TOUCH_SENSORS_PINS & TOUCH_SENSORS_PIN_MASK) == 0;
				#else 
				push_up_count += !(LEFT_TOUCH_SENSORS_PIN & LEFT_TOUCH_SENSOR_PIN_MASK) && !(RIGHT_TOUCH_SENSOR_PIN & RIGHT_TOUCH_SENSOR_PIN_MASK);
				#endif
				ir_sensor_state = UP_POSITION;
				move_cursor(1, 0);
				snprintf(buf, 8, "%u", push_up_count);
				write_str(buf);
			} break;
			default:
				break;
		}
		if (reset_triggered) {
			toggle_interrupts();
			reset_triggered = false;
			clear_screen();
			write_str("Resetting...");
			delay_s(1);
			int res = master_transmit_async((const uint8_t*)&push_up_count, sizeof(int16_t));
			clear_screen();
			write_str(res ? "Ready!" : "Send failed.");
			delay_s(1);
			clear_screen();
			toggle_interrupts();
		}
	}
	return 0;
}

ISR(RESET_SWITCH_PCINT_VECTOR) {
	reset_triggered = RESET_SWITCH_PIN & RESET_SWITCH_PIN_MASK;
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

#ifdef TOUCH_SENSOR_ISR
#ifdef TOUCH_SENSORS_PCIE_NUMBER
ISR(TOUCH_SENSORS_PCINT_VECTOR) {

}
#else
#warning "Don't forget to write an ISR for the touch sensors."
#endif // TOUCH_SENSORS_PCIE_NUMBER
#endif // TOUCH_SENSOR_ISR