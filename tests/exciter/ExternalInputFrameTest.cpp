#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ExternalInputFrame.h"

#include <array>
#include <cmath>

TEST_CASE("External input is captured and trimmed once per host block",
          "[exciter][external][telemetry]")
{
    ExternalInputFrame frame;
    frame.prepare(16);

    std::array<float, 16> left {};
    std::array<float, 16> right {};
    for (int sample = 0; sample < 16; ++sample)
    {
        left[static_cast<std::size_t>(sample)] = 0.25f + 0.01f * static_cast<float>(sample);
        right[static_cast<std::size_t>(sample)] = -0.5f + 0.02f * static_cast<float>(sample);
    }

    const auto dryGain = frame.beginBlock(left.data(), right.data(), 13, true, 0.5f);
    REQUIRE(frame.isValid());
    REQUIRE(frame.getNumSamples() == 13);
    REQUIRE(dryGain == Catch::Approx(std::sqrt(0.5f)).margin(1.0e-6f));
    for (int sample = 0; sample < 13; ++sample)
    {
        REQUIRE(frame.getReadPointer(0)[sample]
                == Catch::Approx(left[static_cast<std::size_t>(sample)] * 0.05f).margin(1.0e-7f));
        REQUIRE(frame.getReadPointer(1)[sample]
                == Catch::Approx(right[static_cast<std::size_t>(sample)] * 0.05f).margin(1.0e-7f));
    }

    const auto& telemetry = frame.getTelemetry();
    REQUIRE(telemetry.hostBlocks == 1);
    REQUIRE(telemetry.validBlocks == 1);
    REQUIRE(telemetry.sourceSamplesCopied == 26);
}

TEST_CASE("Invalid external frames never expose stale host input",
          "[exciter][external][lifecycle]")
{
    ExternalInputFrame frame;
    frame.prepare(8);
    std::array<float, 8> source {};
    source.fill(1.0f);

    REQUIRE(frame.beginBlock(source.data(), source.data(), 8, true, 1.0f)
            == Catch::Approx(0.0f).margin(1.0e-6f));
    REQUIRE(frame.isValid());

    REQUIRE(frame.beginBlock(source.data(), source.data(), 8, false, 0.5f) == 1.0f);
    REQUIRE_FALSE(frame.isValid());
    REQUIRE(frame.getNumSamples() == 0);

    REQUIRE(frame.beginBlock(nullptr, nullptr, 8, true, 0.5f) == 1.0f);
    REQUIRE_FALSE(frame.isValid());
    REQUIRE(frame.getNumSamples() == 0);

    const auto& telemetry = frame.getTelemetry();
    REQUIRE(telemetry.hostBlocks == 3);
    REQUIRE(telemetry.validBlocks == 1);
    REQUIRE(telemetry.sourceSamplesCopied == 16);

    frame.clearTelemetry();
    REQUIRE(frame.getTelemetry().hostBlocks == 0);
    REQUIRE(frame.getTelemetry().validBlocks == 0);
    REQUIRE(frame.getTelemetry().sourceSamplesCopied == 0);
}

TEST_CASE("External frame dry law and valid region cover every host offset corner",
          "[exciter][external][routing]")
{
    ExternalInputFrame frame;
    frame.prepare(32);
    std::array<float, 32> left {};
    std::array<float, 32> right {};
    for (int sample = 0; sample < 32; ++sample)
    {
        left[static_cast<std::size_t>(sample)] = 0.125f * static_cast<float>(sample + 1);
        right[static_cast<std::size_t>(sample)] = -0.25f * static_cast<float>(sample + 1);
    }

    for (const auto mix : { 0.0f, 0.5f, 1.0f })
    {
        const auto dry = frame.beginBlock(left.data(), right.data(), 32, true, mix);
        REQUIRE(dry == Catch::Approx(std::cos(mix * juce::MathConstants<float>::halfPi))
                           .margin(1.0e-6f));
        REQUIRE(frame.isValid());
        REQUIRE(frame.getNumSamples() == 32);
        for (const auto offset : { 0, 1, 31 })
        {
            REQUIRE(frame.getReadPointer(0)[offset]
                    == Catch::Approx(left[static_cast<std::size_t>(offset)] * 0.05f)
                           .margin(1.0e-7f));
            REQUIRE(frame.getReadPointer(1)[offset]
                    == Catch::Approx(right[static_cast<std::size_t>(offset)] * 0.05f)
                           .margin(1.0e-7f));
        }
    }

    REQUIRE(frame.getTelemetry().hostBlocks == 3);
    REQUIRE(frame.getTelemetry().validBlocks == 3);
    REQUIRE(frame.getTelemetry().sourceSamplesCopied == 3 * 32 * 2);
}

TEST_CASE("External frame only copies the current region and validates prepared capacity",
          "[exciter][external][bounds]")
{
    ExternalInputFrame frame;
    frame.prepare(16);
    std::array<float, 17> left {};
    std::array<float, 17> right {};
    left.fill(1.0f);
    right.fill(-1.0f);

    REQUIRE(frame.beginBlock(left.data(), right.data(), 1, true, 0.5f)
            == Catch::Approx(std::sqrt(0.5f)).margin(1.0e-6f));
    REQUIRE(frame.isValid());
    REQUIRE(frame.getNumSamples() == 1);
    REQUIRE(frame.getReadPointer(0)[0] == Catch::Approx(0.05f));
    REQUIRE(frame.getReadPointer(1)[0] == Catch::Approx(-0.05f));

    REQUIRE(frame.beginBlock(left.data(), right.data(), 16, true, 0.5f)
            == Catch::Approx(std::sqrt(0.5f)).margin(1.0e-6f));
    REQUIRE(frame.isValid());
    REQUIRE(frame.getNumSamples() == 16);

    // Oversized and empty blocks publish invalid frames without touching or
    // clearing the prepared storage. Consumers must obey the validity flag.
    REQUIRE(frame.beginBlock(left.data(), right.data(), 17, true, 0.5f) == 1.0f);
    REQUIRE_FALSE(frame.isValid());
    REQUIRE(frame.getNumSamples() == 0);
    REQUIRE(frame.beginBlock(left.data(), right.data(), 0, true, 0.5f)
            == Catch::Approx(std::sqrt(0.5f)).margin(1.0e-6f));
    REQUIRE(frame.isValid());
    REQUIRE(frame.getNumSamples() == 0);

    REQUIRE(frame.getTelemetry().hostBlocks == 4);
    REQUIRE(frame.getTelemetry().validBlocks == 3);
    REQUIRE(frame.getTelemetry().sourceSamplesCopied == (1 + 16) * 2);
}
