// Copyright (c) 2026 Xnuva Blockchain Project
// Isolated RandomX V2 multiworker hash-equivalence test.

#include <xnuva_randomx/engine.h>

#include <array>
#include <cstddef>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <thread>

using namespace xnuva::randomx_foundation;

int main()
{
    std::array<std::byte, 32> seed{};
    for (std::size_t i = 0; i < seed.size(); ++i)
        seed[i] = std::byte(i + 1);

    constexpr std::size_t worker_count = 4;

    std::cout << "Initialising shared V2 dataset...\n";
    FullHasher full{seed, worker_count};
    LightHasher light{seed};

    std::array<std::array<std::byte, 80>, worker_count> headers{};
    std::array<Digest, worker_count> actual{};
    std::array<Digest, worker_count> expected{};

    for (std::size_t w = 0; w < worker_count; ++w) {
        for (std::size_t i = 0; i < 80; ++i)
            headers[w][i] = std::byte((i * 11 + w * 17) & 0xff);

        expected[w] = light.Hash80ByteHeader(headers[w]);
    }

    std::array<std::exception_ptr, worker_count> errors{};
    std::array<std::thread, worker_count> threads;

    for (std::size_t w = 0; w < worker_count; ++w) {
        threads[w] = std::thread([&, w] {
            try {
                actual[w] =
                    full.Hash80ByteHeaderForWorker(w, headers[w]);
            } catch (...) {
                errors[w] = std::current_exception();
            }
        });
    }

    for (auto& thread : threads)
        thread.join();

    for (std::size_t w = 0; w < worker_count; ++w) {
        if (errors[w])
            std::rethrow_exception(errors[w]);

        if (actual[w] != expected[w])
            throw std::runtime_error(
                "FAIL: concurrent worker hash mismatch");
    }

    std::cout << "PASS: four concurrent VMs match LightHasher\n";

    if (full.Hash80ByteHeader(headers[0]) != expected[0])
        throw std::runtime_error(
            "FAIL: default worker compatibility");

    bool rejected = false;
    try {
        (void)full.Hash80ByteHeaderForWorker(
            worker_count, headers[0]);
    } catch (const std::out_of_range&) {
        rejected = true;
    }

    if (!rejected)
        throw std::runtime_error(
            "FAIL: invalid worker index accepted");

    std::cout << "PASS: default worker compatibility\n";
    std::cout << "PASS: invalid worker index rejected\n";

    // Multiple concurrent rounds using the same dataset and VMs.
    constexpr std::size_t rounds = 12;

    for (std::size_t round = 0; round < rounds; ++round) {
        std::array<std::thread, worker_count> jobs{};
        std::array<std::exception_ptr, worker_count> failures{};
        std::array<Digest, worker_count> results{};

        for (std::size_t w = 0; w < worker_count; ++w) {
            headers[w][76] = std::byte(round);
            headers[w][77] = std::byte(w);

            jobs[w] = std::thread([&, w] {
                try {
                    results[w] = full.Hash80ByteHeaderForWorker(
                        w, headers[w]);
                } catch (...) {
                    failures[w] = std::current_exception();
                }
            });
        }

        for (auto& job : jobs)
            job.join();

        for (std::size_t w = 0; w < worker_count; ++w) {
            if (failures[w])
                std::rethrow_exception(failures[w]);

            const Digest reference =
                light.Hash80ByteHeader(headers[w]);

            if (results[w] != reference)
                throw std::runtime_error(
                    "FAIL: repeated concurrent worker mismatch");
        }
    }

    std::cout << "PASS: 12 concurrent rounds, 48 hashes\n";

    // Invalid counts must fail before dataset allocation.
    for (std::size_t invalid_count : {std::size_t{0},
                                      std::size_t{17}}) {
        bool rejected_count = false;

        try {
            FullHasher invalid{seed, invalid_count};
        } catch (const std::invalid_argument&) {
            rejected_count = true;
        }

        if (!rejected_count)
            throw std::runtime_error(
                "FAIL: invalid worker count accepted");
    }

    std::cout << "PASS: invalid worker counts rejected\n";

    std::cout << "ALL CONCURRENT WORKER TESTS PASSED\n";
}
