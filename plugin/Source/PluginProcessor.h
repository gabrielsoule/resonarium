#pragma once

#include <JuceHeader.h>

#include "ResonatorSynth.h"
#include "Parameters.h"
#include <melatonin_perfetto/melatonin_perfetto.h>

#include "dsp/Sampler.h"

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
        modSrcCC,
        modSrcMacro;
    //contains all the parameters that a voice needs modulated polyphonically (which is almost all of 'em)
    UIParams uiParams;
    gin::AudioFifo scopeFifo { 2, 44100 };

    //A copy of the input buffer, which is accessed by Ext. In exciters.
    juce::AudioBuffer<float> inputBuffer;
    //every sample exciter is polyphonic, they all share the same sample buffer.
    Sampler sampler;

#if PERFETTO
    std::unique_ptr<perfetto::TracingSession> tracingSession;
#endif

    template <bool unity = true>
    static void testFilter(float cutoff, float mode, float Q, bool readout, bool peak = false)
    {
        int order = 14;
        juce::dsp::FFT fft(order);
        int fftSize = std::pow(2, order);
        juce::AudioBuffer<float> impulseResponse;
        juce::AudioBuffer<float> spectrum;
        chowdsp::SVFMultiMode<float, 1, unity> svf;
        if(peak)
        {
            svf.setPeakFrequency(cutoff);
        }
        else
        {
            svf.setCutoffFrequency (cutoff);
        }
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

        DBG("Testing unity gain SVF with frequency " + juce::String(cutoff) + " and mode " + juce::String(mode) + " and Q " + juce::String(Q));
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

        //compute the frequency for which the maximum gain occured
        float max = 0;
        int maxIndex = 0;
        for (int i = 0; i < fftSize / 2; i++)
        {
            if (impulseResponse.getSample(0, i) > max)
            {
                max = impulseResponse.getSample(0, i);
                maxIndex = i;
            }
        }

        //convert maximum index to frequency
        float maxFreq = maxIndex * 44100.0f / fftSize;
        DBG("Max gain frequency: " + juce::String(maxFreq));

        DBG("Testing phase response at 50hz, 500hz, 1000hz, and 5000h for filter with mode " + juce::String(mode) + " and Q " + juce::String(Q));
        DBG("50hz: " + juce::String(svf.getPhaseDelayInSamples(50)) + " group: " + juce::String(svf.getGroupDelayInSamples(50)));
        DBG("500hz: " + juce::String(svf.getPhaseDelayInSamples(500)) + " group: " + juce::String(svf.getGroupDelayInSamples(500)));
        DBG("1000hz: " + juce::String(svf.getPhaseDelayInSamples(1000)) + " group: " + juce::String(svf.getGroupDelayInSamples(1000)));
        DBG("5000hz: " + juce::String(svf.getPhaseDelayInSamples(5000)) + " group: " + juce::String(svf.getGroupDelayInSamples(5000)));
        DBG("10000hz: " + juce::String(svf.getPhaseDelayInSamples(10000)) + " group: " + juce::String(svf.getGroupDelayInSamples(10000)));
        DBG("20000hz: " + juce::String(svf.getPhaseDelayInSamples(20000)) + " group: " + juce::String(svf.getGroupDelayInSamples(20000)));
        DBG("Cutoff: " + juce::String(svf.getPhaseDelayInSamples(cutoff)) + " group: " + juce::String(svf.getGroupDelayInSamples(cutoff)));
        DBG("Max gain: " + juce::String(svf.getPhaseDelayInSamples(maxFreq)) + " group: " + juce::String(svf.getGroupDelayInSamples(maxFreq)));
        float special = 4000;
        DBG("Special: " + juce::String(svf.getPhaseDelayInSamples(special)) + " group: " + juce::String(svf.getGroupDelayInSamples(special)));
        juce::String data;
        for(int i = 1; i < 20000; i+= 100)
        {
            data += juce::String(svf.getPhaseDelayInSamples(i)) + " ";
        }
        DBG(data);

        juce::String data2;
        for(int i = 0; i < 20000; i+= 100)
        {
            data2 += juce::String(svf.getPhaseResponse(i)) + " ";
        }

        DBG(data2);

        juce::String data3;
        for(int i = 0; i < 20000; i+= 100)
        {
            data3 += juce::String(svf.getGroupDelayInSamples(i)) + " ";
        }

        DBG(data3);

    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ResonariumProcessor)
};
