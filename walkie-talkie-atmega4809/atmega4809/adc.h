#ifndef ADC_H_
#define ADC_H_

// TODO: not a full HAL, some registers are missing bc i don't use them
typedef struct ADCConfig {
	ADC_t* const adc;

	// CTRLA register
	const bool run_standby_enabled;
	const ADC_RESSEL_t resolution;
	const bool freerun_enabled;
	
	// CLTRB register
	const ADC_SAMPNUM_t sample_accumulation_count;
	
	// CTRLC register
	const bool reduced_size_sampling_capacitance_enabled; // SAMPCAP false for <1V, true for higher ref voltages
	const ADC_REFSEL_t voltage_reference;
	const ADC_PRESC_t prescalar;
	
	// TODO: CTRLD, CTRLE, SAMPCTRL, EVCTRL registers
	
	// INTCTRL register
	const bool window_comparator_interrupt_enabled;
	const bool result_ready_interrupt_enabled;
	
	// MUXPOS register
	const ADC_MUXPOS_t pins;
} ADCConfig_t;

static inline void configure_adc(const ADCConfig_t config) {
	config.adc->CTRLA = 
		(config.run_standby_enabled << ADC_RUNSTBY_bp) |
		(config.resolution) |
		(config.freerun_enabled << ADC_FREERUN_bp);
	
	config.adc->CTRLB = config.sample_accumulation_count;

	config.adc->CTRLC = 
		(config.reduced_size_sampling_capacitance_enabled << ADC_SAMPCAP_bp) |
		(config.voltage_reference) |
		(config.prescalar);
	
	config.adc->INTCTRL =
		(config.window_comparator_interrupt_enabled << ADC_WCMP_bp) |
		(config.result_ready_interrupt_enabled << ADC_RESRDY_bp);
		
	config.adc->MUXPOS = config.pins;
}

static inline void enable_adc(ADC_t* const adc) {
	adc->CTRLA |= ADC_ENABLE_bm;
}

static inline void disable_adc(ADC_t* const adc) {
	adc->CTRLA &= ~ADC_ENABLE_bm;
}

static inline void start_adc_conversion(ADC_t* const adc) {
	adc->COMMAND = ADC_STCONV_bm;
}



#endif /* ADC_H_ */