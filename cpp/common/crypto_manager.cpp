#include "crypto_manager.hpp"
#include <sodium.h>
#include <stdexcept>
#include <cstring>
#include "logger.hpp"

namespace Crypto {

bool CryptoManager::Initialize() {
    if (sodium_init() < 0) {
        LOG_ERROR("Crypto", "libsodium initialization failed!");
        return false;
    }
    return true;
}

CryptoManager::KeyPair CryptoManager::GenerateX25519KeyPair() {
    KeyPair kp;
    kp.publicKey.resize(crypto_kx_PUBLICKEYBYTES);
    kp.privateKey.resize(crypto_kx_SECRETKEYBYTES);
    crypto_kx_keypair(kp.publicKey.data(), kp.privateKey.data());
    return kp;
}

std::vector<uint8_t> CryptoManager::ComputeSharedSecret(const std::vector<uint8_t>& privateKey, const std::vector<uint8_t>& peerPublicKey) {
    if (privateKey.size() != crypto_kx_SECRETKEYBYTES || peerPublicKey.size() != crypto_kx_PUBLICKEYBYTES) {
        throw std::runtime_error("Invalid key sizes for X25519");
    }
    std::vector<uint8_t> sharedSecret(crypto_scalarmult_BYTES);
    if (crypto_scalarmult(sharedSecret.data(), privateKey.data(), peerPublicKey.data()) != 0) {
        throw std::runtime_error("Shared secret computation failed (weak public key?)");
    }
    return sharedSecret;
}

bool CryptoManager::DeriveSessionKeys(const std::vector<uint8_t>& publicKey, const std::vector<uint8_t>& privateKey,
                                      const std::vector<uint8_t>& peerPublicKey,
                                      std::vector<uint8_t>& rxKey, std::vector<uint8_t>& txKey,
                                      bool isServer) {
    rxKey.resize(crypto_kx_SESSIONKEYBYTES);
    txKey.resize(crypto_kx_SESSIONKEYBYTES);

    int res;
    if (isServer) {
        res = crypto_kx_server_session_keys(rxKey.data(), txKey.data(),
                                            publicKey.data(), privateKey.data(),
                                            peerPublicKey.data());
    } else {
        res = crypto_kx_client_session_keys(rxKey.data(), txKey.data(),
                                            publicKey.data(), privateKey.data(),
                                            peerPublicKey.data());
    }
    return res == 0;
}

bool CryptoManager::Encrypt(const uint8_t* plaintext, size_t plaintextLen,
                            const uint8_t* ad, size_t adLen,
                            uint64_t nonce, const std::vector<uint8_t>& key,
                            uint8_t* ciphertext, uint8_t* tag) {
    // XChaCha20 nonce is 24 bytes. We use 8 bytes for our sequence number and 16 bytes of zero padding.
    uint8_t npub[crypto_aead_xchacha20poly1305_ietf_NPUBBYTES];
    std::memset(npub, 0, sizeof(npub));
    std::memcpy(npub, &nonce, sizeof(nonce));

    unsigned long long clen;
    int res = crypto_aead_xchacha20poly1305_ietf_encrypt_detached(ciphertext, tag, &clen,
                                                                  plaintext, plaintextLen,
                                                                  ad, adLen,
                                                                  nullptr, npub, key.data());
    return res == 0;
}

bool CryptoManager::Decrypt(const uint8_t* ciphertext, size_t ciphertextLen,
                            const uint8_t* tag,
                            const uint8_t* ad, size_t adLen,
                            uint64_t nonce, const std::vector<uint8_t>& key,
                            uint8_t* plaintext) {
    uint8_t npub[crypto_aead_xchacha20poly1305_ietf_NPUBBYTES];
    std::memset(npub, 0, sizeof(npub));
    std::memcpy(npub, &nonce, sizeof(nonce));

    int res = crypto_aead_xchacha20poly1305_ietf_decrypt_detached(plaintext, nullptr,
                                                                  ciphertext, ciphertextLen,
                                                                  tag,
                                                                  ad, adLen,
                                                                  npub, key.data());
    return res == 0;
}

std::vector<uint8_t> CryptoManager::GenerateRandomBytes(size_t len) {
    std::vector<uint8_t> res(len);
    randombytes_buf(res.data(), len);
    return res;
}

} // namespace Crypto
