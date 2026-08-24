#ifndef PERIODIC_INTERRUPT_TIMER_H_
#define PERIODIC_INTERRUPT_TIMER_H_

typedef struct PeriodicInterruptTimerConfig {
    TCB_t* const timer;
    
    // CTRLA register
    const bool run_standby_enabled;
    const bool sync_update_enabled;
    const TCB_CLKSEL_t prescaler;
    
    // CTRLB register
    const bool async;
    
    // CCMPINIT and CCMPEN in periodic interrupt mode won't really do anything but set the WO pin (no toggling)
    // idk why you'd want it but im keeping it in the abstraction to not be restrictive
    const bool initial_pin_state_high; // CCMPINIT
    const bool waveform_output_enabled; // CCMPEN
    
    // INTCTRL register
    const bool interrupt_enabled; // CAPT
    
    const uint16_t start_value; // CNT
    const uint16_t max_value; // CCMP
    
    // doesn't include PORTMUX, EVCTRL, etc. bc i don't use it
} PeriodicInterruptTimerConfig_t;

static inline void configure_periodic_interrupt_timer(PeriodicInterruptTimerConfig_t config) {
    config.timer->CTRLA = 
        (config.run_standby_enabled << TCB_RUNSTDBY_bp) |
        (config.sync_update_enabled << TCB_SYNCUPD_bp) |
        config.prescaler;
    
    config.timer->CTRLB =
        (config.async << TCB_ASYNC_bp) |
        (config.initial_pin_state_high << TCB_CCMPINIT_bp) |
        (config.waveform_output_enabled << TCB_CCMPEN_bp) |
        TCB_CNTMODE_INT_gc;
    
    config.timer->INTCTRL = (config.interrupt_enabled << TCB_CAPT_bp);
    config.timer->CNT = config.start_value;
    config.timer->CCMP = config.max_value;
}

static inline void enable_tcb(TCB_t* const timer) {
    timer->CTRLA |= TCB_ENABLE_bm;
}

static inline void disable_tcb(TCB_t* const timer) {
    timer->CTRLA &= ~TCB_ENABLE_bm;
}

static inline void clear_interrupt_tcb(TCB_t* const timer) {
    timer->INTFLAGS |= TCB_CAPT_bm;
}

#endif /* PERIODIC_INTERRUPT_TIMER_H_ */