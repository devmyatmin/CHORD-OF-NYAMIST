import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
WORKFLOW = ROOT / ".github" / "workflows" / "r1.7-android-shader-pipeline.yml"


class R17AndroidBuildContractTests(unittest.TestCase):
    def test_workflow_pins_official_vulkan_headers_and_passes_root_to_cmake(self):
        workflow = WORKFLOW.read_text(encoding="utf-8")
        self.assertIn("Vulkan-Headers.git", workflow)
        self.assertIn("--branch v1.4.368", workflow)
        self.assertIn("VK_HEADER_VERSION 368", workflow)
        self.assertIn(
            'NYAMIST_VULKAN_HEADERS_ROOT="$RUNNER_TEMP/Vulkan-Headers" gradle',
            workflow,
        )

    def test_workflow_runs_for_r17_branch_and_r16_integration_prs(self):
        workflow = WORKFLOW.read_text(encoding="utf-8")
        self.assertIn("feature/g925-r1.7-shader-pipeline-triangle-draw", workflow)
        self.assertIn("feature/g925-r1.6-swapchain-rendering-frame-timing", workflow)


if __name__ == "__main__":
    unittest.main()
