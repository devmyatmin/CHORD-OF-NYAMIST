#!/data/data/com.termux/files/usr/bin/bash

set +e

PROJECT="$HOME/storage/downloads/nyamist-device-test"
WORK="$HOME/nyamist-device-test"
DOWNLOAD="$HOME/storage/downloads"
STAMP=$(date +%Y%m%d-%H%M%S)
NAME="NYAMIST-FULL-SCORE-$STAMP"
TEMP="$HOME/$NAME"

mkdir -p "$TEMP"

echo "===== NYAMIST FULL SCORE COLLECTION ====="

# 1. Run the complete build and test script
cd "$PROJECT" || exit 1
bash nyamist-build-all.sh 2>&1 | tee "$TEMP/build-and-gpu-results.txt"
echo "${PIPESTATUS[0]}" > "$TEMP/build-exit-code.txt"

# 2. Collect complete project and build directory
[ -d "$WORK" ] && cp -a "$WORK" "$TEMP/project"

# 3. Collect CTest detailed logs
if [ -d "$WORK/build/Testing" ]; then
    cp -a "$WORK/build/Testing" "$TEMP/ctest-logs"
fi

# 4. Device and compiler information
{
    echo "===== DATE ====="
    date
    echo "===== ANDROID ====="
    getprop ro.build.version.release
    getprop ro.product.model
    getprop ro.product.cpu.abi
    echo "===== KERNEL ====="
    uname -a
    echo "===== COMPILER ====="
    c++ --version
    echo "===== CMAKE ====="
    cmake --version
    echo "===== VULKAN ====="
    ls -l "$PREFIX/lib/libvulkan.so" 2>&1
    echo "===== STORAGE ====="
    df -h "$HOME"
} > "$TEMP/device-info.txt" 2>&1

# 5. Source file inventory
find "$PROJECT" -type f \
    ! -path '*/.git/*' \
    > "$TEMP/source-file-list.txt"

# 6. Package everything into Downloads
cd "$HOME" || exit 1
zip -qr "$DOWNLOAD/$NAME.zip" "$NAME"

echo
echo "===== SCORE ZIP READY ====="
ls -lh "$DOWNLOAD/$NAME.zip"
echo "PATH: $DOWNLOAD/$NAME.zip"
