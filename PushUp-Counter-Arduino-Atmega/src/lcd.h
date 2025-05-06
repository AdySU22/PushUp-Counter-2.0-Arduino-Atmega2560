#ifndef LCD_H__ 
#define LCD_H__

#include "common.h"
#include "i2c.h"
#include "timer.h"
#include <avr/interrupt.h>

#define LCD_REGISTER_SELECT_BIT_MASK 0x1
#define LCD_WRITE_READ_BIT_MASK 0x2
#define LCD_ENABLE_BIT_MASK 0x4
#define LCD_BACKLIGHT_BIT_MASK 0x8

void init_lcd(void);
uint8_t send_bits(uint8_t data, uint8_t flags);
uint8_t move_cursor(bool y, uint8_t x);
uint8_t write_char(uint8_t character);
uint8_t send_command(uint8_t command);
uint8_t write_str(const char* str);
uint8_t clear_screen(void);
uint8_t configure_display(const bool display_on, const bool cursor_on, const bool blink_on);

extern bool lcd_initialized;

#endif
