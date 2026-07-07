# AquaLook V4 — Phase 3 — Run 3.3

Date: 7 juillet 2026

Branch: `feature/aqualook-v4-domain`

Base: `9e42120514510bc74b3343e46f9965e1314962d0`

Created files:

- `src/domain/GpioBinaryActuatorDriver.h`
- `src/domain/GpioBinaryActuatorDriver.cpp`
- `src/drivers/ArduinoGpioPlatform.h`
- `src/drivers/ArduinoGpioPlatform.cpp`

The GPIO driver is compiled only when `AQUALOOK_V4_ENABLE_GPIO=1`.

The domain layer uses `GpioPlatformOps`; only the adapter under `src/drivers` includes Arduino and ESP32 GPIO APIs.

Validated behaviors:

- normal active-high output
- inverted output
- logical readback
- safe state applied during configuration
- invalid output pin rejected
- write failure propagated
- enabled and excluded build profiles

Host results:

```text
Compilation hôte OK
normalWrites=2 invertedWrites=2 normalReads=1 invertedReads=1
GPIO driver enabled
GPIO driver excluded
Profils conditionnels OK
```

No GPIO driver instance is created by the firmware. `RelaisManager`, `RelayTopology`, NVS, Web and LCD remain unchanged.

PlatformIO compilation remains required before runtime integration.
