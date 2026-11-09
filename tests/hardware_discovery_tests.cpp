#include "nyamist/discovery/hardware_discovery.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void testJsonEscapingAndProvenance() {
    nyamist::discovery::DiscoveryReport report;
    report.captured_at_utc = "2026-11-09T00:00:00Z";
    report.platform = "android";
    report.device_fields.push_back({
        "model", {"REDMI \"K80\" Ultra", "Android property: ro.product.model", "direct"}
    });
    report.vulkan_compiled = true;
    report.vulkan_status = "unavailable";
    report.vulkan_loader_library = "libvulkan.so";
    report.vulkan_instance_extensions = {
        "VK_KHR_surface", "VK_EXT_debug_utils"
    };
    report.vulkan_failure_reason = "line1\nline2";

    nyamist::discovery::VulkanDeviceObservation device;
    device.name = "Mali-G925-Immortalis MC12";
    device.extension_query_status = "success";
    device.feature_query_status = "success";
    device.feature_query_api_max = "1.3";
    device.features.push_back({"dynamicRendering", "1.3", true});
    device.limits_query_status = "success";
    device.limits.push_back({"maxImageDimension2D", 16384ULL});
    device.floating_limits.push_back({"timestampPeriod_ns", 1.0});
    device.queue_families.push_back({0U, 2U, 7U, {"graphics", "compute", "transfer"}});
    device.memory_types.push_back({0U, 0U, 3U, {"device_local", "host_visible"}});
    device.formats.push_back({
        "R8G8B8A8_UNORM", 1U, 2U, 4U,
        {"sampled_image"}, {"storage_image"}, {"vertex_buffer"}
    });
    report.vulkan_devices.push_back(std::move(device));

    const std::string json = nyamist::discovery::toJson(report);
    check(json.find("\"schema_version\": 2") != std::string::npos,
          "JSON must contain the schema version");
    check(json.find(R"(REDMI \"K80\" Ultra)") != std::string::npos,
          "JSON string values must be escaped");
    check(json.find("\"source\": \"Android property: ro.product.model\"") != std::string::npos,
          "observations must retain their source");
    check(json.find("line1\\nline2") != std::string::npos,
          "control characters must be escaped");
    check(json.find("\"enumeration_status\": \"unavailable\"") != std::string::npos,
          "Vulkan status must be explicit");
    check(json.find("\"loader_library\": \"libvulkan.so\"") != std::string::npos,
          "the selected Vulkan loader library must be recorded");
    check(json.find("\"instance_extensions\": [\"VK_KHR_surface\", \"VK_EXT_debug_utils\"]")
              != std::string::npos,
          "instance extensions must be serialized as evidence");
    check(json.find("\"limits_query_status\": \"success\"") != std::string::npos,
          "physical-device limits query status must be serialized");
    check(json.find("\"feature_query_status\": \"success\"") != std::string::npos &&
          json.find("\"name\": \"dynamicRendering\", \"api_version\": \"1.3\", \"supported\": true")
              != std::string::npos,
          "core feature support and API provenance must be serialized");
    check(json.find("\"maxImageDimension2D\", \"value\": 16384") != std::string::npos,
          "physical-device limits must be serialized as numeric values");
    check(json.find("\"property_flag_names\": [\"device_local\", \"host_visible\"]")
              != std::string::npos,
          "memory type flags must have symbolic names");
    check(json.find("\"flag_names\": [\"graphics\", \"compute\", \"transfer\"]")
              != std::string::npos,
          "queue flags must retain raw masks and expose symbolic names");
    check(json.find("\"optimal_tiling_feature_names\": [\"storage_image\"]")
              != std::string::npos,
          "format feature masks must expose symbolic names");
}

void testLiveCollectionHasExplicitVulkanState() {
    const auto report = nyamist::discovery::collectHardwareDiscoveryReport();
    check(report.schema_version == 2U, "discovery schema must be version 2");
    check(!report.captured_at_utc.empty(), "capture timestamp must be present");
    check(!report.platform.empty(), "platform must be identified");
    check(report.vulkan_status == "success" ||
          report.vulkan_status == "failed" ||
          report.vulkan_status == "unavailable",
          "Vulkan result must be explicit and bounded");
    const std::string json = nyamist::discovery::toJson(report);
    check(json.find("\"physical_devices\"") != std::string::npos,
          "JSON must contain physical_devices when enumeration is unavailable");
    for (const auto& device : report.vulkan_devices) {
        check(device.extension_query_status == "success" ||
              device.extension_query_status == "failed",
              "device extension query state must be explicit");
        check(device.feature_query_status == "success" ||
              device.feature_query_status == "partial" ||
              device.feature_query_status == "failed",
              "feature query state must be explicit");
        check(device.limits_query_status == "success" ||
              device.limits_query_status == "unavailable",
              "limits query state must be explicit");
    }
}

} // namespace

int main() {
    try {
        testJsonEscapingAndProvenance();
        testLiveCollectionHasExplicitVulkanState();
        std::cout << "All NYAMIST hardware-discovery tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Hardware-discovery test failure: " << error.what() << '\n';
        return 1;
    }
}
