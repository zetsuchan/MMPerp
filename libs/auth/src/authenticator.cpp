#include "tradecore/auth/authenticator.hpp"

#include <sodium.h>

#include <cstring>
#include <stdexcept>

namespace tradecore {
namespace auth {

namespace {

class SodiumInitializer {
 public:
  SodiumInitializer() {
    if (sodium_init() < 0) {
      throw std::runtime_error("Failed to initialize libsodium");
    }
  }
};

// Ensure sodium is initialized before any crypto operations
void ensure_sodium_init() {
  static SodiumInitializer init;
}

}  // namespace

Authenticator::Authenticator() {
  ensure_sodium_init();
}

Authenticator::~Authenticator() = default;

void Authenticator::register_account(common::AccountId account, const PublicKey& public_key) {
  std::lock_guard<std::mutex> lock(mutex_);
  keys_[account] = public_key;
}

void Authenticator::unregister_account(common::AccountId account) {
  std::lock_guard<std::mutex> lock(mutex_);
  keys_.erase(account);
}

bool Authenticator::has_account(common::AccountId account) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return keys_.find(account) != keys_.end();
}

const PublicKey* Authenticator::get_public_key(common::AccountId account) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = keys_.find(account);
  if (it == keys_.end()) {
    return nullptr;
  }
  return &it->second;
}

bool Authenticator::verify(common::AccountId account,
                           std::span<const std::byte> message,
                           const Signature& signature) const {
  const PublicKey* key = get_public_key(account);
  if (!key) {
    return false;
  }
  return verify_with_key(*key, message, signature);
}

bool Authenticator::verify_with_key(const PublicKey& public_key,
                                    std::span<const std::byte> message,
                                    const Signature& signature) {
  ensure_sodium_init();

  return crypto_sign_verify_detached(
             signature.data(),
             reinterpret_cast<const unsigned char*>(message.data()),
             message.size(),
             public_key.data()) == 0;
}

bool Authenticator::sign(const SecretKey& secret_key,
                         std::span<const std::byte> message,
                         Signature& out_signature) {
  ensure_sodium_init();

  return crypto_sign_detached(
             out_signature.data(),
             nullptr,
             reinterpret_cast<const unsigned char*>(message.data()),
             message.size(),
             secret_key.data()) == 0;
}

void Authenticator::generate_keypair(PublicKey& out_public, SecretKey& out_secret) {
  ensure_sodium_init();
  crypto_sign_keypair(out_public.data(), out_secret.data());
}

std::size_t Authenticator::account_count() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return keys_.size();
}

FrameAuthenticator::FrameAuthenticator(const Authenticator& auth) : auth_(auth) {}

bool FrameAuthenticator::verify_frame(const void* header_data, std::size_t header_size,
                                      std::span<const std::byte> payload,
                                      common::AccountId account) const {
  // Payload must contain at least the signature
  if (payload.size() < kSignatureSize) {
    return false;
  }

  // Extract signature from beginning of payload
  Signature signature;
  std::memcpy(signature.data(), payload.data(), kSignatureSize);

  // Message is: header + remaining payload (after signature)
  std::vector<std::byte> message;
  message.reserve(header_size + payload.size() - kSignatureSize);

  // Append header
  const auto* header_bytes = static_cast<const std::byte*>(header_data);
  message.insert(message.end(), header_bytes, header_bytes + header_size);

  // Append payload after signature
  if (payload.size() > kSignatureSize) {
    message.insert(message.end(), payload.begin() + kSignatureSize, payload.end());
  }

  return auth_.verify(account, message, signature);
}

}  // namespace auth
}  // namespace tradecore
