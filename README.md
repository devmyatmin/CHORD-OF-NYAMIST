# NYAMIST-LAYER

**Mali AI-assisted D3D11 → Vulkan optimization layer**  
Project status: **prototype / DX11-first**

NYAMIST is a device-aware companion layer for running Direct3D 11 games through an established translation runtime on Android Mali GPUs. The first target device is the **REDMI K80 Ultra (MediaTek Dimensity 9400+, Arm Immortalis-G925, 12 GB RAM)**.

> NYAMIST does not replace the GPU, Vulkan driver, Wine, Box64, or DXVK. DXVK remains responsible for translating D3D11 calls to Vulkan. NYAMIST is being built around profiling, compatibility decisions, validated optimization metadata, and reusable local cache entries.

## DX11-first architecture

```text
Windows D3D11 game
       |
       v
Wine + Box64 (Windows/x86-64 compatibility)
       |
       v
DXVK (D3D11 -> Vulkan translation)
       |
       v
NYAMIST policy / profile / validated-cache companion
       |
       v
Android Vulkan driver (vendor driver or a compatible community driver)
       |
       v
Immortalis-G925 GPU
```

The exact integration point between DXVK and NYAMIST is not implemented yet. The current repository establishes a portable C++ core and tests its cache/decision contracts before attempting runtime injection or a DXVK fork.

## Why this design

- **Do not rewrite DXVK.** Use the mature D3D11-to-Vulkan implementation as the rendering translation backend.
- **Keep AI out of the frame-critical path.** A future on-device model may analyze profiles and propose optimizations. Runtime decisions must be deterministic and validated.
- **Cache only validated results.** Cache identity includes source fingerprint, API pair, GPU architecture, Vulkan driver version, and NYAMIST translator/policy version.
- **Do not duplicate DXVK's shader cache.** DXVK continues to own its compiled shader and pipeline caches. NYAMIST initially caches its own analysis and optimization metadata.
- **Probe capabilities; do not guess.** The phone model alone is not enough to determine Vulkan extensions or driver correctness.

## Current prototype

- Portable C++20 core.
- Stable, non-cryptographic fingerprint for cache identity (not for security or file authenticity).
- Versioned local cache format with payload checksum and atomic temporary-file write.
- Validation gate: unvalidated artifacts are rejected.
- Rule-based optimization advisor interface that a local AI adapter can implement later.
- Unit tests and GitHub Actions build/test workflow.

This prototype is **not yet a game launcher, D3D11 hook, DXVK patch, Vulkan driver, or working Android game runtime**.

## Build

Requirements: CMake 3.20+, C++20 compiler, and a standard library with C++20 support.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Run the demo:

```sh
./build/nyamist-cli advice
./build/nyamist-cli fingerprint "example shader/profile input"
./build/nyamist-cli demo-cache ./nyamist-cache
```

## Development order

1. Build and test the portable core.
2. Collect the K80 Ultra's real Vulkan device, API version, extension list, and driver version.
3. Establish a baseline DX11 compatibility matrix using an existing Wine/Box64 + DXVK environment.
4. Integrate NYAMIST profile decisions without changing DXVK rendering behavior.
5. Add repeatable game/scene tests and performance measurements.
6. Evaluate a lightweight local AI advisor only after deterministic profiling works.
7. Consider DX12 only after the DX11 path has measurable results.

See [architecture](docs/architecture.md), [roadmap](docs/roadmap.md), and [device validation](docs/device-validation.md).
