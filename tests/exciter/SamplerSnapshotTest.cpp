#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "dsp/Sampler.h"

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace
{
using namespace std::chrono_literals;

struct CompletionState
{
    std::mutex mutex;
    std::condition_variable condition;
    bool called = false;
    Sampler::LoadResult result;
};

Sampler::LoadResult awaitInstall(Sampler& sampler, const juce::AudioBuffer<float>& source)
{
    auto state = std::make_shared<CompletionState>();
    REQUIRE(sampler.requestInstallForTesting(source, 48000.0, [state](Sampler::LoadResult result)
    {
        {
            const std::scoped_lock lock(state->mutex);
            state->result = std::move(result);
            state->called = true;
        }
        state->condition.notify_one();
    }));
    std::unique_lock lock(state->mutex);
    REQUIRE(state->condition.wait_for(lock, 5s, [&] { return state->called; }));
    return state->result;
}

Sampler::LoadResult awaitClear(Sampler& sampler)
{
    auto state = std::make_shared<CompletionState>();
    sampler.requestClear([state](Sampler::LoadResult result)
    {
        {
            const std::scoped_lock lock(state->mutex);
            state->result = std::move(result);
            state->called = true;
        }
        state->condition.notify_one();
    });
    std::unique_lock lock(state->mutex);
    REQUIRE(state->condition.wait_for(lock, 5s, [&] { return state->called; }));
    return state->result;
}

Sampler::LoadResult awaitLoad(Sampler& sampler, const juce::File& file)
{
    auto state = std::make_shared<CompletionState>();
    REQUIRE(sampler.requestLoadFile(file, [state](Sampler::LoadResult result)
    {
        {
            const std::scoped_lock lock(state->mutex);
            state->result = std::move(result);
            state->called = true;
        }
        state->condition.notify_one();
    }));
    std::unique_lock lock(state->mutex);
    REQUIRE(state->condition.wait_for(lock, 5s, [&] { return state->called; }));
    return state->result;
}

void writeLittleEndian16(std::array<std::uint8_t, 44>& bytes,
                         std::size_t offset,
                         std::uint16_t value)
{
    bytes[offset] = static_cast<std::uint8_t>(value);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8U);
}

void writeLittleEndian32(std::array<std::uint8_t, 44>& bytes,
                         std::size_t offset,
                         std::uint32_t value)
{
    for (std::size_t byte = 0; byte < 4; ++byte)
        bytes[offset + byte] = static_cast<std::uint8_t>(value >> (8U * byte));
}

juce::File makeOversizedWavHeader()
{
    std::array<std::uint8_t, 44> bytes {};
    const auto writeTag = [&bytes](std::size_t offset, const char* tag)
    {
        for (std::size_t byte = 0; byte < 4; ++byte)
            bytes[offset + byte] = static_cast<std::uint8_t>(tag[byte]);
    };

    constexpr std::uint32_t bytesPerFrame = 4;
    const auto dataBytes = static_cast<std::uint32_t>(
        (Sampler::maxDecodedFrames + 1) * bytesPerFrame);
    writeTag(0, "RIFF");
    writeLittleEndian32(bytes, 4, 36U + dataBytes);
    writeTag(8, "WAVE");
    writeTag(12, "fmt ");
    writeLittleEndian32(bytes, 16, 16);
    writeLittleEndian16(bytes, 20, 1);
    writeLittleEndian16(bytes, 22, 2);
    writeLittleEndian32(bytes, 24, 48000);
    writeLittleEndian32(bytes, 28, 48000U * bytesPerFrame);
    writeLittleEndian16(bytes, 32, bytesPerFrame);
    writeLittleEndian16(bytes, 34, 16);
    writeTag(36, "data");
    writeLittleEndian32(bytes, 40, dataBytes);

    auto file = juce::File::createTempFile("resonarium-oversized-sample.wav");
    REQUIRE(file.replaceWithData(bytes.data(), bytes.size()));
    return file;
}

bool waitForDestroyed(const Sampler& sampler, std::uint64_t count)
{
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (sampler.getTelemetry().destroyedOffAudioThread >= count)
            return true;
        std::this_thread::sleep_for(1ms);
    }
    return false;
}
} // namespace

TEST_CASE("Sampler snapshots normalize channels and remain latched by voices",
          "[exciter][sample][snapshot]")
{
    Sampler sampler;
    sampler.prepare({ 48000.0, 128, 2 });
    REQUIRE(sampler.getDisplayGeneration() == 0);

    juce::AudioBuffer<float> mono(1, 5);
    for (int sample = 0; sample < mono.getNumSamples(); ++sample)
        mono.setSample(0, sample, 0.1f * static_cast<float>(sample + 1));
    const auto first = awaitInstall(sampler, mono);
    REQUIRE(first.success);
    REQUIRE(sampler.getDisplayGeneration() == first.generation);
    REQUIRE(sampler.commitPendingAtBlockBoundary());
    auto firstHandle = sampler.acquireForVoice();
    REQUIRE(firstHandle);
    REQUIRE(firstHandle.generation == first.generation);
    for (int sample = 0; sample < mono.getNumSamples(); ++sample)
    {
        REQUIRE(firstHandle.getSample(0, sample) == Catch::Approx(mono.getSample(0, sample)));
        REQUIRE(firstHandle.getSample(1, sample) == Catch::Approx(mono.getSample(0, sample)));
    }

    juce::AudioBuffer<float> wide(3, 5);
    wide.clear();
    wide.applyGain(0, 0, 5, 0.0f);
    for (int sample = 0; sample < wide.getNumSamples(); ++sample)
    {
        wide.setSample(0, sample, 1.0f + static_cast<float>(sample));
        wide.setSample(1, sample, -2.0f - static_cast<float>(sample));
        wide.setSample(2, sample, 99.0f);
    }
    const auto second = awaitInstall(sampler, wide);
    REQUIRE(second.success);
    REQUIRE(sampler.getDisplayGeneration() == second.generation);
    REQUIRE(sampler.commitPendingAtBlockBoundary());
    auto secondHandle = sampler.acquireForVoice();
    REQUIRE(secondHandle);
    for (int sample = 0; sample < wide.getNumSamples(); ++sample)
    {
        REQUIRE(secondHandle.getSample(0, sample) == Catch::Approx(wide.getSample(0, sample)));
        REQUIRE(secondHandle.getSample(1, sample) == Catch::Approx(wide.getSample(1, sample)));
        REQUIRE(firstHandle.getSample(0, sample) == Catch::Approx(mono.getSample(0, sample)));
    }

    const auto cleared = awaitClear(sampler);
    REQUIRE(cleared.success);
    REQUIRE(sampler.getDisplayGeneration() == cleared.generation);
    REQUIRE(sampler.commitPendingAtBlockBoundary());
    REQUIRE_FALSE(sampler.acquireForVoice());
    REQUIRE(firstHandle.getSample(1, 4) == Catch::Approx(0.5f));
    REQUIRE(secondHandle.getSample(0, 4) == Catch::Approx(5.0f));

    sampler.releaseFromVoice(firstHandle);
    sampler.releaseFromVoice(secondHandle);
    REQUIRE_FALSE(firstHandle);
    REQUIRE_FALSE(secondHandle);
    REQUIRE(waitForDestroyed(sampler, 2));
    REQUIRE(sampler.getTelemetry().reclaimOverflows == 0);
}

TEST_CASE("Failed and coalesced sampler requests publish the newest generation safely",
          "[exciter][sample][transport]")
{
    Sampler sampler;
    sampler.prepare({ 48000.0, 128, 2 });
    juce::AudioBuffer<float> initial(2, 3);
    initial.clear();
    initial.setSample(0, 0, 0.25f);
    const auto installed = awaitInstall(sampler, initial);
    REQUIRE(installed.success);
    REQUIRE(sampler.commitPendingAtBlockBoundary());
    auto existingAssignment = sampler.acquireForVoice();
    REQUIRE(existingAssignment.generation == installed.generation);

    const auto invalidFile = juce::File::createTempFile("resonarium-invalid-sample");
    REQUIRE(invalidFile.replaceWithText("not audio"));
    auto failed = std::make_shared<CompletionState>();
    REQUIRE(sampler.requestLoadFile(invalidFile, [failed](Sampler::LoadResult result)
    {
        {
            const std::scoped_lock lock(failed->mutex);
            failed->result = std::move(result);
            failed->called = true;
        }
        failed->condition.notify_one();
    }));
    {
        std::unique_lock lock(failed->mutex);
        REQUIRE(failed->condition.wait_for(lock, 5s, [&] { return failed->called; }));
        REQUIRE_FALSE(failed->result.success);
    }
    REQUIRE(sampler.commitPendingAtBlockBoundary());
    REQUIRE_FALSE(sampler.acquireForVoice());
    REQUIRE(existingAssignment.getSample(0, 0) == Catch::Approx(0.25f));
    REQUIRE(sampler.getStatePath() == invalidFile.getFullPathName());
    sampler.releaseFromVoice(existingAssignment);
    REQUIRE(invalidFile.deleteFile());

    auto last = std::make_shared<CompletionState>();
    for (int generation = 0; generation < 64; ++generation)
    {
        juce::AudioBuffer<float> value(1, 1);
        value.setSample(0, 0, static_cast<float>(generation));
        const auto callback = generation == 63
                                  ? Sampler::CompletionCallback([last](Sampler::LoadResult result)
                                    {
                                        {
                                            const std::scoped_lock lock(last->mutex);
                                            last->result = std::move(result);
                                            last->called = true;
                                        }
                                        last->condition.notify_one();
                                    })
                                  : Sampler::CompletionCallback {};
        REQUIRE(sampler.requestInstallForTesting(value, 48000.0, callback));
    }
    {
        std::unique_lock lock(last->mutex);
        REQUIRE(last->condition.wait_for(lock, 5s, [&] { return last->called; }));
        REQUIRE(last->result.success);
    }
    REQUIRE(sampler.commitPendingAtBlockBoundary());
    auto newest = sampler.acquireForVoice();
    REQUIRE(newest);
    REQUIRE(newest.getSample(0, 0) == Catch::Approx(63.0f));
    REQUIRE(newest.generation == last->result.generation);
    sampler.releaseFromVoice(newest);
    REQUIRE(sampler.getTelemetry().coalesced > 0);
    REQUIRE(sampler.getTelemetry().reclaimOverflows == 0);
}

TEST_CASE("Concurrent sampler requests serialize state with generation order",
          "[exciter][sample][transport][concurrency]")
{
    Sampler sampler;
    sampler.prepare({ 48000.0, 128, 2 });

    const auto invalidFile = juce::File::createTempFile("resonarium-raced-sample");
    REQUIRE(invalidFile.replaceWithText("not audio"));

    for (int iteration = 0; iteration < 16; ++iteration)
    {
        auto load = std::make_shared<CompletionState>();
        auto clear = std::make_shared<CompletionState>();
        std::atomic<int> ready { 0 };
        std::atomic<bool> start { false };
        std::atomic<bool> loadAccepted { false };

        const auto complete = [](const std::shared_ptr<CompletionState>& state,
                                 Sampler::LoadResult result)
        {
            {
                const std::scoped_lock lock(state->mutex);
                state->result = std::move(result);
                state->called = true;
            }
            state->condition.notify_one();
        };

        std::thread loadThread([&]
        {
            ready.fetch_add(1, std::memory_order_release);
            while (!start.load(std::memory_order_acquire))
                std::this_thread::yield();
            loadAccepted.store(
                sampler.requestLoadFile(invalidFile, [load, complete](Sampler::LoadResult result)
                {
                    complete(load, std::move(result));
                }),
                std::memory_order_release);
        });

        std::thread clearThread([&]
        {
            ready.fetch_add(1, std::memory_order_release);
            while (!start.load(std::memory_order_acquire))
                std::this_thread::yield();
            sampler.requestClear([clear, complete](Sampler::LoadResult result)
            {
                complete(clear, std::move(result));
            });
        });

        while (ready.load(std::memory_order_acquire) != 2)
            std::this_thread::yield();
        start.store(true, std::memory_order_release);
        loadThread.join();
        clearThread.join();
        REQUIRE(loadAccepted.load(std::memory_order_acquire));

        {
            std::unique_lock lock(load->mutex);
            REQUIRE(load->condition.wait_for(lock, 5s, [&] { return load->called; }));
        }
        {
            std::unique_lock lock(clear->mutex);
            REQUIRE(clear->condition.wait_for(lock, 5s, [&] { return clear->called; }));
        }

        REQUIRE(load->result.generation != clear->result.generation);
        const auto loadIsNewest = load->result.generation > clear->result.generation;
        REQUIRE(sampler.getStatePath()
                == (loadIsNewest ? invalidFile.getFullPathName() : juce::String {}));

        // Drain any successful/null publications before the next race so this
        // test also exercises publication coalescing without filling the SPSC.
        static_cast<void>(sampler.commitPendingAtBlockBoundary());
    }

    REQUIRE(invalidFile.deleteFile());
    REQUIRE(sampler.getTelemetry().reclaimOverflows == 0);
}

TEST_CASE("Sampler rejects oversized decodes and contains worker callback exceptions",
          "[exciter][sample][transport][failure]")
{
    REQUIRE(Sampler::maxDecodedBytes == 256ULL * 1024ULL * 1024ULL);
    REQUIRE(Sampler::isDecodedLengthSupported(Sampler::maxDecodedFrames));
    REQUIRE_FALSE(Sampler::isDecodedLengthSupported(Sampler::maxDecodedFrames + 1));
    REQUIRE_FALSE(Sampler::isDecodedLengthSupported(0));

    Sampler sampler;
    sampler.prepare({ 48000.0, 128, 2 });

    const auto oversizedFile = makeOversizedWavHeader();
    const auto oversized = awaitLoad(sampler, oversizedFile);
    REQUIRE_FALSE(oversized.success);
    REQUIRE(oversized.error.containsIgnoreCase("limit"));
    REQUIRE(sampler.commitPendingAtBlockBoundary());
    REQUIRE_FALSE(sampler.acquireForVoice());
    REQUIRE(sampler.getTelemetry().oversizedRejected == 1);
    REQUIRE(oversizedFile.deleteFile());

    std::atomic<bool> throwingCallbackRan { false };
    juce::AudioBuffer<float> first(1, 1);
    first.setSample(0, 0, 0.25f);
    REQUIRE(sampler.requestInstallForTesting(first, 48000.0,
                                             [&throwingCallbackRan](Sampler::LoadResult)
    {
        throwingCallbackRan.store(true, std::memory_order_release);
        throw std::runtime_error("test callback failure");
    }));

    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while (!throwingCallbackRan.load(std::memory_order_acquire)
           && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(1ms);
    REQUIRE(throwingCallbackRan.load(std::memory_order_acquire));
    REQUIRE(sampler.commitPendingAtBlockBoundary());

    juce::AudioBuffer<float> second(1, 1);
    second.setSample(0, 0, 0.75f);
    const auto recovered = awaitInstall(sampler, second);
    REQUIRE(recovered.success);
    REQUIRE(sampler.commitPendingAtBlockBoundary());
    auto handle = sampler.acquireForVoice();
    REQUIRE(handle.getSample(0, 0) == Catch::Approx(0.75f));
    sampler.releaseFromVoice(handle);
    REQUIRE(sampler.getTelemetry().completionFailures == 1);
    REQUIRE(sampler.getTelemetry().workerFailures == 0);
}
