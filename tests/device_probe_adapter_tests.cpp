#include "nyamist/runtime/device_probe_adapter.hpp"

#include <algorithm>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

void check(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::string sampleReport() {
    return
        "# Synthetic CI evidence only; not a measurement from a real device\n"
        "schema_version=1\n"
        "captured_at=2026-11-08T00:00:00Z\n"
        "evidence_source=synthetic CI fixture\n"
        "device.manufacturer=Example\n"
        "device.brand=Example\n"
        "device.model=Example Android Device\n"
        "device.soc_model=Example SoC\n"
        "device.android_release=15\n"
        "device.android_api=35\n"
        "device.kernel_release=6.6.0-example\n"
        "device.primary_abi=arm64-v8a\n"
        "device.abi_list=arm64-v8a,armeabi-v7a\n"
        "device.logical_cpu_count=8\n"
        "device.total_memory_bytes=8589934592\n"
        "device.available_memory_bytes=4294967296\n"
        "vulkan.enumeration_status=success\n"
        "vulkan.device_count=1\n"
        "vulkan.device_name=Example Vulkan GPU\n"
        "vulkan.vendor_id=0x13b5\n"
        "vulkan.device_id=0xd8300015\n"
        "vulkan.instance_api_version=1.3.280\n"
        "vulkan.device_api_version=1.3.280\n"
        "vulkan.driver_name=example-vulkan\n"
        "vulkan.driver_info=synthetic driver report\n"
        "vulkan.driver_version=1.0\n"
        "vulkan.extensions_reported=true\n"
        "vulkan.supported_extensions=VK_KHR_timeline_semaphore,VK_KHR_synchronization2,VK_KHR_dynamic_rendering\n"
        "vulkan.feature.timelineSemaphore=true\n"
        "vulkan.feature.dynamicRendering=true\n"
        "vulkan.feature.shaderInt64=false\n";
}

nyamist::runtime::DeviceProbeSnapshot parse(const std::string& source) {
    std::istringstream input(source);
    return nyamist::runtime::DeviceProbeReportParser::parse(input);
}

bool hasCapability(const nyamist::runtime::ComponentReport& report, const std::string& value) {
    return std::find(report.capabilities.begin(), report.capabilities.end(), value) !=
           report.capabilities.end();
}

void testDeviceFactsAreImportedWithoutInference() {
    using namespace nyamist::runtime;
    const auto snapshot = parse(sampleReport());
    const auto report = DeviceFactsAdapter(snapshot).inspect();

    check(report.descriptor.id == "device-facts", "device facts must have a stable component ID");
    check(report.descriptor.kind == ComponentKind::monitor,
          "device facts must not masquerade as a Vulkan driver");
    check(report.validation == ValidationLevel::discovered,
          "valid device facts should be marked discovered");
    check(hasCapability(report, "device.model=Example Android Device"),
          "the reported model must be preserved");
    check(hasCapability(report, "android.api=35"), "Android API level must be retained");
    check(hasCapability(report, "memory.total_bytes=8589934592"),
          "reported memory counters must be retained");
}

void testVulkanCapabilitiesAreDiscoveredButNotRenderValidated() {
    using namespace nyamist::runtime;
    const auto snapshot = parse(sampleReport());
    const auto report = VulkanCapabilityAdapter(snapshot).inspect();

    check(report.descriptor.id == "vulkan-capabilities",
          "Vulkan capability report must have a stable ID");
    check(report.descriptor.kind == ComponentKind::monitor,
          "capability evidence must not be selected as an active Vulkan driver");
    check(report.validation == ValidationLevel::discovered,
          "physical-device enumeration must only be classified as discovered");
    check(hasCapability(report, "vulkan.vendor_id=5045"),
          "hexadecimal vendor IDs must be parsed and retained numerically");
    check(hasCapability(report, "vulkan.extension=VK_KHR_timeline_semaphore"),
          "supported extensions must be preserved");
    check(hasCapability(report, "vulkan.feature.dynamicRendering=true"),
          "queried feature support must be preserved");
    check(hasCapability(report, "vulkan.feature.shaderInt64=false"),
          "unsupported feature bits must not be dropped");
    check(!report.diagnostics.empty(),
          "enumeration must carry an explicit non-render-validation diagnostic");
}

void testUnavailableVulkanDoesNotInvalidateDeviceFacts() {
    using namespace nyamist::runtime;
    auto source = sampleReport();
    const auto marker = source.find("vulkan.enumeration_status=success\n");
    source.erase(marker, std::string("vulkan.enumeration_status=success\n").size());
    const auto device_marker = source.find("vulkan.device_count=1\n");
    const auto feature_marker = source.find("vulkan.feature.timelineSemaphore=true\n");
    source.erase(device_marker, feature_marker - device_marker);
    source.erase(source.find("vulkan.feature.timelineSemaphore=true\n"), source.size());
    source += "vulkan.enumeration_status=unavailable\n"
              "vulkan.device_count=0\n"
              "vulkan.failure_reason=No Vulkan loader visible\n";

    const auto snapshot = parse(source);
    check(DeviceFactsAdapter(snapshot).inspect().validation == ValidationLevel::discovered,
          "device facts must remain available when Vulkan enumeration is unavailable");
    const auto report = VulkanCapabilityAdapter(snapshot).inspect();
    check(report.validation == ValidationLevel::unavailable,
          "an unavailable Vulkan loader must not be marked discovered");
    check(!report.diagnostics.empty(), "the Vulkan failure reason must remain visible");
}

void testMissingRequiredFieldRejected() {
    auto source = sampleReport();
    const auto marker = source.find("device.model=Example Android Device\n");
    source.erase(marker, std::string("device.model=Example Android Device\n").size());
    bool threw = false;
    try {
        (void)parse(source);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "missing required device facts must be rejected");
}

void testUnsupportedSchemaRejected() {
    auto source = sampleReport();
    source.replace(source.find("schema_version=1"), std::string("schema_version=1").size(),
                   "schema_version=2");
    bool threw = false;
    try {
        (void)parse(source);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "unknown report schema versions must fail closed");
}

void testUnknownAndDuplicateKeysRejected() {
    bool unknown_threw = false;
    try {
        (void)parse(sampleReport() + "device.unknwon=value\n");
    } catch (const std::invalid_argument&) {
        unknown_threw = true;
    }
    check(unknown_threw, "unknown keys must reject schema typos");

    bool duplicate_threw = false;
    try {
        (void)parse(sampleReport() + "device.model=spoofed\n");
    } catch (const std::invalid_argument&) {
        duplicate_threw = true;
    }
    check(duplicate_threw, "duplicate keys must be rejected");
}

void testMalformedFeatureAndDuplicateExtensionsRejected() {
    bool invalid_feature_threw = false;
    try {
        (void)parse(sampleReport() + "vulkan.feature.bad name=true\n");
    } catch (const std::invalid_argument&) {
        invalid_feature_threw = true;
    }
    check(invalid_feature_threw, "feature keys outside the schema identifier grammar must be rejected");

    auto source = sampleReport();
    const auto marker = source.find("VK_KHR_synchronization2");
    source.insert(marker, "VK_KHR_timeline_semaphore,");
    bool duplicate_extension_threw = false;
    try {
        (void)parse(source);
    } catch (const std::invalid_argument&) {
        duplicate_extension_threw = true;
    }
    check(duplicate_extension_threw, "duplicate Vulkan extension names must be rejected");
}

void testMemoryCountersMustBeConsistent() {
    auto source = sampleReport();
    source.replace(source.find("device.available_memory_bytes=4294967296"),
                   std::string("device.available_memory_bytes=4294967296").size(),
                   "device.available_memory_bytes=99999999999");
    bool threw = false;
    try {
        (void)parse(source);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "available memory greater than total memory must be rejected");
}

void testFeatureBooleanMustBeTyped() {
    auto source = sampleReport();
    source.replace(source.find("vulkan.feature.dynamicRendering=true"),
                   std::string("vulkan.feature.dynamicRendering=true").size(),
                   "vulkan.feature.dynamicRendering=maybe");
    bool threw = false;
    try {
        (void)parse(source);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "feature values must be strict booleans");
}

} // namespace

int main() {
    try {
        testDeviceFactsAreImportedWithoutInference();
        testVulkanCapabilitiesAreDiscoveredButNotRenderValidated();
        testUnavailableVulkanDoesNotInvalidateDeviceFacts();
        testMissingRequiredFieldRejected();
        testUnsupportedSchemaRejected();
        testUnknownAndDuplicateKeysRejected();
        testMalformedFeatureAndDuplicateExtensionsRejected();
        testMemoryCountersMustBeConsistent();
        testFeatureBooleanMustBeTyped();
        std::cout << "All NYAMIST device probe adapter tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Device probe adapter test failure: " << error.what() << '\n';
        return 1;
    }
}
