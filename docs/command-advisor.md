# Runtime Command Advisor

The current Command Advisor is a deterministic rule-based proposal engine,
not an AI model. It consumes only a fresh CPU-utilization reading, the device's
reported logical CPU count, and a Pipeline Gate decision.

It proposes a shader-compilation worker budget in the fixed range 1–4, further
capped at clamp(logical_cpu_count / 2, 1, 4). At CPU utilization >=85%, it
reduces by one step when possible. At <=55%, it increases by one step when
below the CPU-derived ceiling. Otherwise it holds. If the Pipeline Gate blocks
the session or fresh CPU telemetry is missing, no adjustment is proposed.

Every proposal carries a policy ID/version, the current value, proposed value,
hard bounds, supporting source/timestamp evidence, expected benefit, risk, and
rollback value. Automatic application is always disabled. These initial
thresholds are conservative policy defaults for controlled experiments, not
claims of measured optimal settings; tune them only with repeatable device
A/B evidence.

The advisor does not write configuration, environment variables, driver
settings, translator files, kernel controls, or upstream-owned caches. A future
AI model may propose values, but deterministic validation and A/B gates must
remain authoritative.
