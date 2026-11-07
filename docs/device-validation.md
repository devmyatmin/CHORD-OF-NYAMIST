# K80 Ultra device validation

Target: REDMI K80 Ultra / MediaTek Dimensity 9400+ / Arm Immortalis-G925 / 12 GB RAM.

The GPU family and device name are project targets, not proof that a particular Android Vulkan driver exposes every feature required by DXVK. Capture runtime capabilities before choosing settings.

## Baseline capture

Save the following into a dated test folder (do not commit personal paths, account data, or unrelated logs):
- Android version and build number;
- process ABI and Wine/Box64 version;
- DXVK version and configuration;
- Vulkan device name, API version, driver name/version;
- available device extensions and feature bits;
- game name/version, executable architecture, and graphics options;
- launch outcome and the complete relevant log.

Use the Vulkan information utility already available in the chosen Android/Winlator environment. If the utility cannot enumerate the actual device used by the container, mark the report as incomplete rather than copying values from another app.

## Test matrix

| Test | Cold run | Warm run | Pass condition |
|---|---:|---:|---|
| Container starts | yes | yes | No Wine/Box64 startup failure |
| Vulkan device opens | yes | yes | Correct device and driver reported |
| D3D11 device creation | yes | yes | No missing required Vulkan feature |
| Game reaches menu | yes | yes | Correct image; no black screen |
| Repeat scene | yes | yes | No new correctness regression |
| Cache reuse | n/a | yes | NYAMIST cache identity matches exact profile |
| Driver change | n/a | yes | Old NYAMIST cache entry is not reused |

## Safety

- Keep a copy of the working container and original driver configuration.
- Test one variable at a time.
- Do not replace the stock Vulkan driver until the stock baseline is recorded.
- Use release artifacts from their maintainers and verify published checksums.
- Do not use experimental driver builds for accounts or multiplayer games where a crash or anti-cheat conflict could cause account loss.

## Result template

```text
Date:
Device / Android build:
GPU / Vulkan device:
Vulkan API:
Driver name / version:
Wine / Box64:
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
