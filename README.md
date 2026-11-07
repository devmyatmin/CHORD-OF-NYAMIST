# NYAMIST-LAYER

**Mali AI-assisted D3D11 → Vulkan optimization layer**  
Project status: **DX11-first prototype / Android kbase hardware path**

Target device: **REDMI K80 Ultra · MediaTek Dimensity 9400+ · Arm Immortalis-G925 · 12 GB RAM**.

NYAMIST is a device-aware companion layer for running Direct3D 11 games through an established translation runtime on Android Mali GPUs. It does not replace the GPU, Vulkan driver, Wine/FEX/Box64, or DXVK.

## DX11 + direct Mali hardware path

```text
Windows D3D11 game
       |
       v
Wine / FEX / Box64 compatibility runtime
       |
       v
DXVK (D3D11 -> Vulkan)
       |
       v
Android Vulkan loader + selected PanVK ICD
       |
       v
PanVK kbase backend -> vendor mali_kbase (/dev/mali0)
       |
       v
Immortalis-G925 hardware
```

PanVK is the Vulkan driver, not the D3D11 translator. DXVK translates D3D11 calls to Vulkan. NYAMIST sits beside this path as the profiling, compatibility-policy, validation, and metadata-cache layer.

**Hardware support evidence:** the Android PanVK kbase upstream lists Immortalis-G925 in its 5th-generation CSF GPU matrix and includes a G925 tablet test. That is upstream evidence only; REDMI K80 Ultra validation is still pending. See [PanVK kbase integration](docs/panvk-kbase-integration.md).

## Current NYAMIST core

- Portable C++20 core.
- Deterministic cache fingerprint and versioned local metadata cache.
- Validation gate: unvalidated artifacts are rejected.
- Rule-based optimization advisor interface for a future lightweight on-device AI adapter.
- Unit tests and GitHub Actions build/test workflow.
- Read-only Termux hardware/runtime probe.
- Termux script to build and test the portable NYAMIST core on-device.

This repository does **not** yet contain a custom PanVK driver, DXVK patch, game launcher, Vulkan ICD loader, or complete Android game runtime. We will integrate a maintained PanVK kbase implementation rather than reimplementing Mesa's Vulkan driver.

## Termux quick start

```sh
pkg update
pkg install git clang cmake ninja pkg-config python
git clone https://github.com/devmyatmin/NYAMIST-LAYER.git
cd NYAMIST-LAYER
chmod +x tools/termux-probe.sh tools/termux-build-core.sh
./tools/termux-probe.sh | tee k80ultra-probe.txt
./tools/termux-build-core.sh
```

The probe is read-only. It does not change the stock driver or access the GPU with privileged operations. Review and redact device build fingerprints before sharing the report.

**Important:** Termux can build the portable NYAMIST core. The full PanVK Android driver has a separate NDK + host LLVM/Mesa toolchain build; a successful Termux core build is not a PanVK build. See [Termux workflow](docs/termux-workflow.md).

## Build NYAMIST core

Requirements: CMake 3.20+, C++20 compiler.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Development order

1. Capture the K80 Ultra's real Android API, ABI, kbase visibility, Vulkan device, driver, extensions, and feature bits.
2. Validate a PanVK kbase ICD through a supported Android app/launcher path without replacing the stock driver.
3. Run Vulkan smoke/feature tests on the actual phone.
4. Establish a baseline D3D11 compatibility matrix with DXVK.
5. Integrate NYAMIST device profiles and validated metadata without changing DXVK rendering behavior.
6. Measure cold/warm shader behavior, frame time, memory, and thermals.
7. Add a lightweight local AI advisor after deterministic profiling works.
8. Consider DX12 only after DX11 results are repeatable.

See [architecture](docs/architecture.md), [roadmap](docs/roadmap.md), [device validation](docs/device-validation.md), [PanVK integration](docs/panvk-kbase-integration.md), and [Termux workflow](docs/termux-workflow.md).
