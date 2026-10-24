# Source-Review Notes and Known Limitations

These notes are based on a static review of the supplied firmware files. They are not a substitute for bench testing. No code behavior is considered hardware-verified unless explicitly stated.

## 1. Live Temperature Acquisition Restored; Hardware Verification Remains

The demo-temperature override has been removed from `Core/Src/main.c`. The loop now uses the status returned by `DS18B20_ReadTemperature()` and reports an error if the read fails.

Verify the live temperature against a reference thermometer and confirm that sensor failures are handled correctly on the target board.

## 2. DHT11 Protocol Read Implemented; Hardware Verification Remains

The DHT11 routine on PB12 includes a start pulse, waits for the sensor response, reads 40 bits, validates the checksum, and returns timeout/error status rather than unconditionally failing.

This is a source-level implementation only. Verify timing and signal integrity with the connected sensor. If reads fail, inspect the pull-up, wiring, GPIO timing, and logic-analyzer trace.

**Important:** DHT11 readings describe ambient temperature and relative humidity, not direct properties of the cooking oil.

## 3. Turbidity Code Is Not Active Hardware

The connected hardware setup does not include a turbidity sensor. The driver remains in `Core/Src/turbidity.c` and `Core/Inc/turbidity.h`, but it must not be treated as an active sensor input.

The driver reads ADC1 channel 0/PA0, which is also used by the impedance acquisition path. Calling the turbidity driver may therefore interfere with the same ADC resource or pin. Keep this module out of the active sensor path unless the hardware and peripheral allocation are redesigned.

## 4. Model Validity Is Not Established by the C Source

The repository includes a generated gradient-boosting model, a separate decision-tree implementation, and X-CUBE-AI-generated sources. The presence of these files does not establish predictive accuracy or demonstrate that the model generalizes to unseen oil samples.

The current firmware source invokes the gradient-boosting model to estimate remaining frying cycles. The separate decision-tree implementation should not be assumed to be part of the active inference path unless its invocation is confirmed in the application code.

Before making performance claims, document:

- The training dataset and its source.
- The target-label definition and meaning of a predicted remaining cycle.
- The training, validation, and independent test split.
- Feature order, units, scaling, and preprocessing.
- Output interpretation and any class mapping or prediction constraints.
- Evaluation metrics and results on independent samples.

Confirm that the C inference path uses exactly the same feature preprocessing and feature semantics as the training pipeline.

The gradient-boosting header documents six input features: temperature, humidity, TCS red frequency, TCS green frequency, TCS blue frequency, and impedance output voltage. Confirm that the units and meanings of these inputs match the training data.

## 5. Physical Measurements and Calibration

- **TCS3200:** The firmware measures output frequencies. These are not automatically calibrated colour coordinates or direct measurements of oil quality. Lighting, sensor distance, container material, oil depth, and filter timing may affect readings.
- **Impedance measurement circuit:** The driver generates a waveform through the DAC and samples the response through the ADC. The resulting voltage is a circuit measurement, not impedance in ohms unless the analog network and conversion equations support that interpretation.
- **DS18B20:** Correct one-wire timing and suitable pull-up wiring are required. Verify temperature against a reference thermometer.
- **DHT11:** The sensor measures ambient temperature and humidity, not a direct property of the oil. Keep this distinction clear in feature descriptions and model documentation.

## 6. Safety and Claim Boundaries

This project is an experimental prototype for cooking-oil condition assessment.

The firmware currently produces two separate outputs:

1. A **FRESH/USED** label derived from provisional TCS3200 RGB frequency thresholds.
2. A **remaining-frying-cycle estimate** produced by the embedded gradient-boosting model when the required sensor readings are valid.

Neither output is a certified food-safety determination. Do not claim that the system establishes whether oil is safe to consume, guarantees oil lifetime, or achieves a particular predictive accuracy without a documented validation study using a defined reference method and representative samples.

## 7. TCS3200 Classification Thresholds and Model Inference

The active firmware derives a binary `FRESH`/`USED` label directly from TCS3200 red, green, and blue output-frequency thresholds. This classification is independent of the gradient-boosting model.

The thresholds are provisional and require calibration against representative, independently labelled oil samples under controlled lighting, fixed sensor geometry, and consistent measurement conditions.

Separately, the gradient-boosting model receives six features:

1. DS18B20 temperature.
2. DHT11 relative humidity.
3. TCS3200 red output frequency.
4. TCS3200 green output frequency.
5. TCS3200 blue output frequency.
6. Impedance-circuit AC RMS voltage.

The model output is interpreted as the predicted number of remaining frying cycles, from 0 to 4. Inference is skipped when required sensor readings fail or any TCS3200 RGB frequency is zero. An unavailable prediction must not be interpreted as zero remaining cycles.

The output range and feature handling in the firmware do not establish model accuracy. Verify the output interpretation and feature units against the original training pipeline and independent test samples.

## Suggested Pre-Release Checklist

- [ ] Build from a clean checkout using a documented STM32CubeIDE/toolchain version.
- [ ] Verify each connected sensor independently and capture raw serial logs.
- [ ] Verify live DS18B20 acquisition and remove any demo-temperature override. *(Source change made; hardware behavior still needs testing.)*
- [ ] Validate DHT11 protocol timing and checksum handling, and verify DS18B20 error handling on hardware.
- [ ] Document the impedance circuit schematic, component values, expected input range, and calibration procedure.
- [ ] Resolve the active pin/ADC allocation and ensure unused turbidity code cannot be called inadvertently.
- [ ] Calibrate the TCS3200-based `FRESH`/`USED` thresholds against representative, independently labelled oil samples.
- [ ] Document model feature units, training source, target-label definition, output interpretation, preprocessing, and independent test metrics.
- [ ] Verify that the gradient-boosting inference path matches the training-time feature order, units, and preprocessing.
- [ ] Test repeatability across oil samples, temperature, illumination, containers, and sensor placement.
- [ ] Test the complete firmware on physical hardware and document the observed results.
- [x] Add a project license for original work and retain applicable vendor licenses/notices.

**Validation status:** The checklist reflects source-review tasks and remaining validation needs. Checked source-level changes do not imply successful hardware operation, calibrated measurements, or validated model predictions.
