#!/data/data/com.termux/files/usr/bin/bash

cd "$(dirname "$0")" || exit 1

echo "======================================"
echo "       NYAMIST FULL SYSTEM AUDIT"
echo "======================================"

PASS=0
FAIL=0

check_file() {
    if find include src tests -type f 2>/dev/null | grep -qiE "$1"; then
        echo "[PASS] $2"
        PASS=$((PASS+1))
    else
        echo "[FAIL] $2"
        FAIL=$((FAIL+1))
    fi
}

echo
echo "===== COMPONENT AUDIT ====="

check_file 'vulkan_backend' "Vulkan Backend"
check_file 'vulkan_resource_manager' "Vulkan Resource Manager"
check_file 'vulkan_compute_pipeline_manager' "Compute Pipeline Manager"
check_file 'vulkan_graphics_pipeline_manager' "Graphics Pipeline Manager"
check_file 'vulkan_backend_tests' "Vulkan Backend Tests"
check_file 'vulkan_resource_manager_tests' "Resource Manager Tests"
check_file 'vulkan_compute_pipeline_manager_tests' "Compute Pipeline Tests"
check_file 'vulkan_graphics_pipeline_manager_tests' "Graphics Pipeline Tests"

echo
echo "===== CMAKE CONFIGURATION ====="

if cmake -S . -B build; then
    echo "[PASS] CMake Configuration"
    PASS=$((PASS+1))
else
    echo "[FAIL] CMake Configuration"
    FAIL=$((FAIL+1))
fi

echo
echo "===== BUILD ====="

if cmake --build build -j2; then
    echo "[PASS] Build"
    PASS=$((PASS+1))
else
    echo "[FAIL] Build"
    FAIL=$((FAIL+1))
fi

echo
echo "===== TEST SUITE ====="

if ctest --test-dir build --output-on-failure; then
    echo "[PASS] Test Suite"
    PASS=$((PASS+1))
else
    echo "[FAIL] Test Suite"
    FAIL=$((FAIL+1))
fi

echo
echo "======================================"
echo "             FINAL REPORT"
echo "======================================"
echo "PASS: $PASS"
echo "FAIL: $FAIL"

if [ "$FAIL" -eq 0 ]; then
    echo "RESULT: ALL CHECKS PASSED"
else
    echo "RESULT: ISSUES DETECTED"
fi
