#include "nyamist/cache_store.hpp"
#include "nyamist/fingerprint.hpp"

#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

std::size_t parseSize(const char* text, const char* label,
                      std::size_t minimum, std::size_t maximum) {
    const std::string value(text);
    std::size_t parsed_value = 0;
    const auto parsed = std::from_chars(
        value.data(), value.data() + value.size(), parsed_value);
    if (value.empty() || parsed.ec != std::errc{} ||
        parsed.ptr != value.data() + value.size() ||
        parsed_value < minimum || parsed_value > maximum) {
        throw std::invalid_argument(
            std::string(label) + " must be between " +
            std::to_string(minimum) + " and " + std::to_string(maximum));
    }
    return parsed_value;
}

std::vector<std::byte> makePayload(std::size_t size) {
    std::vector<std::byte> payload(size);
    for (std::size_t i = 0; i < size; ++i) {
        const auto value = static_cast<unsigned char>(
            (i * 131U + (i / 7U) * 17U + 29U) & 0xffU);
        payload[i] = static_cast<std::byte>(value);
    }
    return payload;
}

// Reproduce the original V1 hexadecimal payload representation for a
// format-level comparison. This is benchmark-only code, not a runtime path.
std::string encodeLegacyHex(std::span<const std::byte> payload) {
    std::ostringstream output;
    output << std::hex << std::nouppercase << std::setfill('0');
    for (const std::byte value : payload) {
        output << std::setw(2)
               << static_cast<unsigned int>(
                      std::to_integer<unsigned char>(value));
    }
    return output.str();
}

void writeLegacyV1(const std::filesystem::path& root,
                   const nyamist::CacheStore& store,
                   const nyamist::CacheIdentity& identity,
                   std::span<const std::byte> payload) {
    const std::string key = store.keyFor(identity);
    const std::string checksum = nyamist::fingerprint(payload);
    const std::string payload_hex = encodeLegacyHex(payload);
    const std::filesystem::path destination = root / (key + ".nyc");

    std::filesystem::create_directories(root);

    static std::atomic<std::uint64_t> sequence{0};
    const auto tick = Clock::now().time_since_epoch().count();
    const std::filesystem::path temporary =
        destination.string() + "." + std::to_string(tick) + "-" +
        std::to_string(sequence.fetch_add(1, std::memory_order_relaxed)) + ".tmp";

    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) {
            throw std::runtime_error("could not create legacy V1 benchmark file");
        }
        output << "NYAMIST_CACHE_V1\n" << key << '\n' << checksum << '\n'
               << payload_hex << '\n';
        output.flush();
        if (!output) {
            output.close();
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            throw std::runtime_error("failed writing legacy V1 benchmark file");
        }
    }

    std::error_code error;
    std::filesystem::rename(temporary, destination, error);
    if (error) {
        std::error_code ignored;
        std::filesystem::remove(destination, ignored);
        error.clear();
        std::filesystem::rename(temporary, destination, error);
    }
    if (error) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw std::runtime_error(
            "could not publish legacy V1 benchmark file: " + error.message());
    }
}

template <typename Operation>
std::vector<double> measureMilliseconds(std::size_t iterations,
                                        Operation&& operation) {
    std::vector<double> samples;
    samples.reserve(iterations);
    for (std::size_t i = 0; i < iterations; ++i) {
        const auto start = Clock::now();
        operation();
        const auto end = Clock::now();
        samples.push_back(
            std::chrono::duration<double, std::milli>(end - start).count());
    }
    return samples;
}

double medianMilliseconds(std::vector<double> samples) {
    if (samples.empty()) {
        return 0.0;
    }
    std::sort(samples.begin(), samples.end());
    const std::size_t middle = samples.size() / 2U;
    if (samples.size() % 2U == 0U) {
        return (samples[middle - 1U] + samples[middle]) / 2.0;
    }
    return samples[middle];
}

void printLatency(const char* label, const std::vector<double>& samples,
                  std::size_t payload_bytes) {
    const double median = medianMilliseconds(samples);
    const double mib = static_cast<double>(payload_bytes) / (1024.0 * 1024.0);
    const double throughput = median > 0.0 ? (mib * 1000.0 / median) : 0.0;
    std::cout << label << " median_ms=" << std::fixed << std::setprecision(3)
              << median << " throughput_MiB_s=" << throughput << '\n';
}

nyamist::CacheIdentity makeIdentity(const char* format) {
    nyamist::CacheIdentity identity;
    identity.source_fingerprint =
        nyamist::fingerprint("nyamist-cache-format-benchmark-payload");
    identity.source_api = "benchmark-" + std::string(format);
    identity.target_api = "vulkan";
    identity.gpu_architecture = "Immortalis-G925";
    identity.vulkan_api_version = "1.4.363";
    identity.driver_version = "PanVK-RC5";
    identity.translator_version = "nyamist-cache-benchmark";
    return identity;
}

} // namespace

int main(int argc, char** argv) {
    if (argc > 4) {
        std::cerr
            << "Usage: nyamist-cache-bench <directory> [payload_mib=4] [iterations=10]\n";
        return 2;
    }

    try {
        const std::filesystem::path base =
            argc >= 2 ? argv[1] : "./nyamist-cache-benchmark";
        const std::size_t payload_mib =
            argc >= 3 ? parseSize(argv[2], "payload_mib", 1U, 64U) : 4U;
        const std::size_t iterations =
            argc >= 4 ? parseSize(argv[3], "iterations", 3U, 100U) : 10U;
        const std::size_t payload_size = payload_mib * 1024U * 1024U;
        const auto payload = makePayload(payload_size);

        const auto v1_root = base / "v1";
        const auto v2_root = base / "v2";
        const nyamist::CacheStore v1_reader(v1_root);
        const nyamist::CacheStore v2_store(v2_root);
        const auto v1_identity = makeIdentity("v1");
        const auto v2_identity = makeIdentity("v2");

        const auto v1_path = v1_root / (v1_reader.keyFor(v1_identity) + ".nyc");
        const auto v2_path = v2_root / (v2_store.keyFor(v2_identity) + ".nyc");

        std::filesystem::create_directories(base);
        writeLegacyV1(v1_root, v1_reader, v1_identity, payload);
        v2_store.saveValidated(v2_identity, payload, true);

        const auto verify = [&payload](const nyamist::CacheStore& store,
                                       const nyamist::CacheIdentity& identity) {
            const auto loaded = store.load(identity);
            if (!loaded || *loaded != payload) {
                throw std::runtime_error("cache verification failed before benchmark");
            }
        };
        verify(v1_reader, v1_identity);
        verify(v2_store, v2_identity);

        const auto v1_write = measureMilliseconds(iterations, [&] {
            writeLegacyV1(v1_root, v1_reader, v1_identity, payload);
        });
        const auto v2_write = measureMilliseconds(iterations, [&] {
            v2_store.saveValidated(v2_identity, payload, true);
        });

        // Warm-read timings intentionally measure repeated reads after the
        // OS filesystem cache has already seen both files.
        verify(v1_reader, v1_identity);
        verify(v2_store, v2_identity);
        const auto v1_read = measureMilliseconds(iterations, [&] {
            verify(v1_reader, v1_identity);
        });
        const auto v2_read = measureMilliseconds(iterations, [&] {
            verify(v2_store, v2_identity);
        });

        const auto v1_bytes = std::filesystem::file_size(v1_path);
        const auto v2_bytes = std::filesystem::file_size(v2_path);
        const double saved_percent =
            v1_bytes > 0U
                ? (static_cast<double>(v1_bytes - v2_bytes) /
                   static_cast<double>(v1_bytes)) * 100.0
                : 0.0;

        std::cout << "NYAMIST cache format benchmark (synthetic payload)\n"
                  << "Payload: " << payload_size << " bytes (" << payload_mib
                  << " MiB), iterations: " << iterations << '\n'
                  << "V1 file bytes: " << v1_bytes << '\n'
                  << "V2 file bytes: " << v2_bytes << '\n'
                  << "Disk payload reduction: " << std::fixed
                  << std::setprecision(2) << saved_percent << "%\n";
        printLatency("V1 write", v1_write, payload_size);
        printLatency("V2 write", v2_write, payload_size);
        printLatency("V1 warm read", v1_read, payload_size);
        printLatency("V2 warm read", v2_read, payload_size);
        std::cout
            << "Note: warm filesystem-cache microbenchmark only; not game FPS, "
               "GPU throughput, or a cold-storage benchmark.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Cache benchmark failed: " << error.what() << '\n';
        return 1;
    }
}
