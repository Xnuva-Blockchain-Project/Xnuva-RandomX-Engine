// Copyright (c) 2026 Xnuva Blockchain Project
// Experimental RandomX V2 full/light memory parity test.

#include <xnuva_randomx/engine.h>

#include <array>
#include <cassert>
#include <cstddef>
#include <iostream>
#include <stdexcept>

using namespace xnuva::randomx_foundation;

int main()
{
    const std::array<std::byte, 32> seed = [] {
        std::array<std::byte, 32> s{};
        for (size_t i = 0; i < s.size(); ++i)
            s[i] = std::byte(i);
        return s;
    }();

    std::array<std::byte, 80> header{};
    for (size_t i = 0; i < header.size(); ++i)
        header[i] = std::byte((i * 7) & 0xff);

    std::cout << "Initializing RandomX V2 light engine...\n";
    LightHasher light{seed};

    std::cout << "Initializing RandomX V2 full dataset...\n";
    FullHasher full{seed};

    std::cout << "Comparing first header...\n";
    const Digest light_hash = light.Hash80ByteHeader(header);
    const Digest full_hash = full.Hash80ByteHeader(header);

    if (light_hash != full_hash)
        throw std::runtime_error(
            "FAIL: RandomX V2 full/light hash mismatch");

    std::cout << "PASS: identical 80-byte header hashes\n";

    header[79] ^= std::byte{0x01};

    const Digest light_second = light.Hash80ByteHeader(header);
    const Digest full_second = full.Hash80ByteHeader(header);

    if (light_second != full_second)
        throw std::runtime_error(
            "FAIL: modified header hash mismatch");

    if (light_second == light_hash)
        throw std::runtime_error(
            "FAIL: modified header did not change hash");

    std::cout << "PASS: modified header parity\n";

    if (full.Hash80ByteHeader(header) != full_second)
        throw std::runtime_error(
            "FAIL: repeated full-memory hash changed");

    std::cout << "PASS: repeat hashing deterministic\n";

    bool rejected = false;

    try {
        std::array<std::byte, 79> invalid{};
        (void)full.Hash80ByteHeader(invalid);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }

    if (!rejected)
        throw std::runtime_error(
            "FAIL: invalid header length was accepted");

    std::cout << "PASS: invalid header length rejected\n";

    std::cout << "Testing second RandomX seed...\n";

    auto seed_two = seed;
    seed_two[0] ^= std::byte{0x5a};

    LightHasher light_two{seed_two};
    FullHasher full_two{seed_two};

    const Digest light_two_hash =
        light_two.Hash80ByteHeader(header);

    const Digest full_two_hash =
        full_two.Hash80ByteHeader(header);

    if (light_two_hash != full_two_hash)
        throw std::runtime_error(
            "FAIL: second seed light/full mismatch");

    if (light_two_hash == light_second)
        throw std::runtime_error(
            "FAIL: changing RandomX seed did not change hash");

    if (full.Hash80ByteHeader(header) != full_second)
        throw std::runtime_error(
            "FAIL: original seed engine changed after new seed");

    if (full_two.Hash80ByteHeader(header) != full_two_hash)
        throw std::runtime_error(
            "FAIL: second seed engine not deterministic");

    std::cout << "PASS: seed transition hashes agree\n";
    std::cout << "PASS: original seed remains stable\n";

    std::cout << "ALL FULL-MEMORY V2 TESTS PASSED\n";

    return 0;
}
