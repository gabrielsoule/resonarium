#include "ResonariumLookAndFeel.h"

#include "ResonariumComponents.h"
#include "juce_gui_basics/detail/juce_LookAndFeelHelpers.h"

//Code adapted from gin::PluginLookAndFeel

ResonariumLookAndFeel::ResonariumLookAndFeel()
{
    typeface = juce::Typeface::createSystemTypefaceFor (gin::Resources::BarlowThin_ttf, gin::Resources::BarlowThin_ttfSize);
    auto typefacePtr = juce::Typeface::createSystemTypefaceFor (BinaryData::Jost100Hairline_otf, BinaryData::Jost100Hairline_otfSize);
    auto font = juce::FontOptions{}.withName (typefacePtr->getName()).withStyle("Medium").withPointHeight(14);
    DBG(typefacePtr->getName());
    setDefaultSansSerifTypeface(typefacePtr);
    defaultFont = font;
    juce::Array<juce::Font> fonts;
    juce::Font::findFonts(fonts);
    setColour (whiteColourId, juce::Colour (0xffFFFFFF));
    setColour (blackColourId, juce::Colour (0xff000000));
    setColour (displayColourId, juce::Colour (0xff0D0E0F));
    setColour (backgroundColourId, juce::Colour (0xff16171A));
    setColour (groupColourId, juce::Colour (0xff28292E));
    setColour (lightGroupColourId, juce::Colour (0xff35373D));
    setColour (grey30ColourId, juce::Colour (0xff585A62));
    setColour (grey45ColourId, juce::Colour (0xff797C84));
    setColour (grey60ColourId, juce::Colour (0xff9B9EA5));
    setColour (grey90ColourId, juce::Colour (0xffE6E6E9));
    setColour (glass1ColourId, juce::Colour (0xff0f1012));
    setColour (glass2ColourId, juce::Colour (0xff0b0b0d));
    setColour (matte1ColourId, juce::Colour (0xff18191C));
    setColour (matte2ColourId, juce::Colour (0xff131417));
    setColour (title1ColourId, juce::Colour (0xff2A2C30));
    setColour (title2ColourId, juce::Colour (0xff25272B));
    setColour (accentColourId, juce::Colour (0xff775cff));

    setColour (gin::WavetableComponent::waveColourId, juce::Colours::green.withAlpha (0.3f));
    setColour (gin::WavetableComponent::activeWaveColourId, juce::Colours::yellow);

    setColour (juce::BubbleComponent::backgroundColourId, findColour (backgroundColourId));
    setColour (juce::BubbleComponent::outlineColourId, findColour (grey45ColourId));

    setColour (juce::TooltipWindow::textColourId, findColour (grey60ColourId));

    setColour (juce::ScrollBar::thumbColourId, findColour (grey60ColourId));

    setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colours::white.withAlpha (0.8f));
    setColour (juce::MidiKeyboardComponent::blackNoteColourId, juce::Colours::black.withAlpha (0.8f));
    setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, findColour (grey90ColourId));
    setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, findColour (accentColourId).withAlpha (0.7f));
    setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, findColour (accentColourId).withAlpha (0.7f));
    setColour (juce::MidiKeyboardComponent::textLabelColourId, juce::Colours::transparentBlack);
    setColour (juce::MidiKeyboardComponent::shadowColourId, findColour (grey90ColourId));

    setColour (juce::Label::textColourId, findColour (grey90ColourId).withAlpha(0.8f));

    setColour (juce::TextButton::textColourOnId, findColour (accentColourId));
    setColour (juce::TextButton::textColourOffId, findColour (grey60ColourId));

    setColour (juce::TextEditor::textColourId, findColour (grey90ColourId));
    setColour (juce::TextEditor::outlineColourId, findColour (grey60ColourId));
    setColour (juce::TextEditor::focusedOutlineColourId, findColour (grey60ColourId));

    setColour (juce::ComboBox::textColourId, findColour (accentColourId));
    setColour (juce::ComboBox::backgroundColourId, findColour (glass1ColourId));
    setColour (juce::ComboBox::outlineColourId, findColour (blackColourId));

    setColour (juce::Slider::thumbColourId, findColour (accentColourId));
    setColour (juce::Slider::rotarySliderFillColourId, findColour (accentColourId));
    setColour (juce::Slider::trackColourId, findColour (grey45ColourId));

    setColour (juce::PopupMenu::backgroundColourId, findColour (backgroundColourId));
    setColour (juce::PopupMenu::textColourId, findColour (grey60ColourId));
    setColour (juce::PopupMenu::headerTextColourId, findColour (grey60ColourId));
    setColour (juce::PopupMenu::highlightedBackgroundColourId, findColour (accentColourId));
    setColour (juce::PopupMenu::highlightedTextColourId, findColour (whiteColourId));

    setColour (juce::AlertWindow::backgroundColourId, findColour (backgroundColourId));
    setColour (juce::AlertWindow::textColourId, findColour (grey60ColourId));
    setColour (juce::AlertWindow::outlineColourId, findColour (accentColourId));
}

juce::Typeface::Ptr ResonariumLookAndFeel::getTypefaceForFont (const juce::Font& font)
{
    if (font.getTypefaceName() == juce::Font::getDefaultSansSerifFontName())
        return typeface;

    return juce::LookAndFeel_V4::getTypefaceForFont (font);
}

void ResonariumLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                       float sliderPos, float /*minSliderPos*/, float /*maxSliderPos*/,
                                       const juce::Slider::SliderStyle, juce::Slider& slider)
{
    const bool isMouseOver = slider.isMouseOverOrDragging() && slider.isEnabled();
    auto rc = juce::Rectangle<int> (x, y, width, height);
    rc = rc.withSizeKeepingCentre (width, juce::jmin (height, 4));

    g.setColour (slider.findColour (juce::Slider::trackColourId).withAlpha (0.1f));
    g.fillRect (rc);

    if (slider.isEnabled())
        g.setColour (slider.findColour (juce::Slider::thumbColourId).withAlpha (isMouseOver ? 0.95f : 0.85f));

    if (slider.isHorizontal())
    {
        float t = rc.getY() + 0.5f;
        float h = rc.getHeight() - 1.0f;

        if (slider.getProperties().contains ("fromCentre"))
        {
            auto c = rc.getCentreX();
            if (sliderPos < c)
                g.fillRect (juce::Rectangle<float> (sliderPos, t, c - sliderPos, h));
            else
                g.fillRect (juce::Rectangle<float> (float (c), t, sliderPos - c, h));
        }
        else
        {
            g.fillRect (juce::Rectangle<float> (float (rc.getX()), float (rc.getY()), sliderPos - rc.getX(), float (rc.getHeight())));
        }
    }
    else
    {
        g.fillRect (juce::Rectangle<float> (rc.getX() + 0.5f, sliderPos, rc.getWidth() - 1.0f, rc.getY() + (rc.getHeight() - sliderPos)));
    }
}

void ResonariumLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                       const float rotaryStartAngleIn, const float rotaryEndAngle, juce::Slider& slider)
{
    if(slider.getProperties()["textOnly"])
    {
        float value = slider.getValue();
        float min = slider.getMinimum();
        float max = slider.getMaximum();

        float proportion = (value - min) / (max - min);

        g.setColour(slider.findColour(juce::Slider::thumbColourId).withAlpha(1.0f));
        bool showModDepth = slider.getProperties().contains("modDepth");
        if((slider.isEnabled() && (slider.isMouseOverOrDragging() || showModDepth)))
        {
            g.fillRect(x + 3, y + 18, static_cast<int>(juce::jmap<float>(proportion, 0.0f, width - 3.0f)), 2);
        }

        if (showModDepth)
        {
            bool bipolar = (bool)slider.getProperties()["modBipolar"];
            if(bipolar)
            {
                auto depth = std::abs((float)slider.getProperties()["modDepth"]);
                float start = juce::jmax(0.0f, proportion - depth);
                float end = juce::jmin(1.0f, proportion + depth);
                g.setColour(juce::Colours::white);
                float rX = x + 3 + static_cast<int>(juce::jmap<float>(start, 0.0f, width - 3.0f));
                float rY = y + 16;
                float rW = proportion == 0 ? 0 : static_cast<int>(juce::jmap<float>(end - start, 0.0f, width - 3.0f));
                float rH = 2;
                g.fillRect(rX, rY, rW, rH);
            } else
            {
                auto depth = (float)slider.getProperties()["modDepth"];
                float start = proportion;
                float end = juce::jlimit(0.0f, 1.0f, proportion + depth);
                g.setColour(juce::Colours::white);
                float rX = x + 3 + static_cast<int>(juce::jmap<float>(start, 0.0f, width - 3.0f));
                float rY = y + 16;
                float rW = proportion == 0 ? 0 : static_cast<int>(juce::jmap<float>(end - start, 0.0f, width - 3.0f));
                float rH = 2;
                g.fillRect(rX, rY, rW, rH);
            }
        }

        if (slider.getProperties().contains ("modValues") && slider.isEnabled())
        {
            // g.setColour (findColour (GinLookAndFeel::whiteColourId).withAlpha (0.9f));
            g.setColour(juce::Colours::palevioletred.withLightness(0.7f).withAlpha(1.0f));
            auto varArray = slider.getProperties()["modValues"];
            if (varArray.isArray())
            {
                for (auto value : *varArray.getArray())
                {
                    g.fillEllipse (x + 3 + ((float) value) * (width - 7), y + 17, 4.0f, 4.0f);
                }
            }

            g.setColour(juce::Colours::teal.withLightness(0.7f).withAlpha(1.0f));
            varArray = slider.getProperties()["modValuesR"];
            if (varArray.isArray())
            {
                for (auto value : *varArray.getArray())
                {
                    g.fillEllipse (x + 3 + (float) value * (width - 7), y + 17, 4.0f, 4.0f);
                }
            }
        }

        return;
    } else
    {

    }

    float rotaryStartAngle = rotaryStartAngleIn;
    const float radius = juce::jmin (width / 2, height / 2) - 2.0f;
    const float centreX = x + width * 0.5f;
    const float centreY = y + height * 0.5f;
    const float rx = centreX - radius;
    const float ry = centreY - radius;
    const float rw = radius * 2.0f;
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    const bool isMouseOver = slider.isMouseOverOrDragging() && slider.isEnabled();

    const float thickness = (radius - 1) / radius;

    {
        const auto scl = 1.3f;
        const auto pi = juce::MathConstants<float>::pi;
        const auto rcO = juce::Rectangle<float> (rx, ry, rw, rw).withSizeKeepingCentre (radius * scl, radius * scl);
        const auto rcI = juce::Rectangle<float> (rx, ry, rw, rw).withSizeKeepingCentre (radius * 0.17f, radius * 0.17f);
        const auto c = 2.0f * pi * radius * scl;
        const auto gap = (rcI.getWidth () / c) * 2.0f * pi;

        juce::Path knob;
        knob.addArc (rcO.getX(), rcO.getY(), rcO.getWidth(), rcO.getHeight(), angle + gap, angle - gap + pi * 2, true );
        knob.addArc (rcI.getX(), rcI.getY(), rcI.getWidth(), rcI.getHeight(), angle - pi / 2, angle + pi / 2 - pi * 2, false );
        knob.closeSubPath();
        if(slider.isEnabled())
        {
            juce::DropShadow shadow(juce::Colours::black, 24, juce::Point<int>{0,4});
            shadow.drawForPath(g, knob);
        }
        if(!slider.isEnabled())
        {
            g.setColour (slider.findColour (juce::Slider::trackColourId).withMultipliedAlpha(0.5f));
        } else if(isMouseOver)
        {
            g.setColour (slider.findColour (juce::Slider::trackColourId).brighter (0.3f));
        } else
        {
            g.setColour (slider.findColour (juce::Slider::trackColourId).withMultipliedAlpha (1.0f));
        }
        g.fillPath (knob);
    }

    {
        juce::Path filledArc;
        filledArc.addPieSegment (rx, ry, rw, rw, rotaryStartAngle, rotaryEndAngle, thickness);
        g.fillPath (filledArc);
    }

    if (slider.isEnabled())
    {
        // if(slider.getProperties().contains("customColour"))
        // {
        //     g.setColour(slider.getProperties()["customColour"]);
        // }
        g.setColour (slider.findColour (juce::Slider::rotarySliderFillColourId).withAlpha (isMouseOver ? 0.95f : 0.85f));
    }

    auto fillStartAngle = rotaryStartAngle;
    if (slider.getProperties().contains ("fromCentre"))
        fillStartAngle = (rotaryStartAngle + rotaryEndAngle) / 2;

    {
        juce::Path filledArc;
        filledArc.addPieSegment (rx, ry, rw, rw, fillStartAngle, angle, thickness);
        g.fillPath (filledArc);
    }

    if (slider.getProperties().contains ("modDepth"))
    {
        auto depth = (float)slider.getProperties()["modDepth"];
        bool bipolar = (bool)slider.getProperties()["modBipolar"];

        g.setColour (findColour (GinLookAndFeel::whiteColourId).withAlpha (0.9f));

        juce::Path filledArc;
        if (bipolar)
        {
            auto a = juce::jlimit (rotaryStartAngle, rotaryEndAngle, angle - depth * (rotaryEndAngle - rotaryStartAngle));
            auto b = juce::jlimit (rotaryStartAngle, rotaryEndAngle, angle + depth * (rotaryEndAngle - rotaryStartAngle));
            filledArc.addPieSegment (rx, ry, rw, rw, std::min (a, b), std::max (a, b), thickness);
        }
        else
        {
            auto modPos = juce::jlimit (rotaryStartAngle, rotaryEndAngle, angle + depth * (rotaryEndAngle - rotaryStartAngle));
            filledArc.addPieSegment (rx, ry, rw, rw, angle, modPos, thickness);
        }

        g.fillPath (filledArc);
    }

    if (slider.getProperties().contains ("modValues") && slider.isEnabled())
    {
        // g.setColour (findColour (GinLookAndFeel::whiteColourId).withAlpha (0.9f));
        g.setColour(juce::Colours::palevioletred.withLightness(0.7f).withAlpha(1.0f));
        auto varArray = slider.getProperties()["modValues"];
        if (varArray.isArray())
        {
            for (auto value : *varArray.getArray())
            {
                float modAngle = float (value) * (rotaryEndAngle - rotaryStartAngle) + rotaryStartAngle;

                float modX = centreX + std::sin (modAngle) * radius;
                float modY = centreY - std::cos (modAngle) * radius;

                g.fillEllipse (modX - 2, modY - 2, 4.0f, 4.0f);
            }
        }

        g.setColour(juce::Colours::teal.withLightness(0.8f).withAlpha(1.0f));
        varArray = slider.getProperties()["modValuesR"];
        if (varArray.isArray())
        {
            for (auto value : *varArray.getArray())
            {
                float modAngle = float (value) * (rotaryEndAngle - rotaryStartAngle) + rotaryStartAngle;

                float modX = centreX + std::sin (modAngle) * radius;
                float modY = centreY - std::cos (modAngle) * radius;

                g.fillEllipse (modX - 2, modY - 2, 4.0f, 4.0f);
            }
        }
    }
}

void ResonariumLookAndFeel::drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool)
{
}

void ResonariumLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    auto c = b.findColour (b.getToggleState() ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId).withMultipliedAlpha (b.isEnabled() ? 1.0f : 0.5f);

    if (b.isMouseOver() && b.isEnabled())
        c = c.brighter();

    g.setColour (c);

    if(auto svg = dynamic_cast<SVGFilePluginButton*>(&b))
    {
        //do nothing right now, in the future we'll draw the SVG here L&F
    }
    else if (auto svg = dynamic_cast<gin::SVGButton*> (&b))
    {
        juce::Path path;
        if(b.getToggleState() && svg->rawSVGEnabled.isNotEmpty())
        {
            path = gin::parseSVGPath(svg->rawSVGEnabled);
        }
        else
        {
            path = gin::parseSVGPath (svg->rawSVG);
        }
        auto font = getTextButtonFont (b, b.getHeight());

        int sz = std::min (b.getHeight(), b.getWidth()) - svg->inset;

        auto rc = b.getLocalBounds().toFloat().withSizeKeepingCentre (float (sz), float (sz));
        g.fillPath (path, path.getTransformToScaleToFit (rc, true));
    }
    else
    {
        g.setFont (getTextButtonFont (b, b.getHeight()));
        g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
    }
}

void ResonariumLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool /*isButtonDown*/,
                                   int /*buttonX*/, int /*buttonY*/, int /*buttonW*/, int /*buttonH*/,
                                   juce::ComboBox& box)
{
    const juce::Rectangle<int> boxBounds (0, 0, width, height);

    juce::ColourGradient grad (findColour (PluginLookAndFeel::glass1ColourId), 0, 0,
                               findColour (PluginLookAndFeel::glass2ColourId), 0, float (height), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (boxBounds.toFloat(), boxBounds.getHeight() / 2.0f);

    g.setColour (box.findColour (juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle (boxBounds.toFloat(), boxBounds.getHeight() / 2.0f, 1.0f);
}

void ResonariumLookAndFeel::drawTooltip(juce::Graphics& g, const juce::String& text, int width, int height)
{
    juce::Rectangle<int> bounds (width, height);
    auto cornerSize = 5.0f;

    g.setColour (findColour(backgroundColourId));
    g.fillRoundedRectangle (bounds.toFloat(), cornerSize);

    g.setColour (findColour (accentColourId));
    g.drawRoundedRectangle (bounds.toFloat().reduced (0.5f, 0.5f), 0, 1.0f);

    juce::detail::LookAndFeelHelpers::layoutTooltipText (getDefaultMetricsKind(), text, findColour (juce::TooltipWindow::textColourId))
        .draw (g, { static_cast<float> (width), static_cast<float> (height) });
    // g.setColour(juce::Colours::white.withAlpha(0.8f));
    // g.setFont(defaultFont.withHeight(14).withExtraKerningFactor(0.05f));
    // g.drawFittedText(text, bounds.reduced(5, 0), juce::Justification::centredLeft, 50);
}

void ResonariumLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (1, 1, box.getWidth() - 1, box.getHeight() - 1);
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centred);
}

void ResonariumLookAndFeel::drawTextEditorOutline (juce::Graphics& g, int width, int height, juce::TextEditor& textEditor)
{
    if (dynamic_cast<juce::AlertWindow*> (textEditor.getParentComponent()) == nullptr)
    {
        if (textEditor.isEnabled())
        {
            if (textEditor.hasKeyboardFocus (true) && ! textEditor.isReadOnly())
            {
                g.setColour (textEditor.findColour (juce::TextEditor::focusedOutlineColourId));
                g.drawRect (0, 0, width, height, 2);
            }
            else
            {
                g.setColour (textEditor.findColour (juce::TextEditor::outlineColourId));
                g.drawRect (0, 0, width, height);
            }
        }
    }
}

juce::PopupMenu::Options ResonariumLookAndFeel::getOptionsForComboBoxPopupMenu (juce::ComboBox& box, juce::Label&)
{
    return juce::PopupMenu::Options().withTargetComponent (&box)
                                     .withItemThatMustBeVisible (box.getSelectedId())
                                     .withInitiallySelectedItem (box.getSelectedId())
                                     .withMinimumWidth (box.getWidth())
                                     .withMaximumNumColumns (20);
}

juce::Font ResonariumLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
   return juce:: FontOptions{}.withName ("Jost*").withStyle ("Medium").withPointHeight(14);

    // return juce::Font ("Futura", "Medium", 16.0f); // Change to your preferred font
}

juce::Font ResonariumLookAndFeel::getPopupMenuFont()
{
   return juce:: FontOptions{}.withName ("Jost*").withStyle ("Medium").withPointHeight(14);
}

// Override the font for Labels
juce::Font ResonariumLookAndFeel::getLabelFont (juce::Label& label)
{
    if(true)
    {
        return defaultFont.withHeight(label.getFont().getHeight()).withExtraKerningFactor(label.getFont().getExtraKerningFactor());
    } else
    {
        return defaultFont.withHeight(14.5f);
    }
}

//==============================================================================
ResonariumLookAndFeelWrapper::ResonariumLookAndFeelWrapper()
{
    juce::LookAndFeel::setDefaultLookAndFeel (this);
}

ResonariumLookAndFeelWrapper::~ResonariumLookAndFeelWrapper()
{
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
}

