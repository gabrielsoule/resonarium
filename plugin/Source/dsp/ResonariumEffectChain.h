#ifndef RESONARIUMEFFECTCHAIN_H
#define RESONARIUMEFFECTCHAIN_H

#include "Distortion.h"
#include "MultiAmp.h"
#include "MultiDelay.h"
#include "MVerb.h"
#include "WrappedSVF.h"

#include <array>
#include <cstdint>

class ResonariumEffectChain
{
public:
   #if defined (RESONARIUM_TEST_INSTRUMENTATION)
    struct TestCallCounters
    {
        std::uint64_t resets = 0;
        std::uint64_t parameterUpdates = 0;
        std::uint64_t processCalls = 0;

        TestCallCounters& operator+= (const TestCallCounters& other) noexcept
        {
            resets += other.resets;
            parameterUpdates += other.parameterUpdates;
            processCalls += other.processCalls;
            return *this;
        }
    };
   #endif

    explicit ResonariumEffectChain(EffectChainParams params);

    void prepare(const juce::dsp::ProcessSpec& spec);

    void reset();

    template <typename T>
    void updateParameters(T& source, juce::AudioPlayHead* playhead);

    void process(juce::dsp::AudioBlock<float> block) noexcept;

   #if defined (RESONARIUM_TEST_INSTRUMENTATION)
    [[nodiscard]] TestCallCounters getTestCallCounters() const noexcept { return testCallCounters; }
    void clearTestCallCounters() noexcept { testCallCounters = {}; }
   #endif

    juce::dsp::Chorus<float> chorus;
    ChorusParams chorusParams;

    MultiDelay delay;
    DelayParams delayParams;

    Distortion distortion;
    DistortionParams distortionParams;

    MultiAmp multiAmp;
    MultiAmpParams multiAmpParams;

    juce::dsp::Phaser<float> phaser;
    PhaserParams phaserParams;

    juce::dsp::Compressor<float> compressor;
    CompressorParams compressorParams;

    MVerb<float> mverb;
    ReverbParams reverbParams;

    WrappedSVF filter1;
    WrappedSVF filter2;
    SVFParams filter1Params;
    SVFParams filter2Params;

    double sampleRate = -1;

private:
    struct EnabledEffects
    {
        bool chorus = false;
        bool delay = false;
        bool distortion = false;
        bool multiAmp = false;
        bool phaser = false;
        bool compressor = false;
        bool reverb = false;
        bool filter1 = false;
        bool filter2 = false;

        [[nodiscard]] bool any() const noexcept
        {
            return chorus || delay || distortion || multiAmp || phaser || compressor
                   || reverb || filter1 || filter2;
        }
    };

    [[nodiscard]] EnabledEffects readEnabledEffects() const noexcept;
    void resetNewlyEnabled(const EnabledEffects& nextState);
    void invalidateControlCaches() noexcept;

    struct CachedFloat
    {
        [[nodiscard]] bool update(float next) noexcept
        {
            if (valid && next == value)
                return false;
            value = next;
            valid = true;
            return true;
        }

        void invalidate() noexcept { valid = false; }

        float value = 0.0f;
        bool valid = false;
    };

    struct CachedBool
    {
        [[nodiscard]] bool update(bool next) noexcept
        {
            if (valid && next == value)
                return false;
            value = next;
            valid = true;
            return true;
        }

        void invalidate() noexcept { valid = false; }

        bool value = false;
        bool valid = false;
    };

    EnabledEffects enabledEffects;
    std::array<CachedFloat, 5> chorusControls;
    std::array<CachedFloat, 5> phaserControls;
    std::array<CachedFloat, 8> reverbControls;
    std::array<CachedFloat, 6> delayControls;
    CachedBool delayPingPong;
    std::array<CachedFloat, 4> compressorControls;

   #if defined (RESONARIUM_TEST_INSTRUMENTATION)
    TestCallCounters testCallCounters;
   #endif
};

#endif //RESONARIUMEFFECTCHAIN_H
