#include "nyamist/cache_store.hpp"
#include "nyamist/fingerprint.hpp"
#include "nyamist/optimization_advisor.hpp"

#include <cstddef>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

void printUsage() {
    std::cout
        << "NYAMIST-LAYER prototype\n"
        << "Usage:\n"
        << "  nyamist-cli advice\n"
        << "  nyamist-cli fingerprint <text>\n"
        << "  nyamist-cli demo-cache <directory>\n";
}

int runAdvice() {
    nyamist::GpuProfile gpu;
    gpu.gpu_name = "REDMI K80 Ultra (target profile; runtime probe pending)";
    gpu.gpu_architecture = "Arm Immortalis-G925";
    gpu.vulkan_api_version = "probe-required";
    gpu.driver_version = "probe-required";
    gpu.logical_cpu_count = 8;

    nyamist::ShaderProfile shader;
    const nyamist::RuleBasedAdvisor advisor;
    const auto plan = advisor.recommend(gpu, shader);

    std::cout << "GPU: " << gpu.gpu_name << '\n'
              << "Architecture: " << gpu.gpu_architecture << '\n'
              << "Graphics Pipeline Library: "
              << (plan.use_graphics_pipeline_library ? "reported by profile" : "not confirmed") << '\n'
              << "Suggested shader workers: " << plan.suggested_shader_workers << '\n';
    for (const auto& note : plan.notes) {
        std::cout << "[note] " << note << '\n';
    }
    for (const auto& warning : plan.compatibility_warnings) {
        std::cout << "[warning] " << warning << '\n';
    }
    return 0;
}

int runDemoCache(const std::string& directory) {
    nyamist::CacheStore cache(directory);
    nyamist::CacheIdentity identity;
    identity.source_fingerprint = nyamist::fingerprint("demo-d3d11-shader-bytecode");
    identity.gpu_architecture = "Immortalis-G925";
    identity.vulkan_api_version = "runtime-probe-required";
    identity.driver_version = "runtime-probe-required";

    const std::string demo = "validated-demo-optimization-metadata";
    const auto bytes = std::as_bytes(std::span(demo.data(), demo.size()));
    cache.saveValidated(identity, bytes, true);

    const auto restored = cache.load(identity);
    if (!restored) {
        std::cerr << "Cache round-trip failed.\n";
        return 1;
    }
    std::cout << "Validated cache entry stored and loaded. Key: "
              << cache.keyFor(identity) << '\n';
    std::cout << "This is a cache-core demo, not a translated game shader.\n";
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        printUsage();
        return 2;
    }

    const std::string command = argv[1];
    if (command == "advice") {
        return runAdvice();
    }
    if (command == "fingerprint" && argc >= 3) {
        std::cout << nyamist::fingerprint(argv[2]) << '\n';
        return 0;
    }
    if (command == "demo-cache" && argc >= 3) {
        return runDemoCache(argv[2]);
    }

    printUsage();
    return 2;
}
