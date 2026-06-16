#include "capture_dxgi.hpp"

#ifdef _WIN32
#include <iostream>
#include <sstream>
#include "../common/logger.hpp"

namespace Host {

CaptureDXGI::CaptureDXGI() : m_device(nullptr), m_context(nullptr), m_dupl(nullptr) {}

CaptureDXGI::~CaptureDXGI() {
    Cleanup();
}

bool CaptureDXGI::Initialize() {
    if (m_dupl) {
        m_dupl->Release();
        m_dupl = nullptr;
    }

    if (m_device && FAILED(m_device->GetDeviceRemovedReason())) {
        Cleanup();
    }

    IDXGIFactory1* factory = nullptr;
    HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&factory);
    if (FAILED(hr)) return false;

    IDXGIAdapter1* adapter = nullptr;
    hr = factory->EnumAdapters1(0, &adapter);
    factory->Release();
    if (FAILED(hr)) return false;

    if (!m_device) {
        hr = D3D11CreateDevice(adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION, &m_device, nullptr, &m_context);
        if (FAILED(hr)) {
            adapter->Release();
            return false;
        }
    }

    IDXGIOutput* dxgiOutput = nullptr;
    hr = adapter->EnumOutputs(0, &dxgiOutput);
    adapter->Release();
    if (FAILED(hr)) return false;

    IDXGIOutput1* dxgiOutput1 = nullptr;
    hr = dxgiOutput->QueryInterface(__uuidof(IDXGIOutput1), (void**)&dxgiOutput1);
    dxgiOutput->Release();
    if (FAILED(hr)) return false;

    hr = dxgiOutput1->DuplicateOutput(m_device, &m_dupl);
    dxgiOutput1->Release();

    if (FAILED(hr)) {
        std::stringstream ss;
        ss << std::hex << hr;
        if (hr == DXGI_ERROR_ACCESS_DENIED) {
            LOG_ERROR("Capture", "DXGI DuplicateOutput failed: Access Denied (0x" + ss.str() + "). Try running as administrator.");
        } else {
            LOG_ERROR("Capture", "DXGI DuplicateOutput failed with HR: 0x" + ss.str());
        }
        return false;
    }
    return true;
}

HRESULT CaptureDXGI::AcquireFrame(ID3D11Texture2D** texture) {
    if (!m_dupl) return DXGI_ERROR_INVALID_CALL;

    DXGI_OUTDUPL_FRAME_INFO frameInfo;
    IDXGIResource* desktopResource = nullptr;
    HRESULT hr = m_dupl->AcquireNextFrame(100, &frameInfo, &desktopResource);
    if (FAILED(hr)) {
        if (hr == DXGI_ERROR_ACCESS_LOST || hr == DXGI_ERROR_DEVICE_REMOVED) {
            std::stringstream ss;
            ss << "0x" << std::hex << hr;
            LOG_WARN("Capture", "DXGI Device lost (HR: " + ss.str() + "), cleaning up for re-init.");
            Cleanup();
        }
        return hr;
    }

    hr = desktopResource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)texture);
    desktopResource->Release();

    return hr;
}

void CaptureDXGI::ReleaseFrame() {
    if (m_dupl) m_dupl->ReleaseFrame();
}

void CaptureDXGI::Cleanup() {
    if (m_dupl) { m_dupl->Release(); m_dupl = nullptr; }
    if (m_context) { m_context->Release(); m_context = nullptr; }
    if (m_device) { m_device->Release(); m_device = nullptr; }
}

} // namespace Host
#endif
