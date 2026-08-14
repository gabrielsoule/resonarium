#include "RandomLFOComponent.h"

RandomLFOComponent::RandomLFOComponent(RandomLFOParams params) : params(params)
{
}

void RandomLFOComponent::resized()
{
    auto area = getLocalBounds().reduced(2);
    bufferSize = std::min(maxCurvePoints,
                          static_cast<size_t>(std::max(0, area.getWidth() / scalingFactor)));
    std::fill_n(curveBuffer.begin(), bufferSize, valueToY(0.0f));

    bufferIndex = 0;
    path.preallocateSpace(static_cast<int>(bufferSize * 3));
    createPath();
}

void RandomLFOComponent::paint(juce::Graphics& g)
{
    g.setColour(juce::Colours::black);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 10);
    const auto c = findColour (gin::GinLookAndFeel::accentColourId).withAlpha (isEnabled() ? 1.0f : 0.5f);
    g.setColour(c);
    g.strokePath(path, juce::PathStrokeType(2.0f));
}

void RandomLFOComponent::updateFromTimer()
{
    if (params.enabled->isOn() && valueCallback && bufferSize >= 2)
    {
        const float value = valueCallback() * 2.0f - 1.0f;
        curveBuffer[bufferIndex] = valueToY(value);
        bufferIndex = (bufferIndex + 1) % bufferSize;
        createPath();
        repaint();
    }
}

float RandomLFOComponent::valueToY(float value)
{
    auto area = getLocalBounds().reduced(2);
    return area.getBottom() - (value + 1.0f) / 2.0f * area.getHeight();
}

void RandomLFOComponent::createPath()
{
    path.clear();

    if (bufferSize < 2)
        return;

    auto area = getLocalBounds().reduced(2);
    const auto numPoints = bufferSize;

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
