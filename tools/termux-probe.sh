#!/data/data/com.termux/files/usr/bin/bash
# Read-only NYAMIST hardware/runtime probe for Termux on Android.
# Does not request root, change drivers, or write to /dev/mali0.
set -u

section() { printf '\n===== %s =====\n' "$1"; }
prop() {
  if command -v getprop >/dev/null 2>&1; then
    value="$(getprop "$1" 2>/dev/null || true)"
    [ -n "$value" ] && printf '%s=%s\n' "$1" "$value" || printf '%s=<empty>\n' "$1"
  else
    printf '%s=<getprop unavailable>\n' "$1"
  fi
}

section "NYAMIST Termux probe"
printf 'date=%s\n' "$(date -Iseconds 2>/dev/null || date)"
printf 'user=%s\n' "$(id 2>/dev/null || true)"
printf 'kernel=%s\n' "$(uname -a 2>/dev/null || true)"
printf 'termux_prefix=%s\n' "${PREFIX:-<unset>}"

section "Android properties"
prop ro.product.manufacturer
prop ro.product.brand
prop ro.product.model
prop ro.soc.model
prop ro.hardware
prop ro.board.platform
prop ro.build.version.release
prop ro.build.version.sdk
prop ro.build.fingerprint
prop ro.product.cpu.abi
prop ro.product.cpu.abilist

section "Mali kernel interface (read-only checks)"
if [ -e /dev/mali0 ]; then
  ls -l /dev/mali0 2>&1 || true
  [ -r /dev/mali0 ] && echo "mali0_readable=yes" || echo "mali0_readable=no"
  [ -w /dev/mali0 ] && echo "mali0_writable=yes" || echo "mali0_writable=no"
  echo "NOTE: file permission checks do not prove an ioctl/device-context can be opened."
else
  echo "/dev/mali0=not-visible-from-this-Termux-process"
fi

section "Vulkan tools"
if command -v vulkaninfo >/dev/null 2>&1; then
  echo "vulkaninfo_path=$(command -v vulkaninfo)"
  vulkaninfo --summary 2>&1 || echo "vulkaninfo_summary_failed"
else
  echo "vulkaninfo=not-installed"
  echo "If available in your Termux repository, install the Vulkan tools package and rerun."
fi

section "Android graphics service"
if command -v dumpsys >/dev/null 2>&1; then
  dumpsys SurfaceFlinger 2>/dev/null | grep -iE 'GLES:|Vulkan|GPU|Mali|Immortalis' | head -n 30 || true
else
  echo "dumpsys=unavailable"
fi

section "Vendor Vulkan libraries (visibility only)"
for d in /vendor/lib64/hw /vendor/lib/hw /system/lib64/hw /system/lib/hw; do
  if [ -d "$d" ]; then
    echo "[$d]"
    ls "$d" 2>/dev/null | grep -iE 'vulkan|mali' || true
  fi
done

section "Termux build tools"
for tool in git clang cmake ninja pkg-config python; do
  if command -v "$tool" >/dev/null 2>&1; then
    printf '%s=%s\n' "$tool" "$(command -v "$tool")"
  else
    printf '%s=missing\n' "$tool"
  fi
done

section "Probe interpretation"
echo "This report is observational only. It does not install, activate, or replace any graphics driver."
echo "A missing /dev/mali0 in Termux may be Android app-sandbox visibility, not proof that the device lacks a Mali GPU."
echo "The Vulkan device/driver reported by the actual game container is the final runtime source of truth."
