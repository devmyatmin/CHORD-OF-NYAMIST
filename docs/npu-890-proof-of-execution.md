# MediaTek NPU 890 proof-of-execution experiment

**Status: experiment prepared; no device-side inference result has been recorded yet.**

## Objective

Prove or disprove that a normal third-party Android/Termux process on the REDMI K80 Ultra can execute a real language-model inference workload on the MediaTek NPU 890 (SoC MT6991), using the documented MediaTek NeuroPilot dispatch path.

This is an isolated feasibility experiment for the Local Lightweight Graphics Advisor. It does not change the advisor's evidence contract, does not run during gameplay, and does not modify PanPlay, Proton, FEX, DXVK, PanVK, the Android system driver, vendor files, or game assets.

## Hardware and upstream support evidence

Target device:
- REDMI K80 Ultra
- MediaTek Dimensity 9400+ / SoC MT6991
- MediaTek NPU 890
- Android 16 / ARM64

Google's LiteRT MediaTek backend lists MT6991 (Dimensity 9400) as a supported SoC. Google's LiteRT-LM MediaTek NPU guide explicitly lists the MT6991 model variant and a command using `--backend=npu`. MediaTek identifies NPU 890 on Dimensity 9400+.

Official references:
- https://developers.google.com/edge/litert/next/mediatek
- https://developers.google.com/edge/litert/next/litert_lm_npu
- https://www.mediatek.com/products/smartphones/mediatek-dimensity-9400-plus

This support documentation is a strong compatibility signal, not proof that this particular phone firmware exposes a usable NeuroPilot runtime to an unprivileged Termux process.

## Selected proof model

For this backend test only (not a final advisor-model selection), use the exact MediaTek model listed by Google:

- Hugging Face repository: `litert-community/Gemma3-1B-IT`
- File: `Gemma3-1B-IT_q4_ekv1280_mt6991.litertlm`
- Format: LiteRT-LM
- Quantization: 4-bit per-channel
- Context size: 1280
- Published size: approximately 986 MB

The Hugging Face model may require sign-in and access approval. Download it in the browser after accepting any required access terms. Do not place access tokens in this repository, terminal command history, logs, or chat. Do not commit the model file; keep it in NYAMIST-owned local model storage.

This model is a test vehicle for the MT6991 NPU backend. Its use does not decide the final model, prompt, or adapter architecture for the graphics advisor.

## Runtime build

The official LiteRT-LM instructions require a Linux build host and Android NDK r28b or newer. The build is therefore isolated in GitHub Actions; it does not consume the phone's RAM or modify the phone's graphics stack.

Pinned upstream source for this experiment:
- Repository: https://github.com/google-ai-edge/LiteRT-LM
- Ref: `v0.19.0` (revisit deliberately when changing the pinned release)
- Build target: `//runtime/engine:litert_lm_main`
- MediaTek dispatch target: `@litert//litert/vendors/mediatek/dispatch:dispatch_api_so`
- Target ABI: Android ARM64

The build artifact must contain:
- `litert_lm_main`
- MediaTek dispatch shared libraries from `bazel-bin/external/litert/litert/vendors/mediatek/`
- Android ARM64 prebuilt shared libraries from `prebuilt/android_arm64/`
- a build manifest and SHA-256 checksums

The workflow is a cross-compile only. A successful GitHub Actions build does not prove that the phone can load the backend or execute NPU kernels.

## Device preflight

Before downloading the model or running inference, collect current free RAM and storage. The approximately 986 MB model is not the full runtime memory requirement; loading, context buffers, and OS services need additional headroom.

Run in Termux:

```sh
echo "===== SOC / ANDROID ====="
getprop ro.soc.model
getprop ro.board.platform
getprop ro.build.version.release
uname -m

echo
echo "===== MEMORY ====="
grep -E 'MemTotal|MemAvailable|SwapTotal|SwapFree' /proc/meminfo

echo
echo "===== STORAGE ====="
df -h "$HOME" /storage/emulated/0

echo
echo "===== EXISTING NPU / LITERT FILES ====="
find "$HOME" /storage/emulated/0/Download -type f \
  \( -iname '*litert*' -o -iname '*neuron*' -o -iname '*mediatek*' \
     -o -iname '*.litertlm' \) 2>/dev/null | head -80
```

Do not start the model during gameplay. If available RAM is low, stop here and review the memory report before loading the model.

## Device-side smoke test

After the workflow artifact and model are downloaded, extract the runtime into a private directory under Termux `$HOME`, not into Android system/vendor locations.

Confirm that the executable is Android ARM64 and that the dispatch/prebuilt shared libraries are present. Then run the upstream-documented NPU backend command, capturing the complete output:

```sh
RUNTIME_DIR="$HOME/nyamist-npu-test/runtime"
MODEL="$HOME/.local/share/nyamist/models/mediatek-mt6991/Gemma3-1B-IT_q4_ekv1280_mt6991.litertlm"
BIN="$RUNTIME_DIR/litert_lm_main"

mkdir -p "$HOME/nyamist-npu-test"
test -x "$BIN"
test -s "$MODEL"
file "$BIN"
find "$RUNTIME_DIR" -maxdepth 2 -type f -name '*.so' -print

LD_LIBRARY_PATH="$RUNTIME_DIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
  "$BIN" --backend=npu --model_path="$MODEL" \
  2>&1 | tee "$HOME/nyamist-npu-test/mt6991-npu-smoke.log"
```

The upstream command is intentionally retained. Do not add CPU/GPU fallback flags or silently substitute a different model. If the process reports a missing library, permission denial, unsupported device, delegate/dispatch failure, or backend fallback, preserve the complete output and stop; do not copy vendor libraries from another phone or modify SELinux/system partitions.

If the binary cannot execute from Termux because of Android process-domain or SELinux restrictions, that is a meaningful result for the Termux route, not proof that the NPU silicon is unsupported. A signed Android APK using the documented LiteRT API may need to be tested as a separate application-runtime route.

## Result classification

Only assign one of these results after collecting the full logs:

- **BUILD_ONLY** — Android ARM64 binary and dispatch library compiled; no device execution yet.
- **RUNTIME_LOAD_FAILED** — executable starts but MediaTek backend/dispatch cannot load.
- **DEVICE_UNSUPPORTED** — backend explicitly rejects the SoC/device or required runtime capability.
- **NPU_INFERENCE_CONFIRMED** — MT6991-specific model loads, the MediaTek NPU backend initializes without CPU/GPU fallback, and a non-empty model response is produced. Capture runtime logs that identify the MediaTek/Neuron NPU path.
- **INCONCLUSIVE** — command exits without enough backend evidence to distinguish NPU execution from fallback.

A successful response alone is insufficient if the backend may have fallen back to CPU. Likewise, merely listing an NPU or finding a vendor library is not proof of inference.

## Safety and project integration

- Run the proof only after the game is stopped; never concurrently with Stranded Deep.
- Keep all runtime binaries and model data under NYAMIST-owned Termux storage.
- No root, vendor/system partition writes, boot-image changes, SELinux changes, or modifications to GPU drivers.
- Keep the NPU experiment separate from PanVK/Vulkan compute. An NPU result does not prove GPU compute support.
- Do not feed model output directly into runtime configuration. It remains untrusted, advisory text until the deterministic NYAMIST policy and evidence validator approve it.
- Preserve raw logs, artifact checksums, model identifier, device build fingerprint, and test date with every result.

## Next step after the first proof

If NPU inference is confirmed, benchmark memory, prompt latency, token generation rate, thermal behavior, and power with the game stopped. Only then decide whether the same LiteRT-LM runtime can host the advisor's bounded log-analysis prompt and whether it fits the device's resource budget.
