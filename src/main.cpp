#include "nyamist/cache_store.hpp"
#include "nyamist/fingerprint.hpp"
#include "nyamist/optimization_advisor.hpp"
#include "nyamist/runtime/panvk_adapter.hpp"
#include "nyamist/runtime/translator_adapters.hpp"

#include <cstddef>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

void printUsage() {
    std::cout
        << "NYAMIST Graphics Runtime\n"
        << "Usage:\n"
        << "  nyamist-cli advice\n"
        << "  nyamist-cli fingerprint <text>\n"
        << "  nyamist-cli demo-cache <directory>\n"
        << "  nyamist-cli inspect-panvk <report.env>\n"
        << "  nyamist-cli inspect-dxvk <report.env>\n"
        << "  nyamist-cli inspect-vkd3d <report.env>\n";
}

const char* validationName(nyamist::runtime::ValidationLevel level) {
    using nyamist::runtime::ValidationLevel;
    switch (level) {
    case ValidationLevel::unavailable: return "unavailable";
    case ValidationLevel::discovered: return "discovered";
    case ValidationLevel::smoke_tested: return "smoke_tested";
    case ValidationLevel::sustained_tested: return "sustained_tested";
    }
    return "unknown";
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

int runInspectPanvk(const std::string& report_path) {
    try {
        const auto adapter =
            nyamist::runtime::PanVkAdapter::fromReportFile(report_path);
        const auto report = adapter.inspect();

        std::cout << "Component: " << report.descriptor.id << '\n'
                  << "Driver: " << report.descriptor.version << '\n'
                  << "Origin: " << report.descriptor.origin << '\n'
                  << "Validation: " << validationName(report.validation) << '\n';

        for (const auto& capability : report.capabilities) {
            std::cout << "[capability] " << capability << '\n';
        }
        for (const auto& diagnostic : report.diagnostics) {
            std::cout << "[diagnostic] " << diagnostic << '\n';
        }
        return report.validation == nyamist::runtime::ValidationLevel::unavailable ? 1 : 0;
    } catch (const std::exception& error) {
        std::cerr << "PanVK report inspection failed: " << error.what() << '\n';
        return 1;
    }
}

template <typename Adapter>
int runInspectTranslator(const std::string& report_path, const char* label) {
    try {
        const auto adapter = Adapter::fromReportFile(report_path);
        const auto report = adapter.inspect();

        std::cout << "Component: " << report.descriptor.id << '\\n'
                  << "Version: " << report.descriptor.version << '\\n'
                  << "Origin: " << report.descriptor.origin << '\\n'
                  << "Validation: " << validationName(report.validation) << '\\n';

        for (const auto& capability : report.capabilities) {
            std::cout << "[capability] " << capability << '\\n';
        }
        for (const auto& diagnostic : report.diagnostics) {
            std::cout << "[diagnostic] " << diagnostic << '\\n';
        }
        return report.validation == nyamist::runtime::ValidationLevel::unavailable ? 1 : 0;
    } catch (const std::exception& error) {
        std::cerr << label << " report inspection failed: " << error.what() << '\\n';
        return 1;
    }
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
    if (command == "inspect-panvk" && argc == 3) {
        return runInspectPanvk(argv[2]);
    }
    if (command == "inspect-dxvk" && argc == 3) {
        return runInspectTranslator<nyamist::runtime::DxvkAdapter>(argv[2], "DXVK");
    }
    if (command == "inspect-vkd3d" && argc == 3) {
        return runInspectTranslator<nyamist::runtime::Vkd3dProtonAdapter>(argv[2], "VKD3D-Proton");
    }

    printUsage();
    return 2;
}
