import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
ACTIVITY = (
    ROOT
    / "android"
    / "surface-validation"
    / "app"
    / "src"
    / "main"
    / "java"
    / "dev"
    / "nyamist"
    / "g925surface"
    / "MainActivity.java"
)
NATIVE = (
    ROOT
    / "android"
    / "surface-validation"
    / "app"
    / "src"
    / "main"
    / "cpp"
    / "native_surface_swapchain.cpp"
)
REPORT_HEADER = (
    ROOT
    / "include"
    / "nyamist"
    / "discovery"
    / "android_surface_swapchain_validation.hpp"
)
REPORT_SERIALIZER = ROOT / "src" / "discovery" / "android_surface_swapchain_validation.cpp"


class R21SurfaceLifecycleContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.activity = ACTIVITY.read_text(encoding="utf-8")
        cls.native = NATIVE.read_text(encoding="utf-8")
        cls.report_header = REPORT_HEADER.read_text(encoding="utf-8")
        cls.report_serializer = REPORT_SERIALIZER.read_text(encoding="utf-8")

    def test_android_lifecycle_callbacks_are_registered(self):
        self.assertIn("implements SurfaceHolder.Callback", self.activity)
        for callback in ("surfaceCreated(", "surfaceChanged(", "surfaceDestroyed(",
                         "onPause(", "onResume("):
            with self.subTest(callback=callback):
                self.assertIn(callback, self.activity)

    def test_native_sessions_are_serialized(self):
        self.assertIn("Executors.newSingleThreadExecutor", self.activity)
        self.assertIn("maxConcurrentNativeSessions", self.activity)
        self.assertIn("max_concurrent_native_sessions", self.activity)
        self.assertIn("native_session_active_count", self.activity)

    def test_stale_surface_generations_are_rejected(self):
        self.assertIn("sessionGeneration != surfaceGeneration", self.activity)
        self.assertIn("requestId != latestRequestId", self.activity)
        self.assertIn("nativeSessionSkippedCount", self.activity)

    def test_lifecycle_report_is_attached_to_native_report(self):
        self.assertIn("android_surface_lifecycle", self.activity)
        for field in (
            "surface_created_count",
            "surface_recreated_count",
            "surface_changed_count",
            "surface_destroyed_count",
            "surface_resize_count",
            "surface_resize_observed",
            "surface_initial_width",
            "surface_initial_height",
            "surface_last_known_width",
            "surface_last_known_height",
            "surface_last_resize_from_width",
            "surface_last_resize_from_height",
            "surface_last_resize_to_width",
            "surface_last_resize_to_height",
            "surface_resize_test_toggle_count",
            "live_resize_window_request_count",
            "live_resize_window_mode_requested",
            "configuration_change_count",
            "last_configuration_orientation",
            "activity_resume_count",
            "activity_pause_count",
            "native_session_started_count",
            "native_session_completed_count",
            "native_session_success_count",
            "native_session_skipped_count",
        ):
            with self.subTest(field=field):
                self.assertIn(field, self.activity)

    def test_resize_detection_survives_surface_destruction_and_recreation(self):
        changed_body = self.activity.split("public void surfaceChanged(", 1)[1].split(
            "private void toggleSurfaceTestSize(", 1
        )[0]
        created_body = self.activity.split("public void surfaceCreated(", 1)[1].split(
            "public void surfaceChanged(", 1
        )[0]
        self.assertIn("lastKnownSurfaceWidth", changed_body)
        self.assertIn("lastKnownSurfaceHeight", changed_body)
        self.assertIn("lastResizeFromWidth = previousWidth", changed_body)
        self.assertIn("lastResizeToWidth = width", changed_body)
        self.assertIn("currentSurfaceWidth = 0", created_body)
        self.assertNotIn("lastKnownSurfaceWidth = 0", created_body)

    def test_rotation_and_controlled_surface_resize_are_reported(self):
        self.assertIn("onConfigurationChanged(Configuration newConfig)", self.activity)
        self.assertIn("Configuration.ORIENTATION_PORTRAIT", self.activity)
        self.assertIn("Configuration.ORIENTATION_LANDSCAPE", self.activity)
        self.assertIn("INCREASE SURFACE SIZE (WSI RESIZE TEST)", self.activity)
        self.assertIn("RESTORE SURFACE SIZE (220dp)", self.activity)
        self.assertIn("params.height = dp(expanded ? 320 : 220)", self.activity)
        self.assertIn("\\\"schema_version\\\": 2", self.activity)

    def test_live_wsi_resize_window_is_explicit_and_report_is_preserved(self):
        self.assertIn("START LIVE WSI RESIZE WINDOW", self.activity)
        self.assertIn("requestLiveResizeWindow()", self.activity)
        self.assertIn("live_wsi_resize_window", self.activity)
        self.assertIn("validateSurfaceNative(sessionSurface, width, height, liveResizeMode)", self.activity)
        self.assertIn("latestLiveResizeReport = completeReport", self.activity)
        self.assertIn("nyamist-g925-r2.2-live-wsi-resize-window.json", self.activity)
        self.assertIn("copyLatestButton.setOnClickListener(view -> copyReport(false))", self.activity)
        self.assertIn("copyLiveButton.setOnClickListener(view -> copyReport(true))", self.activity)
        self.assertIn("String reportToCopy = liveWsi ? latestLiveResizeReport : latestReport", self.activity)
        self.assertIn("session_current_at_completion", self.activity)

    def test_triangle_viewport_preserves_shape_across_surface_aspect_ratios(self):
        self.assertIn("std::min(extent.width, extent.height)", self.native)
        self.assertIn("viewport.x = static_cast<float>(render_viewport_x)", self.native)
        self.assertIn("viewport.y = static_cast<float>(render_viewport_y)", self.native)
        self.assertIn("viewport.width = static_cast<float>(render_viewport_extent)", self.native)
        self.assertIn("render_viewport.height = static_cast<float>(render_viewport_extent)", self.native)
        self.assertIn("render_scissor.extent = extent", self.native)
        self.assertIn("VK_DYNAMIC_STATE_VIEWPORT", self.native)
        self.assertIn("VK_DYNAMIC_STATE_SCISSOR", self.native)
        self.assertIn("vk.cmd_set_viewport(command_buffer, 0U, 1U, &render_viewport)", self.native)
        self.assertIn("vk.cmd_set_scissor(command_buffer, 0U, 1U, &render_scissor)", self.native)
        self.assertIn('LOAD_DEVICE(cmd_set_viewport, "vkCmdSetViewport")', self.native)
        self.assertIn('LOAD_DEVICE(cmd_set_scissor, "vkCmdSetScissor")', self.native)

    def test_dynamic_viewport_and_scissor_values_are_exported(self):
        fields = (
            "render_viewport_dynamic_state_enabled",
            "render_viewport_x",
            "render_viewport_y",
            "render_viewport_width",
            "render_viewport_height",
            "render_viewport_update_count",
            "render_scissor_width",
            "render_scissor_height",
            "render_scissor_update_count",
        )
        for field in fields:
            with self.subTest(field=field):
                self.assertIn(field, self.report_header)
                self.assertIn(f'"{field}"', self.report_serializer)
        self.assertIn("report.render_viewport_x = render_viewport_x", self.native)
        self.assertIn("report.render_viewport_width = render_viewport.width", self.native)
        self.assertIn("++report.render_viewport_update_count", self.native)
        self.assertIn("report.render_scissor_width = render_scissor.extent.width", self.native)
        self.assertIn("++report.render_scissor_update_count", self.native)

    def test_activity_resume_forces_a_fresh_wsi_session_on_existing_surface(self):
        resume_body = self.activity.split("protected void onResume()", 1)[1].split(
            "protected void onPause()", 1
        )[0]
        self.assertIn('scheduleValidation(holderToRebuild, width, height, "activity_resumed", true)',
                      resume_body)

    def test_surface_destruction_waits_for_native_cleanup(self):
        destroy_body = self.activity.split("public void surfaceDestroyed(", 1)[1].split(
            "private void requestManualRebuild(", 1
        )[0]
        self.assertIn("while (activeNativeSessions > 0)", destroy_body)
        self.assertIn("lifecycleLock.wait()", destroy_body)
        self.assertIn("lifecycleLock.notifyAll()", self.activity)
        self.assertIn("ANativeWindow_fromSurface", self.native)

    def test_native_invocation_rebuilds_a_fresh_wsi_session(self):
        self.assertIn("ANativeWindow_fromSurface", self.native)
        self.assertIn("create_android_surface", self.native)
        self.assertIn("create_swapchain", self.native)
        self.assertIn("destroy_swapchain", self.native)
        self.assertIn("destroy_surface", self.native)
        self.assertIn("native_window_release_status", self.native)


if __name__ == "__main__":
    unittest.main()
