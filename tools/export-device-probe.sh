#!/data/data/com.termux/files/usr/bin/bash
# Export schema-v1 Android device facts from Termux.
# Read-only: no root, driver loading, /dev/mali0 access, or system changes.
set -euo pipefail

fail() { printf 'ERROR: %s\n' "$*" >&2; exit 1; }
clean() { printf '%s' "$1" | tr '\r\n=' '   '; }
getprop_value() {
  local value
  value="$(getprop "$1" 2>/dev/null || true)"
  clean "$value"
}
required_prop() {
  local value
  value="$(getprop_value "$1")"
  [ -n "$value" ] || fail "Android property is unavailable: $1"
  printf '%s' "$value"
}

command -v getprop >/dev/null 2>&1 || fail "getprop is unavailable; run this script inside Termux on Android"
model="$(required_prop ro.product.model)"
api="$(required_prop ro.build.version.sdk)"
abi="$(required_prop ro.product.cpu.abi)"
manufacturer="$(getprop_value ro.product.manufacturer)"
brand="$(getprop_value ro.product.brand)"
soc="$(getprop_value ro.soc.model)"
hardware="$(getprop_value ro.hardware)"
board="$(getprop_value ro.board.platform)"
release="$(getprop_value ro.build.version.release)"
abi_list="$(getprop_value ro.product.cpu.abilist)"
kernel="$(uname -r 2>/dev/null || true)"
[ -n "$kernel" ] || fail "kernel release could not be read"
# Android/Termux may not provide getconf. Prefer it when available, then
# fall back to /proc/cpuinfo and nproc without requiring extra packages.
cpus=0
if command -v getconf >/dev/null 2>&1; then
  cpus="$(getconf _NPROCESSORS_ONLN 2>/dev/null || printf '0')"
fi
case "$cpus" in ''|*[!0-9]*) cpus=0 ;; esac
if [ "$cpus" -le 0 ] && [ -r /proc/cpuinfo ]; then
  cpus="$(awk '/^processor[[:space:]]*:/ { count++ } END { print count+0 }' /proc/cpuinfo)"
fi
case "$cpus" in ''|*[!0-9]*) cpus=0 ;; esac
if [ "$cpus" -le 0 ] && command -v nproc >/dev/null 2>&1; then
  cpus="$(nproc 2>/dev/null || printf '0')"
fi
case "$cpus" in ''|*[!0-9]*) cpus=0 ;; esac
[ "$cpus" -gt 0 ] || fail "logical CPU count could not be determined (getconf, /proc/cpuinfo, and nproc unavailable)"

total_kb=0
available_kb=0
if [ -r /proc/meminfo ]; then
  total_kb="$(awk '/^MemTotal:/ {print $2; exit}' /proc/meminfo)"
  available_kb="$(awk '/^MemAvailable:/ {print $2; exit}' /proc/meminfo)"
fi
case "$total_kb:$available_kb" in
  *[!0-9:]*|:*) total_kb=0; available_kb=0 ;;
esac
total_bytes=$((total_kb * 1024))
available_bytes=$((available_kb * 1024))
if [ "$total_bytes" -le 0 ] || [ "$available_bytes" -le 0 ] || [ "$available_bytes" -gt "$total_bytes" ]; then
  total_bytes=0
  available_bytes=0
fi

output="${1:-$PWD/nyamist-device-probe-$(date +%Y%m%d-%H%M%S).env}"
umask 077
{
  printf '# Read-only Termux observation. Vulkan is intentionally not inferred.\n'
  printf 'schema_version=1\n'
  printf 'captured_at=%s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
  printf 'evidence_source=NYAMIST Termux device facts exporter\n'
  printf 'device.manufacturer=%s\n' "$manufacturer"
  printf 'device.brand=%s\n' "$brand"
  printf 'device.model=%s\n' "$model"
  printf 'device.soc_model=%s\n' "$soc"
  printf 'device.hardware=%s\n' "$hardware"
  printf 'device.board_platform=%s\n' "$board"
  printf 'device.android_release=%s\n' "$release"
  printf 'device.android_api=%s\n' "$api"
  printf 'device.kernel_release=%s\n' "$kernel"
  printf 'device.primary_abi=%s\n' "$abi"
  printf 'device.abi_list=%s\n' "$abi_list"
  printf 'device.logical_cpu_count=%s\n' "$cpus"
  if [ "$total_bytes" -gt 0 ]; then
    printf 'device.total_memory_bytes=%s\n' "$total_bytes"
    printf 'device.available_memory_bytes=%s\n' "$available_bytes"
  fi
  printf 'vulkan.enumeration_status=unavailable\n'
  printf 'vulkan.device_count=0\n'
  printf 'vulkan.failure_reason=Termux exporter does not query the target app Vulkan context\n'
} > "$output"
printf 'NYAMIST device report written: %s\n' "$output"
printf 'Vulkan status: unavailable (not queried; no capability inferred)\n'
printf 'Next: import with nyamist-cli inspect-device "%s"\n' "$output"
