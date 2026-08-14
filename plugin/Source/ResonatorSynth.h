//
// Created by Gabriel Soule on 5/1/24.
//

#ifndef RESONATORSYNTH_H
#define RESONATORSYNTH_H

#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>
#include <utility>
#include "defines.h"

#include "GlobalState.h"
#include "ExternalInputFrame.h"
#include "Exciters.h"
#include "Parameters.h"
#include "WaveguideResonatorBank.h"
#include "dsp/ResonariumEffectChain.h"
#include "util/RandomLFO.h"
#include "util/StereoLFOWrapper.h"
#include "util/StereoMSEGWrapper.h"

class ResonatorSynth : public gin::Synthesiser
{
public:
    struct VisualSnapshot
    {
        struct Voice
        {
            bool active = false;
            std::array<int, NUM_ENVELOPES> envelopeStages {};
            std::array<float, NUM_ENVELOPES> envelopePhases {};
            std::array<float, NUM_LFOS> lfoPhases {};
            std::array<float, NUM_MSEGS> msegPhases {};
        };

        [[nodiscard]] size_t copyEnvelopePhases(
            int envelopeIndex, std::span<std::pair<int, float>> destination) const noexcept;
        /** Includes the monophonic phase first, followed by active polyphonic voices. */
        [[nodiscard]] size_t copyLFOPhases(int lfoIndex, std::span<float> destination) const noexcept;
        /** Includes the monophonic phase first, followed by active polyphonic voices. */
        [[nodiscard]] size_t copyMSEGPhases(int msegIndex, std::span<float> destination) const noexcept;
        /** Returns the monophonic random source's visual value in the [0, 1] range. */
        [[nodiscard]] float getMonoRandomValue(int randomIndex) const noexcept;

        [[nodiscard]] const gin::VoiceTelemetry& getVoiceTelemetry() const noexcept
        {
            return voiceTelemetry;
        }

        [[nodiscard]] int getCpuUsage() const noexcept { return cpuUsage; }

    private:
        friend ResonatorSynth;

        std::array<float, NUM_LFOS> monoLFOPhases {};
        std::array<float, NUM_MSEGS> monoMSEGPhases {};
        std::array<float, NUM_RANDOMS> monoRandomValues {};
        std::array<Voice, NUM_PHYSICAL_VOICES> voices {};
        gin::VoiceTelemetry voiceTelemetry {};
        int cpuUsage = 0;
    };
    static_assert(std::is_trivially_copyable_v<VisualSnapshot>);

   #if defined (RESONARIUM_TEST_INSTRUMENTATION)
    struct EffectChainTopology
    {
        int globalInstances = 1;
        int voiceInstances = 0;
        int totalInstances = 1;
    };

   #endif

    ResonatorSynth(GlobalState& state, SynthParams params);
    void prepare(const juce::dsp::ProcessSpec& spec);
    [[nodiscard]] float beginExternalInputBlock(const float* left, const float* right,
                                                int numSamples, bool enabled,
                                                float rawMix) noexcept
    {
        return externalInputFrame.beginBlock(left, right, numSamples, enabled, rawMix);
    }
    [[nodiscard]] const ExternalInputFrame& getExternalInputFrame() const noexcept
    {
        return externalInputFrame;
    }
    [[nodiscard]] ExternalInputFrame& getExternalInputFrameForTesting() noexcept
    {
        return externalInputFrame;
    }
    bool beginHostBlock(int numSamples);
    void endBlock(int blockSize);
    void updateParameters();
    void renderNextSubBlock(juce::AudioBuffer<float>& outputAudio, int startSample, int numSamples) override;
    void handleController(int channel, int controller, int value) override;
    void panic();
    bool consumePanicRequest() noexcept { return panicRequested.exchange(false, std::memory_order_acq_rel); }
    void requestReset() noexcept { resetRequested.store(true, std::memory_order_release); }
    bool consumeResetRequest() noexcept { return resetRequested.exchange(false, std::memory_order_acq_rel); }

    [[nodiscard]] WaveguideScratchArena& getWaveguideScratchArena() noexcept { return waveguideScratchArena; }
    [[nodiscard]] const WaveguideScratchArena& getWaveguideScratchArena() const noexcept
    {
        return waveguideScratchArena;
    }

    /**
     * Enables block-boundary publication while at least one editor consumes
     * visual data. Calls are paired by editor lifetime and may come from the
     * message thread while audio is running.
     */
    void acquireVisualConsumer() noexcept;
    void releaseVisualConsumer() noexcept;

    /**
     * Makes one bounded attempt to copy the latest coherent frame. On
     * contention, returns false and leaves destination unchanged.
     */
    [[nodiscard]] bool copyVisualSnapshot(VisualSnapshot& destination) const noexcept;

    [[nodiscard]] static constexpr size_t getMaxEnvelopeVisualPhases() noexcept
    {
        return NUM_PHYSICAL_VOICES;
    }

    [[nodiscard]] static constexpr size_t getMaxLFOVisualPhases() noexcept
    {
        return NUM_PHYSICAL_VOICES + 1;
    }

    [[nodiscard]] static constexpr size_t getMaxMSEGVisualPhases() noexcept
    {
        return NUM_PHYSICAL_VOICES + 1;
    }

   #if defined (RESONARIUM_TEST_INSTRUMENTATION)
    [[nodiscard]] ResonariumEffectChain::TestCallCounters getGlobalEffectTestCallCounters() const noexcept;
    void clearGlobalEffectTestCallCounters() noexcept;
    [[nodiscard]] static constexpr EffectChainTopology getEffectChainTopology() noexcept { return {}; }
   #endif

    GlobalState& state;
    SynthParams params;
    ResonariumEffectChain effectChain;
    ExternalInputFrame externalInputFrame;
    WaveguideScratchArena waveguideScratchArena;
    StereoLFOWrapper monoLFOs[NUM_LFOS];
    RandomLFO monoRandomLFOs[NUM_RANDOMS];
    juce::Array<StereoMSEGWrapper> monoMSEGs;
    juce::Array<gin::MSEG::Data> msegData;

    int currentBlockSize = -1;

private:
    [[nodiscard]] bool publishVisualSnapshot() noexcept;

    static_assert(std::atomic<int>::is_always_lock_free);
    static_assert(std::atomic<unsigned int>::is_always_lock_free);
    static_assert(std::atomic<bool>::is_always_lock_free);
    static constexpr int visualSnapshotSlotFree = 0;
    static constexpr int visualSnapshotSlotReading = 1;
    static constexpr int visualSnapshotSlotWriting = -1;
    static constexpr size_t visualSnapshotSlotCount = 3;
    static constexpr int visualSnapshotPublicationRateHz = 30;
    std::array<VisualSnapshot, visualSnapshotSlotCount> visualSnapshots {};
    mutable std::array<std::atomic<int>, visualSnapshotSlotCount> visualSnapshotUseStates {};
    std::atomic<int> publishedVisualSnapshotSlot { 0 };
    std::atomic<unsigned int> visualConsumerCount { 0 };
    std::atomic<bool> visualPublicationRequested { false };
    std::uint64_t visualPublicationIntervalSamples = 1;
    std::uint64_t visualSamplesSincePublication = 0;
    std::atomic<bool> panicRequested { false };
    std::atomic<bool> resetRequested { false };
};


#endif //RESONATORSYNTH_H
