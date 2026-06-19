#include "renderer_d3d11.hpp"

#ifdef _WIN32
#include <iostream>
#include <algorithm>
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
    sd.BufferCount = 2; // Required for Flip Model
    sd.BufferDesc.Width = width;
    sd.BufferDesc.Height = height;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // Modern low-latency flip model
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING; // Allow VSync Off (Variable Refresh Rate)

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
    // Clear backbuffer to a dark color to avoid white screens when no frames are arriving
    float clearColor[4] = { 0.043f, 0.063f, 0.125f, 1.0f }; // Matches Theme Background #0B1020
    m_context->ClearRenderTargetView(m_backBufferView, clearColor);

    ImGui_ImplD3D11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void RendererD3D11::EndFrame() {
    ImGui::Render();
    m_context->OMSetRenderTargets(1, &m_backBufferView, nullptr);
    ImGui_ImplD3D11_RenderDrawData(ImGui::GetDrawData());
    // Use DXGI_PRESENT_ALLOW_TEARING for lowest possible latency with Flip Discard
    m_swapChain->Present(0, DXGI_PRESENT_ALLOW_TEARING);
}

bool RendererD3D11::SetupVideoProcessor(int width, int height) {
    if (m_videoProcessor) return true;
    if (!m_device) return false;

    if (!m_videoDevice) {
        HRESULT hr = m_device->QueryInterface(__uuidof(ID3D11VideoDevice), (void**)&m_videoDevice);
        if (FAILED(hr)) return false;
    }

    if (!m_videoContext) {
        m_context->QueryInterface(__uuidof(ID3D11VideoContext), (void**)&m_videoContext);
    }

    D3D11_VIDEO_PROCESSOR_CONTENT_DESC contentDesc = {};
    contentDesc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
    contentDesc.InputFrameRate.Numerator = 60;
    contentDesc.InputFrameRate.Denominator = 1;
    contentDesc.InputWidth = width;
    contentDesc.InputHeight = height;
    contentDesc.OutputFrameRate.Numerator = 60;
    contentDesc.OutputFrameRate.Denominator = 1;
    contentDesc.OutputWidth = width;
    contentDesc.OutputHeight = height;
    contentDesc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

    hr = m_videoDevice->CreateVideoProcessorEnumerator(&contentDesc, &m_videoEnumerator);
    if (FAILED(hr)) return false;

    hr = m_videoDevice->CreateVideoProcessor(m_videoEnumerator, 0, &m_videoProcessor);
    return SUCCEEDED(hr);
}

void RendererD3D11::Render(ID3D11Texture2D* texture) {
    if (!texture || !m_context) return;

    D3D11_TEXTURE2D_DESC srcDesc;
    texture->GetDesc(&srcDesc);

    ID3D11Texture2D* backBuffer = nullptr;
    m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
    if (!backBuffer) return;

    D3D11_TEXTURE2D_DESC dstDesc;
    backBuffer->GetDesc(&dstDesc);

    if (srcDesc.Format == DXGI_FORMAT_NV12) {
        if (SetupVideoProcessor(srcDesc.Width, srcDesc.Height)) {
            // Recreate input view only if texture changed
            if (texture != m_lastInputTexture) {
                if (m_inputView) m_inputView->Release();
                D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC inputViewDesc = {};
                inputViewDesc.FourCC = 0;
                inputViewDesc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
                inputViewDesc.Texture2D.MipSlice = 0;
                inputViewDesc.Texture2D.ArraySlice = 0;
                m_videoDevice->CreateVideoProcessorInputView(texture, m_videoEnumerator, &inputViewDesc, &m_inputView);
                m_lastInputTexture = texture;
            }

            // Recreate output view only if backBuffer changed (e.g. resize)
            if (backBuffer != m_lastOutputTexture) {
                if (m_outputView) m_outputView->Release();
                D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outputViewDesc = {};
                outputViewDesc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
                outputViewDesc.Texture2D.MipSlice = 0;
                outputViewDesc.Texture2D.ArraySlice = 0;
                m_videoDevice->CreateVideoProcessorOutputView(backBuffer, m_videoEnumerator, &outputViewDesc, &m_outputView);
                m_lastOutputTexture = backBuffer;
            }

            if (m_inputView && m_outputView) {
                D3D11_VIDEO_PROCESSOR_STREAM stream = {};
                stream.Enable = TRUE;
                stream.pInputSurface = m_inputView;

                m_videoContext->VideoProcessorBlt(m_videoProcessor, m_outputView, 0, 1, &stream);
            }
        }
    } else {
        if (srcDesc.Width == dstDesc.Width && srcDesc.Height == dstDesc.Height && srcDesc.Format == dstDesc.Format) {
            m_context->CopyResource(backBuffer, texture);
        } else {
            D3D11_BOX box = { 0, 0, 0, std::min(srcDesc.Width, dstDesc.Width), std::min(srcDesc.Height, dstDesc.Height), 1 };
            m_context->CopySubresourceRegion(backBuffer, 0, 0, 0, 0, texture, 0, &box);
        }
    }

    backBuffer->Release();
}

void RendererD3D11::Shutdown() {
    ImGui_ImplD3D11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    if (m_inputView) m_inputView->Release();
    if (m_outputView) m_outputView->Release();
    if (m_videoProcessor) m_videoProcessor->Release();
    if (m_videoEnumerator) m_videoEnumerator->Release();
    if (m_videoContext) m_videoContext->Release();
    if (m_videoDevice) m_videoDevice->Release();

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
