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
    outSample = outSample + delayLine.popSample(0, delayLengthInSamples, true);
    outSample = dampingFilter.processSample(outSample);
    // outSample = svf.processSample(0, outSample);
    if(!testFlag) outSample = dispersionFilter.processSample(outSample);
    outSample = outSample * decayCoefficient;
    return outSample;
}

float Resonator::pushSample(float input)
{
    if(!enabled) return 0.0f;
    // delayLine.pushSample(0, dcBlocker.processSample(input));
    if(!testFlag) delayLine.pushSample(0, input);
    else delayLine.pushSample(0, dcBlocker.processSample(input));
    return input;
}

void Resonator::reset()
{
    delayLine.reset();
    dampingFilter.reset();
    dcBlocker.reset();
    dispersionFilter.reset();
    svf.reset();
}

void Resonator::prepare(const juce::dsp::ProcessSpec& spec)
{
    this->reset();
    this->frequency = 440.0f; //set the frequency to anything just to avoid dividing by zero when computing delay length
    sampleRate = spec.sampleRate;
    minFrequency = 15;
    maxFrequency = (sampleRate / 2.0f) - 1;
    delayLine.setMaximumDelayInSamples(4096);
    delayLine.prepare(spec);

    dampingFilter.coefficients =
        juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass(
            sampleRate, spec.sampleRate / 4.0f);
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

/**
 * Applies a frequency offset to this resonator's frequency, based on a number of semitones and cents.
 */
void Resonator::setHarmonicOffsetInSemitones(float semitones, float cents)
{
    //convert the semitones and cents into a frequency offset, based on the current frequency
    this->harmonicMultiplier = std::pow(2.0f, semitones / 12.0f + cents / 1200.0f);
}

void Resonator::updateParameters(float frequency)
{
    this->enabled = params.enabled->isOn();

    this->harmonicMultiplier = std::pow(2.0f, voice.getValue(params.harmonic) / 12.0f);
    this->frequency = frequency;

    delayLengthInSamples = sampleRate / (frequency * harmonicMultiplier);
    delayLine.setDelay(delayLengthInSamples);

    decayTime = voice.getValue(params.decayTime);
    decayCoefficient = std::pow(0.001, 1.0 / (decayTime * frequency)); //simple t60 decay time calculation
    dispersionFilter.setDispersionAmount(voice.getValue(params.dispersion));
    dampingFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass(
        sampleRate, voice.getValue(params.decayFilterCutoff));
    //TODO implement decayFilterType
    //TODO implement decayFilterResonance
    //TODO implement decayFilterKeytrack
    gain = voice.getValue(params.gain);

    if(voice.getValue(params.testParameter) > 0.5f)
    {
        testFlag = true;
        DBG("TestFlag is true");
    } else
    {
        testFlag = false;
        DBG("TestFlag is false");
    }
}