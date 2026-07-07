# AquaLook V4 — Phase 3 — Run 3.4

Date: 7 juillet 2026

Branch: `feature/aqualook-v4-domain`

Base: `009832a8af81f4d6f9aef2adfc964695aec62a0e`

## Objective

Run a complete PlatformIO build of `ProgrammeArrosage`, correct isolated V4 integration errors, and record flash/RAM usage.

## CI infrastructure

Created:

```text
.github/workflows/platformio-v4-domain.yml
```

The workflow runs:

```text
pio run -e ProgrammeArrosage
```

on pushes to `feature/aqualook-v4-domain`, pull requests to `main`, and manual dispatch.

## Initial build failure

The first local PlatformIO build reached all new V4 files and failed only in:

```text
src/drivers/ArduinoGpioPlatform.cpp
```

Cause:

```text
Arduino macros INPUT, OUTPUT, INPUT_PULLUP, INPUT_PULLDOWN, HIGH and LOW
collided with identically named enum values in the V4 GPIO domain model.
```

## Corrections applied

Modified:

```text
src/domain/GpioBinaryActuatorDriver.h
src/domain/GpioBinaryActuatorDriver.cpp
src/drivers/ArduinoGpioPlatform.cpp
```

Renames:

```text
INPUT            -> MODE_INPUT
OUTPUT           -> MODE_OUTPUT
INPUT_PULLUP     -> MODE_INPUT_PULLUP
INPUT_PULLDOWN   -> MODE_INPUT_PULLDOWN
LOW              -> LEVEL_LOW
HIGH             -> LEVEL_HIGH
```

The correction changes identifiers only. GPIO behavior remains unchanged.

Correction commits:

```text
8e5dd8feaaec10859557e7605777e78fca8887c3
5b2c7055fd7ec24f2c4fd7b784df075020de3ff4
e19d1d656fc09e5e01eb87243c7dbc33fa5a5069
```

## Final PlatformIO validation

Command executed locally:

```powershell
pio run -e ProgrammeArrosage
```

Result:

```text
ProgrammeArrosage  SUCCESS
1 succeeded, 0 failed
Duration: 00:02:41.876
```

## Memory usage

```text
RAM:   20.6% — 67,384 bytes used of 327,680 bytes
Flash: 62.6% — 1,271,749 bytes used of 2,031,616 bytes
```

Remaining capacity:

```text
RAM available:   260,296 bytes
Flash available: 759,867 bytes
```

## Warnings

The build still reports the existing SdFat warning:

```text
#warning File not defined because __has_include(FS.h)
```

This warning is non-blocking and unrelated to the V4 domain additions.

## Integration status

All V4 source files compiled successfully, including:

```text
BinaryActuatorDriver.cpp
BoardPortModel.cpp
DependencyModel.cpp
EquipmentModel.cpp
EquipmentPortBinding.cpp
EquipmentRuntimeState.cpp
EquipmentTypeCatalog.cpp
ExecutionModel.cpp
GpioBinaryActuatorDriver.cpp
HardwareCatalog.cpp
HardwareInventoryModel.cpp
IntentModel.cpp
SimulatedBinaryActuatorDriver.cpp
ArduinoGpioPlatform.cpp
```

## Historical runtime unchanged

No runtime integration was added to:

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

The new drivers compile but remain uninstantiated by the active firmware.

## Decision

Run 3.4 is complete.

- CI workflow: created;
- initial compiler error: diagnosed;
- isolated correction: applied;
- full PlatformIO build: successful;
- RAM and flash usage: recorded;
- runtime behavior: unchanged.

The Phase 3 driver work may continue.
