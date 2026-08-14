#pragma once

#include "../dsp/Sampler.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>

class SampleDropperComponent : public juce::Component,
                               public juce::FileDragAndDropTarget,
                               public juce::DragAndDropContainer,
                               public juce::ChangeListener,
                               public juce::SettableTooltipClient
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
    void updateFromTimer();
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;
    void loadFile(const juce::File& file);
    void clearCurrentFile();

private:
    void updateLabels(const Sampler::DisplayInfo& info);
    void handleLoadResult(Sampler::LoadResult result, bool showFailure);

    Sampler& sampler;
    juce::String currentFileName = "";
    bool isFileLoaded = false;
    bool isBeingDraggedOver = false;
    std::uint64_t displayedGeneration = 0;

    juce::Label emptyStateLabel;
    juce::Label loadedStateLabel;

    juce::AudioFormatManager thumbnailFormatManager;
    juce::AudioThumbnailCache thumbnailCache;
    juce::AudioThumbnail thumbnail;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SampleDropperComponent)
};
