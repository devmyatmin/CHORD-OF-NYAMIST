#pragma once

#include "nyamist/runtime/component.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace nyamist::runtime {

class ComponentRegistry {
public:
    // The registry owns only probe objects, never the external graphics components.
    void add(std::shared_ptr<const IComponentProbe> probe);

    [[nodiscard]] std::vector<ComponentReport> inspectAll() const;
    [[nodiscard]] std::optional<ComponentReport> inspect(
        const std::string& component_id) const;

private:
    std::vector<std::shared_ptr<const IComponentProbe>> probes_;
};

} // namespace nyamist::runtime
