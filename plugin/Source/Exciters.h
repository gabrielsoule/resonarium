#ifndef EXCITERS_H
#define EXCITERS_H

#pragma once

#include <array>
#include <bit>
#include <cstdint>
#include <span>
#include <type_traits>

#include "ExternalInputFrame.h"
#include "GlobalState.h"
#include "Parameters.h"

enum class ExciterKind : std::uint8_t
{
    impulse,
    noise,
    sequence,
    sample,
    externalInput,
    count,
};

/** Original-host-input coordinates supplied for one MIDI-delimited render. */
struct ExciterRenderContext
{
    const ExternalInputFrame* externalInput = nullptr;
    int sourceStartSample = 0;
};

/** Audio-owned diagnostics. A containing synth may publish an immutable copy. */
struct ExciterTelemetry
{
    std::uint8_t enabledMask = 0;
    std::uint8_t initializedMask = 0;
    std::uint8_t pendingResetMask = 0;
    std::array<std::uint64_t, static_cast<std::size_t>(ExciterKind::count)> sourceSamplesGenerated {};
    std::uint64_t filterCoefficientRebuilds = 0;
    std::uint64_t externalVoiceReads = 0;
    std::uint64_t assignments = 0;
    std::uint64_t snapshotAcquires = 0;
    std::uint64_t snapshotReleases = 0;
};

struct ExciterAdsrControl
{
    float attack = 0.0f;
    float decay = 0.0f;
    float sustain = 0.0f;
    float release = 0.0f;
};

struct ExciterFilterControl
{
    std::uint8_t type = 0;
    float frequency = 2000.0f;
    float resonance = 0.707f;
    bool normalization = false;
};

/**
 * Fixed controls committed once for one MIDI-delimited render quantum.
 *
 * Parameter/modulation reads populate this POD before processing begins. The
 * sample kernels consume only these immutable scalars and the source state;
 * they never reach back into the parameter graph from their inner loops.
 */
struct ExciterControlFrame
{
    std::uint8_t enabledMask = 0;

    int impulseThickness = 1;
    float impulseLevel = 0.0f;
    ExciterFilterControl impulseFilter;

    float noiseLevelL = 0.0f;
    float noiseLevelR = 0.0f;
    ExciterAdsrControl noiseEnvelope;
    ExciterFilterControl noiseFilter;

    float sequenceLevel = 0.0f;
    std::uint8_t sequenceMode = 0;
    int sequencePeriodInSamples = 1;
    float sequenceStaticProbability = 0.0f;
    int sequenceImpulseLength = 1;
    ExciterAdsrControl sequenceEnvelope;
    ExciterFilterControl sequenceFilter;

    float sampleMixL = 1.0f;
    float sampleMixR = 1.0f;
    float sampleLevel = 1.0f;
    bool sampleLoop = false;
    ExciterAdsrControl sampleEnvelope;
    ExciterFilterControl sampleFilter;

    float externalGainL = 0.0f;
    float externalGainR = 0.0f;
    ExciterAdsrControl externalEnvelope;
    // The external-input filter controls are state-schema no-ops.
    ExciterFilterControl externalFilter;
};

static_assert(std::is_trivially_copyable_v<ExciterControlFrame>);

/**
 * Fixed per-voice exciter rack.
 *
 * Five concrete sources run in a fixed order and accumulate directly into the
 * voice's private excitation/direct buses.
 */
class VoiceExciterRack final
{
public:
    VoiceExciterRack(GlobalState& state, gin::ModVoice& voice, const VoiceParams& params);
    ~VoiceExciterRack();

    VoiceExciterRack(const VoiceExciterRack&) = delete;
    VoiceExciterRack& operator=(const VoiceExciterRack&) = delete;

    void prepare(const juce::dsp::ProcessSpec& spec);

    /** A full physical-voice assignment. Mono legato retargets must not call this. */
    void beginAssignment(int physicalVoiceId, std::uint64_t assignmentGeneration) noexcept;
    void noteStopped(bool allowTailOff) noexcept;
    void voiceRetired() noexcept;

    /** Resolve the five enabled controls and only the controls of active sources. */
    void updateParameters() noexcept;

    /** Add all enabled sources in their fixed processing order. */
    void process(juce::dsp::AudioBlock<float>& excitation,
                 juce::dsp::AudioBlock<float>& directOutput,
                 const ExciterRenderContext& context) noexcept;

    [[nodiscard]] const ExciterTelemetry& getTelemetry() const noexcept { return telemetry; }
    void clearTelemetry() noexcept;

#if defined(RESONARIUM_TEST_INSTRUMENTATION)
    static void renderPrngSequenceForTesting(int physicalVoiceId,
                                             std::uint64_t assignmentGeneration,
                                             ExciterKind kind,
                                             std::uint32_t substream,
                                             std::span<float> destination) noexcept;
#endif

private:
    static constexpr std::uint8_t allSourcesMask =
        (std::uint8_t { 1 } << static_cast<unsigned>(ExciterKind::count)) - 1u;

    [[nodiscard]] static constexpr std::uint8_t sourceBit(ExciterKind kind) noexcept
    {
        return static_cast<std::uint8_t>(1u << static_cast<unsigned>(kind));
    }

    class Xoshiro128PlusPlus final
    {
    public:
        void seed(std::uint64_t projectSeed, int voiceIndex,
                  std::uint64_t generation, ExciterKind kind, std::uint32_t substream) noexcept;
        [[nodiscard]] std::uint32_t nextUInt() noexcept;
        [[nodiscard]] float nextUnitFloat() noexcept;
        [[nodiscard]] float nextBipolarFloat() noexcept { return nextUnitFloat() * 2.0f - 1.0f; }

    private:
        std::array<std::uint32_t, 4> words { 1u, 0u, 0u, 0u };
    };

    class CachedStereoFilter final
    {
    public:
        explicit CachedStereoFilter(bool normalize = false) : normalization(normalize) {}

        void prepare(const juce::dsp::ProcessSpec& spec);
        void reset() noexcept
        {
            filterL.reset();
            filterR.reset();
        }
        void update(const ExciterFilterControl& control,
                    ExciterTelemetry& counters) noexcept;
        void process(float inputL, float inputR, float& outputL, float& outputR) noexcept;
        [[nodiscard]] bool isBypassed() const noexcept { return type == Type::none; }

    private:
        enum class Type : std::uint8_t
        {
            none = 0,
            lowpass,
            highpass,
            bandpass,
            notch,
            allpass,
        };

        [[nodiscard]] static std::array<float, 6> makeHighGainBandpass(float sampleRate,
                                                                       float frequency,
                                                                       float q) noexcept;

        juce::dsp::IIR::Filter<float> filterL;
        juce::dsp::IIR::Filter<float> filterR;
        std::array<float, 6> coefficients {};
        Type type = Type::none;
        float frequency = 2000.0f;
        float resonance = 0.707f;
        float sampleRate = 44100.0f;
        bool normalization = false;
        bool hasTuple = false;
    };

    struct ImpulseState
    {
        ImpulseExciterParams params;
        CachedStereoFilter filter { false };
        int impulsesRemaining = 0;
    };

    struct NoiseState
    {
        NoiseExciterParams params;
        CachedStereoFilter filter { false };
        gin::AnalogADSR envelope;
        Xoshiro128PlusPlus random;
    };

    struct SequenceState
    {
        enum class Mode : std::uint8_t { impulse, staticNoise, triangular };

        SequenceExciterParams params;
        CachedStereoFilter filter { false };
        gin::AnalogADSR envelope;
        Xoshiro128PlusPlus triggerRandom;
        Xoshiro128PlusPlus valueRandom;
        Mode mode = Mode::impulse;
        int periodInSamples = 1;
        int samplesSinceLastImpulse = 0;
        int impulsesLeft = 0;
        float triPhase = 0.0f;
        float currentTriValue = 0.0f;
        float targetTriValue = 0.0f;
        float triPhaseInc = 0.0f;
    };

    struct SampleState
    {
        SampleExciterParams params;
        CachedStereoFilter filter { false };
        gin::AnalogADSR envelope;
        Sampler::AudioHandle snapshot;
        int startSample = 0;
        int endSample = 0;
        int currentSample = 0;
        bool isPlaying = false;
    };

    struct ExternalState
    {
        ExternalInputExciterParams params;
        gin::AnalogADSR envelope;
    };

    void initializePendingSource(ExciterKind kind) noexcept;
    void seedSource(ExciterKind kind) noexcept;
    void resetImpulse() noexcept;
    void resetNoise() noexcept;
    void resetSequence() noexcept;
    void resetSample() noexcept;
    void resetExternal() noexcept;

    void processImpulse(juce::dsp::AudioBlock<float>& excitation) noexcept;
    void processNoise(juce::dsp::AudioBlock<float>& excitation) noexcept;
    void processSequence(juce::dsp::AudioBlock<float>& excitation) noexcept;
    void processSample(juce::dsp::AudioBlock<float>& excitation,
                       juce::dsp::AudioBlock<float>& directOutput) noexcept;
    void processExternal(juce::dsp::AudioBlock<float>& excitation,
                         const ExciterRenderContext& context) noexcept;

    void processImpulseSequence(juce::dsp::AudioBlock<float>& excitation) noexcept;
    void processStaticSequence(juce::dsp::AudioBlock<float>& excitation) noexcept;
    void processTriangularSequence(juce::dsp::AudioBlock<float>& excitation) noexcept;

    [[nodiscard]] bool sourceEnabled(ExciterKind kind) const noexcept
    {
        return (enabledMask & sourceBit(kind)) != 0;
    }

    GlobalState& state;
    gin::ModVoice& voice;
    double sampleRate = 44100.0;
    int physicalVoiceId = 0;
    std::uint64_t assignmentGeneration = 0;
    std::uint8_t enabledMask = 0;
    std::uint8_t previousEnabledMask = 0;
    std::uint8_t initializedMask = 0;
    std::uint8_t pendingResetMask = allSourcesMask;
    bool assignmentActive = false;
    bool gateHeld = false;
    ExciterControlFrame controlFrame;

    ImpulseState impulse;
    NoiseState noise;
    SequenceState sequence;
    SampleState sample;
    ExternalState externalInput;
    ExciterTelemetry telemetry;

};

#endif // EXCITERS_H
