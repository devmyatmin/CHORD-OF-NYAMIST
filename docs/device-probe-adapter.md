# Device Facts and Vulkan Capability Adapter

The adapter imports a bounded, versioned key=value report. It is deliberately
separate from the game render path and does not open a Vulkan device, load an
ICD, change Android properties, or modify another graphics component.

## Evidence contract

The current format is schema_version=1. evidence_source is required and
should identify the process/container that produced the report. captured_at
is optional. Device facts are required for device.model, device.android_api,
device.kernel_release, device.primary_abi, and device.logical_cpu_count.
Manufacturer, brand, SoC, board, Android release, ABI list, and memory counters
are optional. Total and available memory must be supplied together; available
memory cannot exceed total memory.

vulkan.enumeration_status is required and must be success, unavailable,
or failed. For success, the report must include a nonzero
vulkan.device_count, physical-device name, vendor/device IDs, device API
version, driver name, and driver info. vulkan.extensions_reported=true
requires vulkan.supported_extensions; an empty value means a complete empty
list. Feature queries are represented by typed keys such as
vulkan.feature.timelineSemaphore=true. Feature values must be boolean.

For unavailable or failed, vulkan.device_count may be omitted or must be
zero. An optional vulkan.failure_reason records the observation. Vulkan
feature keys are not accepted unless enumeration succeeded.

The parser rejects unknown or duplicate keys, unsupported schema versions,
invalid numeric/boolean values, malformed or duplicated extension names,
reports larger than 128 KiB, and lines larger than 4 KiB. This is intentional:
missing or malformed evidence must not be silently converted into a positive
capability.

## CLI

    nyamist-cli inspect-device configs/device-probe.example.env

The example file is explicitly synthetic and is only a CLI/CI fixture. Replace
its values with measurements before treating it as device evidence. The
device-facts and vulkan-capabilities reports are both classified as monitor
components, not active Vulkan drivers. Successful physical-device enumeration
is only discovered; it does not establish queue submission, render/present,
long-duration stability, or game compatibility.

## Next integration boundary

A future collector may populate this format from Android getprop, kernel and
memory observations, and Vulkan instance/physical-device queries performed in
the intended app or game-container context. Termux visibility alone is not a
reliable substitute for the graphics process's loader/device context. Keep the
stock vendor Vulkan driver untouched and preserve the source/timestamp with
every imported snapshot.
