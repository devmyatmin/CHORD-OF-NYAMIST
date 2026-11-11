#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h>
#include <psapi.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <iterator>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

namespace {
constexpr UINT kWidth = 1280;
constexpr UINT kHeight = 720;
constexpr wchar_t kClassName[] = L"NYAMIST_DX11_LIVE_CUBE";

struct Vertex { XMFLOAT3 position; XMFLOAT3 color; };
struct ConstantBuffer { XMFLOAT4X4 mvp; };

const char* kShader = R"(
cbuffer Scene : register(b0) { float4x4 mvp; };
struct VSIn { float3 position : POSITION; float3 color : COLOR; };
struct VSOut { float4 position : SV_POSITION; float3 color : COLOR; };
VSOut VSMain(VSIn input) {
  VSOut output;
  output.position = mul(float4(input.position, 1.0), mvp);
  output.color = input.color;
  return output;
}
float4 PSMain(VSOut input) : SV_TARGET { return float4(input.color, 1.0); }
)";

class App {
public:
    HWND window{};
    HWND hud{};
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGISwapChain> swapChain;
    ComPtr<ID3D11RenderTargetView> rtv;
    ComPtr<ID3D11Texture2D> depthTexture;
    ComPtr<ID3D11DepthStencilView> dsv;
    ComPtr<ID3D11VertexShader> vertexShader;
    ComPtr<ID3D11PixelShader> pixelShader;
    ComPtr<ID3D11InputLayout> inputLayout;
    ComPtr<ID3D11Buffer> vertexBuffer;
    ComPtr<ID3D11Buffer> indexBuffer;
    ComPtr<ID3D11Buffer> constantBuffer;
    D3D_FEATURE_LEVEL featureLevel{D3D_FEATURE_LEVEL_9_1};
    std::wstring adapterName = L"Unknown";
    SIZE_T dedicatedVideoMemory{};
    LARGE_INTEGER qpcFrequency{};
    LARGE_INTEGER previousFrame{};
    LARGE_INTEGER sessionStart{};
    LARGE_INTEGER previousStats{};
    uint64_t previousCpu100ns{};
    std::array<double, 600> frameMs{};
    size_t frameCursor{};
    size_t frameSamples{};
    LARGE_INTEGER lastMetricUpdate{};
    std::ofstream csv;
    uint64_t frameCount{};
    uint64_t stutterCount{};
    double currentFps{};
    double avgFps{};
    double low1{};
    double low01{};
    double currentMs{};
    double avgMs{};
    double p50{}, p95{}, p99{};
    double cpuPercent{};
    SIZE_T ramBytes{};
    double elapsedSeconds{};
    int fpsLimit = 60;

    bool initialize(HWND hwnd) {
        window = hwnd;
        QueryPerformanceFrequency(&qpcFrequency);

        DXGI_SWAP_CHAIN_DESC swap{};
        swap.BufferCount = 2;
        swap.BufferDesc.Width = kWidth;
        swap.BufferDesc.Height = kHeight;
        swap.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swap.BufferDesc.RefreshRate.Numerator = 0;
        swap.BufferDesc.RefreshRate.Denominator = 0;
        swap.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swap.OutputWindow = hwnd;
        swap.SampleDesc.Count = 1;
        swap.Windowed = TRUE;
        swap.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
        HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
            0, levels, static_cast<UINT>(std::size(levels)), D3D11_SDK_VERSION,
            &swap, &swapChain, &device, &featureLevel, &context);
        if (hr == E_INVALIDARG) {
            const D3D_FEATURE_LEVEL fallback[] = {D3D_FEATURE_LEVEL_11_0};
            hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                0, fallback, 1, D3D11_SDK_VERSION, &swap, &swapChain, &device, &featureLevel, &context);
        }
        if (FAILED(hr)) return false;
        if (!createTargets(kWidth, kHeight) || !createPipeline()) return false;
        queryAdapter();
        createHud();
        QueryPerformanceCounter(&sessionStart);
        previousFrame = sessionStart;
        previousStats = sessionStart;
        lastMetricUpdate = sessionStart;
        FILETIME creation{}, exit{}, kernel{}, user{};
        if (GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user)) {
            previousCpu100ns = (static_cast<uint64_t>(kernel.dwHighDateTime) << 32 | kernel.dwLowDateTime) +
                               (static_cast<uint64_t>(user.dwHighDateTime) << 32 | user.dwLowDateTime);
        }
        csv.open("NYAMIST-DX11-Live-Cube.csv", std::ios::out | std::ios::trunc);
        if (csv) csv << "frame,elapsed_s,current_fps,average_fps,1pct_low,0_1pct_low,frame_ms,p50_ms,p95_ms,p99_ms,cpu_percent_device,working_set_mb,feature_level,adapter\n";
        return true;
    }

    bool createTargets(UINT width, UINT height) {
        ComPtr<ID3D11Texture2D> backBuffer;
        HRESULT hr = swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
        if (FAILED(hr)) return false;
        hr = device->CreateRenderTargetView(backBuffer.Get(), nullptr, &rtv);
        if (FAILED(hr)) return false;
        D3D11_TEXTURE2D_DESC depth{};
        depth.Width = width; depth.Height = height; depth.MipLevels = 1; depth.ArraySize = 1;
        depth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; depth.SampleDesc.Count = 1;
        depth.Usage = D3D11_USAGE_DEFAULT; depth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        hr = device->CreateTexture2D(&depth, nullptr, &depthTexture);
        if (FAILED(hr)) return false;
        return SUCCEEDED(device->CreateDepthStencilView(depthTexture.Get(), nullptr, &dsv));
    }

    bool createPipeline() {
        ComPtr<ID3DBlob> vs, ps, errors;
        HRESULT hr = D3DCompile(kShader, strlen(kShader), nullptr, nullptr, nullptr,
            "VSMain", "vs_4_0", D3DCOMPILE_ENABLE_STRICTNESS, 0, &vs, &errors);
        if (FAILED(hr)) return false;
        hr = D3DCompile(kShader, strlen(kShader), nullptr, nullptr, nullptr,
            "PSMain", "ps_4_0", D3DCOMPILE_ENABLE_STRICTNESS, 0, &ps, &errors);
        if (FAILED(hr)) return false;
        if (FAILED(device->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, &vertexShader))) return false;
        if (FAILED(device->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(), nullptr, &pixelShader))) return false;
        const D3D11_INPUT_ELEMENT_DESC layout[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}
        };
        if (FAILED(device->CreateInputLayout(layout, 2, vs->GetBufferPointer(), vs->GetBufferSize(), &inputLayout))) return false;

        const std::array<Vertex, 8> vertices{{
            {{-1,-1,-1},{1,0.2f,0.2f}}, {{-1,1,-1},{1,0.6f,0.1f}},
            {{1,1,-1},{1,1,0.2f}}, {{1,-1,-1},{0.2f,1,0.3f}},
            {{-1,-1,1},{0.1f,0.8f,1}}, {{-1,1,1},{0.3f,0.4f,1}},
            {{1,1,1},{0.8f,0.3f,1}}, {{1,-1,1},{1,0.3f,0.7f}}
        }};
        const std::array<uint16_t, 36> indices{{
            0,1,2, 0,2,3, 4,7,6, 4,6,5, 4,5,1, 4,1,0,
            3,2,6, 3,6,7, 1,5,6, 1,6,2, 4,0,3, 4,3,7
        }};
        D3D11_BUFFER_DESC bd{}; bd.Usage = D3D11_USAGE_DEFAULT; bd.ByteWidth = sizeof(vertices); bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA init{}; init.pSysMem = vertices.data();
        if (FAILED(device->CreateBuffer(&bd, &init, &vertexBuffer))) return false;
        bd.ByteWidth = sizeof(indices); bd.BindFlags = D3D11_BIND_INDEX_BUFFER; init.pSysMem = indices.data();
        if (FAILED(device->CreateBuffer(&bd, &init, &indexBuffer))) return false;
        bd.ByteWidth = sizeof(ConstantBuffer); bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER; init.pSysMem = nullptr;
        return SUCCEEDED(device->CreateBuffer(&bd, nullptr, &constantBuffer));
    }

    void queryAdapter() {
        ComPtr<IDXGIDevice> dxgiDevice;
        ComPtr<IDXGIAdapter> adapter;
        if (SUCCEEDED(device.As(&dxgiDevice)) && SUCCEEDED(dxgiDevice->GetAdapter(&adapter))) {
            DXGI_ADAPTER_DESC desc{};
            if (SUCCEEDED(adapter->GetDesc(&desc))) {
                adapterName = desc.Description;
                dedicatedVideoMemory = desc.DedicatedVideoMemory;
            }
        }
    }

    void createHud() {
        hud = CreateWindowExW(WS_EX_TRANSPARENT, L"STATIC", L"Starting DX11 workload...",
            WS_CHILD | WS_VISIBLE | SS_LEFT, 14, 14, 560, 190, window, nullptr,
            GetModuleHandleW(nullptr), nullptr);
        SendMessageW(hud, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(ANSI_FIXED_FONT)), TRUE);
    }

    static double percentile(std::vector<double> values, double p) {
        if (values.empty()) return 0.0;
        std::sort(values.begin(), values.end());
        const size_t i = std::min(values.size()-1, static_cast<size_t>(std::ceil(p * values.size()) - 1.0));
        return values[i];
    }

    std::vector<double> frameSamplesCopy() const {
        std::vector<double> values;
        values.reserve(frameSamples);
        for (size_t i = 0; i < frameSamples; ++i) values.push_back(frameMs[i]);
        return values;
    }

    void updateStats(double deltaMs) {
        currentMs = deltaMs;
        currentFps = deltaMs > 0 ? 1000.0 / deltaMs : 0.0;
        frameMs[frameCursor] = deltaMs;
        frameCursor = (frameCursor + 1) % frameMs.size();
        frameSamples = std::min(frameSamples + 1, frameMs.size());
        ++frameCount;
        LARGE_INTEGER now{}; QueryPerformanceCounter(&now);
        elapsedSeconds = static_cast<double>(now.QuadPart - sessionStart.QuadPart) / qpcFrequency.QuadPart;
        avgFps = elapsedSeconds > 0 ? frameCount / elapsedSeconds : 0;
        avgMs = frameCount > 0 ? (elapsedSeconds * 1000.0) / frameCount : 0;

        const double sinceMetricUpdate = static_cast<double>(now.QuadPart - lastMetricUpdate.QuadPart) / qpcFrequency.QuadPart;
        if (sinceMetricUpdate >= 0.25) {
            auto values = frameSamplesCopy();
            p50 = percentile(values, .50); p95 = percentile(values, .95); p99 = percentile(values, .99);
            low1 = p99 > 0 ? 1000.0 / p99 : 0;
            const double p999 = percentile(values, .999);
            low01 = p999 > 0 ? 1000.0 / p999 : 0;
            if (values.size() > 8 && deltaMs > 2.0 * percentile(values, .50)) ++stutterCount;
            lastMetricUpdate = now;
        }

        if (static_cast<double>(now.QuadPart - previousStats.QuadPart) / qpcFrequency.QuadPart >= 1.0) {
            FILETIME creation{}, exit{}, kernel{}, user{};
            if (GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user)) {
                const uint64_t curr = (static_cast<uint64_t>(user.dwHighDateTime) << 32 | user.dwLowDateTime) +
                                      (static_cast<uint64_t>(kernel.dwHighDateTime) << 32 | kernel.dwLowDateTime);
                const double elapsed100ns = static_cast<double>(now.QuadPart - previousStats.QuadPart) * 10'000'000.0 / qpcFrequency.QuadPart;
                SYSTEM_INFO si{}; GetSystemInfo(&si);
                cpuPercent = elapsed100ns > 0 && curr >= previousCpu100ns
                    ? 100.0 * static_cast<double>(curr - previousCpu100ns) / elapsed100ns / std::max<DWORD>(1u, si.dwNumberOfProcessors) : 0.0;
                previousCpu100ns = curr;
            }
            PROCESS_MEMORY_COUNTERS_EX mem{}; mem.cb = sizeof(mem);
            if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&mem), sizeof(mem))) ramBytes = mem.WorkingSetSize;
            previousStats = now;
            updateHudText();
        }
        if (csv) {
            csv << frameCount << ',' << std::fixed << std::setprecision(4) << elapsedSeconds << ','
                << currentFps << ',' << avgFps << ',' << low1 << ',' << low01 << ',' << currentMs << ','
                << p50 << ',' << p95 << ',' << p99 << ',' << cpuPercent << ','
                << (static_cast<double>(ramBytes) / (1024.0*1024.0)) << ','
                << static_cast<unsigned>(featureLevel) << ",\"";
            for (wchar_t ch : adapterName) csv << (ch < 128 ? static_cast<char>(ch) : '?');
            csv << "\"\n";
        }
    }

    void updateHudText() {
        if (!hud) return;
        std::wostringstream text;
        text << L"NYAMIST | REAL DIRECT3D 11 | LIVE ROTATING CUBE\r\n"
             << L"API: D3D11   Feature Level: " << featureLevelName() << L"   Resolution: 1280x720   FPS cap: " << (fpsLimit > 0 ? std::to_wstring(fpsLimit) : L"OFF") << L" (F2 cycles)\r\n"
             << L"FPS: " << std::fixed << std::setprecision(1) << currentFps << L" current / " << avgFps << L" avg\r\n"
             << L"1% low: " << low1 << L"   0.1% low: " << low01 << L"\r\n"
             << L"Frame: " << std::setprecision(2) << currentMs << L" ms   avg " << avgMs
             << L"   P50/P95/P99 " << p50 << L"/" << p95 << L"/" << p99 << L" ms\r\n"
             << L"CPU: " << std::setprecision(1) << cpuPercent << L"% device   RAM: "
             << (static_cast<double>(ramBytes)/(1024.0*1024.0)) << L" MB\r\n"
             << L"GPU: " << adapterName << L"\r\n"
             << L"Dedicated VRAM: " << (dedicatedVideoMemory/(1024*1024)) << L" MB   GPU load: N/A\r\n"
             << L"Frames: " << frameCount << L"   Stutters (>2x median): " << stutterCount
             << L"   Session: " << std::setprecision(0) << elapsedSeconds << L" s\r\n"
             << L"CSV: NYAMIST-DX11-Live-Cube.csv";
        SetWindowTextW(hud, text.str().c_str());
    }

    const wchar_t* featureLevelName() const {
        switch (featureLevel) {
            case D3D_FEATURE_LEVEL_11_1: return L"11_1";
            case D3D_FEATURE_LEVEL_11_0: return L"11_0";
            case D3D_FEATURE_LEVEL_10_1: return L"10_1";
            case D3D_FEATURE_LEVEL_10_0: return L"10_0";
            default: return L"lower";
        }
    }

    void render() {
        LARGE_INTEGER now{}; QueryPerformanceCounter(&now);
        const double delta = static_cast<double>(now.QuadPart - previousFrame.QuadPart) * 1000.0 / qpcFrequency.QuadPart;
        previousFrame = now;
        updateStats(delta);

        const float clear[4] = {0.018f, 0.026f, 0.045f, 1.0f};
        context->OMSetRenderTargets(1, rtv.GetAddressOf(), dsv.Get());
        context->ClearRenderTargetView(rtv.Get(), clear);
        context->ClearDepthStencilView(dsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
        D3D11_VIEWPORT vp{}; vp.Width = static_cast<float>(kWidth); vp.Height = static_cast<float>(kHeight); vp.MinDepth = 0; vp.MaxDepth = 1;
        context->RSSetViewports(1, &vp);
        UINT stride = sizeof(Vertex), offset = 0;
        context->IASetInputLayout(inputLayout.Get());
        context->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);
        context->IASetIndexBuffer(indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        const float seconds = static_cast<float>(elapsedSeconds);
        XMMATRIX world = XMMatrixRotationRollPitchYaw(seconds * .47f, seconds * .71f, seconds * .29f);
        XMMATRIX view = XMMatrixLookAtLH(XMVectorSet(3.2f,2.4f,-5.8f,1), XMVectorZero(), XMVectorSet(0,1,0,0));
        XMMATRIX projection = XMMatrixPerspectiveFovLH(XM_PIDIV4, static_cast<float>(kWidth)/kHeight, .1f, 100.0f);
        XMMATRIX mvp = XMMatrixTranspose(world * view * projection);
        ConstantBuffer cb{}; XMStoreFloat4x4(&cb.mvp, mvp);
        context->UpdateSubresource(constantBuffer.Get(), 0, nullptr, &cb, 0, 0);
        context->VSSetShader(vertexShader.Get(), nullptr, 0);
        context->VSSetConstantBuffers(0, 1, constantBuffer.GetAddressOf());
        context->PSSetShader(pixelShader.Get(), nullptr, 0);
        context->DrawIndexed(36, 0, 0);
        swapChain->Present(0, 0);
    }
};

App* gApp = nullptr;
LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_KEYDOWN && wParam == VK_F2 && gApp) {
        const int limits[] = {30, 60, 90, 120, 0};
        auto it = std::find(std::begin(limits), std::end(limits), gApp->fpsLimit);
        gApp->fpsLimit = (it == std::end(limits) || ++it == std::end(limits)) ? limits[0] : *it;
        gApp->updateHudText();
        return 0;
    }
    if (message == WM_CTLCOLORSTATIC) {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetTextColor(dc, RGB(235, 242, 255)); SetBkColor(dc, RGB(8, 12, 20));
        static HBRUSH brush = CreateSolidBrush(RGB(8, 12, 20)); return reinterpret_cast<LRESULT>(brush);
    }
    if (message == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    WNDCLASSEXW wc{}; wc.cbSize = sizeof(wc); wc.lpfnWndProc = windowProc;
    wc.hInstance = instance; wc.lpszClassName = kClassName; wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    if (!RegisterClassExW(&wc)) return 10;
    const DWORD windowStyle = WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT rect{0,0,static_cast<LONG>(kWidth),static_cast<LONG>(kHeight)};
    AdjustWindowRect(&rect, windowStyle, FALSE);
    HWND hwnd = CreateWindowExW(0, kClassName, L"NYAMIST — DX11 Live Cube", windowStyle,
        CW_USEDEFAULT, CW_USEDEFAULT, rect.right-rect.left, rect.bottom-rect.top,
        nullptr, nullptr, instance, nullptr);
    if (!hwnd) return 11;
    App app; gApp = &app;
    if (!app.initialize(hwnd)) {
        MessageBoxW(hwnd, L"Direct3D 11 device or pipeline creation failed.", L"NYAMIST DX11", MB_ICONERROR);
        return 12;
    }
    ShowWindow(hwnd, show);
    MSG msg{};
    bool running = true;
    while (running) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { running = false; break; }
            TranslateMessage(&msg); DispatchMessageW(&msg);
        }
        if (running) {
            const auto frameStart = std::chrono::steady_clock::now();
            app.render();
            if (app.fpsLimit > 0) {
                const auto budget = std::chrono::nanoseconds(1'000'000'000LL / app.fpsLimit);
                const auto spent = std::chrono::steady_clock::now() - frameStart;
                if (spent < budget) std::this_thread::sleep_for(budget - spent);
            }
        }
    }
    if (app.csv) app.csv.flush();
    return static_cast<int>(msg.wParam);
}
