#ifndef NRF24L01_H_
#define NRF24L01_H_

#include <util/delay.h>
#include "constants.h"

// helper macros
#define PINnCTRL_HELPER(pin) PIN##pin##CTRL // allows us to evalute macros first then do text replacement (not to be used externally)
#define SET_PINnCTRL(port, pin, gc) ((port).PINnCTRL_HELPER(pin) = (gc))

typedef struct nrf24L01 {
	const uint8_t chip_select_pin_bp;
	PORT_t* const chip_select_port;
	const uint8_t chip_enable_pin_bp;
	PORT_t* const chip_enable_port;
	uint8_t (*const send_spi)(uint8_t);
} nrf24L01_t;

// nrf24l01 is active low therefore clear and set are inverted
// api level: if we want to "select" => true/false
static inline void nrf24L01_select(const nrf24L01_t* const nrf, const bool active) {
	const uint8_t mask = (1 << nrf->chip_select_pin_bp);
	if(active) {
		nrf->chip_select_port->OUT &= ~mask;
	} else {
		nrf->chip_select_port->OUT |= mask;
	}
}

// this is active high
static inline void nrf24L01_chip_enable(const nrf24L01_t* const nrf, bool active) {
	const uint8_t mask = (1 << nrf->chip_enable_pin_bp);
	if(active) {
		nrf->chip_enable_port->OUT |= mask;
	} else {
		nrf->chip_enable_port->OUT &= ~mask;
	}
}

static inline void configure_nrf24L01_pins(const nrf24L01_t* const nrf) {
	// set CSN as output
	nrf->chip_select_port->DIR |= (1 << nrf->chip_select_pin_bp);
	nrf24L01_select(nrf, false);
	
	// set CE as output
	nrf->chip_enable_port->DIR |= (1 << nrf->chip_enable_pin_bp);
	nrf24L01_chip_enable(nrf, false);

	#if defined(NRF24L01_IRQ_PORT) && defined(NRF24L01_IRQ_PIN_bp)
		// falling as IRQ is active low
		// by default all interrupts sources are enabled on the nrf24L01
		// we use macros here instead of the struct bc it is optional
		NRF24L01_IRQ_PORT.DIR &= ~(1 << NRF24L01_IRQ_PIN_bp);
		SET_PINnCTRL(NRF24L01_IRQ_PORT, NRF24L01_IRQ_PIN_bp, PORT_ISC_FALLING_gc);
	#endif
	nrf24L01_chip_enable(nrf, false);
	
	// wait 10.3ms for the radio module to start (power on reset)
	_delay_ms(10.3);
}

static inline uint8_t nrf24L01_read_register(const nrf24L01_t* const nrf, uint8_t address) {
	nrf24L01_select(nrf, true);
	nrf->send_spi(CMD_R_REGISTER(address));
	uint8_t miso = nrf->send_spi(CMD_NOP);
	nrf24L01_select(nrf, false);
	return miso;
}

#endif /* NRF24L01_H_ */