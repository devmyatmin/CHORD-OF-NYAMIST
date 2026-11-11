#include "nyamist/runtime/vulkan_driver_adapter.hpp"

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

void check(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::string sampleReport() {
    return
        "# NYAMIST normalized Android Vulkan driver evidence\n"
        "driver_name=Android system Vulkan\n"
        "driver_info=Example stock Android Vulkan driver\n"
        "api_version=1.3.278\n"
        "gpu_id=0xd8300010\n"
        "architecture=Immortalis-G925\n"
        "model=Immortalis-G925 MC12\n"
        "load_success=true\n"
        "device_count=1\n"
        "probe_pass=0\n"
        "probe_fail=0\n"
        "probe_skip=0\n"
        "gralloc_hwmodule=false\n"
        "conformant=false\n"
        "evidence_source=Android Vulkan driver report\n";
}

nyamist::runtime::ComponentReport inspect(const std::string& source) {
    std::istringstream input(source);
    const auto snapshot = nyamist::runtime::VulkanDriverAdapter::parseReport(input);
    return nyamist::runtime::VulkanDriverAdapter(snapshot).inspect();
}

void testCapabilityProbeEvidenceIsDiscoveredNotSmokeTested() {
    const auto report = inspect(sampleReport());
    using nyamist::runtime::ValidationLevel;

    check(report.descriptor.id == "vulkan-driver", "adapter must use the stable generic Vulkan driver ID");
    check(report.descriptor.kind == nyamist::runtime::ComponentKind::vulkan_driver,
          "driver must be classified as a Vulkan driver");
    check(report.validation == ValidationLevel::discovered,
          "device enumeration and probe counters alone must not imply render/present");
    check(report.capabilities.size() >= 7U, "driver identity and feature evidence must be retained");
    check(!report.diagnostics.empty(), "non-conformant/fallback-gralloc warnings must be retained");
}

void testExplicitRenderPresentEvidencePromotesToSmokeTested() {
    auto source = sampleReport();
    source +=
        "render_present_passed=true\n"
        "render_present_frames=19003\n"
        "test_duration_seconds=20\n"
        "process_exit_code=0\n"
        "device_lost_events=0\n";

    const auto report = inspect(source);
    check(report.validation == nyamist::runtime::ValidationLevel::smoke_tested,
          "successful render/present evidence must promote Vulkan driver to smoke-tested");
}

void testSustainedRequiresFiveMinutesAndCleanExit() {
    auto source = sampleReport();
    source +=
        "render_present_passed=true\n"
        "render_present_frames=200000\n"
        "test_duration_seconds=300\n"
        "process_exit_code=0\n"
        "device_lost_events=0\n"
        "sustained_test_passed=true\n";

    const auto report = inspect(source);
    check(report.validation == nyamist::runtime::ValidationLevel::sustained_tested,
          "a clean 300-second sustained test may promote Vulkan driver");
}

void testShortRunCannotClaimSustained() {
    auto source = sampleReport();
    source +=
        "render_present_passed=true\n"
        "render_present_frames=19003\n"
        "test_duration_seconds=20\n"
        "process_exit_code=0\n"
        "device_lost_events=0\n"
        "sustained_test_passed=true\n";

    const auto report = inspect(source);
    check(report.validation == nyamist::runtime::ValidationLevel::smoke_tested,
          "a short run must never be classified as sustained-tested");
}

void testMissingDeviceLostCounterRejectsRenderEvidence() {
    auto source = sampleReport();
    source +=
        "render_present_passed=true\n"
        "render_present_frames=1000\n"
        "test_duration_seconds=20\n"
        "process_exit_code=0\n";
    const auto report = inspect(source);
    check(report.validation == nyamist::runtime::ValidationLevel::discovered,
          "missing device-lost status must not be treated as zero");
}

void testDeviceLostRejectsRenderEvidence() {
    auto source = sampleReport();
    source +=
        "render_present_passed=true\n"
        "render_present_frames=1000\n"
        "test_duration_seconds=20\n"
        "process_exit_code=0\n"
        "device_lost_events=1\n";

    const auto report = inspect(source);
    check(report.validation == nyamist::runtime::ValidationLevel::discovered,
          "device-lost events must reject smoke-test promotion");
}

void testFailedLoadIsUnavailable() {
    auto source = sampleReport();
    const auto marker = source.find("load_success=true");
    source.replace(marker, std::string("load_success=true").size(), "load_success=false");

    const auto report = inspect(source);
    check(report.validation == nyamist::runtime::ValidationLevel::unavailable,
          "failed load must be unavailable");
}

void testGenericVendorDriverIdentityAccepted() {
    auto source = sampleReport();
    const auto marker = source.find("driver_name=Android system Vulkan");
    source.replace(marker, std::string("driver_name=Android system Vulkan").size(),
                   "driver_name=ARM proprietary Vulkan");
    const auto report = inspect(source);
    check(report.validation == nyamist::runtime::ValidationLevel::discovered,
          "the generic adapter must accept a vendor-provided driver identity");
}

void testDuplicateKeyRejected() {
    auto source = sampleReport() + "device_count=2\n";
    bool threw = false;
    try {
        (void)inspect(source);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "duplicate report keys must be rejected");
}

} // namespace

int main() {
    try {
        testCapabilityProbeEvidenceIsDiscoveredNotSmokeTested();
        testExplicitRenderPresentEvidencePromotesToSmokeTested();
        testSustainedRequiresFiveMinutesAndCleanExit();
        testShortRunCannotClaimSustained();
        testDeviceLostRejectsRenderEvidence();
        testMissingDeviceLostCounterRejectsRenderEvidence();
        testFailedLoadIsUnavailable();
        testGenericVendorDriverIdentityAccepted();
        testDuplicateKeyRejected();
        std::cout << "All NYAMIST Vulkan driver adapter tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Vulkan driver adapter test failure: " << error.what() << '\n';
        return 1;
    }
}
