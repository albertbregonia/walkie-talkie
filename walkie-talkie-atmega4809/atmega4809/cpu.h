#ifndef CPU_H_
#define CPU_H_

#include <avr/cpufunc.h>

static inline void disable_cpu_prescaler(void) {
	// disable CPU prescaler to get full 20 MHz
	ccp_write_io((uint8_t*) &CLKCTRL.MCLKCTRLB, ~CLKCTRL_PEN_bm);
}

static inline void set_cpu_prescaler(CLKCTRL_PDIV_t prescaler) {
	// enable prescalar and set to CLKCTRL_PDIV_t value
	ccp_write_io((uint8_t*) &CLKCTRL.MCLKCTRLB, (prescaler | CLKCTRL_PEN_bm));
}

#endif /* CPU_H_ */