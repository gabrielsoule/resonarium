#ifndef WAVEGUIDERESONATORBANK_H
#define WAVEGUIDERESONATORBANK_H
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "defines.h"
#include "GlobalState.h"
#include "Parameters.h"
#include "dsp/WaveguideKernel.h"

class ResonatorVoice;

/**
 * Transient bank-routing storage shared by the audio-thread-owned voices.
 * Voices render serially, so one arena is sufficient for every bank and voice.
 */
struct WaveguideScratchArena
{
    enum class Region : std::uint8_t
    {
        bankInput,
        previousBank,
        outputBank,
    };

    static constexpr int channelCount =
        resonarium::waveguide::WaveguideControlFrame::channelCount;
    static constexpr int regionCount = 3;
    static constexpr std::size_t storageAlignment =
        resonarium::waveguide::WaveguideDelay::storageAlignment;

    void prepare(const juce::dsp::ProcessSpec& spec);

    [[nodiscard]] bool isPrepared() const noexcept { return prepared; }

    /**
     * Returns a block beginning at the aligned start of a reusable region.
     * Voices render serially, so MIDI-delimited subblocks always reuse offset
     * zero rather than weakening alignment with the original host offset.
    */
    [[nodiscard]] juce::dsp::AudioBlock<float> getBlock(Region region, int numSamples) noexcept;

    // Kernels receive this already-combined/aligned input region; they do not
    // allocate private per-bank scratch buffers.
private:
    using SampleStorage = std::vector<
        float,
        resonarium::waveguide::WaveguideAlignedAllocator<float, storageAlignment>>;

    [[nodiscard]] static constexpr std::size_t regionIndex(Region region) noexcept
    {
        return static_cast<std::size_t>(region);
    }

    [[nodiscard]] bool isStorageAligned() const noexcept;

    SampleStorage storage;
    std::array<std::array<float*, channelCount>, regionCount> regionChannels {};
    int maximumBlockSamples = 0;
    int channelStrideSamples = 0;
    bool prepared = false;
};

/**
 * A bank of several Resonators, with support for different intra-resonator feedback modes.
 */
class WaveguideResonatorBank
{
public:
    enum CouplingMode
    {
        PARALLEL,
        INTERLINKED,
        CASCADE,
    };

    WaveguideResonatorBank(GlobalState& state, ResonatorVoice& parentVoice,
                           WaveguideResonatorBankParams params);
    /** The output never aliases the previous-bank input. */
    void processRouted(juce::dsp::AudioBlock<float>& exciterBlock,
                       juce::dsp::AudioBlock<float>& previousResonatorBankBlock,
                       juce::dsp::AudioBlock<float>& outputBlock);
    void reset();
    void prepare(const juce::dsp::ProcessSpec& spec);
    void updateParameters(float newFrequency, int numSamples);

    /** True when the committed controls cannot produce a bank output. */
    [[nodiscard]] bool isEmptyForRouting() const noexcept;

    GlobalState& state;
    ResonatorVoice& voice;
    WaveguideResonatorBankParams params;
    int index = -1;
    CouplingMode couplingMode = PARALLEL;
    float inputMix = 0.0f;
    float inputGain = 1.0f;
    float outputGain = 1.0f;

private:
    void updateControlFrame(float newFrequency, int numSamples) noexcept;
    void processKernel(juce::dsp::AudioBlock<float>& exciterBlock,
                       juce::dsp::AudioBlock<float>& previousResonatorBankBlock,
                       juce::dsp::AudioBlock<float>& outputBlock) noexcept;

    resonarium::waveguide::WaveguideKernel kernel;
    resonarium::waveguide::WaveguideControlFrame controlFrame;

    struct PitchRatioCache
    {
        float loopSemitones = 0.0f;
        float loopRatio = 1.0f;
        float postSemitones = 0.0f;
        float postRatio = 1.0f;
        bool loopInitialised = false;
        bool postInitialised = false;
    };

    std::array<PitchRatioCache,
               resonarium::waveguide::WaveguideControlFrame::laneCount>
        pitchRatioCache {};
};

#endif //WAVEGUIDERESONATORBANK_H
