import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
NATIVE = ROOT / "android/surface-validation/app/src/main/cpp/native_surface_swapchain.cpp"
HEADER = ROOT / "include/nyamist/discovery/android_surface_swapchain_validation.hpp"
SERIALIZER = ROOT / "src/discovery/android_surface_swapchain_validation.cpp"
ACTIVITY = ROOT / "android/surface-validation/app/src/main/java/dev/nyamist/g925surface/MainActivity.java"
GRADLE = ROOT / "android/surface-validation/app/build.gradle.kts"


class R26Uint8IndexFunctionalProbeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.native = NATIVE.read_text(encoding="utf-8")
        cls.header = HEADER.read_text(encoding="utf-8")
        cls.serializer = SERIALIZER.read_text(encoding="utf-8")
        cls.activity = ACTIVITY.read_text(encoding="utf-8")
        cls.gradle = GRADLE.read_text(encoding="utf-8")

    def test_schema_and_release_identity_are_r26(self):
        self.assertIn("schema_version{12U}", self.header)
        self.assertIn('"CHORD OF NYAMIST / NYAMIST G925 R2.6"', self.serializer)
        self.assertIn('schema_version\\": 12', self.activity)
        self.assertIn("nyamist-g925-r2.6-uint8-index-probe.json", self.activity)
        self.assertIn("versionCode = 260", self.gradle)
        self.assertIn('versionName = "2.6.0"', self.gradle)

    def test_only_previously_supported_uint8_feature_is_enabled(self):
        self.assertIn('"VK_KHR_index_type_uint8.indexTypeUint8"', self.native)
        self.assertIn("contains_feature(report.vulkan_1_4_promoted_features_supported)", self.native)
        self.assertIn("hasDeviceExtension(selected_physical_device_extensions", self.native)
        self.assertIn("VK_KHR_INDEX_TYPE_UINT8_EXTENSION_NAME", self.native)
        self.assertIn("uint8_index_features_enable.indexTypeUint8 = VK_TRUE", self.native)
        self.assertIn("device_info.pNext = &uint8_index_features_enable", self.native)
        self.assertIn("report.index_type_uint8_feature_enable_status = \"requested\"", self.native)
        self.assertIn("report.index_type_uint8_feature_enable_status = \"enabled\"", self.native)
        self.assertIn("report.index_type_uint8_feature_enable_status = \"device_create_failed\"", self.native)

    def test_query_failure_is_not_mislabeled_as_unsupported(self):
        self.assertIn("contains_feature(report.vulkan_1_4_promoted_features_not_supported)", self.native)
        self.assertIn("contains_feature(report.vulkan_1_4_promoted_features_not_advertised)", self.native)
        self.assertIn("contains_feature(report.vulkan_1_4_promoted_features_query_failed)", self.native)
        self.assertIn("contains_feature(report.vulkan_1_4_promoted_features_not_queryable)", self.native)
        self.assertIn('report.index_type_uint8_feature_support_status = "query_failed"', self.native)
        self.assertIn('report.index_type_uint8_feature_support_status = "not_queryable"', self.native)
        self.assertIn('report.index_type_uint8_feature_support_status = "unclassified"', self.native)

    def test_uint8_index_buffer_is_uploaded_and_drawn(self):
        self.assertIn("VK_BUFFER_USAGE_INDEX_BUFFER_BIT", self.native)
        self.assertIn("constexpr std::array<std::uint8_t, 3> indices{0U, 1U, 2U}", self.native)
        self.assertIn("VK_INDEX_TYPE_UINT8_KHR", self.native)
        self.assertIn("vk.cmd_bind_index_buffer(command_buffer, uint8_index_buffer", self.native)
        self.assertIn("vk.cmd_draw_indexed(command_buffer, 3U, 1U, 0, 0U, 0U)", self.native)
        self.assertIn("report.index_type_uint8_draw_recorded = true", self.native)

    def test_pixel_readback_is_required_to_confirm_functional_probe(self):
        self.assertIn('report.index_type_uint8_probe_status = "success"', self.native)
        self.assertIn('report.index_type_uint8_probe_status = "pixel_readback_failed"', self.native)
        self.assertIn("uint8_index_probe_valid", self.native)
        self.assertIn("report.index_type_uint8_buffer_destroy_status == \"success\"", self.native)
        self.assertIn("report.index_type_uint8_memory_free_status == \"success\"", self.native)

    def test_report_serializes_each_probe_stage(self):
        for field in (
            "index_type_uint8_extension_status",
            "index_type_uint8_feature_support_status",
            "index_type_uint8_feature_enable_status",
            "index_type_uint8_buffer_create_status",
            "index_type_uint8_memory_allocate_status",
            "index_type_uint8_memory_bind_status",
            "index_type_uint8_memory_map_status",
            "index_type_uint8_upload_status",
            "index_type_uint8_memory_unmap_status",
            "index_type_uint8_draw_entry_points_status",
            "index_type_uint8_draw_record_status",
            "index_type_uint8_draw_recorded",
            "index_type_uint8_draw_index_count",
            "index_type_uint8_probe_status",
            "index_type_uint8_buffer_destroy_status",
            "index_type_uint8_memory_free_status",
        ):
            self.assertIn(field, self.header)
            self.assertIn(field, self.serializer)

    def test_feature_not_supported_is_a_nonfatal_skip(self):
        self.assertIn('report.index_type_uint8_feature_support_status = "not_supported"', self.native)
        self.assertIn("report.index_type_uint8_probe_status = support", self.native)
        self.assertIn('report.index_type_uint8_probe_status == "not_supported"', self.native)


if __name__ == "__main__":
    unittest.main()
