#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <gin_plugin/gin_plugin.h>
#include <gin_graphics/images/gin_imageeffects.h>  // For applyStackBlur
#include "../PluginProcessor.h"
#include "ResonariumLookAndFeel.h"

class SettingsPanel : public juce::Component
{
public:
    SettingsPanel(ResonariumProcessor& processor, juce::Component* parent);
    ~SettingsPanel() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    void show();
    void hide();
    
    // Close button click handler
    std::function<void()> onCloseButtonClick;
    
private:
    ResonariumProcessor& proc;
    juce::Component* parentComponent;
    
    juce::ComboBox themeSelector;
    juce::Label themeLabel;
    juce::ToggleButton showTooltipsToggle;
    juce::ToggleButton enableAnimationsToggle;
    juce::Slider uiScaleSlider;
    juce::Label uiScaleLabel;
    juce::TextButton closeButton{"Close"};

    // BlurryComp from gin::PluginAlertWindow
    class BlurryComp : public juce::Component
    {
    public:
        BlurryComp(juce::Image img) : background(img)
        {
            // Use the global function directly from gin
            gin::applyStackBlur(img, 4);
        }

        void paint(juce::Graphics& g) override
        {
            g.drawImage(background, getLocalBounds().toFloat());
        }

        juce::Image background;
    };

    std::unique_ptr<BlurryComp> blur;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsPanel)
};