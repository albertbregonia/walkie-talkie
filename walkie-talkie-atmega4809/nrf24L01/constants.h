#ifndef CONSTANTS_H_
#define CONSTANTS_H_

// many of these constants shouldn't be used directly at the top level
// they should be abstracted behind a command / enum

// derived from https://cdn.sparkfun.com/datasheets/Wireless/Nordic/nRF24L01_Product_Specification_v2_0.pdf
#define CMD_R_REGISTER 0b00000000 // format: 000A AAAA where A = 5 bit register address
#define CMD_R_REGISTER_MASK 0b00011111 // & with this mask to ensure opcode is not overwritten
#define CMD_W_REGISTER 0b00100000 // format: 001A AAAA where A = 5 bit register address
#define CMD_W_REGISTER_MASK 0b00111111 // & with this mask to ensure opcode is not overwritten
#define CMD_R_RX_PAYLOAD 0b01100001 // read from FIFO
#define CMD_W_TX_PAYLOAD 0b1010000 // write to FIFO
#define CMD_FLUSH_TX 0b11100001 // flush TX FIFO (must be in TX mode)
#define CMD_FLUSH_RX 0b11100010 // flush RX FIFO (must be in RX mode)
#define CMD_REUSE_TX_PL 0b11100011 // used for a PTX device
#define CMD_ACTIVATE 0b01010000 // activates R_RX_PL_WID, W_ACK_PAYLOAD, W_TX_PAYLOAD_NOACK
#define CMD_ACTIVATE_DATA 0x73 // magic number byte that needs to follow CMD_ACTIVATE
#define CMD_R_RX_PL_WID 0b0110000 // rx payload width

// 1-32 LSByte first - data needs to follow this command
#define CMD_W_ACK_PAYLOAD 0b10101000 // format: 0b10101PPP where P=000-101 pipe address (rx mode) write payload with ack in packet on pipe 
#define CMD_W_ACK_PAYLOAD_MASK 0b10101111 // & with this mask to ensure opcode is not overwritten

// 1-32 LSByte first - data needs to follow this command
#define W_TX_PAYLOAD_NOACK 0b10110000 // (tx mode) disable AUTOACK on this specific packet (datasheet has a typo and should have the last 0 bc it's only 7 bits lol)

#define CMD_NOP 0xFF // no operation, "might be used to read status register"
#define REGISTER_STATUS 0x07
#endif /* CONSTANTS_H_ */