#include "SampleDropperComponent.h"

#include "ResonariumLookAndFeel.h"

SampleDropperComponent::SampleDropperComponent(Sampler& s)
    : sampler(s),
      thumbnailCache(5),    // Cache 5 thumbnails
      thumbnail(512, sampler.getFormatManager(), thumbnailCache)  // 512 samples per thumbnail point
{
    setOpaque(false);
    setWantsKeyboardFocus(true);

    // Setup empty state label
    addAndMakeVisible(emptyStateLabel);
    emptyStateLabel.setJustificationType(juce::Justification::centred);
    emptyStateLabel.setMinimumHorizontalScale(1.0f);
    emptyStateLabel.setFont(18.0f);
    emptyStateLabel.setText("DRAG AND DROP SAMPLE", juce::dontSendNotification);
    // Use explicit color for now and update it in lookAndFeelChanged
    emptyStateLabel.setColour(juce::Label::textColourId, juce::Colour(0xff775cff));
    emptyStateLabel.setInterceptsMouseClicks(false, false);

    // Setup loaded state label
    addAndMakeVisible(loadedStateLabel);
    loadedStateLabel.setJustificationType(juce::Justification::centred);
    loadedStateLabel.setMinimumHorizontalScale(1.0f);
    loadedStateLabel.setFont(14.0f);
    loadedStateLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.7f));
    loadedStateLabel.setInterceptsMouseClicks(false, false);

    // Listen for thumbnail changes
    thumbnail.addChangeListener(this);

    updateLabels();
}

SampleDropperComponent::~SampleDropperComponent()
{
    thumbnail.removeChangeListener(this);
}

void SampleDropperComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().reduced(8);

    // Draw background
    g.setColour(juce::Colours::black);
    g.fillRoundedRectangle(bounds.toFloat(), 10.0f);

    if (isFileLoaded)
    {
        // Draw waveform using accent color from look and feel
        auto accentColor = juce::Colour(0xff775cff); // Default color
        
        // Try to get accent color from look and feel
        if (auto* lf = dynamic_cast<ResonariumLookAndFeel*>(&getLookAndFeel()))
            accentColor = lf->findColour(ResonariumLookAndFeel::accentColourId);
            
        g.setColour(accentColor.withAlpha(isEnabled() ? 1.0f : 0.5f));
        thumbnail.drawChannel(g,
                            bounds,
                            0.0,                          // start time
                            thumbnail.getTotalLength(),    // end time
                            0,                            // channel num
                            1.0f);                        // vertical zoom
    }
}

void SampleDropperComponent::resized()
{
    auto bounds = getLocalBounds().reduced(4);
    emptyStateLabel.setBounds(bounds);
    loadedStateLabel.setBounds(bounds);
}

void SampleDropperComponent::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &thumbnail)
        repaint();
}

void SampleDropperComponent::lookAndFeelChanged()
{
    // Update colors when look and feel changes
    auto accentColor = juce::Colour(0xff775cff); // Default color
    
    // Try to get accent color from look and feel
    if (auto* lf = dynamic_cast<ResonariumLookAndFeel*>(&getLookAndFeel()))
        accentColor = lf->findColour(ResonariumLookAndFeel::accentColourId);
    
    // Update the label color
    emptyStateLabel.setColour(juce::Label::textColourId, accentColor);
    
    repaint(); // Repaint to update waveform color
}

void SampleDropperComponent::updateFromSampler()
{
    if (sampler.isLoaded())
    {
        isFileLoaded = true;
        currentFileName = sampler.getSampleName();
        thumbnail.setSource(new juce::FileInputSource(juce::File(sampler.getFilePath())));
        DBG("SampleDropperComponent: updated sample: " + currentFileName);
    }
    else
    {
        isFileLoaded = false;
        currentFileName = "";
        thumbnail.setSource(nullptr);
        DBG("SampleDropperComponent: clearing loaded sample");
    }

    updateLabels();
    repaint();
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
    DBG("SampleDropperComponent: loading file: " + file.getFileName());
    if (sampler.loadFile(file))
    {
        updateFromSampler();
        if (onSampleLoaded)
            onSampleLoaded();
    }
    else
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                             "Error",
                                             "Couldn't load file: " + file.getFileName());
    }
}

void SampleDropperComponent::clearCurrentFile()
{
    DBG("SampleDropperComponent: clearing current file");
    sampler.clear();  // Assuming you add this method to Sampler
    updateFromSampler();
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
                clearCurrentFile();
            }
        });
    }
}