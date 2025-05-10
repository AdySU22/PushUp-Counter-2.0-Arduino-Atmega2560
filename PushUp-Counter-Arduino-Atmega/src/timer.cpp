#include "timer.h"

void init_timer0(void) {
	TCCR0B = 0;
	TCCR0A = 1 << WGM01;
}

void init_timer1(void) {
	TCCR1B = 1 << WGM12;
	TCCR1A = 0;
}

void init_timer3(void) {
	// enable CTC mode
	TCCR3A = 0;
	TCCR3B = (1 << WGM32);
	// enable output compare A interrupt
	TIMSK3 = (1 << OCIE3A);
}

void delay_us_async(uint16_t t_us) {
	OCR3A = t_us << 1;
	TCNT3 = 0;
	TCCR3B |= I2C_TIMER_PRESCALER;
}

void delay_us_sync(uint16_t t_us) {
	OCR1A = t_us << 1;
	TCNT1 = 0;
	TCCR1B |= 1 << CS11;
	while (!(TIFR1 & (1 << OCF1A)));
	TIFR1 |= 1 << OCF1A;
	TCCR1B = 1 << WGM12;
}

void delay_ms(uint16_t t_ms) {
	// set delay
	OCR1A = t_ms << 4;
	TCNT1 = 0;
	// prescaler 1024
	TCCR1B |= (1 << CS12) | (1 << CS10);
	while (!(TIFR1 & (1 << OCF1A)));
	TIFR1 |= 1 << OCF1A;
	TCCR1B = 1 << WGM12;
}

void delay_s(uint16_t t_s) {
	OCR1A = 62500;
	TCNT1 = 0;
	// prescaler 256
	TCCR1B |= 1 << CS12;
	while (t_s > 0) {
		while (!(TIFR1 & (1 << OCF1A)));
		t_s--;
		TIFR1 |= 1 << OCF1A;
	}
	TCCR1B = 1 << WGM12;
}
