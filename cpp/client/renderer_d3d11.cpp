#include "renderer_d3d11.hpp"

#ifdef _WIN32
#include <iostream>
#include <algorithm>
#include "../common/logger.hpp"
#include "third_party/imgui/imgui.h"
#include "third_party/imgui/imgui_impl_win32.h"
#include "third_party/imgui/imgui_impl_d3d11.h"
#include <cstdint>

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

    for (int i = 0; i < 2; i++) {
        ID3D11Texture2D* pBackBuffer = nullptr;
        hr = m_swapChain->GetBuffer(i, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
        if (SUCCEEDED(hr) && pBackBuffer) {
            hr = m_device->CreateRenderTargetView(pBackBuffer, nullptr, &m_backBufferViews[i]);
            pBackBuffer->Release();
            if (FAILED(hr)) return false;
        } else {
            return false;
        }
    }

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
    if (!m_swapChain || !m_context) return;

    // Determine current backbuffer index for Flip Model
    DXGI_SWAP_CHAIN_DESC sd;
    m_swapChain->GetDesc(&sd);
    if (sd.SwapEffect == DXGI_SWAP_EFFECT_FLIP_DISCARD || sd.SwapEffect == DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL) {
        IDXGISwapChain3* sc3 = nullptr;
        if (SUCCEEDED(m_swapChain->QueryInterface(__uuidof(IDXGISwapChain3), (void**)&sc3))) {
            m_currentBufferIndex = sc3->GetCurrentBackBufferIndex();
            sc3->Release();
        }
    } else {
        m_currentBufferIndex = 0;
    }

    if (m_currentBufferIndex < 0 || m_currentBufferIndex >= 2) m_currentBufferIndex = 0;

    // Clear backbuffer to a dark color to avoid white screens when no frames are arriving
    if (m_backBufferViews[m_currentBufferIndex]) {
        float clearColor[4] = { 0.043f, 0.063f, 0.125f, 1.0f }; // Matches Theme Background #0B1020
        m_context->ClearRenderTargetView(m_backBufferViews[m_currentBufferIndex], clearColor);
    }

    ImGui_ImplD3D11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void RendererD3D11::EndFrame() {
    if (!m_context || !m_swapChain) return;

    ImGui::Render();
    if (m_backBufferViews[m_currentBufferIndex]) {
        m_context->OMSetRenderTargets(1, &m_backBufferViews[m_currentBufferIndex], nullptr);
    }
    ImGui_ImplD3D11_RenderDrawData(ImGui::GetDrawData());
    // Use DXGI_PRESENT_ALLOW_TEARING for lowest possible latency with Flip Discard
    m_swapChain->Present(0, DXGI_PRESENT_ALLOW_TEARING);
}

bool RendererD3D11::SetupVideoProcessor(int width, int height) {
    if (m_videoProcessor) return true;
    if (!m_device) return false;

    HRESULT hr;

    if (!m_videoDevice) {
        hr = m_device->QueryInterface(__uuidof(ID3D11VideoDevice), (void**)&m_videoDevice);
        if (FAILED(hr)) return false;
    }

    if (!m_videoContext) {
        hr = m_context->QueryInterface(__uuidof(ID3D11VideoContext), (void**)&m_videoContext);
        if (FAILED(hr)) return false;
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

void RendererD3D11::Render(ID3D11Texture2D* texture, int arrayIndex) {
    if (!texture || !m_context || !m_swapChain) {
        LOG_ERROR("StreamTrace", "RENDER_INPUT_INVALID resources null.");
        return;
    }
    LOG_INFO("StreamTrace", "RENDER_INPUT texture=" + std::to_string(reinterpret_cast<uintptr_t>(texture)) +
             " arrayIndex=" + std::to_string(arrayIndex));

    D3D11_TEXTURE2D_DESC srcDesc;
    texture->GetDesc(&srcDesc);
    LOG_INFO("StreamTrace", "RENDER_TEXTURE_DESC width=" + std::to_string(srcDesc.Width) +
             " height=" + std::to_string(srcDesc.Height) +
             " format=" + std::to_string(srcDesc.Format));

    ID3D11Texture2D* backBuffer = nullptr;
    HRESULT hr = m_swapChain->GetBuffer(m_currentBufferIndex, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
    if (FAILED(hr) || !backBuffer) {
        LOG_ERROR("Renderer", "Failed to get swap chain buffer.");
        return;
    }

    D3D11_TEXTURE2D_DESC dstDesc;
    backBuffer->GetDesc(&dstDesc);

    if (srcDesc.Format == DXGI_FORMAT_NV12) {
        if (SetupVideoProcessor(srcDesc.Width, srcDesc.Height)) {
            // Recreate input view only if texture or array index changed
            if (texture != m_lastInputTexture || arrayIndex != m_lastInputArrayIndex) {
                if (m_inputView) m_inputView->Release();
                m_inputView = nullptr;

                D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC inputViewDesc = {};
                inputViewDesc.FourCC = 0;
                inputViewDesc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
                inputViewDesc.Texture2D.MipSlice = 0;
                inputViewDesc.Texture2D.ArraySlice = (srcDesc.ArraySize > 1) ? arrayIndex : 0;

                hr = m_videoDevice->CreateVideoProcessorInputView(texture, m_videoEnumerator, &inputViewDesc, &m_inputView);
                if (FAILED(hr)) {
                    LOG_ERROR("Renderer", "Failed to create video processor input view.");
                    m_lastInputTexture = nullptr;
                } else {
                    m_lastInputTexture = texture;
                    m_lastInputArrayIndex = arrayIndex;
                }
            }

            // Recreate output view for the current backbuffer if the texture changed (e.g. resize or new backbuffer ptr)
            if (backBuffer != m_lastOutputTextures[m_currentBufferIndex]) {
                if (m_outputViews[m_currentBufferIndex]) m_outputViews[m_currentBufferIndex]->Release();
                m_outputViews[m_currentBufferIndex] = nullptr;

                D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outputViewDesc = {};
                outputViewDesc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
                outputViewDesc.Texture2D.MipSlice = 0;

                hr = m_videoDevice->CreateVideoProcessorOutputView(backBuffer, m_videoEnumerator, &outputViewDesc, &m_outputViews[m_currentBufferIndex]);
                if (FAILED(hr)) {
                    LOG_ERROR("Renderer", "Failed to create video processor output view.");
                    m_lastOutputTextures[m_currentBufferIndex] = nullptr;
                } else {
                    m_lastOutputTextures[m_currentBufferIndex] = backBuffer;
                }
            }

            if (m_inputView && m_outputViews[m_currentBufferIndex]) {
                D3D11_VIDEO_PROCESSOR_STREAM stream = {};
                stream.Enable = TRUE;
                stream.pInputSurface = m_inputView;

                HRESULT bltHr = m_videoContext->VideoProcessorBlt(m_videoProcessor, m_outputViews[m_currentBufferIndex], 0, 1, &stream);
                if (SUCCEEDED(bltHr)) {
                    LOG_INFO("StreamTrace", "RENDER_PRESENT_PATH video_processor result=ok");
                } else {
                    LOG_ERROR("StreamTrace", "RENDER_PRESENT_PATH video_processor result=fail hr=" + std::to_string((long)bltHr));
                }
            }
        }
    } else {
        if (srcDesc.Width == dstDesc.Width && srcDesc.Height == dstDesc.Height && srcDesc.Format == dstDesc.Format) {
            m_context->CopyResource(backBuffer, texture);
            LOG_INFO("StreamTrace", "RENDER_PRESENT_PATH copy_resource result=ok");
        } else {
            D3D11_BOX box = { 0, 0, 0, std::min(srcDesc.Width, dstDesc.Width), std::min(srcDesc.Height, dstDesc.Height), 1 };
            m_context->CopySubresourceRegion(backBuffer, 0, 0, 0, 0, texture, 0, &box);
            LOG_INFO("StreamTrace", "RENDER_PRESENT_PATH copy_subresource result=ok");
        }
    }

    backBuffer->Release();
}

void RendererD3D11::Shutdown() {
    ImGui_ImplD3D11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    if (m_inputView) m_inputView->Release();
    m_inputView = nullptr;
    m_lastInputTexture = nullptr;
    for (int i = 0; i < 2; i++) {
        if (m_outputViews[i]) m_outputViews[i]->Release();
        m_outputViews[i] = nullptr;
        m_lastOutputTextures[i] = nullptr;
    }
    if (m_videoProcessor) m_videoProcessor->Release();
    if (m_videoEnumerator) m_videoEnumerator->Release();
    if (m_videoContext) m_videoContext->Release();
    if (m_videoDevice) m_videoDevice->Release();

    for (int i = 0; i < 2; i++) {
        if (m_backBufferViews[i]) m_backBufferViews[i]->Release();
        m_backBufferViews[i] = nullptr;
    }
    if (m_swapChain) m_swapChain->Release();
    if (m_context) m_context->Release();
    if (m_device) m_device->Release();

    m_swapChain = nullptr;
    m_context = nullptr;
    m_device = nullptr;
}

} // namespace Client
#endif
