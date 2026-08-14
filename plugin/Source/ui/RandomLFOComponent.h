#pragma once

#include "../Parameters.h"

#include <array>

class RandomLFOComponent : public juce::Component
{
public:
    RandomLFOComponent(RandomLFOParams params);
    ~RandomLFOComponent() override = default;

    void resized() override;
    void paint(juce::Graphics& g) override;
    void updateFromTimer();

    std::function<float()> valueCallback;

private:
    // Converts a value in the range [-1, 1] to a Y-coordinate
    float valueToY(float value);

    // Creates the path for the waveform using the curve buffer
    void createPath();

    static constexpr size_t maxCurvePoints = 2048;
    std::array<float, maxCurvePoints> curveBuffer {};
    size_t bufferIndex = 0;
    size_t bufferSize = 0;
    juce::Path path;
    RandomLFOParams params;

    int scalingFactor = 2;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RandomLFOComponent)
};
