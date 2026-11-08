#include "nyamist/runtime/profile_store.hpp"

#include "nyamist/fingerprint.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <map>
#include <span>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

namespace nyamist::runtime {
namespace {

constexpr std::string_view kSessionMagic = "NYAMIST_SESSION_PROFILE_V1";
constexpr std::string_view kCompatibilityMagic = "NYAMIST_COMPAT_PROFILE_V1";
constexpr std::size_t kMaximumPayloadBytes = 1024U * 1024U;
constexpr std::size_t kMaximumStringBytes = 4096U;
constexpr std::uint32_t kMaximumCollectionItems = 1024U;

class Writer {
public:
    void u8(std::uint8_t value) {
        bytes_.push_back(static_cast<std::byte>(value));
    }

    void boolean(bool value) { u8(value ? 1U : 0U); }

    void u32(std::uint32_t value) {
        for (unsigned shift = 0U; shift < 32U; shift += 8U) {
            u8(static_cast<std::uint8_t>((value >> shift) & 0xffU));
        }
    }

    void u64(std::uint64_t value) {
        for (unsigned shift = 0U; shift < 64U; shift += 8U) {
            u8(static_cast<std::uint8_t>((value >> shift) & 0xffU));
        }
    }

    void i64(std::int64_t value) { u64(static_cast<std::uint64_t>(value)); }

    void string(const std::string& value) {
        if (value.size() > kMaximumStringBytes) {
            throw std::invalid_argument("profile string exceeds the 4096-byte limit");
        }
        u32(static_cast<std::uint32_t>(value.size()));
        const auto* first = reinterpret_cast<const std::byte*>(value.data());
        bytes_.insert(bytes_.end(), first, first + value.size());
    }

    [[nodiscard]] const std::vector<std::byte>& bytes() const noexcept {
        return bytes_;
    }

private:
    std::vector<std::byte> bytes_;
};

class Reader {
public:
    explicit Reader(std::span<const std::byte> bytes) : bytes_(bytes) {}

    [[nodiscard]] std::uint8_t u8() {
        require(1U);
        return std::to_integer<std::uint8_t>(bytes_[offset_++]);
    }

    [[nodiscard]] bool boolean() {
        const auto value = u8();
        if (value > 1U) {
            throw std::runtime_error("profile contains an invalid boolean");
        }
        return value == 1U;
    }

    [[nodiscard]] std::uint32_t u32() {
        std::uint32_t value = 0U;
        for (unsigned shift = 0U; shift < 32U; shift += 8U) {
            value |= static_cast<std::uint32_t>(u8()) << shift;
        }
        return value;
    }

    [[nodiscard]] std::uint64_t u64() {
        std::uint64_t value = 0U;
        for (unsigned shift = 0U; shift < 64U; shift += 8U) {
            value |= static_cast<std::uint64_t>(u8()) << shift;
        }
        return value;
    }

    [[nodiscard]] std::int64_t i64() { return std::bit_cast<std::int64_t>(u64()); }

    [[nodiscard]] std::string string() {
        const auto size = u32();
        if (size > kMaximumStringBytes) {
            throw std::runtime_error("profile string exceeds the 4096-byte limit");
        }
        require(size);
        const auto* first = reinterpret_cast<const char*>(bytes_.data() + offset_);
        std::string value(first, size);
        offset_ += size;
        return value;
    }

    [[nodiscard]] bool finished() const noexcept { return offset_ == bytes_.size(); }

private:
    void require(std::size_t count) const {
        if (count > bytes_.size() - offset_) {
            throw std::runtime_error("profile payload is truncated");
        }
    }

    std::span<const std::byte> bytes_;
    std::size_t offset_ = 0U;
};

bool isProfileKey(std::string_view key) {
    if (key.size() != 16U) return false;
    return std::all_of(key.begin(), key.end(), [](char value) {
        return (value >= '0' && value <= '9') || (value >= 'a' && value <= 'f');
    });
}

std::optional<std::size_t> parseSize(const std::string& text) {
    std::size_t value = 0U;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (text.empty() || result.ec != std::errc{} ||
        result.ptr != text.data() + text.size()) {
        return std::nullopt;
    }
    return value;
}

std::string savePayload(
    const std::filesystem::path& root,
    std::string_view extension,
    std::string_view magic,
    std::span<const std::byte> payload) {
    if (payload.empty() || payload.size() > kMaximumPayloadBytes) {
        throw std::invalid_argument("profile payload must be within [1, 1048576] bytes");
    }

    const std::string key = nyamist::fingerprint(payload);
    const auto destination = root / (key + std::string(extension));
    std::filesystem::create_directories(root);

    std::error_code exists_error;
    const bool destination_exists = std::filesystem::exists(destination, exists_error);
    if (!exists_error && destination_exists) {
        const auto existing = [&]() -> std::optional<std::vector<std::byte>> {
            std::error_code size_error;
            const auto file_size = std::filesystem::file_size(destination, size_error);
            if (size_error || file_size > kMaximumPayloadBytes + 256U) return std::nullopt;
            std::ifstream input(destination, std::ios::binary);
            if (!input) return std::nullopt;
            std::string stored_magic, stored_key, size_text;
            if (!std::getline(input, stored_magic) ||
                !std::getline(input, stored_key) ||
                !std::getline(input, size_text) ||
                stored_magic != magic || stored_key != key) return std::nullopt;
            const auto declared_size = parseSize(size_text);
            const auto position = input.tellg();
            if (!declared_size || *declared_size != payload.size() ||
                position == std::streampos(-1) ||
                file_size - static_cast<std::uintmax_t>(position) != *declared_size) {
                return std::nullopt;
            }
            std::vector<std::byte> data(*declared_size);
            input.read(reinterpret_cast<char*>(data.data()),
                       static_cast<std::streamsize>(data.size()));
            if (input.gcount() != static_cast<std::streamsize>(data.size()) ||
                nyamist::fingerprint(data) != key) return std::nullopt;
            return data;
        }();
        if (existing && std::equal(existing->begin(), existing->end(), payload.begin(), payload.end())) {
            return key;
        }
        std::error_code remove_error;
        std::filesystem::remove(destination, remove_error);
        if (remove_error) {
            throw std::runtime_error("could not replace corrupt profile: " + remove_error.message());
        }
    } else if (exists_error) {
        throw std::runtime_error("could not inspect profile destination: " + exists_error.message());
    }

    static std::atomic<std::uint64_t> sequence{0U};
    const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto temporary = root / (key + "." + std::to_string(tick) + "-" +
        std::to_string(sequence.fetch_add(1U, std::memory_order_relaxed)) + ".tmp");
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) throw std::runtime_error("could not create temporary profile file");
        output << magic << '\n' << key << '\n' << payload.size() << '\n';
        output.write(reinterpret_cast<const char*>(payload.data()),
                     static_cast<std::streamsize>(payload.size()));
        output.flush();
        if (!output) {
            output.close();
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            throw std::runtime_error("failed while writing profile file");
        }
    }

    std::error_code rename_error;
    std::filesystem::rename(temporary, destination, rename_error);
    if (rename_error) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw std::runtime_error("could not publish profile file: " + rename_error.message());
    }
    return key;
}

std::optional<std::vector<std::byte>> loadPayload(
    const std::filesystem::path& root,
    std::string_view extension,
    std::string_view magic,
    std::string_view key) {
    if (!isProfileKey(key)) return std::nullopt;
    const auto path = root / (std::string(key) + std::string(extension));
    std::error_code size_error;
    const auto file_size = std::filesystem::file_size(path, size_error);
    if (size_error || file_size > kMaximumPayloadBytes + 256U) return std::nullopt;

    std::ifstream input(path, std::ios::binary);
    if (!input) return std::nullopt;
    std::string stored_magic, stored_key, size_text;
    if (!std::getline(input, stored_magic) ||
        !std::getline(input, stored_key) ||
        !std::getline(input, size_text) ||
        stored_magic != magic || stored_key != key) return std::nullopt;

    const auto payload_size = parseSize(size_text);
    const auto payload_position = input.tellg();
    if (!payload_size || *payload_size == 0U ||
        *payload_size > kMaximumPayloadBytes ||
        payload_position == std::streampos(-1)) return std::nullopt;
    const auto offset = static_cast<std::uintmax_t>(payload_position);
    if (file_size < offset || file_size - offset != *payload_size) return std::nullopt;

    std::vector<std::byte> payload(*payload_size);
    input.read(reinterpret_cast<char*>(payload.data()),
               static_cast<std::streamsize>(payload.size()));
    if (input.gcount() != static_cast<std::streamsize>(payload.size()) ||
        nyamist::fingerprint(payload) != key) return std::nullopt;
    return payload;
}

void writeStrings(Writer& writer, const std::vector<std::string>& values) {
    if (values.size() > kMaximumCollectionItems) {
        throw std::invalid_argument("profile collection exceeds the item limit");
    }
    writer.u32(static_cast<std::uint32_t>(values.size()));
    for (const auto& value : values) writer.string(value);
}

std::vector<std::string> readStrings(Reader& reader) {
    const auto count = reader.u32();
    if (count > kMaximumCollectionItems) {
        throw std::runtime_error("profile collection exceeds the item limit");
    }
    std::vector<std::string> values;
    values.reserve(count);
    for (std::uint32_t i = 0U; i < count; ++i) values.push_back(reader.string());
    return values;
}

void writeDevice(Writer& w, const DeviceFactsSnapshot& d) {
    w.string(d.manufacturer); w.string(d.brand); w.string(d.model);
    w.string(d.soc_model); w.string(d.hardware); w.string(d.board_platform);
    w.string(d.android_release); w.u32(d.android_api); w.string(d.kernel_release);
    w.string(d.primary_abi); w.string(d.abi_list); w.u32(d.logical_cpu_count);
    w.u64(d.total_memory_bytes); w.u64(d.available_memory_bytes);
    w.boolean(d.memory_stats_reported);
}

DeviceFactsSnapshot readDevice(Reader& r) {
    DeviceFactsSnapshot d;
    d.manufacturer=r.string(); d.brand=r.string(); d.model=r.string();
    d.soc_model=r.string(); d.hardware=r.string(); d.board_platform=r.string();
    d.android_release=r.string(); d.android_api=r.u32(); d.kernel_release=r.string();
    d.primary_abi=r.string(); d.abi_list=r.string(); d.logical_cpu_count=r.u32();
    d.total_memory_bytes=r.u64(); d.available_memory_bytes=r.u64();
    d.memory_stats_reported=r.boolean();
    if (d.logical_cpu_count > 256U ||
        (d.memory_stats_reported && d.available_memory_bytes > d.total_memory_bytes)) {
        throw std::runtime_error("profile contains invalid device facts");
    }
    return d;
}

void writeVulkan(Writer& w, const VulkanCapabilitySnapshot& v) {
    w.string(v.enumeration_status); w.string(v.failure_reason); w.u32(v.device_count);
    w.string(v.device_name); w.u32(v.vendor_id); w.u32(v.device_id);
    w.string(v.instance_api_version); w.string(v.device_api_version);
    w.string(v.driver_name); w.string(v.driver_info); w.string(v.driver_version);
    w.boolean(v.extensions_reported); writeStrings(w, v.supported_extensions);
    if (v.features.size() > kMaximumCollectionItems) {
        throw std::invalid_argument("Vulkan feature map exceeds the item limit");
    }
    w.u32(static_cast<std::uint32_t>(v.features.size()));
    for (const auto& [name, enabled] : v.features) {
        w.string(name); w.boolean(enabled);
    }
}

VulkanCapabilitySnapshot readVulkan(Reader& r) {
    VulkanCapabilitySnapshot v;
    v.enumeration_status=r.string(); v.failure_reason=r.string(); v.device_count=r.u32();
    v.device_name=r.string(); v.vendor_id=r.u32(); v.device_id=r.u32();
    v.instance_api_version=r.string(); v.device_api_version=r.string();
    v.driver_name=r.string(); v.driver_info=r.string(); v.driver_version=r.string();
    v.extensions_reported=r.boolean(); v.supported_extensions=readStrings(r);
    const auto count=r.u32();
    if (count > kMaximumCollectionItems) throw std::runtime_error("Vulkan feature map exceeds the item limit");
    for (std::uint32_t i=0U;i<count;++i) {
        auto name=r.string();
        const auto inserted=v.features.emplace(std::move(name),r.boolean());
        if (!inserted.second) throw std::runtime_error("duplicate Vulkan feature in profile");
    }
    return v;
}

std::uint8_t metricValue(TelemetryMetric metric) {
    switch (metric) {
    case TelemetryMetric::cpu_utilization_percent: return 0U;
    case TelemetryMetric::gpu_utilization_percent: return 1U;
    case TelemetryMetric::memory_available_bytes: return 2U;
    case TelemetryMetric::gpu_temperature_millicelsius: return 3U;
    case TelemetryMetric::battery_temperature_millicelsius: return 4U;
    case TelemetryMetric::frame_time_p95_microseconds: return 5U;
    }
    throw std::invalid_argument("unknown telemetry metric");
}

TelemetryMetric parseMetric(std::uint8_t value) {
    switch(value) {
    case 0U: return TelemetryMetric::cpu_utilization_percent;
    case 1U: return TelemetryMetric::gpu_utilization_percent;
    case 2U: return TelemetryMetric::memory_available_bytes;
    case 3U: return TelemetryMetric::gpu_temperature_millicelsius;
    case 4U: return TelemetryMetric::battery_temperature_millicelsius;
    case 5U: return TelemetryMetric::frame_time_p95_microseconds;
    default: throw std::runtime_error("profile contains an unknown telemetry metric");
    }
}

bool validReading(const DeviceFactsSnapshot& device,
                  const TelemetryReading& reading,
                  std::uint64_t aggregated_at,
                  std::uint64_t freshness_window,
                  bool should_be_fresh) {
    if (reading.source.empty() || reading.observed_at_unix_ms == 0U ||
        reading.observed_at_unix_ms > aggregated_at || freshness_window == 0U) return false;
    switch (reading.metric) {
    case TelemetryMetric::cpu_utilization_percent:
    case TelemetryMetric::gpu_utilization_percent:
        if (reading.value < 0 || reading.value > 100) return false;
        break;
    case TelemetryMetric::memory_available_bytes:
        if (reading.value < 0 ||
            (device.memory_stats_reported &&
             static_cast<std::uint64_t>(reading.value) > device.total_memory_bytes)) return false;
        break;
    case TelemetryMetric::gpu_temperature_millicelsius:
    case TelemetryMetric::battery_temperature_millicelsius:
        if (reading.value < -40000 || reading.value > 200000) return false;
        break;
    case TelemetryMetric::frame_time_p95_microseconds:
        if (reading.value <= 0 || reading.value > 120000000) return false;
        break;
    }
    const auto age = aggregated_at - reading.observed_at_unix_ms;
    return should_be_fresh ? age <= freshness_window : age > freshness_window;
}

void writeReadingMap(Writer& w, const std::map<TelemetryMetric, TelemetryReading>& values) {
    if (values.size() > 6U) throw std::invalid_argument("telemetry map has too many metrics");
    w.u32(static_cast<std::uint32_t>(values.size()));
    for (const auto& [metric, reading] : values) {
        w.u8(metricValue(metric)); w.i64(reading.value);
        w.u64(reading.observed_at_unix_ms); w.string(reading.source);
    }
}

std::map<TelemetryMetric, TelemetryReading> readReadingMap(
    Reader& r, const DeviceFactsSnapshot& device,
    std::uint64_t aggregated_at, std::uint64_t freshness_window, bool fresh) {
    const auto count=r.u32();
    if (count > 6U) throw std::runtime_error("telemetry map has too many metrics");
    std::map<TelemetryMetric, TelemetryReading> values;
    for (std::uint32_t i=0U;i<count;++i) {
        TelemetryReading reading;
        reading.metric=parseMetric(r.u8()); reading.value=r.i64();
        reading.observed_at_unix_ms=r.u64(); reading.source=r.string();
        if (!validReading(device,reading,aggregated_at,freshness_window,fresh) ||
            !values.emplace(reading.metric,reading).second) {
            throw std::runtime_error("profile contains invalid or duplicate telemetry");
        }
    }
    return values;
}

void validateSession(const DeviceSessionSnapshot& session) {
    const auto& probe=session.probe();
    const auto& device=probe.device;
    const auto& telemetry=session.telemetry();
    if (session.schemaVersion()!=1U || probe.schema_version!=1U ||
        probe.evidence_source.empty() || device.model.empty() ||
        device.logical_cpu_count>256U ||
        (device.memory_stats_reported && device.available_memory_bytes>device.total_memory_bytes) ||
        telemetry.schema_version!=1U ||
        telemetry.aggregated_at_unix_ms!=session.capturedAtUnixMs() ||
        telemetry.freshness_window_ms==0U) {
        throw std::invalid_argument("session does not satisfy the schema-v1 profile contract");
    }
    for (const auto& [metric,reading] : telemetry.fresh_readings) {
        (void)metric;
        if (!validReading(device,reading,telemetry.aggregated_at_unix_ms,
                          telemetry.freshness_window_ms,true))
            throw std::invalid_argument("session contains invalid fresh telemetry");
    }
    for (const auto& [metric,reading] : telemetry.stale_readings) {
        (void)metric;
        if (!validReading(device,reading,telemetry.aggregated_at_unix_ms,
                          telemetry.freshness_window_ms,false))
            throw std::invalid_argument("session contains invalid stale telemetry");
    }
}

void writeSession(Writer& w, const DeviceSessionSnapshot& session) {
    validateSession(session);
    const auto& p=session.probe();
    const auto& t=session.telemetry();
    w.u32(1U); w.u64(session.capturedAtUnixMs());
    w.u32(p.schema_version); w.string(p.captured_at); w.string(p.evidence_source);
    writeDevice(w,p.device); writeVulkan(w,p.vulkan);
    w.u32(t.schema_version); w.u64(t.aggregated_at_unix_ms);
    w.u64(t.freshness_window_ms);
    writeReadingMap(w,t.fresh_readings); writeReadingMap(w,t.stale_readings);
    w.u64(t.rejected_readings); writeStrings(w,t.diagnostics);
}

DeviceSessionSnapshot readSession(Reader& r) {
    const auto session_schema=r.u32();
    if (session_schema!=1U) throw std::runtime_error("unknown session profile schema version");
    const auto captured_at=r.u64();
    DeviceProbeSnapshot probe;
    probe.schema_version=r.u32(); probe.captured_at=r.string();
    probe.evidence_source=r.string(); probe.device=readDevice(r); probe.vulkan=readVulkan(r);
    TelemetrySnapshot telemetry;
    telemetry.schema_version=r.u32(); telemetry.aggregated_at_unix_ms=r.u64();
    telemetry.freshness_window_ms=r.u64();
    telemetry.fresh_readings=readReadingMap(
        r,probe.device,telemetry.aggregated_at_unix_ms,telemetry.freshness_window_ms,true);
    telemetry.stale_readings=readReadingMap(
        r,probe.device,telemetry.aggregated_at_unix_ms,telemetry.freshness_window_ms,false);
    telemetry.rejected_readings=r.u64();
    if (telemetry.rejected_readings>1000000U)
        throw std::runtime_error("profile rejected-reading count exceeds the limit");
    telemetry.diagnostics=readStrings(r);
    DeviceSessionSnapshot session(std::move(probe),std::move(telemetry),captured_at);
    validateSession(session);
    return session;
}

bool validCompatibility(const RuntimeCompatibilityProfile& p) {
    const bool known_api=p.graphics_api==GraphicsApi::d3d11 || p.graphics_api==GraphicsApi::d3d12;
    const auto valid_level=[](ValidationLevel v) {
        return v==ValidationLevel::unavailable || v==ValidationLevel::discovered ||
               v==ValidationLevel::smoke_tested || v==ValidationLevel::sustained_tested;
    };
    if (p.schema_version!=1U || p.captured_at_unix_ms==0U ||
        !isProfileKey(p.device_profile_key) || !known_api ||
        p.translator_id.empty() || p.vulkan_driver_id.empty() ||
        !valid_level(p.translator_validation) || !valid_level(p.vulkan_driver_validation)) return false;
    if (p.smoke_test_allowed && !p.blockers.empty()) return false;
    if (p.production_ready &&
        (!p.smoke_test_allowed ||
         p.translator_validation!=ValidationLevel::sustained_tested ||
         p.vulkan_driver_validation!=ValidationLevel::sustained_tested)) return false;
    return p.selected_components.size()<=kMaximumCollectionItems &&
           p.blockers.size()<=kMaximumCollectionItems &&
           p.warnings.size()<=kMaximumCollectionItems &&
           p.capabilities.size()<=kMaximumCollectionItems;
}

void writeCompatibility(Writer& w, const RuntimeCompatibilityProfile& p) {
    if (!validCompatibility(p))
        throw std::invalid_argument("compatibility profile does not satisfy schema-v1 requirements");
    w.u32(p.schema_version); w.u64(p.captured_at_unix_ms); w.string(p.device_profile_key);
    w.u8(p.graphics_api==GraphicsApi::d3d11?0U:1U);
    w.string(p.translator_id); w.string(p.translator_version);
    w.u8(static_cast<std::uint8_t>(p.translator_validation));
    w.string(p.vulkan_driver_id); w.string(p.vulkan_driver_version);
    w.u8(static_cast<std::uint8_t>(p.vulkan_driver_validation));
    w.boolean(p.smoke_test_allowed); w.boolean(p.production_ready);
    writeStrings(w,p.selected_components); writeStrings(w,p.blockers);
    writeStrings(w,p.warnings); writeStrings(w,p.capabilities);
}

RuntimeCompatibilityProfile readCompatibility(Reader& r) {
    RuntimeCompatibilityProfile p;
    p.schema_version=r.u32(); p.captured_at_unix_ms=r.u64(); p.device_profile_key=r.string();
    const auto api=r.u8();
    if (api>1U) throw std::runtime_error("unknown graphics API in compatibility profile");
    p.graphics_api=api==0U?GraphicsApi::d3d11:GraphicsApi::d3d12;
    p.translator_id=r.string(); p.translator_version=r.string();
    const auto translator_level=r.u8();
    const auto valid_level=[](std::uint8_t v){return v<=3U;};
    if (!valid_level(translator_level)) throw std::runtime_error("invalid translator validation level");
    p.translator_validation=static_cast<ValidationLevel>(translator_level);
    p.vulkan_driver_id=r.string(); p.vulkan_driver_version=r.string();
    const auto driver_level=r.u8();
    if (!valid_level(driver_level)) throw std::runtime_error("invalid driver validation level");
    p.vulkan_driver_validation=static_cast<ValidationLevel>(driver_level);
    p.smoke_test_allowed=r.boolean(); p.production_ready=r.boolean();
    p.selected_components=readStrings(r); p.blockers=readStrings(r);
    p.warnings=readStrings(r); p.capabilities=readStrings(r);
    if (!validCompatibility(p))
        throw std::runtime_error("compatibility profile violates schema-v1 requirements");
    return p;
}

std::vector<std::string> listProfiles(
    const std::filesystem::path& root,
    std::string_view extension,
    const std::function<bool(std::string_view)>& validator) {
    std::vector<std::string> result;
    std::error_code error;
    if (!std::filesystem::exists(root,error)) return result;
    if (error) return result;
    for (std::filesystem::directory_iterator it(root,error),end;
         !error && it!=end; it.increment(error)) {
        if (!it->is_regular_file(error) || error) continue;
        if (it->path().extension()!=extension) continue;
        const auto key=it->path().stem().string();
        if (isProfileKey(key) && validator(key)) result.push_back(key);
    }
    std::sort(result.begin(),result.end());
    return result;
}

} // namespace

RuntimeProfileStore::RuntimeProfileStore(std::filesystem::path root)
    : root_(std::move(root)) {}

std::string RuntimeProfileStore::saveSession(const DeviceSessionSnapshot& session) const {
    Writer writer;
    writeSession(writer,session);
    if (writer.bytes().size()>kMaximumPayloadBytes)
        throw std::invalid_argument("serialized session profile exceeds the 1 MiB limit");
    return savePayload(root_,".nys",kSessionMagic,writer.bytes());
}

std::optional<DeviceSessionSnapshot> RuntimeProfileStore::loadSession(
    std::string_view profile_key) const {
    const auto payload=loadPayload(root_,".nys",kSessionMagic,profile_key);
    if (!payload) return std::nullopt;
    try {
        Reader reader(*payload);
        auto session=readSession(reader);
        if (!reader.finished()) return std::nullopt;
        return session;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

std::string RuntimeProfileStore::saveCompatibility(
    const RuntimeCompatibilityProfile& profile) const {
    Writer writer;
    writeCompatibility(writer,profile);
    return savePayload(root_,".nyc",kCompatibilityMagic,writer.bytes());
}

std::optional<RuntimeCompatibilityProfile> RuntimeProfileStore::loadCompatibility(
    std::string_view profile_key) const {
    const auto payload=loadPayload(root_,".nyc",kCompatibilityMagic,profile_key);
    if (!payload) return std::nullopt;
    try {
        Reader reader(*payload);
        auto profile=readCompatibility(reader);
        if (!reader.finished()) return std::nullopt;
        return profile;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

std::vector<std::string> RuntimeProfileStore::listSessionProfiles() const {
    return listProfiles(root_,".nys",[this](std::string_view key){
        return loadSession(key).has_value();
    });
}

std::vector<std::string> RuntimeProfileStore::listCompatibilityProfiles() const {
    return listProfiles(root_,".nyc",[this](std::string_view key){
        return loadCompatibility(key).has_value();
    });
}

} // namespace nyamist::runtime
