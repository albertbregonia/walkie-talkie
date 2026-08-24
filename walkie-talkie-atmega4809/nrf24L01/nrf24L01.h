#ifndef NRF24L01_H_
#define NRF24L01_H_

#include <util/delay.h>
#include "constants.h"

// many of these functions are 'static inline'
// mostly bc in firmware there is usually only going to be one place 
// where the function is called and only differ by parameters
// so we try our best to keep things minimal and optimized for the compiler when we can

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

// nrf24L01 is active low therefore clear and set are inverted
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

static inline uint8_t nrf24L01_read_register(const nrf24L01_t* const nrf, const uint8_t address) {
    nrf24L01_select(nrf, true);
    nrf->send_spi(CMD_R_REGISTER(address));
    uint8_t miso = nrf->send_spi(CMD_NOP);
    nrf24L01_select(nrf, false);
    return miso;
}

static inline uint8_t nrf24L01_write_register(const nrf24L01_t* const nrf, const uint8_t address, const uint8_t value) {
    nrf24L01_select(nrf, true);
    nrf->send_spi(CMD_W_REGISTER(address));
    uint8_t miso = nrf->send_spi(value);
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

// helper function to see if the RX_DR bit is set given a STATUS register value
static inline bool nrf24L01_is_data_ready(const uint8_t status_register_value) {
    return status_register_value & (1 << STATUS_RX_DR_bp);
}

// helper function to see if the TX_DS bit is set given a STATUS register value
static inline bool nrf24L01_is_data_sent(const uint8_t status_register_value) {
    return status_register_value & (1 << STATUS_TX_DS_bp);
}

// helper function to see if the MAX_RT bit is set given a STATUS register value
static inline bool nrf24L01_is_max_retransmit(const uint8_t status_register_value) {
    return status_register_value & (1 << STATUS_MAX_RT_bp);
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

static inline void nrf24L01_flush(const nrf24L01_t* const nrf, const bool subscriber) {
    nrf24L01_select(nrf, true);
    nrf->send_spi(subscriber ? CMD_FLUSH_RX : CMD_FLUSH_TX); // FLUSH RX/TX
    nrf24L01_select(nrf, false);
}

// NOT supposed to be publicly used
static inline void nrf24L01_wait_for_power_up() {
    _delay_ms(1.5); // datasheet spec
}

// NOTE: does not set PRIM_RX in the CONFIG register
// simply handles everything AFTER the config register is set
// NOT supposed to be publicly used (but F_CPU requires it being here to use _delay_us())
static inline void nrf24L01_handle_transceiver_mode_transition(
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
        nrf24L01_handle_transceiver_mode_transition(nrf, config.subscriber, config.stream);
    }
}

// simple function to handle switching to RX mode by writing to the CONFIG register
// and setting the correct pin values assuming the nrf24L01's PWR_UP bit is set.
// this can be wasteful if you need to set multiple values in the CONFIG register
// as it will do a read-modify-write operation with the nrf24L01.
// use configure_nrf24L01() to paralleize writes if more than
// just the PRIM_RX bit needs to be configured
// it will implicitly enter RX mode if PRIM_RX=1
static inline void nrf24L01_set_subscriber_rx_mode(const nrf24L01_t* const nrf) {
    nrf24L01_chip_enable(nrf, false); // go standby
    uint8_t current_config = nrf24L01_read_register(nrf, REGISTER_CONFIG);
    current_config |= (1 << CONFIG_PRIM_RX_bp);
    nrf24L01_write_register(nrf, REGISTER_CONFIG, current_config);
    nrf24L01_handle_transceiver_mode_transition(nrf, true, false);
}

// simple function to handle switching to TX mode by writing to the CONFIG register
// and setting the correct pin values assuming the nrf24L01's PWR_UP bit is set.
// this can be wasteful if you need to set multiple values in the CONFIG register
// as it will do a read-modify-write operation with the nrf24L01.
// use configure_nrf24L01() to paralleize writes if more than
// just the PRIM_RX bit needs to be configured
// it will implicitly enter TX mode if PRIM_RX=0
static inline void nrf24L01_set_publisher_tx_mode(const nrf24L01_t* const nrf, const bool stream) {
    nrf24L01_chip_enable(nrf, false); // go standby
    uint8_t current_config = nrf24L01_read_register(nrf, REGISTER_CONFIG);
    current_config &= ~(1 << CONFIG_PRIM_RX_bp); // clear bit for publisher
    nrf24L01_write_register(nrf, REGISTER_CONFIG, current_config);
    nrf24L01_handle_transceiver_mode_transition(nrf, false, stream);
}

typedef enum nrf24L01Pipe {
    RX_PW_P0 = 0x11,
    RX_PW_P1, // implicit 0x12 onward
    RX_PW_P2,
    RX_PW_P3,
    RX_PW_P4,
    RX_PW_P5,
} nrf24L01Pipe_t;

// using this enum allows us to guarantee a valid pipe packet width/size
// as uint8_t is our smallest type that allows 0-255 but we only support 0-32
typedef enum nrf24L01PipePacketWidth {
    PIPE_UNUSED = 0,
    PACKET_WIDTH_1BYTE, // implicit 1-32
    PACKET_WIDTH_2BYTES,
    PACKET_WIDTH_3BYTES,
    PACKET_WIDTH_4BYTES,
    PACKET_WIDTH_5BYTES,
    PACKET_WIDTH_6BYTES,
    PACKET_WIDTH_7BYTES,
    PACKET_WIDTH_8BYTES,
    PACKET_WIDTH_9BYTES,
    PACKET_WIDTH_10BYTES,
    PACKET_WIDTH_11BYTES,
    PACKET_WIDTH_12BYTES,
    PACKET_WIDTH_13BYTES,
    PACKET_WIDTH_14BYTES,
    PACKET_WIDTH_15BYTES,
    PACKET_WIDTH_16BYTES,
    PACKET_WIDTH_17BYTES,
    PACKET_WIDTH_18BYTES,
    PACKET_WIDTH_19BYTES,
    PACKET_WIDTH_20BYTES,
    PACKET_WIDTH_21BYTES,
    PACKET_WIDTH_22BYTES,
    PACKET_WIDTH_23BYTES,
    PACKET_WIDTH_24BYTES,
    PACKET_WIDTH_25BYTES,
    PACKET_WIDTH_26BYTES,
    PACKET_WIDTH_27BYTES,
    PACKET_WIDTH_28BYTES,
    PACKET_WIDTH_29BYTES,
    PACKET_WIDTH_30BYTES,
    PACKET_WIDTH_31BYTES,
    PACKET_WIDTH_32BYTES,
} nrf24L01PipePacketWidth_t;

// pre-req: `pipe` is enabled in EN_RXADDR
static inline void nrf24L01_set_pipe_packet_width(
    const nrf24L01_t* const nrf,
    const nrf24L01Pipe_t pipe,
    const nrf24L01PipePacketWidth_t width
) {
    nrf24L01_write_register(nrf, pipe, width);
}

// pre-reqs: PWR_UP=1, PRIM_RX=0, CE=1 for at least (10 + 130)us to enter TX mode / standby 2
// NOTE: there is no compile-time guarantee that packet width matches the array passed
static inline void nrf24L01_stream_packet(
    const nrf24L01_t* const nrf, 
    const nrf24L01PipePacketWidth_t size,
    const uint8_t data[size]
) {
    nrf24L01_select(nrf, true);
    nrf->send_spi(CMD_W_TX_PAYLOAD);
    for(uint8_t i=0; i<size; i++) {
        nrf->send_spi(data[i]);
    }
    nrf24L01_select(nrf, false);
}

// pre-reqs: PWR_UP=1, PRIM_RX=0
// CE will be pulsed long enough to transition to TX mode (11us)
// but will not wait for TX mode to settle and finish sending
// this is done so that the dev decides interrupt/polling for flow control
// NOTE: there is no compile-time guarantee that packet width matches the array passed
static inline void nrf24L01_send_packet(
    const nrf24L01_t* const nrf, 
    const nrf24L01PipePacketWidth_t size, 
    const uint8_t data[size]
) {
    nrf24L01_stream_packet(nrf, size, data);
    // pulse CE=1 for 10+ us according to datasheet
    nrf24L01_chip_enable(nrf, true);
    _delay_us(11);
    nrf24L01_chip_enable(nrf, false); // go back to standby
}

// pre-reqs: PWR_UP=1, PRIM_RX=1
// reads in a packet from the RX FIFO buffer on the nrf24L01
// and places the contents into the given buffer
// NOTE: there is no compile-time guarantee that packet width matches the array passed
static inline void nrf24L01_read_packet(
    const nrf24L01_t* const nrf,
    const nrf24L01PipePacketWidth_t size,
    uint8_t buffer[size]
) {
    nrf24L01_select(nrf, true);
    nrf->send_spi(CMD_R_RX_PAYLOAD);
    for(uint8_t i=0; i<size; i++) {
        buffer[i] = nrf->send_spi(CMD_NOP);
    }
    nrf24L01_select(nrf, false);
}

#endif /* NRF24L01_H_ */