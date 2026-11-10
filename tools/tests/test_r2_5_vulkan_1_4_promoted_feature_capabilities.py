import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
NATIVE = ROOT / "android/surface-validation/app/src/main/cpp/native_surface_swapchain.cpp"
HEADER = ROOT / "include/nyamist/discovery/android_surface_swapchain_validation.hpp"
SERIALIZER = ROOT / "src/discovery/android_surface_swapchain_validation.cpp"
ACTIVITY = ROOT / "android/surface-validation/app/src/main/java/dev/nyamist/g925surface/MainActivity.java"
GRADLE = ROOT / "android/surface-validation/app/build.gradle.kts"

FEATURE_STRUCTS = (
    "VkPhysicalDeviceDynamicRenderingLocalReadFeaturesKHR",
    "VkPhysicalDeviceGlobalPriorityQueryFeaturesKHR",
    "VkPhysicalDeviceIndexTypeUint8FeaturesKHR",
    "VkPhysicalDeviceLineRasterizationFeaturesKHR",
    "VkPhysicalDeviceMaintenance5FeaturesKHR",
    "VkPhysicalDeviceMaintenance6FeaturesKHR",
    "VkPhysicalDeviceShaderExpectAssumeFeaturesKHR",
    "VkPhysicalDeviceShaderFloatControls2FeaturesKHR",
    "VkPhysicalDeviceShaderSubgroupRotateFeaturesKHR",
    "VkPhysicalDeviceVertexAttributeDivisorFeaturesKHR",
    "VkPhysicalDeviceHostImageCopyFeaturesEXT",
    "VkPhysicalDevicePipelineProtectedAccessFeaturesEXT",
    "VkPhysicalDevicePipelineRobustnessFeaturesEXT",
)

FEATURE_BITS = (
    "globalPriorityQuery",
    "shaderSubgroupRotate",
    "shaderSubgroupRotateClustered",
    "shaderFloatControls2",
    "shaderExpectAssume",
    "rectangularLines",
    "bresenhamLines",
    "smoothLines",
    "stippledRectangularLines",
    "stippledBresenhamLines",
    "stippledSmoothLines",
    "vertexAttributeInstanceRateDivisor",
    "vertexAttributeInstanceRateZeroDivisor",
    "indexTypeUint8",
    "dynamicRenderingLocalRead",
    "maintenance5",
    "maintenance6",
    "pipelineProtectedAccess",
    "pipelineRobustness",
    "hostImageCopy",
    "pushDescriptor",
)


class R25PromotedFeatureCapabilityContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.native = NATIVE.read_text(encoding="utf-8")
        cls.header = HEADER.read_text(encoding="utf-8")
        cls.serializer = SERIALIZER.read_text(encoding="utf-8")
        cls.activity = ACTIVITY.read_text(encoding="utf-8")
        cls.gradle = GRADLE.read_text(encoding="utf-8")

    def test_report_schema_tracks_r25_and_all_21_feature_bits(self):
        self.assertIn("schema_version{10U}", self.header)
        self.assertIn("vulkan_1_4_promoted_feature_total_count{21U}", self.header)
        self.assertIn('"vulkan_1_4_promoted_feature_total_count"', self.serializer)
        for field in (
            "vulkan_1_4_promoted_features_supported",
            "vulkan_1_4_promoted_features_not_supported",
            "vulkan_1_4_promoted_features_not_advertised",
            "vulkan_1_4_promoted_features_implicitly_supported",
            "vulkan_1_4_promoted_features_query_failed",
            "vulkan_1_4_promoted_features_not_queryable",
            "vulkan_1_4_promoted_features_not_applicable",
        ):
            self.assertIn(field, self.header)
            self.assertIn(field, self.serializer)

    def test_pre_14_path_only_chains_advertised_extension_feature_structs(self):
        for structure in FEATURE_STRUCTS:
            self.assertIn(structure, self.native)
        self.assertIn("append_if_advertised", self.native)
        self.assertIn("hasDeviceExtension(selected_physical_device_extensions", self.native)
        self.assertIn("advertised_extension_feature_structs_on_pre_1_4_device", self.native)
        self.assertIn("A pre-1.4 physical device must never receive the Vulkan 1.4", self.native)

    def test_core_14_aggregate_is_guarded_by_physical_and_instance_api_versions(self):
        self.assertIn("versionAtLeast(properties.apiVersion, 1U, 4U)", self.native)
        self.assertIn("versionAtLeast(instance_api_version, 1U, 4U)", self.native)
        self.assertIn("VkPhysicalDeviceVulkan14Features features14", self.native)
        self.assertIn("report.vulkan_1_4_promoted_feature_query_status =", self.native)
        self.assertIn('"skipped_instance_api_below_1_4"', self.native)

    def test_all_21_core_feature_members_are_accounted_for(self):
        for feature in FEATURE_BITS:
            self.assertIn(feature, self.native)
        self.assertEqual(len(FEATURE_BITS), 21)

    def test_non_feature_promotions_and_pre14_push_descriptor_are_not_false_defaults(self):
        self.assertIn("VK_KHR_load_store_op_none (no feature bit)", self.native)
        self.assertIn("VK_KHR_map_memory2 (no feature bit)", self.native)
        self.assertIn("VK_KHR_push_descriptor.pushDescriptor", self.native)
        self.assertIn("implicit via VK_KHR_push_descriptor", self.native)
        self.assertIn("vulkan_1_4_promoted_feature_implicitly_supported_count", self.native)
        self.assertIn("vulkan_1_4_promoted_feature_query_failed_count", self.native)
        self.assertIn("vulkan_1_4_promoted_features_not_queryable", self.serializer)

    def test_app_identity_is_r25(self):
        self.assertIn('schema_version\\": 10', self.activity)
        self.assertIn("nyamist-g925-r2.5-vulkan-1.4-audit.json", self.activity)
        self.assertIn("versionCode = 250", self.gradle)
        self.assertIn('versionName = "2.5.0"', self.gradle)


if __name__ == "__main__":
    unittest.main()
