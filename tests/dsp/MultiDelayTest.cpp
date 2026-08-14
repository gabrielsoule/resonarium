#include <catch2/catch_test_macros.hpp>

#include "dsp/MultiDelay.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace {

struct ImpulseRender {
  float echoLeft = 0.0f;
  float echoRight = 0.0f;
  float maximumUnexpectedMagnitude = 0.0f;
};

ImpulseRender renderImpulse(MultiDelay& delay, int delaySamples,
                            float impulseLeft, float impulseRight) {
  constexpr int blockSize = 257;
  const int totalSamples = delaySamples + blockSize;
  juce::AudioBuffer<float> buffer(2, blockSize);
  ImpulseRender result;

  for (int blockStart = 0; blockStart < totalSamples;
       blockStart += blockSize) {
    const auto samplesThisBlock =
        std::min(blockSize, totalSamples - blockStart);
    buffer.clear();
    if (blockStart == 0) {
      buffer.setSample(0, 0, impulseLeft);
      buffer.setSample(1, 0, impulseRight);
    }

    auto audioBlock =
        juce::dsp::AudioBlock<float>(buffer).getSubBlock(0, samplesThisBlock);
    juce::dsp::ProcessContextReplacing<float> context(audioBlock);
    delay.process(context);

    for (int sample = 0; sample < samplesThisBlock; ++sample) {
      const auto absoluteSample = blockStart + sample;
      const auto left = buffer.getSample(0, sample);
      const auto right = buffer.getSample(1, sample);
      if (absoluteSample == delaySamples) {
        result.echoLeft = left;
        result.echoRight = right;
      } else {
        result.maximumUnexpectedMagnitude =
            std::max({result.maximumUnexpectedMagnitude, std::abs(left),
                      std::abs(right)});
      }
    }
  }

  return result;
}

}  // namespace

TEST_CASE("MultiDelay supports its full two-second range at every production rate",
          "[dsp][delay][multidelay]") {
  constexpr std::array<double, 3> sampleRates{44100.0, 48000.0, 96000.0};

  for (const auto sampleRate : sampleRates) {
    CAPTURE(sampleRate);
    MultiDelay delay(2.0f);
    delay.prepare({sampleRate, 257, 2});
    delay.setDelayTimeSmoothingTime(0.0f);
    delay.setDelayTime(0, 2.0f);
    delay.setDelayTime(1, 2.0f);
    delay.setFeedback(0, 0.0f);
    delay.setFeedback(1, 0.0f);
    delay.setMix(0, 1.0f);
    delay.setMix(1, 1.0f);
    delay.setPingPong(false);

    const auto delaySamples =
        static_cast<int>(std::ceil(sampleRate * 2.0));
    const auto first = renderImpulse(delay, delaySamples, 1.0f, -0.5f);
    REQUIRE(first.echoLeft == 1.0f);
    REQUIRE(first.echoRight == -0.5f);
    REQUIRE(first.maximumUnexpectedMagnitude == 0.0f);

    delay.reset();
    const auto afterReset = renderImpulse(delay, delaySamples, 0.0f, 0.0f);
    REQUIRE(afterReset.echoLeft == 0.0f);
    REQUIRE(afterReset.echoRight == 0.0f);
    REQUIRE(afterReset.maximumUnexpectedMagnitude == 0.0f);
  }
}
