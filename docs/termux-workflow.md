# Termux workflow

Use Termux for source checkout, portable C++ builds, local logs and read-only device probing.

## Setup

~~~sh
pkg update
pkg install git clang cmake ninja pkg-config python
~~~

## Checkout the focused baseline

~~~sh
git clone -b baseline/dx11-first https://github.com/devmyatmin/NYAMIST-LAYER.git
cd NYAMIST-LAYER
chmod +x tools/termux-probe.sh tools/termux-build-core.sh tools/export-device-probe.sh
~~~

After the baseline is merged, use the repository's `main` branch instead.

## Probe and build

~~~sh
./tools/termux-probe.sh | tee k80ultra-probe.txt
./tools/export-device-probe.sh
./tools/termux-build-core.sh
~~~

The probe/export scripts are read-only. The C++20 build configures CMake, compiles NYAMIST, and runs unit tests. It does not build PanVK, DXVK, Wine/FEX/Box64, or a Windows game.

A Termux permission denial for `/dev/mali0` does not prove the GPU is unsupported. Validate PanVK through its intended app-local launcher path. Never chmod the Mali device node, replace vendor libraries, flash a kernel, or weaken SELinux.

## Report handling

Review and redact device build fingerprints before sharing a probe report. Keep the raw PanProbe/PanPlay exports alongside normalized NYAMIST reports; normalized metadata is not a substitute for the original evidence.
