#!/usr/bin/env bash
# Build NyaPANVK from pinned PanVK/Mesa source. Output is an app-local ICD artifact.
# This script never installs the driver or changes Android system/vendor files.
set -Eeuo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORK="${NYA_WORK_DIR:-$ROOT/.nya-build/panvk}"
OUT="${NYA_OUT_DIR:-$ROOT/dist/nya-panvk}"
ANDROID_API="${ANDROID_API:-35}"
PROFILE="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["components"]["panvk"]["buildProfile"])' "$ROOT/configs/component-locks.json")"
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
    "packagedProfiles": ["g52-v7-jm", "g57-v9-jm", "g610-v10-csf", "g615-v11-csf", "g720-v12-csf"],
    "profilePackagingPolicy": "Five metadata-labelled packages reuse this same universal ICD binary; profile labels do not create five separate machine-code drivers.",
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

# PanPlay accepts imported Vulkan drivers as flat .adpkg.zip files.
# The pinned upstream tree is one universal v6-v14 ICD; each profile package
# carries a distinct target profile and provenance, while reusing the same .so.
mapfile -t PROFILE_IDS < <(python3 - "$ROOT/configs/component-locks.json" <<'PY'
import json, sys
lock = json.load(open(sys.argv[1], encoding="utf-8"))
for item in lock["components"]["panvk"]["buildProfiles"]:
    print(item["id"])
PY
)
EXPECTED_PROFILES=(g52-v7-jm g57-v9-jm g610-v10-csf g615-v11-csf g720-v12-csf)
[[ "\${#PROFILE_IDS[@]}" -eq 5 ]] || { echo "Expected exactly five NyaPANVK profiles" >&2; exit 1; }
[[ "\${PROFILE_IDS[*]}" == "\${EXPECTED_PROFILES[*]}" ]] || {
  echo "NyaPANVK profile list differs from the five locked profile targets" >&2
  exit 1
}
PROFILE_PACKAGE_ROWS="$WORK/profile-packages.tsv"
: > "$PROFILE_PACKAGE_ROWS"
for PROFILE_ID in "\${PROFILE_IDS[@]}"; do
  PROFILE_JSON="$BUILDER/profiles/$PROFILE_ID.json"
  [[ -s "$PROFILE_JSON" ]] || { echo "Missing pinned upstream profile: $PROFILE_JSON" >&2; exit 1; }
  PROFILE_GPU="$(python3 -c 'import json,sys;print(json.load(open(sys.argv[1]))["gpu"])' "$PROFILE_JSON")"
  PROFILE_ARCH="$(python3 -c 'import json,sys;print(json.load(open(sys.argv[1]))["panArch"])' "$PROFILE_JSON")"
  PROFILE_FRONTEND="$(python3 -c 'import json,sys;print(json.load(open(sys.argv[1]))["frontend"])' "$PROFILE_JSON")"
  PROFILE_PATCH_SERIES_ID="$(python3 "$BUILDER/scripts/compute-patch-series-id.py" --profile "$PROFILE_ID")"
  PACKAGE_NAME="NyaPANVK-$MESA_VERSION-$PATCH_LEVEL-git$DRIVER_GIT_SHA8-$PROFILE_ID.adpkg.zip"
  PACKAGE_DIR="$WORK/packages/$PROFILE_ID"
  rm -rf "$PACKAGE_DIR"
  mkdir -p "$PACKAGE_DIR"
  cp "$DRIVER_SO" "$PACKAGE_DIR/libvulkan_panfrost.so"
  cp "$OUT/BUILD.json" "$PACKAGE_DIR/BUILD.json"
  cp "$ROOT/configs/component-locks.json" "$PACKAGE_DIR/component-locks.json"
  cp "$BUILDER/NOTICE.md" "$PACKAGE_DIR/NOTICE.md"
  cp "$PROFILE_JSON" "$PACKAGE_DIR/PROFILE.json"
  cp -a "$BUILDER/LICENSES" "$PACKAGE_DIR/LICENSES"
  export PROFILE_ID PROFILE_GPU PROFILE_ARCH PROFILE_FRONTEND PROFILE_PATCH_SERIES_ID PACKAGE_NAME PROFILE_JSON
  python3 - "$OUT/SOURCE.json" "$PACKAGE_DIR/SOURCE.json" "$PACKAGE_DIR/meta.json" <<'PY'
import json, os, pathlib, sys
base_source = json.loads(pathlib.Path(sys.argv[1]).read_text())
profile = json.loads(pathlib.Path(os.environ["PROFILE_JSON"]).read_text())
source = dict(base_source)
source["artifact"] = os.environ["PACKAGE_NAME"]
source["buildProfile"] = os.environ["PROFILE_ID"]
source["profileDetails"] = profile
source["patchSeriesId"] = os.environ["PROFILE_PATCH_SERIES_ID"]
source["profilePackagingPolicy"] = (
    "Profile-labelled PanPlay package over one universal v6-v14 ICD binary; "
    "the Vulkan driver selects GPU architecture/frontend at runtime."
)
pathlib.Path(sys.argv[2]).write_text(json.dumps(source, indent=2) + "\n")
meta = {
    "schemaVersion": 1,
    "name": "NyaPANVK",
    "author": "Mesa / PanVK contributors; downstream build by NYAMIST-LAYER",
    "packageVersion": (
        f"{os.environ['MESA_VERSION']}-{os.environ['PATCH_LEVEL']}-"
        f"git{os.environ['DRIVER_GIT_SHA8']}-{os.environ['PROFILE_ID']}"
    ),
    "driverVersion": f"Mesa {os.environ['MESA_VERSION']}",
    "description": (
        "Experimental Android ARM64 universal PanVK Vulkan ICD for Mali GPUs "
        "using the mali_kbase kernel interface. "
        f"Profile package: {os.environ['PROFILE_ID']} / {profile['gpu']} "
        f"(Pan arch {profile['panArch']}, {profile['frontend']}). "
        "This is a metadata-labelled profile package over the same universal "
        "v6-v14 driver binary, not a separate architecture-specific binary. "
        "Upstream Mesa/PanVK identity and licenses are preserved."
    ),
    "libraryName": "libvulkan_panfrost.so",
    "sha256": os.environ["DRIVER_SHA256"],
    "sourceCommit": os.environ["MESA_PIN"],
    "patchSeriesId": os.environ["PROFILE_PATCH_SERIES_ID"],
    "buildId": os.environ["MESA_PATCHED_HEAD"],
    "profile": os.environ["PROFILE_ID"],
    "androidApi": int(os.environ["ANDROID_API"]),
    "abi": "arm64-v8a"
}
pathlib.Path(sys.argv[3]).write_text(json.dumps(meta, indent=2) + "\n")
PY
  (
    cd "$PACKAGE_DIR"
    sha256sum libvulkan_panfrost.so meta.json SOURCE.json BUILD.json PROFILE.json > SHA256SUMS.txt
    zip -qr "$OUT/$PACKAGE_NAME" .
  )
  unzip -t "$OUT/$PACKAGE_NAME" >/dev/null
  PACKAGE_ENTRIES="$WORK/$PROFILE_ID-package-entries.txt"
  unzip -Z1 "$OUT/$PACKAGE_NAME" > "$PACKAGE_ENTRIES"
  grep -Fxq "libvulkan_panfrost.so" "$PACKAGE_ENTRIES"
  grep -Fxq "meta.json" "$PACKAGE_ENTRIES"
  grep -Fxq "PROFILE.json" "$PACKAGE_ENTRIES"
  sha256sum "$OUT/$PACKAGE_NAME" > "$OUT/$PACKAGE_NAME.sha256"
  printf '%s\t%s\t%s\t%s\t%s\n' \
    "$PROFILE_ID" "$PACKAGE_NAME" "$PROFILE_GPU" "$PROFILE_ARCH" "$PROFILE_FRONTEND" \
    >> "$PROFILE_PACKAGE_ROWS"
  echo "NyaPANVK profile package ready: $OUT/$PACKAGE_NAME"
done
python3 - "$PROFILE_PACKAGE_ROWS" "$OUT/PROFILE-PACKAGES.json" "$DRIVER_SHA256" <<'PY'
import json, pathlib, sys
rows = []
for line in pathlib.Path(sys.argv[1]).read_text().splitlines():
    profile, filename, gpu, arch, frontend = line.split("\t")
    rows.append({
        "profile": profile, "file": filename, "gpu": gpu,
        "panArch": int(arch), "frontend": frontend,
        "sharedUniversalIcdSha256": sys.argv[3]
    })
pathlib.Path(sys.argv[2]).write_text(json.dumps({
    "schemaVersion": 1,
    "packageCount": len(rows),
    "binaryCount": 1,
    "policy": "Five profile-labelled packages share one universal v6-v14 ICD binary.",
    "packages": rows
}, indent=2) + "\n")
assert len(rows) == 5
PY
(
  cd "$OUT"
  sha256sum "$DRIVER_NAME" ./*.adpkg.zip > SHA256SUMS.txt
)
echo "NyaPANVK universal Android ARM64 library: $OUT/$DRIVER_NAME"
echo "NyaPANVK profile packages: 5"
echo "Shared driver SHA-256: $DRIVER_SHA256"
