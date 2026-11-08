#include "nyamist/runtime/component_registry.hpp"

#include <stdexcept>
#include <utility>

namespace nyamist::runtime {

void ComponentRegistry::add(std::shared_ptr<const IComponentProbe> probe) {
    if (!probe) {
        throw std::invalid_argument("component probe must not be null");
    }

    const auto report = probe->inspect();
    if (report.descriptor.id.empty()) {
        throw std::invalid_argument("component id must not be empty");
    }

    for (const auto& existing : probes_) {
        if (existing->inspect().descriptor.id == report.descriptor.id) {
            throw std::invalid_argument("duplicate component id: " + report.descriptor.id);
        }
    }

    probes_.push_back(std::move(probe));
}

std::vector<ComponentReport> ComponentRegistry::inspectAll() const {
    std::vector<ComponentReport> reports;
    reports.reserve(probes_.size());
    for (const auto& probe : probes_) {
        reports.push_back(probe->inspect());
    }
    return reports;
}

std::optional<ComponentReport> ComponentRegistry::inspect(
    const std::string& component_id) const {
    for (const auto& probe : probes_) {
        auto report = probe->inspect();
        if (report.descriptor.id == component_id) {
            return report;
        }
    }
    return std::nullopt;
}

} // namespace nyamist::runtime
