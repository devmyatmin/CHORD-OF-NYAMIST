#pragma once

#include <string>
#include <vector>

namespace nyamist::runtime {

enum class ComponentKind {
    translator,
    vulkan_driver,
    advisor,
    monitor
};

enum class ValidationLevel {
    unavailable,
    discovered,
    smoke_tested,
    sustained_tested
};

struct ComponentDescriptor {
    std::string id;
    ComponentKind kind = ComponentKind::monitor;
    std::string version = "unknown";
    std::string origin = "external";
};

struct ComponentReport {
    ComponentDescriptor descriptor;
    ValidationLevel validation = ValidationLevel::unavailable;
    std::vector<std::string> capabilities;
    std::vector<std::string> diagnostics;
};

// Adapters inspect independently installed components. They do not load,
// replace, patch, or take ownership of DXVK, VKD3D-Proton, or Vulkan ICDs.
class IComponentProbe {
public:
    virtual ~IComponentProbe() = default;
    [[nodiscard]] virtual ComponentReport inspect() const = 0;
};

} // namespace nyamist::runtime
