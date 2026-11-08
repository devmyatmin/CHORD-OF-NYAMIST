# REDMI K80 Ultra D3D11 game-validation runbook

This runbook validates a real D3D11 game through the existing PanPlay + PanVK + DXVK stack. NYAMIST remains a read-only sidecar: it does not launch the game, load the driver, intercept Vulkan calls, or change game settings.

## Scope

1. Test D3D11 + DXVK + PanVK RC5 first.
2. Keep game version, save/scene, resolution, graphics options, driver build and DXVK version fixed.
3. Treat D3D12 as parked and do not combine it with D3D11 results.
4. Do not replace the stock Android Vulkan driver or modify `/vendor`, `/dev/mali0`, SELinux, or upstream-owned caches.

## 1. Update and build NYAMIST

While this baseline branch is under review, use:

~~~sh
git fetch origin baseline/dx11-first
git checkout baseline/dx11-first
git pull --ff-only origin baseline/dx11-first
pkg install git clang cmake ninja pkg-config python
chmod +x tools/termux-build-core.sh tools/termux-probe.sh
./tools/termux-build-core.sh
~~~

The build compiles the portable C++20 sidecar and runs its tests. It does not build PanVK, DXVK, Wine, or the game runtime.

## 2. Prepare the D3D11 game run

In PanPlay, select the known app-local PanVK RC5 driver and intended DXVK D3D11 runtime. Confirm the game actually uses the D3D11/DXVK path; a D3D9-only or Vulkan-native game is not a D3D11 result.

Before launching:
- Record game title/version, save or scene, resolution, graphics preset, driver identity/build, DXVK version and compatibility options.
- Start PanPlay PerfRecorder before launching, with DXVK statistics enabled where supported.
- Export SessionLogs ZIP and the matching PerfRecorder ZIP. Their performance ID (`perfId`) must match.

## 3. Sustained gameplay run

Play one repeatable, representative scene for at least **300 seconds**. Include movement, camera changes, effects and asset streaming rather than remaining in a static menu. Record visible corruption, missing textures, shader stutter, audio/input problems, crashes and surface loss.

A synthetic Cube benchmark or a short feature-level test is not a substitute for a real game.

If the game crashes/freezes, preserve the exports. Change one variable at a time.

## 4. Import and assess evidence

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

The checker returns:
- **REVIEW_READY** — duration, clean-exit, DXVK frames and p95 evidence are present for human review.
- **INCOMPLETE** — required evidence is missing.
- **BLOCKED** — the process failed or the log scan found a device-loss marker.

REVIEW_READY is not a rendering-validation pass. Current PanPlay exports do not expose explicit successful Vulkan Present calls, explicit zero Present failures, or an authoritative device-loss counter. The imported translator report therefore remains discovered-only.

## 5. Repeatability and A/B

First repeat the same D3D11 scene after a cold game/container start. Keep all workload and runtime inputs unchanged.

A formal NYAMIST A/B decision requires at least three independent 300-second runs per arm, matched workload/device identity, distinct profile keys, fresh p95 telemetry, explicit render/Present success, explicit zero Present failures and device-loss events, and normal process exit. Current PanPlay exports do not yet provide all required counters; do not claim an A/B pass or automatically apply tuning without them.

## What to submit for review

Send the original SessionLogs ZIP and matching PerfRecorder ZIP, or the generated JSON sidecar and review JSON. Include game version, scene, resolution, graphics preset, and any visual issues. Keep the original ZIPs so manifest hashes and log inventory remain available.
