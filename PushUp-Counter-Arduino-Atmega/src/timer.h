#ifndef TIMER_H__
#define TIMER_H__

#include "common.h"

#define I2C_TIMER_PRESCALER ((1 << CS31))

void init_timer0(void);
void init_timer1(void);
void init_timer3(void);
void delay_us_sync(uint16_t t_us);
void delay_us_async(uint16_t t_us);
void delay_ms(uint16_t t_ms);
void delay_s(uint16_t t_s);

#endif // TIMER_H__