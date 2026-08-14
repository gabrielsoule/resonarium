#pragma once

#include "ResonatorSynth.h"
#include "Parameters.h"
#include <melatonin_perfetto/melatonin_perfetto.h>
#include "GlobalState.h"
#include <gin/gin.h>
#include <gin_plugin/gin_plugin.h>

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
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;
    bool supportsMPE() const override { return true; }
    void setupModMatrix();
    static gin::ProcessorOptions getOptions();
    /**
     * Select the product's physical buses before AudioProcessor construction.
     *
     * JUCE's LV2 client advertises disabled buses as connection-optional ports,
     * but JUCE 8.0.8 does not accept a null buffer for such a port at run time.
     * The Instrument LV2 therefore has no audio-input bus. Other Instrument
     * formats retain the optional stereo external-exciter input, while the
     * Effect always has a required stereo input.
     */
    static juce::AudioProcessor::BusesProperties getBusesPropertiesForCurrentWrapper();
    
    // Get the synthesizer instance (needed for Python bindings)
    ResonatorSynth* getSynth() { return &synth; }
    const ResonatorSynth* getSynth() const { return &synth; }

    void acquireScopeConsumer() noexcept
    {
        scopeConsumerCount.fetch_add(1, std::memory_order_acq_rel);
    }

    void releaseScopeConsumer() noexcept
    {
        const auto previous = scopeConsumerCount.fetch_sub(1, std::memory_order_acq_rel);
        jassert(previous != 0);
        if (previous == 0)
            scopeConsumerCount.store(0, std::memory_order_release);
    }

    // Must outlive every DSP object that stores a reference to shared state.
    GlobalState globalState;
    ResonatorSynth synth;
    UIParams uiParams;
    gin::AudioFifo scopeFifo { 2, 44100 };
    std::atomic<unsigned int> scopeConsumerCount { 0 };
    int pluginInstanceID = -1;
    bool prepared = false; //indicates that prepareToPlay has been called at least once
    bool soloActive = false; //true if a resonator is in solo mode

    bool simulateConstantNote = false;
    bool constantNoteActive = false;
    bool constantNoteSuppressed = false;
    const int constantNoteNumber = 48 + 5;
    const float constantNoteVelocity = 0.5f;
    const int constantNoteChannel = 15;
    float constantNoteFrequency = 440.0f;

#if PERFETTO
    std::unique_ptr<perfetto::TracingSession> tracingSession;
#endif

    static bool checkBufferForNaN(juce::dsp::AudioBlock<float> block) noexcept
    {
        for (size_t channelIdx = 0; channelIdx < block.getNumChannels(); channelIdx++)
        {
            for (size_t sampleIdx = 0; sampleIdx < block.getNumSamples(); sampleIdx++)
            {
                if (std::isnan(block.getSample(channelIdx, sampleIdx)))
                {
                    return true;
                }
            }
        }

        return false;
    }
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ResonariumProcessor)
};
