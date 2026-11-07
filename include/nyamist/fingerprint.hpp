#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <string_view>

namespace nyamist {

// FNV-1a 64-bit, formatted as 16 lowercase hexadecimal characters.
// This is a deterministic cache fingerprint, NOT a cryptographic hash.
[[nodiscard]] std::string fingerprint(std::span<const std::byte> data);
[[nodiscard]] std::string fingerprint(std::string_view text);

} // namespace nyamist
