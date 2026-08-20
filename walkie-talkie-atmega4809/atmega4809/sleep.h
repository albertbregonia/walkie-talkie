#ifndef SLEEP_H_
#define SLEEP_H_

#include <avr/sleep.h>

static inline void configure_sleep(SLPCTRL_SMODE_t mode) {
	set_sleep_mode(mode);
	sleep_enable();
}

#endif /* SLEEP_H_ */