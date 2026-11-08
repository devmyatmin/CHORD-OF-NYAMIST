# Nya component builds

## Product naming and upstream identity

The Nya prefix identifies downstream build outputs maintained for the NYAMIST graphics runtime:

- NyaPANVK: Android ARM64 PanVK Vulkan driver.
- NyaDXVK: Direct3D 8/9/10/11 to Vulkan component package.
- NyaVKD3D: Direct3D 12 to Vulkan component package.
- NyaBuild: pinned source orchestration, build, packaging and provenance.

Nya is an outer distribution label. It does not claim that upstream projects were renamed, authored by NYAMIST, or stripped of attribution. Keep upstream driverName, driverInfo, DLL version resources, licenses, source commits and patch provenance intact. NYAMIST core remains a sidecar and does not load or replace these components.

## Pinned source inputs

The machine-readable source lock is configs/component-locks.json. Build scripts deliberately use immutable commits rather than moving branch names.

### NyaPANVK

The builder is zenithblue-oss/panvk-kbase-android at cf70e02b918e1864cbcd746a7fefa57b96c673ce. It pins Mesa to 5a07217f034b3e50d8c7c7794f97a2df1742613b and applies the upstream universal patch series with prepare-rc5-tree.sh. The selected g615-v11-csf build profile is used only after the universal v6-v14 tree is prepared; it is not intended to limit the resulting ICD to G615.

The pinned recipe expects patch level p0175 and Mesa driver identity git-795ca7ff3e, matching the PanVK RC5 identity recorded from the REDMI K80 Ultra tests. The inspected PanPlay driver SHA-256 is 62c2b3f98ca987d5ab50b6f01e2d2c1c5b3ee995fe93666aa5da9415f65c3348. That is a reference hash for comparison, not a promise that a new build will be byte-identical.

The produced NyaPANVK artifact is a standalone Android arm64-v8a shared library with SOURCE.json, BUILD.json, SHA256SUMS.txt, upstream NOTICE.md and LICENSES. The build job performs static ELF/ICD checks only. Vulkan loading, PanProbe, surface presentation and sustained device tests remain separate hardware gates.

### NyaDXVK and NyaVKD3D

The pinned builder recipe is zenithblue-oss/panplay at 469b997a061b9bd404d32b52c9d78c5ee228c903. It builds DXVK from doitsujin/dxvk v3.1.1 / b1a1c99ab52b687cf950d62c88bc2fa316b41663 with the two pinned PanPlay patches, using the pinned llvm-mingw archive and checksum. It also builds VKD3D-Proton at 2230755878b01993b4b82d0ac0c0624f3ab4d333. Both packages contain ARM64EC and i686 targets.

Important version reconciliation: the inspected PanPlay APK contains dxvk-3.1.1-5-arm64ec.wcp, but the pinned public builder commit currently declares and builds 3.1.1-4-arm64ec. The first NyaDXVK build therefore keeps the builder's truthful 3.1.1-4 version and marks APK parity as unverified. Do not rename it to 3.1.1-5 or claim it reproduces the APK until the missing source revision or patch is found and pinned.

The WCP internal profile continues to identify its component as DXVK or VKD3D for loader compatibility. Nya appears in the outer artifact filename and provenance metadata.

## Build locally

For NyaPANVK, install the host Mesa build prerequisites, Android SDK command-line tools and Android NDK r29 (revision 29.0.14206865). Set ANDROID_NDK_ROOT to that NDK directory:

~~~sh
export ANDROID_NDK_ROOT="$ANDROID_HOME/ndk/29.0.14206865"
NYA_BUILD_JOBS=2 bash tools/build-nya-panvk.sh
~~~

For NyaDXVK and NyaVKD3D, install git, curl, Python 3, Meson, Ninja, tar and zstd:

~~~sh
NYA_BUILD_JOBS=2 bash tools/build-nya-dxvk.sh
~~~

Both scripts place disposable build trees under .nya-build/ and generated artifacts under dist/. They do not write to /vendor, /system, the kernel, SELinux policy, or the stock Vulkan driver.

## GitHub Actions

The manually dispatched workflow is .github/workflows/component-build.yml. Select panvk, dxvk or all. The PanVK job uses Android NDK r29 and performs static binary validation. The DXVK job verifies WCP integrity and embeds upstream source, patch and toolchain provenance. Pull requests run only lightweight lockfile and shell-syntax checks; they do not launch expensive cross-builds.

## Release and device gates

A successful hosted build is not a device validation result. Do not promote a build to beta, RC or stable or replace the current app-local component until:

1. SHA-256 and source provenance are reviewed.
2. The new PanVK library loads through the app-local selector and enumerates the expected Vulkan physical device.
3. PanProbe passes on the target GPU and relevant Android WSI/surface paths are tested.
4. NyaDXVK D3D11 tests pass with the exact target architecture and runtime.
5. A real game is run for at least five minutes with clean process exit, frame-time evidence, and explicit Present/device-loss evidence where available.
6. A/B tests and rollback to the current RC5 and stock driver are documented.

The stock Android Vulkan driver is the rollback path. Never overwrite vendor libraries, chmod /dev/mali0, flash a kernel, or weaken SELinux.
