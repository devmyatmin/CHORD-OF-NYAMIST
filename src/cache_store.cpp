#include "nyamist/cache_store.hpp"

#include "nyamist/fingerprint.hpp"

#include <atomic>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <utility>

namespace nyamist {
namespace {

constexpr std::string_view kMagicV1 = "NYAMIST_CACHE_V1";
constexpr std::string_view kMagicV2 = "NYAMIST_CACHE_V2";
constexpr std::uintmax_t kMaximumPayloadBytes = 64U * 1024U * 1024U;
constexpr std::uintmax_t kMaximumFileBytes =
    (kMaximumPayloadBytes * 2U) + 4096U;

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

int hexValue(unsigned char value) noexcept {
    if (value >= '0' && value <= '9') {
        return static_cast<int>(value - '0');
    }
    if (value >= 'a' && value <= 'f') {
        return static_cast<int>(value - 'a') + 10;
    }
    if (value >= 'A' && value <= 'F') {
        return static_cast<int>(value - 'A') + 10;
    }
    return -1;
}

// V1 compatibility only. Decode directly into bytes without allocating a
// temporary string or constructing a stream for every byte.
std::optional<std::vector<std::byte>> decodeLegacyHex(const std::string& text) {
    if (text.empty() || text.size() % 2U != 0U ||
        text.size() / 2U > kMaximumPayloadBytes) {
        return std::nullopt;
    }

    std::vector<std::byte> result(text.size() / 2U);
    for (std::size_t i = 0; i < text.size(); i += 2U) {
        const int high = hexValue(static_cast<unsigned char>(text[i]));
        const int low = hexValue(static_cast<unsigned char>(text[i + 1U]));
        if (high < 0 || low < 0) {
            return std::nullopt;
        }
        const auto value = static_cast<unsigned char>((high << 4) | low);
        result[i / 2U] = static_cast<std::byte>(value);
    }
    return result;
}

std::optional<std::size_t> parsePayloadSize(const std::string& text) {
    std::size_t value = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    if (text.empty() || parsed.ec != std::errc{} ||
        parsed.ptr != text.data() + text.size()) {
        return std::nullopt;
    }
    return value;
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

    std::error_code size_error;
    const auto file_bytes = std::filesystem::file_size(path, size_error);
    if (size_error || file_bytes > kMaximumFileBytes) {
        return std::nullopt;
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return std::nullopt;
    }

    std::string magic;
    std::string stored_key;
    std::string stored_checksum;
    if (!std::getline(input, magic) ||
        !std::getline(input, stored_key) ||
        !std::getline(input, stored_checksum) ||
        stored_key != key) {
        return std::nullopt;
    }

    if (magic == kMagicV1) {
        std::string payload_hex;
        if (!std::getline(input, payload_hex)) {
            return std::nullopt;
        }

        std::string trailing;
        if (std::getline(input, trailing)) {
            return std::nullopt;
        }

        auto payload = decodeLegacyHex(payload_hex);
        if (!payload || payload->empty() ||
            fingerprint(*payload) != stored_checksum) {
            return std::nullopt;
        }
        return payload;
    }

    if (magic != kMagicV2) {
        return std::nullopt;
    }

    std::string size_text;
    if (!std::getline(input, size_text)) {
        return std::nullopt;
    }
    const auto payload_size = parsePayloadSize(size_text);
    if (!payload_size || *payload_size == 0U ||
        *payload_size > kMaximumPayloadBytes) {
        return std::nullopt;
    }

    const auto payload_position = input.tellg();
    if (payload_position == std::streampos(-1)) {
        return std::nullopt;
    }
    const auto payload_offset =
        static_cast<std::uintmax_t>(static_cast<std::streamoff>(payload_position));
    if (file_bytes < payload_offset ||
        file_bytes - payload_offset != *payload_size) {
        return std::nullopt;
    }

    std::vector<std::byte> payload(*payload_size);
    const auto stream_size = static_cast<std::streamsize>(*payload_size);
    input.read(reinterpret_cast<char*>(payload.data()), stream_size);
    if (input.gcount() != stream_size || fingerprint(payload) != stored_checksum) {
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
    if (payload.size() > kMaximumPayloadBytes) {
        throw std::invalid_argument("cache artifact exceeds the 64 MiB per-entry limit");
    }

    std::filesystem::create_directories(root_);
    const std::string key = keyFor(identity);
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
        output << kMagicV2 << '\n' << key << '\n' << checksum << '\n'
               << payload.size() << '\n';
        output.write(
            reinterpret_cast<const char*>(payload.data()),
            static_cast<std::streamsize>(payload.size()));
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
