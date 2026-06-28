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

    RECT rect;
    if (GetClientRect(hwnd, &rect)) {
        width = rect.right - rect.left;
        height = rect.bottom - rect.top;
    }

    if (width <= 0 || height <= 0) {
        LOG_WARN("Renderer", "Invalid window dimensions (" + std::to_string(width) + "x" + std::to_string(height) + "). Defaulting to 1280x720.");
        width = 1280;
        height = 720;
    }

    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferDesc.Width = width;
    sd.BufferDesc.Height = height;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 0;
    sd.BufferDesc.RefreshRate.Denominator = 0;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

    UINT deviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
    deviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    struct FallbackOption {
        DXGI_SWAP_EFFECT effect;
        UINT bufferCount;
        bool tearing;
    };

    std::vector<FallbackOption> fallbacks = {
        { DXGI_SWAP_EFFECT_FLIP_DISCARD, 2, true },
        { DXGI_SWAP_EFFECT_FLIP_DISCARD, 2, false },
        { DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL, 2, false },
        { DXGI_SWAP_EFFECT_DISCARD, 1, false }
    };

    HRESULT hr = E_FAIL;
    for (const auto& opt : fallbacks) {
        sd.SwapEffect = opt.effect;
        sd.BufferCount = opt.bufferCount;
        if (opt.tearing) sd.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
        else sd.Flags &= ~DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

        hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, deviceFlags, nullptr, 0,
                                           D3D11_SDK_VERSION, &sd, &m_swapChain, &m_device, nullptr, &m_context);
        if (SUCCEEDED(hr)) {
            DXGI_SWAP_CHAIN_DESC actualSd;
            if (SUCCEEDED(m_swapChain->GetDesc(&actualSd))) {
                m_bufferCount = std::min(actualSd.BufferCount, 8U);
            } else {
                m_bufferCount = sd.BufferCount;
            }
            m_tearingSupported = opt.tearing;
            LOG_INFO("Renderer", "Swap chain created: Effect=" + std::to_string(sd.SwapEffect) +
                     " Buffers=" + std::to_string(m_bufferCount) +
                     " Tearing=" + std::to_string(opt.tearing) +
                     " Resolution=" + std::to_string(width) + "x" + std::to_string(height));
            break;
        }
    }

    if (FAILED(hr)) {
        std::stringstream ss;
        ss << "0x" << std::hex << hr;
        LOG_ERROR("Renderer", "All D3D11 swap chain fallback options failed. HR: " + ss.str());
        return false;
    }

    for (UINT i = 0; i < m_bufferCount; i++) {
        ID3D11Texture2D* pBackBuffer = nullptr;
        hr = m_swapChain->GetBuffer(i, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
        if (FAILED(hr) || !pBackBuffer) {
            std::stringstream ss;
            ss << "0x" << std::hex << (FAILED(hr) ? hr : E_FAIL);
            if (i > 0) {
                LOG_WARN("Renderer", "Failed to get swap chain buffer " + std::to_string(i) + ". HR: " + ss.str() + ". Continuing with " + std::to_string(i) + " buffers.");
                m_bufferCount = i;
                break;
            } else {
                LOG_ERROR("Renderer", "Failed to get swap chain buffer " + std::to_string(i) + ". HR: " + ss.str());
                return false;
            }
        }

        hr = m_device->CreateRenderTargetView(pBackBuffer, nullptr, &m_backBufferViews[i]);
        pBackBuffer->Release();
        if (FAILED(hr)) {
            std::stringstream ss;
            ss << "0x" << std::hex << hr;
            LOG_ERROR("Renderer", "Failed to create render target view for buffer " + std::to_string(i) + ". HR: " + ss.str());
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
    if (m_bufferCount > 1) {
        IDXGISwapChain3* sc3 = nullptr;
        if (SUCCEEDED(m_swapChain->QueryInterface(__uuidof(IDXGISwapChain3), (void**)&sc3))) {
            m_currentBufferIndex = sc3->GetCurrentBackBufferIndex();
            sc3->Release();
        } else {
            // Fallback for older swap effects or if QueryInterface fails
            m_currentBufferIndex = 0;
        }
    } else {
        m_currentBufferIndex = 0;
    }

    if (m_currentBufferIndex < 0 || m_currentBufferIndex >= (int)m_bufferCount) m_currentBufferIndex = 0;

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
    // Use DXGI_PRESENT_ALLOW_TEARING only if supported
    UINT presentFlags = (m_tearingSupported) ? DXGI_PRESENT_ALLOW_TEARING : 0;
    HRESULT hr = m_swapChain->Present(0, presentFlags);
    if (FAILED(hr)) {
        std::stringstream ss;
        ss << "0x" << std::hex << hr;
        LOG_ERROR("Renderer", "Present failed. HR: " + ss.str());
    }
}

bool RendererD3D11::SetupVideoProcessor(int inWidth, int inHeight, int outWidth, int outHeight) {
    HRESULT hr;

    if (!m_videoDevice) {
        hr = m_device->QueryInterface(__uuidof(ID3D11VideoDevice), (void**)&m_videoDevice);
        if (FAILED(hr)) return false;
    }

    if (!m_videoContext) {
        hr = m_context->QueryInterface(__uuidof(ID3D11VideoContext), (void**)&m_videoContext);
        if (FAILED(hr)) return false;
    }

    // Recreate if dimensions changed
    if (m_videoProcessor) {
        D3D11_VIDEO_PROCESSOR_CONTENT_DESC currentDesc;
        m_videoEnumerator->GetVideoProcessorContentDesc(&currentDesc);
        if (currentDesc.InputWidth == (UINT)inWidth && currentDesc.InputHeight == (UINT)inHeight &&
            currentDesc.OutputWidth == (UINT)outWidth && currentDesc.OutputHeight == (UINT)outHeight) {
            return true;
        }

        // Release old processor
        if (m_inputView) { m_inputView->Release(); m_inputView = nullptr; }
        m_lastInputTexture = nullptr;
        m_lastInputArrayIndex = -1;
        for (int i = 0; i < 8; i++) {
            if (m_outputViews[i]) { m_outputViews[i]->Release(); m_outputViews[i] = nullptr; }
            m_lastOutputTextures[i] = nullptr;
        }
        m_videoProcessor->Release(); m_videoProcessor = nullptr;
        m_videoEnumerator->Release(); m_videoEnumerator = nullptr;
    }

    D3D11_VIDEO_PROCESSOR_CONTENT_DESC contentDesc = {};
    contentDesc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
    contentDesc.InputFrameRate.Numerator = 60;
    contentDesc.InputFrameRate.Denominator = 1;
    contentDesc.InputWidth = inWidth;
    contentDesc.InputHeight = inHeight;
    contentDesc.OutputFrameRate.Numerator = 60;
    contentDesc.OutputFrameRate.Denominator = 1;
    contentDesc.OutputWidth = outWidth;
    contentDesc.OutputHeight = outHeight;
    contentDesc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

    hr = m_videoDevice->CreateVideoProcessorEnumerator(&contentDesc, &m_videoEnumerator);
    if (FAILED(hr)) return false;

    hr = m_videoDevice->CreateVideoProcessor(m_videoEnumerator, 0, &m_videoProcessor);
    return SUCCEEDED(hr);
}

void RendererD3D11::Render(ID3D11Texture2D* texture, int arrayIndex) {
    if (!m_context || !m_swapChain) {
        LOG_ERROR("StreamTrace", "RENDER_INPUT_INVALID resources null.");
        return;
    }

    if (!texture) {
        texture = m_lastInputTexture;
        arrayIndex = m_lastInputArrayIndex;
    }

    if (!texture) {
        // No cached frame available yet
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
        if (SetupVideoProcessor(srcDesc.Width, srcDesc.Height, dstDesc.Width, dstDesc.Height)) {
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
                } else {
                    // Cache will be updated at the end of the function
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

                // Ensure scaling is applied to the full destination rect
                RECT srcRect = { 0, 0, (LONG)srcDesc.Width, (LONG)srcDesc.Height };
                RECT dstRect = { 0, 0, (LONG)dstDesc.Width, (LONG)dstDesc.Height };
                m_videoContext->VideoProcessorSetStreamSourceRect(m_videoProcessor, 0, TRUE, &srcRect);
                m_videoContext->VideoProcessorSetStreamDestRect(m_videoProcessor, 0, TRUE, &dstRect);
                m_videoContext->VideoProcessorSetOutputTargetRect(m_videoProcessor, TRUE, &dstRect);

                HRESULT bltHr = m_videoContext->VideoProcessorBlt(m_videoProcessor, m_outputViews[m_currentBufferIndex], 0, 1, &stream);
                if (SUCCEEDED(bltHr)) {
                    LOG_INFO("StreamTrace", "RENDER_PRESENT_PATH video_processor result=ok");
                } else {
                    LOG_ERROR("StreamTrace", "RENDER_PRESENT_PATH video_processor result=fail hr=" + std::to_string((long)bltHr));
                }
            }
        }
    } else {
        // Fallback for non-NV12 (e.g. software decoded RGBA)
        UINT srcSubresource = (srcDesc.ArraySize > 1) ? (UINT)arrayIndex : 0;
        if (srcDesc.Width == dstDesc.Width && srcDesc.Height == dstDesc.Height && srcDesc.Format == dstDesc.Format && srcDesc.ArraySize == 1) {
            m_context->CopyResource(backBuffer, texture);
            LOG_INFO("StreamTrace", "RENDER_PRESENT_PATH copy_resource result=ok");
        } else {
            // Try to use VideoProcessor for scaling even for non-NV12 if possible
            // Most D3D11 video processors support RGBA scaling.
            if (SetupVideoProcessor(srcDesc.Width, srcDesc.Height, dstDesc.Width, dstDesc.Height)) {
                 if (texture != m_lastInputTexture || arrayIndex != m_lastInputArrayIndex) {
                    if (m_inputView) m_inputView->Release();
                    m_inputView = nullptr;

                    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC inputViewDesc = {};
                    inputViewDesc.FourCC = 0;
                    inputViewDesc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
                    inputViewDesc.Texture2D.MipSlice = 0;
                    inputViewDesc.Texture2D.ArraySlice = (srcDesc.ArraySize > 1) ? arrayIndex : 0;

                    hr = m_videoDevice->CreateVideoProcessorInputView(texture, m_videoEnumerator, &inputViewDesc, &m_inputView);
                }

                if (backBuffer != m_lastOutputTextures[m_currentBufferIndex]) {
                    if (m_outputViews[m_currentBufferIndex]) m_outputViews[m_currentBufferIndex]->Release();
                    m_outputViews[m_currentBufferIndex] = nullptr;

                    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outputViewDesc = {};
                    outputViewDesc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
                    outputViewDesc.Texture2D.MipSlice = 0;

                    hr = m_videoDevice->CreateVideoProcessorOutputView(backBuffer, m_videoEnumerator, &outputViewDesc, &m_outputViews[m_currentBufferIndex]);
                    if (SUCCEEDED(hr)) m_lastOutputTextures[m_currentBufferIndex] = backBuffer;
                }

                if (m_inputView && m_outputViews[m_currentBufferIndex]) {
                    D3D11_VIDEO_PROCESSOR_STREAM stream = {};
                    stream.Enable = TRUE;
                    stream.pInputSurface = m_inputView;

                    RECT srcRect = { 0, 0, (LONG)srcDesc.Width, (LONG)srcDesc.Height };
                    RECT dstRect = { 0, 0, (LONG)dstDesc.Width, (LONG)dstDesc.Height };
                    m_videoContext->VideoProcessorSetStreamSourceRect(m_videoProcessor, 0, TRUE, &srcRect);
                    m_videoContext->VideoProcessorSetStreamDestRect(m_videoProcessor, 0, TRUE, &dstRect);
                    m_videoContext->VideoProcessorSetOutputTargetRect(m_videoProcessor, TRUE, &dstRect);

                    HRESULT bltHr = m_videoContext->VideoProcessorBlt(m_videoProcessor, m_outputViews[m_currentBufferIndex], 0, 1, &stream);
                    if (SUCCEEDED(bltHr)) {
                        LOG_INFO("StreamTrace", "RENDER_PRESENT_PATH video_processor (fallback) result=ok");
                        goto render_done;
                    }
                }
            }

            // Ultimate fallback: CopySubresourceRegion (will crop if sizes differ)
            D3D11_BOX box = { 0, 0, 0, std::min(srcDesc.Width, dstDesc.Width), std::min(srcDesc.Height, dstDesc.Height), 1 };
            m_context->CopySubresourceRegion(backBuffer, 0, 0, 0, 0, texture, srcSubresource, &box);
            LOG_INFO("StreamTrace", "RENDER_PRESENT_PATH copy_subresource result=ok srcSub=" + std::to_string(srcSubresource));
        }
    }

render_done:

    backBuffer->Release();

    // Update cache with AddRef/Release for safety during redraws
    if (texture != m_lastInputTexture || arrayIndex != m_lastInputArrayIndex) {
        if (m_lastInputTexture) m_lastInputTexture->Release();
        m_lastInputTexture = texture;
        m_lastInputArrayIndex = arrayIndex;
        if (m_lastInputTexture) m_lastInputTexture->AddRef();
    }
}

void RendererD3D11::Shutdown() {
    ImGui_ImplD3D11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    if (m_inputView) m_inputView->Release();
    m_inputView = nullptr;
    if (m_lastInputTexture) m_lastInputTexture->Release();
    m_lastInputTexture = nullptr;
    for (UINT i = 0; i < m_bufferCount; i++) {
        if (m_outputViews[i]) m_outputViews[i]->Release();
        m_outputViews[i] = nullptr;
        m_lastOutputTextures[i] = nullptr;
    }
    if (m_videoProcessor) m_videoProcessor->Release();
    if (m_videoEnumerator) m_videoEnumerator->Release();
    if (m_videoContext) m_videoContext->Release();
    if (m_videoDevice) m_videoDevice->Release();

    for (UINT i = 0; i < m_bufferCount; i++) {
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
