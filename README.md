# CHORD OF NYAMIST

**THE CREATIVE INTELLIGENCE UNIVERSE — V1**  
*Every Signal Has a Story.*

CHORD OF NYAMIST is an Android-first hardware and graphics engineering project. A shared runtime works with platform adapters and device profiles to bring hardware discovery, graphics, compute, telemetry, and validation into one system.

## Current reference platform

- **Device:** REDMI K80 Ultra · dali
- **SoC:** MediaTek MT6991 / Dimensity 9400+
- **CPU:** Cortex-A720 × 4, Cortex-X4 × 3, Cortex-X925 × 1
- **GPU:** Arm Immortalis-G925 MC12
- **Platform:** Android 16 · API 36 · arm64-v8a

## Current engineering evidence

- **Vulkan:** G925 physical-device discovery, logical-device/queue creation, queue submission/fence validation, and GPU resource-transfer validation have passed on the reference device.
- **G925 rendering:** R1.8 passed a corrected-build 17-frame triangle-draw and GPU timestamp retest. R1.9 passed on-device WSI present-fence validation (17 fence attaches / waits, 16 intermediate resets, 16 GPU timestamp samples). **R2.0 passed on the REDMI K80 Ultra:** five real swapchain-image pixels were copied to host memory and verified (four exact clear-color matches plus triangle-center detection). **R2.1 passed on-device:** 16/16 fresh WSI sessions succeeded, five Surface destroy/recreate cycles and five Activity pause events were observed. **R2.2 Hosted CI PASS:** same-device oldSwapchain handoff/replacement-image probe, schema-v7 reporting, bounded OUT_OF_DATE/SUBOPTIMAL diagnostics and arm64 APK packaging compile successfully. Target-device validation is pending.
- **Graphics stack:** Mali userspace libraries, Android graphics HAL declarations, and MediaTek GPU services have been inventoried.
- **NPU:** MediaTek NeuroPilot NNAPI services are present and the Neuron HAL is running; actual NPU inference is not yet validated.

Queue submission is not rendering. Pixel readback is not physical-panel scanout. Sustained game workloads, FPS, and NPU inference remain separate validation steps.

## How the project is organized

The shared runtime stays separate from device-specific capabilities. Platform code handles Android interfaces; profiles record observed hardware facts and validation evidence. The current graphics target is Immortalis-G925 MC12 on MT6991. Other devices require their own evidence before being treated as supported.

## Project boundaries

- Keep the stock Android/vendor drivers intact and use supported interfaces.
- Do not require root, SELinux changes, device-node permission changes, or vendor-partition modification.
- Do not commit proprietary vendor libraries, firmware, private dumps, credentials, or generated build output.
- Record discovered, declared, registered, and executed capabilities as distinct evidence states.

## Engineering notes

- [Hardware-first architecture](docs/hardware-first.md)
- [Device profile index](devices/README.md)
- [MT6991 reference profile](devices/mt6991/profile.json)
- [MT6991 hardware inventory report](docs/hardware-reports/mt6991-redmi-k80-ultra-2026-11-09.md)
- [Detailed module contracts](docs/module-contracts.md)
- [Vulkan release plan](docs/releases/NYAMIST-G925-R1.md)
- [R1.9 present-fence hardware evidence](docs/releases/NYAMIST-G925-R1.9-wsi-present-fence-validation.md)
- [R2.0 pixel readback validation](docs/releases/NYAMIST-G925-R2.0-pixel-readback-verification.md)
- [R2.0 REDMI K80 Ultra hardware evidence](docs/evidence/NYAMIST-G925-R2.0-redmi-k80-ultra-2026-11-10.md)
- [R2.1 REDMI K80 Ultra hardware evidence](docs/evidence/NYAMIST-G925-R2.1-redmi-k80-ultra-2026-11-10.md)
- [R2.2 swapchain recovery validation](docs/releases/NYAMIST-G925-R2.2-swapchain-resize-out-of-date-recovery.md)

**Created by AMGNYA**
## NYAMIST G925 R2.3 — Vulkan 1.4 driver audit

R2.3 audits the Android public Vulkan loader and the selected Immortalis-G925
physical-device implementation independently. It records loader/instance/
device API versions, Vulkan driver properties, raw driver version in decimal
and hexadecimal, conformance version and physical-device UUID. The Vulkan
1.4 feature structure is queried only when the physical device and instance
API versions both make that query valid.

A Vulkan 1.4 loader does not turn a Vulkan 1.3 physical-device driver into
Vulkan 1.4. R2.3 retains the stock Android vendor driver and does not replace
the ICD or modify protected system components.



### R2.3 hardware audit result (2026-11-10)

On REDMI K80 Ultra / Mali-G925-Immortalis MC12, the Vulkan loader and instance
reported 1.4.0 while the selected physical device reported 1.3.278. The Arm
proprietary driver reported conformance version 1.3.8.0. R2.3 hardware audit
passed and conclusively records that Vulkan 1.4 is **not exposed** by the
currently installed physical-device driver. This does not establish an
individual Vulkan 1.4 feature failure because the 1.4 feature query was
skipped on the 1.3 device.

Primary triangle draw and pixel readback, GPU timestamps, present fences, and
same-device oldSwapchain handoff passed. Extent did not change and no actual
OUT_OF_DATE/SUBOPTIMAL result was observed; resize and recovery remain
unvalidated. See
`docs/evidence/NYAMIST-G925-R2.3-device-audit-2026-11-10.md`.


## NYAMIST G925 R2.4 — Vulkan 1.4 promoted-extension inventory

R2.4 adds a device-side inventory of the 16 extension families promoted into Vulkan 1.4. On pre-1.4 physical devices, it records exact extension-name advertisement using `vkEnumerateDeviceExtensionProperties`, separating advertised names from names not advertised by the selected physical device. On Vulkan 1.4-or-newer devices, it identifies the promoted functionality as core rather than falsely reporting absent extension aliases as unsupported.

This is an **extension-name inventory only**. It does not claim the Vulkan 1.4 core API, does not query every extension feature structure, and does not certify Vulkan 1.4 feature/limit compliance. The stock REDMI K80 Ultra physical-device API remains the R2.3-observed 1.3.278 until a new on-device R2.4 report proves otherwise.
