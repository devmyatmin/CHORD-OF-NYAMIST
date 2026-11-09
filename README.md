# CHORD OF NYAMIST

**THE CREATIVE INTELLIGENCE UNIVERSE — V1**

> *“Every Signal Has a Story”*

**Created by AMGNYA**

---

## About the project

**CHORD OF NYAMIST** is a creative and technical universe for building, exploring, documenting, and connecting ideas across intelligent systems, software, graphics technology, music, storytelling, and experimental creation.

The name represents one connected body of work: different signals, disciplines, and creations forming one larger composition.

This repository preserves the project's existing engineering work and carries it forward under the new identity **CHORD OF NYAMIST**. The previous README is retained separately as [README_OLD.md](README_OLD.md); it is an archive of the earlier project description, not the current project identity.

## Current technical focus

The active engineering work is an **offline, on-device Graphics and Performance Intelligence system** for Android. Its intended scope includes:

- DX11/DX12 runtime and compatibility diagnostics.
- DXVK and VKD3D-Proton log interpretation.
- Vulkan capability and graphics-stack inspection.
- CPU, GPU, NPU, memory, thermal, power, and performance telemetry.
- Local AI-assisted analysis that works offline and produces evidence-linked, advisory recommendations.
- Deterministic validation, safe testing, and rollback-oriented workflows.

The system is diagnostic and advisory. It must not silently modify Android system components, vendor drivers, game assets, or upstream runtime components.

## Target device and hardware record

The primary test device is:

- **Device:** REDMI K80 Ultra
- **Android:** Android 16
- **SoC:** MediaTek MT6991 / Dimensity 9400+
- **CPU:** 1× Cortex-X925, 3× Cortex-X4, 4× Cortex-A720
- **GPU:** Arm Immortalis-G925 MC12
- **NPU:** MediaTek NPU 890

Hardware properties and performance readings must be treated as measured evidence only when supported by device-side logs or repeatable tests. A single frequency sample is not a sustained performance benchmark.

## On-device AI progress

A device-side LiteRT-LM experiment using the MT6991-specific Gemma 3 1B IT model has successfully completed inference through the MediaTek dispatch path.

Recorded evidence includes:

- Android ARM64 LiteRT-LM runtime and MediaTek NeuroPilot dispatch library built and packaged by GitHub Actions.
- The runtime loaded the MediaTek dispatch library and NeuroPilot SDK 8.2.26.
- The model generated coherent responses with exit status 0 using the explicit NPU backend selection.
- One recorded run reported approximately **327.86 tokens/s prefill** and **48.51 tokens/s decode**; a separate longer run reported approximately **45.78 tokens/s decode**.
- MediaTek APUSYS AIDL activity was observed in device logs.

**Evidence limitation:** these results confirm a successful LiteRT-LM inference run through the MediaTek dispatch/delegate integration and interaction with the NeuroPilot/APUSYS stack. They do **not** prove that every model operation ran on the physical NPU. CPU XNNPACK also participated, and hardware-level execution counters are still needed for a definitive per-operation NPU execution claim.

The AI advisor remains under development. Inference quality, resource use, thermal behavior, power consumption, and hardware execution placement require further measurement before the backend can be considered fully validated.

## Engineering principles

1. **Evidence before claims.** Separate confirmed results, hypotheses, pending tests, and future plans.
2. **Preserve original work.** Keep source assets, upstream components, logs, and historical records intact.
3. **Offline-first.** Prefer local analysis and on-device inference where technically feasible.
4. **Safe by design.** Use read-only inspection by default; require explicit approval for changes.
5. **Reproducibility.** Record commands, versions, hashes, device state, and test conditions.
6. **No false hardware claims.** Dispatch selection or library presence alone does not prove physical accelerator execution.
7. **One variable per diagnostic test.** Preserve backups and logs and make rollback possible.

## Repository map

The existing engineering documents remain available:

- [Architecture and scope](docs/architecture.md)
- [Module contracts](docs/module-contracts.md)
- [Device validation](docs/device-validation-redmi-k80-ultra.md)
- [PanVK adapter](docs/panvk-adapter.md)
- [DX11/Vulkan validation](docs/dx11-vulkan-validation.md)
- [PanPlay import format](docs/panplay-import.md)
- [Session profiles](docs/profile-persistence.md)
- [A/B regression evaluation](docs/ab-regression-evaluation.md)
- [Roadmap](docs/roadmap.md)
- [MediaTek NPU 890 proof-of-execution experiment](docs/npu-890-proof-of-execution.md)
- [Original README archive](README_OLD.md)

These documents contain historical project terminology and development decisions. They are retained as records; this top-level README defines the current project name and identity.

## Version record

**CHORD OF NYAMIST — THE CREATIVE INTELLIGENCE UNIVERSE, V1**

*Every Signal Has a Story.*

**Created by AMGNYA**
