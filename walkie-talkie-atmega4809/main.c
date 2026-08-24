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
//     configure_periodic_interrupt_timer((PeriodicInterruptTimerConfig_t) {
//         .timer = &TCB0,
//         .run_standby_enabled = true,
//         .max_value = PLAYBACK_RATE(48000),
//     });
//     enable_tcb(&TCB0);
}

#define PACKET_SIZE PACKET_WIDTH_2BYTES

ISR(PORTA_PORT_vect) {
    cli();
    PORTA.INTFLAGS |= (1 << NRF24L01_IRQ_PIN_bp); // clear interrupt flag
    uint8_t volatile status = nrf24L01_clear_irq(&nrf, CLEAR_ALL_HEADER);
    if(nrf24L01_is_data_ready(status)) {
        union { uint16_t word; uint8_t bytes[PACKET_SIZE]; } packet;
        nrf24L01_read_packet(&nrf, PACKET_SIZE, packet.bytes);
        // play audio when we get a packet - in an ideal world, assuming no over-the-air latency,
        // we would send packets at n ksps/kHz and so playing the packet as soon as we get it would have the best quality
        // however, we don't live in an ideal world and the nrf24L01 has some overhead
        // when packets are being sent (preamble, address, CRC) according to datasheet
        mcp4921_write(&mcp4921, (MCP4921Header_t) {
            .enable = true,
            .gain_2x_disabled = true,
            .value = packet.word,
        });
    }
    if(nrf24L01_is_data_sent(status)) {
        start_adc_conversion(&ADC0);
    }
    sei();
}

ISR(ADC0_RESRDY_vect) {
    cli();
    // use the ADC to sample audio and send packets on the nrf24L01
    // we should use buffering here but this is just a test
    nrf24L01_stream_packet(&nrf, PACKET_SIZE, (uint8_t*)&ADC0.RES);
    sei();
}

int main(void) {
    setup();
    PORTF.DIR = PIN5_bm; // built-in LED
    PORTF.OUTSET = PIN5_bm; // turn off LED, connected to pullup

    const bool publisher = false; // NOTE: set true and false on 2 devices to test publisher/subscriber
    if(publisher) {
        configure_adc((ADCConfig_t) {
            .adc = &ADC0,
            .run_standby_enabled = true,
            .resolution = ADC_RESSEL_10BIT_gc,
            .result_ready_interrupt_enabled = true,
            .pins = ADC_MUXPOS_AIN8_gc,
            .prescaler = ADC_PRESC_DIV2_gc, // TODO: 2.5 MHz / (4 * 13 cycles per conversion) = 48 kHz (roughly)
        });
        enable_adc(&ADC0);
    }
    // configure as streamed publisher if publishers
    configure_nrf24L01(&nrf, (nrf24L01Config_t) {
        .stream = publisher,
        .subscriber = !publisher,
        .power_up = true,
    });
    if(!publisher) {
        // must be set to receive data unless dynamic packet size is configured
        nrf24L01_set_pipe_packet_width(&nrf, RX_PW_P0, PACKET_SIZE);
    }
    
    // POR will ensure these are reset
    // but as i develop, software resetting does not also reset the nrf24L01
    nrf24L01_flush(&nrf, !publisher);
    nrf24L01_write_register(&nrf, 0x03, 1); // 3 byte address
    if(publisher) {
        // start the first sample
        start_adc_conversion(&ADC0);
    }

    sei();
    while(1) {
        sleep_cpu();
    }
}

