import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
NATIVE = ROOT / "android/surface-validation/app/src/main/cpp/native_surface_swapchain.cpp"
HEADER = ROOT / "include/nyamist/discovery/android_surface_swapchain_validation.hpp"
SERIALIZER = ROOT / "src/discovery/android_surface_swapchain_validation.cpp"
ACTIVITY = ROOT / "android/surface-validation/app/src/main/java/dev/nyamist/g925surface/MainActivity.java"
GRADLE = ROOT / "android/surface-validation/app/build.gradle.kts"


class R23Vulkan14DriverAuditContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.native = NATIVE.read_text(encoding="utf-8")
        cls.header = HEADER.read_text(encoding="utf-8")
        cls.serializer = SERIALIZER.read_text(encoding="utf-8")
        cls.activity = ACTIVITY.read_text(encoding="utf-8")
        cls.gradle = GRADLE.read_text(encoding="utf-8")

    def test_schema_v8_and_r23_report_identity(self):
        self.assertIn("schema_version{8U}", self.header)
        self.assertIn("NYAMIST G925 R2.3", self.serializer)
        self.assertIn('schema_version\\": 8', self.activity)

    def test_instance_api_negotiates_up_to_loader_cap(self):
        self.assertIn("versionAtLeast(loader_api_version, 1U, 4U)", self.native)
        self.assertIn("VK_MAKE_VERSION(1, 4, 0)", self.native)
        self.assertIn("report.instance_api_version", self.native)

    def test_driver_identity_uses_public_vulkan_properties(self):
        self.assertIn("vkGetPhysicalDeviceProperties2", self.native)
        self.assertIn("VkPhysicalDeviceDriverProperties", self.native)
        self.assertIn("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES", self.native)
        self.assertIn("driver_properties.driverID", self.native)
        self.assertIn("driver_properties.driverName", self.native)
        self.assertIn("driver_properties.driverInfo", self.native)
        self.assertIn("properties.driverVersion", self.native)
        self.assertIn("driver_properties.conformanceVersion", self.native)
        self.assertIn('"physical_device_uuid"', self.serializer)

    def test_device_api_is_not_inferred_from_loader_api(self):
        self.assertIn("versionAtLeast(properties.apiVersion, 1U, 4U)", self.native)
        self.assertIn("not_exposed_by_physical_device", self.native)
        self.assertIn('"physical_device_api_version"', self.serializer)

    def test_vulkan14_features_are_queried_only_on_14_devices(self):
        self.assertIn("VkPhysicalDeviceVulkan14Features", self.native)
        self.assertIn("features14.bresenhamLines", self.native)
        self.assertIn("features14.shaderSubgroupRotateClustered", self.native)
        self.assertIn("features14.pushDescriptor", self.native)
        self.assertIn("features14.pipelineProtectedAccess", self.native)
        self.assertIn("VkPhysicalDeviceVulkan11Features", self.native)
        self.assertIn("features11.protectedMemory", self.native)
        self.assertIn("skipped_physical_device_api_below_1_4", self.native)

    def test_app_version_and_report_path_are_r23(self):
        self.assertIn("versionCode = 230", self.gradle)
        self.assertIn('versionName = "2.3.0"', self.gradle)
        self.assertIn("nyamist-g925-r2.3-vulkan-1.4-audit.json", self.activity)

    def test_android_build_uses_pinned_official_vulkan_headers(self):
        cmake = (ROOT / "android/surface-validation/app/src/main/cpp/CMakeLists.txt").read_text(encoding="utf-8")
        workflow = (ROOT / ".github/workflows/r2.3-vulkan-1.4-driver-audit.yml").read_text(encoding="utf-8")
        self.assertIn("NYAMIST_VULKAN_HEADERS_ROOT", cmake)
        self.assertIn("v1.4.368", workflow)
        self.assertIn("VK_HEADER_VERSION 368", workflow)


if __name__ == "__main__":
    unittest.main()
