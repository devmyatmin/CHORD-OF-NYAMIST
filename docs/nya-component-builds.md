# Nya component builds

## Product naming and upstream identity

The Nya prefix identifies downstream build outputs maintained for the NYAMIST graphics runtime:

- NyaPANVK: Android ARM64 PanVK Vulkan driver.
- NyaDXVK: Direct3D 8/9/10/11 to Vulkan component package.
- NyaVKD3D: Direct3D 12 to Vulkan component package.
- NyaBuild: pinned source orchestration, build, packaging and provenance.

Nya is an outer distribution label. It does not claim that upstream projects were renamed, authored by NYAMIST, or stripped of attribution. Keep upstream driverName, driverInfo, DLL version resources, licenses, source commits and patch provenance intact. NYAMIST core remains a sidecar and does not load or replace these components.

## Pinned source inputs

The machine-readable source lock is configs/component-locks.json. Build scripts deliberately use immutable commits rather than moving branch names. Both build paths bootstrap the exact Meson version in that lock (currently 1.4.2) into an isolated Python virtual environment under `.nya-build/tooling`; they do not rely on the Ubuntu runner’s apt Meson package. The pinned Mesa revision requires Meson >=1.4.0.

### NyaPANVK

The builder is zenithblue-oss/panvk-kbase-android at cf70e02b918e1864cbcd746a7fefa57b96c673ce. It pins Mesa to 5a07217f034b3e50d8c7c7794f97a2df1742613b and applies the upstream universal patch series with prepare-rc5-tree.sh. The selected g615-v11-csf build profile is used only after the universal v6-v14 tree is prepared; it is not intended to limit the resulting ICD to G615.

The build emits five PanPlay-compatible profile packages from the pinned upstream profile set: `g52-v7-jm`, `g57-v9-jm`, `g610-v10-csf`, `g615-v11-csf`, and `g720-v12-csf`. These are five metadata-labelled target packages over one universal v6-v14 `libvulkan_panfrost.so`, not five independently compiled machine-code drivers. Each package records its profile JSON, source provenance, and shared binary SHA-256. The five profile labels must not be read as five hardware validation passes: G52/G610/G720 remain planned, G57 has only limited separate external probe evidence, and G615 is the primary reference profile.

The pinned recipe expects patch level p0175 and Mesa driver identity git-795ca7ff3e, matching the PanVK RC5 identity recorded from the REDMI K80 Ultra tests. The inspected PanPlay driver SHA-256 is 62c2b3f98ca987d5ab50b6f01e2d2c1c5b3ee995fe93666aa5da9415f65c3348. That is a reference hash for comparison, not a promise that a new build will be byte-identical.

The produced NyaPANVK output includes the standalone Android arm64-v8a shared library and five PanPlay-compatible `NyaPANVK-<profile>.adpkg.zip` packages. The import archive contains `libvulkan_panfrost.so`, `meta.json`, SOURCE.json, BUILD.json, checksums, the upstream NOTICE.md and LICENSES. PanPlay's Drivers screen accepts this archive through **Import driver (.adpkg.zip)**. The build job performs static ELF/ICD checks only. Vulkan loading, PanProbe, surface presentation and sustained device tests remain separate hardware gates.

### NyaDXVK and NyaVKD3D

The pinned builder recipe is zenithblue-oss/panplay at 469b997a061b9bd404d32b52c9d78c5ee228c903. It builds DXVK from doitsujin/dxvk v3.1.1 / b1a1c99ab52b687cf950d62c88bc2fa316b41663 with the two pinned PanPlay patches, using the pinned llvm-mingw archive and checksum. It also builds VKD3D-Proton at 2230755878b01993b4b82d0ac0c0624f3ab4d333. Both packages contain ARM64EC and i686 targets.

Important version reconciliation: the inspected PanPlay APK contains dxvk-3.1.1-5-arm64ec.wcp, but the pinned public builder commit currently declares and builds 3.1.1-4-arm64ec. The first NyaDXVK build therefore keeps the builder's truthful 3.1.1-4 version and marks APK parity as unverified. Do not rename it to 3.1.1-5 or claim it reproduces the APK until the missing source revision or patch is found and pinned.

The WCP internal profile continues to identify its component as DXVK or VKD3D for loader compatibility. Nya appears in the outer artifact filename and provenance metadata. Before invoking the pinned upstream build recipe, NyaBuild pre-fetches the exact llvm-mingw release archive using bounded curl retries and verifies its locked SHA-256; the upstream script then re-checks that same digest before extraction.

## Build locally

For NyaPANVK, install the host Mesa build prerequisites, Python 3 venv support, Android SDK command-line tools and Android NDK r29 (revision 29.0.14206865). Set ANDROID_NDK_ROOT to that NDK directory. The build script installs the lockfile-pinned Meson 1.4.2 into an isolated venv:

~~~sh
export ANDROID_NDK_ROOT="$ANDROID_HOME/ndk/29.0.14206865"
NYA_BUILD_JOBS=2 bash tools/build-nya-panvk.sh
~~~

For NyaDXVK and NyaVKD3D, install git, curl, Python 3 with venv support, Ninja, tar and zstd. The same isolated Meson 1.4.2 bootstrap is used automatically:

~~~sh
NYA_BUILD_JOBS=2 bash tools/build-nya-dxvk.sh
~~~

Both scripts place disposable build trees under .nya-build/ and generated artifacts under dist/. They do not write to /vendor, /system, the kernel, SELinux policy, or the stock Vulkan driver.

## GitHub Actions

The workflow is .github/workflows/component-build.yml. Manual dispatch can select panvk, dxvk or all. Pull requests that touch the configured component-build paths run lockfile validation, shell syntax checks, and both component cross-build jobs. The jobs use the pinned Meson venv; PanVK uses Android NDK r29 and performs static binary validation, while DXVK verifies WCP integrity and embeds upstream source, patch and toolchain provenance.

## Import and first real-game test in PanPlay

Use PanPlay on the physical REDMI K80 Ultra for this target. A generic Android Studio virtual device does not expose the phone's Immortalis-G925 Mali GPU or its `mali_kbase` kernel interface; it cannot validate this PanVK build. PanPlay runs the Windows game environment on Android through its Wine/FEX path.

1. Open PanPlay and preserve the bundled PanVK RC5 driver as the known-good rollback choice.
2. Open **Drivers** → **Import driver (.adpkg.zip)** and select the generated `NyaPANVK-*.adpkg.zip`. It installs as an additional app-local driver; it does not replace the bundled driver or Android's stock Vulkan driver.
3. In Drivers, select the bundled RC5 and use **Probe active driver** first. Record the result. Then select NyaPANVK and probe it. If import, load, or enumeration fails, switch back to bundled RC5. Never copy the .so into `/vendor` or change `/dev/mali0` permissions.
4. Add the game executable/container in PanPlay. Start with a D3D11 game and choose **DXVK** as its Render API. Keep the bundled DXVK 3.1.1-5 and bundled PanVK RC5 for the first 300-second baseline. Check that the game actually starts, renders to the Android surface, and accepts touch/gamepad/keyboard input as applicable.
5. After exporting SessionLogs and the matching PerfRecorder ZIP, import them using the commands in `docs/game-validation-runbook.md`. Send both original ZIPs and the generated review JSON for analysis.
6. Repeat the same scene and settings with only NyaPANVK selected while keeping DXVK unchanged. This isolates the driver change. Only after that should NyaDXVK be tested as a separate variable.

PanPlay installs local DXVK/VKD3D component packages from **Components** → **Add from file** → **Install local .wcp**. The current pinned DXVK builder produces `3.1.1-4-arm64ec`, while the inspected APK contains `3.1.1-5-arm64ec`; treat the NyaDXVK package as a separate candidate and do not replace the bundled baseline without a deliberate comparison. Keep VKD3D/D3D12 tests separate from D3D11, and remember the current device evidence only reaches D3D12 feature level 11_1.

The first game run is a compatibility check, not proof of a production-ready driver. Preserve the current RC5 selection and original exports so every failed run can be rolled back and diagnosed.

## Release and device gates

A successful hosted build is not a device validation result. Do not promote a build to beta, RC or stable or replace the current app-local component until:

1. SHA-256 and source provenance are reviewed.
2. The new PanVK library loads through the app-local selector and enumerates the expected Vulkan physical device.
3. PanProbe passes on the target GPU and relevant Android WSI/surface paths are tested.
4. NyaDXVK D3D11 tests pass with the exact target architecture and runtime.
5. A real game is run for at least five minutes with clean process exit, frame-time evidence, and explicit Present/device-loss evidence where available.
6. A/B tests and rollback to the current RC5 and stock driver are documented.

The stock Android Vulkan driver is the rollback path. Never overwrite vendor libraries, chmod /dev/mali0, flash a kernel, or weaken SELinux.
