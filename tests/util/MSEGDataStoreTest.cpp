#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "util/MSEGDataStore.h"

#include <atomic>
#include <cstdint>
#include <thread>

namespace
{
void prepareAudioData(gin::MSEG::Data& data)
{
    data.points.resize(data.maxPoints);
    data.reset();
}
}

TEST_CASE("MSEG curve publications are immutable and versioned", "[ui][mseg][snapshot]")
{
    MSEGDataStore store;
    gin::MSEG::Data audioData;
    prepareAudioData(audioData);

    std::uint64_t version = 0;
    REQUIRE(store.copyLatestAudioData(audioData, version));
    REQUIRE(version == store.getPublishedVersion());
    REQUIRE(audioData.numPoints == 3);

    auto& editorData = store.getEditorData();
    editorData.numPoints = 4;
    editorData.startIndex = 1;
    editorData.endIndex = 3;
    editorData.points.setUnchecked(0, {0.0f, -0.5f, 1.0f});
    editorData.points.setUnchecked(1, {0.25f, 0.75f, 2.0f});
    editorData.points.setUnchecked(2, {0.5f, -0.25f, 3.0f});
    editorData.points.setUnchecked(3, {1.0f, -0.5f, 4.0f});

    const auto oldVersion = version;
    store.commitEditorData();

    REQUIRE(store.getPublishedVersion() > oldVersion);
    REQUIRE(store.copyLatestAudioData(audioData, version));
    REQUIRE(version == store.getPublishedVersion());
    REQUIRE(audioData.numPoints == 4);
    REQUIRE(audioData.startIndex == 1);
    REQUIRE(audioData.endIndex == 3);
    REQUIRE(audioData.points[1].value == Catch::Approx(0.75f));
    REQUIRE(audioData.points[2].curve == Catch::Approx(3.0f));
}

TEST_CASE("Concurrent MSEG publication never exposes a partially written curve",
          "[ui][mseg][snapshot][threading]")
{
    MSEGDataStore store;
    auto& editorData = store.getEditorData();
    editorData.numPoints = 16;
    editorData.startIndex = 0;
    editorData.endIndex = 15;

    std::atomic<bool> stop { false };
    std::atomic<bool> incoherent { false };
    std::thread reader([&]
    {
        gin::MSEG::Data audioData;
        prepareAudioData(audioData);
        std::uint64_t version = 0;

        while (!stop.load(std::memory_order_acquire))
        {
            if (!store.copyLatestAudioData(audioData, version) || audioData.numPoints != 16)
                continue;

            const auto generation = audioData.points[0].value;
            for (int i = 0; i < audioData.numPoints; ++i)
            {
                if (audioData.points[i].value != generation
                    || audioData.points[i].curve != -generation)
                {
                    incoherent.store(true, std::memory_order_release);
                    return;
                }
            }
        }
    });

    for (int generation = 1; generation <= 2000; ++generation)
    {
        for (int point = 0; point < editorData.numPoints; ++point)
        {
            editorData.points.setUnchecked(point, {
                static_cast<float>(point) / static_cast<float>(editorData.numPoints - 1),
                static_cast<float>(generation),
                static_cast<float>(-generation),
            });
        }
        store.commitEditorData();
    }

    stop.store(true, std::memory_order_release);
    reader.join();
    REQUIRE_FALSE(incoherent.load(std::memory_order_acquire));
}
