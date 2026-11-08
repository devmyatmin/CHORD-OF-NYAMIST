# REDMI K80 Ultra game-validation runbook

This runbook prepares a real-game test of the existing PanPlay + PanVK +
translator path. NYAMIST remains a read-only sidecar: it does not launch the
game, load the driver, intercept Vulkan calls, or change game settings.

## Scope and order

1. Validate a real D3D11 game with DXVK + PanVK RC5 first.
2. Keep the game, save, scene, resolution, graphics options, and driver build
   fixed while collecting the first baseline.
3. Treat D3D12 / VKD3D-Proton as a separate experimental test. Do not combine
   D3D11 and D3D12 results.
4. Do not replace the stock Android Vulkan driver or modify /vendor,
   /dev/mali0, SELinux, or upstream-owned shader/pipeline caches.

## 1. Update and build NYAMIST

From the NYAMIST checkout in Termux:

~~~sh
git fetch origin feature/modular-graphics-runtime
git checkout feature/modular-graphics-runtime
git pull --ff-only origin feature/modular-graphics-runtime
pkg install git clang cmake ninja pkg-config python
chmod +x tools/termux-build-core.sh
./tools/termux-build-core.sh
~~~

The build script compiles the portable C++20 sidecar and runs its tests. It
does not build PanVK, DXVK, VKD3D-Proton, Wine, or the game runtime.

## 2. Prepare the D3D11 game run

In PanPlay, select the already-tested app-local PanVK RC5 driver and the
intended DXVK D3D11 runtime. Do not switch drivers or modify settings during
the baseline run. Confirm the game is actually using the D3D11/DXVK path;
a D3D9-only or Vulkan-native game is not a D3D11 result.

Before launching:

- Record the game title/version, save or scene, resolution, graphics preset,
  driver identity/build, DXVK version, and selected compatibility options.
- Start PanPlay PerfRecorder before launching the game, with DXVK statistics
  enabled where the launcher supports that option.
- Export the SessionLogs ZIP and the matching PerfRecorder ZIP after the run.
  The performance ZIP ID must match the session's performance ID (perfId).

## 3. First sustained run

Play a repeatable real-game scene for at least 5 minutes (300 seconds). Prefer
a scene with representative movement, camera changes, effects, and asset
streaming rather than sitting in a static menu. Note visible corruption,
missing textures, shader compilation stutter, audio issues, input latency,
crashes, and whether the Android surface remains visible.

Do not use the synthetic Cube FPS as a substitute for a real game. A clean
20-second feature-level test is only a smoke test, not sustained stability.

If the game crashes or freezes, preserve and export the session anyway. A
failed run is useful diagnostic evidence; do not repeatedly change several
settings at once.

## 4. Import and assess the exported evidence

Copy both ZIP files into the NYAMIST checkout, then run:

~~~sh
mkdir -p build/game-tests
python3 tools/import_panplay.py /path/to/session-logs.zip \
  --performance-zip /path/to/perf-recorder.zip \
  --output build/game-tests/d3d11-session.json \
  --report-output build/game-tests/d3d11-translator.env

python3 tools/check_game_session.py build/game-tests/d3d11-session.json \
  --minimum-duration-seconds 300 \
  --output build/game-tests/d3d11-review.json

./build-termux/nyamist-cli inspect-dxvk build/game-tests/d3d11-translator.env
~~~

The checker reports REVIEW_READY, INCOMPLETE, or BLOCKED:

- REVIEW_READY means the exported session has the duration, clean-exit,
  DXVK frame samples, and p95 data needed for human performance review.
- INCOMPLETE means one or more required evidence fields or duration targets
  are missing.
- BLOCKED means the process failed or the log scan found a device-loss marker.

REVIEW_READY is not a rendering-validation pass. Current PanPlay exports do
not provide explicit successful Vulkan Present calls, explicit zero Present
failures, or an authoritative device-loss counter. The imported translator
report therefore remains discovered; do not promote it to smoke_tested,
sustained_tested, or production_ready.

## 5. Repeatability and A/B plan

For a useful first compatibility assessment, run the same D3D11 game scene
again after a cold game/container start and export a second matching pair of
ZIPs. Keep scene, graphics options, resolution, and driver/runtime identity
unchanged. Record ambient/device temperature, battery state, and any thermal
throttling visible in the launcher.

A formal NYAMIST A/B decision requires at least three independent sustained
runs for both baseline and candidate, each at least 300 seconds, on the same
workload and hardware identity, with distinct compatibility profile keys,
fresh p95 telemetry, explicit render/Present success, explicit zero Present
failures, explicit zero device-loss events, and normal process exit. The
current PanPlay importer cannot provide all of those counters, so it cannot
yet produce a formal A/B pass or automatically apply tuning.

## 6. D3D12 follow-up

Only after a D3D11 run is repeatable, perform a separate VKD3D-Proton D3D12
session. The currently recorded device test reached feature level 11_1; FL12_0
through FL12_2 remain unsupported in that recorded configuration. Do not
assume a D3D12 game requiring FL12_x will run.

## What to send for review

Send the exported SessionLogs ZIP and matching PerfRecorder ZIP, or the
generated d3d11-session.json plus d3d11-review.json. Include the game
version, scene, resolution, graphics preset, and a note describing anything
visually wrong. Preserve the original ZIPs so the manifest hashes and log
inventory remain available for forensic review.
