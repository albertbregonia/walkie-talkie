#define F_CPU 2500000UL
#include <avr/io.h>
#include <avr/interrupt.h>

// ATmega4809 HAL
#include "atmega4809/cpu.h"
#include "atmega4809/spi.h"
#include "atmega4809/adc.h"
#include "atmega4809/sleep.h"
#include "atmega4809/periodic_timer.h"

// peripheral HALs
#include "mcp4921/mcp4921.h"
// IRQ ports need to be defined before the import
#define NRF24L01_IRQ_PORT PORTA
#define NRF24L01_IRQ_PIN_bp PIN2_bp
#include "nrf24L01/nrf24L01.h"

#define PLAYBACK_RATE(AUDIO_SAMPLE_RATE) ((F_CPU / AUDIO_SAMPLE_RATE) - 1) 

static inline void configure_spi_bus() {
	configure_spi((SPIConfig_t) {
		.spi = &SPI0,
		.master = true,
		.clk_double_speed_enabled = true,
		.client_select_disabled = true,
		// assume defaults for the others
	});
	enable_spi(&SPI0);
}

uint8_t send_spi(uint8_t mosi) {
	return send_spi_poll_completion_non_buffer(&SPI0, mosi);
}

// PERIPHERAL CONFIGURATIONS

const MCP4921_t mcp4921 = {
	.chip_select_pin_bp = PIN3_bp,
	.chip_select_port = &PORTE,
	.send_spi = send_spi,
};

const nrf24L01_t nrf = {
	.chip_select_pin_bp = PIN7_bp,
	.chip_select_port = &PORTA,
	.chip_enable_pin_bp = PIN3_bp,
	.chip_enable_port = &PORTA,
	.send_spi = send_spi,
};

static inline void setup(void) {
	set_cpu_prescaler(CLKCTRL_PDIV_8X_gc); // enable 2.5 MHz aka 20 MHz / 8
	// we can only use standby bc the ADC does not support SLEEP_MODE_PWR_DOWN
	configure_sleep(SLEEP_MODE_STANDBY);
	configure_adc((ADCConfig_t) {
		.adc = &ADC0,
		.run_standby_enabled = true,
		.resolution = ADC_RESSEL_10BIT_gc,
		.freerun_enabled = true,
		.result_ready_interrupt_enabled = true,
		.pins = ADC_MUXPOS_AIN8_gc,
		.prescaler = ADC_PRESC_DIV2_gc, // 20 MHz / 8 prescaler = 2.5 MHz (although datasheet states 1.5MHz maximum @ 10bits)
	});
	configure_spi_bus();
	configure_mcp4921(&mcp4921);
	configure_nrf24L01_pins(&nrf);
	configure_periodic_interrupt_timer((PeriodicInterruptTimerConfig_t) {
		.timer = &TCB0,
		.run_standby_enabled = true,
		.max_value = PLAYBACK_RATE(48000),
	});
	enable_tcb(&TCB0);
	
	// TEMP: use event signal system to let CPU continue to sleep until we need to sample
	// timer counter will run at 48kHz and signal the ADC to sample
	// the main will immediately play the sample afterward
	// (basically scales the CPU down to 48kHz accurately)
	EVSYS.CHANNEL0 = EVSYS_GENERATOR_TCB0_CAPT_gc; // TCB signals ADC
	EVSYS.USERADC0 = EVSYS_CHANNEL_CHANNEL0_gc; // ADC accepts signals
	ADC0.EVCTRL |= ADC_STARTEI_bm; // enable event trigger
	
	enable_adc(&ADC0);
}

int main(void) {
	setup();
	uint8_t volatile status = nrf24L01_read_register(&nrf, REGISTER_STATUS);
	asm volatile ("nop"); // breakpoint to read default status register values
	sei();
    while(1) {
		if(ADC0.INTFLAGS & ADC_RESRDY_bm) {
			// NOTE: compiling in debug -Og seems to have a significant performance impact
			// however, compiling in release -Os seems to have the same performance
			// as writing the values directly without the abstraction
			// ive tried to physically optimize around it, may simply decrease prescalers during debug mode to avoid that
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

