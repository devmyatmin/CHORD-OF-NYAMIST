# Local Lightweight Graphics Advisor

## Objective

Add an optional, fully local model that helps diagnose and tune existing Windows-to-Vulkan graphics stacks on Android. The first target remains the REDMI K80 Ultra (Dimensity 9400+, Immortalis-G925 MC12).

The model is an evidence analyst and proposal generator. It is not a graphics driver, API translator, shader compiler, game converter, or real-time render-loop component.

## Runtime boundaries

The execution path remains owned by the existing components:

- D3D11: game -> Wine/Proton + CPU compatibility layer -> DXVK -> Android Vulkan loader -> app-local PanVK -> Mali kbase -> Immortalis-G925.
- D3D12: game -> Wine/Proton + CPU compatibility layer -> VKD3D-Proton -> Android Vulkan loader -> selected Vulkan ICD -> GPU.

NYAMIST observes logs, device facts, capability reports, and performance captures beside those paths. It does not replace or fork DXVK, VKD3D-Proton, PanVK, Wine, FEX, or the vendor driver.

## Model placement

The local model must run out-of-process and outside the game's frame/render/present path.

Initial mode:
1. Game runs with the unchanged translator and driver configuration.
2. PanPlay exports SessionLogs and PerfRecorder evidence.
3. NYAMIST verifies archive identity, timestamps, source provenance, and data quality.
4. Deterministic parsers normalize DXVK/VKD3D logs, Vulkan/device facts, frame samples, and system telemetry.
5. The local model analyzes only this bounded evidence package and returns a structured proposal.
6. A deterministic policy validator checks the proposal against known capabilities, hard safety bounds, and evidence freshness.
7. The proposal is stored as advisory/dry-run data. It is not applied automatically.
8. A/B evaluation decides whether a human-approved candidate actually improves the measured workload.

Do not run inference concurrently with a game until memory, CPU, thermal, and power contention have been measured. The first implementation should run after the game exits. In-game inference, hooks, per-draw-call decisions, and per-frame model calls are explicit non-goals.

## Suggested component split

| Component | Role | Authority |
|---|---|---|
| Evidence normalizer | Parse SessionLogs, PerfRecorder CSV/JSON, DXVK/VKD3D logs, Vulkan reports | Source-grounded facts only |
| Local model adapter | Invoke an installed local inference runtime and selected model | Advisory only |
| Context builder | Supply bounded, relevant evidence with source IDs and timestamps | Read-only |
| Proposal schema | Require diagnosis, evidence references, confidence, proposed setting, expected effect, risk, and rollback | Schema validation |
| Deterministic policy gate | Reject unsupported, stale, unsafe, or out-of-range proposals | Final authority |
| Profile store | Save candidate and provenance under NYAMIST-owned storage | No upstream cache writes |
| A/B evaluator | Compare at least three sustained baseline and candidate runs | Measured acceptance only |

The local model must never invent a Vulkan feature, extension, device capability, Present count, frame sample, or performance counter. Unknown fields remain unknown.

## Model and inference backend strategy

Use a model/runtime matrix rather than hard-coding one model into the core.

Candidate tiers for evaluation, not preselected winners:

- Tiny: approximately 0.5B–1.2B parameters for short log classification and structured issue summaries.
- Balanced: approximately 1.5B–2.6B parameters for multi-step diagnostic reasoning and constrained proposal generation.
- Optional specialized/mobile formats: evaluate only after their model format, tokenizer, quantization, and inference runtime are verified on Android.

The initial backend should be a CPU-capable Android/Termux runtime with a portable model format (for example, GGUF through llama.cpp), built with conservative thread and memory limits. A Vulkan compute backend is a separate experiment: generic llama.cpp Vulkan support does not prove the target PanVK build supports every inference kernel or performs well. Keep CPU fallback available.

The Dimensity 9400+ product includes MediaTek NPU 890 and MediaTek lists on-device DeepSeek-R1-Distill model support. This is a hardware/platform capability claim, not proof that a Termux process can access the NPU runtime. Add an NPU backend only after the required vendor SDK/runtime is available, its licensing is reviewed, and a device-side proof-of-execution benchmark passes.

## DX11 and DX12 coverage

DX11 and DX12 are distinct translation paths, not one interchangeable mode.

- DX11 adapter: DXVK/D3D11 logs, selected versus maximum D3D feature level, Vulkan device identity, swapchain setup, frame-time data, and runtime errors.
- DX12 adapter: VKD3D-Proton logs, D3D12 feature/capability requirements, Vulkan feature/extension requirements, descriptor/resource/synchronization diagnostics, and its own Present evidence.
- Never infer DX12 game support from a passing DX11 session.
- Do not let the model recommend enabling unsupported D3D12 features or arbitrary DXVK/VKD3D flags.

D3D11 remains the first acceptance path. D3D12 stays experimental until a named D3D12 game passes its own device-creation, presentation, sustained-run, clean-exit, and repeated-baseline gates.

## Proposal contract

Each model proposal must contain:

- schema and model/prompt/policy versions;
- game/workload ID and hardware/runtime fingerprint;
- evidence IDs with source archive, member, timestamp, and relevant excerpt/hash;
- diagnosis category and concise rationale;
- confidence with explicit uncertainty;
- exact candidate setting and current value;
- expected measurable effect and possible regressions;
- compatibility preconditions and hard bounds;
- rollback value and one-step rollback instruction.

Reject proposals that lack evidence references, exceed an allowlist, conflict with hardware capabilities, target upstream cache files, modify system/vendor/kernel/SELinux state, or attempt to change driver/translator binaries.

Model output is untrusted input. Parse it as data, never as executable shell, C++, environment text, or configuration instructions.

## Performance-analysis requirements

The evidence pipeline must distinguish summary metadata from actual frame records. Validate CSV schemas and row tags before calculating frame-time distributions. Do not assume a single-header CSV when the export uses multiple typed record headers.

At minimum, report:
- sample count and capture duration;
- independently recomputed p50/p95/p99 frame time;
- average FPS and low-FPS calculation method;
- stutter count and the source definition of a stutter;
- GPU/CPU load, frequency, memory, temperature, and power with missing values preserved;
- start/end timestamp consistency;
- PerfRecorder statistics-sample interval distribution and long S-record gaps, paired with co-timed F-record count, summed frame time, and maximum frame time; report this as temporal correlation only, not causality;
- DXVK/VKD3D version and settings consistency across SessionLogs, PerfRecorder, and runtime logs.

If summary fields disagree with raw samples or logs, mark the evidence inconsistent and block tuning decisions until reconciled.

## Acceptance gates

1. **Runtime isolation:** the game runs unchanged when the model runtime is absent or disabled.
2. **Evidence integrity:** imports reject malformed archives, mismatched run IDs, inconsistent timestamps, and unsupported CSV layouts.
3. **Local inference:** model invocation works offline and produces schema-valid output within bounded memory, thread, and time limits.
4. **Grounding:** every proposal references existing evidence; unsupported claims are rejected.
5. **Safety:** no automatic application; no writes to upstream driver/translator settings or caches.
6. **DX11 baseline:** three or more matched sustained runs establish repeatability before tuning.
7. **DX12 baseline:** separate VKD3D-Proton and D3D12 workload validation; DX11 success cannot satisfy this gate.
8. **Measured optimization:** at least three matched candidate runs are compared with at least three baseline runs using the deterministic A/B evaluator.

## Current known evidence caveats

The Nov 8 Stranded Deep session provides:
- 519-second session and 489.130544-second PerfRecorder duration;
- DXVK D3D11 device creation and 1280x720 swapchain setup in the DXVK log;
- selected D3D11 feature level 11_0 and maximum supported level 11_1 (now recorded separately by the importer);
- 41,868 frame samples and significant reported frame-pacing issues;
- 849 S-record statistics samples, with a 0.510-second median sample interval, 0.550-second P95, and 24.481-second maximum interval;
- six S-record intervals over 1 second, five over 2 seconds, three over 5 seconds, and two over 10 seconds;
- in the largest S-record gaps, the summed F-record frame times approximately equal the S timestamp interval. This establishes time alignment, not causation. No S-record GPU/compiler samples exist inside those gaps, so GPU/compiler state during the stalls remains unknown.

The raw typed CSV layout is now validated by the importer. The importer records bounded statistics-gap summaries and co-timed frame evidence without retaining raw samples or inferring causality. The PerfRecorder summary still reports `dxvk: off` while SessionLogs and the DXVK runtime log identify DXVK 3.1.1; this metadata/provenance conflict remains unresolved. The capture also lacks authoritative Vulkan Present success/failure counters and a clean process exit code. Do not use it to authorize tuning.

## Non-goals

- Replacing DXVK or VKD3D-Proton with an LLM.
- Generating replacement game assets or game content.
- Rewriting shaders or pipelines at runtime.
- Calling an LLM from a Vulkan draw, queue-submit, or present call.
- Automatically editing upstream configuration or caches.
- Claiming NPU, PanVK compute, or Vulkan acceleration before target-device execution is measured.
