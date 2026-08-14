#include "SampleDropperComponent.h"

#include "ResonariumLookAndFeel.h"

SampleDropperComponent::SampleDropperComponent(Sampler& s)
    : sampler(s),
      thumbnailCache(5),
      thumbnail(512, thumbnailFormatManager, thumbnailCache)
{
    thumbnailFormatManager.registerBasicFormats();
    setOpaque(false);
    setWantsKeyboardFocus(true);

    addAndMakeVisible(emptyStateLabel);
    emptyStateLabel.setJustificationType(juce::Justification::centred);
    emptyStateLabel.setMinimumHorizontalScale(1.0f);
    emptyStateLabel.setFont(juce::FontOptions().withHeight(18.0f));
    emptyStateLabel.setText("DRAG AND DROP SAMPLE", juce::dontSendNotification);
    emptyStateLabel.setColour(juce::Label::textColourId, juce::Colour(0xff775cff));
    emptyStateLabel.setInterceptsMouseClicks(false, false);

    addAndMakeVisible(loadedStateLabel);
    loadedStateLabel.setJustificationType(juce::Justification::centred);
    loadedStateLabel.setMinimumHorizontalScale(1.0f);
    loadedStateLabel.setFont(juce::FontOptions().withHeight(14.0f));
    loadedStateLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.7f));
    loadedStateLabel.setInterceptsMouseClicks(false, false);

    thumbnail.addChangeListener(this);

    updateFromSampler();
}

SampleDropperComponent::~SampleDropperComponent()
{
    thumbnail.removeChangeListener(this);
}

void SampleDropperComponent::updateFromTimer()
{
    if (sampler.getDisplayGeneration() != displayedGeneration)
        updateFromSampler();
}

void SampleDropperComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().reduced(8);

    g.setColour(juce::Colours::black);
    g.fillRoundedRectangle(bounds.toFloat(), 10.0f);

    if (isFileLoaded)
    {
        auto accentColor = juce::Colour(0xff775cff);

        if (auto* lf = dynamic_cast<ResonariumLookAndFeel*>(&getLookAndFeel()))
            accentColor = lf->findColour(ResonariumLookAndFeel::accentColourId);

        g.setColour(accentColor.withAlpha(isEnabled() ? 1.0f : 0.5f));
        thumbnail.drawChannel(g,
                              bounds,
                              0.0,
                              thumbnail.getTotalLength(),
                              0,
                              1.0f);
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
    auto accentColor = juce::Colour(0xff775cff);

    if (auto* lf = dynamic_cast<ResonariumLookAndFeel*>(&getLookAndFeel()))
        accentColor = lf->findColour(ResonariumLookAndFeel::accentColourId);

    emptyStateLabel.setColour(juce::Label::textColourId, accentColor);
    repaint();
}

void SampleDropperComponent::updateFromSampler()
{
    const auto info = sampler.getDisplayInfo();
    displayedGeneration = info.generation;
    if (info.loaded)
    {
        isFileLoaded = true;
        currentFileName = info.sampleName;
        if (info.filePath.isNotEmpty())
            thumbnail.setSource(new juce::FileInputSource(juce::File(info.filePath)));
        else
            thumbnail.setSource(nullptr);
    }
    else
    {
        isFileLoaded = false;
        currentFileName = "";
        thumbnail.setSource(nullptr);
    }

    updateLabels(info);
    repaint();
}

bool SampleDropperComponent::isInterestedInFileDrag(const juce::StringArray& files)
{
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

void SampleDropperComponent::filesDropped(const juce::StringArray& files, int, int)
{
    isBeingDraggedOver = false;

    for (auto& filename : files)
    {
        juce::File file(filename);
        if (isInterestedInFileDrag(juce::StringArray(filename)))
        {
            loadFile(file);
            break;
        }
    }

    repaint();
}

void SampleDropperComponent::loadFile(const juce::File& file)
{
    const juce::Component::SafePointer<SampleDropperComponent> safeThis(this);
    if (!sampler.requestLoadFile(file, [safeThis](Sampler::LoadResult workerResult)
        {
            juce::MessageManager::callAsync([safeThis, messageResult = std::move(workerResult)]() mutable
            {
                if (safeThis != nullptr)
                    safeThis->handleLoadResult(std::move(messageResult), true);
            });
        }))
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                               "Error",
                                               "Couldn't load file: " + file.getFileName());
    }
}

void SampleDropperComponent::clearCurrentFile()
{
    const juce::Component::SafePointer<SampleDropperComponent> safeThis(this);
    sampler.requestClear([safeThis](Sampler::LoadResult workerResult)
    {
        juce::MessageManager::callAsync([safeThis, messageResult = std::move(workerResult)]() mutable
        {
            if (safeThis != nullptr)
                safeThis->handleLoadResult(std::move(messageResult), false);
        });
    });
}

void SampleDropperComponent::handleLoadResult(Sampler::LoadResult result, bool showFailure)
{
    if (result.superseded)
        return;

    if (!result.success)
    {
        if (showFailure)
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::WarningIcon,
                "Error",
                result.error.isNotEmpty() ? result.error : "Couldn't load the selected sample");
        }
        return;
    }

    updateFromSampler();
}

void SampleDropperComponent::updateLabels(const Sampler::DisplayInfo& info)
{
    if (isFileLoaded)
    {
        const juce::String labelText = currentFileName + "\n" +
                                       juce::String(info.numSamples) + " samples @ " +
                                       juce::String(info.fileSampleRate) + " Hz";
        loadedStateLabel.setText(labelText, juce::dontSendNotification);

        setTooltip(loadedStateLabel.getText());
        loadedStateLabel.setVisible(false);
        emptyStateLabel.setVisible(false);
    }
    else
    {
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

        const juce::Component::SafePointer<SampleDropperComponent> safeThis(this);
        menu.showMenuAsync(juce::PopupMenu::Options(), [safeThis](int result)
        {
            if (safeThis == nullptr)
                return;

            if (result == 1)
            {
                auto chooser = std::make_shared<juce::FileChooser>(
                    "Select an audio file...",
                    juce::File::getSpecialLocation(juce::File::userHomeDirectory),
                    "*.wav;*.mp3;*.aif;*.aiff"
                );

                chooser->launchAsync(juce::FileBrowserComponent::openMode |
                                         juce::FileBrowserComponent::canSelectFiles,
                    [safeThis, chooser](const juce::FileChooser& fc)
                    {
                        if (safeThis != nullptr && fc.getResults().size() > 0)
                        {
                            safeThis->loadFile(fc.getResult());
                        }
                    });
            }
            else if (result == 2)
            {
                safeThis->clearCurrentFile();
            }
        });
    }
}
