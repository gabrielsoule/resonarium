#pragma once

#include "../dsp/Sampler.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>

class SampleDropperComponent : public juce::Component,
                               public juce::FileDragAndDropTarget,
                               public juce::DragAndDropContainer,
                               public juce::ChangeListener,
                               public juce::SettableTooltipClient// Add this to listen for thumbnail changes
{
public:
    explicit SampleDropperComponent(Sampler& sampler);
    ~SampleDropperComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    void lookAndFeelChanged() override;
    void updateFromSampler();
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;
    void loadFile(const juce::File& file);
    void clearCurrentFile();

    std::function<void()> onSampleLoaded;

private:
    void updateLabels();

    Sampler& sampler;
    juce::String currentFileName = "";
    bool isFileLoaded = false;
    bool isBeingDraggedOver = false;

    juce::Label emptyStateLabel;
    juce::Label loadedStateLabel;

    juce::AudioThumbnailCache thumbnailCache;   // Add this
    juce::AudioThumbnail thumbnail;             // Add this

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SampleDropperComponent)
};
