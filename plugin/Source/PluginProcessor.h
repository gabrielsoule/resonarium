#pragma once

#include <JuceHeader.h>

#include "ResonatorSynth.h"
#include "Parameters.h"
#include "util/TooltipManager.h"

//==============================================================================
class ResonariumProcessor : public gin::Processor
{
public:
    ResonariumProcessor();
    ~ResonariumProcessor() override;

    void stateUpdated() override;
    void updateState() override;
    void reset() override;
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;
    bool supportsMPE() const override { return true; }
    void setupModMatrix();
    static static gin::ProcessorOptions getOptions();

    ResonatorSynth synth;
    gin::ModMatrix modMatrix;
    gin::ModSrcId
        modSrcPressure,
        modSrcTimbre,
        modSrcPitchbend,
        modSrcNote,
        modSrcVelocity;
    juce::Array<gin::ModSrcId>
        modSrcMonoLFO,
        modSrcPolyLFO,
        modSrcMonoRND,
        modSrcPolyRND,
        modSrcPolyENV,
        modSrcMonoMSEG,
        modSrcPolyMSEG,
        modSrcCC;
    //contains all the parameters that a voice needs modulated polyphonically (which is almost all of 'em)
    UIParams uiParams;
    gin::AudioFifo scopeFifo { 2, 44100 };
    TooltipManager tooltips;

    //A copy of the input buffer, which is accessed by Ext. In exciters.
    juce::AudioBuffer<float> inputBuffer;

    static void testFilter(float mode, float Q, bool readout)
    {
        int order = 13;
        juce::dsp::FFT fft(order);
        int fftSize = std::pow(2, order);
        juce::AudioBuffer<float> impulseResponse;
        juce::AudioBuffer<float> spectrum;
        chowdsp::SVFMultiMode<float, 1, true> svf;
        svf.setCutoffFrequency(1000);
        svf.setMode(mode);
        svf.setQValue(Q);
        svf.update();
        juce::dsp::ProcessSpec spec{44100, 256, 1};

        svf.prepare(spec);

        impulseResponse.setSize(1, fftSize * 2);
        spectrum.setSize(1, fftSize * 2);

        impulseResponse.clear();
        impulseResponse.setSample(0, 0, 1.0f);

        for (int i = 0; i < fftSize; i++)
        {
            impulseResponse.setSample(0, i, svf.processSample(0, impulseResponse.getSample(0, i)));
        }

        fft.performFrequencyOnlyForwardTransform(impulseResponse.getWritePointer(0), true); // Perform

        auto str = juce::String();
        for (int i = 0; i < fftSize / 2; i++)
        {
            str = str + juce::String(impulseResponse.getSample(0, i)) + " ";
        }

        DBG("Testing unity gain SVF with mode " + juce::String(mode) + " and Q " + juce::String(Q));
        DBG("Min amplitude: " + juce::String(impulseResponse.findMinMax(0, 0, impulseResponse.getNumSamples()).getStart()));
        DBG("Max amplitude: " + juce::String(impulseResponse.findMinMax(0, 0, impulseResponse.getNumSamples()).getEnd()));
        DBG("Expected gain:" + juce::String(svf.getPeakGain()));
        //pass if unity gain
        if(impulseResponse.findMinMax(0, 0, impulseResponse.getNumSamples()).getEnd() > 1.0f)
        {
            DBG("Test status: FAIL");
        } else
        {
            DBG("Test status: PASS");
        }
        if(readout)
        {
            DBG("Raw Data:");
            DBG(str);
        }



    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ResonariumProcessor)
};
