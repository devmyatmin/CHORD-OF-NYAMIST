#include "nyamist/runtime/component_registry.hpp"
#include "nyamist/runtime/runtime_coordinator.hpp"

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

void check(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

class StaticProbe final : public nyamist::runtime::IComponentProbe {
public:
    explicit StaticProbe(nyamist::runtime::ComponentReport report)
        : report_(std::move(report)) {}

    [[nodiscard]] nyamist::runtime::ComponentReport inspect() const override {
        return report_;
    }

private:
    nyamist::runtime::ComponentReport report_;
};

std::shared_ptr<const StaticProbe> probe(
    std::string id,
    nyamist::runtime::ComponentKind kind,
    nyamist::runtime::ValidationLevel validation) {
    nyamist::runtime::ComponentReport report;
    report.descriptor.id = std::move(id);
    report.descriptor.kind = kind;
    report.descriptor.version = "test";
    report.validation = validation;
    return std::make_shared<StaticProbe>(std::move(report));
}

void testRegistryAndD3D11Plan() {
    using namespace nyamist::runtime;
    ComponentRegistry registry;
    registry.add(probe("dxvk", ComponentKind::translator, ValidationLevel::smoke_tested));
    registry.add(probe("panvk", ComponentKind::vulkan_driver, ValidationLevel::smoke_tested));

    RuntimeCoordinator coordinator(registry);
    const auto plan = coordinator.evaluate({GraphicsApi::d3d11, "panvk"});

    check(plan.prerequisites_present, "D3D11 prerequisites should be present");
    check(plan.smoke_test_allowed, "smoke test should be allowed");
    check(!plan.production_ready, "smoke-tested components are not sustained validation");
    check(plan.selected_components.size() == 2, "D3D11 plan should select DXVK and PanVK");
}

void testD3D12RequiresVkd3d() {
    using namespace nyamist::runtime;
    ComponentRegistry registry;
    registry.add(probe("dxvk", ComponentKind::translator, ValidationLevel::smoke_tested));
    registry.add(probe("panvk", ComponentKind::vulkan_driver, ValidationLevel::smoke_tested));

    RuntimeCoordinator coordinator(registry);
    const auto plan = coordinator.evaluate({GraphicsApi::d3d12, "panvk"});

    check(!plan.prerequisites_present, "D3D12 must not silently reuse DXVK");
    check(!plan.smoke_test_allowed, "missing VKD3D-Proton must block D3D12 smoke test");
}

void testProductionRequiresSustainedValidation() {
    using namespace nyamist::runtime;
    ComponentRegistry registry;
    registry.add(probe("dxvk", ComponentKind::translator, ValidationLevel::sustained_tested));
    registry.add(probe("panvk", ComponentKind::vulkan_driver, ValidationLevel::sustained_tested));

    RuntimeCoordinator coordinator(registry);
    const auto plan = coordinator.evaluate({GraphicsApi::d3d11, "panvk"});
    check(plan.production_ready, "sustained-tested components should pass readiness gate");
}

void testComponentKindMismatchRejected() {
    using namespace nyamist::runtime;
    ComponentRegistry registry;
    registry.add(probe("dxvk", ComponentKind::vulkan_driver, ValidationLevel::sustained_tested));
    registry.add(probe("panvk", ComponentKind::vulkan_driver, ValidationLevel::sustained_tested));

    RuntimeCoordinator coordinator(registry);
    const auto plan = coordinator.evaluate({GraphicsApi::d3d11, "panvk"});
    check(!plan.prerequisites_present, "a Vulkan driver must not masquerade as DXVK");
    check(!plan.production_ready, "invalid component kinds must never be production-ready");
}

void testDuplicateComponentRejected() {
    using namespace nyamist::runtime;
    ComponentRegistry registry;
    registry.add(probe("panvk", ComponentKind::vulkan_driver, ValidationLevel::discovered));

    bool threw = false;
    try {
        registry.add(probe("panvk", ComponentKind::vulkan_driver, ValidationLevel::discovered));
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "duplicate component IDs must be rejected");
}

} // namespace

int main() {
    try {
        testRegistryAndD3D11Plan();
        testD3D12RequiresVkd3d();
        testProductionRequiresSustainedValidation();
        testComponentKindMismatchRejected();
        testDuplicateComponentRejected();
        std::cout << "All NYAMIST runtime tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Runtime test failure: " << error.what() << '\n';
        return 1;
    }
}
