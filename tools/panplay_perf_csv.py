"""Parser for PanPlay PerfRecorder's typed frame/statistics CSV stream.

The export begins with separate frame and statistics schemas, followed by
interleaved F and S records. This parser retains aggregate evidence only.
"""
from __future__ import annotations

import csv
import io
import math
from bisect import bisect_right
from typing import Any

FRAME_HEADER = ["# F", "timestamp_us", "frametime_us"]
STATS_HEADER = [
    "# S", "timestamp_us", "gpu_load_pct", "vidmem_used_mb",
    "vidmem_alloc_mb", "sysmem_used_mb", "sysmem_alloc_mb",
    "draw_calls", "submissions", "renderpasses", "barriers",
    "graphics_pipelines", "compiler_busy",
]
MAX_RETAINED_STAT_GAPS = 10


class PerfCsvError(ValueError):
    """Raised when a PerfRecorder typed CSV stream is malformed."""


def _integer(value: str, field: str, *, minimum: int = 0) -> int:
    try:
        parsed = int(value, 10)
    except (TypeError, ValueError) as exc:
        raise PerfCsvError(f"invalid integer in {field}") from exc
    if parsed < minimum:
        raise PerfCsvError(f"{field} is below the allowed minimum")
    return parsed


def _number(value: str, field: str) -> float | None:
    if value == "":
        return None
    try:
        parsed = float(value)
    except (TypeError, ValueError) as exc:
        raise PerfCsvError(f"invalid number in {field}") from exc
    if not math.isfinite(parsed) or parsed < 0:
        raise PerfCsvError(f"{field} must be a finite non-negative number")
    return parsed


def _nearest_rank(values: list[int], percentile: float) -> float:
    ordered = sorted(values)
    index = max(0, math.ceil(percentile * len(ordered)) - 1)
    return ordered[index] / 1000.0


def _nearest_rank_seconds(values_us: list[int], percentile: float) -> float:
    ordered = sorted(values_us)
    index = max(0, math.ceil(percentile * len(ordered)) - 1)
    return ordered[index] / 1_000_000.0


def _statistics_timing(
    statistics_timestamps_us: list[int],
    frame_timestamps_us: list[int],
    frame_times_us: list[int],
) -> dict[str, Any] | None:
    """Summarize S-record intervals and their co-timed F-record evidence.

    Frame samples are associated by timestamp only. This is a correlation
    aid; it does not infer why an S-record interval is long or prove causality.
    """
    if len(statistics_timestamps_us) < 2:
        return None

    intervals = [
        statistics_timestamps_us[index] - statistics_timestamps_us[index - 1]
        for index in range(1, len(statistics_timestamps_us))
    ]
    gap_counts = {
        "over_1_second": sum(value > 1_000_000 for value in intervals),
        "over_2_seconds": sum(value > 2_000_000 for value in intervals),
        "over_5_seconds": sum(value > 5_000_000 for value in intervals),
        "over_10_seconds": sum(value > 10_000_000 for value in intervals),
    }

    largest_gaps: list[dict[str, Any]] = []
    ranked_indices = sorted(
        range(len(intervals)),
        key=lambda index: intervals[index],
        reverse=True,
    )[:MAX_RETAINED_STAT_GAPS]

    for index in ranked_indices:
        start_us = statistics_timestamps_us[index]
        end_us = statistics_timestamps_us[index + 1]
        gap_us = intervals[index]

        # Match frame records in (previous S timestamp, next S timestamp].
        # Both streams are independently validated as monotonic.
        left = bisect_right(frame_timestamps_us, start_us)
        right = bisect_right(frame_timestamps_us, end_us)
        interval_frames = frame_times_us[left:right]
        frame_sum_us = sum(interval_frames)

        largest_gaps.append({
            "start_timestamp_us": start_us,
            "end_timestamp_us": end_us,
            "gap_seconds": round(gap_us / 1_000_000.0, 6),
            "frame_records": len(interval_frames),
            "frametime_sum_seconds": round(frame_sum_us / 1_000_000.0, 6),
            "frametime_max_ms": (
                round(max(interval_frames) / 1000.0, 3)
                if interval_frames else None
            ),
            "frametime_sum_minus_gap_ms": (
                round((frame_sum_us - gap_us) / 1000.0, 3)
                if interval_frames else None
            ),
        })

    return {
        "interval_count": len(intervals),
        "interval_median_seconds": round(
            _nearest_rank_seconds(intervals, 0.50), 6
        ),
        "interval_p95_seconds": round(
            _nearest_rank_seconds(intervals, 0.95), 6
        ),
        "interval_max_seconds": round(max(intervals) / 1_000_000.0, 6),
        "gap_counts": gap_counts,
        "largest_gaps": largest_gaps,
        "frame_association": "F records in (previous S timestamp, next S timestamp]",
        "causality_inferred": False,
    }


def parse_perf_csv(data: bytes) -> dict[str, Any]:
    """Parse the two-schema F/S stream and return bounded aggregate evidence."""
    try:
        decoded = data.decode("utf-8-sig", errors="strict")
    except UnicodeDecodeError as exc:
        raise PerfCsvError("PerfRecorder CSV is not valid UTF-8") from exc

    reader = csv.reader(io.StringIO(decoded, newline=""))
    try:
        frame_header = next(reader)
        stats_header = next(reader)
    except StopIteration as exc:
        raise PerfCsvError("PerfRecorder CSV is missing its two typed headers") from exc

    if [item.strip() for item in frame_header] != FRAME_HEADER:
        raise PerfCsvError("unexpected PerfRecorder frame header")
    if [item.strip() for item in stats_header] != STATS_HEADER:
        raise PerfCsvError("unexpected PerfRecorder statistics header")

    frame_times_us: list[int] = []
    frame_timestamps_us: list[int] = []
    frame_first_timestamp: int | None = None
    frame_last_timestamp: int | None = None
    stats_timestamps_us: list[int] = []
    stats_first_timestamp: int | None = None
    stats_last_timestamp: int | None = None
    stats_count = 0
    stats_values: dict[str, list[float]] = {
        "gpu_load_pct": [],
        "vidmem_used_mb": [],
        "vidmem_alloc_mb": [],
        "sysmem_used_mb": [],
        "sysmem_alloc_mb": [],
        "draw_calls": [],
        "submissions": [],
        "renderpasses": [],
        "barriers": [],
        "graphics_pipelines": [],
        "compiler_busy": [],
    }

    for line_number, row in enumerate(reader, start=3):
        if not row or all(not item.strip() for item in row):
            continue
        marker = row[0].strip()
        if marker == "F":
            if len(row) != len(FRAME_HEADER):
                raise PerfCsvError(f"frame record at line {line_number} has the wrong column count")
            timestamp = _integer(row[1].strip(), "frame timestamp_us")
            frametime = _integer(row[2].strip(), "frametime_us", minimum=1)
            if frame_last_timestamp is not None and timestamp < frame_last_timestamp:
                raise PerfCsvError("frame timestamps are not monotonic")
            if frame_first_timestamp is None:
                frame_first_timestamp = timestamp
            frame_last_timestamp = timestamp
            frame_timestamps_us.append(timestamp)
            frame_times_us.append(frametime)
        elif marker == "S":
            if len(row) != len(STATS_HEADER):
                raise PerfCsvError(f"statistics record at line {line_number} has the wrong column count")
            timestamp = _integer(row[1].strip(), "statistics timestamp_us")
            if stats_last_timestamp is not None and timestamp < stats_last_timestamp:
                raise PerfCsvError("statistics timestamps are not monotonic")
            if stats_first_timestamp is None:
                stats_first_timestamp = timestamp
            stats_last_timestamp = timestamp
            stats_timestamps_us.append(timestamp)
            stats_count += 1
            for index, field in enumerate(STATS_HEADER[2:], start=2):
                parsed = _number(row[index].strip(), field)
                if parsed is None:
                    continue
                if field == "gpu_load_pct" and parsed > 100:
                    raise PerfCsvError("gpu_load_pct exceeds 100")
                stats_values[field].append(parsed)
        else:
            raise PerfCsvError(f"unknown PerfRecorder record type at line {line_number}")

    result: dict[str, Any] = {
        "format": "PanPlay.PerfRecorder.TypedCSV.v1",
        "frame_records": len(frame_times_us),
        "statistics_records": stats_count,
        "frame_timestamp_start_us": frame_first_timestamp,
        "frame_timestamp_end_us": frame_last_timestamp,
        "statistics_timestamp_start_us": stats_first_timestamp,
        "statistics_timestamp_end_us": stats_last_timestamp,
    }

    if frame_times_us:
        total_us = sum(frame_times_us)
        result.update({
            "frametime_sum_seconds": round(total_us / 1_000_000.0, 6),
            "frametime_avg_ms": round(total_us / len(frame_times_us) / 1000.0, 3),
            "frametime_min_ms": round(min(frame_times_us) / 1000.0, 3),
            "frametime_max_ms": round(max(frame_times_us) / 1000.0, 3),
            "frametime_p50_ms": round(_nearest_rank(frame_times_us, 0.50), 3),
            "frametime_p95_ms": round(_nearest_rank(frame_times_us, 0.95), 3),
            "frametime_p99_ms": round(_nearest_rank(frame_times_us, 0.99), 3),
            "avg_fps_from_frame_times": round(len(frame_times_us) * 1_000_000.0 / total_us, 3),
        })
        if frame_first_timestamp is not None and frame_last_timestamp is not None:
            result["frame_timestamp_span_seconds"] = round(
                (frame_last_timestamp - frame_first_timestamp) / 1_000_000.0, 6
            )

    if stats_count:
        aggregates: dict[str, dict[str, float | int]] = {}
        for field, values in stats_values.items():
            # Omit all-zero columns: some PanPlay telemetry fields use zero as
            # an unavailable sentinel, so exporting them would imply a false reading.
            if values and any(value != 0.0 for value in values):
                aggregates[field] = {
                    "samples": len(values),
                    "avg": round(sum(values) / len(values), 3),
                    "min": round(min(values), 3),
                    "max": round(max(values), 3),
                }
        result["statistics"] = aggregates
        if stats_first_timestamp is not None and stats_last_timestamp is not None:
            result["statistics_timestamp_span_seconds"] = round(
                (stats_last_timestamp - stats_first_timestamp) / 1_000_000.0, 6
            )
        timing = _statistics_timing(
            stats_timestamps_us, frame_timestamps_us, frame_times_us
        )
        if timing is not None:
            result["statistics_timing"] = timing

    return result
