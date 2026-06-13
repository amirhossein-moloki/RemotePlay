#include <d3d11.h>
#include <dxgi1_2.h>
#include <iostream>
#include <vector>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

namespace Host {

class CaptureDXGI {
public:
    CaptureDXGI() : m_device(nullptr), m_context(nullptr), m_dupl(nullptr) {}
    ~CaptureDXGI() { Cleanup(); }

    bool Initialize() {
        HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &m_device, nullptr, &m_context);
        if (FAILED(hr)) return false;

        IDXGIDevice* dxgiDevice = nullptr;
        m_device->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDevice);

        IDXGIAdapter* dxgiAdapter = nullptr;
        dxgiDevice->GetParent(__uuidof(IDXGIAdapter), (void**)&dxgiAdapter);
        dxgiDevice->Release();

        IDXGIOutput* dxgiOutput = nullptr;
        dxgiAdapter->GetOutput(0, &dxgiOutput);
        dxgiAdapter->Release();

        IDXGIOutput1* dxgiOutput1 = nullptr;
        dxgiOutput->QueryInterface(__uuidof(IDXGIOutput1), (void**)&dxgiOutput1);
        dxgiOutput->Release();

        hr = dxgiOutput1->DuplicateOutput(m_device, &m_dupl);
        dxgiOutput1->Release();

        if (FAILED(hr)) return false;
        return true;
    }

    bool AcquireFrame(ID3D11Texture2D** texture) {
        DXGI_OUTDUPL_FRAME_INFO frameInfo;
        IDXGIResource* desktopResource = nullptr;
        HRESULT hr = m_dupl->AcquireNextFrame(100, &frameInfo, &desktopResource);
        if (FAILED(hr)) return false;

        hr = desktopResource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)texture);
        desktopResource->Release();

        if (FAILED(hr)) return false;
        return true;
    }

    void ReleaseFrame() {
        m_dupl->ReleaseFrame();
    }

private:
    void Cleanup() {
        if (m_dupl) m_dupl->Release();
        if (m_context) m_context->Release();
        if (m_device) m_device->Release();
    }

    ID3D11Device* m_device;
    ID3D11DeviceContext* m_context;
    IDXGIOutputDuplication* m_dupl;
};

} // namespace Host
