import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from panplay_perf_csv import PerfCsvError, parse_perf_csv


HEADER = (
    "# F,timestamp_us,frametime_us\n"
    "# S,timestamp_us,gpu_load_pct,vidmem_used_mb,vidmem_alloc_mb,"
    "sysmem_used_mb,sysmem_alloc_mb,draw_calls,submissions,renderpasses,"
    "barriers,graphics_pipelines,compiler_busy\n"
)


class PanPlayPerfCsvTests(unittest.TestCase):
    def test_parses_interleaved_frame_and_statistics_records(self):
        data = (
            HEADER
            + "S,1000000,60,100,128,0,0,120,4,2,1,10,0\n"
            + "F,1000000,10000\n"
            + "F,1010000,20000\n"
            + "S,1020000,80,120,128,0,0,140,5,3,1,12,0\n"
            + "F,1030000,30000\n"
        ).encode()
        result = parse_perf_csv(data)

        self.assertEqual(result["frame_records"], 3)
        self.assertEqual(result["statistics_records"], 2)
        self.assertEqual(result["frame_timestamp_span_seconds"], 0.03)
        self.assertEqual(result["frametime_sum_seconds"], 0.06)
        self.assertEqual(result["frametime_p50_ms"], 20.0)
        self.assertEqual(result["frametime_p95_ms"], 30.0)
        self.assertEqual(result["frametime_p99_ms"], 30.0)
        self.assertEqual(result["statistics"]["gpu_load_pct"]["avg"], 70.0)
        self.assertEqual(result["statistics"]["draw_calls"]["max"], 140.0)

    def test_accepts_statistics_only_export_when_dxvk_frame_stats_are_off(self):
        data = (
            HEADER
            + "S,1000000,10,20,30,0,0,4,4,2,15,2,0\n"
        ).encode()
        result = parse_perf_csv(data)
        self.assertEqual(result["frame_records"], 0)
        self.assertEqual(result["statistics_records"], 1)
        self.assertNotIn("frametime_p95_ms", result)

    def test_rejects_unexpected_typed_header(self):
        with self.assertRaises(PerfCsvError):
            parse_perf_csv(b"time,frames\n0,100\n")

    def test_rejects_non_monotonic_frame_timestamps(self):
        data = (
            HEADER
            + "F,1000000,10000\n"
            + "F,999999,12000\n"
        ).encode()
        with self.assertRaises(PerfCsvError):
            parse_perf_csv(data)

    def test_rejects_gpu_load_over_100_percent(self):
        data = (
            HEADER
            + "S,1000000,101,20,30,0,0,4,4,2,15,2,0\n"
        ).encode()
        with self.assertRaises(PerfCsvError):
            parse_perf_csv(data)

    def test_rejects_unknown_record_marker(self):
        data = (HEADER + "X,1000000,10\n").encode()
        with self.assertRaises(PerfCsvError):
            parse_perf_csv(data)


if __name__ == "__main__":
    unittest.main()
