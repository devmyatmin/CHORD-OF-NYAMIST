import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
NATIVE = (
    ROOT / "android" / "surface-validation" / "app" / "src" / "main"
    / "cpp" / "native_surface_swapchain.cpp"
)
HEADER = ROOT / "include" / "nyamist" / "discovery" / "android_surface_swapchain_validation.hpp"
SERIALIZER = ROOT / "src" / "discovery" / "android_surface_swapchain_validation.cpp"
GRADLE = ROOT / "android" / "surface-validation" / "app" / "build.gradle.kts"
ACTIVITY = (
    ROOT / "android" / "surface-validation" / "app" / "src" / "main"
    / "java" / "dev" / "nyamist" / "g925surface" / "MainActivity.java"
)
WORKFLOW = ROOT / ".github" / "workflows" / "r2.2-android-swapchain-recovery.yml"


class R22SwapchainRecoveryContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.native = NATIVE.read_text(encoding="utf-8")
        cls.header = HEADER.read_text(encoding="utf-8")
        cls.serializer = SERIALIZER.read_text(encoding="utf-8")
        cls.gradle = GRADLE.read_text(encoding="utf-8")
        cls.activity = ACTIVITY.read_text(encoding="utf-8")
        cls.workflow = WORKFLOW.read_text(encoding="utf-8")

    def test_r22_report_contract_is_preserved_in_schema_v8(self):
        # R2.3 extends the native report schema while retaining the R2.2
        # oldSwapchain and OUT_OF_DATE evidence fields.
        self.assertIn("schema_version{12U}", self.header)
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

    def test_swapchain_prefers_identity_transform_for_current_surface_extent(self):
        self.assertIn("chooseSwapchainPreTransform", self.native)
        self.assertIn("VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR", self.native)
        self.assertIn("recordSurfaceTransform(report, capabilities)", self.native)
        self.assertIn("swapchain_info.preTransform = chooseSwapchainPreTransform(capabilities)", self.native)
        self.assertIn("create_info.preTransform = chooseSwapchainPreTransform(capabilities)", self.native)
        for field in (
            "surface_current_transform",
            "surface_supported_transforms",
            "swapchain_pre_transform",
            "swapchain_pre_transform_identity_selected",
            "swapchain_pre_transform_policy",
        ):
            self.assertIn(field, self.header)
            self.assertIn(f'"{field}"', self.serializer)

    def test_android_workflow_pins_vulkan_headers_for_r23_native_build(self):
        self.assertIn("Vulkan-Headers.git", self.workflow)
        self.assertIn("--branch v1.4.368", self.workflow)
        self.assertIn('NYAMIST_VULKAN_HEADERS_ROOT="$RUNNER_TEMP/Vulkan-Headers" gradle', self.workflow)

    def test_actual_resize_instrumentation_is_versioned_and_built_on_feature_branch(self):
        self.assertIn("\\\"schema_version\\\": 2", self.activity)
        self.assertIn("surface_resize_observed", self.activity)
        self.assertIn("surface_last_resize_from_width", self.activity)
        self.assertIn("surface_last_resize_to_width", self.activity)
        self.assertIn("feature/g925-r2.2-actual-surface-resize-instrumentation", self.workflow)
        self.assertIn("nyamist-g925-r2.2-actual-resize-validation-apk", self.workflow)

    def test_live_resize_window_pauses_before_acquire_and_present(self):
        self.assertIn("#include <thread>", self.native)
        self.assertIn("live_resize_window_mode_enabled", self.header)
        self.assertIn("live_resize_window_pre_acquire_pause_count", self.header)
        self.assertIn("live_resize_window_pre_present_pause_count", self.header)
        self.assertIn("pauseForLiveResizeWindow(report, live_resize_pause_ms, false)", self.native)
        self.assertIn("pauseForLiveResizeWindow(report, live_resize_pause_ms, true)", self.native)
        self.assertIn("std::this_thread::sleep_for(std::chrono::milliseconds(pause_ms))", self.native)
        self.assertIn("report.live_resize_window_mode_enabled ? 5000U : 0U", self.native)
        self.assertIn("live_resize_window_pause_total_ms", self.serializer)
        self.assertIn("latestLiveResizeReport", self.activity)

    def test_r27_starts_landscape_but_keeps_portrait_wsi_validation_accessible(self):
        self.assertIn('android:screenOrientation="sensorLandscape"', self.manifest)
        self.assertIn("SCREEN_ORIENTATION_SENSOR_LANDSCAPE", self.activity)
        self.assertIn("SCREEN_ORIENTATION_FULL_SENSOR", self.activity)
        self.assertIn("toggleOrientationMode()", self.activity)

    def test_android_pre_rotation_matches_current_transform_and_rotates_clip_space(self):
        self.assertIn("return capabilities.currentTransform;", self.native)
        self.assertIn("chooseIdentitySwapchainExtent", self.native)
        self.assertIn("transformSwapsExtent", self.native)
        self.assertIn("VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR", self.native)
        self.assertIn("VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR", self.native)
        self.assertIn("create_info.preTransform = chooseSwapchainPreTransform(capabilities)", self.native)
        self.assertIn("swapchain_info.preTransform = chooseSwapchainPreTransform(capabilities)", self.native)
        self.assertIn("cmd_push_constants", self.native)
        self.assertIn("pipeline_info.pDynamicState", self.native)
        self.assertIn("surface_prerotation_applied", self.header)
        self.assertIn('"surface_prerotation_status"', self.serializer)
        self.assertIn("layout(push_constant) uniform SurfaceTransform", self.shader)

    def test_apk_displays_the_exact_source_revision(self):
        self.assertIn('buildConfigField("String", "NYAMIST_BUILD_SHA"', self.gradle)
        self.assertIn("providers.environmentVariable(\"GITHUB_SHA\")", self.gradle)
        self.assertIn("BuildConfig.NYAMIST_BUILD_SHA", self.activity)

    def test_app_build_identity_is_r23_with_r22_recovery_fields(self):
        self.assertIn("R2.7 WSI session", self.activity)
        self.assertIn("nyamist-g925-r2.7-prerotation-validation.json", self.activity)
        self.assertIn("out_of_date_full_session_retry_count", self.activity)
        self.assertIn("out_of_date_full_wsi_session_retry", self.activity)
        self.assertIn("out_of_date_full_session_retry_count", self.activity)
        self.assertIn("out_of_date_full_wsi_session_retry", self.activity)



if __name__ == "__main__":
    unittest.main()
