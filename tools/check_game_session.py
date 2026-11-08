#!/usr/bin/env python3
"""Assess a normalized PanPlay session without promoting render validation.

This checker is intentionally conservative. It can identify whether a game
session has enough duration, process-exit, translator, and DXVK performance
evidence for human review. PanPlay's current exports do not provide explicit
Vulkan Present success/failure counters, so this tool never marks rendering
or a compatibility profile as smoke-tested or production-ready.
"""
from __future__ import annotations

import argparse
import json
import math
import sys
from pathlib import Path
from typing import Any

SUPPORTED_TRANSLATORS = {
    ("dxvk", "d3d11"),
    ("vkd3d-proton", "d3d12"),
}


class EvidenceError(ValueError):
    """Raised when a normalized evidence sidecar is malformed."""


def _object(value: Any, label: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise EvidenceError(f"{label} must be a JSON object")
    return value


def _integer(value: Any, label: str, *, minimum: int = 0) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value < minimum:
        raise EvidenceError(f"{label} must be an integer >= {minimum}")
    return value


def _number(value: Any, label: str, *, minimum: float = 0.0) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise EvidenceError(f"{label} must be a finite number >= {minimum}")
    number = float(value)
    if not math.isfinite(number) or number < minimum:
        raise EvidenceError(f"{label} must be a finite number >= {minimum}")
    return number


def _check(checks: list[dict[str, str]], key: str, state: str, detail: str) -> None:
    checks.append({"key": key, "state": state, "detail": detail})


def assess_evidence(evidence: Any, minimum_duration_seconds: int = 300) -> dict[str, Any]:
    """Return a human-review assessment; never a rendering-validation verdict."""
    if not 20 <= minimum_duration_seconds <= 86_400:
        raise EvidenceError("minimum duration must be between 20 and 86400 seconds")

    root = _object(evidence, "evidence")
    if root.get("schema_version") != 1:
        raise EvidenceError("unsupported evidence schema_version; expected 1")

    session = _object(root.get("session"), "session")
    translator = _object(root.get("translator"), "translator")
    performance_value = root.get("performance")
    performance = _object(performance_value, "performance") if performance_value is not None else {}

    checks: list[dict[str, str]] = []
    blockers: list[str] = []
    game = session.get("game") if isinstance(session.get("game"), str) else "unknown game"
    translator_id = translator.get("id")
    api = translator.get("api")
    translator_key = (translator_id, api)
    if translator_key in SUPPORTED_TRANSLATORS:
        version = translator.get("version") or "version not reported"
        _check(checks, "translator_identity", "pass",
               f"{translator_id} / {api}; {version}")
    else:
        detail = "The session does not identify exactly one supported D3D11/D3D12 translator."
        _check(checks, "translator_identity", "incomplete", detail)
        blockers.append(detail)

    if translator_key == ("dxvk", "d3d11"):
        if translator.get("device_created_confirmed") is True:
            feature_level = translator.get("feature_level")
            detail = "DXVK D3D11 device creation is recorded"
            if isinstance(feature_level, str) and feature_level:
                detail += f" at selected feature level {feature_level.replace('_', '.')}"
            maximum_feature_level = translator.get("maximum_feature_level")
            if isinstance(maximum_feature_level, str) and maximum_feature_level:
                detail += f" (maximum supported {maximum_feature_level.replace('_', '.')})"
            _check(checks, "d3d11_device_creation", "pass", detail + ".")
        else:
            detail = "DXVK log does not confirm successful D3D11 device creation."
            _check(checks, "d3d11_device_creation", "incomplete", detail)
            blockers.append(detail)

        if translator.get("swapchain_configured_confirmed") is True:
            swapchain = translator.get("swapchain")
            buffer_size = swapchain.get("buffer_size") if isinstance(swapchain, dict) else None
            detail = "DXVK actual swapchain properties were recorded"
            if isinstance(buffer_size, str) and buffer_size:
                detail += f" ({buffer_size})"
            _check(checks, "swapchain_setup", "pass", detail + "; this does not prove successful Present.")
        else:
            detail = "DXVK log does not confirm actual swapchain properties."
            _check(checks, "swapchain_setup", "incomplete", detail)
            blockers.append(detail)

    exit_code = session.get("exit_code")
    crash = session.get("crash")
    if isinstance(exit_code, int) and not isinstance(exit_code, bool) and exit_code == 0 and crash is False:
        _check(checks, "process_exit", "pass", "Process exit code is 0 and crash flag is false.")
    elif exit_code is None or not isinstance(crash, bool):
        detail = "Clean process-exit evidence is missing."
        _check(checks, "process_exit", "incomplete", detail)
        blockers.append(detail)
    else:
        detail = f"Process did not complete cleanly (exit_code={exit_code}, crash={crash})."
        _check(checks, "process_exit", "blocked", detail)
        blockers.append(detail)

    session_duration = _integer(session.get("duration_seconds"), "session.duration_seconds")
    perf_duration_value = performance.get("durationSec")
    perf_duration = None
    if perf_duration_value is not None:
        perf_duration = _number(perf_duration_value, "performance.durationSec", minimum=0.001)

    duration_ok = session_duration >= minimum_duration_seconds and (
        perf_duration is not None and perf_duration >= minimum_duration_seconds
    )
    if duration_ok:
        _check(checks, "sustained_duration", "pass",
               f"Session {session_duration}s; performance capture {perf_duration:g}s; "
               f"minimum {minimum_duration_seconds}s.")
    else:
        capture_text = "not captured" if perf_duration is None else f"{perf_duration:g}s"
        detail = (f"Need both session and performance capture to reach "
                  f"{minimum_duration_seconds}s (session={session_duration}s, "
                  f"performance={capture_text}).")
        _check(checks, "sustained_duration", "incomplete", detail)
        blockers.append(detail)

    dxvk_stats = performance.get("dxvk_stats")
    frames_value = performance.get("frames")
    frames_ok = (
        dxvk_stats is True and isinstance(frames_value, int) and
        not isinstance(frames_value, bool) and frames_value > 0
    )
    if frames_ok:
        _check(checks, "frame_samples", "pass",
               f"PerfRecorder reports {frames_value} frames with DXVK statistics enabled.")
    else:
        detail = ("DXVK frame samples are unavailable. Check that the matching "
                  "PerfRecorder export was supplied and DXVK statistics were enabled.")
        _check(checks, "frame_samples", "incomplete", detail)
        blockers.append(detail)

    p95_value = performance.get("p95_frame_time_microseconds")
    p95_ok = (
        isinstance(p95_value, int) and not isinstance(p95_value, bool) and p95_value > 0
    )
    if p95_ok:
        _check(checks, "p95_frame_time", "pass",
               f"p95 frame time: {p95_value / 1000.0:.3f} ms.")
    else:
        detail = "A valid DXVK p95 frame-time sample is unavailable."
        _check(checks, "p95_frame_time", "incomplete", detail)
        blockers.append(detail)

    device_lost_scan = session.get("device_lost_log_scan")
    if device_lost_scan is True:
        detail = "PanPlay's log scan found a device-loss marker; inspect the original logs."
        _check(checks, "device_lost_log_scan", "blocked", detail)
        blockers.append(detail)
    elif device_lost_scan is False:
        _check(checks, "device_lost_log_scan", "warning",
               "No device-loss marker was found by PanPlay's log scan; this is not an explicit counter.")
    else:
        detail = "Device-loss log-scan result was not supplied."
        _check(checks, "device_lost_log_scan", "incomplete", detail)
        blockers.append(detail)

    _check(checks, "vulkan_present", "not_measured",
           "PanPlay does not export explicit successful Present calls and zero Present-failure counters. "
           "Rendering validation therefore remains discovered-only.")

    observed_metrics = [
        key for key in (
            "gpuLoadAvg", "gpuLoadMax", "gameCpuAvg", "ramPeakMb",
            "vidmemPeakMb", "memAvailMinMb", "battTempMaxC",
            "gpuFreqAvgMhz", "stutters",
        ) if key in performance
    ]
    if observed_metrics:
        _check(checks, "device_performance_metrics", "pass",
               "Captured: " + ", ".join(observed_metrics))
    else:
        _check(checks, "device_performance_metrics", "warning",
               "No GPU/CPU/memory/thermal summary fields were included in PerfRecorder.")

    if any(item["state"] == "blocked" for item in checks):
        status = "BLOCKED"
    elif blockers:
        status = "INCOMPLETE"
    else:
        status = "REVIEW_READY"

    performance_summary_keys = (
        "frames", "avgFps", "low1Fps", "low01Fps",
        "frametimeP50Ms", "frametimeP95Ms", "frametimeP99Ms", "stutters",
        "gpuLoadAvg", "gpuLoadMax", "gameCpuAvg", "gameCpuMax",
        "ramPeakMb", "vidmemPeakMb", "memAvailMinMb",
        "battTempAvgC", "battTempMaxC", "powerAvgMw", "powerMaxMw",
    )
    performance_summary = {
        key: performance[key]
        for key in performance_summary_keys
        if key in performance
        and isinstance(performance[key], (int, float))
        and not isinstance(performance[key], bool)
        and math.isfinite(float(performance[key]))
    }

    return {
        "schema_version": 1,
        "assessment": status,
        "game": game,
        "translator_id": translator_id,
        "graphics_api": api,
        "minimum_duration_seconds": minimum_duration_seconds,
        "session_duration_seconds": session_duration,
        "performance_duration_seconds": perf_duration,
        "performance_summary": performance_summary,
        "p95_frame_time_microseconds": p95_value if p95_ok else None,
        "checks": checks,
        "blockers": blockers,
        "render_validation": "discovered_only",
        "automatic_application_allowed": False,
        "production_ready": False,
        "notes": [
            "REVIEW_READY means the collected session is suitable for human performance review; it is not a smoke-test or production-readiness verdict.",
            "Explicit Vulkan Present success/failure counters are not present in current PanPlay exports.",
            "A/B regression evaluation still requires at least three sustained runs in each arm with matching workload/device identity and distinct compatibility profile keys.",
        ],
    }


def _positive_duration(value: str) -> int:
    try:
        parsed = int(value, 10)
    except ValueError as exc:
        raise argparse.ArgumentTypeError("duration must be an integer") from exc
    if not 20 <= parsed <= 86_400:
        raise argparse.ArgumentTypeError("duration must be between 20 and 86400 seconds")
    return parsed


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Check normalized PanPlay evidence for a sustained game-session review."
    )
    parser.add_argument("evidence", type=Path, help="JSON sidecar from import_panplay.py")
    parser.add_argument("--minimum-duration-seconds", type=_positive_duration, default=300)
    parser.add_argument("--output", type=Path, help="optional machine-readable assessment JSON")
    args = parser.parse_args(argv)

    try:
        loaded = json.loads(args.evidence.read_text(encoding="utf-8"))
        result = assess_evidence(loaded, args.minimum_duration_seconds)
        if args.output:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_text(json.dumps(result, indent=2, ensure_ascii=False) + "\n",
                                   encoding="utf-8")
    except (OSError, json.JSONDecodeError, EvidenceError) as exc:
        print(f"Game-session evidence check failed: {exc}", file=sys.stderr)
        return 1

    print(f"Game: {result['game']}")
    print(f"Translator: {result['translator_id'] or 'unclassified'} / "
          f"{result['graphics_api'] or 'unknown'}")
    print(f"Assessment: {result['assessment']}")
    for item in result["checks"]:
        print(f"[{item['state'].upper()}] {item['key']}: {item['detail']}")
    if result["performance_summary"]:
        summary = result["performance_summary"]
        print("Performance summary:")
        for key in (
            "avgFps", "low1Fps", "low01Fps",
            "frametimeP50Ms", "frametimeP95Ms", "frametimeP99Ms",
            "stutters", "gpuLoadAvg", "ramPeakMb", "vidmemPeakMb",
            "memAvailMinMb", "battTempMaxC",
        ):
            if key in summary:
                print(f"  {key}: {summary[key]}")
    print(f"Render validation: {result['render_validation']}")
    if args.output:
        print(f"Assessment JSON: {args.output}")
    print("No driver, translator, game, or upstream cache was modified.")
    return {"REVIEW_READY": 0, "INCOMPLETE": 2, "BLOCKED": 3}[result["assessment"]]


if __name__ == "__main__":
    raise SystemExit(main())
