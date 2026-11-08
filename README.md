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

### REDMI K80 Ultra cache benchmark

User-run Termux benchmark on 2026-11-08, using a 4 MiB synthetic payload and 10 iterations:

| Metric | V1 hexadecimal | V2 binary |
|---|---:|---:|
| Cache file size | 8,388,660 bytes | 4,194,363 bytes |
| Median write | 227.076 ms | 11.031 ms |
| Median warm read | 57.281 ms | 10.703 ms |
| Write throughput | 17.615 MiB/s | 362.602 MiB/s |
| Warm-read throughput | 69.832 MiB/s | 373.728 MiB/s |

Observed on this run: 50.00% smaller cache files, 20.59× faster median writes, and 5.35× faster median warm reads. The V1 write path uses the legacy hexadecimal stream encoder; V1 reads use the current optimized backward-compatible decoder, not the original per-byte stream decoder. This is a cache-format microbenchmark with synthetic data and a warm filesystem cache. It is not a game-FPS, GPU-throughput, cold-storage, or total-device-RAM result. Repeat runs and game-workload A/B tests are still needed.

## Current implementation

- Portable C++20 core, deterministic fingerprinting, and validated binary metadata cache with V1 read compatibility.
- Optional V1-versus-V2 cache microbenchmark for file size, write latency, and warm-read latency; excluded from default CMake builds and explicitly enabled by the Termux developer build and CI.
- Rule-based optimization advisor interface.
- Component probe registry and runtime readiness coordinator.
- Read-only PanVK, DXVK, and VKD3D-Proton report adapters with CLI inspection and evidence-gated validation levels.
- Read-only Termux device/runtime probe and on-device core build script.
- Immutable in-memory DeviceSessionSnapshot and typed telemetry aggregation with stale/future/out-of-range sample filtering.
- Deterministic Pipeline Gate that blocks stale sessions/missing Vulkan devices and separates smoke-test permission from production readiness.
- Deterministic Command Advisor that emits bounded shader-worker proposals with evidence and rollback values; automatic application is disabled.
- Unit tests and GitHub Actions CI.

External graphics components are inspected and coordinated, not vendored or loaded by the NYAMIST core. Their own shader/pipeline caches remain under their ownership.

## Build

Requirements: CMake 3.20+ and a C++20 compiler.

\`\`\`sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DNYAMIST_BUILD_TESTS=ON -DNYAMIST_BUILD_BENCHMARKS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/nyamist-cli inspect-panvk configs/panvk-rc5-redmi-k80-ultra.example.env
./build/nyamist-cli inspect-dxvk configs/dxvk-d3d11-report.template.env
./build/nyamist-cli inspect-vkd3d configs/vkd3d-d3d12-report.template.env
\`\`\`

All three inspection commands read normalized evidence files only; they do not load or activate drivers or translators. The checked-in templates remain at the discovered level until matching render/present counters are explicitly captured.

### Termux cache benchmark

After pulling the latest branch and running the Termux build:

\`\`\`sh
./build-termux/nyamist-cache-bench "$HOME/nyamist-cache-benchmark" 4 10
\`\`\`

The final arguments are payload size in MiB (1–64) and iteration count (3–100). The benchmark compares V1 hexadecimal storage with V2 binary storage, verifies that both return identical bytes, and reports cache-file size plus median write and warm-read latency. It uses synthetic data and a warm filesystem cache; it does not measure game FPS, GPU throughput, cold-storage latency, or total device RAM.

### Termux build

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


## Device and Vulkan evidence

NYAMIST imports versioned, read-only Android device/Vulkan capability reports
with `nyamist-cli inspect-device <report.env>`. The importer validates
reported device facts, Vulkan physical-device identity, extension lists, and
feature booleans without loading or changing a Vulkan driver. The checked-in
`configs/device-probe.example.env` is synthetic CI data, not target-device
proof. See [`docs/device-probe-adapter.md`](docs/device-probe-adapter.md).


### Session snapshot and telemetry

The in-memory `DeviceSessionSnapshot` joins a schema-v1 device/Vulkan report
with typed telemetry. `TelemetryAggregator` selects the newest sample per
metric, rejects future and out-of-range readings, and keeps stale readings
separate from fresh policy inputs. Samples identify their source and timestamp.
This is a NYAMIST control-plane object, not shared memory or a graphics-driver
ABI. See [session snapshot contract](docs/session-snapshot.md).


### Pipeline Gate

The Pipeline Gate combines RuntimeCoordinator evidence with a fresh DeviceSessionSnapshot. It returns blocked/warning/ready states, keeps missing telemetry as a warning, blocks stale/future sessions and missing Vulkan device enumeration, and never treats a smoke-test allowance as production approval. See [pipeline gate contract](docs/pipeline-gate.md).


### Command Advisor

The current advisor is deterministic and rule-based, not an on-device AI model. It uses fresh CPU utilization evidence and a Pipeline Gate decision to propose a bounded shader-worker budget (1–4), one step at a time, with source/timestamp evidence and rollback value. It never applies the proposal. See [Command Advisor contract](docs/command-advisor.md).
