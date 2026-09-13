#ifndef USART_H_
#define USART_H_

#ifndef F_CPU
#error "F_CPU must be defined to calculate USART3's baud rate register value"
#endif

#define SAMPLES_PER_BIT_ASYNC_NORMAL 16
#define SAMPLES_PER_BIT_ASYNC_DOUBLE 8

#include <avr/io.h>
#include <stdbool.h>

// NOT a full USART HAL for the ATmega4809
// simply util functions to output on USART3 for debugging peripherals
// (aka things not supported by Microchip Studio's debugger)

typedef struct USARTConfig {
    USART_t* const usart;
    
    // CTRLA
    const bool receive_complete_interrupt_enabled; // RXCIE
    const bool transmit_complete_interrupt_enabled; // TXCIE
    const bool data_register_empty_interrupt_enabled; // DREIE
    const bool receive_start_frame_interrupt_enabled; // RXSIE
    const bool loopback_mode_enabled; // LBME
    const bool autobaud_error_interrupt_enabled; // ABEIE
    
    // CTRLB
    const bool receiver_enabled; // RXEN
    const bool transmitter_enabled; // TXEN
    const bool start_frame_detection_enabled; // SFDEN
    const bool open_drain_mode_enabled; // ODME
    const USART_RXMODE_t receiver_mode; // RXMODE
    const bool multiprocessor_communication_mode_enabled; // MPCM
    
    // CTRLC
    // MSPI/IRCOM cannot be used here, those will use separate configuration structs / functions
    const bool synchronous; // CMODE 
    const USART_PMODE_t parity_mode; // PMODE
    const USART_SBMODE_t stop_bit_mode; // SBMODE
    const USART_CHSIZE_t character_size; // CHSIZE - 5bit default as opposed to 8bit as 0x00 is 5bit
    
    // BAUD
    const uint32_t baud; // this is desired baud rate not baud register value

    // PORTMUX USARTROUTEA
    // TODO: each USART has a separate enum (eg. PORTMUX_USART0_enum)
    // we should design around the contract being that based on the USART_t passed in
    // the USARTROUTEA bitfield maps accordingly
} USARTConfig_t;

static inline void configure_usart(const USARTConfig_t config) {
    config.usart->CTRLA = 
        (config.receive_complete_interrupt_enabled << USART_RXCIE_bp) |
        (config.transmit_complete_interrupt_enabled << USART_TXCIE_bp) |
        (config.data_register_empty_interrupt_enabled << USART_DREIE_bp) |
        (config.receive_start_frame_interrupt_enabled << USART_RXSIE_bp) |
        (config.loopback_mode_enabled << USART_LBME_bp) |
        (config.autobaud_error_interrupt_enabled << USART_ABEIE_bp);
    
    config.usart->CTRLB = 
        (config.receiver_enabled << USART_RXEN_bp) |
        (config.transmitter_enabled << USART_TXEN_bp) |
        (config.start_frame_detection_enabled << USART_SFDEN_bp) |
        (config.open_drain_mode_enabled << USART_ODME_bp) |
        (config.receiver_mode) |
        (config.multiprocessor_communication_mode_enabled << USART_MPCM_bp);
    
    config.usart->CTRLC =
        (config.synchronous ? USART_CMODE_SYNCHRONOUS_gc : USART_CMODE_ASYNCHRONOUS_gc) |
        (config.parity_mode) |
        (config.stop_bit_mode) |
        (config.character_size);
    
    // S according to datasheet formula
    // async normal = 16
    // async double speed = 8
    const uint8_t samples_per_bit = (config.receiver_mode == USART_RXMODE_CLK2X_gc) 
        ? SAMPLES_PER_BIT_ASYNC_DOUBLE 
        : SAMPLES_PER_BIT_ASYNC_NORMAL;
    config.usart->BAUD = config.synchronous // NEEDS to be >= 64
        ? (F_CPU/(2.0*config.baud))
        : (((64.0*F_CPU)/(samples_per_bit*config.baud)));
}

static inline bool is_usart_data_register_empty(const USART_t* const usart) {
    return usart->STATUS & USART_DREIF_bm;
}

// TODO: works for 5-8 data bits but not 9 bits therefore we need a separate function
// uses polling to keep things simple as opposed to a buffer
static inline void usart_send_byte_blocking(USART_t* const usart, const uint8_t byte) {
    while(!is_usart_data_register_empty(usart));
    usart->TXDATAL = byte;
}

static inline void usart_send_msg_blocking(USART_t* const usart, const char* const msg) {
    for(int i=0; msg[i] != 0; i++) {
        usart_send_byte_blocking(usart, msg[i]);
    }
}

// TEMP: test function to configure USART3 and trigger the above ISR
static inline void configure_debug_usart() {
    configure_usart((USARTConfig_t) {
        .usart = &USART3, // connected to USB on curiosity nano
        .receiver_enabled = true,
        .transmitter_enabled = true,
        .character_size = USART_CHSIZE_8BIT_gc,
        .baud = 9600
    });
    // set USART3 pin directions
    PORTB.DIRSET = PIN0_bm; // TX
    PORTB.DIRCLR = PIN1_bm; // RX
}

#endif /* USART_H_ */