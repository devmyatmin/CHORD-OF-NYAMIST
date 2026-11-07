# K80 Ultra device validation

Target: REDMI K80 Ultra / MediaTek Dimensity 9400+ / Arm Immortalis-G925 / 12 GB RAM.

## First pass: Termux probe

Run `tools/termux-probe.sh` from Termux and preserve the output. It records Android API level, ABI, SoC/GPU properties, whether `/dev/mali0` is visible to the shell, Vulkan summary if `vulkaninfo` is installed, and visible Vulkan libraries. The script is read-only.

A Termux permission denial for `/dev/mali0` is not proof that the GPU or kbase driver is absent. Android app sandbox and linker namespaces can differ from the Termux shell. The decisive test is whether the PanVK ICD loads through its supported Android app/launcher path.

## PanVK direct-hardware checks

- Confirm the selected driver reports `driverName=panvk`.
- Confirm the Vulkan physical device is Immortalis-G925 (or the exact hardware ID/marketing name reported by the driver).
- Record Vulkan API version, driver version, extensions, and feature bits.
- Run the upstream PanProbe suite through the intended app/runtime.
- Keep the stock Vulkan driver and a known-good launcher configuration for rollback.
- Do not chmod `/dev/mali0`, replace vendor libraries globally, flash a kernel, or use root as a first-line workaround.

The upstream PanVK kbase project lists G925 in its supported GPU matrix and has a G925 tablet test. This does not establish compatibility with the REDMI K80 Ultra firmware; local validation is mandatory.

## DX11 acceptance matrix

| Test | Cold run | Warm run | Pass condition |
|---|---:|---:|---|
| Container starts | yes | yes | No Wine/FEX/Box64 startup failure |
| PanVK ICD loads | yes | yes | Driver identifies as PanVK |
| Vulkan device opens | yes | yes | Immortalis-G925 is selected |
| D3D11 device creation | yes | yes | DXVK creates a D3D11 device |
| Game reaches menu | yes | yes | Correct image; no black screen |
| Repeat scene | yes | yes | No new correctness regression |
| Cache reuse | n/a | yes | NYAMIST cache identity matches exact profile |
| Driver change | n/a | yes | Old NYAMIST cache entry is not reused |

## Baseline capture

Record:
- Android version/API level and build number;
- process ABI;
- Vulkan device, API version, driver name/version;
- available device extensions and feature bits;
- PanVK build identifier and PanProbe results;
- Wine/FEX/Box64 and DXVK versions;
- game name/version, executable architecture, and graphics options;
- launch outcome, frame time, memory/thermal behavior, and relevant logs.

Review and redact build fingerprints or other identifying details before publishing logs.

## Result template

```text
Date:
Device / Android API:
GPU / Vulkan device:
Vulkan API:
Driver name / version:
PanVK build:
PanProbe:
Wine / FEX / Box64:
DXVK:
Game / version:
Game architecture:
Cold launch:
Warm launch:
Rendering issues:
Crash / log reference:
Cache hit:
Notes:
```
