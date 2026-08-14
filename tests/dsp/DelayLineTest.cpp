#include <catch2/catch_test_macros.hpp>

#include "dsp/DelayLine.h"

#include <array>
#include <cmath>

namespace {

template <typename DelayType>
float processSample(DelayType& delay, float input) {
  const auto output = delay.popSample(0);
  delay.pushSample(0, input);
  return output;
}

TEST_CASE("Fifth-order delay preserves polynomial interpolation",
          "[dsp][delay][interpolation]") {
  resonarium::DelayLine<float, resonarium::DelayInterpolation::lagrange5th>
      delay(8);
  delay.prepare(1);
  constexpr float delaySamples = 4.5f;
  delay.setDelay(delaySamples);

  for (int sample = 0; sample < 25; ++sample) {
    const auto output = processSample(
        delay, std::pow(static_cast<float>(sample), 4.0f));

    if (sample >= 12) {
      const auto delayedPosition = static_cast<float>(sample) - delaySamples;
      const auto expected = std::pow(delayedPosition, 4.0f);
      REQUIRE(std::abs(output - expected) < 0.1f);
    }
  }
}

}  // namespace

TEST_CASE("Delay line preserves its exact ring period", "[dsp][delay]") {
  resonarium::DelayLine<float> delay(8);
  delay.prepare(1);
  delay.setDelay(4.0f);

  std::array<float, 64> input{};
  for (size_t i = 0; i < input.size(); ++i)
    input[i] = static_cast<float>(i + 1);

  for (size_t i = 0; i < input.size(); ++i) {
    const auto output = processSample(delay, input[i]);
    const auto expected = i < 4 ? 0.0f : input[i - 4];
    REQUIRE(output == expected);
  }
}

TEST_CASE("Logical delay reset cannot expose stale ring samples",
          "[dsp][delay][reset]") {
  resonarium::DelayLine<float> delay(8);
  delay.prepare(1);
  delay.setDelay(4.0f);

  for (int i = 0; i < 32; ++i)
    (void)processSample(delay, static_cast<float>(100 + i));

  delay.reset();
  REQUIRE(processSample(delay, 1.0f) == 0.0f);
  REQUIRE(processSample(delay, 0.0f) == 0.0f);
  REQUIRE(processSample(delay, 0.0f) == 0.0f);
  REQUIRE(processSample(delay, 0.0f) == 0.0f);
  REQUIRE(processSample(delay, 0.0f) == 1.0f);
}
