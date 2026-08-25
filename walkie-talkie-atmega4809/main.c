#define F_CPU 20000000UL
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

const MCP4921_t dac = {
    .chip_select_pin_bp = PIN3_bp,
    .chip_select_port = &PORTE,
    .send_spi = send_spi,
};

const nrf24L01_t radio = {
    .chip_select_pin_bp = PIN7_bp,
    .chip_select_port = &PORTA,
    .chip_enable_pin_bp = PIN3_bp,
    .chip_enable_port = &PORTA,
    .send_spi = send_spi,
};

static inline void setup(void) {
    disable_cpu_prescaler();
    // set_cpu_prescaler(CLKCTRL_PDIV_2X_gc); // enable 2.5 MHz aka 20 MHz / 8
    // we can only use standby bc the ADC does not support SLEEP_MODE_PWR_DOWN
    configure_sleep(SLEEP_MODE_STANDBY);
    configure_spi_bus();
    configure_mcp4921(&dac);
    configure_nrf24L01_pins(&radio);
    configure_periodic_interrupt_timer((PeriodicInterruptTimerConfig_t) {
        .timer = &TCB0,
        .run_standby_enabled = true,
        .max_value = PLAYBACK_RATE(48000), // 20 MHz / (DIV32 prescalar * 13 clock cycles per sample) = 48.076 ksps/kHz
        .interrupt_enabled = true,
    });
}

#define PACKET_SIZE PACKET_WIDTH_32BYTES
#define PACKET_WORD_SIZE (PACKET_SIZE/2)
#define BUFFER_SIZE (PACKET_WORD_SIZE*191) // fill up the 6KB of SRAM as much as possible, we can buffer 191 packets
uint16_t head = 0, tail = 0;
uint16_t buffer[BUFFER_SIZE];

ISR(PORTA_PORT_vect) {
    cli();
    PORTA.INTFLAGS |= (1 << NRF24L01_IRQ_PIN_bp); // clear interrupt flag
    uint8_t volatile status = nrf24L01_clear_irq(&radio, CLEAR_ALL_HEADER);
    if(nrf24L01_is_data_ready(status)) {
        PORTF.OUTCLR = PIN5_bm;
        nrf24L01_read_packet(&radio, PACKET_SIZE, (uint8_t*)(buffer+tail));
        tail = (tail + PACKET_WORD_SIZE) % BUFFER_SIZE;
        if(!(TCB0.CTRLA & TCB_ENABLE_bm)) {
            enable_tcb(&TCB0); // enable playback
        }
        PORTF.OUTSET = PIN5_bm;
    }
    sei();
}

ISR(TCB0_INT_vect) {
    cli();
    TCB0.INTFLAGS |= TCB_CAPT_bm; // clear interrupt
    if(head != tail) {
        mcp4921_write(&dac, (MCP4921Header_t) {
            .enable = true,
            .gain_2x_disabled = true,
            .value = buffer[head],
        });
        head = (head+1) % BUFFER_SIZE;
    } else {
        disable_tcb(&TCB0); // disable playback, buffer exhausted, saves power under no audio sent
    }
    sei();
}

// the ADC ISR will continually write to the TX FIFO of the nrf24L01
// and simply deselect the module to finalize sending the packet once we've queued enough samples
// as we initialize the module in standby 2 we can hit our timings better
ISR(ADC0_RESRDY_vect) {
    cli();
    radio.send_spi(ADC0.RESL);
    radio.send_spi(ADC0.RESH);
    if(++tail == PACKET_WORD_SIZE) {
        tail = 0;
        PORTF.OUTCLR = PIN5_bm; // flash LED to indicate sending
        nrf24L01_select(&radio, false); // deselect to send packet
        nrf24L01_select(&radio, true);
        radio.send_spi(CMD_W_TX_PAYLOAD);
        PORTF.OUTSET = PIN5_bm;
    }
    // start next sample, must be here so samples are as live as possible
    // if we sample early, the ADC may complete before the ISR finishes
    // thus slowing the audio down
    start_adc_conversion(&ADC0);
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
            .freerun_enabled = false,
            .result_ready_interrupt_enabled = true,
            .pins = ADC_MUXPOS_AIN8_gc,
            .prescaler = ADC_PRESC_DIV32_gc, // 20 MHz / (32 * 13 cycles per conversion) = 48 kHz (roughly)
        });
        enable_adc(&ADC0);
    }
    // configure as streamed publisher if publishers
    configure_nrf24L01(&radio, (nrf24L01Config_t) {
        .stream = publisher,
        .subscriber = !publisher,
        .power_up = true,
    });
    if(!publisher) {
        // must be set to receive data unless dynamic packet size is configured
        nrf24L01_set_pipe_packet_width(&radio, RX_PW_P0, PACKET_SIZE);
    }
    
    // POR will ensure these are reset
    // but as i develop, software resetting does not also reset the nrf24L01
    nrf24L01_flush(&radio, !publisher);

    // TODO: custom configs - turn these into abstractions in the HAL
    // these are added to improve audio quality by reducing latency and increasing throughput
    nrf24L01_write_register(&radio, REGISTER_SETUP_AW, PIPE_ADDRESS_WIDTH_3BYTES); // address width
    nrf24L01_write_register(&radio, REGISTER_EN_AA, PIPE_AUTOACK_DISABLE_ALL); // disable ack
    nrf24L01_write_register(&radio, REGISTER_SETUP_RETR, RETRANSMIT_COUNT_0); // disable retries

    if(publisher) {
        // NOTE: audio quality is good but doesn't sound truly like 10-bit 48 kHz
        // has really good bass and low frequency audio but treble/voice/high frequency
        // sounds like im playing it on flip phone from the early 2000s
        // i don't think the nrf24L01 is the bottleneck because its OTA is 2Mbps
        // i think it's my ADC sampling, i can't get circular buffering + freerun to hit the timings yet
        // it cannot be the DAC because we've achieved stellar audio quality without the radio just ADC -> DAC
        nrf24L01_select(&radio, true);
        radio.send_spi(CMD_W_TX_PAYLOAD);
        // start the first sample
        start_adc_conversion(&ADC0);
    }

    sei();
    while(1) {
        sleep_cpu();
    }
}

