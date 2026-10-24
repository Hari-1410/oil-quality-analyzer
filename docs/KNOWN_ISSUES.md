# Source-review notes and known limitations

These notes were derived from a static review of the supplied firmware files. They are not a substitute for bench testing. No code behavior is represented as hardware-verified unless noted explicitly.

## 1. Live temperature acquisition restored; hardware verification remains

The demo-temperature override has been removed from `Core/Src/main.c`. The loop now uses the status returned by `DS18B20_ReadTemperature()` and reports an error if the read fails. Verify the live value against a reference thermometer and confirm that sensor failures are handled correctly on the target board.

## 2. DHT11 protocol read added; hardware verification remains

The DHT11 routine on PB12 now sends the start pulse, waits for the sensor response, reads 40 bits, validates the checksum, and returns timeout/error status rather than unconditionally failing. This is a source-level implementation only: verify timing and signal integrity with the connected sensor. If reads fail, inspect the pull-up, wiring, GPIO timing, and logic-analyzer trace. DHT11 values describe ambient humidity, not a direct property of the oil.

## 3. Turbidity code is not active hardware

The user-confirmed connected hardware does not include a turbidity sensor. The driver remains in `Core/Src/turbidity.c` and `Core/Inc/turbidity.h`; keep it documented as inactive. It reads ADC1 channel 0/PA0, which is also used by the impedance acquisition path, so calling it may interfere with the same ADC resource/pin.

## 4. Model validity is not established by the C source

The repository includes a generated gradient-boosting model, a separate decision-tree implementation, and X-CUBE-AI-generated sources. The existence of these files does not establish which model has been trained appropriately, which is called in the main loop, or how accurate the prediction is on unseen oil samples. Record the source dataset, target-label definition, training/test split, feature units, scaling, calibration, class mapping, and metrics. Confirm the C inference path exactly matches training-time preprocessing.

The gradient-boosting header documents input features as temperature, humidity, TCS red, TCS green, TCS blue, and impedance output voltage. Confirm the units and semantics match the data used for training.

## 5. Physical measurement and calibration

- TCS3200 values are measured as output frequencies; they are not automatically calibrated colour coordinates. Lighting, sensor distance, container, oil depth, and filter timing may change readings.
- The impedance driver generates a waveform through DAC and samples through ADC. The voltage is a circuit measurement, not an impedance in ohms unless the analog network and conversion equations support that interpretation.
- The DS18B20 requires correct one-wire timing and suitable pull-up wiring. Verify temperature against a reference thermometer.
- DHT11 measures ambient temperature/humidity, not a direct property of the oil. Keep this distinction clear in feature descriptions.

## 6. Safety and claim boundaries

This is a prototype for experimentation. The current acquisition loop displays sensor measurements and does not display a model-derived oil class or remaining-cycle estimate. Sensor readings alone are not a certified food-safety determination. Do not claim food safety, oil lifetime, or predictive accuracy without a documented validation study against a defined reference method and representative samples.

## Suggested pre-release checklist

- [ ] Build from a clean checkout with a documented STM32CubeIDE version.
- [ ] Verify each connected sensor independently and capture raw serial logs.
- [x] Remove the demo-temperature override and remove model-based classification from the live acquisition display. (Source change made; hardware behavior still needs testing.)
- [ ] Validate DHT11 protocol timing and checksum handling, and verify DS18B20 error handling on hardware.
- [ ] Document the impedance circuit schematic, component values, expected input range, and calibration.
- [ ] Fix the active pin/ADC allocation and ensure unused turbidity code cannot be called inadvertently.
- [ ] Document model feature units, training source, class mapping, and independent test metrics.
- [ ] Test repeatability across samples, temperature, illumination, containers, and sensor placement.
- [ ] Add a project license for original work and retain vendor licenses/notices.

## 7. TCS classification thresholds and model inference

The active firmware now derives a binary `FRESH`/`USED` label from TCS3200 RGB frequency thresholds, independently of the gradient-boosting model. These thresholds are provisional and require calibration. The model is used only to estimate remaining frying cycles and is called only when temperature, humidity, impedance, and non-zero RGB readings are available. Model accuracy and the training-time feature units still require independent validation.
