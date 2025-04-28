#include "reset_switch.h"

void init_reset_switch_pin(void) {
    RESET_SWITCH_DDR &= ~RESET_SWITCH_PIN_MASK;
	// Enable pull down resistor
	RESET_SWITCH_PORT &= ~RESET_SWITCH_PIN_MASK;
	PCICR |= RESET_SWITCH_PCIE_MASK;
	RESET_SWITCH_PCMSK |= RESET_SWITCH_PCINT_MASK;
}