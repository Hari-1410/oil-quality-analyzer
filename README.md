# Oil Quality Analyzer

An embedded-systems prototype for exploring cooking-oil condition assessment on an **STM32F446RE** using optical, temperature, humidity, and impedance-related measurements. Firmware is written in C with STM32 HAL and is maintained as an STM32CubeIDE project.

> **Prototype status:** The firmware acquires live sensor measurements, classifies oil as FRESH or USED using provisional TCS3200 RGB frequency thresholds, and uses an embedded gradient-boosting model to estimate remaining frying cycles. Hardware operation, threshold calibration, and model accuracy still require validation. This is an experimental prototype, not a certified food-safety instrument.

## Hardware in the Current Prototype

- **MCU board:** STM32 NUCLEO-F446RE / STM32F446RE
- **TCS3200:** Optical colour sensor; firmware measures output frequency for red, green, and blue filter selections.
- **DS18B20:** One-wire temperature sensor.
- **DHT11:** Firmware includes a start pulse, 40-bit read, checksum, and timeout path for ambient relative humidity. Physical sensor operation still requires verification.
- **Impedance measurement circuit:** DAC-generated excitation and ADC acquisition.
- **I²C character LCD:** Rotates through temperature/humidity, TCS3200 frequency, impedance-circuit voltage, and prediction/classification pages.

The turbidity driver (`Core/Src/turbidity.c`, `Core/Inc/turbidity.h`) is retained in the source tree, but **a turbidity sensor is not connected in the current hardware setup** and must not be treated as an active input.

## Repository Layout

```text
Core/                 Application code, sensor drivers, HAL configuration
Drivers/              STM32 HAL and board-support code
Middlewares/ST/AI/    X-CUBE-AI runtime files included in the project
X-CUBE-AI/App/        Generated X-CUBE-AI network sources
*.ioc                 STM32CubeMX project configuration
*.ld                  Linker scripts
*.launch              STM32CubeIDE debug launch configuration
```

Important application files:

| File | Purpose |
|---|---|
| `Core/Src/main.c` | Initialization, live sensor acquisition, classification, inference, LCD pages, and serial readings |
| `Core/Src/impedance.c` / `Core/Inc/impedance.h` | DAC/ADC-based impedance-related signal acquisition |
| `Core/Src/ds18b20.c` / `Core/Inc/ds18b20.h` | DS18B20 one-wire driver |
| `Core/Src/lcd.c` / `Core/Inc/lcd.h` | I²C LCD driver |
| `Core/Src/gradient_boosting_model.c` / `Core/Inc/gradient_boosting_model.h` | Embedded gradient-boosting inference code |
| `Core/Src/decision_tree.c` / `Core/Inc/decision_tree.h` | Additional generated decision-tree model code |
| `Core/Src/turbidity.c` / `Core/Inc/turbidity.h` | Retained but currently unused turbidity driver |

## Pin Mapping Documented in the Supplied Source

Verify this table against the physical wiring before powering the board. The project configuration and source are the basis for these mappings; the complete impedance circuit wiring is not specified here.

| Function | MCU pin / peripheral | Notes |
|---|---|---|
| TCS3200 S0 | PB0 | Scale control |
| TCS3200 S1 | PB1 | Scale control |
| TCS3200 S2 | PB2 | Colour filter selection |
| TCS3200 S3 | PB10 | Colour filter selection |
| TCS3200 OUT | PA6 / TIM3_CH1 | Frequency measurement input |
| DS18B20 DATA | PC0 | One-wire data; check required external pull-up and sensor wiring |
| DHT11 DATA | PB12 | DHT11 protocol read implemented; verify timing and wiring on hardware |
| Impedance excitation | PA4 / DAC_OUT1 | DAC output from generated waveform |
| Impedance acquisition | PA0 / ADC1_IN0 | ADC input used by impedance code |
| LCD SCL | PB8 / I²C1_SCL | I²C bus |
| LCD SDA | PB9 / I²C1_SDA | I²C bus |

**Pin conflict to keep in mind:** The retained turbidity driver also reads `ADC1` channel 0, corresponding to PA0 in this project, which is used by the impedance acquisition. Because turbidity is not connected, leave that module out of the active sensor path unless the pin allocation and analog front-end are redesigned.

## Build with STM32CubeIDE

1. Install STM32CubeIDE and the STM32F4 device support package.
2. Clone or download this repository.
3. In STM32CubeIDE, choose **File → Import → Existing Projects into Workspace**.
4. Select the repository directory containing `.project` and `.cproject` and import the project.
5. Inspect `Oil_Quality_Analyzer.ioc` and confirm the target board, clock setup, peripherals, and pin assignments match your hardware.
6. Build the project. Connect the board and use the configured USART2 serial console (115200 baud, 8-N-1) to inspect live sensor readings, prediction output, and error statuses.

The project is configured for STM32CubeIDE and STM32CubeMX-generated STM32F4 firmware. The exact local toolchain version used for the supplied snapshot has not been independently reproduced here. A successful clean build and on-board test should be recorded before describing the firmware as verified.

## Live Sensor Acquisition and Inference

The firmware acquires measurements from the following sources:

- **DS18B20:** Oil-probe temperature, subject to correct sensor placement and calibration.
- **DHT11:** Ambient relative humidity.
- **TCS3200:** Red, green, and blue filter output frequencies.
- **Impedance circuit:** Voltage measurements derived from the analog excitation and acquisition circuit.

Two separate outputs are produced:

- **Oil classification:** TCS3200 RGB frequency thresholds independently classify the oil as `FRESH` or `USED`. These thresholds are provisional and require calibration using representative oil samples under controlled lighting and fixed sensor geometry.
- **Remaining frying cycles:** The embedded gradient-boosting model receives six features: temperature, relative humidity, red frequency, green frequency, blue frequency, and impedance-circuit AC RMS voltage. Its output is interpreted as a predicted number of remaining frying cycles from 0 to 4.

Inference is skipped when required sensor readings are invalid or any TCS3200 RGB frequency is zero. An unavailable prediction is not treated as zero remaining cycles.

Raw readings, sensor error statuses, oil classification, and available predictions are printed over USART2. The LCD rotates through sensor readings and the classification/prediction outputs.

**Measurement and model caveats:**

- TCS3200 readings are raw output frequencies, not calibrated colour coordinates or direct measurements of oil quality.
- The impedance subsystem reports voltage quantities; these are not equivalent to calibrated impedance in ohms without appropriate circuit characterization and conversion.
- DHT11 humidity is an environmental measurement, not a direct property of the oil.
- The model's predictive accuracy has not been independently established. The model's training data, feature preprocessing, feature units, and output interpretation must be checked before its predictions can be considered reliable.
- Neither the FRESH/USED classification nor the remaining-cycle estimate establishes that oil is safe to consume.

## Known Limitations and Validation Checklist

See [`docs/KNOWN_ISSUES.md`](docs/KNOWN_ISSUES.md) for source-review findings and remaining validation tasks.

Key validation tasks include:

- [ ] Build the project successfully using a documented STM32CubeIDE/toolchain version.
- [ ] Verify DHT11 timing, checksum handling, and readings on the connected sensor.
- [ ] Compare DS18B20 readings against a reference thermometer.
- [ ] Calibrate the impedance circuit and document the relationship between ADC readings, measured voltage, and the intended impedance-related feature.
- [ ] Evaluate TCS3200 readings under controlled illumination and fixed sensor geometry.
- [ ] Validate the FRESH/USED thresholds using representative, independently labelled oil samples.
- [ ] Verify the gradient-boosting model's feature order, units, preprocessing, output range, and predictions against independent test samples.
- [ ] Test the complete firmware on physical hardware and document observed results.

A successful software build does not, by itself, establish sensor accuracy or model performance.

## Contributing

Issues and pull requests are welcome. Please include the board revision, sensor wiring, CubeIDE/toolchain version, and a reproducible test procedure when reporting hardware-specific results.

## License

The original project code is licensed under the MIT License.

This repository also includes third-party components from STMicroelectronics, STM32Cube HAL, CMSIS, and X-CUBE-AI, which may be subject to separate license terms. Their original license files and notices are retained where applicable. The MIT License for the original project code does not override or replace the licenses governing those third-party components.
