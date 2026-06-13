#include "renderer_d3d11.hpp"

#ifdef _WIN32
#include <iostream>
#include "../common/logger.hpp"
#include "third_party/imgui/imgui.h"
#include "third_party/imgui/imgui_impl_win32.h"
#include "third_party/imgui/imgui_impl_d3d11.h"

namespace Client {

RendererD3D11::RendererD3D11() {}
RendererD3D11::~RendererD3D11() { Shutdown(); }

bool RendererD3D11::Initialize(HWND hwnd, int width, int height) {
    if (!hwnd) {
        LOG_ERROR("Renderer", "Invalid HWND provided.");
        return false;
    }

    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = width;
    sd.BufferDesc.Height = height;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;

    UINT flags = 0;
#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, nullptr, 0,
                                               D3D11_SDK_VERSION, &sd, &m_swapChain, &m_device, nullptr, &m_context);
    if (FAILED(hr)) {
        std::stringstream ss;
        ss << "0x" << std::hex << hr;
        LOG_ERROR("Renderer", "Failed to create D3D11 device and swap chain. HR: " + ss.str());
        return false;
    }

    ID3D11Texture2D* pBackBuffer = nullptr;
    m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
    m_device->CreateRenderTargetView(pBackBuffer, nullptr, &m_backBufferView);
    pBackBuffer->Release();

    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplD3D11_Init(m_device, m_context);

    LOG_INFO("Renderer", "D3D11 Renderer initialized at " + std::to_string(width) + "x" + std::to_string(height));
    return true;
}

void RendererD3D11::NewFrame() {
    ImGui_ImplD3D11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void RendererD3D11::EndFrame() {
    ImGui::Render();
    m_context->OMSetRenderTargets(1, &m_backBufferView, nullptr);
    ImGui_ImplD3D11_RenderDrawData(ImGui::GetDrawData());
    m_swapChain->Present(0, 0);
}

void RendererD3D11::Render(ID3D11Texture2D* texture) {
    if (!texture || !m_context) return;

    m_context->OMSetRenderTargets(1, &m_backBufferView, nullptr);

    // Copy the decoded texture to the backbuffer
    // In a production app, we would use a shader to handle colorspace conversion if needed
    ID3D11Texture2D* backBuffer = nullptr;
    m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
    m_context->CopyResource(backBuffer, texture);
    backBuffer->Release();

}

void RendererD3D11::Shutdown() {
    ImGui_ImplD3D11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    if (m_backBufferView) m_backBufferView->Release();
    if (m_swapChain) m_swapChain->Release();
    if (m_context) m_context->Release();
    if (m_device) m_device->Release();

    m_backBufferView = nullptr;
    m_swapChain = nullptr;
    m_context = nullptr;
    m_device = nullptr;
}

} // namespace Client
#endif
