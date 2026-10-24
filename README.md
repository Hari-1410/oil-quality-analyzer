# Oil Quality Analyzer

An embedded-systems prototype for exploring cooking-oil condition assessment on an **STM32F446RE** using optical, temperature, humidity, and impedance-related measurements. Firmware is written in C with STM32 HAL and is maintained as an STM32CubeIDE project.

> **Prototype status:** The active main loop acquires and displays live sensor measurements; demo temperature overrides and model-based classification display have been removed. The firmware has not been built and tested on physical hardware as part of this edit. Do not use these measurements to decide whether oil is safe to consume.

## Hardware in the current prototype

- **MCU board:** STM32 NUCLEO-F446RE / STM32F446RE
- **TCS3200:** optical colour sensor; firmware measures output frequency for red, green, and blue filter selections
- **DS18B20:** one-wire temperature sensor
- **DHT11:** firmware now contains a start-pulse, 40-bit read, checksum, and timeout path for ambient relative humidity; physical sensor operation still requires verification
- **Impedance measurement circuit:** DAC-generated excitation and ADC acquisition
- **I²C character LCD:** rotates through the acquired temperature/humidity, TCS3200 frequency, and impedance-circuit voltage readings

The turbidity driver (`Core/Src/turbidity.c`, `Core/Inc/turbidity.h`) is retained in the source tree, but **a turbidity sensor is not connected in the current hardware setup** and must not be treated as an active input.

## Repository layout

```text
Core/                 Application code, sensor drivers, HAL configuration
Drivers/               STM32 HAL and board-support code
Middlewares/ST/AI/     X-CUBE-AI runtime files included in the project
X-CUBE-AI/App/         Generated X-CUBE-AI network sources
*.ioc                  STM32CubeMX project configuration
*.ld                   Linker scripts
*.launch               STM32CubeIDE debug launch configuration
```

Important application files:

| File | Purpose |
|---|---|
| `Core/Src/main.c` | Initialization, live sensor acquisition, LCD pages, and serial readings |
| `Core/Src/impedance.c` / `Core/Inc/impedance.h` | DAC/ADC-based impedance-related signal acquisition |
| `Core/Src/ds18b20.c` / `Core/Inc/ds18b20.h` | DS18B20 one-wire driver |
| `Core/Src/lcd.c` / `Core/Inc/lcd.h` | I²C LCD driver |
| `Core/Src/gradient_boosting_model.c` / `Core/Inc/gradient_boosting_model.h` | Embedded gradient-boosting inference code |
| `Core/Src/decision_tree.c` / `Core/Inc/decision_tree.h` | Additional generated decision-tree model code |
| `Core/Src/turbidity.c` / `Core/Inc/turbidity.h` | Retained but currently unused turbidity driver |

## Pin mapping documented in the supplied source

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

**Pin conflict to keep in mind:** the retained turbidity driver also reads `ADC1` channel 0, corresponding to PA0 in this project, which is used by the impedance acquisition. Because turbidity is not connected, leave that module out of the active sensor path unless the pin allocation and analog front-end are redesigned.

## Build with STM32CubeIDE

1. Install STM32CubeIDE and the STM32F4 device support package.
2. Clone or download this repository.
3. In STM32CubeIDE, choose **File → Import → Existing Projects into Workspace**.
4. Select the repository directory containing `.project` and `.cproject` and import the project.
5. Inspect `Oil_Quality_Analyzer.ioc` and confirm the target board, clock setup, peripherals, and pin assignments match your hardware.
6. Build the project. Connect the board and use the configured USART2 serial console (115200 baud, 8-N-1) to inspect live sensor readings and error statuses.

The project was configured for STM32CubeIDE and STM32CubeMX-generated STM32F4 firmware. The exact local toolchain version used for the supplied snapshot has not been independently reproduced here. A successful clean build and on-board test should be recorded before describing the firmware as verified.

## Live sensor acquisition

The active main loop reads the DS18B20 temperature, attempts a DHT11 humidity read, measures TCS3200 red/green/blue output frequencies, and acquires the impedance circuit response. Readings and sensor error statuses are printed over USART2. The 16x2 LCD rotates through pages for temperature/humidity, RGB frequencies, and impedance-circuit voltages. The application no longer substitutes demo temperature values or displays a model-based oil class / remaining-cycle estimate.

The gradient-boosting and decision-tree source files remain in the repository for separate review, but they are not invoked by the current main acquisition loop. Their presence does not establish model validity or accuracy.

**Measurement caveats:** TCS3200 values are raw output frequencies, and the impedance subsystem currently reports voltage quantities from its analog circuit. Neither is automatically a calibrated oil-quality measurement. DHT11 measures ambient humidity rather than a direct oil property.

## Known limitations and validation checklist

See [`docs/KNOWN_ISSUES.md`](docs/KNOWN_ISSUES.md) for the source-review findings and remaining validation tasks. In particular, verify the DHT11 timing on the connected sensor, compare DS18B20 readings with a reference thermometer, calibrate the impedance circuit, and verify the RGB sensor under controlled illumination. A clean build and physical hardware test are still required.

## Contributing

Issues and pull requests are welcome. Please include the board revision, sensor wiring, CubeIDE/toolchain version, and a reproducible test procedure when reporting hardware-specific results.

## License

No top-level project license was present in the supplied snapshot. Third-party STM32 and X-CUBE-AI components include their own license files. Before publishing this repository, choose and add a license for your original code and retain all third-party notices. Do not assume that the whole repository is covered by the STM32 component licenses.

## Oil classification and remaining-cycle estimate

- The **FRESH/USED** label is derived directly from TCS3200 red/green/blue frequency thresholds in `main.c`; it is independent of the ML model. These thresholds are provisional and must be calibrated with representative oil samples under controlled lighting and fixed sensor geometry.
- The embedded gradient-boosting model receives the live DS18B20 temperature, DHT11 relative humidity, TCS3200 RGB frequencies, and impedance-circuit AC RMS voltage. Its output is displayed only as the predicted number of remaining frying cycles (0–4).
- Inference is skipped when any required sensor read fails or any TCS RGB frequency is zero. A missing prediction is shown as unavailable, not as zero cycles.
- Neither the classification nor the cycle estimate is a certified food-safety determination; validate the model and feature units against independent samples before making accuracy claims.
