#ifndef SPI_H_
#define SPI_H_

#include <avr/io.h>
#include <stdbool.h>

// many of these functions are `static inline` as they are small helpers / zero-cost abstractions
// for example, SPIConfig as a struct on its own is a lot of data to be copied around.
// however, combined with `static inline configure_spi()`, it essentially compiles down to writing constants to registers
// without giving up the high level, easily readable API (zero cost abstraction) 
// (see .lss for proof, may need to compile with -Os or -O3, -Og doesn't always optimize well)

typedef struct SPIConfig {
	SPI_t* const spi;
	// CTRLA register
	const bool data_order_lsb; // bit 6 - DORD
	const bool master; // bit 5 - MASTER
	const bool clk_double_speed; // bit 4 - clock CLK2X
	const SPI_PRESC_t prescalar; // bits 2:1 - PRESC[1:0]
	
	// CLTRB register
	const bool buffer_mode; 
	const bool buffer_mode_wait_for_receive;
	const bool client_select_disable;
	const SPI_MODE_t spi_mode;
	
	// INTCTRL register
	const bool buffer_mode_receive_complete_interrupt; // bit 7 - RXCIE
	const bool buffer_mode_transfer_complete_interrupt; // bit 6 - TXCIE
	const bool buffer_mode_data_register_empty_interrupt; // bit 5 - DREIE
	const bool buffer_mode_client_select_trigger_interrupt; // bit 4 - SSIE
	const bool non_buffer_mode_interrupt; // bit 0 - IE triggered when RXCIF/IF is set in int flags
	
	// TWISPIROUTEA register (PORTMUX)
	const PORTMUX_SPI0_t pins;
} SPIConfig_t;

// === setup functions === //

/// configures the registers of the SPI peripheral with the given configuration
/// does no validation of the configuration - aka you can set buffer mode 
static inline void configure_spi(const SPIConfig_t config) {
	config.spi->CTRLA =
		(config.data_order_lsb << SPI_DORD_bp) |
		(config.master << SPI_MASTER_bp) |
		(config.clk_double_speed << SPI_CLK2X_bp) |
		config.prescalar;

	config.spi->CTRLB =
		(config.buffer_mode << SPI_BUFEN_bp) |
		(config.buffer_mode_wait_for_receive << SPI_BUFWR_bp) |
		(config.client_select_disable << SPI_SSD_bp) |
		config.spi_mode;
	
	config.spi->INTCTRL =
		(config.buffer_mode_receive_complete_interrupt << SPI_RXCIE_bp) |
		(config.buffer_mode_transfer_complete_interrupt << SPI_TXCIE_bp) |
		(config.buffer_mode_data_register_empty_interrupt << SPI_DREIE_bp) |
		(config.buffer_mode_client_select_trigger_interrupt << SPI_SSIE_bp) |
		(config.non_buffer_mode_interrupt << SPI_IE_bp);
		
	PORTMUX.TWISPIROUTEA = (PORTMUX.TWISPIROUTEA & 0b11111100) | (config.pins);
}

static inline void enable_spi(SPI_t* const spi) {
	spi->CTRLA |= SPI_ENABLE_bm;
}

static inline void disable_spi(SPI_t* const spi) {
	spi->CTRLA &= ~SPI_ENABLE_bm;
}




// === interrupt flag abstractions if polling === //

// normal mode
static inline bool has_non_buffer_mode_interrupt(const SPI_t* const spi) {
	return spi->INTFLAGS & SPI_IF_bm;
}

static inline bool has_write_collision(const SPI_t* const spi) {
	return spi->INTFLAGS & SPI_WRCOL_bm;
}

// buffer mode
static inline bool has_buffer_mode_receive_complete_interrupt(const SPI_t* const spi) {
	return spi->INTFLAGS & SPI_RXCIF_bm;
}

static inline bool has_buffer_mode_transfer_complete_interrupt(const SPI_t* const spi) {
	return spi->INTFLAGS & SPI_TXCIF_bm;
}

static inline bool has_buffer_mode_data_register_empty_interrupt(const SPI_t* const spi) {
	return spi->INTFLAGS & SPI_DREIF_bm;
}

static inline bool has_buffer_mode_client_select_trigger_interrupt(const SPI_t* const spi) {
	return spi->INTFLAGS & SPI_SSIF_bm;
}

static inline bool has_buffer_overflow_interrupt(const SPI_t* const spi) {
	return spi->INTFLAGS & SPI_BUFOVF_bm;
}

static inline uint8_t send_spi_poll_completion_non_buffer(SPI_t* const spi, const uint8_t mosi) {
	spi->DATA = mosi;
	while(!has_non_buffer_mode_interrupt(spi)); // wait until completion
	// SPI_IF_bm is auto-cleared by reading SPI0.DATA when the flag is set
	return spi->DATA; // MISO return value
}

#endif /* SPI_H_ */