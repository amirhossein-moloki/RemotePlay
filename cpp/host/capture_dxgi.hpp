#pragma once

#ifdef _WIN32
#include <d3d11.h>
#include <dxgi1_2.h>

namespace Host {

class CaptureDXGI {
public:
    CaptureDXGI();
    ~CaptureDXGI();

    bool Initialize();
    ID3D11Device* GetDevice() { return m_device; }
    HRESULT AcquireFrame(ID3D11Texture2D** texture);
    void ReleaseFrame();

private:
    void Cleanup();

    ID3D11Device* m_device;
    ID3D11DeviceContext* m_context;
    IDXGIOutputDuplication* m_dupl;
};

} // namespace Host
#endif
