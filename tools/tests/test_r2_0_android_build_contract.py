import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
WORKFLOW = ROOT / ".github" / "workflows" / "r2.0-android-pixel-readback.yml"


class R20AndroidBuildContractTests(unittest.TestCase):
    def test_workflow_pins_official_vulkan_headers(self):
        workflow = WORKFLOW.read_text(encoding="utf-8")
        self.assertIn("Vulkan-Headers.git", workflow)
        self.assertIn("--branch v1.4.368", workflow)
        self.assertIn("VK_HEADER_VERSION 368", workflow)
        self.assertIn(
            'NYAMIST_VULKAN_HEADERS_ROOT="$RUNNER_TEMP/Vulkan-Headers" gradle',
            workflow,
        )


if __name__ == "__main__":
    unittest.main()
