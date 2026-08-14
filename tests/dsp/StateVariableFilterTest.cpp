#include <catch2/catch_test_macros.hpp>
#include "dsp/StateVariableFilter.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace
{
using resonarium::StateVariableFilter;

constexpr double lowHostRate = 32'000.0;
constexpr float requestedCutoff = 20'000.0f;
constexpr float expectedClampedCutoff = 15'999.0f;
}

TEST_CASE("state-variable filter clamps cutoff controls below low-rate Nyquist",
          "[dsp][filter][state-variable][sample-rate]")
{
    StateVariableFilter<float, 1, false> filter;
    filter.prepare({ lowHostRate, 512, 1 });

    filter.setCutoffFrequency(requestedCutoff);
    CHECK(filter.getCutoffFrequency() == expectedClampedCutoff);

    std::array<float, 512> output {};
    output.front() = 1.0e-28f;
    for (auto& sample : output)
        sample = filter.processSample(sample);

    CHECK(std::all_of(output.begin(), output.end(),
                      [](float sample) { return std::isfinite(sample); }));
    float peak = 0.0f;
    for (const auto sample : output)
        peak = std::max(peak, std::abs(sample));
    CHECK(peak < 1.0f);
}

TEST_CASE("state-variable filter clamps peak-frequency controls below low-rate Nyquist",
          "[dsp][filter][state-variable][sample-rate]")
{
    StateVariableFilter<float, 1, false> filter;
    filter.prepare({ lowHostRate, 128, 1 });
    filter.setQValue(1.0f);

    filter.setPeakFrequency(requestedCutoff);
    CHECK(filter.getCutoffFrequency() == expectedClampedCutoff);
}

TEST_CASE("state-variable filter preserves in-range cutoff controls",
          "[dsp][filter][state-variable][sample-rate]")
{
    StateVariableFilter<float, 1, false> filter;
    filter.prepare({ 48'000.0, 128, 1 });

    filter.setCutoffFrequency(requestedCutoff);
    CHECK(filter.getCutoffFrequency() == requestedCutoff);
}

TEST_CASE("state-variable filter caches effective low-rate controls",
          "[dsp][filter][state-variable][sample-rate]")
{
    StateVariableFilter<float, 1, false> filter;
    filter.prepare({ lowHostRate, 128, 1 });

    CHECK(filter.updateParameters(requestedCutoff,
                                  StateVariableFilter<float, 1, false>::inverseRootTwo,
                                  0.0f));
    CHECK(filter.getCutoffFrequency() == expectedClampedCutoff);
    CHECK_FALSE(filter.updateParameters(requestedCutoff,
                                        StateVariableFilter<float, 1, false>::inverseRootTwo,
                                        0.0f));
    CHECK_FALSE(filter.updateParameters(19'000.0f,
                                        StateVariableFilter<float, 1, false>::inverseRootTwo,
                                        0.0f));

    CHECK(filter.updateParameters(12'000.0f,
                                  StateVariableFilter<float, 1, false>::inverseRootTwo,
                                  0.0f));
    CHECK_FALSE(filter.updateParameters(12'000.0f,
                                        StateVariableFilter<float, 1, false>::inverseRootTwo,
                                        0.0f));
}

TEST_CASE("state-variable filter sanitizes unsafe resonance controls",
          "[dsp][filter][state-variable][safety]")
{
    using Filter = StateVariableFilter<float, 1, false>;
    Filter filter;
    filter.prepare({ 48'000.0, 128, 1 });

    CHECK(filter.updateParameters(1'000.0f, 0.0f, 0.0f));
    CHECK(filter.getQValue() == Filter::minimumResonance);
    CHECK_FALSE(filter.updateParameters(1'000.0f, -1.0f, 0.0f));
    CHECK_FALSE(filter.updateParameters(1'000.0f,
                                        std::numeric_limits<float>::quiet_NaN(),
                                        0.0f));
    CHECK_FALSE(filter.updateParameters(1'000.0f,
                                        std::numeric_limits<float>::infinity(),
                                        0.0f));

    std::array<float, 128> output {};
    output.front() = 1.0f;
    for (auto& sample : output)
        sample = filter.processSample(sample);

    CHECK(std::all_of(output.begin(), output.end(),
                      [](float sample) { return std::isfinite(sample); }));
}

TEST_CASE("state-variable peak-frequency conversion is finite at zero resonance",
          "[dsp][filter][state-variable][safety]")
{
    using Filter = StateVariableFilter<float, 1, false>;
    Filter filter;
    filter.prepare({ lowHostRate, 128, 1 });

    CHECK(filter.updateParameters(requestedCutoff, 0.0f, 0.0f, true));
    CHECK(filter.getCutoffFrequency() == expectedClampedCutoff);
    CHECK(filter.getQValue() == Filter::minimumResonance);
    CHECK_FALSE(filter.updateParameters(19'000.0f, -1.0f, 0.0f, true));

    std::array<float, 128> output {};
    output.front() = 1.0f;
    for (auto& sample : output)
        sample = filter.processSample(sample);

    CHECK(std::all_of(output.begin(), output.end(),
                      [](float sample) { return std::isfinite(sample); }));
}
