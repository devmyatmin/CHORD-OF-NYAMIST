#include "nyamist/cache_store.hpp"
#include "nyamist/fingerprint.hpp"
#include "nyamist/optimization_advisor.hpp"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void check(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

nyamist::CacheIdentity sampleIdentity() {
    nyamist::CacheIdentity id;
    id.source_fingerprint = nyamist::fingerprint("sample-d3d11-bytecode");
    id.gpu_architecture = "Immortalis-G925";
    id.vulkan_api_version = "1.x-probed";
    id.driver_version = "test-driver-1";
    return id;
}

void testFingerprint() {
    check(nyamist::fingerprint("") == "cbf29ce484222325",
          "empty FNV-1a fingerprint must be stable");
    check(nyamist::fingerprint("abc") == "e71fa2190541574b",
          "known FNV-1a fingerprint must match");
    check(nyamist::fingerprint("abc") == nyamist::fingerprint("abc"),
          "same input must produce same fingerprint");
}

void testCacheRoundTripAndInvalidation(const std::filesystem::path& root) {
    nyamist::CacheStore store(root);
    const auto identity = sampleIdentity();
    const std::string payload_text = "validated optimizer metadata";
    const auto payload = std::as_bytes(std::span(payload_text.data(), payload_text.size()));

    store.saveValidated(identity, payload, true);
    const auto loaded = store.load(identity);
    check(loaded.has_value(), "saved cache entry must load");
    check(loaded->size() == payload.size(), "cache payload size must match");
    check(nyamist::fingerprint(*loaded) == nyamist::fingerprint(payload),
          "cache payload bytes must round-trip");

    const auto path = root / (store.keyFor(identity) + ".nyc");
    std::ifstream input(path, std::ios::binary);
    std::string magic;
    check(static_cast<bool>(std::getline(input, magic)) && magic == "NYAMIST_CACHE_V2",
          "new cache entries must use compact binary V2 format");

    auto changed_driver = identity;
    changed_driver.driver_version = "test-driver-2";
    check(!store.load(changed_driver).has_value(),
          "driver version change must invalidate cache identity");
}

void testBinaryPayloadRoundTrip(const std::filesystem::path& root) {
    nyamist::CacheStore store(root);
    const auto identity = sampleIdentity();
    const std::vector<std::byte> payload{
        std::byte{0x00}, std::byte{0xff}, std::byte{0x10},
        std::byte{0x00}, std::byte{0x7f}, std::byte{0x80}
    };

    store.saveValidated(identity, payload, true);
    const auto loaded = store.load(identity);
    check(loaded.has_value() && *loaded == payload,
          "binary cache must preserve null and non-text bytes exactly");
}

void testLegacyV1CacheRead(const std::filesystem::path& root) {
    nyamist::CacheStore store(root);
    const auto identity = sampleIdentity();
    const std::string payload_text = "legacy";
    const auto path = root / (store.keyFor(identity) + ".nyc");
    std::filesystem::create_directories(root);
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output << "NYAMIST_CACHE_V1\n"
               << store.keyFor(identity) << '\n'
               << nyamist::fingerprint(payload_text) << '\n'
               << "6c6567616379\n";
    }

    const auto loaded = store.load(identity);
    check(loaded.has_value(), "legacy V1 cache entries must remain readable");
    check(nyamist::fingerprint(*loaded) == nyamist::fingerprint(payload_text),
          "legacy V1 payload must retain its exact contents");
}

void testTruncatedV2EntryRejected(const std::filesystem::path& root) {
    nyamist::CacheStore store(root);
    const auto identity = sampleIdentity();
    std::filesystem::create_directories(root);
    const auto path = root / (store.keyFor(identity) + ".nyc");
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output << "NYAMIST_CACHE_V2\n"
               << store.keyFor(identity) << '\n'
               << nyamist::fingerprint("12345") << '\n'
               << "5\n"
               << "12";
    }
    check(!store.load(identity).has_value(),
          "truncated binary cache payloads must be rejected");
}

void testValidationGate(const std::filesystem::path& root) {
    nyamist::CacheStore store(root);
    const auto identity = sampleIdentity();
    const std::string payload_text = "not-validated";
    const auto payload = std::as_bytes(std::span(payload_text.data(), payload_text.size()));

    bool threw = false;
    try {
        store.saveValidated(identity, payload, false);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "unvalidated payload must be rejected");
    check(!store.load(identity).has_value(), "rejected payload must not be cached");
}

void testCorruptEntryRejected(const std::filesystem::path& root) {
    nyamist::CacheStore store(root);
    const auto identity = sampleIdentity();
    const std::string payload_text = "valid-at-write-time";
    const auto payload = std::as_bytes(std::span(payload_text.data(), payload_text.size()));
    store.saveValidated(identity, payload, true);

    const auto path = root / (store.keyFor(identity) + ".nyc");
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output << "NYAMIST_CACHE_V1\nwrong-key\nwrong-checksum\n00\n";
    }
    check(!store.load(identity).has_value(), "corrupt cache entry must be rejected");
}

void testRuleBasedAdvisor() {
    nyamist::GpuProfile gpu;
    gpu.logical_cpu_count = 8;
    gpu.supports_graphics_pipeline_library = true;

    nyamist::ShaderProfile shader;
    shader.uses_bindless_resources = true;
    const nyamist::RuleBasedAdvisor advisor;
    const auto plan = advisor.recommend(gpu, shader);

    check(plan.use_graphics_pipeline_library, "advisor must honor probed GPL support");
    check(plan.suggested_shader_workers == 4, "worker suggestion must be clamped");
    check(!plan.compatibility_warnings.empty(),
          "bindless shader profile must produce a compatibility warning");
}

} // namespace

int main() {
    try {
        const auto root = std::filesystem::temp_directory_path() / "nyamist-layer-tests";
        std::filesystem::remove_all(root);
        std::filesystem::create_directories(root);

        testFingerprint();
        testCacheRoundTripAndInvalidation(root / "roundtrip");
        testBinaryPayloadRoundTrip(root / "binary");
        testLegacyV1CacheRead(root / "legacy-v1");
        testTruncatedV2EntryRejected(root / "truncated-v2");
        testValidationGate(root / "validation");
        testCorruptEntryRejected(root / "corrupt");
        testRuleBasedAdvisor();

        std::filesystem::remove_all(root);
        std::cout << "All NYAMIST tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }
}
