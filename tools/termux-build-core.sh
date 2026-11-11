#!/data/data/com.termux/files/usr/bin/bash
# Build and run NYAMIST's portable C++ core inside Termux.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
command -v cmake >/dev/null 2>&1 || { echo "Missing cmake. Install with: pkg install cmake"; exit 1; }
command -v clang++ >/dev/null 2>&1 || { echo "Missing clang++. Install with: pkg install clang"; exit 1; }
cmake -S "$ROOT" -B "$ROOT/build-termux" -DCMAKE_BUILD_TYPE=Release -DNYAMIST_BUILD_TESTS=ON -DNYAMIST_BUILD_BENCHMARKS=ON
cmake --build "$ROOT/build-termux" --parallel 4
ctest --test-dir "$ROOT/build-termux" --output-on-failure
"$ROOT/build-termux/nyamist-cli" advice
