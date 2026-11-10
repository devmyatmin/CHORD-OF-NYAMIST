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


class R21SurfaceLifecycleContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.activity = ACTIVITY.read_text(encoding="utf-8")
        cls.native = NATIVE.read_text(encoding="utf-8")

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
            "activity_resume_count",
            "activity_pause_count",
            "native_session_started_count",
            "native_session_completed_count",
            "native_session_success_count",
            "native_session_skipped_count",
        ):
            with self.subTest(field=field):
                self.assertIn(field, self.activity)

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
