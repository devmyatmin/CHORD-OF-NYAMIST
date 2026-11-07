#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace nyamist {

struct CacheIdentity {
    std::string source_fingerprint;
    std::string source_api = "d3d11";
    std::string target_api = "vulkan";
    std::string gpu_architecture;
    std::string vulkan_api_version;
    std::string driver_version;
    std::string translator_version = "nyamist-0.1";
};

class CacheStore {
public:
    explicit CacheStore(std::filesystem::path root);

    [[nodiscard]] std::string keyFor(const CacheIdentity& identity) const;
    [[nodiscard]] std::optional<std::vector<std::byte>> load(
        const CacheIdentity& identity) const;

    // Artifacts are persisted only after the caller's validator has passed.
    // Throws std::invalid_argument when validation_passed is false or payload is empty.
    void saveValidated(
        const CacheIdentity& identity,
        std::span<const std::byte> payload,
        bool validation_passed) const;

private:
    std::filesystem::path root_;
};

} // namespace nyamist
