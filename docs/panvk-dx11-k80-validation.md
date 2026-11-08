# PanVK + DXVK validation — REDMI K80 Ultra

Last updated: 2026-11-08

## Device and test scope

Target device: REDMI K80 Ultra, MediaTek Dimensity 9400+ / MT6991, Immortalis-G925 MC12, Android 16, arm64-v8a.

PanVK was tested through the PanPlay/PanProbe app-local bundled/imported driver path. The stock Android Vulkan driver was not replaced.

## PanVK RC5 results

- Imported PanVK .so: load success, one Vulkan device enumerated, PanProbe 41/41.
- Bundled Mesa PanVK RC5: load success, one Vulkan device enumerated, PanProbe 41/41.
- PanVK reported Mesa 26.3.0-devel (git-795ca7ff3e, build dated 2026-10-31), Vulkan API 1.4.363.
- GPU identified through the kbase path as gpu_id 0xd8300015, CSF UAPI 1.30, architecture v13 / G925-family.
- The app logged a non-conformant/testing-only warning and no gralloc hwmodule, using fallback gralloc. This does not prove Android surface presentation is broken; it remains a separate validation item.
- RC5 did not show the shader-cache permission warning observed in the RC4 run.

## D3D11 / DXVK results

ARM64EC D3D11 feature-level test:
- D3D_FEATURE_LEVEL_10_0: PASS
- D3D_FEATURE_LEVEL_10_1: PASS
- D3D_FEATURE_LEVEL_11_0: PASS
- D3D_FEATURE_LEVEL_11_1: PASS
- DXVK reported maximum and selected feature level 11_1.
- 27,508 frames over 20 seconds; test-reported 1,375.3 FPS.
- Wine exit code 0; no tombstone reported by the app.

Synthetic D3D11 Cube benchmark:
- 1280x720, 20,077 frames over about 23.96 seconds.
- Reported average 838.1 FPS; 1% low 157.5 FPS; 0.1% low 47.1 FPS.
- Average GPU load about 92.6%.
- This is a small synthetic benchmark, not a real-game performance estimate. Stutter count and frame-time tails need further investigation.

## D3D12 / VKD3D-Proton results

ARM64EC D3D12 feature-level test:
- D3D_FEATURE_LEVEL_11_0: PASS
- D3D_FEATURE_LEVEL_11_1: PASS
- D3D_FEATURE_LEVEL_12_0, 12_1, 12_2: UNSUPPORTED
- Highest reported feature level: 11_1.
- 19,003 frames over 20 seconds; test-reported 950.0 FPS.
- Repeated Present calls returned hr=0x00000000.
- Wine exit code 0; no tombstone reported by the app.

Shader Model 6.6/6.7/6.8 messages in the VKD3D log are not equivalent to D3D12 feature level 12_x support.

## Interpretation

These results provide positive PanVK load, Vulkan feature-test, D3D11/DXVK, and D3D12/VKD3D smoke-test evidence on this device. The feature-level applications exited normally after their approximately 20-second test windows; that is not evidence of a GPU crash.

Not yet established:
- 5–10 minute sustained rendering stability;
- broad commercial-game compatibility;
- full Android gralloc/surface behavior across launchers;
- visual correctness across complex workloads;
- reliable GPU telemetry under Android SELinux restrictions;
- production readiness.

## Safety and rollback

- Keep the stock ARM Vulkan driver intact.
- Use only the driver's supported app-local selection mechanism.
- Do not overwrite /vendor, chmod /dev/mali0, flash a kernel, or weaken SELinux.
- DXVK, VKD3D-Proton, and PanVK retain ownership of their own caches.
- NYAMIST stores only its own profiles, reports, and validated optimization metadata.

## Next tests

1. Run a sustained D3D11 render/present workload for at least 5 minutes.
2. Capture frame-time percentiles, GPU/device-lost events, memory, and thermal behavior.
3. Repeat the same workload after a cold start to compare cache behavior.
4. Run a separate sustained D3D12 test; keep D3D12 experimental.
5. Add read-only NYAMIST adapters for PanVK, DXVK, VKD3D-Proton, and device facts.
