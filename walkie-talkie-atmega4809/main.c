#define F_CPU 20000000UL // define F_CPU for use with _delay_ms(), calculations, etc.

#include <avr/io.h>
#include <avr/sleep.h>

#include "atmega4809/spi.h"
#include "mcp4921/mcp4921.h"

static inline void configure_sleep() {
	// we can only use standby bc the ADC does not support SLEEP_MODE_PWR_DOWN
	set_sleep_mode(SLEEP_MODE_STANDBY);
	sleep_enable();
}

uint8_t send_spi(uint8_t value) {
	SPI0.DATA = value;
	while(!(SPI0.INTFLAGS & SPI_IF_bm)); // poll
	return value;
}

const MCP4921_t mcp4921 = {
	.chip_select_pin_bp = PIN3_bp,
	.chip_select_port = &PORTE,
	.send_spi = send_spi,
};

static inline void configure_spi_bus() {
	configure_spi((SPIConfig_t) {
		.spi = &SPI0,
		.master = true,
		.clk_double_speed = true,
		.client_select_disable = true,
		// assume defaults for the others
	});
	enable_spi(&SPI0);
	// TODO: !SS should be set in the setup function for the radio
}

static inline void setup(void) {
	configure_sleep();
	configure_spi_bus();
	configure_mcp4921(&mcp4921);
}

int main(void) {
	setup();
	mcp4921_write(&mcp4921, (MCP4921Header_t) {
		.disable_2x_gain = true,
		.enable = true,
		.value = 2048
	});
	
    while(1) {
		sleep_cpu();
	}
}

