# NYAMIST standalone runtime validation runbook

## Target

- REDMI K80 Ultra
- MediaTek MT6991
- Immortalis-G925 MC12
- Android 16 / API 36
- arm64-v8a
- Termux development environment

## Architecture under test

D3D11 sample -> NYAMIST D3D11 runtime -> NYAMIST Vulkan backend -> Android Vulkan loader -> installed vendor GPU driver.

PanPlay, PanVK, and DXVK are not part of this test path. Do not install or select them for NYAMIST validation. Do not replace the vendor driver or modify protected Android system/vendor partitions.

## 1. Build the repository

For a fresh checkout:

~~~sh
pkg update
pkg install git clang cmake ninja pkg-config python
git clone https://github.com/devmyatmin/CHORD-OF-NYAMIST.git
cd CHORD-OF-NYAMIST
~~~

For an existing checkout:

~~~sh
cd ~/CHORD-OF-NYAMIST
git switch feature/nyamist-owned-runtime
git pull --ff-only origin feature/nyamist-owned-runtime
~~~

Build the portable core and tests:

~~~sh
cmake -S . -B build-termux -G Ninja -DCMAKE_BUILD_TYPE=Release -DNYAMIST_BUILD_TESTS=ON
cmake --build build-termux
ctest --test-dir build-termux --output-on-failure
~~~

A successful build proves only that the source compiles and its host tests pass.

## 2. Vulkan backend checks

The NYAMIST Vulkan backend must report, from live calls:

- Loader/API version.
- Physical-device name, vendor ID, device ID and API version.
- Queue-family properties.
- Selected graphics queue family.
- Logical-device creation result.
- Enabled extensions and features (only those actually queried and enabled).

Do not substitute imported device facts or old PanVK reports for live Vulkan results.

## 3. Render smoke test

Run the NYAMIST-owned offscreen test:

- Create a color image and required memory.
- Record a clear and triangle draw.
- Submit to the selected graphics queue.
- Wait using a fence.
- Copy the result to host-visible memory.
- Compare selected pixels against deterministic expected values.

Record the test binary revision, device identity, Vulkan result codes, and pixel comparison result.

## 4. Android presentation test

On an app-owned Android surface:

- Create the surface and query present support.
- Create a swapchain using supported formats and present modes.
- Acquire, render, submit, and present repeatedly.
- Exercise pause/resume and orientation/size changes.
- Recreate the swapchain after `VK_SUBOPTIMAL_KHR` or `VK_ERROR_OUT_OF_DATE_KHR`.
- Record acquire/submit/present results, fence completion, and recreation count.

A headless test cannot establish Android WSI correctness.

## 5. D3D11 translator test

Start with a minimal D3D11 sample that uses only the implemented subset:

- Device and immediate context.
- Vertex buffer and input layout.
- Vertex and pixel shaders.
- Render target.
- One draw and Present.

Confirm from NYAMIST logs that the NYAMIST D3D11 interfaces and Vulkan backend handled the work. Do not infer this from an external translator's log.

## 6. Sustained application validation

Only after the sample passes:

- Fix application version, scene, resolution, and settings.
- Run the same workload for at least 300 seconds.
- Record visual correctness, frame-time p50/p95/p99, memory, temperature, crashes, and device-loss events where available.
- Repeat a cold and warm run.
- Keep unsupported features and failures explicit.

A synthetic sample is not a commercial-game compatibility result. Do not label NYAMIST game-compatible until a named real game passes repeatable sustained rendering.
