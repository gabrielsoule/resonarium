#pragma once

#include "../Parameters.h"

class RandomLFOComponent : public juce::Component, private juce::Timer
{
public:
    RandomLFOComponent(RandomLFOParams params);
    ~RandomLFOComponent() override;

    void resized() override;
    void paint(juce::Graphics& g) override;

    std::function<float()> stateCallback;

private:
    // Timer callback for updating the curve
    void timerCallback() override;

    // Generates a random value between -1 and 1
    float generateRandomValue();

    // Converts a value in the range [-1, 1] to a Y-coordinate
    float valueToY(float value);

    // Creates the path for the waveform using the curve buffer
    void createPath();

    std::vector<float> curveBuffer;
    size_t bufferIndex = 0;
    size_t bufferSize = 0;
    juce::Path path;
    juce::Random random;
    RandomLFOParams params;

    int scalingFactor = 2;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RandomLFOComponent)
};
