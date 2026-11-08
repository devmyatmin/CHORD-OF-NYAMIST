#include "nyamist/runtime/runtime_coordinator.hpp"

namespace nyamist::runtime {
namespace {

bool isAtLeastSmokeTested(ValidationLevel level) {
    return level == ValidationLevel::smoke_tested ||
           level == ValidationLevel::sustained_tested;
}

bool isSustained(ValidationLevel level) {
    return level == ValidationLevel::sustained_tested;
}

} // namespace

RuntimeCoordinator::RuntimeCoordinator(const ComponentRegistry& registry)
    : registry_(registry) {}

RuntimePlan RuntimeCoordinator::evaluate(const RuntimeRequest& request) const {
    RuntimePlan plan;
    const std::string translator_id =
        request.api == GraphicsApi::d3d11 ? "dxvk" : "vkd3d-proton";

    const auto translator = registry_.inspect(translator_id);
    const auto driver = registry_.inspect(request.vulkan_driver_id);

    if (!translator || translator->validation == ValidationLevel::unavailable) {
        plan.blockers.push_back("Required translator is unavailable: " + translator_id);
    } else {
        plan.selected_components.push_back(translator_id);
        if (!isAtLeastSmokeTested(translator->validation)) {
            plan.warnings.push_back(
                "Translator is discovered but has not passed a smoke test: " + translator_id);
        }
    }

    if (!driver || driver->validation == ValidationLevel::unavailable) {
        plan.blockers.push_back(
            "Required Vulkan driver is unavailable: " + request.vulkan_driver_id);
    } else {
        plan.selected_components.push_back(request.vulkan_driver_id);
        if (!isAtLeastSmokeTested(driver->validation)) {
            plan.warnings.push_back(
                "Vulkan driver is discovered but has not passed a smoke test: " +
                request.vulkan_driver_id);
        }
    }

    plan.prerequisites_present = plan.blockers.empty();
    plan.smoke_test_allowed = plan.prerequisites_present;
    plan.production_ready =
        plan.prerequisites_present &&
        isSustained(translator->validation) &&
        isSustained(driver->validation);

    if (plan.prerequisites_present && !plan.production_ready) {
        plan.warnings.push_back(
            "Components are available for controlled testing; sustained stability is not proven.");
    }

    return plan;
}

} // namespace nyamist::runtime
