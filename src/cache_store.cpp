#include "nyamist/cache_store.hpp"

#include "nyamist/fingerprint.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace nyamist {
namespace {

constexpr const char* kMagic = "NYAMIST_CACHE_V1";

void appendField(std::string& material, const std::string& value) {
    material += std::to_string(value.size());
    material.push_back(':');
    material += value;
    material.push_back(';');
}

std::string identityMaterial(const CacheIdentity& id) {
    std::string material;
    appendField(material, id.source_fingerprint);
    appendField(material, id.source_api);
    appendField(material, id.target_api);
    appendField(material, id.gpu_architecture);
    appendField(material, id.vulkan_api_version);
    appendField(material, id.driver_version);
    appendField(material, id.translator_version);
    return material;
}

std::string encodeHex(std::span<const std::byte> bytes) {
    std::ostringstream out;
    out << std::hex << std::nouppercase << std::setfill('0');
    for (const std::byte value : bytes) {
        out << std::setw(2)
            << static_cast<unsigned int>(std::to_integer<unsigned char>(value));
    }
    return out.str();
}

std::optional<std::vector<std::byte>> decodeHex(const std::string& text) {
    if (text.size() % 2 != 0) {
        return std::nullopt;
    }

    std::vector<std::byte> result;
    result.reserve(text.size() / 2);
    for (std::size_t i = 0; i < text.size(); i += 2) {
        unsigned int value = 0;
        std::istringstream in(text.substr(i, 2));
        in >> std::hex >> value;
        if (!in || !in.eof() || value > 255U) {
            return std::nullopt;
        }
        result.push_back(static_cast<std::byte>(value));
    }
    return result;
}

} // namespace

CacheStore::CacheStore(std::filesystem::path root) : root_(std::move(root)) {}

std::string CacheStore::keyFor(const CacheIdentity& identity) const {
    return fingerprint(identityMaterial(identity));
}

std::optional<std::vector<std::byte>> CacheStore::load(
    const CacheIdentity& identity) const {
    const std::string key = keyFor(identity);
    const std::filesystem::path path = root_ / (key + ".nyc");
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return std::nullopt;
    }

    std::string magic;
    std::string stored_key;
    std::string stored_checksum;
    std::string payload_hex;
    if (!std::getline(input, magic) ||
        !std::getline(input, stored_key) ||
        !std::getline(input, stored_checksum) ||
        !std::getline(input, payload_hex)) {
        return std::nullopt;
    }

    std::string trailing;
    if (std::getline(input, trailing) || magic != kMagic || stored_key != key) {
        return std::nullopt;
    }

    auto payload = decodeHex(payload_hex);
    if (!payload || payload->empty() || fingerprint(*payload) != stored_checksum) {
        return std::nullopt;
    }
    return payload;
}

void CacheStore::saveValidated(
    const CacheIdentity& identity,
    std::span<const std::byte> payload,
    bool validation_passed) const {
    if (!validation_passed) {
        throw std::invalid_argument("refusing to cache an artifact that failed validation");
    }
    if (payload.empty()) {
        throw std::invalid_argument("refusing to cache an empty artifact");
    }

    std::filesystem::create_directories(root_);
    const std::string key = keyFor(identity);
    const std::string payload_hex = encodeHex(payload);
    const std::string checksum = fingerprint(payload);

    static std::atomic<std::uint64_t> sequence{0};
    const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::string suffix = std::to_string(tick) + "-" +
        std::to_string(sequence.fetch_add(1, std::memory_order_relaxed));
    const std::filesystem::path destination = root_ / (key + ".nyc");
    const std::filesystem::path temporary = root_ / (key + "." + suffix + ".tmp");

    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) {
            throw std::runtime_error("could not create temporary cache file");
        }
        output << kMagic << '\n' << key << '\n' << checksum << '\n' << payload_hex << '\n';
        output.flush();
        if (!output) {
            output.close();
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            throw std::runtime_error("failed while writing cache file");
        }
    }

    std::error_code error;
    std::filesystem::rename(temporary, destination, error);
    if (error) {
        // Some platforms do not replace an existing destination on rename.
        std::error_code remove_error;
        std::filesystem::remove(destination, remove_error);
        error.clear();
        std::filesystem::rename(temporary, destination, error);
    }
    if (error) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw std::runtime_error("could not publish cache file: " + error.message());
    }
}

} // namespace nyamist
