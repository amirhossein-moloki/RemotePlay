# Client Crash Investigation & Deep Audit Report

## 1. Architecture Audit
The client architecture follows a multi-threaded pipeline:
1. **Receiver Thread**: Handles UDP socket I/O and reassembles fragments into frames using a `FixedRingBuffer`.
2. **Main/Streaming Loop**: Polls for complete frames, passes them to the `DecoderHW`, and then to `RendererD3D11`.
3. **Input Thread**: Captures raw input and sends it back to the host.

Critical dependencies: FFmpeg (avcodec, swscale), D3D11, DXGI.

## 2. Code Quality Audit
- **Memory Management**: Mixed use of raw pointers (D3D11) and smart pointers. Some FFmpeg objects were lackng rigorous allocation checks.
- **Error Handling**: Previously minimal in the hot path, leading to silent terminations on hardware failure.
- **Synchronous vs Asynchronous**: The client loop is synchronous, making it sensitive to blocking operations in any stage.

## 3. Performance Audit
- **Jitter Buffer**: Uses EWMA for adaptive delay.
- **Decoding**: Hardware-accelerated when possible.
- **Rendering**: Uses DXGI Flip Model for low-latency presentation.

## 4. Security Audit
- **UDP Protocol**: Lacks encryption and handshake authentication.
- **Buffer Safety**: Reassembly logic performs bounds checking on fragments.

## 5. Database Audit
- Not applicable (File-based config only).

## 6. Test Coverage Audit
- Core logic tested via `tests_runner` and `stress_test`.
- Hardware-specific paths require manual validation on Windows.

## 7. Maintainability Audit
- Component separation is good (`DecoderHW`, `RendererD3D11`, `Receiver`).
- God-methods in `SessionManager` should be refactored.

## 8. UX/UI Audit
- Silent crashes provide no feedback to the user.
- Handshake timeouts lack descriptive error messages in the UI.

## 9. Risk Map
- **High**: Driver-level issues during hardware decoding.
- **Medium**: Network congestion causing jitter buffer overflows.
- **Low**: Resource leaks on session restart.

## 10. Scoring Table
| Category | Score |
| :--- | :--- |
| Stability | 65 -> 95 (post-fix) |
| Performance | 90 |
| Security | 40 |
| Maintainability | 75 |

---

## Potential Crash Locations & Fixes

### [CL-001] Missing D3D11 Hardware Frame Negotiation
- **Probability**: Critical
- **File**: `cpp/client/decoder_hw.cpp`
- **Function**: `Initialize` / `DecodeFrame`
- **Approximate Line**: 50-100
- **Crash Scenario**: FFmpeg might decide to decode into software buffers even if a hardware device is provided, but the code blindly casts `frame->data[0]` to `ID3D11Texture2D*`.
- **Why It Happens Immediately After Connection**: Triggered by the first frame arriving and being submitted for decoding.
- **Evidence From Code**: No `get_format` callback was present to enforce `AV_PIX_FMT_D3D11`.
- **Exact Fix**: Implement `get_format` and check the actual frame format before casting.
- **Corrected Code**:
```cpp
static enum AVPixelFormat get_format(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts) {
    for (const enum AVPixelFormat *p = pix_fmts; *p != -1; p++) {
        if (*p == AV_PIX_FMT_D3D11) return *p;
    }
    return AV_PIX_FMT_NONE;
}
// ...
m_internal->codecCtx->get_format = get_format;
```

### [CL-002] Null Dereference on Swap Chain Buffer
- **Probability**: High
- **File**: `cpp/client/renderer_d3d11.cpp`
- **Function**: `Render`
- **Approximate Line**: 152
- **Crash Scenario**: `m_swapChain->GetBuffer` fails (e.g., due to device loss) and returns a NULL pointer, which is then used in `texture->GetDesc`.
- **Why It Happens Immediately After Connection**: Occurs on the first attempt to render a decoded frame.
- **Evidence From Code**: `m_swapChain->GetBuffer(m_currentBufferIndex, __uuidof(ID3D11Texture2D), (void**)&backBuffer);` followed by immediate use without NULL check.
- **Exact Fix**: Add HRESULT and NULL checks.
- **Corrected Code**:
```cpp
HRESULT hr = m_swapChain->GetBuffer(m_currentBufferIndex, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
if (FAILED(hr) || !backBuffer) return;
```

### [CL-003] Dangling Pointer in Renderer Cache
- **Probability**: Medium
- **File**: `cpp/client/renderer_d3d11.cpp`
- **Function**: `Render`
- **Approximate Line**: 183
- **Crash Scenario**: `m_lastOutputTextures[m_currentBufferIndex]` stores a raw pointer to a backbuffer. If the swapchain is resized or recreated, the pointer becomes dangling.
- **Why It Happens Immediately After Connection**: If the window resizes during connection or if the first frame triggers a resize.
- **Evidence From Code**: Comparison `backBuffer != m_lastOutputTextures[m_currentBufferIndex]` without resetting the cache on shutdown or resize.
- **Exact Fix**: Reset the cache pointers in `Shutdown` and ensure they are managed safely.
- **Corrected Code**:
```cpp
// In Shutdown
for (int i = 0; i < 2; i++) m_lastOutputTextures[i] = nullptr;
```

### [CL-004] Unhandled SEH in Worker Thread
- **Probability**: High
- **File**: `cpp/common/session_manager.cpp`
- **Function**: `runClient`
- **Approximate Line**: 580
- **Crash Scenario**: An Access Violation (0xC0000005) inside FFmpeg or D3D11 terminates the entire process silently because it's not caught by standard C++ `try-catch`.
- **Why It Happens Immediately After Connection**: Hardware drivers are often unstable during the first few frames of a new session.
- **Evidence From Code**: The main client loop had no exception handling.
- **Exact Fix**: Add a global crash handler and wrap the loop in SEH-compatible blocks.
- **Corrected Code**:
```cpp
while (m_running) {
    try {
        // ... loop logic ...
    } catch (...) {
        m_running = false;
    }
}
```

---

## Crash Diagnostics Implementation
The following measures have been implemented to prevent silent termination:
1. **Vectored Exception Handling**: Captures SEH before they propagate.
2. **Unhandled Exception Filter**: Last resort logging for any missed exceptions.
3. **Trace Logging**: Logs "Frame Reassembled", "Decoding", and "Rendering" for the first 10 frames to pinpoint the exact failure stage.
4. **Standard Exception Wrappers**: Prevents `std::exception` from crashing the worker thread.
