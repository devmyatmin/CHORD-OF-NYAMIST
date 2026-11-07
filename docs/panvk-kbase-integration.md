# PanVK on Android kbase — NYAMIST integration plan

## Decision

Use an existing Android kbase port of Mesa PanVK as the Vulkan driver candidate. Do not start a separate Vulkan driver implementation inside NYAMIST.

Candidate upstream:
- https://github.com/zenithblue-oss/panvk-kbase-android
- https://github.com/JimVulkan/mali-panvk

The Android kbase port documents direct use of the vendor Mali kernel interface at `/dev/mali0`, and its published GPU matrix includes Immortalis-G925 (5th Gen / CSF). Its device test table includes a Galaxy Tab S11 Ultra with Immortalis-G925 MC12. That is useful evidence for the GPU family, but it is **not a validation result for the REDMI K80 Ultra**.

## Runtime architecture

```text
D3D11 game
   |
   v
Wine / FEX or Box64 compatibility runtime
   |
   v
DXVK (D3D11 -> Vulkan)
   |
   v
Android Vulkan loader / selected PanVK ICD
   |
   v
PanVK kbase backend
   |
   v
Vendor mali_kbase kernel driver (/dev/mali0)
   |
   v
Immortalis-G925 hardware
```

NYAMIST sits beside this path as a profiler, device-profile manager, validation gate, and cache policy layer. It must not intercept or rewrite raw GPU command streams in the first milestone.

## Important constraints

- PanVK is a Vulkan driver, not a Direct3D driver. DXVK remains the D3D11 translation layer.
- The kbase port uses the device's existing vendor kernel driver. It does not replace the kernel driver.
- The upstream project documents Android 12+ platform-library assumptions for its Android build, but the actual device ABI, API level, linker namespace, and loader path must be probed.
- A Termux shell may not have permission or visibility for `/dev/mali0`. Do not use root or chmod workarounds. Test the driver through its intended Android app/loader integration.
- Never overwrite the stock Vulkan driver. Install a custom ICD only through a supported per-app/launcher driver-loading mechanism, with a rollback path.
- Do not assume the G925 implementation works on every G925 phone. Firmware, kbase UAPI, CSF revision, gralloc, linker namespace, and Android build can differ.

## First device validation

Run from Termux:

```sh
pkg update
pkg install git clang cmake ninja pkg-config python
git clone https://github.com/devmyatmin/NYAMIST-LAYER.git
cd NYAMIST-LAYER
chmod +x tools/termux-probe.sh tools/termux-build-core.sh
./tools/termux-probe.sh | tee k80ultra-probe.txt
./tools/termux-build-core.sh
```

Share the generated `k80ultra-probe.txt` contents. Review and redact Android build fingerprint or other identifying device details before posting publicly.

The portable NYAMIST core can be built in Termux. Building the complete PanVK driver is a separate Android cross-build: the upstream build requires Android NDK plus host LLVM/Mesa tools, Meson, Ninja, Python modules, and shader compiler tools. Do not assume a successful Termux C++ build means the PanVK driver has been built.

## Acceptance gates

1. Probe identifies the actual Android API level, ABI, SoC/GPU strings, and current Vulkan device/driver.
2. PanVK loads through a supported app/launcher ICD path without changing the stock driver.
3. Vulkan smoke tests pass on the K80 Ultra.
4. DXVK creates a D3D11 device using PanVK.
5. A known D3D11 test game reaches a repeatable scene with correct rendering.
6. Cold/warm runs and frame-time data are recorded before enabling any NYAMIST optimization.

## Upstream status snapshot

The upstream project lists Immortalis-G925 among its supported/tested 5th-generation CSF GPUs and reports Vulkan 1.4. Its separate device matrix includes a G925 tablet test. Treat this as upstream evidence, not a claim that NYAMIST or the REDMI K80 Ultra has passed the tests.
