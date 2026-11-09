#include "nyamist/cache_store.hpp"
#include "nyamist/discovery/hardware_discovery.hpp"
#include "nyamist/discovery/logical_device_validation.hpp"
#include "nyamist/fingerprint.hpp"
#include "nyamist/optimization_advisor.hpp"
#include "nyamist/runtime/device_probe_adapter.hpp"
#include "nyamist/runtime/panvk_adapter.hpp"
#include "nyamist/runtime/translator_adapters.hpp"

#include <cstddef>
#include <fstream>
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
        << "  nyamist-cli discover [--output <report.json>]\n"
        << "  nyamist-cli validate-device [--output <report.json>]\n"
        << "  nyamist-cli inspect-panvk <report.env>\n"
        << "  nyamist-cli inspect-device <report.env>\n"
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

int runDiscover(int argc, char** argv) {
    std::string output_path;
    if (argc == 4 && std::string(argv[2]) == "--output") {
        output_path = argv[3];
    } else if (argc != 2) {
        printUsage();
        return 2;
    }

    const auto report = nyamist::discovery::collectHardwareDiscoveryReport();
    const std::string json = nyamist::discovery::toJson(report);
    if (output_path.empty()) {
        std::cout << json;
        return std::cout.good() ? 0 : 1;
    }

    std::ofstream output(output_path, std::ios::binary | std::ios::trunc);
    if (!output) {
        std::cerr << "Unable to create discovery report: " << output_path << '\n';
        return 1;
    }
    output << json;
    if (!output.good()) {
        std::cerr << "Failed while writing discovery report: " << output_path << '\n';
        return 1;
    }
    std::cout << "NYAMIST hardware discovery report: " << output_path << '\n'
              << "Vulkan enumeration status: " << report.vulkan_status << '\n';
    if (!report.vulkan_failure_reason.empty()) {
        std::cout << "Vulkan note: " << report.vulkan_failure_reason << '\n';
    }
    return 0;
}

int runValidateDevice(int argc, char** argv) {
    std::string output_path;
    if (argc == 4 && std::string(argv[2]) == "--output") {
        output_path = argv[3];
    } else if (argc != 2) {
        printUsage();
        return 2;
    }

    const auto report =
        nyamist::discovery::collectLogicalDeviceValidationReport();
    const std::string json = nyamist::discovery::toJson(report);
    if (output_path.empty()) {
        std::cout << json;
        if (!std::cout.good()) return 1;
    } else {
        std::ofstream output(output_path, std::ios::binary | std::ios::trunc);
        if (!output) {
            std::cerr << "Unable to create logical-device report: "
                      << output_path << '\n';
            return 1;
        }
        output << json;
        if (!output.good()) {
            std::cerr << "Failed while writing logical-device report: "
                      << output_path << '\n';
            return 1;
        }
        std::cout << "NYAMIST R1.2 logical-device report: "
                  << output_path << '\n'
                  << "Validation status: " << report.status << '\n'
                  << "Physical device: "
                  << (report.device_name.empty() ? "not identified" : report.device_name)
                  << '\n'
                  << "Target G925 name match: "
                  << (report.target_gpu_match ? "yes" : "no") << '\n'
                  << "Logical device: " << report.logical_device_status << '\n'
                  << "Queue retrieval: " << report.queue_retrieval_status << '\n'
                  << "Cleanup: " << report.cleanup_status << '\n';
        if (!report.failure_reason.empty()) {
            std::cout << "Failure reason: " << report.failure_reason << '\n';
        }
    }
    return report.status == "success" ? 0 : 1;
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

void printMonitorReport(const nyamist::runtime::ComponentReport& report) {
    std::cout << "Component: " << report.descriptor.id << '\n'
              << "Origin: " << report.descriptor.origin << '\n'
              << "Validation: " << validationName(report.validation) << '\n';
    for (const auto& capability : report.capabilities) {
        std::cout << "[capability] " << capability << '\n';
    }
    for (const auto& diagnostic : report.diagnostics) {
        std::cout << "[diagnostic] " << diagnostic << '\n';
    }
}

int runInspectDevice(const std::string& report_path) {
    try {
        const auto snapshot = nyamist::runtime::DeviceProbeReportParser::fromFile(report_path);
        const auto device = nyamist::runtime::DeviceFactsAdapter(snapshot).inspect();
        const auto vulkan = nyamist::runtime::VulkanCapabilityAdapter(snapshot).inspect();
        printMonitorReport(device);
        std::cout << '\n';
        printMonitorReport(vulkan);
        return device.validation == nyamist::runtime::ValidationLevel::unavailable ? 1 : 0;
    } catch (const std::exception& error) {
        std::cerr << "Device probe inspection failed: " << error.what() << '\n';
        return 1;
    }
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

        std::cout << "Component: " << report.descriptor.id << '\n'
                  << "Version: " << report.descriptor.version << '\n'
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
        std::cerr << label << " report inspection failed: " << error.what() << '\n';
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
    if (command == "discover") {
        return runDiscover(argc, argv);
    }
    if (command == "validate-device") {
        return runValidateDevice(argc, argv);
    }
    if (command == "inspect-panvk" && argc == 3) {
        return runInspectPanvk(argv[2]);
    }
    if (command == "inspect-device" && argc == 3) {
        return runInspectDevice(argv[2]);
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
