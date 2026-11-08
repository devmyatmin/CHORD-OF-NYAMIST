# Pipeline Gate

PipelineGate consumes a RuntimePlan and an immutable DeviceSessionSnapshot.
It produces a deterministic blocked, warning, or ready decision.

A session is accepted only when its capture timestamp is not in the future
and its age does not exceed the caller-supplied maximum. Vulkan readiness
requires a successful physical-device enumeration with a non-zero device
count. Runtime prerequisites and the coordinator's smoke-test authorization
must both be present.

Missing or stale telemetry produces warnings, not false graphics failures.
Stale telemetry is excluded from fresh policy input by TelemetryAggregator.
A smoke-test decision is not production approval: production_ready remains
false unless the RuntimeCoordinator independently reports sustained validation
for the required components and the session passes the gate.

The gate is a policy/evidence evaluator. It does not launch workloads, select
drivers, write configuration, modify translator binaries, or enter the
per-frame rendering path. Its clock and freshness threshold are explicit
inputs, making decisions deterministic in tests.
