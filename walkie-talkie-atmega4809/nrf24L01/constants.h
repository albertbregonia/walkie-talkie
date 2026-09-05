#ifndef CONSTANTS_H_
#define CONSTANTS_H_

// many of these constants shouldn't be used directly at the top level
// they should be abstracted behind a command / enum
// derived from https://cdn.sparkfun.com/datasheets/Wireless/Nordic/nRF24L01_Product_Specification_v2_0.pdf

// 1-5 LSByte first
#define CMD_R_REGISTER(ADDRESS) (0b00000000 | (0b00011111 & ADDRESS))// format: 000A AAAA where A = 5 bit register address

// 1-5 LSByte first - data needs to follow this command
// NOTE: only available in power down / standby modes
#define CMD_W_REGISTER(ADDRESS) (0b00100000 | (0b00011111 & ADDRESS)) // format: 001A AAAA where A = 5 bit register address

// 1-32 LSByte first - NOP should follow this command to read value on MISO
#define CMD_R_RX_PAYLOAD 0b01100001 // read from FIFO

// 1-32 LSByte first - data needs to follow this command
#define CMD_W_TX_PAYLOAD 0b10100000 // write to FIFO

#define CMD_FLUSH_TX 0b11100001 // flush TX FIFO (must be in TX mode)
#define CMD_FLUSH_RX 0b11100010 // flush RX FIFO (must be in RX mode, should not be used while an ACK is being transmitted)
#define CMD_REUSE_TX_PL 0b11100011 // used for a PTX device, CE must remain high to reuse the last TX payload

// toggles R_RX_PL_WID, W_ACK_PAYLOAD, W_TX_PAYLOAD_NOACK features,
// NOTE: only available in power down / standby modes
#define CMD_ACTIVATE 0b01010000 
#define CMD_ACTIVATE_DATA 0x73 // magic number byte that needs to follow CMD_ACTIVATE
#define CMD_R_RX_PL_WID 0b01100000 // rx payload width

// 1-32 LSByte first - data needs to follow this command
#define CMD_W_ACK_PAYLOAD(PIPE_ADDRESS) (0b10101000 | (0b00000111 & PIPE_ADDRESS)) // format: 0b10101PPP where P=000-101 (0-5) pipe address (rx mode) write payload with ack in packet on pipe 

// 1-32 LSByte first - data needs to follow this command
#define W_TX_PAYLOAD_NOACK 0b10110000 // (tx mode) disable AUTOACK on this specific packet (datasheet has a typo and says it's only 7 bits so we assume it should have the last 0 lol)

#define CMD_NOP 0xFF // no operation, "might be used to read status register"

// CONFIG register + fields
#define REGISTER_CONFIG 0x00
// bit 7 is reserved for 0 only
#define CONFIG_MASK_RX_DR_bp 6 // default 0, 1 to disable RX_DR interrupt on IRQ
#define CONFIG_MASK_TX_DS_bp 5 // defualt 0, 1 to disable TX_DS interrupt on IRQ
#define CONFIG_MASK_MAX_RT_bp 4 // default 0, 1 to disable MAX_RT interrupt on IRQ
#define CONFIG_EN_CRC_bp 3 // default 1, enable CRC (forced high if one of the bits in EN_AA is high)
#define CONFIG_CRCO_bp 2 // default 0 (1 byte), 1 for 2 bytes, CRC encoding scheme
#define CONFIG_PWR_UP_bp 1 // default 0, 1 for power up, 0 for power down
#define CONFIG_PRIM_RX_bp 0 // 1 for subscriber, 0 for publisher, default 0

// STATUS register + fields
#define REGISTER_STATUS 0x07
// bit 7 is reserved for 0 only
#define STATUS_RX_DR_bp 6 // data ready interrupt, write 1 to clear
#define STATUS_TX_DS_bp 5 // data sent interrupt, write 1 to clear - if AUTO_ACK then it will interrupt when ack'ed
#define STATUS_MAX_RT_bp 4 // max TX retransmit interrupt, write 1 to clear, blocks communication while set
// read-only STATUS bits
// NOTE: data pipe 0b110 is not used and 111 is FIFO empty
#define STATUS_RX_P_NO(STATUS) ((0b00001110 & STATUS) >> 1) // use this macro to get 0-5 (000-101) 
#define STATUS_TX_FULL_bp(STATUS) (1 & STATUS) // true if TX FIFO is full

// EN_AA register + fields
#define REGISTER_EN_AA 0x01
typedef enum nrf24L01PipeAutoAck {
    PIPE_AUTOACK_DISABLE_ALL = 0,
    // TODO: add other bit pos/masks for each pipe P0-P5
} nrf24L01PipeAutoAck_t;

// SETUP_AW register + fields
#define REGISTER_SETUP_AW 0x03
typedef enum nrf24L01PipeAddressWidth { // AW bit field
    PIPE_ADDRESS_WIDTH_3BYTES = 1,
    PIPE_ADDRESS_WIDTH_4BYTES, // 2
    PIPE_ADDRESS_WIDTH_5BYTES, // 3
    // all other values are illegal
} nrf24L01PipeAddressWidth_t;

#define CLAMP_RX_TX_ADDRESS_SIZE(SIZE) ((SIZE > 5) ? 5 : SIZE)

// SETUP_RETR register + fields
// TODO: macro + API to combine retransmit delay/count
#define REGISTER_SETUP_RETR 0x04
// ARD bit field
typedef enum nrf24L01RetransmitDelay {
    RETRANSMIT_DELAY_250US = 0,
    RETRANSMIT_DELAY_500US = 1 << 4,
    RETRANSMIT_DELAY_750US,
    // TODO: continues until 0b1111 (high nibble)
} nrf24L01RetransmitDelay_t;

// ARC bit field
typedef enum nrf24L01RetransmitCount {
    RETRANSMIT_COUNT_0 = 0, // disabled
    RETRANSMIT_COUNT_1,
    // TODO: continues until 0b1111 (low nibble)
} nrf24L01RetransmitCount_t;

#define REGISTER_RF_CH 0x05
#define RF_CH_FREQUENCY(FREQ) ((FREQ > 125) ? 125 : FREQ) // range is 2.4 - 2.525GHz, therefore clamp to 0-125

#define REGISTER_FIFO_STATUS 0x17
#define FIFO_STATUS_IS_RX_EMPTY(STATUS) (STATUS & 0x01) // RX_EMPTY is 0 when there is data

typedef enum nrf24L01PipeAddress {
    RX_ADDR_P0 = 0x0A,
    RX_ADDR_P1,
    RX_ADDR_P2,
    RX_ADDR_P3,
    RX_ADDR_P4,
    RX_ADDR_P5, // 0x0F
    // all other values are illegal
} nrf24L01PipeAddress_t;

#define TX_ADDR 0x10
#endif /* CONSTANTS_H_ */