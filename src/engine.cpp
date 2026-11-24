// Copyright (c) 2026 Xnuva Blockchain Project
// Isolated RandomX V2 hashing library; NOT consensus or mining integration.
#include <xnuva_randomx/engine.h>

#include <randomx.h>

#include <memory>
#include <thread>
#include <vector>
#include <algorithm>
#include <mutex>
#include <stdexcept>

namespace xnuva::randomx_foundation {
namespace {
struct CacheDeleter {
    void operator()(randomx_cache* value) const noexcept
    {
        if (value != nullptr) randomx_release_cache(value);
    }
};
struct VMDeleter {
    void operator()(randomx_vm* value) const noexcept
    {
        if (value != nullptr) randomx_destroy_vm(value);
    }
};
using CachePtr = std::unique_ptr<randomx_cache, CacheDeleter>;
using VMPtr = std::unique_ptr<randomx_vm, VMDeleter>;
} // namespace

struct LightHasher::Impl {
    CachePtr cache;
    VMPtr vm;
    mutable std::mutex mutex;

    explicit Impl(std::span<const std::byte> seed_key)
    {
        if (seed_key.empty()) {
            throw std::invalid_argument("RandomX seed key must be nonempty");
        }
        // No JIT/large pages: portable light-mode reference first.
        cache.reset(randomx_alloc_cache(RANDOMX_FLAG_DEFAULT));
        if (!cache) {
            throw std::runtime_error("RandomX light cache allocation failed");
        }
        randomx_init_cache(cache.get(), seed_key.data(), seed_key.size());

        // V2 is intentional: this is the consensus variant in XNUVA.
        vm.reset(randomx_create_vm(RANDOMX_FLAG_V2, cache.get(), nullptr));
        if (!vm) {
            throw std::runtime_error("RandomX V2 light VM allocation failed");
        }
    }
};

LightHasher::LightHasher(std::span<const std::byte> seed_key)
    : impl_(std::make_unique<Impl>(seed_key))
{
}

LightHasher::~LightHasher() = default;

Digest LightHasher::Hash(std::span<const std::byte> input) const
{
    if (input.empty()) {
        throw std::invalid_argument("RandomX input must be nonempty");
    }
    std::lock_guard<std::mutex> lock(impl_->mutex);
    Digest result{};
    randomx_calculate_hash(
        impl_->vm.get(), input.data(), input.size(), result.data());
    return result;
}

Digest LightHasher::Hash80ByteHeader(
    std::span<const std::byte> serialized_header) const
{
    if (serialized_header.size() != 80) {
        throw std::invalid_argument(
            "RandomX block header must be exactly 80 serialized bytes");
    }
    return Hash(serialized_header);
}


namespace {
struct DatasetDeleter {
    void operator()(randomx_dataset* value) const noexcept
    {
        if (value != nullptr) randomx_release_dataset(value);
    }
};
using DatasetPtr = std::unique_ptr<randomx_dataset, DatasetDeleter>;
} // namespace

struct FullHasher::Impl {
    struct Worker {
        VMPtr vm;
        mutable std::mutex mutex;
    };

    CachePtr cache;
    DatasetPtr dataset;
    std::vector<std::unique_ptr<Worker>> workers;

    explicit Impl(std::span<const std::byte> seed_key,
                  std::size_t worker_count)
    {
        if (seed_key.empty()) {
            throw std::invalid_argument("RandomX seed key must be nonempty");
        }
        if (worker_count == 0 || worker_count > 16) {
            throw std::invalid_argument(
                "RandomX worker count must be between 1 and 16");
        }

        cache.reset(randomx_alloc_cache(RANDOMX_FLAG_DEFAULT));
        if (!cache) {
            throw std::runtime_error("RandomX full-mode cache allocation failed");
        }

        randomx_init_cache(cache.get(), seed_key.data(), seed_key.size());

        dataset.reset(randomx_alloc_dataset(RANDOMX_FLAG_DEFAULT));
        if (!dataset) {
            throw std::runtime_error("RandomX dataset allocation failed");
        }

        const unsigned long count = randomx_dataset_item_count();
        if (count == 0) {
            throw std::runtime_error("RandomX dataset has zero items");
        }

        // Independent, non-overlapping dataset ranges.
        // All workers finish before the V2 mining VM is created.
        constexpr unsigned long max_workers = 8;
        const unsigned long init_worker_count =
            std::min(count, max_workers);

        {
            std::vector<std::jthread> workers;
            workers.reserve(init_worker_count);

            const unsigned long base = count / init_worker_count;
            const unsigned long remainder = count % init_worker_count;

            unsigned long start = 0;

            for (unsigned long i = 0; i < init_worker_count; ++i) {
                const unsigned long items =
                    base + (i < remainder ? 1UL : 0UL);

                workers.emplace_back(
                    &randomx_init_dataset,
                    dataset.get(),
                    cache.get(),
                    start,
                    items
                );

                start += items;
            }

            // jthread destructors join every worker before
            // dataset use or resource destruction.
        }


        workers.reserve(worker_count);

        for (std::size_t i = 0; i < worker_count; ++i) {
            auto worker = std::make_unique<Worker>();

            worker->vm.reset(randomx_create_vm(
                static_cast<randomx_flags>(
                    RANDOMX_FLAG_V2 | RANDOMX_FLAG_FULL_MEM),
                nullptr,
                dataset.get()));

            if (!worker->vm) {
                throw std::runtime_error(
                    "RandomX V2 mining VM creation failed");
            }

            workers.push_back(std::move(worker));
        }
    }
};

FullHasher::FullHasher(std::span<const std::byte> seed_key,
                       std::size_t worker_count)
    : impl_(std::make_unique<Impl>(seed_key, worker_count))
{
}

FullHasher::~FullHasher() = default;

Digest FullHasher::Hash(std::span<const std::byte> input) const
{
    return HashForWorker(0, input);
}

Digest FullHasher::Hash80ByteHeader(
    std::span<const std::byte> serialized_header) const
{
    return Hash80ByteHeaderForWorker(0, serialized_header);
}

Digest FullHasher::HashForWorker(
    std::size_t worker_index,
    std::span<const std::byte> input) const
{
    if (input.empty()) {
        throw std::invalid_argument("RandomX input must be nonempty");
    }

    if (worker_index >= impl_->workers.size()) {
        throw std::out_of_range("Invalid RandomX worker index");
    }

    auto& worker = *impl_->workers[worker_index];
    std::lock_guard<std::mutex> lock(worker.mutex);

    Digest result{};
    randomx_calculate_hash(
        worker.vm.get(), input.data(), input.size(), result.data());

    return result;
}

Digest FullHasher::Hash80ByteHeaderForWorker(
    std::size_t worker_index,
    std::span<const std::byte> serialized_header) const
{
    if (serialized_header.size() != 80) {
        throw std::invalid_argument(
            "RandomX block header must be exactly 80 serialized bytes");
    }

    return HashForWorker(worker_index, serialized_header);
}

Digest HashOnce(std::span<const std::byte> seed_key,
                std::span<const std::byte> input)
{
    return LightHasher{seed_key}.Hash(input);
}

} // namespace xnuva::randomx_foundation
