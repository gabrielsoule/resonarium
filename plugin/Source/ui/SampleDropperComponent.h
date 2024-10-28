#pragma once

#include <JuceHeader.h>
#include "../dsp/Sampler.h"

class SampleDropperComponent : public juce::Component,
                               public juce::FileDragAndDropTarget,
                               public juce::DragAndDropContainer,
                               public juce::SettableTooltipClient
{
public:
    SampleDropperComponent(Sampler& sampler);
    ~SampleDropperComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;
    void mouseDown(const juce::MouseEvent& event) override;

    std::function<void()> onSampleLoaded;

private:
    void loadFile(const juce::File& file);
    void updateLabels();
    void createWaveformPath();

    Sampler& sampler;
    juce::String currentFileName;
    bool isFileLoaded = false;
    bool isBeingDraggedOver = false;

    juce::Label emptyStateLabel;
    juce::Label loadedStateLabel;

    juce::Path waveformPath;
    static constexpr int waveformResolution = 150;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SampleDropperComponent)
};
