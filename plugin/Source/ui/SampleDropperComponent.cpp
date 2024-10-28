#include "SampleDropperComponent.h"

SampleDropperComponent::SampleDropperComponent(Sampler& s)
    : sampler(s)
{
    setOpaque(false);
    setWantsKeyboardFocus(true);

    addAndMakeVisible(emptyStateLabel);
    emptyStateLabel.setJustificationType(juce::Justification::centred);
    emptyStateLabel.setMinimumHorizontalScale(1.0f);
    emptyStateLabel.setFont(18.0f);
    emptyStateLabel.setText("DRAG AND DROP SAMPLE",
                           juce::dontSendNotification);
    emptyStateLabel.setColour(juce::Label::textColourId, juce::Colour (0xff775cff));
    emptyStateLabel.setInterceptsMouseClicks(false, false);

    addAndMakeVisible(loadedStateLabel);
    loadedStateLabel.setJustificationType(juce::Justification::centred);
    loadedStateLabel.setMinimumHorizontalScale(1.0f);
    loadedStateLabel.setFont(14.0f);
    loadedStateLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.7f));
    loadedStateLabel.setInterceptsMouseClicks(false, false);

    updateLabels();
}

SampleDropperComponent::~SampleDropperComponent()
{
}


void SampleDropperComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().reduced(8);

    // Draw border
    g.setColour(juce::Colours::black);
    g.fillRoundedRectangle(bounds.toFloat(), 10.0f);

    // Draw waveform if file is loaded
    if (isFileLoaded)
    {
        g.setColour(juce::Colour (0xff775cff));
        g.fillPath(waveformPath);
    }
}

void SampleDropperComponent::createWaveformPath()
{
    waveformPath.clear();

    if (!isFileLoaded || sampler.getNumSamples() == 0)
        return;

    const auto numSamples = sampler.getNumSamples();
    const auto bounds = getLocalBounds().toFloat().reduced(8.0f);
    const float pathHeight = bounds.getHeight() * 0.4f;  // Reduced height for subtler look
    const float centerY = bounds.getCentreY();

    const int skipSamples = std::max(1, numSamples / waveformResolution);

    // First pass: find maximum amplitude
    float maxAmp = 0.0f;
    for (int i = 0; i < numSamples; i += skipSamples)
    {
        float sample = std::abs(sampler.getSample(0, i));
        maxAmp = std::max(maxAmp, sample);
    }

    if (maxAmp == 0.0f) maxAmp = 1.0f;

    // Second pass: create the path with mirroring
    bool firstPoint = true;
    for (int i = 0; i < numSamples; i += skipSamples)
    {
        const float x = bounds.getX() + (i * bounds.getWidth() / numSamples);
        float sample = sampler.getSample(0, i) / maxAmp;
        const float y = centerY - (sample * pathHeight);

        if (firstPoint)
        {
            waveformPath.startNewSubPath(x, centerY);
            firstPoint = false;
        }
        waveformPath.lineTo(x, y);
    }

    // Create mirror image
    for (int i = numSamples - skipSamples; i >= 0; i -= skipSamples)
    {
        const float x = bounds.getX() + (i * bounds.getWidth() / numSamples);
        float sample = -sampler.getSample(0, i) / maxAmp;
        const float y = centerY - (sample * pathHeight);
        waveformPath.lineTo(x, y);
    }

    waveformPath.closeSubPath();
}

void SampleDropperComponent::resized()
{
    auto bounds = getLocalBounds().reduced(4);
    emptyStateLabel.setBounds(bounds);
    loadedStateLabel.setBounds(bounds);
    createWaveformPath();
}

void SampleDropperComponent::updateLabels()
{
    if (isFileLoaded)
    {
        // Update loaded state label text
        juce::String info = currentFileName + "\n" +
                           juce::String(sampler.getNumSamples()) + " samples @ " +
                           juce::String(sampler.getFileSampleRate()) + " Hz";
        loadedStateLabel.setText(info, juce::dontSendNotification);

        // Show/hide appropriate labels
        setTooltip(loadedStateLabel.getText());
        loadedStateLabel.setVisible(false);
        emptyStateLabel.setVisible(false);
    }
    else
    {
        // Show/hide appropriate labels
        loadedStateLabel.setVisible(false);
        emptyStateLabel.setVisible(true);
    }
}


bool SampleDropperComponent::isInterestedInFileDrag(const juce::StringArray& files)
{
    // Check if any of the files are audio files
    for (auto& file : files)
    {
        if (file.endsWith(".wav") || file.endsWith(".mp3") ||
            file.endsWith(".aif") || file.endsWith(".aiff"))
        {
            return true;
        }
    }
    return false;
}

void SampleDropperComponent::filesDropped(const juce::StringArray& files, int x, int y)
{
    isBeingDraggedOver = false;

    for (auto& filename : files)
    {
        juce::File file(filename);
        if (isInterestedInFileDrag(juce::StringArray(filename)))
        {
            loadFile(file);
            break; // Only load the first valid audio file
        }
    }

    repaint();
}

void SampleDropperComponent::loadFile(const juce::File& file)
{
    if (sampler.loadFile(file))
    {
        currentFileName = file.getFileName();
        isFileLoaded = true;
        updateLabels();
        createWaveformPath();  // Add this line to create the waveform

        if (onSampleLoaded)
            onSampleLoaded();
    }
    else
    {
        // Show error message
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                             "Error",
                                             "Couldn't load file: " + file.getFileName());
    }

    repaint();
}

void SampleDropperComponent::mouseDown(const juce::MouseEvent& event)
{
    if (event.mods.isRightButtonDown())
    {
        juce::PopupMenu menu;
        menu.addItem(1, "Load File...");
        if (isFileLoaded)
        {
            menu.addItem(2, "Clear");
        }

        menu.showMenuAsync(juce::PopupMenu::Options(), [this](int result)
        {
            if (result == 1)
            {
                auto chooser = std::make_shared<juce::FileChooser>(
                    "Select an audio file...",
                    juce::File::getSpecialLocation(juce::File::userHomeDirectory),
                    "*.wav;*.mp3;*.aif;*.aiff"
                );

                chooser->launchAsync(juce::FileBrowserComponent::openMode |
                                   juce::FileBrowserComponent::canSelectFiles,
                    [this, chooser](const juce::FileChooser& fc)
                    {
                        if (fc.getResults().size() > 0)
                        {
                            loadFile(fc.getResult());
                        }
                    });
            }
            else if (result == 2)
            {
                // Clear the current sample
                currentFileName = "";
                isFileLoaded = false;
                updateLabels();
                repaint();
            }
        });
    }
}