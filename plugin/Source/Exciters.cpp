#include "Exciters.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr std::uint64_t exciterProjectSeed = UINT64_C(0xd1b54a32d192ed03);
constexpr float minimumExciterFilterFrequency = 10.0f;
constexpr float minimumExciterFilterResonance = 0.01f;

[[nodiscard]] float sanitizeExciterFilterFrequency(float frequency,
                                                    float sampleRate) noexcept
{
    const auto nyquistSafeMaximum = std::max(1.0f, sampleRate * 0.5f - 1.0f);
    const auto minimumFrequency =
        std::min(minimumExciterFilterFrequency, nyquistSafeMaximum);
    if (!std::isfinite(frequency))
        return minimumFrequency;
    return std::clamp(frequency, minimumFrequency, nyquistSafeMaximum);
}

[[nodiscard]] float sanitizeExciterFilterResonance(float resonance) noexcept
{
    if (!std::isfinite(resonance))
        return minimumExciterFilterResonance;
    return std::max(resonance, minimumExciterFilterResonance);
}

[[nodiscard]] std::uint64_t splitMix64(std::uint64_t& state) noexcept
{
    std::uint64_t value = (state += UINT64_C(0x9e3779b97f4a7c15));
    value = (value ^ (value >> 30u)) * UINT64_C(0xbf58476d1ce4e5b9);
    value = (value ^ (value >> 27u)) * UINT64_C(0x94d049bb133111eb);
    return value ^ (value >> 31u);
}

[[nodiscard]] float safeRate(float rate) noexcept
{
    return std::isfinite(rate) && rate > 0.0f ? rate : 1.0f;
}

[[nodiscard]] ExciterAdsrControl resolveAdsrControl(gin::ModVoice& voice,
                                                    const ADSRParams& params) noexcept
{
    return {
        voice.getValue(params.attack),
        voice.getValue(params.decay),
        voice.getValue(params.sustain),
        voice.getValue(params.release),
    };
}

void applyAdsrControl(gin::AnalogADSR& envelope,
                      const ExciterAdsrControl& control) noexcept
{
    envelope.setAttack(control.attack);
    envelope.setDecay(control.decay);
    envelope.setSustainLevel(control.sustain);
    envelope.setRelease(control.release);
}

[[nodiscard]] ExciterFilterControl resolveFilterControl(
    gin::ModVoice& voice, const MultiFilterParams& params,
    bool normalization) noexcept
{
    ExciterFilterControl control;
    control.type = params.type != nullptr
                       ? static_cast<std::uint8_t>(static_cast<int>(params.type->getProcValue()))
                       : 0;
    control.frequency = params.frequency != nullptr
                            ? voice.getValue(params.frequency)
                            : 2000.0f;
    control.resonance = params.resonance != nullptr
                            ? voice.getValue(params.resonance)
                            : 0.707f;
    control.normalization = normalization;
    return control;
}
}


void VoiceExciterRack::Xoshiro128PlusPlus::seed(std::uint64_t projectSeed, int voiceIndex,
                                                std::uint64_t generation, ExciterKind kind,
                                                std::uint32_t substream) noexcept
{
    std::uint64_t seedState = projectSeed;
    seedState ^= static_cast<std::uint64_t>(static_cast<std::uint32_t>(voiceIndex))
                 * UINT64_C(0x9e3779b97f4a7c15);
    seedState ^= generation * UINT64_C(0xd2b74407b1ce6e93);
    seedState ^= static_cast<std::uint64_t>(static_cast<std::uint8_t>(kind) + 1u)
                 * UINT64_C(0xca5a826395121157);
    seedState ^= static_cast<std::uint64_t>(substream + 1u) * UINT64_C(0xa24baed4963ee407);

    for (auto& word : words)
        word = static_cast<std::uint32_t>(splitMix64(seedState));

    if ((words[0] | words[1] | words[2] | words[3]) == 0u)
        words[0] = 1u;
}

std::uint32_t VoiceExciterRack::Xoshiro128PlusPlus::nextUInt() noexcept
{
    const auto result = std::rotl(words[0] + words[3], 7) + words[0];
    const auto temporary = words[1] << 9u;

    words[2] ^= words[0];
    words[3] ^= words[1];
    words[1] ^= words[2];
    words[0] ^= words[3];
    words[2] ^= temporary;
    words[3] = std::rotl(words[3], 11);
    return result;
}

float VoiceExciterRack::Xoshiro128PlusPlus::nextUnitFloat() noexcept
{
    // The top 24 bits have an exactly specified, cross-platform float mapping.
    return static_cast<float>(nextUInt() >> 8u) * 0x1.0p-24f;
}

#if defined(RESONARIUM_TEST_INSTRUMENTATION)
void VoiceExciterRack::renderPrngSequenceForTesting(int voiceId, std::uint64_t generation,
                                                     ExciterKind kind, std::uint32_t substream,
                                                     std::span<float> destination) noexcept
{
    Xoshiro128PlusPlus generator;
    generator.seed(exciterProjectSeed, voiceId, generation, kind, substream);
    for (auto& value : destination)
        value = generator.nextUnitFloat();
}
#endif

void VoiceExciterRack::CachedStereoFilter::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = static_cast<float>(spec.sampleRate);
    frequency = sanitizeExciterFilterFrequency(frequency, sampleRate);
    resonance = sanitizeExciterFilterResonance(resonance);
    coefficients = juce::dsp::IIR::ArrayCoefficients<float>::makeLowPass(
        sampleRate, frequency, resonance);
    *filterL.coefficients = coefficients;
    *filterR.coefficients = coefficients;
    const juce::dsp::ProcessSpec monoSpec { spec.sampleRate, spec.maximumBlockSize, 1 };
    filterL.prepare(monoSpec);
    filterR.prepare(monoSpec);
    hasTuple = false;
}

std::array<float, 6> VoiceExciterRack::CachedStereoFilter::makeHighGainBandpass(
    float currentSampleRate, float currentFrequency, float q) noexcept
{
    const auto n = 1.0f / std::tan(juce::MathConstants<float>::pi * currentFrequency
                                  / currentSampleRate);
    const auto nSquared = n * n;
    const auto inverseQ = 1.0f / q;
    const auto coefficient = 1.0f / (1.0f + inverseQ * n + nSquared);
    return { coefficient * n, 0.0f, -coefficient * n, 1.0f,
             coefficient * 2.0f * (1.0f - nSquared),
             coefficient * (1.0f - inverseQ * n + nSquared) };
}

void VoiceExciterRack::CachedStereoFilter::update(const ExciterFilterControl& control,
                                                   ExciterTelemetry& counters) noexcept
{
    const auto nextType = static_cast<Type>(control.type);
    const auto nextFrequency = sanitizeExciterFilterFrequency(control.frequency, sampleRate);
    const auto nextResonance = sanitizeExciterFilterResonance(control.resonance);
    const auto nextNormalization = control.normalization;

    if (hasTuple && nextType == type && nextFrequency == frequency
        && nextResonance == resonance && nextNormalization == normalization)
        return;

    type = nextType;
    frequency = nextFrequency;
    resonance = nextResonance;
    normalization = nextNormalization;
    hasTuple = true;

    if (type == Type::none)
        return;

    switch (type)
    {
        case Type::lowpass:
            coefficients = juce::dsp::IIR::ArrayCoefficients<float>::makeLowPass(
                sampleRate, frequency, resonance);
            break;
        case Type::highpass:
            coefficients = juce::dsp::IIR::ArrayCoefficients<float>::makeHighPass(
                sampleRate, frequency, resonance);
            break;
        case Type::bandpass:
            coefficients = normalization
                               ? juce::dsp::IIR::ArrayCoefficients<float>::makeBandPass(
                                     sampleRate, frequency, resonance)
                               : makeHighGainBandpass(sampleRate, frequency, resonance);
            break;
        case Type::notch:
            coefficients = juce::dsp::IIR::ArrayCoefficients<float>::makeNotch(
                sampleRate, frequency, resonance);
            break;
        case Type::allpass:
            coefficients = juce::dsp::IIR::ArrayCoefficients<float>::makeAllPass(
                sampleRate, frequency, resonance);
            break;
        case Type::none: break;
    }

    *filterL.coefficients = coefficients;
    *filterR.coefficients = coefficients;
    ++counters.filterCoefficientRebuilds;
}

void VoiceExciterRack::CachedStereoFilter::process(float inputL, float inputR,
                                                    float& outputL, float& outputR) noexcept
{
    if (type == Type::none)
    {
        outputL = inputL;
        outputR = inputR;
        return;
    }

    outputL = filterL.processSample(inputL);
    outputR = filterR.processSample(inputR);
}

VoiceExciterRack::VoiceExciterRack(GlobalState& state, gin::ModVoice& voice,
                                   const VoiceParams& params)
    : state(state), voice(voice)
{
    static_assert(NUM_IMPULSE_EXCITERS == 1);
    static_assert(NUM_NOISE_EXCITERS == 1);
    static_assert(NUM_IMPULSE_TRAIN_EXCITERS == 1);

    impulse.params = params.impulseExciterParams[0];
    noise.params = params.noiseExciterParams[0];
    sequence.params = params.impulseTrainExciterParams[0];
    sample.params = params.sampleExciterParams;
    externalInput.params = params.externalInputExciterParams;
}

VoiceExciterRack::~VoiceExciterRack()
{
    if (sample.snapshot)
        state.sampler.releaseFromVoice(sample.snapshot);
}

void VoiceExciterRack::prepare(const juce::dsp::ProcessSpec& spec)
{
    jassert(spec.numChannels == 2);
    sampleRate = spec.sampleRate;
    impulse.filter.prepare(spec);
    noise.filter.prepare(spec);
    sequence.filter.prepare(spec);
    sample.filter.prepare(spec);

    // Exciter AnalogADSR timing is intentionally fixed at 44.1 kHz.
    impulse.filter.reset();
    noise.filter.reset();
    sequence.filter.reset();
    sample.filter.reset();
    noise.envelope.reset();
    sequence.envelope.reset();
    sample.envelope.reset();
    externalInput.envelope.reset();
}

void VoiceExciterRack::beginAssignment(int newPhysicalVoiceId,
                                       std::uint64_t newAssignmentGeneration) noexcept
{
    if (sample.snapshot)
    {
        state.sampler.releaseFromVoice(sample.snapshot);
        ++telemetry.snapshotReleases;
    }

    sample.snapshot = state.sampler.acquireForVoice();
    if (sample.snapshot)
        ++telemetry.snapshotAcquires;

    physicalVoiceId = newPhysicalVoiceId;
    assignmentGeneration = newAssignmentGeneration;
    assignmentActive = true;
    gateHeld = true;
    enabledMask = 0;
    controlFrame.enabledMask = 0;
    previousEnabledMask = 0;
    initializedMask = 0;
    pendingResetMask = allSourcesMask;
    ++telemetry.assignments;
}

void VoiceExciterRack::noteStopped(bool allowTailOff) noexcept
{
    juce::ignoreUnused(allowTailOff);
    gateHeld = false;

    if (sourceEnabled(ExciterKind::noise)
        && (initializedMask & sourceBit(ExciterKind::noise)) != 0)
        noise.envelope.noteOff();
    if (sourceEnabled(ExciterKind::sequence)
        && (initializedMask & sourceBit(ExciterKind::sequence)) != 0)
        sequence.envelope.noteOff();
    if (sourceEnabled(ExciterKind::sample)
        && (initializedMask & sourceBit(ExciterKind::sample)) != 0)
        sample.envelope.noteOff();
    if (sourceEnabled(ExciterKind::externalInput)
        && (initializedMask & sourceBit(ExciterKind::externalInput)) != 0)
        externalInput.envelope.noteOff();
}

void VoiceExciterRack::voiceRetired() noexcept
{
    if (sample.snapshot)
    {
        state.sampler.releaseFromVoice(sample.snapshot);
        ++telemetry.snapshotReleases;
    }
    sample.snapshot = {};
    sample.isPlaying = false;
    assignmentActive = false;
    gateHeld = false;
    enabledMask = 0;
    controlFrame.enabledMask = 0;
    previousEnabledMask = 0;
    initializedMask = 0;
    pendingResetMask = allSourcesMask;
    telemetry.enabledMask = enabledMask;
    telemetry.initializedMask = initializedMask;
    telemetry.pendingResetMask = pendingResetMask;
}

void VoiceExciterRack::clearTelemetry() noexcept
{
    telemetry = {};
    telemetry.enabledMask = enabledMask;
    telemetry.initializedMask = initializedMask;
    telemetry.pendingResetMask = pendingResetMask;
}

void VoiceExciterRack::updateParameters() noexcept
{
    if (!assignmentActive)
        return;

    std::uint8_t nextEnabledMask = 0;
    if (impulse.params.enabled->isOn())
        nextEnabledMask |= sourceBit(ExciterKind::impulse);
    if (noise.params.enabled->isOn())
        nextEnabledMask |= sourceBit(ExciterKind::noise);
    if (sequence.params.enabled->isOn())
        nextEnabledMask |= sourceBit(ExciterKind::sequence);
    if (sample.params.enabled->isOn())
        nextEnabledMask |= sourceBit(ExciterKind::sample);
    if (externalInput.params.enabled->isOn())
        nextEnabledMask |= sourceBit(ExciterKind::externalInput);
    enabledMask = nextEnabledMask;
    controlFrame.enabledMask = nextEnabledMask;

    if (sourceEnabled(ExciterKind::impulse))
    {
        controlFrame.impulseThickness = juce::jmax(
            1, static_cast<int>(voice.getValue(impulse.params.thickness)));
        controlFrame.impulseLevel = voice.getValue(impulse.params.level);
        controlFrame.impulseFilter = resolveFilterControl(
            voice, impulse.params.filterParams, false);
        impulse.filter.update(controlFrame.impulseFilter, telemetry);
        initializePendingSource(ExciterKind::impulse);
    }

    if (sourceEnabled(ExciterKind::noise))
    {
        controlFrame.noiseLevelL = voice.getValue(noise.params.level, 0);
        controlFrame.noiseLevelR = voice.getValue(noise.params.level, 1);
        controlFrame.noiseEnvelope = resolveAdsrControl(voice, noise.params.adsrParams);
        controlFrame.noiseFilter = resolveFilterControl(voice, noise.params.filterParams, false);
        applyAdsrControl(noise.envelope, controlFrame.noiseEnvelope);
        noise.filter.update(controlFrame.noiseFilter, telemetry);
        initializePendingSource(ExciterKind::noise);
    }

    if (sourceEnabled(ExciterKind::sequence))
    {
        const auto oldPeriod = sequence.periodInSamples;
        const auto rawMode = static_cast<int>(voice.getValue(sequence.params.mode));
        sequence.mode = rawMode == 1 ? SequenceState::Mode::staticNoise
                                    : (rawMode == 2 ? SequenceState::Mode::triangular
                                                    : SequenceState::Mode::impulse);
        controlFrame.sequenceLevel = voice.getValue(sequence.params.level);
        controlFrame.sequenceMode = static_cast<std::uint8_t>(sequence.mode);
        controlFrame.sequenceEnvelope = resolveAdsrControl(voice, sequence.params.adsrParams);
        controlFrame.sequenceFilter = resolveFilterControl(
            voice, sequence.params.filterParams, false);
        applyAdsrControl(sequence.envelope, controlFrame.sequenceEnvelope);
        sequence.filter.update(controlFrame.sequenceFilter, telemetry);

        const auto newPeriod = juce::jmax(1, static_cast<int>(std::round(
            sampleRate / static_cast<double>(safeRate(voice.getValue(sequence.params.rate))))));
        if (sequence.mode == SequenceState::Mode::impulse)
            controlFrame.sequenceImpulseLength = juce::jmax(
                1, static_cast<int>(voice.getValue(sequence.params.impulseLength)));
        else if (sequence.mode == SequenceState::Mode::staticNoise)
            controlFrame.sequenceStaticProbability =
                voice.getValue(sequence.params.staticDensity) * 0.5f;
        else
        {
            if (oldPeriod != newPeriod && oldPeriod > 0)
            {
                sequence.triPhase *= static_cast<float>(oldPeriod) / static_cast<float>(newPeriod);
                while (sequence.triPhase >= 1.0f)
                {
                    sequence.triPhase -= 1.0f;
                    sequence.currentTriValue = sequence.targetTriValue;
                    sequence.targetTriValue = sequence.valueRandom.nextUnitFloat();
                }
            }
            sequence.triPhaseInc = 1.0f / static_cast<float>(newPeriod);
        }
        sequence.periodInSamples = newPeriod;
        controlFrame.sequencePeriodInSamples = sequence.periodInSamples;
        initializePendingSource(ExciterKind::sequence);
    }

    if (sourceEnabled(ExciterKind::sample))
    {
        controlFrame.sampleMixL = voice.getValue(sample.params.mix, 0);
        controlFrame.sampleMixR = voice.getValue(sample.params.mix, 1);
        controlFrame.sampleLevel = voice.getValue(sample.params.level, 0);
        controlFrame.sampleLoop = sample.params.loop->isOn();
        controlFrame.sampleEnvelope = resolveAdsrControl(voice, sample.params.adsrParams);
        controlFrame.sampleFilter = resolveFilterControl(voice, sample.params.filterParams, false);
        applyAdsrControl(sample.envelope, controlFrame.sampleEnvelope);
        sample.filter.update(controlFrame.sampleFilter, telemetry);
        initializePendingSource(ExciterKind::sample);
    }

    if (sourceEnabled(ExciterKind::externalInput))
    {
        const auto wet = std::sin(voice.getValue(externalInput.params.mix)
                                  * juce::MathConstants<float>::halfPi);
        controlFrame.externalGainL = voice.getValue(externalInput.params.gain, 0) * wet;
        controlFrame.externalGainR = voice.getValue(externalInput.params.gain, 1) * wet;
        controlFrame.externalEnvelope = resolveAdsrControl(
            voice, externalInput.params.adsrParams);
        controlFrame.externalFilter = resolveFilterControl(
            voice, externalInput.params.filterParams, false);
        applyAdsrControl(externalInput.envelope, controlFrame.externalEnvelope);
        // The external filter controls intentionally remain compatibility no-ops.
        initializePendingSource(ExciterKind::externalInput);
    }

    const auto newlyEnabled = static_cast<std::uint8_t>(enabledMask & ~previousEnabledMask);
    if (!gateHeld)
    {
        if ((newlyEnabled & sourceBit(ExciterKind::noise)) != 0
            && (initializedMask & sourceBit(ExciterKind::noise)) != 0)
            noise.envelope.noteOff();
        if ((newlyEnabled & sourceBit(ExciterKind::sequence)) != 0
            && (initializedMask & sourceBit(ExciterKind::sequence)) != 0)
            sequence.envelope.noteOff();
        if ((newlyEnabled & sourceBit(ExciterKind::sample)) != 0
            && (initializedMask & sourceBit(ExciterKind::sample)) != 0)
            sample.envelope.noteOff();
        if ((newlyEnabled & sourceBit(ExciterKind::externalInput)) != 0
            && (initializedMask & sourceBit(ExciterKind::externalInput)) != 0)
            externalInput.envelope.noteOff();
    }

    previousEnabledMask = enabledMask;
    telemetry.enabledMask = enabledMask;
    telemetry.initializedMask = initializedMask;
    telemetry.pendingResetMask = pendingResetMask;
}

void VoiceExciterRack::initializePendingSource(ExciterKind kind) noexcept
{
    const auto bit = sourceBit(kind);
    if ((pendingResetMask & bit) == 0)
        return;

    seedSource(kind);
    switch (kind)
    {
        case ExciterKind::impulse: resetImpulse(); break;
        case ExciterKind::noise: resetNoise(); break;
        case ExciterKind::sequence: resetSequence(); break;
        case ExciterKind::sample: resetSample(); break;
        case ExciterKind::externalInput: resetExternal(); break;
        case ExciterKind::count: jassertfalse; break;
    }

    // A lazily initialized source must enter the assignment at its note-on
    // state. If the key was already released, transition immediately
    // to release rather than leaving the envelope idle. The impulse is the
    // intentional exception: noteStopped() never cancels it.
    if (!gateHeld)
    {
        if (kind == ExciterKind::noise)
            noise.envelope.noteOff();
        else if (kind == ExciterKind::sequence)
            sequence.envelope.noteOff();
        else if (kind == ExciterKind::sample)
            sample.envelope.noteOff();
        else if (kind == ExciterKind::externalInput)
            externalInput.envelope.noteOff();
    }

    pendingResetMask &= static_cast<std::uint8_t>(~bit);
    initializedMask |= bit;
}

void VoiceExciterRack::seedSource(ExciterKind kind) noexcept
{
    if (kind == ExciterKind::noise)
        noise.random.seed(exciterProjectSeed, physicalVoiceId, assignmentGeneration, kind, 0);
    else if (kind == ExciterKind::sequence)
    {
        sequence.triggerRandom.seed(exciterProjectSeed, physicalVoiceId,
                                    assignmentGeneration, kind, 0);
        sequence.valueRandom.seed(exciterProjectSeed, physicalVoiceId,
                                  assignmentGeneration, kind, 1);
    }
}

void VoiceExciterRack::resetImpulse() noexcept
{
    impulse.filter.reset();
    impulse.impulsesRemaining = controlFrame.impulseThickness;
}

void VoiceExciterRack::resetNoise() noexcept
{
    noise.envelope.reset();
    noise.filter.reset();
    noise.envelope.noteOn();
}

void VoiceExciterRack::resetSequence() noexcept
{
    sequence.envelope.reset();
    sequence.filter.reset();
    sequence.samplesSinceLastImpulse = 0;
    sequence.impulsesLeft = 0;
    sequence.currentTriValue = 0.0f;
    sequence.targetTriValue = 0.0f;
    sequence.triPhase = 0.0f;
    sequence.envelope.noteOn();
}

void VoiceExciterRack::resetSample() noexcept
{
    sample.envelope.reset();
    sample.filter.reset();
    const auto sampleLength = sample.snapshot.getNumSamples();
    sample.startSample = juce::jlimit(0, sampleLength,
                                      static_cast<int>(sampleLength * voice.getValue(sample.params.start)));
    sample.endSample = juce::jlimit(0, sampleLength,
                                    static_cast<int>(sampleLength * voice.getValue(sample.params.end)));
    // Playback begins at frame zero; startSample is consulted only after
    // reaching endSample and looping.
    sample.currentSample = 0;
    sample.isPlaying = sample.snapshot && sample.endSample > sample.startSample;
    if (sample.isPlaying)
        sample.envelope.noteOn();
}

void VoiceExciterRack::resetExternal() noexcept
{
    externalInput.envelope.reset();
    externalInput.envelope.noteOn();
}

void VoiceExciterRack::process(juce::dsp::AudioBlock<float>& excitation,
                               juce::dsp::AudioBlock<float>& directOutput,
                               const ExciterRenderContext& context) noexcept
{
    jassert(excitation.getNumChannels() == 2 && directOutput.getNumChannels() == 2);
    jassert(excitation.getNumSamples() == directOutput.getNumSamples());
    const auto frameMask = controlFrame.enabledMask;
    if ((frameMask & sourceBit(ExciterKind::impulse)) != 0)
        processImpulse(excitation);
    if ((frameMask & sourceBit(ExciterKind::noise)) != 0)
        processNoise(excitation);
    if ((frameMask & sourceBit(ExciterKind::sequence)) != 0)
        processSequence(excitation);
    if ((frameMask & sourceBit(ExciterKind::sample)) != 0)
        processSample(excitation, directOutput);
    if ((frameMask & sourceBit(ExciterKind::externalInput)) != 0)
        processExternal(excitation, context);
}

void VoiceExciterRack::processImpulse(juce::dsp::AudioBlock<float>& excitation) noexcept
{
    auto* left = excitation.getChannelPointer(0);
    auto* right = excitation.getChannelPointer(1);
    const auto count = static_cast<int>(excitation.getNumSamples());
    const auto adjustedGain = controlFrame.impulseLevel
                              / static_cast<float>(controlFrame.impulseThickness);
    if (impulse.filter.isBypassed())
    {
        const auto impulseCount = std::min(count, impulse.impulsesRemaining);
        if (impulseCount > 0)
        {
            juce::FloatVectorOperations::add(left, -adjustedGain, impulseCount);
            juce::FloatVectorOperations::add(right, -adjustedGain, impulseCount);
            impulse.impulsesRemaining -= impulseCount;
        }
        telemetry.sourceSamplesGenerated[static_cast<std::size_t>(ExciterKind::impulse)] +=
            static_cast<std::uint64_t>(count);
        return;
    }

    for (int index = 0; index < count; ++index)
    {
        const auto mono = impulse.impulsesRemaining > 0 ? -adjustedGain : 0.0f;
        impulse.impulsesRemaining -= impulse.impulsesRemaining > 0 ? 1 : 0;
        float filteredL = 0.0f;
        float filteredR = 0.0f;
        impulse.filter.process(mono, mono, filteredL, filteredR);
        left[index] += filteredL;
        right[index] += filteredR;
    }
    telemetry.sourceSamplesGenerated[static_cast<std::size_t>(ExciterKind::impulse)] +=
        static_cast<std::uint64_t>(count);
}

void VoiceExciterRack::processNoise(juce::dsp::AudioBlock<float>& excitation) noexcept
{
    auto* left = excitation.getChannelPointer(0);
    auto* right = excitation.getChannelPointer(1);
    const auto count = static_cast<int>(excitation.getNumSamples());
    const auto envelopeState = noise.envelope.getState();
    const auto constantEnvelope = envelopeState == gin::AnalogADSR::State::sustain
                                  || envelopeState == gin::AnalogADSR::State::idle;
    const auto constantEnvelopeValue = noise.envelope.getOutput();
    const auto bypassed = noise.filter.isBypassed();
    for (int index = 0; index < count; ++index)
    {
        const auto envelopeValue = constantEnvelope ? constantEnvelopeValue
                                                    : noise.envelope.process();
        const auto randomSample = noise.random.nextBipolarFloat();
        const auto mono = randomSample * 0.1f;
        const auto sampleL = mono * controlFrame.noiseLevelL * envelopeValue;
        const auto sampleR = mono * controlFrame.noiseLevelR * envelopeValue;
        if (bypassed)
        {
            left[index] += sampleL;
            right[index] += sampleR;
        }
        else
        {
            float filteredL = 0.0f;
            float filteredR = 0.0f;
            noise.filter.process(sampleL, sampleR, filteredL, filteredR);
            left[index] += filteredL;
            right[index] += filteredR;
        }
    }
    telemetry.sourceSamplesGenerated[static_cast<std::size_t>(ExciterKind::noise)] +=
        static_cast<std::uint64_t>(count);
}

void VoiceExciterRack::processSequence(juce::dsp::AudioBlock<float>& excitation) noexcept
{
    switch (static_cast<SequenceState::Mode>(controlFrame.sequenceMode))
    {
        case SequenceState::Mode::impulse: processImpulseSequence(excitation); break;
        case SequenceState::Mode::staticNoise: processStaticSequence(excitation); break;
        case SequenceState::Mode::triangular: processTriangularSequence(excitation); break;
    }
    telemetry.sourceSamplesGenerated[static_cast<std::size_t>(ExciterKind::sequence)] +=
        static_cast<std::uint64_t>(excitation.getNumSamples());
}

void VoiceExciterRack::processImpulseSequence(juce::dsp::AudioBlock<float>& excitation) noexcept
{
    auto* left = excitation.getChannelPointer(0);
    auto* right = excitation.getChannelPointer(1);
    const auto count = static_cast<int>(excitation.getNumSamples());
    const auto envelopeState = sequence.envelope.getState();
    const auto constantEnvelope = envelopeState == gin::AnalogADSR::State::sustain
                                  || envelopeState == gin::AnalogADSR::State::idle;
    const auto constantEnvelopeValue = sequence.envelope.getOutput();
    const auto bypassed = sequence.filter.isBypassed();
    for (int index = 0; index < count; ++index)
    {
        const auto envelopeValue = constantEnvelope ? constantEnvelopeValue
                                                    : sequence.envelope.process();
        const auto envelopeSample = controlFrame.sequenceLevel * envelopeValue;
        if (sequence.samplesSinceLastImpulse == 0)
            sequence.impulsesLeft = controlFrame.sequenceImpulseLength;
        const auto mono = sequence.impulsesLeft > 0
                              ? envelopeSample / static_cast<float>(controlFrame.sequenceImpulseLength)
                              : 0.0f;
        sequence.impulsesLeft -= sequence.impulsesLeft > 0 ? 1 : 0;
        if (--sequence.samplesSinceLastImpulse < 0)
            sequence.samplesSinceLastImpulse = controlFrame.sequencePeriodInSamples - 1;
        if (bypassed)
        {
            left[index] += mono;
            right[index] += mono;
        }
        else
        {
            float filteredL = 0.0f;
            float filteredR = 0.0f;
            sequence.filter.process(mono, mono, filteredL, filteredR);
            left[index] += filteredL;
            right[index] += filteredR;
        }
    }
}

void VoiceExciterRack::processStaticSequence(juce::dsp::AudioBlock<float>& excitation) noexcept
{
    auto* left = excitation.getChannelPointer(0);
    auto* right = excitation.getChannelPointer(1);
    const auto count = static_cast<int>(excitation.getNumSamples());
    const auto envelopeState = sequence.envelope.getState();
    const auto constantEnvelope = envelopeState == gin::AnalogADSR::State::sustain
                                  || envelopeState == gin::AnalogADSR::State::idle;
    const auto constantEnvelopeValue = sequence.envelope.getOutput();
    const auto bypassed = sequence.filter.isBypassed();
    for (int index = 0; index < count; ++index)
    {
        const auto envelopeValue = constantEnvelope ? constantEnvelopeValue
                                                    : sequence.envelope.process();
        const auto envelopeSample = controlFrame.sequenceLevel * envelopeValue;
        float mono = 0.0f;
        if (sequence.samplesSinceLastImpulse == 0)
        {
            if (sequence.triggerRandom.nextUnitFloat() < controlFrame.sequenceStaticProbability)
                mono = sequence.valueRandom.nextBipolarFloat() * envelopeSample;
            sequence.samplesSinceLastImpulse = controlFrame.sequencePeriodInSamples - 1;
        }
        else
        {
            --sequence.samplesSinceLastImpulse;
        }
        if (bypassed)
        {
            left[index] += mono;
            right[index] += mono;
        }
        else
        {
            float filteredL = 0.0f;
            float filteredR = 0.0f;
            sequence.filter.process(mono, mono, filteredL, filteredR);
            left[index] += filteredL;
            right[index] += filteredR;
        }
    }
}

void VoiceExciterRack::processTriangularSequence(juce::dsp::AudioBlock<float>& excitation) noexcept
{
    auto* left = excitation.getChannelPointer(0);
    auto* right = excitation.getChannelPointer(1);
    const auto count = static_cast<int>(excitation.getNumSamples());
    const auto envelopeState = sequence.envelope.getState();
    const auto constantEnvelope = envelopeState == gin::AnalogADSR::State::sustain
                                  || envelopeState == gin::AnalogADSR::State::idle;
    const auto constantEnvelopeValue = sequence.envelope.getOutput();
    const auto bypassed = sequence.filter.isBypassed();
    for (int index = 0; index < count; ++index)
    {
        const auto envelopeValue = constantEnvelope ? constantEnvelopeValue
                                                    : sequence.envelope.process();
        const auto envelopeSample = controlFrame.sequenceLevel * envelopeValue;
        const auto value = sequence.currentTriValue
                           + (sequence.targetTriValue - sequence.currentTriValue) * sequence.triPhase;
        const auto mono = (value * 2.0f - 1.0f) * envelopeSample;
        sequence.triPhase += sequence.triPhaseInc;
        if (sequence.triPhase >= 1.0f)
        {
            sequence.currentTriValue = sequence.targetTriValue;
            sequence.targetTriValue = sequence.valueRandom.nextUnitFloat();
            sequence.triPhase -= 1.0f;
        }
        if (bypassed)
        {
            left[index] += mono;
            right[index] += mono;
        }
        else
        {
            float filteredL = 0.0f;
            float filteredR = 0.0f;
            sequence.filter.process(mono, mono, filteredL, filteredR);
            left[index] += filteredL;
            right[index] += filteredR;
        }
    }
}

void VoiceExciterRack::processSample(juce::dsp::AudioBlock<float>& excitation,
                                     juce::dsp::AudioBlock<float>& directOutput) noexcept
{
    if (!sample.isPlaying || !sample.snapshot)
        return;

    auto* excitationL = excitation.getChannelPointer(0);
    auto* excitationR = excitation.getChannelPointer(1);
    auto* directL = directOutput.getChannelPointer(0);
    auto* directR = directOutput.getChannelPointer(1);
    const auto* sourceL = sample.snapshot.getReadPointer(0);
    const auto* sourceR = sample.snapshot.getReadPointer(1);
    const auto count = static_cast<int>(excitation.getNumSamples());
    const auto loopLength = sample.endSample - sample.startSample;
    if (sourceL == nullptr || sourceR == nullptr || loopLength <= 0)
    {
        sample.isPlaying = false;
        return;
    }

    const auto excitationGainL = controlFrame.sampleLevel * controlFrame.sampleMixL;
    const auto excitationGainR = controlFrame.sampleLevel * controlFrame.sampleMixR;
    const auto directGainL = controlFrame.sampleLevel * (1.0f - controlFrame.sampleMixL);
    const auto directGainR = controlFrame.sampleLevel * (1.0f - controlFrame.sampleMixR);
    int generated = 0;

    // Once the envelope is stationary and the filter is bypassed, process
    // contiguous sample runs with JUCE's platform vector kernels. Loop wraps
    // are deliberately retained as chunk boundaries, so this has precisely
    // the same cursor and first-cycle behaviour as the scalar path.
    const auto envelopeState = sample.envelope.getState();
    if (sample.filter.isBypassed()
        && envelopeState == gin::AnalogADSR::State::sustain)
    {
        const auto envelopeValue = sample.envelope.getOutput();
        int outputOffset = 0;
        while (outputOffset < count)
        {
            if (sample.currentSample >= sample.endSample)
            {
                if (!controlFrame.sampleLoop)
                {
                    sample.isPlaying = false;
                    break;
                }
                sample.currentSample = sample.startSample
                                       + (sample.currentSample - sample.endSample) % loopLength;
            }

            const auto chunk = std::min(count - outputOffset,
                                        sample.endSample - sample.currentSample);
            const auto* chunkL = sourceL + sample.currentSample;
            const auto* chunkR = sourceR + sample.currentSample;
            juce::FloatVectorOperations::addWithMultiply(
                excitationL + outputOffset, chunkL, excitationGainL * envelopeValue, chunk);
            juce::FloatVectorOperations::addWithMultiply(
                excitationR + outputOffset, chunkR, excitationGainR * envelopeValue, chunk);
            juce::FloatVectorOperations::addWithMultiply(
                directL + outputOffset, chunkL, directGainL * envelopeValue, chunk);
            juce::FloatVectorOperations::addWithMultiply(
                directR + outputOffset, chunkR, directGainR * envelopeValue, chunk);
            sample.currentSample += chunk;
            outputOffset += chunk;
            generated += chunk;
        }
        telemetry.sourceSamplesGenerated[static_cast<std::size_t>(ExciterKind::sample)] +=
            static_cast<std::uint64_t>(generated);
        return;
    }

    for (int index = 0; index < count; ++index)
    {
        if (sample.currentSample >= sample.endSample)
        {
            if (!controlFrame.sampleLoop)
            {
                sample.isPlaying = false;
                break;
            }
            sample.currentSample = sample.startSample
                                   + (sample.currentSample - sample.endSample) % loopLength;
        }

        float filteredL = 0.0f;
        float filteredR = 0.0f;
        sample.filter.process(sourceL[sample.currentSample], sourceR[sample.currentSample],
                              filteredL, filteredR);
        const auto envelopeValue = sample.envelope.process();
        const auto left = filteredL * envelopeValue;
        const auto right = filteredR * envelopeValue;
        excitationL[index] += left * excitationGainL;
        excitationR[index] += right * excitationGainR;
        directL[index] += left * directGainL;
        directR[index] += right * directGainR;
        ++sample.currentSample;
        ++generated;

        if (sample.envelope.getState() == gin::AnalogADSR::State::idle)
        {
            sample.isPlaying = false;
            break;
        }
    }
    telemetry.sourceSamplesGenerated[static_cast<std::size_t>(ExciterKind::sample)] +=
        static_cast<std::uint64_t>(generated);
}

void VoiceExciterRack::processExternal(juce::dsp::AudioBlock<float>& excitation,
                                       const ExciterRenderContext& context) noexcept
{
    auto* destinationL = excitation.getChannelPointer(0);
    auto* destinationR = excitation.getChannelPointer(1);
    const auto count = static_cast<int>(excitation.getNumSamples());
    const auto* frame = context.externalInput;
    const bool valid = frame != nullptr && frame->isValid()
                       && context.sourceStartSample >= 0
                       && context.sourceStartSample + count <= frame->getNumSamples();
    const auto* sourceL = valid ? frame->getReadPointer(0) + context.sourceStartSample : nullptr;
    const auto* sourceR = valid ? frame->getReadPointer(1) + context.sourceStartSample : nullptr;
    if (valid)
        ++telemetry.externalVoiceReads;

    const auto envelopeState = externalInput.envelope.getState();
    if (envelopeState == gin::AnalogADSR::State::sustain
        || envelopeState == gin::AnalogADSR::State::idle)
    {
        if (valid)
        {
            const auto envelopeValue = externalInput.envelope.getOutput();
            juce::FloatVectorOperations::addWithMultiply(
                destinationL, sourceL, controlFrame.externalGainL * envelopeValue, count);
            juce::FloatVectorOperations::addWithMultiply(
                destinationR, sourceR, controlFrame.externalGainR * envelopeValue, count);
        }
        telemetry.sourceSamplesGenerated[static_cast<std::size_t>(ExciterKind::externalInput)] +=
            static_cast<std::uint64_t>(count);
        return;
    }

    for (int index = 0; index < count; ++index)
    {
        const auto envelopeValue = externalInput.envelope.process();
        const auto left = sourceL != nullptr ? sourceL[index] : 0.0f;
        const auto right = sourceR != nullptr ? sourceR[index] : 0.0f;
        destinationL[index] += left * controlFrame.externalGainL * envelopeValue;
        destinationR[index] += right * controlFrame.externalGainR * envelopeValue;
    }
    telemetry.sourceSamplesGenerated[static_cast<std::size_t>(ExciterKind::externalInput)] +=
        static_cast<std::uint64_t>(count);
}
