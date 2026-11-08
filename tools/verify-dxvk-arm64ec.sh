#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "Usage: $0 <package-directory>" >&2
  exit 2
fi

ROOT="$1"
DXVK_VERSION_VALUE="$(printenv DXVK_VERSION_VALUE || printf '3.1.1')"
command -v llvm-readobj >/dev/null
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

for dll in d3d10core.dll d3d11.dll d3d8.dll d3d9.dll dxgi.dll; do
  arm64ec_dll="$ROOT/system32/$dll"
  x86_dll="$ROOT/syswow64/$dll"
  [[ -s "$arm64ec_dll" ]] || { echo "Missing ARM64EC DLL: $arm64ec_dll" >&2; exit 1; }
  [[ -s "$x86_dll" ]] || { echo "Missing x86 DLL: $x86_dll" >&2; exit 1; }

  llvm-readobj --sections --coff-load-config "$arm64ec_dll" > "$tmp/arm64ec.txt"
  grep -Fq '.hexpthk' "$tmp/arm64ec.txt" || {
    echo "ARM64EC thunk section missing in $dll" >&2; exit 1;
  }
  grep -Fq '.a64xrm' "$tmp/arm64ec.txt" || {
    echo "ARM64EC metadata section missing in $dll" >&2; exit 1;
  }
  grep -Eiq 'CHPEMetadataPointer:[[:space:]]+0x0*[1-9a-f][0-9a-f]*' "$tmp/arm64ec.txt" || {
    echo "Non-zero CHPEMetadataPointer missing in $dll" >&2; exit 1;
  }

  llvm-readobj --file-headers "$x86_dll" > "$tmp/x86.txt"
  grep -Fq 'IMAGE_FILE_MACHINE_I386' "$tmp/x86.txt" || {
    echo "32-bit x86 PE machine type missing in $dll" >&2; exit 1;
  }
  echo "PASS: ARM64EC system32/$dll + x86 syswow64/$dll"
done

python3 - "$ROOT/profile.json" "$DXVK_VERSION_VALUE" <<'PY'
import json
import sys
from pathlib import Path

profile_path = Path(sys.argv[1])
version = sys.argv[2]
profile = json.loads(profile_path.read_text())
assert profile.get("type") == "DXVK", "package type must be DXVK"
assert profile.get("versionName") == version + "-arm64ec-baseline"
expected = set()
for directory in ("system32", "syswow64"):
    for dll in ("d3d10core.dll", "d3d11.dll", "d3d8.dll", "d3d9.dll", "dxgi.dll"):
        expected.add((directory + "/" + dll, chr(92) + chr(36) + "{" + directory + "}/" + dll))
actual = {(item["source"], item["target"]) for item in profile.get("files", [])}
assert actual == expected, "profile.json DLL mappings do not match the package layout"
print("PASS: profile.json package type, version, and DLL mappings")
PY

echo "DXVK ARM64EC package verification passed."
