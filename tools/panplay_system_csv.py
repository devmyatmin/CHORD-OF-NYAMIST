"""Parser and provisional cross-stream correlation for PanPlay system telemetry.

System CSV samples are retained only during import. The normalized evidence
contains bounded aggregates and selected stall-window summaries, never raw rows.
"""
from __future__ import annotations

import csv
import io
import math
from bisect import bisect_left, bisect_right
from statistics import median
from typing import Any

SYSTEM_HEADER = [
    "epoch_ms", "procs", "game_cpu_pct", "game_cpu_cores", "sys_cpu_pct",
    "game_rss_mb", "mem_avail_mb", "batt_temp_c", "batt_current_ma",
    "batt_power_mw", "cpu_freq_max_mhz", "gpu_freq_mhz", "gpu_util_pct",
    "cpu_freqs_mhz",
]
NUMERIC_FIELDS = [
    "procs", "game_cpu_pct", "game_cpu_cores", "sys_cpu_pct",
    "game_rss_mb", "mem_avail_mb", "batt_temp_c", "batt_current_ma",
    "batt_power_mw", "cpu_freq_max_mhz", "gpu_freq_mhz", "gpu_util_pct",
]
CORRELATION_FIELDS = [
    "game_cpu_pct", "game_cpu_cores", "game_rss_mb", "mem_avail_mb",
    "batt_temp_c", "batt_current_ma", "batt_power_mw",
    "cpu_freq_max_mhz", "gpu_freq_mhz", "gpu_util_pct",
]
MAX_RETAINED_STALLS = 10


class SystemCsvError(ValueError):
    """Raised when a PanPlay system telemetry CSV is malformed."""


def _finite_number(value: str, field: str) -> float | None:
    if value == "":
        return None
    try:
        result = float(value)
    except (TypeError, ValueError) as exc:
        raise SystemCsvError(f"invalid number in {field}") from exc
    if not math.isfinite(result):
        raise SystemCsvError(f"{field} must be finite")
    if field not in {"batt_current_ma"} and result < 0:
        raise SystemCsvError(f"{field} must be non-negative")
    if field == "gpu_util_pct" and result > 100:
        raise SystemCsvError("gpu_util_pct exceeds 100")
    return result


def _nearest_rank(values: list[float], percentile: float) -> float:
    ordered = sorted(values)
    index = max(0, math.ceil(percentile * len(ordered)) - 1)
    return ordered[index]


def _metric_summary(values: list[float]) -> dict[str, float | int]:
    return {
        "samples": len(values),
        "min": round(min(values), 3),
        "median": round(float(median(values)), 3),
        "p95": round(_nearest_rank(values, 0.95), 3),
        "max": round(max(values), 3),
    }


def parse_system_csv(data: bytes) -> tuple[dict[str, Any], list[dict[str, Any]]]:
    """Parse PanPlay's epoch_ms system CSV into aggregates and transient samples."""
    try:
        decoded = data.decode("utf-8-sig", errors="strict")
    except UnicodeDecodeError as exc:
        raise SystemCsvError("system CSV is not valid UTF-8") from exc

    reader = csv.DictReader(io.StringIO(decoded, newline=""))
    if reader.fieldnames != SYSTEM_HEADER:
        raise SystemCsvError("unexpected PanPlay system CSV header")

    samples: list[dict[str, Any]] = []
    timestamps: list[int] = []
    metric_values: dict[str, list[float]] = {field: [] for field in NUMERIC_FIELDS}

    for line_number, row in enumerate(reader, start=2):
        if None in row or any(row.get(field) is None for field in SYSTEM_HEADER):
            raise SystemCsvError(f"system record at line {line_number} has the wrong column count")
        raw_epoch = row["epoch_ms"]
        try:
            epoch_ms = int(raw_epoch, 10)
        except (TypeError, ValueError) as exc:
            raise SystemCsvError(f"invalid integer in epoch_ms at line {line_number}") from exc
        if epoch_ms < 0:
            raise SystemCsvError("epoch_ms must be non-negative")
        if timestamps and epoch_ms <= timestamps[-1]:
            raise SystemCsvError("system timestamps must be strictly increasing")

        sample: dict[str, Any] = {"epoch_ms": epoch_ms}
        for field in NUMERIC_FIELDS:
            value = _finite_number(row[field], field)
            sample[field] = value
            if value is not None:
                metric_values[field].append(value)
        samples.append(sample)
        timestamps.append(epoch_ms)

    if not samples:
        raise SystemCsvError("system CSV contains no samples")

    intervals = [
        (timestamps[index] - timestamps[index - 1]) / 1000.0
        for index in range(1, len(timestamps))
    ]
    summary: dict[str, Any] = {
        "format": "PanPlay.PerfRecorder.SystemCSV.v1",
        "records": len(samples),
        "timestamp_start_ms": timestamps[0],
        "timestamp_end_ms": timestamps[-1],
        "timestamp_span_seconds": round((timestamps[-1] - timestamps[0]) / 1000.0, 6),
        "sampling_intervals": {
            "count": len(intervals),
            "median_seconds": round(float(median(intervals)), 6) if intervals else None,
            "p95_seconds": round(_nearest_rank(intervals, 0.95), 6) if intervals else None,
            "max_seconds": round(max(intervals), 6) if intervals else None,
        },
        "metrics": {},
    }
    for field, values in metric_values.items():
        summary["metrics"][field] = (
            _metric_summary(values) if values else {"samples": 0}
        )
    return summary, samples


def correlate_frame_stalls(
    *,
    performance_start_ms: int,
    performance_end_ms: int,
    performance_duration_seconds: float | None,
    session_start_ms: int,
    session_duration_seconds: int,
    frame_stream: dict[str, Any] | None,
    system_summary: dict[str, Any],
    system_samples: list[dict[str, Any]],
) -> dict[str, Any]:
    """Attach provisional epoch-aligned system windows to the largest F stalls.

    PerfRecorder's public export does not explicitly document the relationship
    between epoch_ms and timestamp_us. The performance.startMs anchor is
    therefore reported as provisional, not authoritative, and no causality is
    inferred.
    """
    wall_span = (performance_end_ms - performance_start_ms) / 1000.0
    session_offset = (session_start_ms - performance_start_ms) / 1000.0
    session_end_offset = session_offset + session_duration_seconds

    timing: dict[str, Any] = {
        "alignment_basis": (
            "performance.startMs + [frame.timestamp_us - frametime_us, "
            "frame.timestamp_us] (interval-end timestamp semantics)"
        ),
        "alignment_status": "provisional",
        "causality_inferred": False,
        "performance_wall_span_seconds": round(wall_span, 6),
        "performance_reported_duration_seconds": (
            round(performance_duration_seconds, 6)
            if performance_duration_seconds is not None else None
        ),
        "performance_wall_minus_reported_duration_seconds": (
            round(wall_span - performance_duration_seconds, 6)
            if performance_duration_seconds is not None else None
        ),
        "session_start_offset_from_performance_start_seconds": round(session_offset, 6),
        "session_end_offset_from_performance_start_seconds": round(session_end_offset, 6),
        "system_first_offset_from_performance_start_seconds": round(
            (system_summary["timestamp_start_ms"] - performance_start_ms) / 1000.0, 6
        ),
        "system_last_offset_from_performance_start_seconds": round(
            (system_summary["timestamp_end_ms"] - performance_start_ms) / 1000.0, 6
        ),
        "frame_stream_covered_by_system": None,
        "frame_timestamp_start_offset_seconds": None,
        "frame_timestamp_end_offset_seconds": None,
        "frame_interval_start_offset_seconds": None,
        "frame_interval_end_offset_seconds": None,
        "frame_timestamp_semantics": frame_stream.get("frame_timestamp_semantics"),
        "frame_timestamp_span_seconds": None,
        "frame_time_sum_seconds": None,
        "largest_frame_stalls": [],
        "note": (
            "Epoch alignment uses performance.startMs as a provisional origin "
            "for relative frame timestamps. The export does not explicitly "
            "specify this clock mapping; treat window statistics as temporal "
            "correlation only, not causation."
        ),
    }

    if not isinstance(frame_stream, dict):
        timing["note"] += " Frame CSV stream is unavailable."
        return timing

    first_us = frame_stream.get("frame_timestamp_start_us")
    last_us = frame_stream.get("frame_timestamp_end_us")
    timing["frame_timestamp_span_seconds"] = frame_stream.get(
        "frame_timestamp_span_seconds"
    )
    timing["frame_time_sum_seconds"] = frame_stream.get("frametime_sum_seconds")
    if first_us is not None and last_us is not None:
        first_offset = first_us / 1_000_000.0
        last_offset = last_us / 1_000_000.0
        interval_start_us = frame_stream.get(
            "frame_interval_start_timestamp_us", first_us
        )
        interval_start_offset = interval_start_us / 1_000_000.0
        timing["frame_timestamp_start_offset_seconds"] = round(first_offset, 6)
        timing["frame_timestamp_end_offset_seconds"] = round(last_offset, 6)
        timing["frame_interval_start_offset_seconds"] = round(
            interval_start_offset, 6
        )
        timing["frame_interval_end_offset_seconds"] = round(last_offset, 6)
        first_epoch = performance_start_ms + interval_start_us / 1000.0
        last_epoch = performance_start_ms + last_us / 1000.0
        timing["frame_stream_covered_by_system"] = (
            system_summary["timestamp_start_ms"] <= first_epoch
            and system_summary["timestamp_end_ms"] >= last_epoch
        )

    epochs = [sample["epoch_ms"] for sample in system_samples]
    stalls = frame_stream.get("largest_frame_stalls", [])
    windows: list[dict[str, Any]] = []
    for stall in stalls[:MAX_RETAINED_STALLS]:
        timestamp_us = int(stall["frame_timestamp_us"])
        frametime_ms = float(stall["frametime_ms"])
        interval_start_us = int(
            stall.get(
                "interval_start_timestamp_us",
                timestamp_us - round(frametime_ms * 1000.0),
            )
        )
        start_epoch = performance_start_ms + interval_start_us / 1000.0
        end_epoch = performance_start_ms + timestamp_us / 1000.0
        first_index = bisect_left(epochs, start_epoch)
        last_index = bisect_right(epochs, end_epoch)
        selected = system_samples[first_index:last_index]

        metrics: dict[str, Any] = {}
        for field in CORRELATION_FIELDS:
            values = [
                float(sample[field]) for sample in selected
                if sample.get(field) is not None
            ]
            metrics[field] = _metric_summary(values) if values else {"samples": 0}

        before_index = first_index - 1
        after_index = last_index
        # Epoch values are milliseconds; exported field names are seconds.
        before_delta = (
            round((start_epoch - epochs[before_index]) / 1000.0, 6)
            if before_index >= 0 else None
        )
        after_delta = (
            round((epochs[after_index] - end_epoch) / 1000.0, 6)
            if after_index < len(epochs) else None
        )
        windows.append({
            "frame_timestamp_us": timestamp_us,
            "frame_timestamp_semantics": "interval_end",
            "frame_relative_seconds": round(timestamp_us / 1_000_000.0, 6),
            "frame_interval_start_relative_seconds": round(
                interval_start_us / 1_000_000.0, 6
            ),
            "frame_interval_end_relative_seconds": round(
                timestamp_us / 1_000_000.0, 6
            ),
            "frametime_ms": round(frametime_ms, 3),
            "provisional_epoch_start_ms": round(start_epoch, 3),
            "provisional_epoch_end_ms": round(end_epoch, 3),
            "system_samples_in_frame_interval": len(selected),
            "nearest_prior_system_sample_delta_seconds": before_delta,
            "nearest_following_system_sample_delta_seconds": after_delta,
            "metrics": metrics,
        })

    timing["largest_frame_stalls"] = windows
    return timing
