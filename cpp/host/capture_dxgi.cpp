#include "capture_dxgi.hpp"

#ifdef _WIN32
#include <iostream>

namespace Host {

CaptureDXGI::CaptureDXGI() : m_device(nullptr), m_context(nullptr), m_dupl(nullptr) {}

CaptureDXGI::~CaptureDXGI() {
    Cleanup();
}

bool CaptureDXGI::Initialize() {
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

bool CaptureDXGI::AcquireFrame(ID3D11Texture2D** texture) {
    DXGI_OUTDUPL_FRAME_INFO frameInfo;
    IDXGIResource* desktopResource = nullptr;
    HRESULT hr = m_dupl->AcquireNextFrame(100, &frameInfo, &desktopResource);
    if (FAILED(hr)) return false;

    hr = desktopResource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)texture);
    desktopResource->Release();

    if (FAILED(hr)) return false;
    return true;
}

void CaptureDXGI::ReleaseFrame() {
    m_dupl->ReleaseFrame();
}

void CaptureDXGI::Cleanup() {
    if (m_dupl) m_dupl->Release();
    if (m_context) m_context->Release();
    if (m_device) m_device->Release();
}

} // namespace Host
#endif
