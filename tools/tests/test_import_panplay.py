import hashlib
import json
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from import_panplay import ImportErrorDetail, import_archives


class PanPlayImporterTests(unittest.TestCase):
    def make_session_archive(
        self,
        root: Path,
        *,
        api: str = "d3d11",
        include_vkd3d: bool = False,
        tamper_manifest: bool = False,
        include_d3d11_runtime_markers: bool = True,
    ) -> tuple[Path, str]:
        path = root / "panplay-session.zip"
        session_id = "20261108-120000_Test_Game"
        perf_id = "20261108-120001_Test_Game"
        base = f"20261108-120000_Test_Game"
        session = {
            "time": 1794139200000,
            "startMs": 1794139190000,
            "durationSec": 360,
            "perfId": perf_id,
            "game": "Test Game",
            "exe": "/data/user/0/dev.zenithblue.panvklauncher/files/games/test.exe",
            "reason": "Game exited normally (exit 0)",
            "exit": 0,
            "userStopped": False,
            "crash": False,
            "deviceLost": False,
            "files": ["device-facts.json", "wine-run.log"],
            "manifestInfo": {
                "driver": {
                    "name": "Mesa PanVK",
                    "id": "mesa-panvk-test",
                    "version": "26.3.0-test",
                    "buildId": "build-test",
                    "soSha256": "a" * 64,
                },
                "dxvk": {"enabled": True, "version": "3.1.1-test"},
            },
        }
        if api == "d3d11":
            session["files"].append("dxvk-dxcube-arm64ec_d3d11.log")
        elif api == "d3d9":
            session["files"].append("dxvk-dxcube-i686_d3d9.log")
        if include_vkd3d:
            session["files"].append("vkd3d.log")

        facts = {
            "kbaseUapi": "CSF driver, uAPI version 1.30",
            "gpuId": "0xd8300015",
            "pageSize": 4096,
            "props": {"ro.board.platform": "mt6991"},
        }
        members = {
            f"{base}/session.json": json.dumps(session).encode(),
            f"{base}/device-facts.json": json.dumps(facts).encode(),
            f"{base}/wine-run.log": b"Wine started; session ended normally.\n",
        }
        if api == "d3d11":
            if include_d3d11_runtime_markers:
                d3d11_log = (
                    b"info: D3D11InternalCreateDevice: Maximum supported feature level: D3D_FEATURE_LEVEL_11_1\n"
                    b"info: D3D11InternalCreateDevice: Using feature level D3D_FEATURE_LEVEL_11_0\n"
                    b"info: Presenter: Actual swapchain properties:\n"
                    b"info:   Format:       VK_FORMAT_B8G8R8A8_UNORM\n"
                    b"info:   Color space:  VK_COLOR_SPACE_SRGB_NONLINEAR_KHR\n"
                    b"info:   Present mode: VK_PRESENT_MODE_IMMEDIATE_KHR\n"
                    b"info:   Buffer size:  1280x720\n"
                    b"info:   Image count:  4\n"
                )
            else:
                d3d11_log = b"DXVK D3D11 log without device-creation marker\n"
            members[f"{base}/dxvk-dxcube-arm64ec_d3d11.log"] = d3d11_log
        elif api == "d3d9":
            members[f"{base}/dxvk-dxcube-i686_d3d9.log"] = b"DXVK D3D9 log\n"
        if include_vkd3d:
            members[f"{base}/vkd3d.log"] = b"vkd3d-proton diagnostic log\n"

        manifest_files = [
            {
                "path": name,
                "size": len(data),
                "sha256": hashlib.sha256(data).hexdigest(),
            }
            for name, data in members.items()
        ]
        if tamper_manifest:
            manifest_files[0]["sha256"] = "0" * 64
        manifest = {
            "timestamp": "20261108-120600",
            "app": {"versionName": "1.3.1", "versionCode": 45},
            "game": "Test Game",
            "driver": {"name": "Mesa PanVK", "id": "mesa-panvk-test"},
            "device": {
                "manufacturer": "Xiaomi",
                "model": "REDMI K80 Ultra",
                "board": "mt6991",
                "hardware": "mt6991",
                "socManufacturer": "MediaTek",
                "socModel": "MT6991",
            },
            "android": {"release": "16", "sdk": 36, "kernel": "6.12-test"},
            "gpu": {"deviceName": "Immortalis-G925"},
            "deviceFacts": facts,
            "files": manifest_files,
        }
        with zipfile.ZipFile(path, "w", compression=zipfile.ZIP_DEFLATED) as archive:
            for name, data in members.items():
                archive.writestr(name, data)
            archive.writestr("manifest.json", json.dumps(manifest))
        return path, perf_id

    def make_performance_archive(
        self,
        root: Path,
        perf_id: str,
        *,
        dxvk_stats: bool = True,
        p95: float = 10.0,
    ) -> Path:
        path = root / "panplay-performance.zip"
        summary = {
            "id": perf_id,
            "startMs": 1794139190000,
            "endMs": 1794139550000,
            "game": "Test Game",
            "device": "Xiaomi REDMI K80 Ultra",
            "driverId": "mesa-panvk-test",
            "driver": "Mesa PanVK",
            "driverVersion": "26.3.0-test",
            "dxvk": "3.1.1-test",
            "dxvkStats": dxvk_stats,
            "frames": 3 if dxvk_stats else 0,
            "durationSec": 360.0,
            "avgFps": 100.0,
            "frametimeP50Ms": 8.0 if dxvk_stats else None,
            "frametimeP95Ms": p95 if dxvk_stats else None,
            "frametimeP99Ms": 14.0 if dxvk_stats else None,
            "gpuLoadAvg": 72.0,
            "gameCpuAvg": 28.0,
            "ramPeakMb": 1024.0,
            "battTempMaxC": 38.0,
            "powerAvgMw": 4000.0,
        }
        with zipfile.ZipFile(path, "w", compression=zipfile.ZIP_DEFLATED) as archive:
            archive.writestr(f"{perf_id}.json", json.dumps(summary))
            frame_records = (
                "F,1000000,10000\nF,1010000,15000\nF,1025000,8000\n"
                if dxvk_stats else ""
            )
            typed_csv = (
                "# F,timestamp_us,frametime_us\n"
                "# S,timestamp_us,gpu_load_pct,vidmem_used_mb,vidmem_alloc_mb,"
                "sysmem_used_mb,sysmem_alloc_mb,draw_calls,submissions,renderpasses,"
                "barriers,graphics_pipelines,compiler_busy\n"
                "S,1000000,72,128,256,0,0,100,3,2,1,10,0\n"
                + frame_records
            )
            archive.writestr(f"{perf_id}.csv", typed_csv)
            archive.writestr(
                f"{perf_id}-sys.csv",
                "epoch_ms,procs,game_cpu_pct,game_cpu_cores,sys_cpu_pct,"
                "game_rss_mb,mem_avail_mb,batt_temp_c,batt_current_ma,"
                "batt_power_mw,cpu_freq_max_mhz,gpu_freq_mhz,gpu_util_pct,cpu_freqs_mhz\n"
                "1794139190990,2,20,1.2,,900,2400,39.5,800,3200,1800,,,500|1800|1800\n"
                "1794139192008,3,24,1.5,,950,2300,39.6,900,3500,1900,,,500|1900|1900\n"
            )
        return path

    def test_imports_d3d11_and_p95_but_keeps_render_validation_discovery_only(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            session_zip, perf_id = self.make_session_archive(root)
            perf_zip = self.make_performance_archive(root, perf_id)
            report, sidecar = import_archives(session_zip, perf_zip)

        self.assertIsNotNone(report)
        self.assertIn("translator_id=dxvk\n", report)
        self.assertIn("api=d3d11\n", report)
        self.assertIn("translator_version=3.1.1-test\n", report)
        self.assertIn("device_created=true\n", report)
        self.assertIn("swapchain_configured=true\n", report)
        self.assertIn("d3d11_selected_feature_level=11_0\n", report)
        self.assertIn("d3d11_maximum_supported_feature_level=11_1\n", report)
        self.assertIn("render_present_passed=false\n", report)
        self.assertIn("frame_count=3\n", report)
        self.assertIn("frame_csv_frame_records=3\n", report)
        self.assertIn("frame_csv_statistics_records=1\n", report)
        self.assertNotIn("present_calls=", report)
        self.assertNotIn("present_failures=", report)
        self.assertTrue(sidecar["translator"]["device_created_confirmed"])
        self.assertTrue(sidecar["translator"]["swapchain_configured_confirmed"])
        self.assertEqual(sidecar["translator"]["feature_level"], "11_0")
        self.assertEqual(sidecar["translator"]["selected_feature_level"], "11_0")
        self.assertEqual(sidecar["translator"]["maximum_feature_level"], "11_1")
        self.assertEqual(sidecar["translator"]["feature_levels_used"], ["11_0"])
        self.assertEqual(
            sidecar["translator"]["swapchain"],
            {
                "format": "VK_FORMAT_B8G8R8A8_UNORM",
                "color_space": "VK_COLOR_SPACE_SRGB_NONLINEAR_KHR",
                "present_mode": "VK_PRESENT_MODE_IMMEDIATE_KHR",
                "buffer_size": "1280x720",
                "image_count": "4",
            },
        )
        self.assertFalse(sidecar["translator"]["render_present_passed"])
        self.assertEqual(sidecar["validation"]["level"], "discovered")
        self.assertEqual(sidecar["performance"]["p95_frame_time_microseconds"], 10000)
        self.assertEqual(sidecar["performance"]["csv_stream"]["frame_records"], 3)
        self.assertEqual(sidecar["performance"]["csv_stream"]["statistics_records"], 1)
        self.assertEqual(sidecar["performance"]["csv_stream"]["frametime_p95_ms"], 15.0)
        self.assertEqual(sidecar["performance"]["system_csv"]["records"], 2)
        self.assertEqual(
            sidecar["performance"]["system_csv"]["metrics"]["game_cpu_pct"]["samples"], 2
        )
        self.assertTrue(
            sidecar["performance"]["system_correlation"]["frame_stream_covered_by_system"]
        )
        self.assertFalse(
            sidecar["performance"]["system_correlation"]["causality_inferred"]
        )
        self.assertEqual(sidecar["device"]["device_facts"]["gpuId"], "0xd8300015")
        self.assertTrue(sidecar["validation"]["automatic_application_allowed"] is False)

    def test_d3d11_log_without_device_marker_does_not_confirm_device_creation(self):
        with tempfile.TemporaryDirectory() as temp:
            session_zip, _ = self.make_session_archive(
                Path(temp), include_d3d11_runtime_markers=False
            )
            report, sidecar = import_archives(session_zip)
        self.assertIn("device_created=false\n", report)
        self.assertIn("swapchain_configured=false\n", report)
        self.assertFalse(sidecar["translator"]["device_created_confirmed"])
        self.assertFalse(sidecar["translator"]["swapchain_configured_confirmed"])
        self.assertIsNone(sidecar["translator"]["feature_level"])
        self.assertIsNone(sidecar["translator"]["selected_feature_level"])
        self.assertIsNone(sidecar["translator"]["maximum_feature_level"])
        self.assertFalse(sidecar["translator"]["render_present_passed"])

    def test_detects_vkd3d_from_vkd3d_log_but_does_not_invent_version(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            session_zip, perf_id = self.make_session_archive(
                root, api="d3d9", include_vkd3d=True
            )
            report, sidecar = import_archives(session_zip)
        self.assertIn("translator_id=vkd3d-proton\n", report)
        self.assertIn("api=d3d12\n", report)
        self.assertIn("translator_version=unreported\n", report)
        self.assertIn("render_present_passed=false\n", report)
        self.assertIsNone(sidecar["performance"])

    def test_d3d9_only_session_has_no_unsupported_adapter_report(self):
        with tempfile.TemporaryDirectory() as temp:
            session_zip, _ = self.make_session_archive(Path(temp), api="d3d9")
            report, sidecar = import_archives(session_zip)
        self.assertIsNone(report)
        self.assertEqual(sidecar["validation"]["level"], "unclassified")

    def test_ambiguous_d3d11_and_vkd3d_logs_do_not_select_a_translator(self):
        with tempfile.TemporaryDirectory() as temp:
            session_zip, _ = self.make_session_archive(
                Path(temp), api="d3d11", include_vkd3d=True
            )
            report, sidecar = import_archives(session_zip)
        self.assertIsNone(report)
        self.assertEqual(sidecar["translator"]["id"], None)

    def test_manifest_sha256_mismatch_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            session_zip, _ = self.make_session_archive(
                Path(temp), tamper_manifest=True
            )
            with self.assertRaises(ImportErrorDetail):
                import_archives(session_zip)

    def test_mismatched_performance_id_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            session_zip, _ = self.make_session_archive(root)
            performance_zip = self.make_performance_archive(root, "20261108-OTHER")
            with self.assertRaises(ImportErrorDetail):
                import_archives(session_zip, performance_zip)

    def test_non_finite_frame_time_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            session_zip, perf_id = self.make_session_archive(root)
            performance_zip = self.make_performance_archive(root, perf_id, p95=float("nan"))
            with self.assertRaises(ImportErrorDetail):
                import_archives(session_zip, performance_zip)

    def test_dxvk_stats_false_does_not_export_fake_frame_count(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            session_zip, perf_id = self.make_session_archive(root)
            performance_zip = self.make_performance_archive(
                root, perf_id, dxvk_stats=False
            )
            report, sidecar = import_archives(session_zip, performance_zip)
        self.assertNotIn("frame_count=", report)
        self.assertNotIn("p95_frame_time_microseconds", sidecar["performance"])
        self.assertFalse(sidecar["performance"]["dxvk_stats"])

    def test_duplicate_zip_member_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            session_zip, _ = self.make_session_archive(Path(temp))
            with zipfile.ZipFile(session_zip, "a") as archive:
                archive.writestr("manifest.json", "{}")
            with self.assertRaises(ImportErrorDetail):
                import_archives(session_zip)


if __name__ == "__main__":
    unittest.main()
