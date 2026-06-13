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

private:
    ID3D11Device* m_device = nullptr;
    ID3D11DeviceContext* m_context = nullptr;
    IDXGISwapChain* m_swapChain = nullptr;
    ID3D11RenderTargetView* m_backBufferView = nullptr;
};

} // namespace Client
#endif
