import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from panplay_system_csv import (
    SystemCsvError,
    correlate_frame_stalls,
    parse_system_csv,
)


HEADER = (
    "epoch_ms,procs,game_cpu_pct,game_cpu_cores,sys_cpu_pct,game_rss_mb,"
    "mem_avail_mb,batt_temp_c,batt_current_ma,batt_power_mw,"
    "cpu_freq_max_mhz,gpu_freq_mhz,gpu_util_pct,cpu_freqs_mhz\n"
)


class PanPlaySystemCsvTests(unittest.TestCase):
    def test_parses_metrics_and_preserves_unavailable_columns(self):
        data = (
            HEADER
            + "1000,3,25.0,2.0,,1200,2500,40.0,1000,4000,1900,,,500|1900|1900\n"
            + "2008,4,31.0,2.4,,1400,2400,40.5,1200,4800,2000,,,600|2000|2000\n"
            + "3016,4,29.0,2.2,,1300,2450,40.2,1100,4400,2000,,,600|2000|2000\n"
        ).encode()

        summary, samples = parse_system_csv(data)

        self.assertEqual(summary["records"], 3)
        self.assertEqual(summary["sampling_intervals"]["median_seconds"], 1.008)
        self.assertEqual(summary["sampling_intervals"]["max_seconds"], 1.008)
        self.assertEqual(summary["metrics"]["game_cpu_pct"]["median"], 29.0)
        self.assertEqual(summary["metrics"]["gpu_util_pct"], {"samples": 0})
        self.assertEqual(summary["metrics"]["sys_cpu_pct"], {"samples": 0})
        self.assertEqual(len(samples), 3)
        self.assertIsNone(samples[0]["gpu_util_pct"])

    def test_rejects_unexpected_header(self):
        with self.assertRaises(SystemCsvError):
            parse_system_csv(b"epoch_ms,game_cpu_pct\n1000,20\n")

    def test_rejects_non_monotonic_timestamps(self):
        data = (
            HEADER
            + "2000,1,20,1,,100,2000,39,100,400,1000,,,\n"
            + "1000,1,20,1,,100,2000,39,100,400,1000,,,\n"
        ).encode()
        with self.assertRaises(SystemCsvError):
            parse_system_csv(data)

    def test_rejects_non_finite_metric(self):
        data = (
            HEADER
            + "1000,1,nan,1,,100,2000,39,100,400,1000,,,\n"
        ).encode()
        with self.assertRaises(SystemCsvError):
            parse_system_csv(data)

    def test_correlates_stall_window_provisionally_without_causality(self):
        data = (
            HEADER
            + "1001000,3,20,1.5,,1200,2500,40,1000,4000,1800,,,\n"
            + "1002000,4,30,2.0,,1300,2400,40.5,1200,4500,2000,,,\n"
            + "1003000,4,40,2.5,,1400,2300,41,1400,5000,2200,,,\n"
            + "1004000,4,35,2.2,,1350,2350,40.8,1300,4700,2100,,,\n"
        ).encode()
        summary, samples = parse_system_csv(data)

        result = correlate_frame_stalls(
            performance_start_ms=1_000_000,
            performance_end_ms=1_010_000,
            performance_duration_seconds=3.0,
            session_start_ms=1_000_500,
            session_duration_seconds=8,
            frame_stream={
                "frame_timestamp_start_us": 1_000_000,
                "frame_timestamp_end_us": 3_000_000,
                "frame_timestamp_span_seconds": 2.0,
                "frametime_sum_seconds": 2.5,
                "largest_frame_stalls": [
                    {"frame_timestamp_us": 1_000_000, "frametime_ms": 2500.0}
                ],
            },
            system_summary=summary,
            system_samples=samples,
        )

        self.assertEqual(result["alignment_status"], "provisional")
        self.assertTrue(result["frame_stream_covered_by_system"])
        self.assertFalse(result["causality_inferred"])
        window = result["largest_frame_stalls"][0]
        self.assertEqual(window["system_samples_in_frame_interval"], 3)
        self.assertEqual(window["metrics"]["game_cpu_pct"]["median"], 30.0)
        self.assertEqual(window["metrics"]["batt_temp_c"]["max"], 41.0)

    def test_missing_system_coverage_is_reported_not_fabricated(self):
        data = (
            HEADER
            + "1000,1,20,1,,100,2000,39,100,400,1000,,,\n"
            + "2000,1,20,1,,100,2000,39,100,400,1000,,,\n"
        ).encode()
        summary, samples = parse_system_csv(data)
        result = correlate_frame_stalls(
            performance_start_ms=100_000,
            performance_end_ms=110_000,
            performance_duration_seconds=10.0,
            session_start_ms=100_000,
            session_duration_seconds=10,
            frame_stream={
                "frame_timestamp_start_us": 1_000_000,
                "frame_timestamp_end_us": 2_000_000,
                "largest_frame_stalls": [],
            },
            system_summary=summary,
            system_samples=samples,
        )
        self.assertFalse(result["frame_stream_covered_by_system"])
        self.assertEqual(result["largest_frame_stalls"], [])


if __name__ == "__main__":
    unittest.main()
