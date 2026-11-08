#!/usr/bin/env bash
# Build NyaDXVK and NyaVKD3D from the pinned PanPlay build recipe.
# Nya is the outer package label; component profile identities remain upstream-compatible.
set -Eeuo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORK="${NYA_WORK_DIR:-$ROOT/.nya-build/dxvk}"
OUT="${NYA_OUT_DIR:-$ROOT/dist/nya-dxvk}"
JOBS="${NYA_BUILD_JOBS:-2}"
BUILDER_REPO="https://github.com/zenithblue-oss/panplay.git"
BUILDER_COMMIT="469b997a061b9bd404d32b52c9d78c5ee228c903"
need() { command -v "$1" >/dev/null 2>&1 || { echo "Missing prerequisite: $1" >&2; exit 2; }; }
need python3
# Keep DXVK/VKD3D on the same pinned Meson toolchain as the PanVK build.
source "$ROOT/tools/use-pinned-meson.sh"
for cmd in git curl ninja zstd tar sha256sum awk; do need "$cmd"; done

mkdir -p "$WORK" "$OUT"
BUILDER="$WORK/panplay-builder"
if [[ ! -d "$BUILDER/.git" ]]; then git clone --filter=blob:none --no-checkout "$BUILDER_REPO" "$BUILDER"; fi
git -C "$BUILDER" fetch --depth=1 origin "$BUILDER_COMMIT"
git -C "$BUILDER" checkout --detach FETCH_HEAD
[[ "$(git -C "$BUILDER" rev-parse HEAD)" == "$BUILDER_COMMIT" ]] || { echo "PanPlay builder commit pin mismatch" >&2; exit 1; }
git -C "$BUILDER" reset --hard "$BUILDER_COMMIT"
git -C "$BUILDER" clean -fdx

. "$BUILDER/scripts/components.env"
BUILD_WORK="$WORK/components-build"
PACKAGE_OUT="$WORK/components"
rm -rf "$BUILD_WORK" "$PACKAGE_OUT"
mkdir -p "$BUILD_WORK" "$PACKAGE_OUT"
WORK="$BUILD_WORK" OUT="$PACKAGE_OUT" JOBS="$JOBS" bash "$BUILDER/scripts/build-dxvk.sh"

DXVK_WCP="$PACKAGE_OUT/dxvk-$DXVK_VERSION.wcp"
VKD3D_WCP="$PACKAGE_OUT/vkd3d-$VKD3D_VERSION.wcp"
[[ -s "$DXVK_WCP" && -s "$VKD3D_WCP" ]] || { echo "Expected DXVK/VKD3D WCP outputs are missing" >&2; exit 1; }
zstd -t "$DXVK_WCP"
zstd -t "$VKD3D_WCP"
zstd -dc "$DXVK_WCP" | tar -tf - | grep -q 'profile.json'
zstd -dc "$VKD3D_WCP" | tar -tf - | grep -q 'profile.json'

DXVK_NAME="NyaDXVK-$DXVK_VERSION.wcp"
VKD3D_NAME="NyaVKD3D-$VKD3D_VERSION-arm64ec-i686.wcp"
cp "$DXVK_WCP" "$OUT/$DXVK_NAME"
cp "$VKD3D_WCP" "$OUT/$VKD3D_NAME"
cp "$BUILD_WORK/dxvk-src/LICENSE" "$OUT/LICENSE-DXVK.txt"
cp "$BUILD_WORK/vkd3d-proton-src/COPYING" "$OUT/COPYING-VKD3D-PROTON.txt"
cp "$BUILD_WORK/vkd3d-proton-src/LICENSE" "$OUT/LICENSE-VKD3D-PROTON.txt"
cp "$BUILDER/scripts/build-dxvk.sh" "$OUT/build-dxvk.upstream.sh"
cp "$BUILDER/scripts/components.env" "$OUT/components.upstream.env"
cp "$BUILDER/scripts/llvm-mingw.sh" "$OUT/llvm-mingw.upstream.sh"
mkdir -p "$OUT/source-patches"
cp "$BUILDER/runtimes/dxvk/clear-before-external-rendering.patch" "$OUT/source-patches/"
cp "$BUILDER/runtimes/dxvk/panplay-stats.patch" "$OUT/source-patches/"

export BUILDER_COMMIT DXVK_TAG DXVK_COMMIT DXVK_VERSION
export VKD3D_TAG VKD3D_COMMIT VKD3D_VERSION LLVM_MINGW_VER LLVM_MINGW_SHA256
export DXVK_NAME VKD3D_NAME JOBS
export PATCH_CLEAR_SHA="$(sha256sum "$BUILDER/runtimes/dxvk/clear-before-external-rendering.patch" | awk '{print $1}')"
export PATCH_STATS_SHA="$(sha256sum "$BUILDER/runtimes/dxvk/panplay-stats.patch" | awk '{print $1}')"
export BUILDER_SCRIPT_SHA="$(sha256sum "$BUILDER/scripts/build-dxvk.sh" | awk '{print $1}')"
export DXVK_SHA="$(sha256sum "$OUT/$DXVK_NAME" | awk '{print $1}')"
export VKD3D_SHA="$(sha256sum "$OUT/$VKD3D_NAME" | awk '{print $1}')"
python3 - "$OUT/SOURCE.json" "$OUT/BUILD.json" <<'PY'
import json, os, pathlib, platform, subprocess, sys
source_path, build_path = map(pathlib.Path, sys.argv[1:])
source = {
    "schemaVersion": 1,
    "artifacts": [
        {
            "displayName": "NyaDXVK",
            "file": os.environ["DXVK_NAME"],
            "sha256": os.environ["DXVK_SHA"],
            "upstreamRepository": "https://github.com/doitsujin/dxvk",
            "upstreamTag": os.environ["DXVK_TAG"],
            "upstreamCommit": os.environ["DXVK_COMMIT"],
            "packageRevisionFromPinnedBuilder": os.environ["DXVK_VERSION"],
            "patches": {
                "clearBeforeExternalRenderingSha256": os.environ["PATCH_CLEAR_SHA"],
                "panplayStatsSha256": os.environ["PATCH_STATS_SHA"]
            },
            "architectureTargets": ["arm64ec", "i686"],
            "identityPolicy": "WCP package label is NyaDXVK; internal DXVK identity/version is retained."
        },
        {
            "displayName": "NyaVKD3D",
            "file": os.environ["VKD3D_NAME"],
            "sha256": os.environ["VKD3D_SHA"],
            "upstreamRepository": "https://github.com/HansKristian-Work/vkd3d-proton",
            "upstreamRef": os.environ["VKD3D_TAG"],
            "upstreamCommit": os.environ["VKD3D_COMMIT"],
            "packageVersion": os.environ["VKD3D_VERSION"],
            "architectureTargets": ["arm64ec", "i686"]
        }
    ],
    "builderRepository": "https://github.com/zenithblue-oss/panplay",
    "builderCommit": os.environ["BUILDER_COMMIT"],
    "builderScriptSha256": os.environ["BUILDER_SCRIPT_SHA"],
    "llvmMingwVersion": os.environ["LLVM_MINGW_VER"],
    "llvmMingwSha256": os.environ["LLVM_MINGW_SHA256"],
    "apkParity": {
        "observedPanPlayApkDxvkComponent": "3.1.1-5-arm64ec",
        "pinnedPublicBuilderOutput": os.environ["DXVK_VERSION"],
        "status": "not-verified; builder output revision differs from the inspected APK component"
    }
}
build = {
    "schemaVersion": 1,
    "status": "compiled-and-package-validated; device-runtime-validation-pending",
    "host": platform.platform(),
    "python": platform.python_version(),
    "mesonVersion": os.environ["NYA_MESON_VERSION"],
    "meson": subprocess.check_output(["meson", "--version"], text=True).strip(),
    "jobs": int(os.environ["JOBS"])
}
source_path.write_text(json.dumps(source, indent=2) + "\n")
build_path.write_text(json.dumps(build, indent=2) + "\n")
PY
(cd "$OUT" && sha256sum "$DXVK_NAME" "$VKD3D_NAME" > SHA256SUMS.txt)
echo "NyaDXVK and NyaVKD3D packages ready in: $OUT"
echo "DXVK package revision: $DXVK_VERSION (the inspected APK contains 3.1.1-5; this pinned source recipe builds $DXVK_VERSION)"
