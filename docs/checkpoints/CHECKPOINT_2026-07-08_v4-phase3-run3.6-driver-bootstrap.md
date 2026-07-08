# AquaLook V4 — Phase 3 — Run 3.6

Date: 8 juillet 2026

Branch: `feature/aqualook-v4-domain`

Base: `99db1a57d01f9398d3a3281eb736e316f0ea6e0f`

## Objective

Consolidate the binary actuator driver registry and prepare a non-runtime instantiation strategy.

## Created files

```text
src/domain/BinaryActuatorDriverBootstrap.h
src/domain/BinaryActuatorDriverBootstrap.cpp
```

## Documentation

```text
docs/architecture/adr/ADR-0021-binary-driver-bootstrap-non-runtime.md
docs/architecture/AQUALOOK_V4_BINARY_DRIVER_BOOTSTRAP.md
docs/architecture/AQUALOOK_V4_ARCHITECTURE_BACKLOG.md
```

## Strategy

The bootstrap receives:

```text
BinaryActuatorDriverRegistry
BinaryActuatorDriverBootstrapPlan
explicit driver contexts
enabled driver mask
```

It does not allocate memory and does not create global runtime instances.

## Supported driver mask

```text
BOOTSTRAP_DRIVER_SIMULATED
BOOTSTRAP_DRIVER_GPIO
BOOTSTRAP_DRIVER_XL9535
```

GPIO is gated by `AQUALOOK_V4_ENABLE_GPIO`.

XL9535 is gated by `AQUALOOK_V4_ENABLE_I2C`.

## Validation host

```text
Compilation hôte OK
registered=3 requested=3 failures-ok
```

Validated:

- successful registration of three requested drivers;
- missing context failure;
- duplicate controller type failure;
- propagation of registry errors.

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

The bootstrap is compiled as an available helper but is not called by the firmware.

## Validation remaining

Run locally:

```powershell
pio run -e ProgrammeArrosage
```

Record RAM and flash usage after success.
