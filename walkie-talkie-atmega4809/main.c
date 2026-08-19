#define F_CPU 20000000UL // define F_CPU for use with _delay_ms(), calculations, etc.
#include <avr/io.h>
#include <avr/sleep.h>
#include "spi/spi.h"

static inline void configure_sleep() {
	// we can only use standby bc the ADC does not support power down
	set_sleep_mode(SLEEP_MODE_STANDBY);
	sleep_enable();
}

static inline void configure_spi_bus() {
	configure_spi((SPIConfig_t) {
		.spi = &SPI0,
		.master = true,
		.clk_double_speed = true,
		.client_select_disable = true,
		.pins = PORTMUX_SPI0_DEFAULT_gc, // !SS, SCK, MISO, MOSI, PA[7:4] respectively
		// assume defaults for the others
	});
	enable_spi(&SPI0);
	
	// !SS, SCK, MOSI respectively, 
	// technically !SS is the default for when the mcu is client but we use it as host
	PORTA.DIRSET = PIN7_bm | PIN6_bm | PIN4_bm;
	PORTA.DIRCLR = PIN5_bm; // MISO
	
	// TODO: !SS should be set in the setup function for the radio
}

static inline void setup(void) {
	configure_sleep();
	configure_spi_bus();
}

int main(void) {
	setup();
	
    while(1) {
		sleep_cpu();
	}
}

