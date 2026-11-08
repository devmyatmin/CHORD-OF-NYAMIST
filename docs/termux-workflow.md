# Termux workflow

Use Termux on the REDMI K80 Ultra for source checkout, portable C++ builds, read-only device facts, and local evidence handling.

## 1. Install the required Termux packages

~~~sh
pkg update
pkg install git clang cmake ninja pkg-config python
~~~

No root access, Termux:API add-on, or Android system modification is required for these steps.

## 2. Clone or update the repository

For a fresh checkout:

~~~sh
git clone https://github.com/devmyatmin/NYAMIST-LAYER.git
cd NYAMIST-LAYER
~~~

For an existing checkout:

~~~sh
cd ~/NYAMIST-LAYER
git switch main
git pull --ff-only origin main
~~~

The active baseline is now on `main`; do not use the retired review branch `baseline/dx11-first`.

## 3. Capture read-only device information

~~~sh
chmod +x tools/termux-probe.sh tools/termux-build-core.sh tools/export-device-probe.sh
./tools/termux-probe.sh | tee k80ultra-probe.txt
./tools/export-device-probe.sh
~~~

The first report is a human-readable diagnostic snapshot. The exporter writes a timestamped schema-v1 `.env` file for the NYAMIST device-probe importer. It intentionally does not query the application's Vulkan context or infer that PanVK is active.

## 4. Build and test the NYAMIST sidecar

~~~sh
./tools/termux-build-core.sh
~~~

This configures CMake in `build-termux/`, compiles the portable C++20 sidecar, runs CTest, and prints the CLI advisory. It does **not** build PanVK, DXVK, Wine/FEX/Box64, or a Windows game.

To inspect the exported device facts after the build:

~~~sh
./build-termux/nyamist-cli inspect-device nyamist-device-probe-YYYYMMDD-HHMMSS.env
~~~

Replace the filename with the actual report printed by `export-device-probe.sh`.

## Safety and interpretation

- A Termux permission denial or missing `/dev/mali0` visibility does not prove that the GPU is unsupported. The Android application sandbox and the intended driver-loading path matter.
- Do not chmod the Mali device node, replace vendor libraries, modify `/vendor`, change SELinux, flash a kernel, or force a global non-stock ICD.
- Validate PanVK only through its reversible app-local runtime path. The Vulkan driver reported by the actual game/container process is the relevant runtime evidence.
- Keep NYAMIST read-only with respect to PanVK, DXVK, Wine/FEX/Box64, PanPlay, and their caches.

## Report handling

Keep the original PanProbe/PanPlay exports alongside normalized NYAMIST reports. Review and redact Android build fingerprints and other identifying log fields before sharing reports publicly. A normalized report is not a substitute for the original evidence.
