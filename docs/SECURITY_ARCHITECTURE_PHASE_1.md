# Security Architecture Overview - Phase 1 Hardening

## 1. Handshake Protocol Design (X25519 Key Exchange)

The system transitions from an unauthenticated handshake to a secure ephemeral key exchange using Curve25519 (X25519).

### Step-by-Step Flow:
1.  **Client Hello**: Client generates an ephemeral X25519 key pair. It sends `HandshakePacket` containing:
    *   `PacketType::HandshakeSecure`
    *   `ClientPublicKey` (32 bytes)
    *   `Username` (32 bytes)
2.  **Host Challenge/Response**: Host receives the packet, generates its own ephemeral X25519 key pair.
    *   Computes shared secret: `X25519(HostPrivateKey, ClientPublicKey)`.
    *   Derives `SessionKey` and `MacKey` using BLAKE2b/HKDF.
    *   Sends `HandshakeResponsePacket` containing:
        *   `HostPublicKey` (32 bytes)
        *   `Approved` status
        *   `SessionID` (8 bytes, random)
3.  **Client Finalization**: Client receives the response.
    *   Computes shared secret: `X25519(ClientPrivateKey, HostPublicKey)`.
    *   Derives the same `SessionKey` and `MacKey`.
    *   Transitions to `SECURE_STREAMING` state.

## 2. Encryption Model

### Algorithms
*   **Cipher**: XChaCha20-Poly1305 (AEAD).
*   **Rationale**:
    *   High performance on CPUs without AES-NI.
    *   Resistance to nonce-misuse (XChaCha20 has a 192-bit nonce).
    *   Integrated authentication (Poly1305).
    *   Sub-millisecond encryption/decryption latency.

### Key Management
*   **Per-Session Keys**: Keys are never reused across sessions.
*   **Forward Secrecy**: Ephemeral keys are wiped after session termination.

## 3. Packet Structure Definition

All streaming packets (Video, Audio, Input, Feedback) are wrapped in a Secure Envelope.

| Field | Size | Description |
| :--- | :--- | :--- |
| **Type** | 1 Byte | `PacketType::Secure` |
| **Session ID** | 8 Bytes | Unique ID for the current session to prevent cross-session crosstalk. |
| **Sequence Number** | 8 Bytes | Monotonically increasing counter for replay protection. |
| **Auth Tag** | 16 Bytes | Poly1305 MAC for integrity and authenticity. |
| **Payload** | Variable | Encrypted data (original packet header + data). |

> **Note on Audio**: The current Phase 1 implementation covers Video, Input, and Feedback channels. Audio streaming is not part of the current protocol and is scheduled for design and implementation in **Phase 2 (Media Pipeline Architecture)**.

## 4. Session Lifecycle Design

1.  **Session Creation**: `IDLE` -> `HANDSHAKE_PENDING`.
2.  **Authentication Phase**: Exchange of public keys and derivation of secrets.
3.  **Active Streaming**: All packets must be encrypted and carry a valid Auth Tag and Sequence Number.
4.  **Reconnect Handling**:
    *   Resume: Short-term reconnection using existing Session ID and a "Resume Token" (Future phase).
    *   Reset: Full re-handshake (Default for Phase 1).
5.  **Session Expiration**: Automatic teardown after 30s of silence.

## 5. Threat Model & Mitigations

| Threat | Scenario | Mitigation |
| :--- | :--- | :--- |
| **MITM** | Attacker intercepts handshake. | Ephemeral ECDH ensures forward secrecy. Phase 2 will add certificate pinning. |
| **Replay Attacks** | Attacker captures and re-sends valid packets. | 64-bit Sequence Numbers + Sliding Window validation (Size 1024). |
| **Packet Injection** | Attacker sends malformed UDP packets. | Poly1305 Auth Tag validation; packets failing MAC check are dropped immediately. |
| **Session Hijacking** | Attacker steals Session ID. | Attacker lacks the Session Key derived from ECDH, so they cannot produce valid MACs. |
| **UDP Spoofing** | Attacker spoofs Host IP. | Handshake requires two-way exchange of secrets; spoofing fails the secret derivation. |

## 6. Performance Considerations

*   **Zero-Copy Integration**: Encrypt directly from the packet pool buffers to avoid extra copies.
*   **Non-Blocking IO**: Encryption happens in the `packetizerThread` and decryption in the `receiverThread`, ensuring the `captureThread` and `rendererThread` remain unblocked.
*   **Latency Target**: < 1ms overhead per packet for crypto operations.

## 7. Implementation Guidelines (C++)

*   Use **libsodium** for all cryptographic primitives.
*   Ensure `sodium_init()` is called globally.
*   Use `crypto_kx_*` for key exchange.
*   Use `crypto_aead_xchacha20poly1305_ietf_*` for streaming data.
