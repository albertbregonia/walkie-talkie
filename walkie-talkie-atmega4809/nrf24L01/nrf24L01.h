#ifndef NRF24L01_H_
#define NRF24L01_H_

#include <util/delay.h>
#include <stdlib.h> // for size_t
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

typedef struct nrf24L01ClearStatusHeader {
	const bool clear_data_ready_interrupt;
	const bool clear_data_sent_interrupt;
	const bool clear_max_retransmit_interrupt;
} nrf24L01ClearStatusHeader_t;

const nrf24L01ClearStatusHeader_t CLEAR_ALL_HEADER = {
	.clear_data_ready_interrupt = true,
	.clear_data_sent_interrupt = true,
	.clear_max_retransmit_interrupt = true,
};

static inline uint8_t nrf24L01_write_register(const nrf24L01_t* const nrf, uint8_t address, uint8_t value) {
	nrf24L01_select(nrf, true);
	nrf->send_spi(CMD_W_REGISTER(address));
	uint8_t miso = nrf->send_spi(value);
	nrf24L01_select(nrf, false);
	return miso;
}

static inline uint8_t nrf24L01_clear_irq(const nrf24L01_t* const nrf, nrf24L01ClearStatusHeader_t header) {
	nrf24L01_select(nrf, true);
	uint8_t status = nrf->send_spi(CMD_W_REGISTER(REGISTER_STATUS));
	nrf->send_spi( 
		(status & 0x0F) | // mask to keep original 
		// use the booleans to set specific flags to clear (1 to clear)
		(header.clear_data_ready_interrupt << STATUS_RX_DR_bp) | 
		(header.clear_data_sent_interrupt << STATUS_TX_DS_bp) |
		(header.clear_max_retransmit_interrupt << STATUS_MAX_RT_bp)
	);
	nrf24L01_select(nrf, false);
	return status;
}

typedef struct nrf24L01Config {
	const bool data_ready_interrupt_disabled;
	const bool data_sent_interrupt_disabled;
	const bool max_retransmit_interrupt_disabled;
	const bool crc_disabled;
	const bool crc_2byte_length;
	const bool power_up;
	const bool subscriber;
	const bool stream;
} nrf24L01Config_t;

static inline uint8_t nrf24L01Config_to_byte(const nrf24L01Config_t config) {
	return (
		(0 << 7) | // reserved, must be 0
		(config.data_ready_interrupt_disabled << CONFIG_MASK_RX_DR_bp) |
		(config.data_sent_interrupt_disabled << CONFIG_MASK_TX_DS_bp) |
		(config.max_retransmit_interrupt_disabled << CONFIG_MASK_MAX_RT_bp) |
		((!config.crc_disabled) << CONFIG_EN_CRC_bp) | // default 1, so we invert to keep the API omit if default
		(config.crc_2byte_length << CONFIG_CRCO_bp) |
		(config.power_up << CONFIG_PWR_UP_bp) |
		(config.subscriber << CONFIG_PRIM_RX_bp)
	);
}

// NOT supposed to be publicly used
static inline void nrf24L01_wait_for_power_up() {
	_delay_ms(1.5); // datasheet spec
}

// NOTE: does not set PRIM_RX in the CONFIG register
// simply handles everything AFTER the config register is set
// NOT supposed to be publicly used (but F_CPU requires it being here to use _delay_us())
static inline void nrf24L01_handle_mode_transition(
	const nrf24L01_t* const nrf,
	const bool subscriber,
	const bool stream
) {
	nrf24L01_chip_enable(nrf, true); // readers and streaming writers we hold CE=1
	
	// if we are a writer but not streaming, we only need to pulse CE
	if(!subscriber && !stream) {
		// datasheet says that CE=1 for 10+ us will transition into TX mode
		// it will take 10us + 130us to settle and start sending
		// however, we can start filling up TX FIFO in the meantime
		// therefore we only delay 11us to guarantee TX mode before returning
		_delay_us(11);
		nrf24L01_chip_enable(nrf, false);
	}
}

// this function is a thin abstraction over the CONFIG register
// it should be used in most cases when changing configuration is desired
// other functions that interact with the configuration register
// will also use the nrf24L01Config_t type to optimize writing to the register
static inline void configure_nrf24L01(const nrf24L01_t* const nrf, const nrf24L01Config_t config) {
	nrf24L01_chip_enable(nrf, false); // go standby to configure
	nrf24L01_clear_irq(nrf, CLEAR_ALL_HEADER); // basic software reset
	nrf24L01_write_register(nrf, REGISTER_CONFIG, nrf24L01Config_to_byte(config));
	
	// extra operations that need to be executed based on config
	if(config.power_up) {
		nrf24L01_wait_for_power_up();
		nrf24L01_handle_mode_transition(nrf, config.subscriber, config.stream);
	}
}

void nrf24L01_stream_packet(
	const nrf24L01_t* const nrf, 
	const size_t packet_size, 
	const uint8_t data[packet_size]
) {
	nrf24L01_select(nrf, true);
	nrf->send_spi(CMD_W_TX_PAYLOAD);
	for(size_t i=0; i<packet_size; i++) {
		nrf->send_spi(data[i]);
	}
	nrf24L01_select(nrf, false);
}

void nrf24L01_send_packet(
	const nrf24L01_t* const nrf, 
	const size_t packet_size, 
	const uint8_t data[packet_size]
) {
	nrf24L01_stream_packet(nrf, packet_size, data);
	// pulse CE=1 for 10+ us according to datasheet
	nrf24L01_chip_enable(nrf, true);
	_delay_us(11);
	nrf24L01_chip_enable(nrf, false); // go back to standby
}

#endif /* NRF24L01_H_ */