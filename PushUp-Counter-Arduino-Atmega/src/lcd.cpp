#include "lcd.h" 

bool lcd_initialized = false; 

// Function to send a byte via I2C to the LCD backpack
// This encapsulates the interaction with the PCF8574
uint8_t send_bits(uint8_t data, uint8_t flags) {
    // Wait if I2C is busy from a previous operation
    while (i2c_is_busy()) {}
    // Handle previous errors if necessary
    if(i2c_status != I2C_IDLE) {
        // Log error, try to re-init I2C, etc.
        init_i2c(); // Attempt re-initialization
		delay_ms(10);
		if(i2c_status != I2C_IDLE) {
			lcd_initialized = false;
			return 3;
		} // Give up if re-init failed
		if(!lcd_initialized) return -1;
    }

    // Set the data pins and control pins (RS, RW, EN, Backlight)
    // PCF8574 only needs one byte at a time
    uint8_t packet = data | flags | LCD_BACKLIGHT_BIT_MASK; // Add flags (RS) and keep backlight on

    // Send the data byte (Enable low)
    if (!send_byte(packet, false)) {
		lcd_initialized = false;
		return 4;
	}; // Don't send stop yet
    while (i2c_is_busy()) {} // Wait for completion
    delay_us_sync(1); // Short delay

    // Pulse Enable High
    packet |= LCD_ENABLE_BIT_MASK;
    if (!send_byte(packet, false)) {
		lcd_initialized = false;
		return 5;
	}; // Don't send stop yet
    while (i2c_is_busy()) {}
    delay_us_sync(1); // Enable pulse width

    // Pulse Enable Low
    packet &= ~LCD_ENABLE_BIT_MASK;
    if (!send_byte(packet, true)) {
		lcd_initialized = false;
		return 6;
	}; // Send stop after this final part
    while (i2c_is_busy()) {}
	delay_us_sync(50); // Delay after command/data write
	return 0;
}

// Send a command byte to the LCD
uint8_t send_command(uint8_t cmd) {
	if (!lcd_initialized) return -1;
    uint8_t high = cmd & 0xF0, low = (cmd << 4) & 0xF0;
    if (send_bits(high, 0)) return 1; // RS=1 for data
    if (send_bits(low, 0)) return 2; 
	return 0;
}

// Send a data byte (character) to the LCD
uint8_t write_char(uint8_t data) {
	if (!lcd_initialized) return -1;
    uint8_t high = data & 0xF0, low = (data << 4) & 0xF0;
    if (send_bits(high, LCD_REGISTER_SELECT_BIT_MASK)) return 1; // RS=1 for data
    if (send_bits(low, LCD_REGISTER_SELECT_BIT_MASK)) return 2; // RS=1 for data
	return 0;
}

// Initialize the LCD in 4-bit mode via I2C
void init_lcd(void) {
	lcd_initialized = true;
    delay_ms(50); // Wait for LCD power up

    // Need to send initialization sequence for 4-bit mode
    send_bits(0x30, 0); // Function set (8-bit mode) - Step 1
    delay_ms(5);
    send_bits(0x30, 0); // Function set (8-bit mode) - Step 2
    delay_us_sync(100);
    send_bits(0x30, 0); // Function set (8-bit mode) - Step 3
    delay_us_sync(100);
    send_bits(0x20, 0); // Function set (4-bit mode)
    delay_us_sync(100);

    send_command(0x28); // Function Set: 4-bit, 2 lines, 5x8 font
    send_command(0x8); // Display OFF, Cursor OFF, Blink OFF
    send_command(0x1); // Clear Display
    delay_ms(2);           // Clear display takes longer
    send_command(0x6); // Entry Mode Set: Increment cursor, no shift
    send_command(0xF); // Display ON, Cursor ON, Blink ON
}

uint8_t move_cursor(bool y, uint8_t x) {
    return send_command((0x80 | x | y << 6));
}

uint8_t write_str(const char* str) {
    uint8_t res;
	while (*str) {
		DEBUG_PRINT_VERBOSE("res: ");
        res = write_char(*str++);
		DEBUG_PRINTLN_VERBOSE(res);
		if (res) break;
    }
	return res;
}

uint8_t clear_screen(void) {
	uint8_t res = send_command(0x1);
	if (!res) delay_us_sync(1640);
	return res;
}

uint8_t configure_display(const bool display_on, const bool cursor_on, const bool blink_on) {
	return send_command(0x8 | (display_on << 2) | (cursor_on << 1) | (blink_on << 0));
}