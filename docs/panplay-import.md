# PanPlay session and performance import

## Supported exports

`tools/import_panplay.py` reads the two ZIP formats emitted by PanPlay:

1. **SessionLogs ZIP**: root `manifest.json`, a session directory containing `session.json`, `device-facts.json`, log files, and optional system files. The manifest contains a `files` array with each staged file's path, byte size, and SHA-256 digest.
2. **PerfRecorder ZIP** (optional): one `<id>.json` summary and optional `<id>.csv` / `<id>-sys.csv` samples. Its summary ID must match the session's `perfId`.

The importer follows PanPlay's exported field names, including `manifest.device`, `manifest.android`, `manifest.driver`, `session.manifestInfo`, `session.deviceLost`, `session.durationSec`, and PerfRecorder's `dxvkStats`, `frames`, `frametimeP50Ms`, `frametimeP95Ms`, `frametimeP99Ms`, GPU, CPU, memory, battery-temperature, and power summaries.

## Usage

Import a SessionLogs ZIP and its matching optional performance ZIP:

```sh
python3 tools/import_panplay.py panplay-session.zip \
  --performance-zip panplay-performance.zip \
  --output build/panplay-evidence.json \
  --report-output build/panplay-translator.env
```

The normalized JSON evidence sidecar is always produced. The translator report is optional and is generated only when the session archive identifies exactly one supported translator/API from its log inventory:

- a D3D11-specific DXVK log -> `translator_id=dxvk`, `api=d3d11`;
- `vkd3d.log` -> `translator_id=vkd3d-proton`, `api=d3d12`.

D3D9-only sessions remain in the JSON sidecar and do not generate a D3D11/D3D12 report. If D3D11 and VKD3D evidence appear together, translator selection is treated as ambiguous and no adapter report is generated.

## Evidence and validation boundaries

The importer validates the ZIP structure without extracting any archive member. It rejects absolute/traversal paths, duplicate names, encrypted or non-regular entries, excessive member counts, oversized archives/members, manifest/file-list mismatches, byte-size mismatches, and SHA-256 mismatches. It parses JSON only after enforcing size limits.

The generated adapter report deliberately sets `device_created=false` and `render_present_passed=false`. PanPlay's PerfRecorder reports DXVK frame statistics, not a separately instrumented count of successful/failed Vulkan Present calls. PanPlay's `deviceLost` flag is a log scan, not an explicit zero-event counter. Those values are preserved as observations in the sidecar, but are not promoted to explicit translator validation fields.

Consequently, an imported report remains **discovered**. It cannot become `smoke_tested` or `sustained_tested` until a separate instrumentation source supplies device-creation confirmation, render/Present success, non-zero Present calls, explicit zero Present failures, explicit zero device-loss events, and a normal process exit. A p95 value is normalized from milliseconds to microseconds for later performance analysis, but it is not by itself a successful-render verdict.

The importer's bounds are 64 MiB compressed archive size, 128 MiB aggregate expanded size, 32 MiB per member, 2 MiB per JSON member, and 4096 ZIP entries. No files are extracted and no driver, translator, settings, or upstream cache is modified.

## Tests

`tools/tests/test_import_panplay.py` covers D3D11 and VKD3D identity detection, p95 normalization, discovery-only adapter output, unsupported/ambiguous API selection, manifest hash mismatch, performance/session ID mismatch, non-finite metrics, missing DXVK frame statistics, and duplicate ZIP members.
