#include "SettingsPanel.h"

SettingsPanel::SettingsPanel(ResonariumProcessor& processor, juce::Component* parent) 
    : proc(processor), parentComponent(parent)
{
    setName("Settings Panel");
    
    // Add theme selector
    themeLabel.setText("Theme:", juce::dontSendNotification);
    themeLabel.setJustificationType(juce::Justification::right);
    themeSelector.addItem("Default Theme", 1);
    themeSelector.addItem("Dark Theme", 2);
    themeSelector.addItem("Light Theme", 3);
    themeSelector.setSelectedId(1, juce::dontSendNotification);
    
    // Add show tooltips toggle
    showTooltipsToggle.setButtonText("Show Tooltips");
    showTooltipsToggle.setToggleState(true, juce::dontSendNotification);
    
    // Add enable animations toggle
    enableAnimationsToggle.setButtonText("Enable Animations");
    enableAnimationsToggle.setToggleState(true, juce::dontSendNotification);
    
    // Add UI scale slider
    uiScaleSlider.setRange(0.75, 1.5, 0.05);
    uiScaleSlider.setValue(1.0, juce::dontSendNotification);
    uiScaleSlider.setSliderStyle(juce::Slider::SliderStyle::LinearHorizontal);
    uiScaleSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    
    uiScaleLabel.setText("UI Scale:", juce::dontSendNotification);
    uiScaleLabel.setJustificationType(juce::Justification::right);
    
    // Add close button
    closeButton.onClick = [this]() { 
        if (onCloseButtonClick)
            onCloseButtonClick();
        else
            hide();
    };
    
    // Add all components to the panel
    addAndMakeVisible(themeLabel);
    addAndMakeVisible(themeSelector);
    addAndMakeVisible(showTooltipsToggle);
    addAndMakeVisible(enableAnimationsToggle);
    addAndMakeVisible(uiScaleLabel);
    addAndMakeVisible(uiScaleSlider);
    addAndMakeVisible(closeButton);
    
    // Set look and feel from processor
    setLookAndFeel(processor.lf.get());
    
    // Set initial size
    setSize(350, 220);
}

SettingsPanel::~SettingsPanel()
{
    setLookAndFeel(nullptr);
}

void SettingsPanel::paint(juce::Graphics& g)
{
    // Draw the panel background
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId).darker(0.1f));
    
    // Draw panel title
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(20.0f, juce::Font::bold));
    g.drawText("Settings", getLocalBounds().removeFromTop(40), juce::Justification::centred, true);
    
    // Draw border around the panel
    g.setColour(juce::Colours::white.withAlpha(0.2f));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 5.0f, 1.0f);
}

void SettingsPanel::resized()
{
    
    // Calculate the content area with a little more room for the title area
    auto bounds = getLocalBounds().reduced(20);
    
    // Title area
    bounds.removeFromTop(30); // Space for title
    
    // Theme selector row
    auto themeRow = bounds.removeFromTop(24);
    themeLabel.setBounds(themeRow.removeFromLeft(80));
    themeSelector.setBounds(themeRow);
    
    bounds.removeFromTop(10); // spacing
    
    // Toggle switches
    showTooltipsToggle.setBounds(bounds.removeFromTop(24));
    
    bounds.removeFromTop(10); // spacing
    
    enableAnimationsToggle.setBounds(bounds.removeFromTop(24));
    
    bounds.removeFromTop(10); // spacing
    
    // UI scale row
    auto scaleRow = bounds.removeFromTop(24);
    uiScaleLabel.setBounds(scaleRow.removeFromLeft(80));
    uiScaleSlider.setBounds(scaleRow);
    
    bounds.removeFromTop(20); // spacing
    
    // Close button
    closeButton.setBounds(bounds.removeFromTop(30).withSizeKeepingCentre(120, 30));
}

void SettingsPanel::show()
{
    // Create a blurred background image using BlurryComp from gin
    blur = std::make_unique<BlurryComp>(parentComponent->createComponentSnapshot(parentComponent->getLocalBounds()));
    blur->setBounds(parentComponent->getLocalBounds());
    
    // Add blurred background to parent component
    parentComponent->addAndMakeVisible(*blur);
    
    // Center the panel in the parent component
    setBounds(blur->getLocalBounds().withSizeKeepingCentre(getWidth(), getHeight()));
    
    // Add the settings panel on top of the blur
    blur->addAndMakeVisible(this);
    
    // Bring to front to ensure it's visible
    toFront(true);
}

void SettingsPanel::hide()
{
    // Remove our parent (the blur component) from its parent
    if (blur != nullptr)
    {
        if (blur->getParentComponent() != nullptr)
            blur->getParentComponent()->removeChildComponent(blur.get());
        
        blur = nullptr;
    }
}


