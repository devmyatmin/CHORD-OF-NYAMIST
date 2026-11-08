#pragma once

#include "nyamist/runtime/component.hpp"

#include <cstdint>
#include <filesystem>
#include <istream>
#include <string>
#include <vector>

namespace nyamist::runtime {

struct TranslatorProbeSnapshot {
    std::string translator_id;
    std::string api;
    std::string translator_version;
    std::string evidence_source = "external translator report";

    bool device_created = false;
    std::string max_feature_level;
    std::string selected_feature_level;
    std::vector<std::string> supported_feature_levels;
    std::vector<std::string> shader_models;

    bool render_present_passed = false;
    std::uint64_t frame_count = 0;
    std::uint64_t duration_seconds = 0;
    std::uint64_t present_calls = 0;
    std::uint64_t present_failures = 0;
    int process_exit_code = -1;
    std::uint64_t device_lost_events = 0;
    bool sustained_test_passed = false;
};

class TranslatorAdapterBase : public IComponentProbe {
public:
    [[nodiscard]] ComponentReport inspect() const override;
    [[nodiscard]] static TranslatorProbeSnapshot parseReport(std::istream& input);

protected:
    TranslatorAdapterBase(std::string expected_id,
                          std::string expected_api,
                          TranslatorProbeSnapshot snapshot);

    [[nodiscard]] static TranslatorProbeSnapshot fromReportFile(
        const std::filesystem::path& path,
        const std::string& expected_id,
        const std::string& expected_api);

private:
    std::string expected_id_;
    std::string expected_api_;
    TranslatorProbeSnapshot snapshot_;
};

class DxvkAdapter final : public TranslatorAdapterBase {
public:
    explicit DxvkAdapter(TranslatorProbeSnapshot snapshot);
    [[nodiscard]] static DxvkAdapter fromReportFile(const std::filesystem::path& path);
};

class Vkd3dProtonAdapter final : public TranslatorAdapterBase {
public:
    explicit Vkd3dProtonAdapter(TranslatorProbeSnapshot snapshot);
    [[nodiscard]] static Vkd3dProtonAdapter fromReportFile(const std::filesystem::path& path);
};

} // namespace nyamist::runtime
