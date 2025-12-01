#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <mutex>

#include "tradecore/common/types.hpp"

namespace tradecore {
namespace auth {

// ed25519 key sizes
constexpr std::size_t kPublicKeySize = 32;
constexpr std::size_t kSecretKeySize = 64;
constexpr std::size_t kSignatureSize = 64;

using PublicKey = std::array<std::uint8_t, kPublicKeySize>;
using SecretKey = std::array<std::uint8_t, kSecretKeySize>;
using Signature = std::array<std::uint8_t, kSignatureSize>;

// Signed frame wire format:
// [signature:64][header:36][payload:N]
// The signature covers: header + payload (everything after signature)
struct SignedFrameHeader {
  Signature signature;
  // Followed by WireHeader and payload
};

class Authenticator {
 public:
  Authenticator();
  ~Authenticator();

  // Register a public key for an account
  void register_account(common::AccountId account, const PublicKey& public_key);

  // Remove an account's key
  void unregister_account(common::AccountId account);

  // Check if account is registered
  bool has_account(common::AccountId account) const;

  // Get account's public key (returns nullptr if not found)
  const PublicKey* get_public_key(common::AccountId account) const;

  // Verify a signature against a message using an account's registered key
  // Returns true if signature is valid
  bool verify(common::AccountId account,
              std::span<const std::byte> message,
              const Signature& signature) const;

  // Verify using explicit public key (for testing or one-off verification)
  static bool verify_with_key(const PublicKey& public_key,
                              std::span<const std::byte> message,
                              const Signature& signature);

  // Sign a message with a secret key (for testing/client use)
  static bool sign(const SecretKey& secret_key,
                   std::span<const std::byte> message,
                   Signature& out_signature);

  // Generate a new keypair (for testing/setup)
  static void generate_keypair(PublicKey& out_public, SecretKey& out_secret);

  // Number of registered accounts
  std::size_t account_count() const;

 private:
  mutable std::mutex mutex_;
  std::unordered_map<common::AccountId, PublicKey> keys_;
};

// Create an AuthVerifier callback for use with IngressPipeline
// The verifier expects signed frames where the signature is the first 64 bytes
// of the payload, covering the header bytes and remaining payload
class FrameAuthenticator {
 public:
  explicit FrameAuthenticator(const Authenticator& auth);

  // Verify a frame - expects signature as first 64 bytes of payload
  // Returns true if signature is valid
  bool verify_frame(const void* header_data, std::size_t header_size,
                    std::span<const std::byte> payload,
                    common::AccountId account) const;

 private:
  const Authenticator& auth_;
};

}  // namespace auth
}  // namespace tradecore
