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

    const std::string json = nyamist::discovery::toJson(report);
    check(json.find("\"schema_version\": 1") != std::string::npos,
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
}

void testLiveCollectionHasExplicitVulkanState() {
    const auto report = nyamist::discovery::collectHardwareDiscoveryReport();
    check(report.schema_version == 1U, "discovery schema must be version 1");
    check(!report.captured_at_utc.empty(), "capture timestamp must be present");
    check(!report.platform.empty(), "platform must be identified");
    check(report.vulkan_status == "success" ||
          report.vulkan_status == "failed" ||
          report.vulkan_status == "unavailable",
          "Vulkan result must be explicit and bounded");
    const std::string json = nyamist::discovery::toJson(report);
    check(json.find("\"physical_devices\"") != std::string::npos,
          "JSON must contain physical_devices when enumeration is unavailable");
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
