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
need python3
# Mesa requires Meson >=1.4.0; use the exact version recorded in component-locks.json.
source "$ROOT/tools/use-pinned-meson.sh"
for cmd in git cmake ninja pkg-config clang clang++ readelf file sha256sum zip unzip; do need "$cmd"; done
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
# The upstream preparer creates commits for each patch; give this isolated Mesa
# worktree an explicit downstream build identity instead of relying on runner globals.
git -C "$MESA_DIR" config user.name "${NYA_GIT_AUTHOR_NAME:-NYAMIST-LAYER Reproducible Build System}"
git -C "$MESA_DIR" config user.email "${NYA_GIT_AUTHOR_EMAIL:-build-bot@nyamist-layer.invalid}"
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
    "ndkHost": os.environ["NDK_HOST"],
    "mesonPin": os.environ["NYA_MESON_VERSION"]
}
for tool in ("clang", "meson", "ninja"):
    try: build[tool] = subprocess.check_output([tool, "--version"], text=True).splitlines()[0]
    except Exception: build[tool] = "unknown"
source_path.write_text(json.dumps(source, indent=2) + "\n")
build_path.write_text(json.dumps(build, indent=2) + "\n")
PY
(cd "$OUT" && sha256sum "$DRIVER_NAME" > SHA256SUMS.txt)

# PanPlay accepts imported Vulkan drivers as a flat .adpkg.zip with meta.json
# and the Android ICD library at archive root.
PACKAGE_NAME="NyaPANVK-\${MESA_VERSION}-\${PATCH_LEVEL}-git\${DRIVER_GIT_SHA8}.adpkg.zip"
PACKAGE_DIR="$WORK/package"
rm -rf "$PACKAGE_DIR"
mkdir -p "$PACKAGE_DIR"
cp "$DRIVER_SO" "$PACKAGE_DIR/libvulkan_panfrost.so"
cp "$OUT/SOURCE.json" "$PACKAGE_DIR/SOURCE.json"
cp "$OUT/BUILD.json" "$PACKAGE_DIR/BUILD.json"
cp "$ROOT/configs/component-locks.json" "$PACKAGE_DIR/component-locks.json"
cp "$BUILDER/NOTICE.md" "$PACKAGE_DIR/NOTICE.md"
cp -a "$BUILDER/LICENSES" "$PACKAGE_DIR/LICENSES"
python3 - "$PACKAGE_DIR/meta.json" <<'PY'
import json
import os
import sys

meta = {
    "schemaVersion": 1,
    "name": "NyaPANVK",
    "author": "Mesa / PanVK contributors; downstream build by NYAMIST-LAYER",
    "packageVersion": f"{os.environ['MESA_VERSION']}-{os.environ['PATCH_LEVEL']}-git{os.environ['DRIVER_GIT_SHA8']}",
    "driverVersion": f"Mesa {os.environ['MESA_VERSION']}",
    "description": (
        "Experimental Android ARM64 PanVK Vulkan driver for Mali GPUs using "
        "the mali_kbase kernel interface. Downstream NyaPANVK package label; "
        "upstream Mesa/PanVK driver identity and licenses are preserved. "
        f"Source Mesa {os.environ['MESA_PIN']}, patch level {os.environ['PATCH_LEVEL']}. "
        "Import as an additional app-local driver; this package does not modify the system driver."
    ),
    "libraryName": "libvulkan_panfrost.so",
    "sha256": os.environ["DRIVER_SHA256"],
    "sourceCommit": os.environ["MESA_PIN"],
    "patchSeriesId": "sha256:" + os.environ["PATCH_SERIES_ID"],
    "buildId": os.environ["MESA_PATCHED_HEAD"],
    "profile": os.environ["PROFILE"],
    "androidApi": int(os.environ["ANDROID_API"]),
    "abi": "arm64-v8a"
}
with open(sys.argv[1], "w", encoding="utf-8") as f:
    json.dump(meta, f, indent=2)
    f.write("\n")
PY
(
  cd "$PACKAGE_DIR"
  sha256sum libvulkan_panfrost.so meta.json SOURCE.json BUILD.json > SHA256SUMS.txt
  zip -qr "$OUT/$PACKAGE_NAME" .
)
unzip -t "$OUT/$PACKAGE_NAME" >/dev/null
sha256sum "$OUT/$PACKAGE_NAME" > "$OUT/$PACKAGE_NAME.sha256"
unzip -Z1 "$OUT/$PACKAGE_NAME" | grep -qx "libvulkan_panfrost.so"
unzip -Z1 "$OUT/$PACKAGE_NAME" | grep -qx "meta.json"

echo "NyaPANVK raw library: $OUT/$DRIVER_NAME"
echo "NyaPANVK PanPlay import package: $OUT/$PACKAGE_NAME"
echo "Driver SHA-256: $DRIVER_SHA256"
echo "Package SHA-256: $(awk '{print $1}' "$OUT/$PACKAGE_NAME.sha256")"
