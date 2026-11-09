# NYAMIST G925 R1.4 — GPU Resource & Transfer Validation

**Target:** Immortalis-G925 MC12 / MediaTek MT6991 / REDMI K80 Ultra  
**Baseline:** Android public Vulkan loader and unmodified stock vendor driver  
**Status:** Implementation and CI validation; target-device validation required

## Purpose

R1.3 proved queue submission and fence completion for an empty command buffer. R1.4 adds a real buffer-memory transfer and verifies the bytes returned by the destination buffer.

## Test path

1. Select the G925 physical device and a graphics-capable queue.
2. Create a logical device with no optional features or device extensions.
3. Create a 4 KiB transfer-source buffer and a 4 KiB transfer-destination buffer.
4. Find compatible HOST_VISIBLE | HOST_COHERENT memory types, preferring DEVICE_LOCAL when available.
5. Allocate and bind memory for both buffers.
6. Map the source allocation and write a deterministic byte pattern.
7. Record vkCmdCopyBuffer and submit it with a fence.
8. Wait for the fence and query its status.
9. Map the destination allocation and compare every byte with the source pattern.
10. Destroy the fence, command buffer, pool, buffers, memory, device, and instance.

No Vulkan device extensions, optional core features, root access, device-node access, SELinux changes, vendor-partition writes, or driver replacement are used.

## Build and run on REDMI K80 Ultra

    cmake -S . -B build-termux-r1.4 \
      -DCMAKE_BUILD_TYPE=Release \
      -DNYAMIST_BUILD_TESTS=ON \
      -DNYAMIST_ENABLE_VULKAN_DISCOVERY=ON \
      -DNYAMIST_FORCE_VULKAN_DYNAMIC_LOADING=ON
    cmake --build build-termux-r1.4 --parallel 4
    ctest --test-dir build-termux-r1.4 --output-on-failure
    ./build-termux-r1.4/nyamist-cli validate-transfer \
      --output nyamist-g925-r1.4-resource-transfer.json
    python -m json.tool nyamist-g925-r1.4-resource-transfer.json
    sha256sum nyamist-g925-r1.4-resource-transfer.json

## Acceptance

A target-device pass requires successful physical-device discovery, logical-device and queue creation, both buffer creations, compatible host-visible/coherent memory selection, allocation and binding, source mapping, command recording, queue submission, fence completion, destination readback, byte-for-byte equality for all 4096 bytes, no device loss, and confirmed cleanup.

The test requires host-visible coherent memory so it can write and read mapped buffers without non-coherent flush/invalidate operations. It prefers a memory type that is also DEVICE_LOCAL, but reports the selected properties rather than assuming that it exists.

## Interpretation

- A successful report proves a Vulkan buffer transfer command executed and the destination bytes matched the deterministic source pattern on the selected physical device.
- It does not prove image allocation, shader execution, graphics rendering, surface presentation, DXVK/game compatibility, sustained performance, or FPS.
- A hosted CI pass validates builds, tests, and report contracts; it is not target-device evidence.
- If Termux reports a missing compatible memory type or access restriction, preserve the failure report. Do not change device permissions or system policy to force a pass.

**CHORD OF NYAMIST — Every Signal Has a Story.**
