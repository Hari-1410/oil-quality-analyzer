#ifndef IMPEDANCE_H
#define IMPEDANCE_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

/*
 * STM32F446RE impedance measurement
 *
 * PA4 -> DAC_OUT1
 * PA0 -> ADC1_IN0
 *
 * DAC:
 *   TIM6 TRGO -> DAC1
 *   DMA       -> sine table
 *
 * ADC:
 *   PA0 -> ADC1
 *   DMA -> ADC buffer
 */

#define IMPEDANCE_FREQUENCY_HZ       1000U

/*
 * 144 samples/cycle × 1000 Hz = 144 kHz DAC update rate.
 *
 * TIM6:
 *   Timer clock = 90 MHz
 *   Prescaler   = 0
 *   Period      = 624
 *
 * 90 MHz / 625 = 144 kHz
 */
#define IMPEDANCE_SINE_SAMPLES       144U
#define IMPEDANCE_DAC_SAMPLE_RATE    144000U

#define IMPEDANCE_ADC_SAMPLES        256U

/*
 * DAC output:
 *
 * Midpoint = 2048 ≈ 1.65 V
 * Amplitude = 1000 counts ≈ 0.806 V
 *
 * Therefore approximately:
 *
 * Minimum ≈ 0.84 V
 * Maximum ≈ 2.46 V
 *
 * This keeps the DAC waveform inside 0–3.3 V.
 */
#define IMPEDANCE_DAC_MIDPOINT       2048U
#define IMPEDANCE_DAC_AMPLITUDE      1000U

typedef struct
{
    float dc_voltage;
    float rms_voltage;
    float ac_rms_voltage;
    uint32_t frequency_hz;

} ImpedanceResult_t;


/**
 * @brief Initialize impedance subsystem.
 */
HAL_StatusTypeDef Impedance_Init(void);


/**
 * @brief Start the 1 kHz DAC excitation waveform.
 */
HAL_StatusTypeDef Impedance_StartExcitation(void);


/**
 * @brief Stop DAC excitation.
 */
void Impedance_StopExcitation(void);


/**
 * @brief Measure the voltage appearing at PA0.
 *
 * The analog signal entering PA0 MUST be limited to
 * 0–3.3 V.
 */
HAL_StatusTypeDef Impedance_Measure(
    ImpedanceResult_t *result
);


/**
 * @brief Read a single ADC sample as voltage.
 */
float Impedance_ReadADCVoltage(void);

#endif /* IMPEDANCE_H */
