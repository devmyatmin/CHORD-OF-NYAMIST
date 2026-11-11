# NYAMIST G925 R1.1 — Vulkan Capability Intelligence

**Project:** CHORD OF NYAMIST  
**Target:** Immortalis-G925 MC12 / MediaTek MT6991 / REDMI K80 Ultra  
**Implementation status:** Capability inventory implementation in progress; target-device validation required

## Purpose

R1.1 extends the read-only R1.0 hardware discovery path with a version-aware Vulkan capability inventory. It describes what the loaded physical device reports. It does not create a logical device, allocate GPU memory, submit commands, create an Android surface, render, or present.

The stock MediaTek/ARM Vulkan driver remains the baseline. No NYAMIST ICD or replacement driver is created or selected by this milestone.

## Inventory

For every enumerated physical device, the report adds:

- **Core feature support:** Vulkan 1.0 feature booleans through `vkGetPhysicalDeviceFeatures`. For physical devices reporting API 1.1 or newer, Vulkan 1.1, 1.2, and 1.3 feature structures are chained only when the physical-device API version supports them. Each feature records its core API version and a boolean supported value.
- **Physical-device limits:** a named, typed subset covering image dimensions, descriptor counts, vertex/fragment limits, compute limits, push constants, framebuffer/sample-count masks, alignments, indirect drawing, and timestamp support/period.
- **Memory types:** type index, heap index, raw property mask, and decoded common properties such as device-local, host-visible, coherent, cached, lazily allocated, and protected.
- **Memory heaps:** existing driver-reported heap capacity and device-local property. Capacity is not free VRAM and is not a promise of per-process allocation.
- **Queue families:** raw queue flags plus decoded graphics, compute, transfer, sparse-binding, and protected flags.
- **Format properties:** existing raw linear/optimal/buffer masks plus decoded common format-feature names.
- **Query diagnostics:** extension enumeration status and failure reason are explicit per device; feature queries have their own status, maximum queried core API version, and diagnostics.

Extension enumeration and feature support are intentionally separate. An extension appearing in the extension list means only that it was enumerated. This release does not infer extension-specific feature bits from extension names, and it does not yet query every extension-specific feature structure.

## Version and compatibility rules

- Vulkan 1.0 feature queries use the Vulkan 1.0 command.
- Vulkan 1.1+ feature structures are queried through `vkGetPhysicalDeviceFeatures2` and chained according to the physical device's reported `apiVersion`.
- A missing required entry point is reported as a query failure; unsupported API-version structures are not chained.
- Device limits come from the physical-device properties query and are reported as driver values, not inferred from the GPU marketing specification.
- Raw integer masks are retained alongside symbolic names so later tooling can independently decode or compare the original evidence.
- No extension is enabled and no device feature is enabled. Querying support does not mean a feature was enabled for use.

## JSON evidence

The report schema is now version 2. Existing platform, loader, physical-device, queue, extension, heap, and format observations remain available. New per-device fields include query status/reasons, core feature records, numeric limits, floating-point limits, memory types, and symbolic flag arrays.

A report with Vulkan enumeration success still proves only that the loader created an instance and enumerated a physical device. It does not prove logical-device creation, queue submission, surface compatibility, rendering, presentation, DXVK operation, sustained game execution, or production readiness.

## Build and validation

The capability inventory is compiled into the existing C++20 discovery library. Desktop CI must compile and test both normal Vulkan linkage and the forced runtime-dispatch path. Android/Termux validation must rebuild the dynamic-dispatch variant on the REDMI K80 Ultra and retain the complete schema-v2 JSON report and SHA-256.

Suggested Termux commands:

    cmake -S . -B build-termux-dynamic \
      -DCMAKE_BUILD_TYPE=Release \
      -DNYAMIST_BUILD_TESTS=ON \
      -DNYAMIST_ENABLE_VULKAN_DISCOVERY=ON \
      -DNYAMIST_FORCE_VULKAN_DYNAMIC_LOADING=ON
    cmake --build build-termux-dynamic --parallel 4
    ctest --test-dir build-termux-dynamic --output-on-failure
    ./build-termux-dynamic/nyamist-cli discover --output nyamist-g925-r1.1-capabilities.json
    python -m json.tool nyamist-g925-r1.1-capabilities.json > /dev/null
    sha256sum nyamist-g925-r1.1-capabilities.json

Review the JSON for:
- `feature_query_status`, `feature_query_api_max`, and feature records spanning only the device API version;
- `limits_query_status` and expected limit records;
- per-device extension query status;
- memory type indices that reference an existing heap index;
- raw queue/format masks and corresponding symbolic names;
- no claim that heap capacity is available/free memory.

## Safety and validation boundary

This module remains read-only. It uses the public Vulkan loader/API and does not open `/dev/mali0`, access undocumented registers, modify vendor/system partitions, change SELinux, replace kernel modules, or select another driver.

The next gated milestone after capability inventory is a separate minimal logical-device and queue-submission smoke test. Offscreen rendering, Android surface/present, DXVK, and real-game validation remain subsequent independent gates.
