import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
NATIVE = (
    ROOT / "android" / "surface-validation" / "app" / "src" / "main"
    / "cpp" / "native_surface_swapchain.cpp"
)
HEADER = ROOT / "include" / "nyamist" / "discovery" / "android_surface_swapchain_validation.hpp"
SERIALIZER = ROOT / "src" / "discovery" / "android_surface_swapchain_validation.cpp"
ACTIVITY = (
    ROOT / "android" / "surface-validation" / "app" / "src" / "main"
    / "java" / "dev" / "nyamist" / "g925surface" / "MainActivity.java"
)


class R22SwapchainRecoveryContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.native = NATIVE.read_text(encoding="utf-8")
        cls.header = HEADER.read_text(encoding="utf-8")
        cls.serializer = SERIALIZER.read_text(encoding="utf-8")
        cls.activity = ACTIVITY.read_text(encoding="utf-8")

    def test_r22_report_contract_is_preserved_in_schema_v8(self):
        # R2.3 extends the native report schema while retaining the R2.2
        # oldSwapchain and OUT_OF_DATE evidence fields.
        self.assertIn("schema_version{11U}", self.header)
        self.assertIn("NYAMIST G925 R2.6", self.serializer)
        self.assertIn("swapchain_recreation_status", self.header)
        self.assertIn("swapchain_out_of_date_recovery_status", self.header)

    def test_old_swapchain_is_supplied_for_replacement(self):
        self.assertIn("create_info.oldSwapchain = swapchain", self.native)
        self.assertIn("swapchain_old_handle_supplied", self.header)
        self.assertIn("swapchain_logical_device_reused", self.header)
        self.assertIn("swapchain_old_destroy_status", self.header)

    def test_replacement_swapchain_is_acquired_submitted_and_presented(self):
        self.assertIn("vk.acquire_next_image(", self.native)
        self.assertIn("VK_IMAGE_LAYOUT_UNDEFINED", self.native)
        self.assertIn("VK_IMAGE_LAYOUT_PRESENT_SRC_KHR", self.native)
        self.assertIn("vk.queue_submit(queue, 1U, &submit_info", self.native)
        self.assertIn("vk.queue_present(queue, &present_info)", self.native)
        self.assertIn("swapchain_recreation_probe_present_status", self.header)

    def test_out_of_date_and_suboptimal_have_distinct_policies(self):
        self.assertIn("VK_SUBOPTIMAL_KHR", self.native)
        self.assertIn("VK_ERROR_OUT_OF_DATE_KHR", self.native)
        self.assertIn("recreate_after_operation", self.native)
        self.assertIn("recreate_before_operation", self.native)
        self.assertIn("swapchainRecoveryPolicySelfTest()", self.native)
        self.assertIn("swapchain_out_of_date_recovery_attempt_count", self.header)
        self.assertIn("swapchain_out_of_date_recovery_success_count", self.header)

    def test_retry_is_bounded_and_uses_fresh_swapchain_handles(self):
        self.assertIn("attempt < 3U", self.native)
        self.assertIn("oldSwapchain = swapchain", self.native)
        self.assertIn("out_of_date_retry_exhausted", self.native)

    def test_resize_extent_is_requeried_and_reported(self):
        self.assertIn("vk.get_surface_capabilities(", self.native)
        self.assertIn("capabilities.currentExtent", self.native)
        self.assertIn("swapchain_extent_changed", self.header)
        self.assertIn("swapchain_extent_after_width", self.header)
        self.assertIn("swapchain_extent_after_height", self.header)

    def test_app_build_identity_is_r23_with_r22_recovery_fields(self):
        self.assertIn("R2.6 WSI session", self.activity)
        self.assertIn("nyamist-g925-r2.6-uint8-index-probe.json", self.activity)
        self.assertIn("out_of_date_full_session_retry_count", self.activity)
        self.assertIn("out_of_date_full_wsi_session_retry", self.activity)
        self.assertIn("out_of_date_full_session_retry_count", self.activity)
        self.assertIn("out_of_date_full_wsi_session_retry", self.activity)



if __name__ == "__main__":
    unittest.main()
