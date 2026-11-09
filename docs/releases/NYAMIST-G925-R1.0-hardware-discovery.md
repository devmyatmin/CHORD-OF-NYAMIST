# NYAMIST G925 R1.0 — Hardware Discovery

**Project:** CHORD OF NYAMIST  
**Target:** Immortalis-G925 MC12 / MediaTek MT6991 / REDMI K80 Ultra  
**Status:** Implementation milestone; device validation pending

## Scope

R1.0 adds the first NYAMIST-owned live discovery module to the existing C++20/CMake codebase. It is a read-only diagnostic executable, not a Vulkan driver or ICD.

The module collects platform facts from Android system properties, uname, sysconf, and /proc/meminfo where available. Each collected value carries its source and a direct-observation confidence label.

When built with Vulkan development support, it uses the Vulkan loader API to create a minimal Vulkan instance and query physical devices. It records the physical-device name/type, vendor and device IDs, device API version, raw driver version, driver name/info when exposed, queue-family counts and flags, device extensions, memory heaps, and format properties for a small fixed set of common color/depth formats.

The module does not create a surface, submit work, render, or present. It does not open /dev/mali0 directly and does not modify kernel interfaces, SELinux, vendor partitions, or system driver selection.

## Build and run

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DNYAMIST_BUILD_TESTS=ON
    cmake --build build --parallel
    ctest --test-dir build --output-on-failure
    ./build/nyamist-cli discover
    ./build/nyamist-cli discover --output nyamist-r1-hardware.json

Android builds should use the Android NDK toolchain and link the public Vulkan loader library. A build without Vulkan development headers/library still produces a discovery executable, but reports Vulkan as unavailable rather than fabricating capabilities.

## Result interpretation

- success: the Vulkan loader created an instance and enumerated at least one physical device.
- failed: Vulkan integration was compiled, but loader/instance/device enumeration returned an error or no devices.
- unavailable: this executable was built without Vulkan loader integration.

Successful enumeration is only the discovered state. It is not evidence of surface compatibility, queue submission, rendering, presentation, DXVK operation, or game stability.

Memory heap sizes are hardware-reported heap capacities, not free VRAM or guaranteed application allocation. Raw format/queue flags must be interpreted against the Vulkan specification and the exact runtime version.

## R1.0 device-validation checklist

- [ ] Build with the Android NDK for arm64-v8a.
- [ ] Run on REDMI K80 Ultra / Android 16.
- [ ] Confirm Android properties match the independently recorded device identity.
- [ ] Confirm Vulkan status is success.
- [ ] Reconcile enumerated GPU identity and device ID against the stock-driver evidence record.
- [ ] Retain the complete JSON report and its SHA-256.
- [ ] Confirm no system setting, driver selection, device node, or protected partition was changed.

## Non-goals

R1.0 is not a replacement Vulkan driver, ICD, WCP/ZIP package, renderer, or proof of PanVK/NYAMIST driver performance. The stock vendor driver remains the baseline until a separate NYAMIST-owned implementation is built and validated.
