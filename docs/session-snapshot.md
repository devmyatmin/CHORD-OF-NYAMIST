# Device Session Snapshot and Telemetry Aggregation

NYAMIST's session layer joins one parsed DeviceProbeSnapshot with one
TelemetrySnapshot. DeviceSessionSnapshot is immutable after construction and
carries schema version 1, a capture timestamp, the original device/Vulkan
observations, and the aggregated telemetry view.

## Telemetry contract

TelemetryAggregator accepts typed observations for CPU/GPU utilization,
available memory, GPU/battery temperature, and p95 frame time. Each sample
must include its observation timestamp and source. The caller supplies the
aggregation clock and freshness window explicitly; the code does not read the
wall clock implicitly.

The aggregator keeps the newest sample per metric. Readings in the future,
without a source/timestamp, or outside metric bounds are rejected. Available
memory cannot exceed the physical-memory total when that total was reported.
Older samples are retained under stale_readings for diagnostics, but are not
placed in fresh_readings and therefore are not valid current-policy inputs.
Equally-new conflicting values for the same metric are omitted rather than
arbitrarily selected. Identical equally-new values select a deterministic
source identity.

Metric units are fixed by the enum names: utilization in percent (0–100),
memory in bytes, temperature in milli-degrees Celsius, and p95 frame time in
microseconds. Missing metrics remain absent; they are never synthesized.

## Boundary

This is an in-memory NYAMIST control-plane model. It is not a shared-memory ABI,
not a driver interface, and not a per-frame service. A future Android collector
can feed it observations from an authorized app/game-container context.
Persistence and cross-process transport require a separately versioned
contract and are not implied by this implementation.
