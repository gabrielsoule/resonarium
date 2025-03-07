/**
* Collection of commonly used UI components
 */
#ifndef RESONARIUMCOMPONENTS_H
#define RESONARIUMCOMPONENTS_H

#include "ClickThruSelect.h"
#include "TextSlider.h"
#include "../Parameters.h"
#include "../defines.h"
#include "BinaryData.h"
#include <melatonin_blur/melatonin_blur.h>

static constexpr int SPACING_X = 10; // how far apart are resonator columns?
static constexpr int SPACING_Y_SMALL = 1;
static constexpr int SPACING_Y_LARGE = 10;
static constexpr int PARAMETER_WIDTH = 68;
static constexpr int PARAMETER_HEIGHT = 25;
static constexpr int PARAMETER_TEXT_HEIGHT = 14;

// inline juce::String pianoKeyboardSVG = "m79.398 7.1992h-58.797c-7.3984 0-13.5 6.1016-13.5 13.5v58.801c0 7.3984 6.1016 13.5 13.5 13.5h58.801c7.3984 0 13.5-6.1016 13.5-13.5l-0.003906-58.801c0-7.5-6-13.5-13.5-13.5zm-40.699 53.699c2.3008 0 4.3008-1.8984 4.3008-4.3008v-46.398h14v46.5c0 2.3008 1.8984 4.3008 4.3008 4.3008h2v29h-26.602v-29.102zm-28.598 18.602v-58.801c0-5.8008 4.6992-10.5 10.5-10.5h6.8008v46.5c0 2.3008 1.8984 4.3008 4.3008 4.3008h2l-0.003906 29h-13.098c-5.8008 0-10.5-4.6992-10.5-10.5zm79.797 0c0 5.8008-4.6992 10.5-10.5 10.5h-13.098v-29.102h2c2.3008 0 4.3008-1.8984 4.3008-4.3008v-46.398h6.8008c5.8008 0 10.5 4.6992 10.5 10.5z";
static inline juce::String pianoKeyboardSVG =
    "m71.668 28.449h-43.336c-1.8359 0-3.332 1.4961-3.332 3.332v36.434c0 1.8359 1.4961 3.332 3.332 3.332h43.332c1.8398 0 3.332-1.4961 3.332-3.332l0.003906-36.43c0-1.8398-1.4961-3.3359-3.332-3.3359zm-31.395 38.934h-11.109l0.003907-34.766h8.3008v20.141h2.8047zm15.285 0h-11.121l0.003906-14.625h2.8047v-20.141h5.5078v20.141h2.8047zm15.273 0h-11.109l0.003906-14.625h2.8047v-20.141h8.3008z";
static inline juce::String unlockSVG =
    "M4 30.016v-14.016q0-0.832 0.576-1.408t1.44-0.576v-4q0-2.72 1.344-5.024t3.616-3.648 5.024-1.344q3.616 0 6.368 2.272t3.424 5.728h-4.16q-0.608-1.76-2.144-2.88t-3.488-1.12q-2.496 0-4.256 1.76t-1.728 4.256v4h16q0.8 0 1.408 0.576t0.576 1.408v14.016q0 0.832-0.576 1.408t-1.408 0.576h-20q-0.832 0-1.44-0.576t-0.576-1.408zM8 28h16v-9.984h-16v9.984z";
static inline juce::String lockSVG =
    "M4 30.016q0 0.832 0.576 1.408t1.44 0.576h20q0.8 0 1.408-0.576t0.576-1.408v-14.016q0-0.832-0.576-1.408t-1.408-0.576v-4q0-2.048-0.8-3.872t-2.144-3.2-3.2-2.144-3.872-0.8q-2.72 0-5.024 1.344t-3.616 3.648-1.344 5.024v4q-0.832 0-1.44 0.576t-0.576 1.408v14.016zM8 28v-9.984h16v9.984h-16zM10.016 14.016v-4q0-2.496 1.728-4.256t4.256-1.76 4.256 1.76 1.76 4.256v4h-12z";
static inline juce::String chainSVG =
    "M0 18.016q0 2.496 1.76 4.224t4.256 1.76h1.984q1.952 0 3.488-1.12t2.144-2.88h-7.616q-0.832 0-1.44-0.576t-0.576-1.408v-4q0-0.832 0.576-1.408t1.44-0.608h7.616q-0.608-1.76-2.144-2.88t-3.488-1.12h-1.984q-2.496 0-4.256 1.76t-1.76 4.256v4zM8 16q0 0.832 0.576 1.44t1.44 0.576h12q0.8 0 1.408-0.576t0.576-1.44-0.576-1.408-1.408-0.576h-12q-0.832 0-1.44 0.576t-0.576 1.408zM18.368 20q0.64 1.792 2.176 2.912t3.456 1.088h2.016q2.464 0 4.224-1.76t1.76-4.224v-4q0-2.496-1.76-4.256t-4.224-1.76h-2.016q-1.92 0-3.456 1.12t-2.176 2.88h7.648q0.8 0 1.408 0.608t0.576 1.408v4q0 0.832-0.576 1.408t-1.408 0.576h-7.648z";
/**
 * A component that draws a nice bracket around some content,
 * like you'd see on the control panel of the Eagle lunar module.
 */
class BracketComponent : public juce::Component
{
public:
    enum class Orientation
    {
        left,
        right,
        top,
        bottom
    };

    BracketComponent(Orientation orientation) : orientation(orientation)
    {
        this->orientation = orientation;
        this->colour = juce::Colours::white.withAlpha(0.5f);
        setInterceptsMouseClicks(false, false);
    }

    void paint(juce::Graphics& g) override
    {
        //compute the center point as the center of the edge corresponding to the justification
        g.setColour(colour);
        juce::Point<float> center;
        float length = -1;
        switch (orientation)
        {
        case Orientation::left:
            //make sure to take into account the thickness so that the bracket is drawn entirely inside the component
            center = juce::Point<float>(0 + thickness / 2.0f, getHeight() / 2.0f);
            length = getHeight() - thickness;
            break;
        case Orientation::right:
            center = juce::Point<float>(getWidth() - thickness / 2.0f, getHeight() / 2.0f);
            length = getHeight() - thickness;
            break;
        case Orientation::top:
            center = juce::Point<float>(getWidth() / 2.0f, thickness / 2.0f);
            length = getWidth() - thickness;
            break;
        case Orientation::bottom:
            center = juce::Point<float>(getWidth() / 2.0f, getHeight() -  thickness / 2.0f);
            length = getWidth() - thickness;
            break;
        }



        drawBracket(g, center, length, orientation);
    }

    void drawBracket(const juce::Graphics& g, const juce::Point<float>& center, float length,
                     Orientation orientation) const
    {
        jassert(length >= 0);
        juce::Path bracketPath;
        float halfLength = length / 2.0f;

        switch (orientation)
        {
        case Orientation::left:
            bracketPath.startNewSubPath(center.x, center.y - halfLength);
            bracketPath.lineTo(center.x - inset, center.y - halfLength);
            bracketPath.lineTo(center.x - inset, center.y + halfLength);
            bracketPath.lineTo(center.x, center.y + halfLength);
            break;

        case Orientation::right:
            bracketPath.startNewSubPath(center.x + inset, center.y - halfLength);
            bracketPath.lineTo(center.x, center.y - halfLength);
            bracketPath.lineTo(center.x, center.y + halfLength);
            bracketPath.lineTo(center.x + inset, center.y + halfLength);
            break;

        case Orientation::top:
            bracketPath.startNewSubPath(center.x - halfLength, center.y + inset);
            bracketPath.lineTo(center.x - halfLength, center.y);
            bracketPath.lineTo(center.x + halfLength, center.y);
            bracketPath.lineTo(center.x + halfLength, center.y + inset);
            break;

        case Orientation::bottom:
            bracketPath.startNewSubPath(center.x - halfLength, center.y - inset);
            bracketPath.lineTo(center.x - halfLength, center.y);
            bracketPath.lineTo(center.x + halfLength, center.y);
            bracketPath.lineTo(center.x + halfLength, center.y - inset);
            break;

        default:
            jassertfalse; // Invalid orientation
            return;
        }

        g.strokePath(bracketPath, juce::PathStrokeType(thickness));
    }

    Orientation orientation;
    float thickness = 2;
    float inset = 10;
    juce::Colour colour;
};

class ResonariumLogo : public juce::Component
{
public:
    ResonariumLogo()
    {
        auto logoXml = juce::XmlDocument::parse(BinaryData::resonarium_logo_svg_svg);
        drawableSVG = juce::Drawable::createFromSVG(*logoXml);
        drawableSVG->setName("Logo Drawable");
        addAndMakeVisible(drawableSVG.get());
    }

    void resized() override
    {
        //make the drawable fill the component
        drawableSVG->setBounds(getLocalBounds());
        drawableSVG->setTransformToFit(getLocalBounds().toFloat(), juce::RectanglePlacement::xLeft);
    }

    void lookAndFeelChanged() override
    {
        for (int i = 0; i < drawableSVG->getNumChildComponents(); ++i)
        {
            if (auto* path = dynamic_cast<juce::DrawablePath*>(drawableSVG->getChildComponent(i)))
            {
                path->setStrokeFill(juce::FillType(juce::Colours::white.withAlpha(0.85f)));
            }
        }
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(juce::Colours::green);
        drawableSVG->paint(g);
    }

    std::unique_ptr<juce::Drawable> drawableSVG;
};

/**
 * An extension of the Gin plugin button that uses Juce's modern SVG rendering.
 * This replacement can render an entire SVG file rather than just a single path.
 */
class SVGFilePluginButton : public gin::SVGPluginButton
{
public:
    SVGFilePluginButton(gin::Parameter* p, juce::String svg, juce::String svgEnabled) : gin::SVGPluginButton(p, svg, svgEnabled)
    {
        auto svgXml = juce::XmlDocument::parse(svg);
        auto svgXmlEnabled = juce::XmlDocument::parse(svg);
        //there has to be a better way here... right? Why doesn't JUCE just return a DrawableComposite?
        drawableSVGDisabled = std::unique_ptr<juce::DrawableComposite>(dynamic_cast<juce::DrawableComposite*>(juce::Drawable::createFromSVG(*svgXml).release()));
        drawableSVGEnabled = std::unique_ptr<juce::DrawableComposite>(dynamic_cast<juce::DrawableComposite*>(juce::Drawable::createFromSVG(*svgXmlEnabled).release()));
        drawableSVGDisabled->setName(p->getName(100) + " Disabled");
        drawableSVGEnabled->setName(p->getName(100) + " Enabled");

        addAndMakeVisible(drawableSVGDisabled.get());
        addAndMakeVisible(drawableSVGEnabled.get());

        for (int i = 0; i < drawableSVGDisabled->getNumChildComponents(); ++i)
        {
            if (auto* path = dynamic_cast<juce::DrawablePath*>(drawableSVGDisabled->getChildComponent(i)))
            {
                disabledSVGPaths.add(path);
            }
        }

        for (int i = 0; i < drawableSVGEnabled->getNumChildComponents(); ++i)
        {
            if (auto* path = dynamic_cast<juce::DrawablePath*>(drawableSVGEnabled->getChildComponent(i)))
            {
                enabledSVGPaths.add(path);
            }
        }

        SVGFilePluginButton::valueUpdated(p);
    }

    void lookAndFeelChanged() override
    {
        for (int i = 0; i < drawableSVGDisabled->getNumChildComponents(); ++i)
        {
            if (auto* path = dynamic_cast<juce::DrawablePath*>(drawableSVGDisabled->getChildComponent(i)))
            {
                path->setFill(juce::Colours::white.withAlpha(0.5f));
                path->setStrokeFill(juce::Colours::white.withAlpha(0.5f));
            }
        }

        for (int i = 0; i < drawableSVGEnabled->getNumChildComponents(); ++i)
        {
            if (auto* path = dynamic_cast<juce::DrawablePath*>(drawableSVGEnabled->getChildComponent(i)))
            {
                if (getLookAndFeel().isColourSpecified(ResonariumLookAndFeel::accentColourId))
                {
                    path->setFill(getLookAndFeel().findColour(ResonariumLookAndFeel::accentColourId));
                    path->setStrokeFill(getLookAndFeel().findColour(ResonariumLookAndFeel::accentColourId));
                } else
                {
                    path->setFill(juce::Colours::white);
                    path->setStrokeFill(juce::Colours::white);
                }
            }
        }
    }

    void valueUpdated(gin::Parameter* p) override
    {
        SVGPluginButton::valueUpdated(p);
        drawableSVGDisabled->setVisible(!this->getToggleState());
        drawableSVGEnabled->setVisible(this->getToggleState());
    }

    void mouseEnter(const juce::MouseEvent& event) override
    {
        SVGPluginButton::mouseEnter(event);
    }

    void resized() override
    {
        SVGPluginButton::resized();
        drawableSVGDisabled->setTransformToFit(getLocalBounds().toFloat(), juce::RectanglePlacement::xLeft);
        drawableSVGEnabled->setTransformToFit(getLocalBounds().toFloat(), juce::RectanglePlacement::xLeft);
        drawableSVGDisabled->resetBoundingBoxToContentArea();
        drawableSVGEnabled->resetBoundingBoxToContentArea();
    }

    std::unique_ptr<juce::DrawableComposite> drawableSVGDisabled;
    juce::Array<juce::DrawablePath*> disabledSVGPaths;
    std::unique_ptr<juce::DrawableComposite> drawableSVGEnabled;
    juce::Array<juce::DrawablePath*> enabledSVGPaths;

};

class CircleEnableButton : public gin::PluginButton
{
public:
    CircleEnableButton(gin::Parameter* p) : PluginButton(p), onColor(ResonariumLookAndFeel::accentColourId)
    {
    }

    CircleEnableButton(gin::Parameter* p, juce::Colour onColor) : PluginButton(p), onColor(onColor)
    {
    }

    CircleEnableButton(gin::Parameter* p, juce::Colour onColor, bool showName) : PluginButton(p), onColor(onColor),
        showName(showName)
    {
        addAndMakeVisible(name);
        name.setText(p->getShortName(), juce::dontSendNotification);
        name.setJustificationType(juce::Justification::centred);
        name.setColour(juce::Label::textColourId, juce::Colours::white);
    }

    void resized() override
    {
        if (showName)
        {
            juce::Rectangle<int> r = getLocalBounds().reduced(2, 0);
            auto rc = r.removeFromBottom(15);

            name.setBounds(rc);
        }
    }

    void paintButton(juce::Graphics& g, bool over, bool down) override
    {
        auto c = isEnabled() ? onColor : juce::Colours::darkgrey.withAlpha(0.5f);
        if (over || down)
            c = c.withMultipliedAlpha(1.2f);

        g.setColour(c);

        auto r = getLocalBounds().toFloat().reduced(2.0f);
        //if enabled draw a circle, otherwise drawa hollow circle
        //find a centered square that fits within the bounds
        auto square = r.getIntersection(r.withSizeKeepingCentre(r.getHeight(), r.getHeight()));
        if (getToggleState())
        {
            g.drawEllipse(square, 2.0f);
            g.fillEllipse(square.reduced(3));
        }
        else
        {
            g.drawEllipse(square, 2.0f);
        }
    }

    void parentHierarchyChanged() override
    {
        PluginButton::parentHierarchyChanged();
        auto a = wantsAccessibleKeyboard(*this);
        setWantsKeyboardFocus(a);
    }

    juce::Colour onColor;
    juce::Label name;
    bool showName;
};

class TitleBarDropShadow : public juce::Component
{
public:
    TitleBarDropShadow()
    {
        setInterceptsMouseClicks(false, false);
        shadow.setOpacity(1.0f);
        shadow.setSpread(5);
    }

    void paint(juce::Graphics& g) override
    {
        // drop shadows get painted *before* the path
        shadow.render(g, valueTrack);

        g.setColour(juce::Colours::transparentWhite);
        g.fillPath(valueTrack);
    }

    void resized() override
    {
        valueTrack.clear();
        valueTrack.addEllipse(40, 5, 120, 100);
    }

    void lookAndFeelChanged() override
    {
        if (getLookAndFeel().isColourSpecified(ResonariumLookAndFeel::accentColourId))
        {
            shadow.setColor(findColour(ResonariumLookAndFeel::accentColourId).brighter(0.2f));
        }
    }

private:
    juce::Path valueTrack;
    melatonin::DropShadow shadow = {juce::Colours::purple, 110, {0, 0}};
};

class GlowComponent : public juce::Component
{
public:
    struct GlowParameters
    {
        juce::Colour colour = juce::Colours::purple;
        float opacity = 1.0f;
        float spread = 5.0f;
        float radiusX = 60.0f;
        float radiusY = 50.0f;
        float glowRadius = 100.0f;
        float offsetX = 0.0f;
        float offsetY = 0.0f;
    };

    void paint(juce::Graphics& g) override
    {
        shadow.render(g, glowPath);

        g.setColour(juce::Colours::transparentWhite);
        g.fillPath(glowPath);
    }

    void resized() override
    {
        updateGlowPath();
    }

    void setParameters(const GlowParameters& newParams)
    {
        parameters = newParams;
        updateShadow();
        updateGlowPath();
        repaint();
    }

    const GlowParameters& getParameters() const
    {
        return parameters;
    }

private:
    void updateShadow()
    {
        shadow.setColor(parameters.colour);
        shadow.setOpacity(parameters.opacity);
        shadow.setSpread(parameters.spread);
        shadow.setRadius(parameters.glowRadius);
    }

    void updateGlowPath()
    {
        auto bounds = getLocalBounds().toFloat();
        float centerX = bounds.getCentreX() + parameters.offsetX;
        float centerY = bounds.getCentreY() + parameters.offsetY;

        glowPath.clear();
        glowPath.addEllipse(centerX - parameters.radiusX,
                            centerY - parameters.radiusY,
                            parameters.radiusX * 2,
                            parameters.radiusY * 2);
    }

    GlowParameters parameters;
    juce::Path glowPath;
    melatonin::DropShadow shadow;
};

class WaveguideResonatorComponent_V2 : public gin::MultiParamComponent
{
public:
    explicit WaveguideResonatorComponent_V2(WaveguideResonatorBankParams enclosingBankParams, ResonatorParams resonatorParams, juce::Colour colour) :
        resonatorParams(resonatorParams), colour(colour)
    {
        enableButton = new gin::SVGPluginButton(resonatorParams.enabled, gin::Assets::power);
        enableButton->setColour(juce::TextButton::buttonOnColourId, colour);
        enableButton->setColour(juce::TextButton::buttonColourId, colour);
        soloButton = new SVGFilePluginButton(enclosingBankParams.soloResonator, BinaryData::letter_s_svg, BinaryData::letter_s_svg);
        // soloButton->setColour(juce::TextButton::buttonOnColourId, juce::Colours::red);
        // soloButton->setColour(juce::TextButton::buttonColourId, juce::Colours::blue);
        soloButton->onClick = [param = enclosingBankParams.soloResonator, bankIndex = enclosingBankParams.index, resonatorIndex = resonatorParams.resonatorIndex]() {

            //map the resonator index and bank index to a single integer
            const int id = NUM_RESONATORS * bankIndex + resonatorIndex;
            int paramValue = static_cast<int>(param->getProcValue());
            if(id == paramValue)
            {
                param->setUserValueNotifingHost(-1); //disable solo
                DBG("Disabling solo from" + juce::String(id));
            }
            else
            {
                param->setUserValueNotifingHost(id);
                DBG("Enabling solo from" + juce::String(id));
            }
        };
        soloButton->onParamValueUpdated = [this, param = enclosingBankParams.soloResonator, bankIndex = enclosingBankParams.index, resonatorIndex = resonatorParams.resonatorIndex]() {
            const int id = NUM_RESONATORS * bankIndex + resonatorIndex;
            int paramValue = static_cast<int>(param->getProcValue());
            DBG("Updating solo from" + juce::String(id) + " to " + juce::String(paramValue));
            if(id == paramValue)
            {
                DBG("Setting solo to true, turning button on");
                soloButton->setToggleState(true, juce::dontSendNotification);
            }
            else
            {
                DBG("Setting solo to false, turning button off");
                soloButton->setToggleState(false, juce::dontSendNotification);
            }
        };
        gainKnob = new gin::Knob(resonatorParams.gain);
        gainKnob->getSlider().setColour(juce::Slider::rotarySliderFillColourId, colour);
        pitchKnob = new TextSlider(resonatorParams.pitch, colour);
        pitchKnob->setReadoutDecimals(2);
        pitchKnob->mainReadout.textToParamConversionFunction = [enclosingBankParams](float value) {
            if(enclosingBankParams.useSemitones->isOn())
            {
                return std::pow(2.0f, value / 12.0f);
            }
            else
            {
                return value;
            }
        };
        pitchKnob->mainReadout.paramToTextConversionFunction = [enclosingBankParams](float value) {
            if(enclosingBankParams.useSemitones->isOn())
            {
                return 12 * std::log2(value);
            }
            else
            {
                return value;
            }
        };
        resonatorFrequencyKnob = new TextSlider(resonatorParams.frequency, colour);
        resonatorFrequencyKnob->setReadoutDecimals(0);
        decayTimeKnob = new TextSlider(resonatorParams.decayTime, colour);
        decayTimeKnob->setReadoutDecimals(2);
        dispersionKnob = new TextSlider(resonatorParams.dispersion, colour);
        dispersionKnob->setReadoutDecimals(0);
        loopFilterCutoffKnob = new TextSlider(resonatorParams.loopFilterCutoff, colour);
        loopFilterCutoffKnob->setReadoutDecimals(0);
        loopFilterPitchKnob = new TextSlider(resonatorParams.loopFilterPitchInSemis, colour);
        loopFilterPitchKnob->setReadoutDecimals(2);
        loopFilterResonanceKnob = new TextSlider(resonatorParams.loopFilterResonance, colour);
        loopFilterResonanceKnob->setReadoutDecimals(2);
        // loopFilterModeKnob = new TextSlider(resonatorParams.loopFilterMode, colour);
        // loopFilterModeKnob->setReadoutDecimals(2);
        loopFilterTypeKnob = new ClickThruSelect(resonatorParams.loopFilterType, colour);
        postFilterCutoffKnob = new TextSlider(resonatorParams.postFilterCutoff, colour);
        postFilterCutoffKnob->setReadoutDecimals(0);
        postFilterPitchKnob = new TextSlider(resonatorParams.postFilterPitchInSemis, colour);
        postFilterPitchKnob->setReadoutDecimals(2);
        postFilterResonanceKnob = new TextSlider(resonatorParams.postFilterResonance, colour);
        postFilterResonanceKnob->setReadoutDecimals(2);
        postFilterModeKnob = new TextSlider(resonatorParams.postFilterMode, colour);
        postFilterModeKnob->setReadoutDecimals(2);

        resonatorKeytrack = new CircleEnableButton(resonatorParams.resonatorKeytrack, colour);
        postFilterKeytrack = new CircleEnableButton(resonatorParams.postFilterKeytrack, colour);
        loopFilterKeytrack = new CircleEnableButton(resonatorParams.loopFilterKeytrack, colour);


        addAndMakeVisible(enableButton);
        addAndMakeVisible(gainKnob);
        addAndMakeVisible(pitchKnob);
        addAndMakeVisible(resonatorFrequencyKnob);
        addAndMakeVisible(decayTimeKnob);
        addAndMakeVisible(dispersionKnob);
        addAndMakeVisible(loopFilterCutoffKnob);
        addAndMakeVisible(loopFilterPitchKnob);
        addAndMakeVisible(loopFilterResonanceKnob);
        addAndMakeVisible(loopFilterTypeKnob);
        addAndMakeVisible(postFilterCutoffKnob);
        addAndMakeVisible(postFilterPitchKnob);
        addAndMakeVisible(postFilterResonanceKnob);
        addAndMakeVisible(postFilterModeKnob);

        addAndMakeVisible(resonatorKeytrack);
        addAndMakeVisible(postFilterKeytrack);
        addAndMakeVisible(loopFilterKeytrack);
        addAndMakeVisible(soloButton);

        resonatorFrequencyKnob->setVisible(false);
        loopFilterPitchKnob->setVisible(false);
        postFilterPitchKnob->setVisible(false);

        watchParam(enableButton->parameter);
        watchParam(resonatorKeytrack->parameter);
        watchParam(loopFilterKeytrack->parameter);
        watchParam(postFilterKeytrack->parameter);
    }

    void resized() override
    {
        auto bounds = this->getLocalBounds();
        enableButton->setBounds(bounds.removeFromTop(12));
        bounds.removeFromTop(SPACING_Y_LARGE);
        //centre the soloButton within the bounds, as a square
        soloButton->setBounds(bounds.removeFromTop(PARAMETER_HEIGHT).withSizeKeepingCentre(15, 15));
        bounds.removeFromTop(SPACING_Y_SMALL);
        gainKnob->setBounds(
            bounds.removeFromTop(KNOB_W_SMALL).withWidth(KNOB_W_SMALL).translated(
                bounds.getWidth() / 2 - KNOB_W_SMALL / 2, 0));
        bounds.removeFromTop(SPACING_Y_SMALL);
        resonatorKeytrack->setBounds(bounds.removeFromTop(PARAMETER_HEIGHT).reduced(3));
        bounds.removeFromTop(SPACING_Y_SMALL);
        resonatorFrequencyKnob->setBounds(bounds.removeFromTop(PARAMETER_HEIGHT));
        pitchKnob->setBounds(resonatorFrequencyKnob->getBounds());
        bounds.removeFromTop(SPACING_Y_LARGE);
        decayTimeKnob->setBounds(bounds.removeFromTop(PARAMETER_HEIGHT));
        bounds.removeFromTop(SPACING_Y_LARGE);
        dispersionKnob->setBounds(bounds.removeFromTop(PARAMETER_HEIGHT));
        bounds.removeFromTop(SPACING_Y_LARGE);
        loopFilterKeytrack->setBounds(bounds.removeFromTop(PARAMETER_HEIGHT).reduced(3));
        bounds.removeFromTop(SPACING_Y_SMALL);
        loopFilterCutoffKnob->setBounds(bounds.removeFromTop(PARAMETER_HEIGHT));
        loopFilterPitchKnob->setBounds(loopFilterCutoffKnob->getBounds());
        bounds.removeFromTop(SPACING_Y_SMALL);
        loopFilterResonanceKnob->setBounds(bounds.removeFromTop(PARAMETER_HEIGHT));
        bounds.removeFromTop(SPACING_Y_SMALL);
        loopFilterTypeKnob->setBounds(bounds.removeFromTop(PARAMETER_HEIGHT));
        bounds.removeFromTop(SPACING_Y_LARGE);
        postFilterKeytrack->setBounds(bounds.removeFromTop(PARAMETER_HEIGHT).reduced(3));
        bounds.removeFromTop(SPACING_Y_SMALL);
        postFilterCutoffKnob->setBounds(bounds.removeFromTop(PARAMETER_HEIGHT));
        postFilterPitchKnob->setBounds(postFilterCutoffKnob->getBounds());
        bounds.removeFromTop(SPACING_Y_SMALL);
        postFilterResonanceKnob->setBounds(bounds.removeFromTop(PARAMETER_HEIGHT));
        bounds.removeFromTop(SPACING_Y_SMALL);
        postFilterModeKnob->setBounds(bounds.removeFromTop(PARAMETER_HEIGHT));
    }

    void paramChanged() override
    {
        // for some reason paramChanged() is being called before this component is added to the parent
        // this is fine, except that we can't access the lookandfeel at this point
        // this is a hack to get around that. There might be side effects. That's a later problem.
        if (this->getParentComponent() == nullptr)
        {
            return;
        }

        MultiParamComponent::paramChanged();
        for (auto c : this->getChildren())
        {
            if (c != enableButton) c->setEnabled(resonatorParams.enabled->isOn());
        }

        pitchKnob->setVisible(resonatorParams.resonatorKeytrack->isOn());
        resonatorFrequencyKnob->setVisible(!resonatorParams.resonatorKeytrack->isOn());

        loopFilterPitchKnob->setVisible(resonatorParams.loopFilterKeytrack->isOn());
        loopFilterCutoffKnob->setVisible(!resonatorParams.loopFilterKeytrack->isOn());

        postFilterPitchKnob->setVisible(resonatorParams.postFilterKeytrack->isOn());
        postFilterCutoffKnob->setVisible(!resonatorParams.postFilterKeytrack->isOn());
    }

    gin::SVGPluginButton* enableButton;
    gin::Knob* gainKnob;
    SVGFilePluginButton* soloButton;
    TextSlider* resonatorFrequencyKnob;
    TextSlider* pitchKnob;
    TextSlider* decayTimeKnob;
    TextSlider* dispersionKnob;
    TextSlider* loopFilterCutoffKnob;
    TextSlider* loopFilterPitchKnob;
    TextSlider* loopFilterResonanceKnob;
    ClickThruSelect* loopFilterTypeKnob;
    TextSlider* postFilterCutoffKnob;
    TextSlider* postFilterPitchKnob;
    TextSlider* postFilterResonanceKnob;
    TextSlider* postFilterModeKnob;

    CircleEnableButton* resonatorKeytrack;
    CircleEnableButton* loopFilterKeytrack;
    CircleEnableButton* postFilterKeytrack;

    ResonatorParams resonatorParams;
    juce::Colour colour;

    int backgroundRectangleStartY;
};

#endif //RESONARIUMCOMPONENTS_H
