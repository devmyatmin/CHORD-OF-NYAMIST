# Deterministic A/B Regression Evaluation

## Purpose

The A/B evaluator compares a baseline compatibility profile with a distinct candidate profile using repeated, sustained runs on the same device and workload. It is a deterministic evidence evaluator, not an optimizer and not an AI model. It never changes a worker limit, environment variable, driver setting, translator setting, or cache.

## Required evidence

The default policy requires at least three baseline runs and three candidate runs. Every run must:

- Use the same non-empty workload ID and stable device-profile ID across both arms.
- Reference a valid compatibility-profile key. All runs within one arm must use the same key, and the baseline and candidate keys must differ.
- Match the baseline hardware identity and Vulkan device/driver identity: manufacturer, brand, model, SoC, Android API, ABI, logical CPU count, Vulkan vendor/device IDs, device name, driver name/version, and device API version.
- Cover at least 300 seconds, with a session capture timestamp that follows the recorded run start and covers the sustained duration.
- Report explicit render/Present success, non-zero frame and Present-call counts, and normal process exit (code 0).
- Explicitly report zero Present failures and zero device-loss events.
- Provide fresh, range-valid p95 frame-time telemetry.

Invalid stability evidence blocks the comparison. Too few repetitions or missing p95 telemetry returns `insufficient_evidence`; neither case is converted into a neutral or positive result.

## Decision rule

The evaluator sorts p95 frame-time measurements for each arm and compares their medians. Lower p95 frame time is better. The default threshold is 500 basis points (5%):

- `improved`: candidate median p95 is more than 5% lower than baseline.
- `no_regression`: the change is within the configured ±5% band.
- `regressed`: candidate median p95 is more than 5% higher than baseline.
- `blocked`: workload/device/profile identity or render/stability evidence is invalid.
- `insufficient_evidence`: repetition count or fresh p95 telemetry is inadequate.

The signed delta is returned in basis points, along with both medians and machine-readable evidence. The default policy cannot be weakened below three repetitions or 300 seconds. The regression threshold is configurable from 0.01% to 50%.

## Interpretation limits

This is a workload-scoped result, not a claim of general game compatibility or production readiness. A three-run median is a simple deterministic guardrail, not a statistical significance test. It does not control background processes, ambient temperature, battery state, refresh rate, or run ordering; the test harness/operator must keep those conditions comparable and record them separately. Do not generalize synthetic-cube results to commercial games. Automatic application remains disabled.

## Tests

The dedicated CTest target covers improvement, neutral/no-regression, regression, insufficient repetitions, missing p95 telemetry, failed Present evidence, workload mismatch, identical baseline/candidate profile keys, and rejection of policies below the sustained-test minimum.
