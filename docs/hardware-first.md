# Hardware-first architecture

CHORD OF NYAMIST uses one shared runtime with separate platform adapters and device profiles.

## Structure

- **Core runtime:** common discovery, capability models, resource coordination, telemetry, and evidence handling.
- **Platform adapters:** Android APIs, Vulkan, NNAPI, graphics HALs, and other supported system interfaces.
- **Device profiles:** observed SoC, CPU, GPU, NPU, memory, display, and thermal facts. Device-specific facts do not become universal assumptions in core code.
- **Validation:** separate checks for discovery, API creation, execution, presentation, and sustained workloads.

## Evidence rules

1. **Observed** means a property, file, or interface was found.
2. **Declared** means a platform manifest advertises an interface.
3. **Registered** means the Android service manager can see the service.
4. **Executed** means a bounded workload completed on the named device or accelerator.
5. **Sustained** means a repeatable workload passed with timing, thermal, and failure evidence.

These states are not interchangeable. For example, an NNAPI service being registered does not prove NPU inference, and Vulkan queue submission does not prove rendering or presentation.

## Initial reference device

The first detailed profile is [MediaTek MT6991 / REDMI K80 Ultra](../devices/mt6991/profile.json). The accompanying [hardware report](hardware-reports/mt6991-redmi-k80-ultra-2026-11-09.md) records the observed stock graphics and NeuroPilot stack, access limitations, and validation boundaries.

## Safety and repository policy

- Use supported Android APIs and the stock vendor runtime as the baseline.
- Keep device-specific assumptions inside a device profile or platform adapter.
- Do not change root state, SELinux policy, device-node permissions, kernel modules, or protected partitions.
- Do not redistribute proprietary vendor libraries, firmware, protected assets, private dumps, or credentials.
- Keep raw captures and generated build outputs out of version control; commit only reviewed, non-sensitive evidence summaries.

The README stays brief. This document defines the development boundary; device reports carry the detailed measurements.
