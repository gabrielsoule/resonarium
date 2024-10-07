#include "RandomLFOComponent.h"

RandomLFOComponent::RandomLFOComponent(RandomLFOParams params) : params(params)
{
    startTimerHz(20);
}

RandomLFOComponent::~RandomLFOComponent()
{
    stopTimer();
}

void RandomLFOComponent::resized()
{
    auto area = getLocalBounds().reduced(2);
    bufferSize = area.getWidth() / scalingFactor;
    curveBuffer.resize(bufferSize, valueToY(0.0f));

    bufferIndex = 0;
    createPath();
}

void RandomLFOComponent::paint(juce::Graphics& g)
{
    // Fill the background
    g.setColour(juce::Colours::black);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 10);
    const auto c = findColour (gin::GinLookAndFeel::accentColourId).withAlpha (isEnabled() ? 1.0f : 0.5f);

    // Set the colour for the waveform
    g.setColour(c);

    // Draw the waveform path
    g.strokePath(path, juce::PathStrokeType(2.0f));
}

void RandomLFOComponent::timerCallback()
{
    if(params.enabled->isOn() && stateCallback)
    {
        const float value = stateCallback() * 2.0f - 1.0f;
        curveBuffer[bufferIndex] = valueToY(value);
        bufferIndex = (bufferIndex + 1) % bufferSize;
        createPath();
        repaint();
    }
}

float RandomLFOComponent::generateRandomValue()
{
    return random.nextFloat() * 2.0f - 1.0f;
}

float RandomLFOComponent::valueToY(float value)
{
    auto area = getLocalBounds().reduced(2);
    return area.getBottom() - (value + 1.0f) / 2.0f * area.getHeight();
}

void RandomLFOComponent::createPath()
{
    path.clear();

    if (curveBuffer.empty())
        return;

    auto area = getLocalBounds().reduced(2);
    size_t numPoints = curveBuffer.size();

    float xScale = static_cast<float>(area.getWidth()) / static_cast<float>(numPoints - 1);

    size_t index = bufferIndex;

    float x = 0.0f;
    float y = curveBuffer[index];
    path.startNewSubPath(area.getX() + x, y);

    for (size_t i = 1; i < numPoints; ++i)
    {
        index = (index + 1) % numPoints;
        x = static_cast<float>(i) * xScale;
        y = curveBuffer[index];

        if (x > area.getWidth())
            break;

        path.lineTo(area.getX() + x, y);
    }
}
