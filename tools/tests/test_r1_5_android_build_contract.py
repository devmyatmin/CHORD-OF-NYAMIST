import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
WORKFLOW = ROOT / ".github" / "workflows" / "r1.5-android-surface.yml"


class R15AndroidBuildContractTests(unittest.TestCase):
    def test_workflow_pins_official_vulkan_headers_and_passes_root_to_cmake(self):
        workflow = WORKFLOW.read_text(encoding="utf-8")
        self.assertIn("Vulkan-Headers.git", workflow)
        self.assertIn("--branch v1.4.368", workflow)
        self.assertIn("VK_HEADER_VERSION 368", workflow)
        self.assertIn(
            'NYAMIST_VULKAN_HEADERS_ROOT="$RUNNER_TEMP/Vulkan-Headers" gradle',
            workflow,
        )

    def test_workflow_installs_required_shader_compiler(self):
        workflow = WORKFLOW.read_text(encoding="utf-8")
        self.assertIn("glslang-tools", workflow)
        self.assertIn("sudo apt-get install --yes glslang-tools", workflow)

    def test_workflow_runs_for_r15_branch_and_r14_integration_prs(self):
        workflow = WORKFLOW.read_text(encoding="utf-8")
        self.assertIn("feature/g925-r1.5-android-surface-swapchain-validation", workflow)
        self.assertIn("feature/g925-r1.4-gpu-resource-transfer-validation", workflow)


if __name__ == "__main__":
    unittest.main()
