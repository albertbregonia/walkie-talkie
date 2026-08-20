#include <avr/io.h>
#include <avr/interrupt.h>

#include "atmega4809/spi.h"
#include "atmega4809/adc.h"
#include "atmega4809/sleep.h"
#include "mcp4921/mcp4921.h"

static inline void configure_spi_bus() {
	configure_spi((SPIConfig_t) {
		.spi = &SPI0,
		.master = true,
		.clk_double_speed_enabled = true,
		.client_select_disabled = true,
		// assume defaults for the others
	});
	enable_spi(&SPI0);
	// TODO: !SS should be set in the setup function for the radio
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

static inline void setup(void) {
	// we can only use standby bc the ADC does not support SLEEP_MODE_PWR_DOWN
	configure_sleep(SLEEP_MODE_STANDBY);
	configure_spi_bus();
	configure_mcp4921(&mcp4921);
	configure_adc((ADCConfig_t) {
		.adc = &ADC0,
		.run_standby_enabled = true,
		.resolution = ADC_RESSEL_10BIT_gc,
		.freerun_enabled = true,
		.result_ready_interrupt_enabled = true,
		.pins = ADC_MUXPOS_AIN8_gc,
		.prescalar = ADC_PRESC_DIV2_gc, // 3.333 MHz / 2 prescalar = 1.667 MHz (although datasheet states 1.5MHz maximum @ 10bits)
	});
	enable_adc(&ADC0);
	start_adc_conversion(&ADC0);
}

int main(void) {
	setup();
	sei();
    while(1) {
		// interestingly enough, having the ISR call mcp4921_write is "less performant" (reduces our playback rate)
		// this is because the .lss shows that all the registers need to be pushed to the stack
		// this means a lot of cycles are wasted before the next audio sample is played
		// therefore, we use the interrupt to simply wake the CPU from sleep but use main() to write the sample
		// (assuming mcp4921_write() is always faster than obtaining the next ADC result)
		// 
		// HOWEVER, this is just a test to ensure the ADC and the DAC code works
		// there is no specific timing here (even though there should be)
		// in the current state, we are simply gauging the kind of audio possible (highest bit depth and sample rate @ 3.333MHz)
		if(ADC0.INTFLAGS & ADC_RESRDY_bm) {
			mcp4921_write(&mcp4921, (MCP4921Header_t) {
				.gain_2x_disabled = true,
				.enable = true,
				// ADC.RES is 10-bit, shift once to move closer to 12-bit max
				.value = ADC0.RES << 1,
			});
		}
		sleep_cpu();
	}
}

