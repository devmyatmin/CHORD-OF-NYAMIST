import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
NATIVE = ROOT / "android/surface-validation/app/src/main/cpp/native_surface_swapchain.cpp"
HEADER = ROOT / "include/nyamist/discovery/android_surface_swapchain_validation.hpp"
SERIALIZER = ROOT / "src/discovery/android_surface_swapchain_validation.cpp"
ACTIVITY = ROOT / "android/surface-validation/app/src/main/java/dev/nyamist/g925surface/MainActivity.java"
GRADLE = ROOT / "android/surface-validation/app/build.gradle.kts"

PROMOTED_EXTENSIONS = (
    "VK_KHR_dynamic_rendering_local_read",
    "VK_KHR_global_priority",
    "VK_KHR_index_type_uint8",
    "VK_KHR_line_rasterization",
    "VK_KHR_load_store_op_none",
    "VK_KHR_maintenance5",
    "VK_KHR_maintenance6",
    "VK_KHR_map_memory2",
    "VK_KHR_push_descriptor",
    "VK_KHR_shader_expect_assume",
    "VK_KHR_shader_float_controls2",
    "VK_KHR_shader_subgroup_rotate",
    "VK_KHR_vertex_attribute_divisor",
    "VK_EXT_host_image_copy",
    "VK_EXT_pipeline_protected_access",
    "VK_EXT_pipeline_robustness",
)


class R24PromotedExtensionInventoryContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.native = NATIVE.read_text(encoding="utf-8")
        cls.header = HEADER.read_text(encoding="utf-8")
        cls.serializer = SERIALIZER.read_text(encoding="utf-8")
        cls.activity = ACTIVITY.read_text(encoding="utf-8")
        cls.gradle = GRADLE.read_text(encoding="utf-8")

    def test_inventory_contains_exact_vulkan_1_4_promotion_set(self):
        self.assertIn("kVulkan14PromotedExtensions", self.native)
        for extension in PROMOTED_EXTENSIONS:
            self.assertIn('"' + extension + '"', self.native)
        self.assertIn("std::array<const char*, 16>", self.native)

    def test_inventory_uses_selected_physical_device_extensions(self):
        self.assertIn("selected_physical_device_extensions = device_extensions", self.native)
        self.assertIn("vk.enumerate_device_extensions", self.native)
        self.assertIn("hasDeviceExtension(", self.native)
        self.assertIn("extension_name_advertisement_on_pre_1_4_device", self.native)

    def test_api_14_core_path_is_not_reported_as_missing_extension_names(self):
        self.assertIn("not_applicable_core_api_1_4_or_higher", self.native)
        self.assertIn("promoted_functionality_is_in_core_api", self.native)

    def test_report_separates_advertised_and_not_advertised_names(self):
        self.assertIn("schema_version{9U}", self.header)
        self.assertIn("vulkan_1_4_promoted_extensions_supported", self.header)
        self.assertIn("vulkan_1_4_promoted_extensions_not_advertised", self.header)
        self.assertIn("stringArrayField(", self.serializer)
        self.assertIn("vulkan_1_4_promoted_extension_inventory_status", self.serializer)
        self.assertIn("vulkan_1_4_promoted_extension_coverage_status", self.serializer)

    def test_app_identity_is_r24(self):
        self.assertIn('schema_version\\": 9', self.activity)
        self.assertIn("versionCode = 240", self.gradle)
        self.assertIn('versionName = "2.4.0"', self.gradle)
        self.assertIn("nyamist-g925-r2.4-vulkan-1.4-audit.json", self.activity)


if __name__ == "__main__":
    unittest.main()
