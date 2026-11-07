# PanVK and DX11 on REDMI K80 Ultra — next device test

## Current local baseline

The user's `vulkaninfo --summary` output on REDMI K80 Ultra still reports:

- `driverID = DRIVER_ID_ARM_PROPRIETARY`
- `driverName = Mali-G925-Immortalis MC12`
- physical-device API version `1.3.278`
- driver version `49.1.0`

This is the stock ARM Vulkan driver. It is a successful Vulkan enumeration checkpoint, not a PanVK or DXVK test. The missing `large_page_conf` sysfs parameter warning and missing `DISPLAY` message did not prevent physical-device enumeration; surface details were skipped because no display was configured in the Termux shell.

## Current upstream candidate

The `zenithblue-oss/panvk-kbase-android` project currently documents a universal Android PanVK driver for Mali kbase architectures v6–v14, including v13 Immortalis-G925. Its README's device matrix records a Galaxy Tab S11 Ultra (Immortalis-G925 MC12) PanProbe 41/41 and PanPlay pass. This is encouraging same-GPU-family evidence, but it is not proof for the REDMI K80 Ultra firmware, kernel UAPI, memory allocator, or display path.

The project also provides PanPlay, an Android Windows-game launcher bundling Wine/Proton, FEX-Emu and PanVK. Its documented D3D9/D3D10/D3D11 path uses DXVK and supports D3D11 feature levels 10_0 through 11_1. Its PanProbe app provides 41 Vulkan tests. D3D12 feature level 12_x is documented as unsupported; DX12 remains out of this project's first milestone.

Upstream references:
- Repository: https://github.com/zenithblue-oss/panvk-kbase-android
- Driver releases: https://github.com/zenithblue-oss/panvk-kbase-android/releases
- PanPlay releases: https://github.com/zenithblue-oss/panvk-kbase-android/releases
- PanProbe releases: https://github.com/zenithblue-oss/panvk-kbase-android/releases

## Recommended validation sequence

Use the upstream Android apps as a controlled reference test before attempting to integrate a driver into NYAMIST:

1. Install PanPlay and PanProbe from the upstream project's official releases only. Do not install a random repack or copy a driver into Android system directories.
2. Run PanProbe's full test suite. Save the result and the reported `driverInfo`.
3. Confirm that the driver identity says `panvk` / Mesa PanVK, not `Mali-G925-Immortalis MC12` / ARM proprietary.
4. In PanPlay, add a small user-owned D3D11 test application and explicitly select DXVK (or Auto if the launch report confirms DXVK).
5. Capture the PanPlay session log, D3D11 feature level, rendered output, and whether the app exits normally.
6. If PanProbe or PanPlay crashes, stop and collect logs. Do not change permissions on `/dev/mali0`, replace vendor libraries, flash a kernel, or disable Android security controls.
7. Only after repeatable PanVK + DXVK success should NYAMIST begin collecting profiles and recommending policy changes.

## Important test interpretation

- PanProbe passing validates the tested Vulkan functionality, not every DXVK feature or every game.
- A D3D11 cube/demo passing is a smoke test, not proof that a large game works.
- The Galaxy Tab S11 Ultra G925 result is reference evidence, not a K80 Ultra result.
- Keep the stock ARM driver intact as the fallback. The upstream launcher should select its bundled driver through its supported app-local mechanism.
- NYAMIST should initially observe and record the upstream runtime; it should not patch DXVK or intercept Vulkan calls.

## NYAMIST integration boundary

The near-term NYAMIST deliverable is a versioned device/runtime profile and read-only diagnostics adapter that can record:

- GPU identity and architecture;
- Vulkan driver name, info, and API version;
- PanProbe test summary;
- Wine/FEX/Box64 and DXVK versions;
- D3D11 feature level;
- game launch outcome and session-log reference;
- cold/warm run timings and memory/thermal observations where available.

Any optimization remains advisory until it has a repeatable A/B result. DXVK owns its own shader/pipeline cache. NYAMIST stores only its own validated metadata.
