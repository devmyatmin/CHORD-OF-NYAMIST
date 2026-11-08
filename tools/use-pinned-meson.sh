#!/usr/bin/env bash
# Source this helper after ROOT is set. Keeps Mesa and DXVK on one exact Meson pin.
set -Eeuo pipefail

[[ -n "${ROOT:-}" ]] || { echo "ROOT must be set before sourcing use-pinned-meson.sh" >&2; return 2; }
command -v python3 >/dev/null 2>&1 || { echo "Missing prerequisite: python3" >&2; return 2; }

MESON_VERSION_PIN="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["buildTools"]["mesonVersion"])' "$ROOT/configs/component-locks.json")"
PACKAGING_VERSION_PIN="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["buildTools"]["packagingVersion"])' "$ROOT/configs/component-locks.json")"
MESON_VENV="${NYA_MESON_VENV:-$ROOT/.nya-build/tooling/meson-$MESON_VERSION_PIN}"
MESON_BIN="$MESON_VENV/bin/meson"

if [[ ! -x "$MESON_BIN" ]] || [[ "$("$MESON_BIN" --version 2>/dev/null || true)" != "$MESON_VERSION_PIN" ]]; then
  mkdir -p "$(dirname "$MESON_VENV")"
  python3 -m venv --clear "$MESON_VENV"
  "$MESON_VENV/bin/python" -m pip install --disable-pip-version-check --no-deps \
    "meson==$MESON_VERSION_PIN" "packaging==$PACKAGING_VERSION_PIN"
fi

PACKAGING_ACTUAL="$("$MESON_VENV/bin/python" -c 'import packaging; print(packaging.__version__)' 2>/dev/null || true)"
if [[ "$PACKAGING_ACTUAL" != "$PACKAGING_VERSION_PIN" ]]; then
  "$MESON_VENV/bin/python" -m pip install --disable-pip-version-check --no-deps \
    "packaging==$PACKAGING_VERSION_PIN"
fi
"$MESON_VENV/bin/python" -c 'from packaging.version import Version; assert Version("1.4.2")'

export PATH="$MESON_VENV/bin:$PATH"
export NYA_MESON_VERSION="$MESON_VERSION_PIN"
MESON_ACTUAL="$(meson --version)"
[[ "$MESON_ACTUAL" == "$MESON_VERSION_PIN" ]] || {
  echo "Meson pin mismatch: expected $MESON_VERSION_PIN, found $MESON_ACTUAL" >&2
  return 1
}
echo "NyaBuild pinned Meson: $MESON_ACTUAL"
