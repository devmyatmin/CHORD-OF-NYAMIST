import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from check_game_session import EvidenceError, assess_evidence


def complete_evidence():
    return {
        "schema_version": 1,
        "session": {
            "game": "Test Game",
            "duration_seconds": 330,
            "exit_code": 0,
            "crash": False,
            "device_lost_log_scan": False,
        },
        "translator": {
            "id": "dxvk",
            "api": "d3d11",
            "version": "3.1.1-test",
            "device_created_confirmed": True,
            "feature_level": "11_0",
            "swapchain_configured_confirmed": True,
            "swapchain": {"buffer_size": "1280x720"},
        },
        "performance": {
            "dxvk_stats": True,
            "frames": 30000,
            "durationSec": 325.0,
            "p95_frame_time_microseconds": 18000,
            "avgFps": 85.6,
            "low1Fps": 4.64,
            "low01Fps": 0.678,
            "frametimeP95Ms": 43.983,
            "frametimeP99Ms": 63.733,
            "stutters": 11766,
            "gpuLoadAvg": 72.0,
            "ramPeakMb": 2400,
            "battTempMaxC": 41.0,
        },
    }


class GameSessionAssessmentTests(unittest.TestCase):
    def test_complete_session_is_review_ready_but_not_render_validated(self):
        result = assess_evidence(complete_evidence())
        self.assertEqual(result["assessment"], "REVIEW_READY")
        self.assertEqual(result["render_validation"], "discovered_only")
        self.assertFalse(result["production_ready"])
        present = next(item for item in result["checks"] if item["key"] == "vulkan_present")
        self.assertEqual(present["state"], "not_measured")
        self.assertIn("explicit successful Present", present["detail"])
        device = next(item for item in result["checks"] if item["key"] == "d3d11_device_creation")
        swapchain = next(item for item in result["checks"] if item["key"] == "swapchain_setup")
        self.assertEqual(device["state"], "pass")
        self.assertEqual(swapchain["state"], "pass")
        self.assertEqual(result["performance_summary"]["avgFps"], 85.6)
        self.assertEqual(result["performance_summary"]["low1Fps"], 4.64)
        self.assertEqual(result["performance_summary"]["stutters"], 11766)

    def test_missing_d3d11_device_and_swapchain_markers_are_incomplete(self):
        evidence = complete_evidence()
        evidence["translator"]["device_created_confirmed"] = False
        evidence["translator"]["swapchain_configured_confirmed"] = False
        result = assess_evidence(evidence)
        self.assertEqual(result["assessment"], "INCOMPLETE")
        states = {item["key"]: item["state"] for item in result["checks"]}
        self.assertEqual(states["d3d11_device_creation"], "incomplete")
        self.assertEqual(states["swapchain_setup"], "incomplete")

    def test_short_session_is_incomplete(self):
        evidence = complete_evidence()
        evidence["session"]["duration_seconds"] = 120
        result = assess_evidence(evidence)
        self.assertEqual(result["assessment"], "INCOMPLETE")
        self.assertIn("sustained_duration", {item["key"] for item in result["checks"]})

    def test_short_performance_capture_is_incomplete(self):
        evidence = complete_evidence()
        evidence["performance"]["durationSec"] = 299.0
        result = assess_evidence(evidence)
        self.assertEqual(result["assessment"], "INCOMPLETE")

    def test_crashed_process_is_blocked(self):
        evidence = complete_evidence()
        evidence["session"]["crash"] = True
        evidence["session"]["exit_code"] = 1
        result = assess_evidence(evidence)
        self.assertEqual(result["assessment"], "BLOCKED")

    def test_device_loss_marker_blocks_review(self):
        evidence = complete_evidence()
        evidence["session"]["device_lost_log_scan"] = True
        result = assess_evidence(evidence)
        self.assertEqual(result["assessment"], "BLOCKED")

    def test_missing_p95_is_incomplete(self):
        evidence = complete_evidence()
        evidence["performance"].pop("p95_frame_time_microseconds")
        result = assess_evidence(evidence)
        self.assertEqual(result["assessment"], "INCOMPLETE")

    def test_unknown_or_ambiguous_translator_is_incomplete(self):
        evidence = complete_evidence()
        evidence["translator"]["api"] = "d3d9"
        result = assess_evidence(evidence)
        self.assertEqual(result["assessment"], "INCOMPLETE")

    def test_invalid_numeric_values_are_rejected(self):
        evidence = complete_evidence()
        evidence["performance"]["durationSec"] = float("nan")
        with self.assertRaises(EvidenceError):
            assess_evidence(evidence)

    def test_minimum_duration_cannot_be_lowered_below_smoke_window(self):
        with self.assertRaises(EvidenceError):
            assess_evidence(complete_evidence(), 19)


if __name__ == "__main__":
    unittest.main()
