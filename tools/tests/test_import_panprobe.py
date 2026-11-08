import json
import tempfile
import unittest
import zipfile
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from import_panprobe import ImportErrorDetail, import_archive


class PanProbeImporterTests(unittest.TestCase):
    def make_archive(self, root: Path, *, load_override=None, summary_override=None):
        path = root / "probe.zip"
        load = {
            "driverName": "panvk",
            "driverVersion": 109060195,
            "driverType": "Bundled Mesa PanVK",
            "loadSuccess": True,
            "deviceCount": 1,
            "gpuId": "0xd8300015",
            "buildId": "054636abcd8b1e7344c0463556443b2a0944ab6e",
            "soSha256": "62c2b3f98ca987d5ab50b6f01e2d2c1c5b3ee995fe93666aa5da9415f65c3348",
        }
        summary = {
            "driverName": "panvk",
            "driverInfo": "Mesa 26.3.0-devel (git-795ca7ff3e)",
            "apiVersion": "1.4.363",
            "pass": 41,
            "fail": 0,
            "skip": 0,
            "total": 41,
        }
        load.update(load_override or {})
        summary.update(summary_override or {})
        features = {
            "featureLevels": [
                {"name": "D3D11 FL11_0", "category": "hard", "status": "available", "note": ""},
                {"name": "D3D11 FL12_0", "category": "soft", "status": "missing", "note": "TiledResourcesTier"},
            ]
        }
        with zipfile.ZipFile(path, "w") as archive:
            archive.writestr("driver-load.json", json.dumps(load))
            archive.writestr("20261108-084536/summary.json", json.dumps(summary))
            archive.writestr("20261108-084536/d3d_fl.json", json.dumps(features))
        return path

    def test_imports_driver_and_feature_evidence_without_promoting_render(self):
        with tempfile.TemporaryDirectory() as temp:
            archive = self.make_archive(Path(temp))
            report, sidecar = import_archive(archive)
        self.assertIn("driver_name=panvk\n", report)
        self.assertIn("load_success=true\n", report)
        self.assertIn("panprobe_pass=41\n", report)
        self.assertIn("render_present_passed=false\n", report)
        self.assertEqual(len(sidecar["d3d_feature_levels"]), 2)
        self.assertEqual(sidecar["d3d_feature_levels"][0]["status"], "available")

    def test_rejects_non_panvk_driver(self):
        with tempfile.TemporaryDirectory() as temp:
            archive = self.make_archive(Path(temp), load_override={"driverName": "Mali proprietary"})
            with self.assertRaises(ImportErrorDetail):
                import_archive(archive)

    def test_rejects_missing_or_invalid_required_counter(self):
        with tempfile.TemporaryDirectory() as temp:
            archive = self.make_archive(Path(temp), summary_override={"pass": -1})
            with self.assertRaises(ImportErrorDetail):
                import_archive(archive)

    def test_rejects_ambiguous_summary_members(self):
        with tempfile.TemporaryDirectory() as temp:
            archive = self.make_archive(Path(temp))
            with zipfile.ZipFile(archive, "a") as zipped:
                zipped.writestr("other/summary.json", "{}")
            with self.assertRaises(ImportErrorDetail):
                import_archive(archive)


if __name__ == "__main__":
    unittest.main()
