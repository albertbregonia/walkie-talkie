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
	configure_spi_bus();
	configure_mcp4921(&mcp4921);
	configure_nrf24L01_pins(&nrf);
// 	configure_periodic_interrupt_timer((PeriodicInterruptTimerConfig_t) {
// 		.timer = &TCB0,
// 		.run_standby_enabled = true,
// 		.max_value = PLAYBACK_RATE(48000),
// 	});
// 	configure_adc((ADCConfig_t) {
// 		.adc = &ADC0,
// 		.run_standby_enabled = true,
// 		.resolution = ADC_RESSEL_10BIT_gc,
// 		.freerun_enabled = true,
// 		.result_ready_interrupt_enabled = true,
// 		.pins = ADC_MUXPOS_AIN8_gc,
// 		.prescaler = ADC_PRESC_DIV2_gc, // 2.5 MHz / 2 = 1.25 MHz
// 	});
//	enable_tcb(&TCB0);
//	enable_adc(&ADC0);
}

ISR(PORTA_PORT_vect) {
	cli();
	PORTA.INTFLAGS |= (1 << NRF24L01_IRQ_PIN_bp); // clear interrupt flag
	nrf24L01_clear_irq(&nrf, CLEAR_ALL_HEADER);
	// blink LED upon packet sent
	PORTF.OUTCLR = PIN5_bm;
	_delay_ms(50);
	PORTF.OUTSET = PIN5_bm;
	_delay_ms(50);
	sei();
}

int main(void) {
	setup();
	PORTF.DIR = PIN5_bm; // built-in LED
	PORTF.OUTSET = PIN5_bm; // turn off LED, connected to pullup

	const bool stream = true; // NOTE: change this value to test both streaming and burst send
	configure_nrf24L01(&nrf, (nrf24L01Config_t) {
		.stream = stream, // default TX
		.power_up = true,
	});
	uint8_t data[] = {0xAB, 0xCD}; // 2 bytes
	sei();
    while(1) {
		if(stream) {
			nrf24L01_stream_packet(&nrf, 2, data);	
		} else {
			nrf24L01_send_packet(&nrf, 2, data);
		}
		sleep_cpu();
	}
}

