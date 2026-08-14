#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "dsp/WaveguideKernel.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <vector>

namespace
{
using namespace resonarium::waveguide;

struct Buffers
{
    explicit Buffers(int samples)
        : exciterL(static_cast<std::size_t>(samples)), exciterR(static_cast<std::size_t>(samples)),
          previousL(static_cast<std::size_t>(samples)), previousR(static_cast<std::size_t>(samples)),
          bankInputL(static_cast<std::size_t>(samples)), bankInputR(static_cast<std::size_t>(samples)),
          outputL(static_cast<std::size_t>(samples)), outputR(static_cast<std::size_t>(samples)),
          soloL(static_cast<std::size_t>(samples)), soloR(static_cast<std::size_t>(samples))
    {
    }

    WaveguideProcessView view()
    {
        return { { exciterL.data(), exciterR.data() },
                 { previousL.data(), previousR.data() },
                 { bankInputL.data(), bankInputR.data() },
                 { outputL.data(), outputR.data() },
                 { soloL.data(), soloR.data() },
                 static_cast<int>(outputL.size()) };
    }

    void clearOutput()
    {
        std::fill(outputL.begin(), outputL.end(), 0.0f);
        std::fill(outputR.begin(), outputR.end(), 0.0f);
        std::fill(soloL.begin(), soloL.end(), 0.0f);
        std::fill(soloR.begin(), soloR.end(), 0.0f);
    }

    std::vector<float> exciterL, exciterR, previousL, previousR, bankInputL, bankInputR,
        outputL, outputR, soloL, soloR;
};

WaveguideControlFrame oneResonatorFrame(WaveguideCouplingMode mode)
{
    WaveguideControlFrame frame;
    frame.couplingMode = mode;
    for (int channel = 0; channel < WaveguideControlFrame::channelCount; ++channel)
    {
        auto& lane = frame.lanes[static_cast<std::size_t>(WaveguideKernel::laneIndex(0, channel))];
        lane.enabled = true;
        lane.frequencyHz = 6000.0f;
        lane.decaySeconds = 0.75f;
        lane.dispersion = 0.23f;
        lane.gain = 1.0f;
        lane.loopFilter = { 12000.0f, 0.7082f, 0.0f, true };
        lane.postFilter = { 16000.0f, 0.7072f, 0.0f, false };
    }
    return frame;
}

void zeroAllGains(WaveguideControlFrame& frame)
{
    for (auto& lane : frame.lanes)
        lane.gain = 0.0f;
}

void enableResonator(WaveguideControlFrame& frame, int resonator, float frequency = 6000.0f)
{
    for (int channel = 0; channel < WaveguideControlFrame::channelCount; ++channel)
    {
        auto& lane = frame.lanes[static_cast<std::size_t>(WaveguideKernel::laneIndex(
            resonator, channel))];
        lane.enabled = true;
        lane.frequencyHz = frequency;
        lane.decaySeconds = 0.75f;
        lane.dispersion = 0.23f;
        lane.gain = 1.0f;
        lane.loopFilter = { 12000.0f, 0.7082f, 0.0f, true };
        lane.postFilter = { 8000.0f, 0.9f, 0.0f, false };
    }
}

float rms(const std::vector<float>& values)
{
    double energy = 0.0;
    for (const auto value : values)
        energy += static_cast<double>(value) * value;
    return static_cast<float>(std::sqrt(energy / static_cast<double>(values.size())));
}

float peak(const std::vector<float>& values)
{
    float result = 0.0f;
    for (const auto value : values)
        result = std::max(result, std::abs(value));
    return result;
}

bool allFinite(const std::vector<float>& values)
{
    return std::all_of(values.begin(), values.end(), [](float value) { return std::isfinite(value); });
}

float peakScaledDifference(const std::vector<float>& left,
                           const std::vector<float>& right,
                           float rightScale)
{
    float result = 0.0f;
    for (std::size_t sample = 0; sample < left.size(); ++sample)
        result = std::max(result, std::abs(left[sample] - right[sample] * rightScale));
    return result;
}
} // namespace

TEST_CASE("waveguide scalar kernel has exact bounded storage and silent disabled output",
          "[waveguide][kernel][production]")
{
    constexpr int samples = 128;
    WaveguideKernel kernel;
    REQUIRE(kernel.prepare(48000.0, samples));
    CHECK(kernel.getDelayCapacitySamples() == WaveguideKernel::calculateDelayCapacity(48000.0));
    CHECK(kernel.getDelayStorageBytes()
          == static_cast<std::size_t>(kernel.getDelayCapacitySamples()
                                      * WaveguideKernel::laneCount)
                 * sizeof(float));

    WaveguideControlFrame frame;
    kernel.update(frame, samples, WaveguideRetuneMode::snap);
    Buffers buffers(samples);
    buffers.exciterL[0] = 1.0f;
    buffers.exciterR[0] = -0.5f;
    auto view = buffers.view();
    kernel.process(view);
    CHECK(peak(buffers.outputL) == 0.0f);
    CHECK(peak(buffers.outputR) == 0.0f);
}

TEST_CASE("interlinked scalar kernel routes an asymmetric right impulse through the right post path",
          "[waveguide][kernel][production][routing][right-channel]")
{
    constexpr int samples = 256;
    WaveguideKernel kernel;
    REQUIRE(kernel.prepare(48000.0, samples));
    auto frame = oneResonatorFrame(WaveguideCouplingMode::interlinked);
    kernel.update(frame, samples, WaveguideRetuneMode::snap);

    Buffers buffers(samples);
    buffers.exciterR[0] = 1.0f;
    auto view = buffers.view();
    kernel.process(view);

    CHECK(peak(buffers.outputL) <= 1.0e-8f);
    CHECK(rms(buffers.outputR) > 1.0e-7f);
}

TEST_CASE("scalar kernel consumes a distinct previous-bank input",
          "[waveguide][kernel][production][routing][multibank]")
{
    constexpr int samples = 256;
    WaveguideKernel kernel;
    REQUIRE(kernel.prepare(48000.0, samples));
    auto frame = oneResonatorFrame(WaveguideCouplingMode::parallel);
    frame.inputMix = 1.0f;

    Buffers routed(samples);
    routed.previousR[0] = 1.0f;
    kernel.update(frame, samples, WaveguideRetuneMode::snap);
    auto routedView = routed.view();
    kernel.process(routedView);
    const auto routedRms = rms(routed.outputR);

    kernel.reset();
    Buffers zeroPrevious(samples);
    kernel.update(frame, samples, WaveguideRetuneMode::snap);
    auto zeroView = zeroPrevious.view();
    kernel.process(zeroView);

    CHECK(routedRms > 1.0e-7f);
    CHECK(peak(zeroPrevious.outputL) <= 1.0e-8f);
    CHECK(peak(zeroPrevious.outputR) <= 1.0e-8f);
}

TEST_CASE("waveguide scalar kernel capacity is exact across low through high host rates",
          "[waveguide][kernel][production][capacity]")
{
    struct RateCase
    {
        double sampleRate;
        int expectedCapacity;
    };
    constexpr std::array cases {
        RateCase { 32000.0, 4808 },
        RateCase { 44100.0, 6623 },
        RateCase { 48000.0, 7208 },
        RateCase { 96000.0, 14408 },
        RateCase { 176400.0, 26468 },
        RateCase { 192000.0, 28808 },
    };

    for (const auto& rateCase : cases)
    {
        CAPTURE(rateCase.sampleRate);
        CHECK(WaveguideKernel::calculateDelayCapacity(rateCase.sampleRate)
              == rateCase.expectedCapacity);
        WaveguideKernel kernel;
        REQUIRE(kernel.prepare(rateCase.sampleRate, 128));
        CHECK(kernel.getDelayCapacitySamples() == rateCase.expectedCapacity);
        CHECK(kernel.getDelayStorageBytes()
              == static_cast<std::size_t>(rateCase.expectedCapacity
                                          * WaveguideKernel::laneCount)
                     * sizeof(float));
        CHECK(kernel.isDelayStorageAligned());

        WaveguideControlFrame frame;
        zeroAllGains(frame);
        enableResonator(frame, 0, 6000.0f);
        kernel.update(frame, 128, WaveguideRetuneMode::snap);
        Buffers buffers(128);
        buffers.exciterL[0] = 1.0f;
        buffers.exciterR[0] = -0.5f;
        auto view = buffers.view();
        kernel.process(view);
        CHECK(allFinite(buffers.outputL));
        CHECK(allFinite(buffers.outputR));
        CHECK(peak(buffers.outputL) > 1.0e-7f);
        CHECK(peak(buffers.outputR) > 1.0e-7f);
    }
}

TEST_CASE("waveguide scalar kernel rejects invalid rates, unsafe capacities, and block sizes",
          "[waveguide][kernel][production][capacity][safety]")
{
    constexpr std::array invalidRates {
        0.0,
        -1.0,
        std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::quiet_NaN(),
        1.0e9,
        std::numeric_limits<double>::max(),
    };

    for (const auto sampleRate : invalidRates)
    {
        CAPTURE(sampleRate);
        CHECK(WaveguideKernel::calculateDelayCapacity(sampleRate) == 0);
        WaveguideKernel kernel;
        CHECK_FALSE(kernel.prepare(sampleRate, 128));
    }

    WaveguideKernel zeroBlock;
    CHECK_FALSE(zeroBlock.prepare(48000.0, 0));
    WaveguideKernel negativeBlock;
    CHECK_FALSE(negativeBlock.prepare(48000.0, -1));

    WaveguideKernel reprepare;
    REQUIRE(reprepare.prepare(48000.0, 128));
    REQUIRE(reprepare.isPrepared());
    CHECK_FALSE(reprepare.prepare(0.0, 128));
    CHECK_FALSE(reprepare.isPrepared());
    CHECK(reprepare.getSampleRate() == 0.0);
    CHECK(reprepare.getMaximumBlockSize() == 0);
    REQUIRE(reprepare.prepare(192000.0, 64));
    CHECK(reprepare.isPrepared());
    CHECK(reprepare.getSampleRate() == 192000.0);
    CHECK(reprepare.getMaximumBlockSize() == 64);
}

TEST_CASE("kernel control cache clamps 10 Hz and preserves decay boundary semantics",
          "[waveguide][kernel][production][controls][decay]")
{
    constexpr int samples = 128;
    WaveguideKernel kernel;
    REQUIRE(kernel.prepare(48000.0, samples));

    WaveguideControlFrame frame;
    zeroAllGains(frame);
    enableResonator(frame, 0, 1.0f);
    for (int channel = 0; channel < WaveguideControlFrame::channelCount; ++channel)
    {
        auto& lane = frame.lanes[static_cast<std::size_t>(WaveguideKernel::laneIndex(0, channel))];
        lane.decaySeconds = 0.029999f;
        lane.loopFilter.mode = 0.5f;
        lane.loopFilter.frequencyIsPeak = false;
    }

    kernel.update(frame, samples, WaveguideRetuneMode::snap);
    CHECK(kernel.getControlCache().enabledMask == 0x01u);
    CHECK(kernel.getControlCache().enabledCount == 1u);
    CHECK(kernel.getControlCache().enabledIndices[0] == 0u);
    CHECK((kernel.getControlCache().passthroughMask & 0x0003u) == 0x0003u);

    for (int channel = 0; channel < WaveguideControlFrame::channelCount; ++channel)
        frame.lanes[static_cast<std::size_t>(WaveguideKernel::laneIndex(0, channel))]
            .decaySeconds = 0.03f;
    kernel.update(frame, samples, WaveguideRetuneMode::snap);

    const auto& activeCache = kernel.getControlCache();
    CHECK((activeCache.passthroughMask & 0x0003u) == 0u);
    for (int channel = 0; channel < WaveguideControlFrame::channelCount; ++channel)
    {
        const auto& lane = activeCache.lanes[static_cast<std::size_t>(
            WaveguideKernel::laneIndex(0, channel))];
        CHECK(lane.frequencyHz == 10.0f);
        CHECK(lane.decaySeconds == 0.03f);
        CHECK_THAT(lane.delaySamples, Catch::Matchers::WithinAbs(4798.97f, 1.0e-3f));
        CHECK(lane.decayCoefficient > 0.0f);
        CHECK(lane.decayCoefficient < 1.0f);
    }

    for (int channel = 0; channel < WaveguideControlFrame::channelCount; ++channel)
        frame.lanes[static_cast<std::size_t>(WaveguideKernel::laneIndex(0, channel))]
            .decaySeconds = 60.0f;
    kernel.update(frame, samples, WaveguideRetuneMode::snap);
    for (int channel = 0; channel < WaveguideControlFrame::channelCount; ++channel)
        CHECK(kernel.getControlCache()
                  .lanes[static_cast<std::size_t>(WaveguideKernel::laneIndex(0, channel))]
                  .decayCoefficient
              == 1.0f);
}

TEST_CASE("sparse cascade caches ascending enabled lanes and zero gain exits untouched",
          "[waveguide][kernel][production][cascade][sparse][masks][zero-gain]")
{
    constexpr int samples = 256;
    WaveguideKernel kernel;
    REQUIRE(kernel.prepare(48000.0, samples));

    WaveguideControlFrame frame;
    frame.couplingMode = WaveguideCouplingMode::cascade;
    zeroAllGains(frame);
    enableResonator(frame, 1);
    enableResonator(frame, 5, 7200.0f);
    for (int resonator = 0; resonator < WaveguideControlFrame::resonatorCount; ++resonator)
    {
        if (resonator == 1 || resonator == 5)
            continue;
        for (int channel = 0; channel < WaveguideControlFrame::channelCount; ++channel)
            frame.lanes[static_cast<std::size_t>(WaveguideKernel::laneIndex(
                resonator, channel))]
                .gain = 0.25f;
    }
    frame.cascadeAmount = { 0.4f, -0.2f };
    frame.cascadeFilter = {
        WaveguideFilterControl { 5000.0f, 0.8f, 0.25f, false },
        WaveguideFilterControl { 7000.0f, 0.9f, 0.75f, false },
    };
    frame.soloResonator = 5;
    kernel.update(frame, samples, WaveguideRetuneMode::snap);

    const auto& cache = kernel.getControlCache();
    CHECK(cache.couplingMode == WaveguideCouplingMode::cascade);
    CHECK(cache.enabledMask == 0x22u);
    CHECK(cache.enabledCount == 2u);
    CHECK(cache.enabledIndices[0] == 1u);
    CHECK(cache.enabledIndices[1] == 5u);
    const auto expectedLaneMask = static_cast<std::uint16_t>((1u << 2) | (1u << 3)
                                                             | (1u << 10) | (1u << 11));
    CHECK(cache.enabledLaneMask == expectedLaneMask);
    CHECK_THAT(cache.totalGain[0], Catch::Matchers::WithinAbs(2.0f, 1.0e-7f));
    CHECK_THAT(cache.totalGain[1], Catch::Matchers::WithinAbs(2.0f, 1.0e-7f));

    Buffers sparse(samples);
    sparse.exciterL[0] = 1.0f;
    sparse.exciterR[0] = -0.75f;
    auto sparseView = sparse.view();
    kernel.process(sparseView);
    CHECK(allFinite(sparse.outputL));
    CHECK(allFinite(sparse.outputR));
    CHECK(peak(sparse.outputL) > 1.0e-7f);
    CHECK(peak(sparse.outputR) > 1.0e-7f);
    CHECK(peak(sparse.soloL) > 1.0e-7f);
    CHECK(peak(sparse.soloR) > 1.0e-7f);

    // An enabled zero-audible-gain lane still advances while another enabled
    // lane keeps the bank denominator non-zero.
    for (int channel = 0; channel < WaveguideControlFrame::channelCount; ++channel)
        frame.lanes[static_cast<std::size_t>(WaveguideKernel::laneIndex(5, channel))]
            .gain = 0.0f;
    kernel.reset();
    kernel.update(frame, samples, WaveguideRetuneMode::snap);
    CHECK(kernel.getControlCache().enabledMask == 0x22u);
    CHECK(kernel.getControlCache().totalGain[0] == 1.0f);
    CHECK(kernel.getControlCache().totalGain[1] == 1.0f);
    Buffers mutedLane(samples);
    mutedLane.exciterL[0] = 1.0f;
    mutedLane.exciterR[0] = 1.0f;
    auto mutedLaneView = mutedLane.view();
    kernel.process(mutedLaneView);
    const auto mutedLaneMask = static_cast<std::uint16_t>((1u << 10) | (1u << 11));
    CHECK((kernel.getControlCache().coldMask & mutedLaneMask) == 0u);

    for (auto& lane : frame.lanes)
        lane.gain = 0.0f;
    kernel.update(frame, samples, WaveguideRetuneMode::snap);
    Buffers zeroGain(samples);
    std::fill(zeroGain.outputL.begin(), zeroGain.outputL.end(), 0.375f);
    std::fill(zeroGain.outputR.begin(), zeroGain.outputR.end(), -0.25f);
    std::fill(zeroGain.bankInputL.begin(), zeroGain.bankInputL.end(), 0.625f);
    std::fill(zeroGain.bankInputR.begin(), zeroGain.bankInputR.end(), -0.5f);
    zeroGain.exciterL[0] = 1.0f;
    auto zeroGainView = zeroGain.view();
    kernel.process(zeroGainView);
    CHECK(std::all_of(zeroGain.outputL.begin(), zeroGain.outputL.end(),
                      [](float value) { return value == 0.375f; }));
    CHECK(std::all_of(zeroGain.outputR.begin(), zeroGain.outputR.end(),
                      [](float value) { return value == -0.25f; }));
    CHECK(std::all_of(zeroGain.bankInputL.begin(), zeroGain.bankInputL.end(),
                      [](float value) { return value == 0.625f; }));
    CHECK(std::all_of(zeroGain.bankInputR.begin(), zeroGain.bankInputR.end(),
                      [](float value) { return value == -0.5f; }));
}

TEST_CASE("solo capture preserves topology-specific solo behavior",
          "[waveguide][kernel][production][solo][topology]")
{
    constexpr int samples = 256;
    for (const auto mode : { WaveguideCouplingMode::parallel,
                             WaveguideCouplingMode::interlinked,
                             WaveguideCouplingMode::cascade })
    {
        CAPTURE(static_cast<int>(mode));
        WaveguideKernel kernel;
        REQUIRE(kernel.prepare(48000.0, samples));
        WaveguideControlFrame frame;
        zeroAllGains(frame);
        enableResonator(frame, 0);
        frame.couplingMode = mode;
        frame.outputGain = 0.5f;
        frame.soloResonator = 0;
        kernel.update(frame, samples, WaveguideRetuneMode::snap);

        Buffers buffers(samples);
        buffers.exciterL[0] = 1.0f;
        buffers.exciterR[0] = -0.5f;
        auto view = buffers.view();
        kernel.process(view);
        CHECK(allFinite(buffers.outputL));
        CHECK(allFinite(buffers.outputR));
        CHECK(peak(buffers.soloL) > 1.0e-7f);
        CHECK(peak(buffers.soloR) > 1.0e-7f);

        const auto leftDifference = peakScaledDifference(buffers.outputL, buffers.soloL, frame.outputGain);
        const auto rightDifference = peakScaledDifference(buffers.outputR, buffers.soloR, frame.outputGain);
        if (mode == WaveguideCouplingMode::interlinked)
        {
            CHECK(leftDifference > 1.0e-7f);
            CHECK(rightDifference > 1.0e-7f);
        }
        else
        {
            CHECK(leftDifference <= 1.0e-6f);
            CHECK(rightDifference <= 1.0e-6f);
        }
    }
}

TEST_CASE("topology changes and one-sample quanta remain finite without pending retunes",
          "[waveguide][kernel][production][topology][one-sample][retune]")
{
    WaveguideKernel kernel;
    REQUIRE(kernel.prepare(48000.0, 1));
    WaveguideControlFrame frame;
    zeroAllGains(frame);
    enableResonator(frame, 0, 4000.0f);
    Buffers buffers(1);
    float accumulatedMagnitude = 0.0f;

    for (const auto mode : { WaveguideCouplingMode::parallel,
                             WaveguideCouplingMode::interlinked,
                             WaveguideCouplingMode::cascade })
    {
        frame.couplingMode = mode;
        for (int quantum = 0; quantum < 32; ++quantum)
        {
            const auto frequency = 4000.0f + static_cast<float>(quantum) * 37.0f;
            for (int channel = 0; channel < WaveguideControlFrame::channelCount; ++channel)
                frame.lanes[static_cast<std::size_t>(WaveguideKernel::laneIndex(0, channel))]
                    .frequencyHz = frequency;

            kernel.update(frame, 1, WaveguideRetuneMode::crossfade);
            CHECK(kernel.getControlCache().couplingMode == mode);
            CHECK(kernel.getRetuningLaneMask() == 0u);
            CHECK(kernel.getControlCache().retuningMask == 0u);

            buffers.clearOutput();
            buffers.exciterL[0] = quantum == 0 ? 1.0f : 0.01f;
            buffers.exciterR[0] = quantum == 0 ? -0.5f : -0.005f;
            auto view = buffers.view();
            kernel.process(view);
            REQUIRE(std::isfinite(buffers.outputL[0]));
            REQUIRE(std::isfinite(buffers.outputR[0]));
            accumulatedMagnitude += std::abs(buffers.outputL[0]) + std::abs(buffers.outputR[0]);
        }
    }

    CHECK(accumulatedMagnitude > 1.0e-7f);
}

TEST_CASE("the first effective pitch after reset snaps while later pitch changes retune",
          "[waveguide][kernel][production][reset][retune]")
{
    constexpr int samples = 128;
    constexpr std::uint16_t firstStereoPairMask = 0x0003u;
    WaveguideKernel kernel;
    REQUIRE(kernel.prepare(48000.0, samples));

    WaveguideControlFrame frame;
    zeroAllGains(frame);
    enableResonator(frame, 0, 4000.0f);
    kernel.update(frame, samples, WaveguideRetuneMode::crossfade);
    CHECK((kernel.getRetuningLaneMask() & firstStereoPairMask) == 0u);

    for (int channel = 0; channel < WaveguideControlFrame::channelCount; ++channel)
        frame.lanes[static_cast<std::size_t>(WaveguideKernel::laneIndex(0, channel))]
            .frequencyHz = 5000.0f;
    kernel.update(frame, samples, WaveguideRetuneMode::crossfade);
    CHECK((kernel.getRetuningLaneMask() & firstStereoPairMask)
          == firstStereoPairMask);

    // reset() deliberately preserves coefficient/control caches. A new note at
    // the same effective pitch must still consume the assignment snap; the
    // first later bend must not inherit that snap and silently skip its fade.
    kernel.reset();
    kernel.update(frame, 0, WaveguideRetuneMode::crossfade);
    CHECK((kernel.getRetuningLaneMask() & firstStereoPairMask) == 0u);

    for (int channel = 0; channel < WaveguideControlFrame::channelCount; ++channel)
        frame.lanes[static_cast<std::size_t>(WaveguideKernel::laneIndex(0, channel))]
            .frequencyHz = 6000.0f;
    kernel.update(frame, samples, WaveguideRetuneMode::crossfade);
    CHECK((kernel.getRetuningLaneMask() & firstStereoPairMask)
          == firstStereoPairMask);

    kernel.reset();
    kernel.update(frame, 0, WaveguideRetuneMode::crossfade);
    const auto stationaryDelay = kernel.getControlCache().lanes[0].delaySamples;
    for (int channel = 0; channel < WaveguideControlFrame::channelCount; ++channel)
        frame.lanes[static_cast<std::size_t>(WaveguideKernel::laneIndex(0, channel))]
            .frequencyHz = 7000.0f;
    kernel.update(frame, 1, WaveguideRetuneMode::crossfade);

    // A one-sample quantum has no distinct pair of crossfade endpoints. It
    // commits the new tap immediately and leaves no pending fade.
    CHECK((kernel.getRetuningLaneMask() & firstStereoPairMask) == 0u);
    CHECK(kernel.getControlCache().lanes[0].delaySamples != stationaryDelay);
}

TEST_CASE("every resonator enable mask remains compact, ordered, and finite in every topology",
          "[waveguide][kernel][production][masks][topology]")
{
    constexpr int samples = 64;
    for (const auto mode : { WaveguideCouplingMode::parallel,
                             WaveguideCouplingMode::interlinked,
                             WaveguideCouplingMode::cascade })
    {
        for (unsigned int mask = 0; mask < 256; ++mask)
        {
            CAPTURE(static_cast<int>(mode), mask);
            WaveguideKernel kernel;
            REQUIRE(kernel.prepare(48000.0, samples));

            WaveguideControlFrame frame;
            frame.couplingMode = mode;
            zeroAllGains(frame);
            for (int resonator = 0; resonator < WaveguideControlFrame::resonatorCount; ++resonator)
            {
                if ((mask & (1u << resonator)) == 0)
                    continue;
                enableResonator(frame,
                                resonator,
                                3000.0f + 250.0f * static_cast<float>(resonator));
                for (int channel = 0; channel < WaveguideControlFrame::channelCount; ++channel)
                    frame.lanes[static_cast<std::size_t>(WaveguideKernel::laneIndex(
                        resonator, channel))]
                        .gain = 0.25f + 0.1f * static_cast<float>(resonator);
            }

            kernel.update(frame, samples, WaveguideRetuneMode::snap);
            const auto& cache = kernel.getControlCache();
            CHECK(cache.enabledMask == static_cast<std::uint8_t>(mask));
            CHECK(cache.enabledCount == static_cast<std::uint8_t>(std::popcount(mask)));
            int enabledIndex = 0;
            for (int resonator = 0; resonator < WaveguideControlFrame::resonatorCount; ++resonator)
                if ((mask & (1u << resonator)) != 0)
                    CHECK(cache.enabledIndices[static_cast<std::size_t>(enabledIndex++)]
                          == static_cast<std::uint8_t>(resonator));

            Buffers buffers(samples);
            buffers.exciterL[0] = 0.75f;
            buffers.exciterR[0] = -0.5f;
            auto view = buffers.view();
            kernel.process(view);
            CHECK(allFinite(buffers.outputL));
            CHECK(allFinite(buffers.outputR));
            if (mask == 0)
            {
                CHECK(peak(buffers.outputL) == 0.0f);
                CHECK(peak(buffers.outputR) == 0.0f);
            }
        }
    }
}

TEST_CASE("filter modes and dispersion endpoints remain finite and cache their exact endpoint rotations",
          "[waveguide][kernel][production][filters][dispersion]")
{
    constexpr int samples = 256;
    for (const float filterMode : { 0.0f, 0.5f, 1.0f })
    {
        for (const float dispersion : { 0.0f, 1.0f })
        {
            CAPTURE(filterMode, dispersion);
            WaveguideKernel kernel;
            REQUIRE(kernel.prepare(48000.0, samples));
            WaveguideControlFrame frame;
            zeroAllGains(frame);
            enableResonator(frame, 0, 6000.0f);
            for (int channel = 0; channel < WaveguideControlFrame::channelCount; ++channel)
            {
                auto& lane = frame.lanes[static_cast<std::size_t>(
                    WaveguideKernel::laneIndex(0, channel))];
                lane.dispersion = dispersion;
                lane.loopFilter = { 8000.0f, 0.9f, filterMode, false };
                lane.postFilter = { 12000.0f, 0.8f, filterMode, false };
            }
            kernel.update(frame, samples, WaveguideRetuneMode::snap);

            for (int channel = 0; channel < WaveguideControlFrame::channelCount; ++channel)
            {
                const auto& lane = kernel.getControlCache().lanes[static_cast<std::size_t>(
                    WaveguideKernel::laneIndex(0, channel))];
                if (dispersion == 0.0f)
                {
                    CHECK_THAT(lane.dispersionC, Catch::Matchers::WithinAbs(1.0f, 1.0e-7f));
                    CHECK_THAT(lane.dispersionS, Catch::Matchers::WithinAbs(0.0f, 1.0e-7f));
                }
                else
                {
                    CHECK_THAT(lane.dispersionC, Catch::Matchers::WithinAbs(0.0f, 1.0e-6f));
                    CHECK_THAT(lane.dispersionS, Catch::Matchers::WithinAbs(-1.0f, 1.0e-6f));
                }
                CHECK(std::isfinite(lane.loopFilter.gt0));
                CHECK(std::isfinite(lane.postFilter.gt0));
            }

            Buffers buffers(samples);
            buffers.exciterL[0] = 1.0f;
            buffers.exciterR[0] = -0.75f;
            auto view = buffers.view();
            kernel.process(view);
            CHECK(allFinite(buffers.outputL));
            CHECK(allFinite(buffers.outputR));
            CHECK(peak(buffers.outputL) > 1.0e-7f);
            CHECK(peak(buffers.outputR) > 1.0e-7f);
        }
    }
}

TEST_CASE("non-finite control tuples are sanitised before entering kernel state",
          "[waveguide][kernel][production][controls][safety]")
{
    constexpr int samples = 128;
    const auto nan = std::numeric_limits<float>::quiet_NaN();
    const auto infinity = std::numeric_limits<float>::infinity();

    WaveguideKernel kernel;
    REQUIRE(kernel.prepare(48000.0, samples));

    WaveguideControlFrame frame;
    zeroAllGains(frame);
    enableResonator(frame, 0);
    frame.couplingMode = static_cast<WaveguideCouplingMode>(255);
    frame.inputMix = nan;
    frame.inputGain = infinity;
    frame.outputGain = -infinity;
    frame.cascadeAmount = { nan, infinity };
    for (auto& filter : frame.cascadeFilter)
        filter = { nan, infinity, nan, false };

    for (int channel = 0; channel < WaveguideControlFrame::channelCount; ++channel)
    {
        auto& lane = frame.lanes[static_cast<std::size_t>(
            WaveguideKernel::laneIndex(0, channel))];
        lane.frequencyHz = nan;
        lane.decaySeconds = 0.75f;
        lane.dispersion = infinity;
        lane.gain = 1.0f;
        lane.loopFilter = { nan, infinity, nan, true };
        lane.postFilter = { infinity, nan, infinity, false };
    }

    kernel.update(frame, samples, WaveguideRetuneMode::snap);
    const auto& cache = kernel.getControlCache();
    CHECK(cache.couplingMode == WaveguideCouplingMode::parallel);
    CHECK(cache.exciterMix == 1.0f);
    CHECK(cache.previousBankMix == 0.0f);
    CHECK(cache.inputGain == 0.0f);
    CHECK(cache.outputGain == 0.0f);
    CHECK(cache.cascadeAmount[0] == 0.0f);
    CHECK(cache.cascadeAmount[1] == 0.0f);

    for (int channel = 0; channel < WaveguideControlFrame::channelCount; ++channel)
    {
        const auto& lane = cache.lanes[static_cast<std::size_t>(
            WaveguideKernel::laneIndex(0, channel))];
        CHECK(lane.frequencyHz == WaveguideKernel::minimumSupportedFrequencyHz);
        CHECK(lane.dispersion == 0.0f);
        CHECK(std::isfinite(lane.decayCoefficient));
        CHECK(std::isfinite(lane.delaySamples));
        CHECK(std::isfinite(lane.loopPhaseDelaySamples));
        CHECK(std::isfinite(lane.loopFilter.gt0));
        CHECK(std::isfinite(lane.loopFilter.outputGain));
        CHECK(std::isfinite(lane.postFilter.gt0));
        CHECK(std::isfinite(lane.postFilter.outputGain));
    }

    Buffers buffers(samples);
    buffers.exciterL[0] = 1.0f;
    buffers.exciterR[0] = -1.0f;
    auto view = buffers.view();
    kernel.process(view);
    CHECK(allFinite(buffers.outputL));
    CHECK(allFinite(buffers.outputR));
    CHECK(peak(buffers.outputL) == 0.0f);
    CHECK(peak(buffers.outputR) == 0.0f);
}
