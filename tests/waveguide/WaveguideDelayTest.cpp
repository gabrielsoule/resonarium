#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "dsp/DelayLine.h"
#include "dsp/WaveguideDelay.h"

#include <array>
#include <cmath>
#include <utility>

namespace
{
using resonarium::waveguide::WaveguideDelay;

float deterministicInput(int sample) noexcept
{
    const auto phase = static_cast<float>(sample) * 0.173f;
    return std::sin(phase) * 0.4f + std::cos(phase * 0.37f) * 0.15f;
}
} // namespace

TEST_CASE("waveguide delay matches the stationary Lagrange interpolation oracle",
          "[waveguide][delay][production]")
{
    constexpr int capacity = 64;
    constexpr int samples = 256;

    for (const float delaySamples : { 2.0f, 2.25f, 7.75f, 31.125f })
    {
        resonarium::DelayLine<float> oracle(capacity - 1);
        oracle.prepare(1);
        oracle.setDelay(delaySamples);

        WaveguideDelay subject;
        REQUIRE(subject.prepare(capacity));
        subject.setDelay(0, delaySamples, 1, WaveguideDelay::RetuneMode::snap);

        for (int sample = 0; sample < samples; ++sample)
        {
            const auto oracleOutput = oracle.popSample(0);
            const auto subjectOutput = subject.popSample(0);
            CHECK_THAT(subjectOutput,
                       Catch::Matchers::WithinAbs(oracleOutput, 1.0e-6f));

            const auto input = deterministicInput(sample);
            oracle.pushSample(0, input);
            subject.pushSample(0, input);
        }
    }
}

TEST_CASE("waveguide delay reset is logical and lane-local",
          "[waveguide][delay][production][reset]")
{
    constexpr int capacity = 48;
    WaveguideDelay delay;
    REQUIRE(delay.prepare(capacity));
    REQUIRE(delay.getAllocatedBytes()
            == static_cast<std::size_t>(WaveguideDelay::laneCount * capacity) * sizeof(float));

    delay.setDelay(0, 4.0f, 1, WaveguideDelay::RetuneMode::snap);
    delay.setDelay(1, 4.0f, 1, WaveguideDelay::RetuneMode::snap);
    for (int sample = 0; sample < 16; ++sample)
    {
        (void) delay.popSample(0);
        (void) delay.popSample(1);
        delay.pushSample(0, sample == 0 ? 1.0f : 0.0f);
        delay.pushSample(1, 0.0f);
    }

    CHECK(delay.getValidSamples(0) == 16);
    CHECK(delay.getValidSamples(1) == 16);
    delay.resetLane(0);
    CHECK(delay.getValidSamples(0) == 0);
    CHECK(delay.getValidSamples(1) == 16);

    for (int sample = 0; sample < capacity; ++sample)
    {
        CHECK(delay.popSample(0) == 0.0f);
        delay.pushSample(0, 0.0f);
    }
}

TEST_CASE("waveguide delay crossfade uses the old and new taps",
          "[waveguide][delay][production][retune]")
{
    constexpr int capacity = 96;
    constexpr int fadeSamples = 9;
    constexpr float oldDelay = 5.25f;
    constexpr float newDelay = 11.75f;

    WaveguideDelay subject;
    REQUIRE(subject.prepare(capacity));
    subject.setDelay(0, oldDelay, 1, WaveguideDelay::RetuneMode::snap);

    resonarium::DelayLine<float> oldOracle(capacity - 1);
    resonarium::DelayLine<float> newOracle(capacity - 1);
    oldOracle.prepare(1);
    newOracle.prepare(1);
    oldOracle.setDelay(oldDelay);
    newOracle.setDelay(newDelay);

    for (int sample = 0; sample < capacity; ++sample)
    {
        const auto input = deterministicInput(sample);
        (void) subject.popSample(0);
        (void) oldOracle.popSample(0);
        (void) newOracle.popSample(0);
        subject.pushSample(0, input);
        oldOracle.pushSample(0, input);
        newOracle.pushSample(0, input);
    }

    subject.setDelay(0, newDelay, fadeSamples, WaveguideDelay::RetuneMode::crossfade);
    for (int sample = 0; sample < fadeSamples; ++sample)
    {
        const auto oldOutput = oldOracle.popSample(0);
        const auto newOutput = newOracle.popSample(0);
        const auto alpha = static_cast<float>(sample) / static_cast<float>(fadeSamples - 1);
        const auto expected = oldOutput + (newOutput - oldOutput) * alpha;
        CHECK_THAT(subject.popSample(0), Catch::Matchers::WithinAbs(expected, 1.0e-6f));

        const auto input = deterministicInput(capacity + sample);
        subject.pushSample(0, input);
        oldOracle.pushSample(0, input);
        newOracle.pushSample(0, input);
    }
}

TEST_CASE("waveguide maximum tap wraps and remains equivalent after a cold reset",
          "[waveguide][delay][production][wrap][reset]")
{
    constexpr int stride = 80;
    constexpr int logicalCapacity = stride - 4;
    constexpr int samplesPerEpoch = logicalCapacity * 5;

    WaveguideDelay subject;
    REQUIRE(subject.prepare(stride));
    resonarium::DelayLine<float> oracle(logicalCapacity - 1);
    oracle.prepare(1);

    subject.setDelay(0, 100000.0f, 1, WaveguideDelay::RetuneMode::snap);
    const auto clampedDelay = subject.getDelay(0);
    CHECK(clampedDelay == static_cast<float>(logicalCapacity - 4));
    oracle.setDelay(clampedDelay);

    for (int epoch = 0; epoch < 2; ++epoch)
    {
        for (int sample = 0; sample < samplesPerEpoch; ++sample)
        {
            CHECK_THAT(subject.popSample(0),
                       Catch::Matchers::WithinAbs(oracle.popSample(0), 1.0e-6f));
            const auto input = deterministicInput(epoch * samplesPerEpoch + sample);
            subject.pushSample(0, input);
            oracle.pushSample(0, input);
        }

        CHECK((subject.getColdMask() & 1u) == 0u);
        subject.resetLane(0);
        oracle.reset();
        CHECK((subject.getColdMask() & 1u) != 0u);
    }
}

TEST_CASE("waveguide repeated retargets choose the dominant stationary tap",
          "[waveguide][delay][production][retarget][one-sample]")
{
    constexpr int stride = 128;
    constexpr int logicalCapacity = stride - 4;
    constexpr float oldDelay = 5.25f;
    constexpr float middleDelay = 13.5f;
    constexpr float newDelay = 27.75f;

    WaveguideDelay subject;
    REQUIRE(subject.prepare(stride));
    subject.setDelay(0, oldDelay, 1, WaveguideDelay::RetuneMode::snap);

    resonarium::DelayLine<float> oldOracle(logicalCapacity - 1);
    resonarium::DelayLine<float> middleOracle(logicalCapacity - 1);
    resonarium::DelayLine<float> newOracle(logicalCapacity - 1);
    for (auto* oracle : { &oldOracle, &middleOracle, &newOracle })
        oracle->prepare(1);
    oldOracle.setDelay(oldDelay);
    middleOracle.setDelay(middleDelay);
    newOracle.setDelay(newDelay);

    auto advance = [&](int sample, bool readSubject)
    {
        const auto subjectOutput = readSubject ? subject.popSample(0) : 0.0f;
        const std::array oracleOutputs {
            oldOracle.popSample(0),
            middleOracle.popSample(0),
            newOracle.popSample(0),
        };
        const auto input = deterministicInput(sample);
        subject.pushSample(0, input);
        oldOracle.pushSample(0, input);
        middleOracle.pushSample(0, input);
        newOracle.pushSample(0, input);
        return std::pair { subjectOutput, oracleOutputs };
    };

    int sampleIndex = 0;
    for (; sampleIndex < logicalCapacity + 16; ++sampleIndex)
        (void) advance(sampleIndex, true);

    // Retarget before the midpoint: the old tap remains the source of the next fade.
    subject.setDelay(0, middleDelay, 9, WaveguideDelay::RetuneMode::crossfade);
    for (int sample = 0; sample < 3; ++sample, ++sampleIndex)
        CHECK(std::isfinite(advance(sampleIndex, true).first));

    subject.setDelay(0, newDelay, 5, WaveguideDelay::RetuneMode::crossfade);
    for (int sample = 0; sample < 5; ++sample, ++sampleIndex)
    {
        const auto [actual, oracles] = advance(sampleIndex, true);
        const auto alpha = static_cast<float>(sample) / 4.0f;
        const auto expected = oracles[0] + (oracles[2] - oracles[0]) * alpha;
        CHECK_THAT(actual, Catch::Matchers::WithinAbs(expected, 1.0e-6f));
    }
    CHECK(subject.getRetuneRemainingSamples(0) == 0);

    // Retarget after the midpoint: the pending target becomes the new source tap.
    subject.setDelay(0, middleDelay, 9, WaveguideDelay::RetuneMode::crossfade);
    for (int sample = 0; sample < 5; ++sample, ++sampleIndex)
        CHECK(std::isfinite(advance(sampleIndex, true).first));

    subject.setDelay(0, oldDelay, 5, WaveguideDelay::RetuneMode::crossfade);
    for (int sample = 0; sample < 5; ++sample, ++sampleIndex)
    {
        const auto [actual, oracles] = advance(sampleIndex, true);
        const auto alpha = static_cast<float>(sample) / 4.0f;
        const auto expected = oracles[1] + (oracles[0] - oracles[1]) * alpha;
        CHECK_THAT(actual, Catch::Matchers::WithinAbs(expected, 1.0e-6f));
    }

    // A one-sample quantum snaps directly to the requested tap.
    subject.setDelay(0, newDelay, 1, WaveguideDelay::RetuneMode::crossfade);
    const auto [actual, oracles] = advance(sampleIndex, true);
    CHECK_THAT(actual, Catch::Matchers::WithinAbs(oracles[2], 1.0e-6f));
    CHECK(subject.getRetuneRemainingSamples(0) == 0);
    CHECK(subject.getDelay(0) == newDelay);
}
