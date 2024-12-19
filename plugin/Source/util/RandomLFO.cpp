#include "RandomLFO.h"

RandomLFO::RandomLFO(gin::ModVoice* voice, bool stereo) : voice(voice), stereo(stereo),
                                                          leftState(sideStates[0]),
                                                          rightState(sideStates[1])
{
}

RandomLFO::RandomLFO(RandomLFOParams params, bool stereo) : params(params), stereo(stereo),
                                                            leftState(sideStates[0]),
                                                            rightState(sideStates[1])
{
}

RandomLFO::RandomLFO(gin::ModVoice* voice, RandomLFOParams params, bool stereo) : params(params), stereo(stereo),
    leftState(sideStates[0]),
    rightState(sideStates[1])
{
}


void RandomLFO::prepare(const juce::dsp::ProcessSpec& spec)
{
    this->sampleRate = spec.sampleRate;
    this->oneOverSampleRate = 1.0f / sampleRate;
    centerState.rng.setSeed(params.seed);
    leftState.rng.setSeed(centerState.rng.nextInt());
    rightState.rng.setSeed(centerState.rng.nextInt());
    this->reset();
}

void RandomLFO::updateParameters(float frequency)
{
    this->rate = frequency;
    this->sync = params.sync->isOn();
    this->mode = static_cast<Mode>(static_cast<int>(params.mode->getProcValue()));
    this->depth = voice->getValue(params.depth);
    this->offset = voice->getValue(params.offset);
    this->smooth = voice->getValue(params.smooth);
    this->chaos = voice->getValue(params.chaos);
    this->jitter = voice->getValue(params.jitter);
    this->stereoAmount = voice->getValue(params.stereo);
}

void RandomLFO::updateParametersMono(gin::ModMatrix& matrix, float frequency)
{
    this->rate = frequency;
    this->sync = params.sync->isOn();
    this->mode = static_cast<Mode>(static_cast<int>(params.mode->getProcValue()));
    this->depth = matrix.getValue(params.depth);
    this->offset = matrix.getValue(params.offset);
    this->smooth = matrix.getValue(params.smooth);
    this->chaos = matrix.getValue(params.chaos);
    this->jitter = matrix.getValue(params.jitter);
    this->stereoAmount = matrix.getValue(params.stereo);
}

void RandomLFO::reset()
{
    currentPhase = 0;
    centerState.lastRandomValue = centerState.rng.nextFloat();
    centerState.nextRandomValue = centerState.rng.nextFloat();
    leftState.lastRandomValue = leftState.rng.nextFloat();
    leftState.nextRandomValue = leftState.rng.nextFloat();
    rightState.lastRandomValue = rightState.rng.nextFloat();
    rightState.nextRandomValue = rightState.rng.nextFloat();
}

void RandomLFO::noteOn(float phase)
{
}

void RandomLFO::processInternal(int numSamples, RandomState& state)
{
    float min = std::max(0.0f, state.lastRandomValue - state.lastRandomValue * chaos);
    float max = std::min(1.0f, state.lastRandomValue + (1.0f - state.lastRandomValue) * chaos);

    static constexpr uint32_t MAX_UINT = std::numeric_limits<uint32_t>::max();
    state.lastRandomValue = state.nextRandomValue;

    // Convert signed int to unsigned int before casting to float
    uint32_t unsignedRandom = static_cast<uint32_t>(state.rng.nextInt()) + 0x80000000;
    state.nextRandomValue = min + (static_cast<float>(unsignedRandom) / static_cast<float>(MAX_UINT)) * (max - min);

    jassert(state.lastRandomValue >= 0.0f && state.lastRandomValue <= 1.0f);
    jassert(state.nextRandomValue >= 0.0f && state.nextRandomValue <= 1.0f);
}

void RandomLFO::process(int numSamples)
{
    phaseDelta = rate * oneOverSampleRate * numSamples;
    currentPhase += phaseDelta;
    if (currentPhase >= 1.0f)
    {
        currentPhase -= 1.0f;
        processInternal(numSamples, centerState);
        processInternal(numSamples, leftState);
        processInternal(numSamples, rightState);
    }

    auto modifiedPhase = currentPhase;
    modifiedPhase = smooth == 0 ? 1 : juce::jmin(1.0f, ((1.0f / smooth) * currentPhase));
    centerState.currentRandomValue = perlinInterpolate(centerState.lastRandomValue, centerState.nextRandomValue,
                                                       modifiedPhase);
    leftState.currentRandomValue = perlinInterpolate(leftState.lastRandomValue, leftState.nextRandomValue,
                                                     modifiedPhase);
    rightState.currentRandomValue = perlinInterpolate(rightState.lastRandomValue, rightState.nextRandomValue,
                                                      modifiedPhase);

    centerState.atomicState.store(centerState.currentRandomValue, std::memory_order_relaxed);
    leftState.atomicState.store(leftState.currentRandomValue, std::memory_order_relaxed);
    rightState.atomicState.store(rightState.currentRandomValue, std::memory_order_relaxed);
}

float RandomLFO::getOutput()
{
    return centerState.currentRandomValue * 2.0f - 1.0f;
}

float RandomLFO::getOutputUnclamped()
{
    jassertfalse;
}

float RandomLFO::getOutput(int channel)
{
    jassert(channel == 0 || channel == 1);
    const float output = sideStates[channel].currentRandomValue * stereoAmount + centerState.currentRandomValue * (1.0f
        - stereoAmount);
    return output * 2.0f - 1.0f;
}

float RandomLFO::getOutputUnclamped(int channel)
{
    jassertfalse;
}

float RandomLFO::perlinInterpolate(float from, float to, float t)
{
    t = t * t * (3.0f - 2.0f * t);
    return (from * (1.0f - t) + to * t);
}
