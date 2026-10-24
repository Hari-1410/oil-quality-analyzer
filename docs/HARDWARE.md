# Hardware and wiring notes

This document lists only the mappings identifiable in the supplied `.ioc` and application source. Verify against the actual circuit before connecting power. No complete schematic or verified bill of materials was included in the supplied archive.

## Connected hardware (as confirmed for this repository)

- STM32F446RE / NUCLEO-F446RE
- Impedance measurement circuit
- TCS3200 colour sensor
- DS18B20 temperature sensor
- DHT11 humidity sensor (firmware acquisition still requires validation)
- I²C character LCD

Turbidity sensor: **not connected**.

## Source-defined pin mapping

| Signal | Pin | Peripheral / mode in project | Confidence / caveat |
|---|---|---|---|
| TCS3200 S0 | PB0 | GPIO output | Source-defined |
| TCS3200 S1 | PB1 | GPIO output | Source-defined |
| TCS3200 S2 | PB2 | GPIO output | Source-defined |
| TCS3200 S3 | PB10 | GPIO output | Source-defined |
| TCS3200 OUT | PA6 | TIM3_CH1 input capture | Source-defined |
| DS18B20 DATA | PC0 | GPIO switched between open-drain output/input | Source-defined; verify pull-up and bus wiring |
| DHT11 DATA | PB12 | GPIO toggled/read by routine | Pin test exists; successful sensor read not verified |
| DAC excitation | PA4 | DAC_OUT1 | Source-defined; analog circuit details not supplied |
| ADC acquisition | PA0 | ADC1_IN0 | Used by impedance code; also referenced by inactive turbidity driver |
| LCD SCL | PB8 | I2C1_SCL | Source-defined |
| LCD SDA | PB9 | I2C1_SDA | Source-defined; LCD I²C address not documented here |

## Electrical cautions

- Confirm all modules' logic-level requirements and common ground.
- Do not connect a sensor output above the STM32 input's allowed voltage. Use an appropriate divider or level shifter when required by the sensor/module.
- Confirm the DS18B20 one-wire pull-up and DHT11 pull-up requirements for the exact modules used.
- The impedance front-end should be reviewed for output bias, current limiting, protection, grounding, and ADC range before connecting an unknown sample/circuit.
- Do not use the inactive turbidity driver while PA0/ADC1 is allocated to impedance measurement without redesigning the acquisition and pin/resource allocation.
