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
	const bool clk_double_speed_enabled; // bit 4 - clock CLK2X
	const SPI_PRESC_t prescaler; // bits 2:1 - PRESC[1:0]
	
	// CLTRB register
	const bool buffer_mode_enabled; 
	const bool buffer_mode_wait_for_receive_enabled;
	const bool client_select_disabled;
	const SPI_MODE_t spi_mode;
	
	// INTCTRL register
	const bool buffer_mode_receive_complete_interrupt_enabled; // bit 7 - RXCIE
	const bool buffer_mode_transfer_complete_interrupt_enabled; // bit 6 - TXCIE
	const bool buffer_mode_data_register_empty_interrupt_enabled; // bit 5 - DREIE
	const bool buffer_mode_client_select_trigger_interrupt_enabled; // bit 4 - SSIE
	const bool non_buffer_mode_interrupt_enabled; // bit 0 - IE triggered when RXCIF/IF is set in int flags
	
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
		(config.clk_double_speed_enabled << SPI_CLK2X_bp) |
		config.prescaler;

	config.spi->CTRLB =
		(config.buffer_mode_enabled << SPI_BUFEN_bp) |
		(config.buffer_mode_wait_for_receive_enabled << SPI_BUFWR_bp) |
		(config.client_select_disabled << SPI_SSD_bp) |
		config.spi_mode;
	
	config.spi->INTCTRL =
		(config.buffer_mode_receive_complete_interrupt_enabled << SPI_RXCIE_bp) |
		(config.buffer_mode_transfer_complete_interrupt_enabled << SPI_TXCIE_bp) |
		(config.buffer_mode_data_register_empty_interrupt_enabled << SPI_DREIE_bp) |
		(config.buffer_mode_client_select_trigger_interrupt_enabled << SPI_SSIE_bp) |
		(config.non_buffer_mode_interrupt_enabled << SPI_IE_bp);
		
	// configure SPI pins based on PORTMUX group config
	PORTMUX.TWISPIROUTEA = (PORTMUX.TWISPIROUTEA & 0b11111100) | (config.pins);
	
	switch(config.pins) {
		case PORTMUX_SPI0_DEFAULT_gc: // PA[7:4]
			PORTA.DIRSET = PIN6_bm | PIN4_bm; // SCK, MOSI - outputs
			PORTA.DIRCLR = PIN5_bm; // MISO - input
			if(!config.client_select_disabled) { // multi-host support
				PORTA.DIRCLR = PIN7_bm;
				PORTA.PIN7CTRL |= PORT_PULLUPEN_bm;
			} 
			// datasheet says that if client_select_disable=false and SS is an input
			// that's the only way SS will be used by SPI for multi-host support
			// otherwise, it's regular GPIO
			break;
		case PORTMUX_SPI0_ALT1_gc: // PC[3:0]
			PORTC.DIRSET = PIN2_bm | PIN0_bm; // SCK, MOSI - outputs
			PORTC.DIRCLR = PIN1_bm; // MISO - input
			if(!config.client_select_disabled) { // multi-host support
				PORTC.DIRCLR = PIN3_bm;
				PORTC.PIN3CTRL |= PORT_PULLUPEN_bm;
			}
			break;
		case PORTMUX_SPI0_ALT2_gc: // PE[3:0]
			PORTE.DIRSET = PIN2_bm | PIN0_bm; // SCK, MOSI - outputs
			PORTE.DIRCLR = PIN1_bm; // MISO - input
			if(!config.client_select_disabled) { // multi-host support
				PORTE.DIRCLR = PIN3_bm;
				PORTE.PIN3CTRL |= PORT_PULLUPEN_bm;
			}
			break;
		case PORTMUX_SPI0_NONE_gc:
		default: break; // do nothing, exhaustive
	}
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