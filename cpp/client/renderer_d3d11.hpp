#pragma once

#ifdef _WIN32
#include <d3d11.h>
#include <dxgi1_2.h>

namespace Client {

class RendererD3D11 {
public:
    RendererD3D11();
    ~RendererD3D11();

    bool Initialize(HWND hwnd, int width, int height);
    void Render(ID3D11Texture2D* texture);
    void Shutdown();

    ID3D11Device* GetDevice() { return m_device; }

    void NewFrame();
    void EndFrame();

private:
    ID3D11Device* m_device = nullptr;
    ID3D11DeviceContext* m_context = nullptr;
    IDXGISwapChain* m_swapChain = nullptr;
    ID3D11RenderTargetView* m_backBufferView = nullptr;

    // Video processing for NV12 to RGBA conversion
    ID3D11VideoDevice* m_videoDevice = nullptr;
    ID3D11VideoContext* m_videoContext = nullptr;
    ID3D11VideoProcessor* m_videoProcessor = nullptr;
    ID3D11VideoProcessorEnumerator* m_videoEnumerator = nullptr;

    // Cached views for performance
    ID3D11VideoProcessorInputView* m_inputView = nullptr;
    ID3D11VideoProcessorOutputView* m_outputView = nullptr;
    ID3D11Texture2D* m_lastInputTexture = nullptr;
    ID3D11Texture2D* m_lastOutputTexture = nullptr;

    bool SetupVideoProcessor(int width, int height);
};

} // namespace Client
#endif
