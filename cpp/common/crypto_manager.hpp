#pragma once

#include <vector>
#include <string>
#include <cstdint>

#ifdef PARSEC_LITE_ENABLE_SODIUM
#include <sodium.h>
#endif

namespace Crypto {

class CryptoManager {
public:
    static bool Initialize();

    struct KeyPair {
        std::vector<uint8_t> publicKey;
        std::vector<uint8_t> privateKey;
    };

    static KeyPair GenerateX25519KeyPair();
    static std::vector<uint8_t> ComputeSharedSecret(const std::vector<uint8_t>& privateKey, const std::vector<uint8_t>& peerPublicKey);

    // Derive SessionKeys (32 bytes each) for TX and RX
    static bool DeriveSessionKeys(const std::vector<uint8_t>& publicKey, const std::vector<uint8_t>& privateKey,
                                  const std::vector<uint8_t>& peerPublicKey,
                                  std::vector<uint8_t>& rxKey, std::vector<uint8_t>& txKey,
                                  bool isServer);

    // XChaCha20-Poly1305 Encrypt/Decrypt
    static bool Encrypt(const uint8_t* plaintext, size_t plaintextLen,
                        const uint8_t* ad, size_t adLen,
                        uint64_t nonce, const std::vector<uint8_t>& key,
                        uint8_t* ciphertext, uint8_t* tag);

    static bool Decrypt(const uint8_t* ciphertext, size_t ciphertextLen,
                        const uint8_t* tag,
                        const uint8_t* ad, size_t adLen,
                        uint64_t nonce, const std::vector<uint8_t>& key,
                        uint8_t* plaintext);

    static std::vector<uint8_t> GenerateRandomBytes(size_t len);
};

} // namespace Crypto
