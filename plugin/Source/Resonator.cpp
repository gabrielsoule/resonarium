#include "Resonator.h"

#include "ResonatorVoice.h"

Resonator::Resonator(ResonatorVoice& parentVoice) : voice(parentVoice)
{
}

//TODO Support multi-channel processing and do some interesting stuff in the stereo field
/**
 * WARNING: Since the enclosing ResonatorBank manages the feedback gain and output gain of each resonator,
 * essentially duplicating the signal, the resonator does NOT apply its own gain to its own output signal.
 * The gain (resonator.gain) must be applied by the caller of processSample.
 * @param input
 * @return
 */
float Resonator::processSample(float input)
{
    if(!enabled) return 0.0f;
    float outSample = popSample();
    pushSample(outSample + input);
    return outSample;
}

float Resonator::popSample()
{
    if(!enabled) return 0.0f;
    float outSample = 0;
    outSample = outSample + delayTop.popSample(0, delayLengthInSamples, true);
    outSample = dampingFilter.processSample(outSample);
    // outSample = svf.processSample(0, outSample);
    outSample = dispersionFilter.processSample(outSample);
    outSample = outSample * decayCoefficient;
    return outSample;
}

float Resonator::pushSample(float input)
{
    if(!enabled) return 0.0f;
    delayTop.pushSample(0, dcBlocker.processSample(input));
    return input;
}

void Resonator::reset()
{
    delayTop.reset();
    delayBtm.reset();
    dampingFilter.reset();
    dcBlocker.reset();
    dispersionFilter.reset();
    testMultiTap = false;
    svf.reset();
}

void Resonator::prepare(const juce::dsp::ProcessSpec& spec)
{
    this->reset();
    this->frequency = 440.0f; //set the frequency to anything just to avoid dividing by zero when computing delay length
    sampleRate = spec.sampleRate;
    minFrequency = 15;
    maxFrequency = (sampleRate / 2.0f) - 1;
    dampingFilterCutoff = spec.sampleRate / 4.0f;
    // setDecayTime(2.0f);
    delayTop.setMaximumDelayInSamples(4096);
    delayBtm.setMaximumDelayInSamples(4096);
    delayTop.prepare(spec);
    delayBtm.prepare(spec);

    dampingFilter.coefficients =
        juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass(
            sampleRate, dampingFilterCutoff);
    dampingFilter.prepare(spec);

    dispersionFilter.setDispersionAmount(0.0f);
    dispersionFilter.prepare(spec);

    //One pole DC blocker coefficients that are appropriate for a ~40-50hz sample rate
    juce::dsp::IIR::Coefficients<float>::Ptr dcBlockerCoefficients =
        new juce::dsp::IIR::Coefficients<float>(1, -1, 1, -0.995f);
    dcBlocker.coefficients = dcBlockerCoefficients;
    dcBlocker.prepare(spec);
    svf.setCutoffFrequency(spec.sampleRate / 4.0f);
    svf.setMode(0);
    svf.setQValue(0.707f);
    svf.prepare(spec);
    updateParameters(frequency);
}

// void Resonator::setFrequency(float newFrequency)
// {
//     frequency = newFrequency;
//     updateParameters();
//
// }

// void Resonator::setDampingFilterCutoff(float cutoff)
// {
//     if (cutoff != dampingFilterCutoff)
//     {
//         dampingFilterCutoff = cutoff;
//         dampingFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass(
//             sampleRate, dampingFilterCutoff);
//     }
// }

// void Resonator::setHarmonicMultiplier(float newHarmonicMultiplier)
// {
//     harmonicMultiplier = newHarmonicMultiplier;
//     updateParameters();
// }

/**
 * Applies a frequency offset to this resonator's frequency, based on a number of semitones and cents.
 */
void Resonator::setHarmonicOffsetInSemitones(float semitones, float cents)
{
    //convert the semitones and cents into a frequency offset, based on the current frequency
    this->harmonicMultiplier = std::pow(2.0, semitones / 12.0 + cents / 1200.0);
}

void Resonator::updateParameters(float frequency)
{
    this->enabled = params.enabled->isOn();

    this->harmonicMultiplier = std::pow(2.0f, voice.getValue(params.harmonic) / 12.0f);
    this->frequency = frequency;

    delayLengthInSamples = sampleRate / (frequency * harmonicMultiplier);
    delayTop.setDelay(delayLengthInSamples);
    delayBtm.setDelay(delayLengthInSamples);

    decayTime = voice.getValue(params.decayTime);
    decayCoefficient = std::pow(0.001, 1.0 / (decayTime * frequency)); //simple t60 decay time calculation
    dispersionFilter.setDispersionAmount(voice.getValue(params.dispersion));
    dampingFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass(
        sampleRate, dampingFilterCutoff);
    //TODO implement decayFilterType
    //TODO implement decayFilterResonance
    //TODO implement decayFilterKeytrack
    gain = voice.getValue(params.gain);
}

// void Resonator::setDecayTime(float timeInSeconds)
// {
//     decayTime = timeInSeconds;
//     decayCoefficient = std::pow(0.001, 1.0 / (timeInSeconds * frequency));
//     DBG("Setting decay coefficient to " + juce::String(decayCoefficient) + " for a decay time of " + juce::String(
//         decayCoefficient));
// }
