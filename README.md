# walkie-talkie
bare-metal firmware for the Atmega4809 to create an ultra low-power walkie-talkie

## Bill of Materials
| Category | Part | Note |
| - | - | - |
| Microcontroller | ATmega4809 | Chosen for its low cost and low power consumption (especially during sleep modes) |
| DAC | MCP4921 | 12-bit SPI DAC for audio playback that supports 20 MHz SCK (2.5MB/s) |
| Radio Module | nrf24L01 | 2.4 GHz Radio Transceiver that supports up to 2Mbps data rate over the air) |

## Project structure
This project was built using Microchip Studio v7.0.2594 (despite it being deprecated as I'm very familiar with its debugging/monitoring capabilities)
- [`./atmega4809`](/walkie-talkie-atmega4809/atmega4809) is a small HAL to create a more readable abstraction over the peripherals and different configurations on the ATmega4809 [(datasheet)](https://ww1.microchip.com/downloads/en/DeviceDoc/ATmega4808-09-DataSheet-DS40002173C.pdf).
- [`./mcp4921`](/walkie-talkie-atmega4809/mcp4921) is an abstraction for the MCP4921 [(datasheet)](https://ww1.microchip.com/downloads/aemDocuments/documents/OTH/ProductDocuments/DataSheets/22248a.pdf) 12-bit SPI DAC
- [`./nrf24L01`](/walkie-talkie-atmega4809/nrf24L01) is an abstraction for the nrf24L01 radio module [(datasheet)](https://cdn.sparkfun.com/datasheets/Wireless/Nordic/nRF24L01_Product_Specification_v2_0.pdf) with many quality of life functions to ensure proper functionality (eg. switching from RX/TX mode with timings)

### Hardware Abstractions
The main firmware is written in AVR C with the code under: [`./walkie-talkie-atmega4809`](/walkie-talkie-atmega4809) The code features many abstractions to be as portable and readable as possible without sacrificing functionality and performance. Therefore, the abstractions are thin and still require knowledge of the datasheet but guarantee desired configurations are written to the correct registers— whether or not that configuration is valid is **not** strictly guaranteed.
- Many functions are `static inline` and written in headers as opposed to prototypes with a separate implementation file as they rely on top level constants such as `F_CPU` and will likely be ran once during setup. This allows the compiler to optimize the code further and reduce flash usage / instructions by avoiding unnecessary pushing of registers to the stack and returning from a function call.
- The configuration structs are designed to resemble the register configurations themselves and use default values if fields are omitted. Many of these structs rename many of the bit fields to be more intelligible. Performance is not sacrificed here as these structs are short lived and eventually compile to a simple register write operation with an immediate.
- Like any good API, many functions in the HAL enforce correct usage. This means internal masking and keeping raw register addresses/constants out of the public API. However, a significant trade-off is the lack of runtime error checking. This is because adding code that should not be executed and exists in `.text` solely for developer diligence is wasteful for memory-limited targets.
    - Furthermore, C comes with its own shortcomings that I cannot avoid. For example, enums in Rust require you to use a variant of the enum and will fail to compile otherwise. In C, even when specified that this type expects an enum, if the underlying type is an integer, supplying an integer that is out of the enum's range is still permitted.
    - For example, the `nrf24L01` uses a 3-bit field to distinguish the 5 RX pipe numbers. The problem is 3-bits allows values from 0-7 but 6 and 7 are not part of the valid range. I attempted to enforce this with an enum in the API but I cannot stop the developer from providing any number from 0-255 as the underlying type is a byte.
