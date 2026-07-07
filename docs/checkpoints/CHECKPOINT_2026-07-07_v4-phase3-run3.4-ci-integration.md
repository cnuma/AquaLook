# AquaLook V4 — Phase 3 — Run 3.4

Date: 7 juillet 2026

Branch: `feature/aqualook-v4-domain`

Base: `009832a8af81f4d6f9aef2adfc964695aec62a0e`

## Objective

Run a complete PlatformIO build of `ProgrammeArrosage`, correct isolated V4 integration errors, and record flash/RAM usage.

## Infrastructure added

Created:

```text
.github/workflows/platformio-v4-domain.yml
```

The workflow runs:

```text
pio run -e ProgrammeArrosage
```

on pushes to `feature/aqualook-v4-domain`, pull requests to `main`, and manual dispatch.

## Execution status

The complete build could not be executed from the assistant environment:

- direct repository clone failed because external DNS access to GitHub is unavailable in the container;
- PlatformIO is not installed in the local container;
- commits created through the GitHub connector did not trigger a visible workflow run;
- automatic creation of a draft pull request for CI validation was blocked by connector safety controls.

No successful PlatformIO compilation is claimed for this run.

## Static integration status

The new V4 files remain isolated from the historical runtime. No change was made to:

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

## Required validation command

From the local AquaLook repository:

```powershell
pio run -e ProgrammeArrosage
```

The same command can also be started from GitHub Actions using the new workflow's `workflow_dispatch` action.

## Expected result collection

Record:

```text
compiler errors or warnings
flash usage
RAM usage
final commit after corrections
```

## Decision

Run 3.4 is only partially complete:

- CI workflow: complete;
- full PlatformIO compilation: pending external execution;
- integration corrections: none applied because no compiler diagnostics were available.

Do not start runtime integration before this build succeeds.
