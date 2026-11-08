#!/usr/bin/env bash
# Build NyaPANVK from pinned PanVK/Mesa source. Output is an app-local ICD artifact.
# This script never installs the driver or changes Android system/vendor files.
set -Eeuo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORK="${NYA_WORK_DIR:-$ROOT/.nya-build/panvk}"
OUT="${NYA_OUT_DIR:-$ROOT/dist/nya-panvk}"
ANDROID_API="${ANDROID_API:-35}"
PROFILE="g615-v11-csf"
JOBS="${NYA_BUILD_JOBS:-2}"
BUILDER_REPO="https://github.com/zenithblue-oss/panvk-kbase-android.git"
BUILDER_COMMIT="cf70e02b918e1864cbcd746a7fefa57b96c673ce"
need() { command -v "$1" >/dev/null 2>&1 || { echo "Missing prerequisite: $1" >&2; exit 2; }; }
for cmd in git python3 cmake meson ninja pkg-config clang clang++ readelf file sha256sum; do need "$cmd"; done
: "${ANDROID_NDK_ROOT:?Set ANDROID_NDK_ROOT to Android NDK r29.}"
NDK_HOST="linux-x86_64"
if [[ ! -x "$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/$NDK_HOST/bin/aarch64-linux-android${ANDROID_API}-clang" ]]; then NDK_HOST="linux-$(uname -m)"; fi
if [[ ! -x "$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/$NDK_HOST/bin/aarch64-linux-android${ANDROID_API}-clang" && "$(uname -m)" == "aarch64" ]]; then NDK_HOST="linux-arm64"; fi
[[ -x "$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/$NDK_HOST/bin/aarch64-linux-android${ANDROID_API}-clang" ]] || { echo "Android NDK compiler missing for API $ANDROID_API under $ANDROID_NDK_ROOT" >&2; exit 2; }

mkdir -p "$WORK" "$OUT"
BUILDER="$WORK/panvk-kbase-android"
if [[ ! -d "$BUILDER/.git" ]]; then git clone --filter=blob:none --no-checkout "$BUILDER_REPO" "$BUILDER"; fi
git -C "$BUILDER" fetch --depth=1 origin "$BUILDER_COMMIT"
git -C "$BUILDER" checkout --detach FETCH_HEAD
[[ "$(git -C "$BUILDER" rev-parse HEAD)" == "$BUILDER_COMMIT" ]] || { echo "PanVK builder commit pin mismatch" >&2; exit 1; }
git -C "$BUILDER" reset --hard "$BUILDER_COMMIT"
git -C "$BUILDER" clean -fdx

"$BUILDER/scripts/fetch-mesa.sh" "$WORK/source-work"
MESA_DIR="$WORK/source-work/mesa"
MESA_PIN="$(python3 -c 'import json,sys;print(json.load(open(sys.argv[1]))["mesaCommit"])' "$BUILDER/sources.lock")"
[[ "$MESA_PIN" == "5a07217f034b3e50d8c7c7794f97a2df1742613b" ]] || { echo "Unexpected Mesa source pin: $MESA_PIN" >&2; exit 1; }

# Strict universal v6-v14 patch stack. Any patch drift, offset or fuzz aborts.
sh "$BUILDER/scripts/prepare-rc5-tree.sh" --mesa "$MESA_DIR"
PATCH_LEVEL="$(cat "$MESA_DIR/.panvk-patchlevel")"
[[ "$PATCH_LEVEL" == "p0175" ]] || { echo "Expected PanVK patch level p0175, got $PATCH_LEVEL" >&2; exit 1; }

STAGING="$WORK/staging"
rm -rf "$STAGING"
mkdir -p "$STAGING"
MESA="$MESA_DIR" BDIR="$WORK/build/android-bionic" DDIR="$STAGING" PANVK_JOBS="$JOBS" \
  bash "$BUILDER/scripts/build-android.sh" --profile "$PROFILE" --api "$ANDROID_API" --ndk "$ANDROID_NDK_ROOT"

DRIVER_SO="$STAGING/libvulkan_panfrost.so"
[[ -s "$DRIVER_SO" ]] || { echo "PanVK build did not produce libvulkan_panfrost.so" >&2; exit 1; }
"$BUILDER/scripts/validate-binary.sh" --abi android "$DRIVER_SO"

DRIVER_SHA256="$(sha256sum "$DRIVER_SO" | awk '{print $1}')"
DRIVER_GIT_SHA8="$(python3 "$BUILDER/scripts/driver_identity.py" --field sha8 "$DRIVER_SO")"
PATCH_SERIES_ID="$(python3 "$BUILDER/scripts/compute-patch-series-id.py" --profile "$PROFILE")"
MESA_VERSION="$(python3 -c 'import json,sys;print(json.load(open(sys.argv[1]))["mesaVersion"])' "$BUILDER/sources.lock")"
DRIVER_NAME="NyaPANVK-${MESA_VERSION}-$PATCH_LEVEL-git$DRIVER_GIT_SHA8-android-arm64.so"

cp "$DRIVER_SO" "$OUT/$DRIVER_NAME"
cp "$BUILDER/NOTICE.md" "$OUT/NOTICE.md"
cp -a "$BUILDER/LICENSES" "$OUT/LICENSES"
cp "$ROOT/configs/component-locks.json" "$OUT/component-locks.json"

export DRIVER_NAME DRIVER_SHA256 DRIVER_GIT_SHA8 PATCH_SERIES_ID MESA_PIN MESA_VERSION
export PATCH_LEVEL ANDROID_API PROFILE BUILDER_COMMIT
export BUILDER_REV="$(git -C "$BUILDER" rev-parse HEAD)"
export MESA_PATCHED_HEAD="$(git -C "$MESA_DIR" rev-parse HEAD)"
export NDK_REVISION="$(awk -F ' = ' '/Pkg.Revision/ {print $2; exit}' "$ANDROID_NDK_ROOT/source.properties")"
export NDK_HOST
python3 - "$OUT/SOURCE.json" "$OUT/BUILD.json" <<'PY'
import json, os, pathlib, platform, subprocess, sys
source_path, build_path = map(pathlib.Path, sys.argv[1:])
source = {
    "schemaVersion": 1,
    "displayName": "NyaPANVK",
    "artifact": os.environ["DRIVER_NAME"],
    "artifactSha256": os.environ["DRIVER_SHA256"],
    "upstreamBuilder": "https://github.com/zenithblue-oss/panvk-kbase-android",
    "upstreamBuilderCommit": os.environ["BUILDER_REV"],
    "mesaRepository": "https://gitlab.freedesktop.org/mesa/mesa.git",
    "mesaBaseCommit": os.environ["MESA_PIN"],
    "mesaVersion": os.environ["MESA_VERSION"],
    "patchedMesaHead": os.environ["MESA_PATCHED_HEAD"],
    "patchSeriesId": os.environ["PATCH_SERIES_ID"],
    "patchLevel": os.environ["PATCH_LEVEL"],
    "driverReportedMesaSha8": os.environ["DRIVER_GIT_SHA8"],
    "buildProfile": os.environ["PROFILE"],
    "profileScope": "universal Mali v6-v14 Android ICD",
    "target": "aarch64-linux-android / arm64-v8a",
    "androidApi": int(os.environ["ANDROID_API"]),
    "ndkRevision": os.environ["NDK_REVISION"],
    "identityPolicy": "Nya is a downstream artifact label; upstream Vulkan driver identity is retained.",
    "hardwareValidation": "not asserted by build job; device validation is a separate gate"
}
build = {
    "schemaVersion": 1,
    "status": "compiled-and-statically-validated; hardware-validation-pending",
    "host": platform.platform(),
    "python": platform.python_version(),
    "ndkHost": os.environ["NDK_HOST"]
}
for tool in ("clang", "meson", "ninja"):
    try: build[tool] = subprocess.check_output([tool, "--version"], text=True).splitlines()[0]
    except Exception: build[tool] = "unknown"
source_path.write_text(json.dumps(source, indent=2) + "\n")
build_path.write_text(json.dumps(build, indent=2) + "\n")
PY
(cd "$OUT" && sha256sum "$DRIVER_NAME" > SHA256SUMS.txt)
echo "NyaPANVK build ready: $OUT/$DRIVER_NAME"
echo "SHA-256: $DRIVER_SHA256"
