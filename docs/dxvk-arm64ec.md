# DXVK ARM64EC baseline

## Selected baseline

NYAMIST pins the first reproducible DXVK integration to upstream **DXVK v3.1.1**, commit
`b1a1c99ab52b687cf950d62c88bc2fa316b41663`. The lock file is
`third_party/dxvk.lock.json`.

This is the vanilla upstream source tree, with no GPLAsync or binary-semaphore patch.
The goal is to establish a known, reproducible baseline before adding the NYAMIST
Pipeline Gate or D3D11 Command Governor.

Upstream DXVK does not publish an official ARM64EC build. The workflow therefore
cross-compiles Windows ARM64EC DLLs with the pinned LLVM-MinGW 20251104 toolchain,
and separately builds 32-bit x86 DLLs for the WOW64 side of the package. The
toolchain archive is checked against its SHA-256 before it is used.

## Build workflow

The GitHub Actions workflow is
`.github/workflows/dxvk-arm64ec-build.yml`. It builds:

- `system32/`: ARM64EC `d3d10core.dll`, `d3d11.dll`, `d3d8.dll`,
  `d3d9.dll`, and `dxgi.dll`.
- `syswow64/`: 32-bit x86 versions of the same DLLs.
- `profile.json`: Winlator-compatible DXVK package metadata.
- A `.wcp` (zstd tar) and a plain `.zip` package, plus a build manifest.

The verifier checks ARM64EC-specific PE metadata (including the hybrid thunk and
ARM64EC metadata sections and a non-zero CHPE metadata pointer), verifies that
the WOW64 DLLs are x86 PE files, and checks that the package profile maps every
DLL to the correct target path.

## Installation policy

The workflow only produces build artifacts. It does not install DLLs, modify a
Wine prefix, change a game container, replace the system Vulkan driver, or
touch Android vendor files.

Keep the currently working DXVK package as a rollback copy. Install the new
package only into a separate test prefix/container using that launcher's supported
DXVK/DLL override mechanism. Do not copy these DLLs into Android system folders.
Do not test modified D3D DLLs in competitive online games where DLL replacement
may violate anti-cheat or game rules.

## Device validation gates

A successful cross-build is not a successful game test. Before the package is
considered playable on the REDMI K80 Ultra, record:

1. Exact DXVK version and package SHA-256.
2. Game EXE architecture and Wine/Box64/ARM64EC execution mode.
3. Vulkan device/driver identity from the actual game container.
4. D3D11 device creation and selected feature level.
5. Explicit successful Present calls, zero Present failures, and zero device-lost events.
6. A sustained render/present run of at least 300 seconds.
7. Visual correctness, frame-time percentiles, CPU/GPU load, memory, and thermals.

Keep the stock Android Vulkan driver available as rollback. PanVK remains experimental
until its game-container surface and presentation path is independently proven.
The current NYAMIST core remains a sidecar; it does not intercept every draw call.

## HUD and log capture

The HUD is read by the Wine game process, not by Termux merely setting a variable.
The launcher must inherit this environment or pass it through its own environment
settings. Example for a launcher started from the same shell:

```sh
export DXVK_HUD=version,devinfo,fps,frametimes,submissions,drawcalls,pipelines,memory,gpuload,compiler,cs
export DXVK_LOG_LEVEL=info
mkdir -p "$HOME/nyamist-dxvk-logs"
export DXVK_LOG_PATH="$HOME/nyamist-dxvk-logs"
# Start the game through the launcher that inherits this environment.
```

`gpuload` is an estimate and may be inaccurate. Preserve DXVK's own shader/pipeline
cache; NYAMIST stores only its own metadata and profile records. The initial HUD
run is observational; do not enable an experimental governor or force pipeline
features until a baseline capture is available.
