#!/data/data/com.termux/files/usr/bin/bash
set -Eeuo pipefail

SOURCE="$(pwd)"
PROJECT="$HOME/nyamist-device-test"
BUILD="$PROJECT/build"

trap 'rc=$?; echo "BUILD FAILED: line $LINENO (exit $rc)"; exit "$rc"' ERR

echo "===== 1/5 COPY SOURCE ====="
mkdir -p "$PROJECT"
find "$SOURCE" -mindepth 1 -maxdepth 1 \
  ! -name build ! -name 'build-*' ! -name .git \
  -exec cp -a {} "$PROJECT/" \;

echo "===== 2/5 CONFIGURE ====="
cmake -S "$PROJECT" -B "$BUILD" \
  -DCMAKE_BUILD_TYPE=Release \
  -DNYAMIST_BUILD_TESTS=ON \
  -DNYAMIST_BUILD_BENCHMARKS=OFF

echo "===== 3/5 BUILD ====="
cmake --build "$BUILD" --parallel 2

echo "===== 4/5 TESTS ====="
ctest --test-dir "$BUILD" --output-on-failure

CLI="$BUILD/nyamist-cli"

echo "===== 5/5 VULKAN GPU SMOKE ====="
"$CLI" runtime-vulkan-smoke
"$CLI" runtime-vulkan-buffer-smoke
"$CLI" runtime-vulkan-render-pass-smoke
"$CLI" runtime-vulkan-compute-pipeline-smoke \
  "$PROJECT/shaders/nyamist_compute.spv"

echo "===== BUILD COMPLETE ====="
echo "Score baseline: 17/17"
echo "Project: $PROJECT"
