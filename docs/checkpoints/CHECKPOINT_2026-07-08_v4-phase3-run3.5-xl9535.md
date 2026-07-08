# AquaLook V4 — Phase 3 — Run 3.5

Date: 8 juillet 2026

Branch: `feature/aqualook-v4-domain`

Base: `9b9b4811ead2ac53d0d2af15089133eac8aa9293`

## Objective

Add an isolated conditional binary driver for XL9535 relay boards.

## Created files

```text
src/domain/Xl9535BinaryActuatorDriver.h
src/domain/Xl9535BinaryActuatorDriver.cpp
src/drivers/ArduinoI2cPlatform.h
src/drivers/ArduinoI2cPlatform.cpp
```

## Architecture

```text
BinaryActuator contract
-> Xl9535BinaryActuatorDriver
-> Xl9535I2cOps
-> ArduinoI2cPlatform
-> Wire
```

The domain layer does not include Wire.

## Compilation condition

```text
AQUALOOK_V4_ENABLE_I2C
```

## Register model

```text
INPUT_PORT          0x00
OUTPUT_PORT         0x02
POLARITY_INVERSION  0x04
CONFIGURATION       0x06
```

The driver reads and writes 16-bit registers, low byte first.

## Safe configuration

For a port configuration, the driver:

1. probes the I2C address;
2. writes the safe logical state into the output latch;
3. writes the output latch register;
4. clears only the target channel bit in the configuration mask.

This keeps other channels unchanged and reduces output glitches.

## Validation host

```text
Compilation hôte OK
normalWrites=2 invertedWrites=2 reads=2 probes=3
```

Validated:

- normal output mapping;
- inverted output mapping;
- logical readback;
- absent device path;
- safe state before output enable;
- target channel only.

## PlatformIO validation

Command executed locally:

```powershell
pio run -e ProgrammeArrosage
```

Result:

```text
ProgrammeArrosage  SUCCESS
1 succeeded, 0 failed
Duration: 00:02:54.294
```

## Memory usage after XL9535 driver

```text
RAM:   20.6% — 67,384 bytes used of 327,680 bytes
Flash: 62.6% — 1,271,997 bytes used of 2,031,616 bytes
```

Remaining capacity:

```text
RAM available:   260,296 bytes
Flash available: 759,619 bytes
```

Delta from Run 3.4:

```text
RAM:   +0 bytes
Flash: +248 bytes
```

## Runtime impact

No runtime component was modified:

```text
main.cpp
RelayTopology
RelaisManager
ConfigManager
ScheduleManager
NVS
Web
LCD
```

The XL9535 driver is compiled but not instantiated by the active firmware.

## Decision

Run 3.5 is complete.

- XL9535 driver: created;
- Arduino/Wire adapter: created and isolated;
- host validation: successful;
- PlatformIO validation: successful;
- RAM/flash: recorded;
- runtime behavior: unchanged.
