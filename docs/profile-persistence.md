# Versioned Runtime Profile Persistence

## Scope

`RuntimeProfileStore` persists NYAMIST-owned control-plane evidence only:

- A schema-v1 `DeviceSessionSnapshot`, including normalized device facts, Vulkan capability facts, fresh and stale telemetry, rejection counts, and diagnostics.
- A schema-v1 `RuntimeCompatibilityProfile`, linking a saved session key to one graphics API, translator identity/version/validation level, Vulkan driver identity/version/validation level, selected components, blockers, warnings, and reported capabilities.

The store does not launch a game, load a Vulkan ICD, change translator settings, collect telemetry, or modify Android system configuration. It does not read or write DXVK, VKD3D-Proton, the selected Android Vulkan driver/Mesa, or vendor-driver caches.

## On-disk format

Each profile is a separate content-addressed file under the configured NYAMIST-owned directory:

- `<16-lowercase-hex-fingerprint>.nys` — session profile.
- `<16-lowercase-hex-fingerprint>.nyc` — compatibility profile.

The envelope is a UTF-8 header followed by a bounded binary payload:

```text
NYAMIST_SESSION_PROFILE_V1
<profile-key>
<payload-size-in-bytes>
<binary-payload>
```

Compatibility files use `NYAMIST_COMPAT_PROFILE_V1`. The payload is encoded with fixed-width little-endian integers and length-prefixed strings. Schema versions are explicit. The profile key is the FNV-1a 64-bit fingerprint of the exact payload, formatted as 16 lowercase hexadecimal characters. It detects accidental corruption and enables content addressing; it is not a cryptographic authenticity mechanism.

## Validation and limits

- Maximum serialized payload: 1 MiB per profile.
- Maximum string: 4096 bytes.
- Maximum repeated collection: 1024 entries.
- Maximum CPU count: 256 logical CPUs.
- Telemetry metric IDs and enum values are checked; duplicate or unknown metrics are rejected.
- Fresh/stale classification is revalidated against the persisted aggregation timestamp and freshness window.
- Device memory, utilization, temperature, and frame-time values are range-checked.
- Compatibility profiles must reference a syntactically valid session key. A profile cannot claim smoke-test permission while listing blockers. Production readiness requires smoke-test permission and sustained-test evidence for both translator and Vulkan driver.
- File magic, key, declared payload length, exact end-of-file, schema, and payload fingerprint must all match. Truncated, oversized, malformed, unknown-schema, and checksum-mismatched records are rejected.
- Temporary files are written before publication. Identical payloads reuse the same content key.

The list methods return only records that pass full load-time validation. Invalid records are omitted; the store does not silently repair evidence.

## API example

```cpp
nyamist::runtime::RuntimeProfileStore profiles(
    std::filesystem::path("nyamist/profiles"));

const auto session_key = profiles.saveSession(session);

nyamist::runtime::RuntimeCompatibilityProfile compatibility;
compatibility.captured_at_unix_ms = session.capturedAtUnixMs();
compatibility.device_profile_key = session_key;
compatibility.graphics_api = nyamist::runtime::GraphicsApi::d3d11;
compatibility.translator_id = "dxvk";
compatibility.translator_version = "reported-version";
compatibility.vulkan_driver_id = "android-vulkan";
compatibility.vulkan_driver_version = "reported-version";
// Populate validation levels, readiness decision, selected components,
// blockers, warnings, and capabilities from the actual inspected evidence.
const auto compatibility_key = profiles.saveCompatibility(compatibility);

const auto restored_session = profiles.loadSession(session_key);
const auto restored_compatibility =
    profiles.loadCompatibility(compatibility_key);
```

The store accepts already-constructed snapshots and compatibility summaries. Evidence collection and the policy decision remain separate responsibilities. It must not be used to turn discovered-only components into smoke-tested or production-ready components.

## Tests

The dedicated CTest target covers session and compatibility round trips, content-address idempotence, fresh/stale telemetry preservation, profile listing, invalid readiness combinations, path-like keys, and checksum-corruption rejection.
