# Termux workflow

Termux is the first device-side development environment for NYAMIST. Use it for source checkout, portable C++ builds, logs, and read-only hardware probing.

## One-time setup

```sh
pkg update
pkg install git clang cmake ninja pkg-config python
```

## Clone and probe

```sh
git clone https://github.com/devmyatmin/NYAMIST-LAYER.git
cd NYAMIST-LAYER
chmod +x tools/termux-probe.sh tools/termux-build-core.sh
./tools/termux-probe.sh | tee k80ultra-probe.txt
```

## Build the NYAMIST portable core

```sh
./tools/termux-build-core.sh
```

The script configures CMake, builds the C++20 core, runs unit tests, and prints the target-profile advisor output. It does not build PanVK, DXVK, Wine, or a game runtime.

## What to send back

Send the complete contents of `k80ultra-probe.txt`, after redacting any device build fingerprint if desired. In particular, the Vulkan summary, Android API level, ABI, and `/dev/mali0` visibility determine the next integration step.

Do not run `chmod 666 /dev/mali0`, do not replace vendor libraries, and do not flash a kernel or driver. The probe is read-only.
