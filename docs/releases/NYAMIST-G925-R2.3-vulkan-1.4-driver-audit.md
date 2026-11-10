# NYAMIST G925 R2.3 — Vulkan 1.4 and Driver Identity Audit

**Status:** implementation submitted for hosted validation; REDMI K80 Ultra hardware validation pending.

## Objective

Determine whether the stock Android Vulkan stack on the REDMI K80 Ultra exposes
Vulkan 1.4 at the physical-device level. Keep loader API, requested instance
API and physical-device API independent.

## Implementation

- Negotiate the instance API up to the maximum reported by the public Android
  loader, capped at Vulkan 1.4.
- Query `VkPhysicalDeviceDriverProperties` through
  `vkGetPhysicalDeviceProperties2` when supported by the physical device API.
- Record `driverID`, `driverName`, `driverInfo`, raw `driverVersion`
  (decimal and hexadecimal), conformance version and physical-device UUID.
- Query `VkPhysicalDeviceVulkan14Features` only when both physical-device
  and instance API versions are at least 1.4.
- Check Bresenham lines, clustered subgroup rotate, push descriptors and the
  conditional protected-pipeline-access feature.
- Preserve the existing G925 WSI triangle, pixel readback, timestamps,
  present-fence and Surface lifecycle validation.

## Interpretation

The authoritative hardware version is `physical_device_api_version`.
A loader or instance API version of 1.4 does not upgrade a physical device
that reports 1.3.

The feature-structure check is a focused audit, not Vulkan CTS, Android VTS or
full Android Baseline profile certification. Full conformance requires the
appropriate test suites and device certification evidence.

## Safety boundary

Use only Android's public Vulkan loader and stock vendor driver. No root,
direct GPU device-node access, SELinux/kernel changes, vendor partition
modification, replacement ICD or custom driver installation.

## Device validation procedure

1. Install the R2.3 APK from the successful Android workflow artifact.
2. Run the full G925 Surface validation session on the REDMI K80 Ultra.
3. Copy the complete JSON report.
4. Confirm loader/instance/device API versions, driver identity, raw driver
   version, conformance version, physical-device UUID and Vulkan 1.4 status.
5. If physical-device API is below 1.4, record that as the result; do not
   infer support from loader API.

## Hosted validation

Pending GitHub Actions results.

## Hardware validation

Pending device-generated schema-v8 report.
