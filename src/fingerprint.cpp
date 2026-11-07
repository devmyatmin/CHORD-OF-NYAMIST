#include "nyamist/fingerprint.hpp"

#include <cstdint>
#include <iomanip>
#include <sstream>

namespace nyamist {

std::string fingerprint(std::span<const std::byte> data) {
    constexpr std::uint64_t offset_basis = 14695981039346656037ULL;
    constexpr std::uint64_t prime = 1099511628211ULL;

    std::uint64_t hash = offset_basis;
    for (const std::byte item : data) {
        hash ^= static_cast<std::uint64_t>(std::to_integer<unsigned char>(item));
        hash *= prime;
    }

    std::ostringstream output;
    output << std::hex << std::nouppercase << std::setfill('0') << std::setw(16) << hash;
    return output.str();
}

std::string fingerprint(std::string_view text) {
    if (text.empty()) {
        return fingerprint(std::span<const std::byte>{});
    }
    const auto* data = reinterpret_cast<const std::byte*>(text.data());
    return fingerprint(std::span<const std::byte>(data, text.size()));
}

} // namespace nyamist
