# NYAMIST Graphics Runtime

**Modular Android graphics runtime orchestration and resource-conscious optimization.**

Target validation device: **REDMI K80 Ultra · MediaTek Dimensity 9400+ · Immortalis-G925 · Android 16 · 12 GB RAM**.

NYAMIST coordinates independently maintained graphics components through read-only adapters, deterministic validation, device profiles, and a rule-based optimization advisor. An on-device AI advisor remains a future module; no model is loaded into the current runtime. NYAMIST does not rebrand or replace PanVK, DXVK, VKD3D-Proton, Wine, FEX/Box64, the Android Vulkan loader, or the vendor kernel driver.

## Modular runtime

\`\`\`text
D3D11 game -> Wine/FEX/Box64 -> DXVK --------+
                                             +-> Android Vulkan loader -> PanVK -> Mali GPU
D3D12 game -> Wine/FEX/Box64 -> VKD3D-Proton-+

NYAMIST sidecar:
  Core | Component Registry | Runtime Coordinator
  Device Profiles | Validation | Advisor | Monitor
\`\`\`

- **DXVK** translates Direct3D 9/10/11 to Vulkan.
- **VKD3D-Proton** translates Direct3D 12 to Vulkan.
- **PanVK** is a Vulkan driver/backend, not a Direct3D translator.
- **NYAMIST** profiles and validates the runtime; it is not on the per-draw-call path.

See [Modular Runtime Architecture](docs/modular-runtime.md) and [Module Contracts](docs/module-contracts.md).

## Lightweight runtime design

- NYAMIST remains an app-side sidecar. It does not intercept each draw call, inject work into the frame loop, or load external graphics components.
- Cache V2 writes validated payloads as raw binary instead of hexadecimal text. This reduces payload storage from two text characters per byte to one byte per byte.
- Existing V1 hexadecimal cache entries remain readable. New entries are limited to 64 MiB; file length, declared payload length, and checksum are checked before an entry is accepted.
- The cache stores NYAMIST-owned metadata only. DXVK, VKD3D-Proton, and Mesa/PanVK retain ownership of their own shader and pipeline caches.
- The current advisor is deterministic and rule-based. Model selection and local inference are deferred until memory, latency, and A/B regression measurements justify them.

The binary representation reduces cache payload size, but device-side throughput and memory improvements still require benchmarking on the target phone.

## Current implementation

- Portable C++20 core, deterministic fingerprinting, and validated binary metadata cache with V1 read compatibility.
- Rule-based optimization advisor interface.
- Component probe registry and runtime readiness coordinator.
- Read-only PanVK, DXVK, and VKD3D-Proton report adapters with CLI inspection and evidence-gated validation levels.
- Read-only Termux device/runtime probe and on-device core build script.
- Unit tests and GitHub Actions CI.

External graphics components are inspected and coordinated, not vendored or loaded by the NYAMIST core. Their own shader/pipeline caches remain under their ownership.

## Current REDMI K80 Ultra test status

As of 2026-11-08, PanVK RC5 PanProbe reported 41/41 passing. User-run DXVK D3D11 tests reached feature level 11_1; VKD3D-Proton D3D12 tests reached feature level 11_1, while 12_0–12_2 remained unsupported. The 20-second tests exited normally. These are experimental discovery/smoke-test results, not sustained-stability or broad game-compatibility certification.

## Build

Requirements: CMake 3.20+ and a C++20 compiler.

\`\`\`sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DNYAMIST_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/nyamist-cli inspect-panvk configs/panvk-rc5-redmi-k80-ultra.example.env
./build/nyamist-cli inspect-dxvk configs/dxvk-d3d11-report.template.env
./build/nyamist-cli inspect-vkd3d configs/vkd3d-d3d12-report.template.env
\`\`\`

All three inspection commands read normalized evidence files only; they do not load or activate drivers or translators. The checked-in templates remain at the discovered level until matching render/present counters are explicitly captured.

### Termux

\`\`\`sh
pkg update
pkg install git clang cmake ninja pkg-config python
git clone https://github.com/devmyatmin/NYAMIST-LAYER.git
cd NYAMIST-LAYER
chmod +x tools/termux-probe.sh tools/termux-build-core.sh
./tools/termux-probe.sh | tee k80ultra-probe.txt
./tools/termux-build-core.sh
\`\`\`

The probe is read-only. It does not change the stock driver or require privileged GPU access. The portable core build is not a PanVK driver build.

## Safety boundaries

- Keep the stock Android Vulkan driver intact as a rollback path.
- Never overwrite vendor Vulkan libraries, chmod \`/dev/mali0\`, flash a kernel, or weaken SELinux.
- Do not take ownership of DXVK, VKD3D-Proton, or PanVK caches.
- Advisor recommendations remain advisory until deterministic validation and A/B tests pass.

## Development order

1. Module contracts and runtime coordinator.
2. Read-only PanVK, DXVK, and VKD3D-Proton adapters; device-facts adapter next.
3. Versioned session profiles and validation evidence.
4. Sustained D3D11 stability tests.
5. Experimental D3D12 support.
6. Telemetry aggregation and validated advisor recommendations.

See [roadmap](docs/roadmap.md), [architecture](docs/architecture.md), and [Termux workflow](docs/termux-workflow.md).
