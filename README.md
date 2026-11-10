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

- **Vulkan:** G925 physical-device discovery, logical-device/queue creation, and queue-submit/fence validation have passed on the reference device.
- **G925 rendering:** R1.8 passed a corrected-build 17-frame triangle-draw and GPU timestamp retest; R1.9 passed on-device WSI present-fence validation (17 fence attaches / waits, 16 intermediate resets, 16 GPU timestamp samples). R2.0 is implementing swapchain-image pixel readback; device validation is pending.
- **Graphics stack:** Mali userspace libraries, Android graphics HAL declarations, and MediaTek GPU services have been inventoried.
- **NPU:** MediaTek NeuroPilot NNAPI services are present and the Neuron HAL is running; actual NPU inference is not yet validated.

Queue submission is not rendering. Surface presentation, sustained game workloads, FPS, and NPU inference remain separate validation steps.

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

**Created by AMGNYA**
