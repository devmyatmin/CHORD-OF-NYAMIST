#include "nyamist/runtime/translator_adapters.hpp"

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {
void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::string dxvkReport() {
    return
        "# normalized DXVK report\n"
        "translator_id=dxvk\n"
        "api=d3d11\n"
        "translator_version=3.1.1-5-arm64ec\n"
        "device_created=true\n"
        "max_feature_level=11_1\n"
        "selected_feature_level=11_1\n"
        "supported_feature_levels=10_0,10_1,11_0,11_1\n"
        "shader_models=5_0\n"
        "evidence_source=PanPlay D3D11 ARM64EC report\n";
}

std::string vkd3dReport() {
    return
        "# normalized VKD3D-Proton report\n"
        "translator_id=vkd3d-proton\n"
        "api=d3d12\n"
        "translator_version=2.14-arm64ec\n"
        "device_created=true\n"
        "max_feature_level=11_1\n"
        "selected_feature_level=11_1\n"
        "supported_feature_levels=11_0,11_1\n"
        "shader_models=6_6,6_7,6_8\n"
        "evidence_source=PanPlay D3D12 ARM64EC report\n";
}

nyamist::runtime::TranslatorProbeSnapshot parse(const std::string& source) {
    std::istringstream input(source);
    return nyamist::runtime::TranslatorAdapterBase::parseReport(input);
}

void testDxvkIdentityAndD3D11Capabilities() {
    using namespace nyamist::runtime;
    const auto report = DxvkAdapter(parse(dxvkReport())).inspect();
    check(report.descriptor.id == "dxvk", "DXVK stable component ID mismatch");
    check(report.descriptor.kind == ComponentKind::translator, "DXVK must be a translator");
    check(report.validation == ValidationLevel::discovered,
          "device creation alone must not imply render/present validation");
    bool has_level = false;
    bool has_shader_model = false;
    for (const auto& item : report.capabilities) {
        has_level = has_level || item == "d3d.max_feature_level=11_1";
        has_shader_model = has_shader_model || item == "d3d.shader_models=5_0";
    }
    check(has_level && has_shader_model, "DXVK capability evidence was not preserved");
}

void testVkd3dSeparatesShaderModelsFromFeatureLevels() {
    using namespace nyamist::runtime;
    const auto report = Vkd3dProtonAdapter(parse(vkd3dReport())).inspect();
    bool has_level = false;
    bool has_shader_models = false;
    for (const auto& item : report.capabilities) {
        has_level = has_level || item == "d3d.max_feature_level=11_1";
        has_shader_models = has_shader_models || item == "d3d.shader_models=6_6,6_7,6_8";
    }
    check(report.descriptor.id == "vkd3d-proton" && has_level && has_shader_models,
          "VKD3D-Proton evidence must keep FL and shader-model values separate");
}

void testCleanRenderPresentPromotesToSmokeTested() {
    using namespace nyamist::runtime;
    auto source = dxvkReport() +
        "render_present_passed=true\nframe_count=27508\nduration_seconds=20\n"
        "present_calls=27508\npresent_failures=0\nprocess_exit_code=0\n"
        "device_lost_events=0\n";
    const auto report = DxvkAdapter(parse(source)).inspect();
    check(report.validation == ValidationLevel::smoke_tested,
          "clean explicit D3D11 render/present report should pass smoke validation");
}

void testCleanFiveMinuteRunPromotesToSustained() {
    using namespace nyamist::runtime;
    auto source = vkd3dReport() +
        "render_present_passed=true\nframe_count=180000\nduration_seconds=300\n"
        "present_calls=180000\npresent_failures=0\nprocess_exit_code=0\n"
        "device_lost_events=0\nsustained_test_passed=true\n";
    const auto report = Vkd3dProtonAdapter(parse(source)).inspect();
    check(report.validation == ValidationLevel::sustained_tested,
          "clean 300-second D3D12 run should pass sustained validation");
}

void testShortRunCannotClaimSustained() {
    using namespace nyamist::runtime;
    auto source = dxvkReport() +
        "render_present_passed=true\nframe_count=1000\nduration_seconds=20\n"
        "present_calls=1000\npresent_failures=0\nprocess_exit_code=0\n"
        "device_lost_events=0\nsustained_test_passed=true\n";
    const auto report = DxvkAdapter(parse(source)).inspect();
    check(report.validation == ValidationLevel::smoke_tested,
          "a run below 300 seconds must not be sustained-tested");
}

void testPartialRenderMetricsRemainVisibleWithoutPromotion() {
    using namespace nyamist::runtime;
    auto source = dxvkReport() +
        "frame_count=27508\nduration_seconds=20\nprocess_exit_code=0\n";
    const auto report = DxvkAdapter(parse(source)).inspect();
    check(report.validation == ValidationLevel::discovered,
          "partial render data must not promote a translator to smoke-tested");
    bool has_frames = false;
    bool has_duration = false;
    for (const auto& item : report.capabilities) {
        has_frames = has_frames || item == "render.frames.observed=27508";
        has_duration = has_duration || item == "render.duration_seconds.observed=20";
    }
    check(has_frames && has_duration, "partial frame/time evidence must remain inspectable");
}

void testPresentFailureRejectsSmokeValidation() {
    using namespace nyamist::runtime;
    auto source = dxvkReport() +
        "render_present_passed=true\nframe_count=1000\nduration_seconds=20\n"
        "present_calls=1000\npresent_failures=1\nprocess_exit_code=0\n"
        "device_lost_events=0\n";
    const auto report = DxvkAdapter(parse(source)).inspect();
    check(report.validation == ValidationLevel::discovered && !report.diagnostics.empty(),
          "Present failures must reject smoke validation and create a diagnostic");
}

void testDeviceLostRejectsSmokeValidation() {
    using namespace nyamist::runtime;
    auto source = vkd3dReport() +
        "render_present_passed=true\nframe_count=1000\nduration_seconds=20\n"
        "present_calls=1000\npresent_failures=0\nprocess_exit_code=0\n"
        "device_lost_events=1\n";
    const auto report = Vkd3dProtonAdapter(parse(source)).inspect();
    check(report.validation == ValidationLevel::discovered,
          "device-lost events must reject smoke validation");
}

void testNonZeroExitRejectsSmokeValidation() {
    using namespace nyamist::runtime;
    auto source = dxvkReport() +
        "render_present_passed=true\nframe_count=1000\nduration_seconds=20\n"
        "present_calls=1000\npresent_failures=0\nprocess_exit_code=1\n"
        "device_lost_events=0\n";
    const auto report = DxvkAdapter(parse(source)).inspect();
    check(report.validation == ValidationLevel::discovered,
          "non-zero process exit must reject smoke validation");
}

void testMismatchedAdapterIdentityRejected() {
    using namespace nyamist::runtime;
    bool threw = false;
    try { (void)DxvkAdapter(parse(vkd3dReport())); }
    catch (const std::invalid_argument&) { threw = true; }
    check(threw, "DXVK adapter must reject VKD3D-Proton evidence");
}

void testDuplicateKeyRejected() {
    bool threw = false;
    try { (void)parse(dxvkReport() + "device_created=false\n"); }
    catch (const std::invalid_argument&) { threw = true; }
    check(threw, "duplicate report fields must be rejected");
}
} // namespace

int main() {
    try {
        testDxvkIdentityAndD3D11Capabilities();
        testVkd3dSeparatesShaderModelsFromFeatureLevels();
        testCleanRenderPresentPromotesToSmokeTested();
        testCleanFiveMinuteRunPromotesToSustained();
        testShortRunCannotClaimSustained();
        testPartialRenderMetricsRemainVisibleWithoutPromotion();
        testPresentFailureRejectsSmokeValidation();
        testDeviceLostRejectsSmokeValidation();
        testNonZeroExitRejectsSmokeValidation();
        testMismatchedAdapterIdentityRejected();
        testDuplicateKeyRejected();
        std::cout << "All NYAMIST translator adapter tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }
}
