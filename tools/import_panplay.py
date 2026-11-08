#!/usr/bin/env python3
"""Import PanPlay session/performance ZIPs into NYAMIST evidence reports.

The importer never extracts archive paths. It verifies the session manifest's
file sizes and SHA-256 hashes, bounds all ZIP reads, and deliberately leaves
translator rendering validation at discovery-only unless a separate exporter
supplies the explicit device/Present/failure counters required by NYAMIST.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import re
import sys
import zipfile
from pathlib import PurePosixPath, Path
from typing import Any

MAX_ARCHIVE_BYTES = 64 * 1024 * 1024
MAX_TOTAL_UNCOMPRESSED = 128 * 1024 * 1024
MAX_MEMBER_BYTES = 32 * 1024 * 1024
MAX_JSON_BYTES = 2 * 1024 * 1024
MAX_MEMBERS = 4096
SHA256_RE = re.compile(r"^[0-9a-fA-F]{64}$")
D3D11_LOG_RE = re.compile(r"(?:^|[_-])d3d11(?:[_\-.]|$)", re.IGNORECASE)


class ImportErrorDetail(ValueError):
    """Raised when a PanPlay archive cannot be normalized safely."""


def _safe_archive(archive_path: Path) -> zipfile.ZipFile:
    if not archive_path.is_file():
        raise ImportErrorDetail(f"archive not found: {archive_path}")
    if archive_path.stat().st_size > MAX_ARCHIVE_BYTES:
        raise ImportErrorDetail("PanPlay ZIP exceeds the 64 MiB archive limit")
    try:
        archive = zipfile.ZipFile(archive_path)
        infos = archive.infolist()
        if len(infos) > MAX_MEMBERS:
            archive.close()
            raise ImportErrorDetail("PanPlay ZIP contains too many members")
        total_size = 0
        seen: set[str] = set()
        for info in infos:
            name = info.filename
            if not name or "\x00" in name or "\\" in name:
                archive.close()
                raise ImportErrorDetail("ZIP contains an invalid member path")
            normalized_name = name[:-1] if info.is_dir() and name.endswith("/") else name
            path = PurePosixPath(normalized_name)
            if (path.is_absolute() or path.as_posix() != normalized_name or
                    any(part in ("", ".", "..") for part in path.parts)):
                archive.close()
                raise ImportErrorDetail(f"ZIP contains an unsafe member path: {name}")
            if name in seen:
                archive.close()
                raise ImportErrorDetail(f"ZIP contains a duplicate member: {name}")
            seen.add(name)
            if info.flag_bits & 0x1:
                archive.close()
                raise ImportErrorDetail("encrypted ZIP members are not supported")
            if info.file_size < 0 or info.file_size > MAX_MEMBER_BYTES:
                archive.close()
                raise ImportErrorDetail(f"ZIP member exceeds the 32 MiB limit: {name}")
            total_size += info.file_size
            if total_size > MAX_TOTAL_UNCOMPRESSED:
                archive.close()
                raise ImportErrorDetail("PanPlay ZIP exceeds the 128 MiB expanded-size limit")
            # Reject symlink/device entries. Directories are allowed but never read.
            mode = (info.external_attr >> 16) & 0xFFFF
            file_type = mode & 0o170000
            if file_type not in (0, 0o100000, 0o040000):
                archive.close()
                raise ImportErrorDetail(f"ZIP contains a non-regular entry: {name}")
        return archive
    except zipfile.BadZipFile as exc:
        raise ImportErrorDetail(f"invalid ZIP archive: {exc}") from exc


def _read_member(archive: zipfile.ZipFile, name: str, *, json_member: bool = False) -> bytes:
    matches = [info for info in archive.infolist() if info.filename == name and not info.is_dir()]
    if len(matches) != 1:
        raise ImportErrorDetail(f"expected exactly one ZIP member: {name}")
    info = matches[0]
    limit = MAX_JSON_BYTES if json_member else MAX_MEMBER_BYTES
    if info.file_size > limit:
        raise ImportErrorDetail(f"ZIP member is too large: {name}")
    try:
        data = archive.read(info)
    except (OSError, RuntimeError, zipfile.BadZipFile) as exc:
        raise ImportErrorDetail(f"could not read ZIP member {name}: {exc}") from exc
    if len(data) != info.file_size:
        raise ImportErrorDetail(f"ZIP member length mismatch: {name}")
    return data


def _json_bytes(data: bytes, label: str) -> Any:
    try:
        return json.loads(data.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise ImportErrorDetail(f"invalid JSON in {label}: {exc}") from exc


def _object(value: Any, label: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise ImportErrorDetail(f"{label} must be a JSON object")
    return value


def _nonempty_string(obj: dict[str, Any], key: str, label: str | None = None) -> str:
    value = obj.get(key)
    if not isinstance(value, str) or not value.strip() or "\n" in value or "\r" in value:
        raise ImportErrorDetail(f"missing or invalid {label or key}")
    return value.strip()


def _integer(obj: dict[str, Any], key: str, *, minimum: int = 0, optional: bool = False) -> int | None:
    value = obj.get(key)
    if value is None and optional:
        return None
    if isinstance(value, bool) or not isinstance(value, int) or value < minimum:
        raise ImportErrorDetail(f"missing or invalid integer: {key}")
    return value


def _finite_number(obj: dict[str, Any], key: str, *, minimum: float = 0.0,
                   maximum: float = 1_000_000_000.0, optional: bool = True) -> float | None:
    value = obj.get(key)
    if value is None and optional:
        return None
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ImportErrorDetail(f"missing or invalid numeric field: {key}")
    number = float(value)
    if not math.isfinite(number) or number < minimum or number > maximum:
        raise ImportErrorDetail(f"numeric field outside the allowed range: {key}")
    return number


def _verify_session_manifest(archive: zipfile.ZipFile, manifest: dict[str, Any]) -> list[str]:
    records = manifest.get("files")
    if not isinstance(records, list) or not records:
        raise ImportErrorDetail("manifest.json must contain a non-empty files array")

    expected: dict[str, tuple[int, str]] = {}
    for record in records:
        item = _object(record, "manifest files entry")
        name = _nonempty_string(item, "path", "manifest file path")
        path = PurePosixPath(name)
        if (path.is_absolute() or "\\" in name or path.as_posix() != name or
                any(part in ("", ".", "..") for part in path.parts)):
            raise ImportErrorDetail(f"manifest contains an unsafe file path: {name}")
        size = _integer(item, "size")
        digest = _nonempty_string(item, "sha256")
        if not SHA256_RE.fullmatch(digest):
            raise ImportErrorDetail(f"invalid SHA-256 in manifest for {name}")
        if name in expected:
            raise ImportErrorDetail(f"manifest lists a duplicate file: {name}")
        expected[name] = (int(size), digest.lower())

    actual = {
        info.filename: info
        for info in archive.infolist()
        if not info.is_dir() and info.filename != "manifest.json"
    }
    if set(actual) != set(expected):
        missing = sorted(set(expected) - set(actual))
        unlisted = sorted(set(actual) - set(expected))
        raise ImportErrorDetail(
            f"manifest/archive file list mismatch (missing={missing[:3]}, unlisted={unlisted[:3]})"
        )

    for name, (expected_size, expected_hash) in expected.items():
        info = actual[name]
        if info.file_size != expected_size:
            raise ImportErrorDetail(f"manifest size mismatch for {name}")
        data = _read_member(archive, name)
        if hashlib.sha256(data).hexdigest() != expected_hash:
            raise ImportErrorDetail(f"manifest SHA-256 mismatch for {name}")
    return sorted(actual)


def _unique_basename(names: list[str], basename: str, *, required: bool) -> str | None:
    matches = [name for name in names if PurePosixPath(name).name == basename]
    if not matches and not required:
        return None
    if len(matches) != 1:
        expectation = "exactly one" if required else "at most one"
        raise ImportErrorDetail(f"expected {expectation} file named {basename}; found {len(matches)}")
    return matches[0]


def _d3d11_log_evidence(archive: zipfile.ZipFile, file_names: list[str]) -> dict[str, Any]:
    """Read conservative D3D11-device and swapchain-setup markers from one DXVK log.

    A selected feature-level line is evidence that DXVK completed D3D11 device
    creation. Swapchain properties prove setup/configuration only; they do not
    prove that vkQueuePresentKHR successfully presented a frame.
    """
    candidates = [
        name for name in file_names
        if D3D11_LOG_RE.search(PurePosixPath(name).name)
    ]
    empty = {
        "source_log": None,
        "device_created_confirmed": False,
        "feature_level": None,
        "swapchain_configured_confirmed": False,
        "swapchain": {},
    }
    if len(candidates) != 1:
        return empty

    name = candidates[0]
    raw = _read_member(archive, name)
    text = raw.decode("utf-8", errors="replace")
    feature_match = re.search(
        r"D3D11InternalCreateDevice:\s*Using feature level\s+"
        r"D3D_FEATURE_LEVEL_([0-9]+_[0-9]+)",
        text,
        re.IGNORECASE,
    )
    lines = text.splitlines()
    marker_index = next(
        (index for index, line in enumerate(lines)
         if "Presenter: Actual swapchain properties:" in line),
        None,
    )
    swapchain: dict[str, str] = {}
    if marker_index is not None:
        field_map = {
            "Format": "format",
            "Color space": "color_space",
            "Present mode": "present_mode",
            "Buffer size": "buffer_size",
            "Image count": "image_count",
        }
        for line in lines[marker_index + 1:marker_index + 12]:
            clean = re.sub(r"^\s*(?:info|warn|err):\s*", "", line, flags=re.IGNORECASE)
            match = re.match(
                r"^\s*(Format|Color space|Present mode|Buffer size|Image count):\s*(.*?)\s*$",
                clean,
            )
            if match:
                key = field_map[match.group(1)]
                value = match.group(2)
                if value:
                    swapchain[key] = value

    return {
        "source_log": name,
        "device_created_confirmed": feature_match is not None,
        "feature_level": feature_match.group(1) if feature_match else None,
        "swapchain_configured_confirmed": marker_index is not None,
        "swapchain": swapchain,
    }


def _load_session_archive(path: Path) -> tuple[dict[str, Any], dict[str, Any], list[str], dict[str, Any]]:
    archive = _safe_archive(path)
    try:
        manifest_names = [
            info.filename for info in archive.infolist()
            if info.filename == "manifest.json" and not info.is_dir()
        ]
        if len(manifest_names) != 1:
            raise ImportErrorDetail("session ZIP must contain one root manifest.json")
        manifest = _object(_json_bytes(_read_member(archive, "manifest.json", json_member=True),
                                       "manifest.json"), "manifest.json")
        names = _verify_session_manifest(archive, manifest)
        session_name = _unique_basename(names, "session.json", required=True)
        assert session_name is not None
        session = _object(_json_bytes(_read_member(archive, session_name, json_member=True),
                                      session_name), "session.json")
        facts_name = _unique_basename(names, "device-facts.json", required=False)
        facts: dict[str, Any] = {}
        if facts_name:
            facts = _object(_json_bytes(_read_member(archive, facts_name, json_member=True),
                                        facts_name), "device-facts.json")
        # Keep only bounded, structured DXVK log observations; raw log contents are
        # not copied into the normalized sidecar.
        dxvk_log_evidence = _d3d11_log_evidence(archive, names)
        inventories: dict[str, Any] = {}
        for item in manifest.get("files", []):
            if isinstance(item, dict):
                inventories[str(item.get("path", ""))] = {
                    "size": item.get("size"),
                    "sha256": item.get("sha256"),
                }
        return manifest, session, names, {
            "device_facts": facts,
            "file_inventory": inventories,
            "dxvk_log_evidence": dxvk_log_evidence,
        }
    finally:
        archive.close()


def _load_performance_archive(path: Path) -> dict[str, Any]:
    archive = _safe_archive(path)
    try:
        files = [info for info in archive.infolist() if not info.is_dir()]
        if not files:
            raise ImportErrorDetail("performance ZIP is empty")
        if any("/" in info.filename or info.filename.startswith(".") for info in files):
            raise ImportErrorDetail("performance ZIP must contain only root-level files")
        json_files = [info.filename for info in files if info.filename.endswith(".json")]
        if len(json_files) != 1:
            raise ImportErrorDetail("performance ZIP must contain exactly one summary JSON file")
        summary_name = json_files[0]
        summary = _object(_json_bytes(_read_member(archive, summary_name, json_member=True),
                                      summary_name), summary_name)
        session_id = _nonempty_string(summary, "id")
        if summary_name != f"{session_id}.json":
            raise ImportErrorDetail("performance ZIP filename does not match the summary id")
        allowed = {f"{session_id}.json", f"{session_id}.csv", f"{session_id}-sys.csv"}
        unknown = sorted(info.filename for info in files if info.filename not in allowed)
        if unknown:
            raise ImportErrorDetail(f"unexpected performance ZIP member: {unknown[0]}")
        return summary
    finally:
        archive.close()


def _detect_translator(file_names: list[str], session: dict[str, Any]) -> tuple[str, str, str] | None:
    # PanPlay stores per-API DXVK logs as dxvk-<name>_d3d11.log and VKD3D as vkd3d.log.
    d3d11_logs = [name for name in file_names if D3D11_LOG_RE.search(PurePosixPath(name).name)]
    vkd3d_logs = [
        name for name in file_names
        if PurePosixPath(name).name.lower() in {"vkd3d.log", "dxvk-vkd3d.log"}
    ]
    if bool(d3d11_logs) == bool(vkd3d_logs):
        return None
    manifest_info = session.get("manifestInfo")
    if not isinstance(manifest_info, dict):
        manifest_info = {}
    if d3d11_logs:
        dxvk = manifest_info.get("dxvk")
        if not isinstance(dxvk, dict):
            dxvk = {}
        version = dxvk.get("version")
        if not isinstance(version, str) or not version.strip():
            return ("dxvk", "d3d11", "unreported")
        return ("dxvk", "d3d11", version.strip())
    # The PanPlay session format logs the VKD3D file but does not record its version.
    return ("vkd3d-proton", "d3d12", "unreported")


def _report_env(
    translator: tuple[str, str, str],
    source_name: str,
    session: dict[str, Any],
    performance: dict[str, Any] | None,
    dxvk_log_evidence: dict[str, Any],
) -> str:
    translator_id, api, version = translator
    device_created = dxvk_log_evidence.get("device_created_confirmed") is True
    swapchain_configured = dxvk_log_evidence.get("swapchain_configured_confirmed") is True
    fields: list[tuple[str, str]] = [
        ("translator_id", translator_id),
        ("api", api),
        ("translator_version", version),
        ("device_created", "true" if device_created else "false"),
        ("swapchain_configured", "true" if swapchain_configured else "false"),
        ("evidence_source", f"PanPlay session ZIP import: {source_name}"),
        ("render_present_passed", "false"),
    ]
    if device_created and dxvk_log_evidence.get("feature_level"):
        fields.append(("d3d11_feature_level", str(dxvk_log_evidence["feature_level"])))
    if performance and performance.get("dxvkStats") is True:
        frames = _integer(performance, "frames", minimum=1)
        duration = _finite_number(performance, "durationSec", minimum=0.001,
                                  maximum=86_400.0, optional=False)
        fields.extend([
            ("frame_count", str(frames)),
            ("duration_seconds", str(int(duration))),
        ])
    exit_code = session.get("exit")
    if isinstance(exit_code, int) and not isinstance(exit_code, bool):
        fields.append(("process_exit_code", str(exit_code)))
    return (
        "# Generated by tools/import_panplay.py; imported observations only\n"
        + "".join(f"{key}={value}\n" for key, value in fields)
        + "# Swapchain setup is not proof of a successful Vulkan Present call.\n"
    )


def import_archives(
    session_archive_path: Path,
    performance_archive_path: Path | None = None,
) -> tuple[str | None, dict[str, Any]]:
    manifest, session, file_names, extra = _load_session_archive(session_archive_path)

    game = _nonempty_string(session, "game")
    start_ms = _integer(session, "startMs", minimum=1)
    duration_seconds = _integer(session, "durationSec")
    exit_code = session.get("exit")
    if exit_code is not None and (isinstance(exit_code, bool) or not isinstance(exit_code, int)):
        raise ImportErrorDetail("session exit must be an integer or null")
    device_lost = session.get("deviceLost")
    if not isinstance(device_lost, bool):
        raise ImportErrorDetail("session deviceLost must be a boolean")
    session_file_names = session.get("files")
    if not isinstance(session_file_names, list) or any(
        not isinstance(item, str) or not item or "\n" in item or "\r" in item
        for item in session_file_names
    ):
        raise ImportErrorDetail("session files must be an array of file names")

    performance = _load_performance_archive(performance_archive_path) if performance_archive_path else None
    if performance is not None:
        perf_id = _nonempty_string(performance, "id")
        session_perf_id = session.get("perfId")
        if not isinstance(session_perf_id, str) or session_perf_id != perf_id:
            raise ImportErrorDetail("performance ZIP id does not match session.json perfId")
        performance_start = _integer(performance, "startMs", minimum=1)
        performance_end = _integer(performance, "endMs", minimum=1)
        if performance_end < performance_start:
            raise ImportErrorDetail("performance endMs must not precede startMs")
        _required_dxvk_stats = performance.get("dxvkStats")
        if not isinstance(_required_dxvk_stats, bool):
            raise ImportErrorDetail("performance dxvkStats must be a boolean")
        if _required_dxvk_stats:
            _integer(performance, "frames", minimum=1)
            _finite_number(performance, "durationSec", minimum=0.001,
                           maximum=86_400.0, optional=False)
            for key in ("frametimeP50Ms", "frametimeP95Ms", "frametimeP99Ms"):
                _finite_number(performance, key, minimum=0.000001,
                               maximum=120_000.0, optional=False)
        else:
            # A performance record without DXVK frame samples is still useful for
            # system telemetry, but it cannot supply a DXVK p95 comparison metric.
            if performance.get("frames") not in (None, 0):
                raise ImportErrorDetail("frames must be zero/absent when dxvkStats is false")

    device = manifest.get("device")
    android = manifest.get("android")
    driver = manifest.get("driver")
    if not isinstance(device, dict):
        raise ImportErrorDetail("session manifest has no device object")
    if not isinstance(android, dict):
        android = {}
    if not isinstance(driver, dict):
        driver = {}

    translator = _detect_translator(file_names, session)
    source_label = re.sub(r"[\\r\\n]+", "_", session_archive_path.name)
    report = _report_env(
        translator, source_label, session, performance, extra["dxvk_log_evidence"]
    ) if translator else None

    performance_summary: dict[str, Any] | None = None
    if performance is not None:
        perf_keys = (
            "frames", "durationSec", "avgFps", "low1Fps", "low01Fps",
            "frametimeP50Ms", "frametimeP95Ms", "frametimeP99Ms", "stutters",
            "gpuLoadAvg", "gpuLoadMax", "gameCpuAvg", "gameCpuMax",
            "ramAvgMb", "ramPeakMb", "vidmemPeakMb", "memAvailMinMb",
            "battTempAvgC", "battTempMaxC", "powerAvgMw", "powerMaxMw",
            "cpuFreqAvgMhz", "cpuFreqMaxMhz", "gpuFreqAvgMhz", "gpuFreqMaxMhz",
        )
        performance_summary = {
            key: performance[key] for key in perf_keys if key in performance
        }
        p95_ms = performance.get("frametimeP95Ms")
        if performance.get("dxvkStats") is True and p95_ms is not None:
            performance_summary["p95_frame_time_microseconds"] = int(round(float(p95_ms) * 1000.0))
        performance_summary["dxvk_stats"] = performance["dxvkStats"]

    manifest_info = session.get("manifestInfo")
    if not isinstance(manifest_info, dict):
        manifest_info = {}
    session_dxvk = manifest_info.get("dxvk")
    session_driver = manifest_info.get("driver")
    sidecar = {
        "schema_version": 1,
        "source_format": "PanPlay.SessionLogs",
        "source_archive": session_archive_path.name,
        "session": {
            "game": game,
            "exe": session.get("exe"),
            "start_ms": start_ms,
            "duration_seconds": duration_seconds,
            "reason": session.get("reason"),
            "exit_code": exit_code,
            "user_stopped": session.get("userStopped"),
            "crash": session.get("crash"),
            "device_lost_log_scan": device_lost,
            "performance_id": session.get("perfId"),
        },
        "device": {
            "manufacturer": device.get("manufacturer"),
            "model": device.get("model"),
            "board": device.get("board"),
            "hardware": device.get("hardware"),
            "soc_manufacturer": device.get("socManufacturer"),
            "soc_model": device.get("socModel"),
            "android_release": android.get("release"),
            "android_sdk": android.get("sdk"),
            "kernel": android.get("kernel"),
            "gpu": manifest.get("gpu"),
            "device_facts": extra["device_facts"],
        },
        "driver": {
            "name": (session_driver or {}).get("name") if isinstance(session_driver, dict) else driver.get("name"),
            "id": (session_driver or {}).get("id") if isinstance(session_driver, dict) else driver.get("id"),
            "version": (session_driver or {}).get("version") if isinstance(session_driver, dict) else driver.get("version"),
            "build_id": (session_driver or {}).get("buildId") if isinstance(session_driver, dict) else driver.get("buildId"),
            "sha256": (session_driver or {}).get("soSha256") if isinstance(session_driver, dict) else driver.get("soSha256"),
        },
        "translator": {
            "id": translator[0] if translator else None,
            "api": translator[1] if translator else None,
            "version": translator[2] if translator else None,
            "device_created_confirmed": extra["dxvk_log_evidence"]["device_created_confirmed"],
            "device_creation_evidence": (
                "DXVK D3D11InternalCreateDevice selected feature-level log"
                if extra["dxvk_log_evidence"]["device_created_confirmed"] else None
            ),
            "feature_level": extra["dxvk_log_evidence"]["feature_level"],
            "swapchain_configured_confirmed": extra["dxvk_log_evidence"]["swapchain_configured_confirmed"],
            "swapchain": extra["dxvk_log_evidence"]["swapchain"],
            "render_present_passed": False,
            "present_calls": None,
            "present_failures": None,
            "device_lost_counter": None,
        },
        "performance": performance_summary,
        "logs": {
            "files": extra["file_inventory"],
            "session_file_names": session_file_names,
            "device_lost_is_log_scan_only": True,
        },
        "validation": {
            "level": "discovered" if translator else "unclassified",
            "render_present_evidence": "not exported as explicit counters",
            "automatic_application_allowed": False,
        },
        "notes": [
            "PanPlay's deviceLost field is a log scan, not an explicit zero-event counter.",
            "DXVK D3D11 feature-level log confirms device creation; actual swapchain properties confirm setup only.",
            "PerfRecorder DXVK frame statistics and swapchain setup do not prove successful Vulkan Present calls.",
            "Do not promote this import to smoke_tested or sustained_tested without successful Present evidence, zero failure/device-loss counters, and normal process-exit evidence.",
        ],
    }
    return report, sidecar


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Import a PanPlay SessionLogs ZIP and optional PerfRecorder ZIP into NYAMIST"
    )
    parser.add_argument("session_archive", type=Path, help="PanPlay SessionLogs ZIP export")
    parser.add_argument("-o", "--output", type=Path, required=True,
                        help="output normalized JSON evidence sidecar")
    parser.add_argument("--performance-zip", type=Path,
                        help="optional PanPlay PerfRecorder ZIP for frame/system metrics")
    parser.add_argument("--report-output", type=Path,
                        help="optional NYAMIST translator-adapter .env report")
    args = parser.parse_args(argv)

    try:
        report, sidecar = import_archives(args.session_archive, args.performance_zip)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(sidecar, indent=2, ensure_ascii=False) + "\n",
                               encoding="utf-8")
        if args.report_output:
            if report is None:
                raise ImportErrorDetail(
                    "session logs do not identify one supported D3D11 or D3D12 translator"
                )
            args.report_output.parent.mkdir(parents=True, exist_ok=True)
            args.report_output.write_text(report, encoding="utf-8")
    except (ImportErrorDetail, OSError, zipfile.LargeZipFile) as exc:
        print(f"PanPlay import failed: {exc}", file=sys.stderr)
        return 1

    print(f"Normalized evidence: {args.output}")
    if args.report_output:
        print(f"Translator adapter report (discovery-only): {args.report_output}")
    elif report is None:
        print("No unambiguous D3D11/D3D12 translator identity was found; no adapter report generated.")
    else:
        print("Translator identity found; rendering validation remains discovery-only.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
