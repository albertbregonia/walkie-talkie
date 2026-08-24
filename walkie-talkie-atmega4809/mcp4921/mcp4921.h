#ifndef MCP4921_H_
#define MCP4921_H_

#include <avr/io.h>

// based off datasheet: https://ww1.microchip.com/downloads/aemDocuments/documents/OTH/ProductDocuments/DataSheets/22248a.pdf

// write command: 0 to write, 1 to ignore (therefore no macro)
#define MCP4921_WRITE_bp 15
#define MCP4921_WRITE_gc (0 << MCP4921_WRITE_bp)

// V_ref input buffer control bit
#define MCP4921_BUF_bp 14
#define MCP4921_BUF_BUFFERED_gc (1 << MCP4921_BUF_bp)
#define MCP4921_BUF_UNBUFFERED_gc (0 << MCP4921_BUF_bp)

// !GA output gain selection bit
#define MCP4921_GAIN_bp 13
#define MCP4921_GAIN_1X_gc (1 << MCP4921_GAIN_bp)
#define MCP4921_GAIN_2X_gc (0 << MCP4921_GAIN_bp)

// !SHDN - this is lowk confusing bc it reads like "shutdown is active"
// but it's supposed to be MCP4921 is active
#define MCP4921_SHDN_bp 12
#define MCP4921_SHDN_ACTIVE_gc (1 << MCP4921_SHDN_bp)
#define MCP4921_SHDN_INACTIVE_gc (0 << MCP4921_SHDN_bp)

// MCP4921 payloads are 12-bit, used to clear bits[15:12] for command header
#define MCP4921_COMMAND_bm 0x0FFF
    
typedef struct MCP4921 {
    const uint8_t chip_select_pin_bp;
    PORT_t* const chip_select_port;
    uint8_t (*const send_spi)(uint8_t);
} MCP4921_t;

typedef struct MCP4921Header {
    const bool write_disabled;
    const bool buffered;
    const bool gain_2x_disabled;
    const bool enable;
    const uint16_t value;
} MCP4921Header_t;

// MCP4921 is active low therefore clear and set are inverted
// api level: if we want to "select" => true/false
static inline void mcp4921_select(const MCP4921_t* const mcp4921, const bool active) {
    const uint8_t mask = (1 << mcp4921->chip_select_pin_bp);
    if(active) {
        mcp4921->chip_select_port->OUT &= ~mask;
    } else {
        mcp4921->chip_select_port->OUT |= mask;
    }
}

static inline void configure_mcp4921(const MCP4921_t* const mcp4921) {
    // set CSN as output
    mcp4921->chip_select_port->DIR |= (1 << mcp4921->chip_select_pin_bp);
    mcp4921_select(mcp4921, false);
}

static inline void mcp4921_write(
    const MCP4921_t* const mcp4921, 
    const MCP4921Header_t header
) {
    const uint16_t value = MCP4921_WRITE_gc |
        (header.buffered << MCP4921_BUF_bp) |
        (header.gain_2x_disabled << MCP4921_GAIN_bp) |
        (header.enable << MCP4921_SHDN_bp) |
        (MCP4921_COMMAND_bm & header.value); // clear top bits for command header
        
    mcp4921_select(mcp4921, true);
    mcp4921->send_spi(value >> 8);
    mcp4921->send_spi(value);
    mcp4921_select(mcp4921, false);
}

#endif /* MCP4921_H_ */