// Copyright (c) 2026 Xnuva Blockchain Project
// Experimental shared RandomX v2 adapter. No chain consensus/activation logic.
// BSD-style upstream RandomX license must remain with its dependency.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

namespace xnuva::randomx_foundation {

using Digest = std::array<std::uint8_t, 32>;

// Deterministic RandomX V2 *raw* digest; no blockchain-specific byte reversal.
// The caller supplies the chain's consensus-defined RandomX seed/key.
class LightHasher final {
public:
    explicit LightHasher(std::span<const std::byte> seed_key);
    ~LightHasher();

    LightHasher(const LightHasher&) = delete;
    LightHasher& operator=(const LightHasher&) = delete;
    LightHasher(LightHasher&&) = delete;
    LightHasher& operator=(LightHasher&&) = delete;

    // Reuses a 256-MiB light cache; hashes synchronously and serializes
    // concurrent callers to protect mutable VM state.
    [[nodiscard]] Digest Hash(std::span<const std::byte> input) const;

    // Callers must serialize their own chain's header to exactly 80 bytes.
    // This method deliberately does not depend on a coin's CBlockHeader.
    [[nodiscard]] Digest Hash80ByteHeader(
        std::span<const std::byte> serialized_header) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Optional full-memory RandomX V2 hashing engine.
// Intended for mining; ordinary validating nodes may use LightHasher.
class FullHasher final {
public:
    explicit FullHasher(std::span<const std::byte> seed_key,
                        std::size_t worker_count = 1);
    ~FullHasher();

    FullHasher(const FullHasher&) = delete;
    FullHasher& operator=(const FullHasher&) = delete;
    FullHasher(FullHasher&&) = delete;
    FullHasher& operator=(FullHasher&&) = delete;

    [[nodiscard]] Digest Hash(std::span<const std::byte> input) const;
    [[nodiscard]] Digest Hash80ByteHeader(
        std::span<const std::byte> serialized_header) const;

    // Each worker has its own VM. The dataset is shared.
    [[nodiscard]] Digest HashForWorker(
        std::size_t worker_index,
        std::span<const std::byte> input) const;

    [[nodiscard]] Digest Hash80ByteHeaderForWorker(
        std::size_t worker_index,
        std::span<const std::byte> serialized_header) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Simple fixture helper; it reallocates a light cache on every call.
// Do not use this helper for production block validation or mining loops.
[[nodiscard]] Digest HashOnce(std::span<const std::byte> seed_key,
                              std::span<const std::byte> input);

} // namespace xnuva::randomx_foundation
