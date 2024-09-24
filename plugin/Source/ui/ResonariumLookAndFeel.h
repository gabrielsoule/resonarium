#pragma once

#include <JuceHeader.h>

//Code adapted from gin::PluginLookAndFeel
class ResonariumLookAndFeel : public gin::PluginLookAndFeel
{
public:
    ResonariumLookAndFeel();

    juce::Typeface::Ptr getTypefaceForFont (const juce::Font& f) override;

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;

    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           const juce::Slider::SliderStyle, juce::Slider&) override;


    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool isMouseOverButton, bool isButtonDown) override;

    void drawButtonText (juce::Graphics&, juce::TextButton&, bool isMouseOverButton, bool isButtonDown) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox&) override;

    void drawTooltip(juce::Graphics& g, const juce::String& text, int width, int height) override;

    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    void drawTextEditorOutline (juce::Graphics&, int width, int height, juce::TextEditor&) override;

    juce::PopupMenu::Options getOptionsForComboBoxPopupMenu (juce::ComboBox& box, juce::Label&) override;

    int getAlertWindowButtonHeight() override       { return 20; }
    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override;
    juce::Font getPopupMenuFont() override;

    juce::SharedResourcePointer<gin::Images> images;
    juce::Typeface::Ptr typeface;
    juce::Font defaultFont;
};

class ResonariumLookAndFeelWrapper : public ResonariumLookAndFeel
{
public:
    ResonariumLookAndFeelWrapper();
    ~ResonariumLookAndFeelWrapper();
};
