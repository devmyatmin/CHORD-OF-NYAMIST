#!/usr/bin/env python3
"""Convert a PanProbe ZIP export into NYAMIST's normalized PanVK report.

The importer reads selected JSON members directly from the ZIP; it never extracts
archive paths to disk and never treats feature-test results as render evidence.
"""
from __future__ import annotations

import argparse
import json
import re
import sys
import zipfile
from pathlib import Path
from typing import Any

MAX_ARCHIVE_BYTES = 64 * 1024 * 1024
MAX_MEMBER_BYTES = 2 * 1024 * 1024
SHA256_RE = re.compile(r"^[0-9a-fA-F]{64}$")


class ImportErrorDetail(ValueError):
    """Raised when an archive cannot be normalized safely."""


def _read_json_member(archive: zipfile.ZipFile, name: str) -> Any:
    matches = [info for info in archive.infolist() if info.filename == name]
    if len(matches) != 1:
        raise ImportErrorDetail(f"expected exactly one ZIP member: {name}")
    info = matches[0]
    if info.file_size > MAX_MEMBER_BYTES:
        raise ImportErrorDetail(f"ZIP member is too large: {name}")
    try:
        return json.loads(archive.read(info).decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError, OSError) as exc:
        raise ImportErrorDetail(f"invalid JSON member {name}: {exc}") from exc


def _unique_basename(archive: zipfile.ZipFile, basename: str, required: bool) -> str | None:
    matches = [
        info.filename for info in archive.infolist()
        if Path(info.filename).name == basename and not info.is_dir()
    ]
    if not matches and not required:
        return None
    if len(matches) != 1:
        raise ImportErrorDetail(
            f"expected {'one' if required else 'at most one'} ZIP member named {basename}; "
            f"found {len(matches)}"
        )
    return matches[0]


def _required_string(obj: dict[str, Any], key: str, label: str | None = None) -> str:
    value = obj.get(key)
    if isinstance(value, (dict, list)) or value is None:
        raise ImportErrorDetail(f"missing or invalid {label or key}")
    text = str(value).strip()
    if not text or "\n" in text or "\r" in text:
        raise ImportErrorDetail(f"missing or invalid {label or key}")
    return text


def _required_bool(obj: dict[str, Any], key: str) -> bool:
    value = obj.get(key)
    if not isinstance(value, bool):
        raise ImportErrorDetail(f"missing or invalid boolean: {key}")
    return value


def _required_count(obj: dict[str, Any], key: str) -> int:
    value = obj.get(key)
    if isinstance(value, bool) or not isinstance(value, int) or value < 0:
        raise ImportErrorDetail(f"missing or invalid non-negative integer: {key}")
    return value


def _feature_records(value: Any) -> list[dict[str, Any]]:
    """Find feature records without assuming a particular PanProbe wrapper shape."""
    found: list[dict[str, Any]] = []

    def visit(node: Any) -> None:
        if isinstance(node, dict):
            name = node.get("name")
            status = node.get("status")
            if isinstance(name, str) and isinstance(status, str):
                record: dict[str, Any] = {
                    "name": name,
                    "status": status,
                }
                for key in ("category", "note", "tested_by"):
                    if key in node:
                        record[key] = node[key]
                found.append(record)
            for child in node.values():
                visit(child)
        elif isinstance(node, list):
            for child in node:
                visit(child)

    visit(value)
    # Preserve order while avoiding repeated records in nested summary wrappers.
    unique: list[dict[str, Any]] = []
    seen: set[tuple[str, str]] = set()
    for record in found:
        identity = (record["name"], record["status"])
        if identity not in seen:
            seen.add(identity)
            unique.append(record)
    return unique


def import_archive(archive_path: Path) -> tuple[str, dict[str, Any]]:
    if not archive_path.is_file():
        raise ImportErrorDetail(f"archive not found: {archive_path}")
    if archive_path.stat().st_size > MAX_ARCHIVE_BYTES:
        raise ImportErrorDetail("PanProbe ZIP exceeds the 64 MiB archive limit")

    try:
        with zipfile.ZipFile(archive_path) as archive:
            bad_member = archive.testzip()
            if bad_member is not None:
                raise ImportErrorDetail(f"ZIP CRC check failed: {bad_member}")

            load = _read_json_member(archive, "driver-load.json")
            summary_name = _unique_basename(archive, "summary.json", required=True)
            assert summary_name is not None
            summary = _read_json_member(archive, summary_name)
            feature_name = _unique_basename(archive, "d3d_fl.json", required=False)
            feature_json = (
                _read_json_member(archive, feature_name) if feature_name else None
            )
    except zipfile.BadZipFile as exc:
        raise ImportErrorDetail(f"invalid ZIP archive: {exc}") from exc

    if not isinstance(load, dict) or not isinstance(summary, dict):
        raise ImportErrorDetail("driver-load.json and summary.json must contain JSON objects")

    driver_name = _required_string(load, "driverName")
    if "panvk" not in driver_name.lower():
        raise ImportErrorDetail("driver-load.json does not identify a PanVK driver")

    driver_info_value = summary.get("driverInfo") or load.get("driverInfo")
    if driver_info_value is None:
        driver_name_summary = summary.get("driverName", driver_name)
        driver_version = summary.get("driverVersion", load.get("driverVersion"))
        driver_info_value = (
            f"{driver_name_summary} {driver_version}"
            if driver_version is not None else driver_name_summary
        )
    driver_info = str(driver_info_value).strip()
    if not driver_info or "\n" in driver_info or "\r" in driver_info:
        raise ImportErrorDetail("missing or invalid driverInfo")

    api_version = _required_string(summary, "apiVersion")
    gpu_id = _required_string(load, "gpuId")
    load_success = _required_bool(load, "loadSuccess")
    device_count = _required_count(load, "deviceCount")
    passed = _required_count(summary, "pass")
    failed = _required_count(summary, "fail")
    skipped = _required_count(summary, "skip")

    digest = load.get("soSha256")
    if digest is not None and digest != "" and (
        not isinstance(digest, str) or not SHA256_RE.fullmatch(digest)
    ):
        raise ImportErrorDetail("soSha256 must be a 64-character hexadecimal digest")

    fields = [
        ("driver_name", driver_name),
        ("driver_info", driver_info),
        ("api_version", api_version),
        ("gpu_id", gpu_id),
        ("load_success", str(load_success).lower()),
        ("device_count", str(device_count)),
        ("panprobe_pass", str(passed)),
        ("panprobe_fail", str(failed)),
        ("panprobe_skip", str(skipped)),
    ]
    optional_fields = [
        ("build_id", load.get("buildId")),
        ("sha256", digest),
        ("evidence_source", f"PanProbe ZIP import: {archive_path.name}"),
    ]
    for key, value in optional_fields:
        if value is not None and value != "":
            text_value = str(value).strip()
            if "\n" in text_value or "\r" in text_value:
                raise ImportErrorDetail(f"invalid line break in {key}")
            fields.append((key, text_value))

    report = "# Generated by tools/import-panprobe.py; metadata only\n"
    report += "".join(f"{key}={value}\n" for key, value in fields)
    report += (
        "\n# PanProbe discovery/test counts are not render/present evidence.\n"
        "render_present_passed=false\n"
        "render_present_frames=0\n"
        "test_duration_seconds=0\n"
        "process_exit_code=-1\n"
        "device_lost_events=0\n"
        "sustained_test_passed=false\n"
    )

    sidecar = {
        "schema_version": 1,
        "source_archive": archive_path.name,
        "driver_name": driver_name,
        "driver_info": driver_info,
        "api_version": api_version,
        "gpu_id": gpu_id,
        "load_success": load_success,
        "device_count": device_count,
        "panprobe": {"pass": passed, "fail": failed, "skip": skipped},
        "d3d_feature_levels": _feature_records(feature_json),
        "note": "Feature availability is not proof of game rendering or sustained stability.",
    }
    return report, sidecar


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Import PanProbe ZIP JSON into a NYAMIST PanVK report"
    )
    parser.add_argument("archive", type=Path, help="PanProbe ZIP export")
    parser.add_argument("-o", "--output", type=Path, required=True, help="output .env path")
    args = parser.parse_args(argv)

    try:
        report, sidecar = import_archive(args.archive)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(report, encoding="utf-8")
        sidecar_path = Path(str(args.output) + ".capabilities.json")
        sidecar_path.write_text(
            json.dumps(sidecar, indent=2, ensure_ascii=False) + "\n",
            encoding="utf-8",
        )
    except (ImportErrorDetail, OSError, zipfile.LargeZipFile) as exc:
        print(f"PanProbe import failed: {exc}", file=sys.stderr)
        return 1

    print(f"Normalized report: {args.output}")
    print(f"Capability sidecar: {sidecar_path}")
    print("Validation remains discovery-only; render/present evidence was not inferred.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
