#pragma once

#include "nyamist/runtime/component_registry.hpp"

#include <string>
#include <vector>

namespace nyamist::runtime {

enum class GraphicsApi {
    d3d11,
    d3d12
};

struct RuntimeRequest {
    GraphicsApi api = GraphicsApi::d3d11;
    std::string vulkan_driver_id = "android-vulkan";
};

struct RuntimePlan {
    bool prerequisites_present = false;
    bool smoke_test_allowed = false;
    bool production_ready = false;
    std::vector<std::string> selected_components;
    std::vector<std::string> blockers;
    std::vector<std::string> warnings;
};

// Produces a diagnostic/advisory plan only. It does not launch games or
// change Android loader, Vulkan ICD, Wine, or translator configuration.
class RuntimeCoordinator {
public:
    explicit RuntimeCoordinator(const ComponentRegistry& registry);

    [[nodiscard]] RuntimePlan evaluate(const RuntimeRequest& request) const;

private:
    const ComponentRegistry& registry_;
};

} // namespace nyamist::runtime
