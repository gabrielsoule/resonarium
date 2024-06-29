#ifndef EQTHREE_H
#define EQTHREE_H

#include <JuceHeader.h>
class ResonatorVoice;
/**
* A three-band EQ with arbitrary order, implemented as three parallel biquad cascades, one per band,
* all of which are implemented using the state-variable topology to facilitate rapid coefficient modulation.
*
* Holds an optional voice pointer, for per-voice modulation, and an optional set of hosted parameters.
*
* The EQ can be normalized, which means that gains of each band are adjusted so that the amplitude response
* at any frequency never exceeds one.
*/
class EQThree
{
public:
    EQThree(int order, bool normalize) : order(order), normalize(normalize)
    {
    }

    EQThree(ResonatorVoice* voice, int order, bool normalize) : voice(voice), order(order), normalize(normalize)
    {
    }


    void prepare(const juce::dsp::ProcessSpec& spec);
    void process(juce::dsp::AudioBlock<float>& block);
    void reset();
    /**
     * Sets the gain of the low band.
     */
    void setLowGain(float gain);
    /**
     * Sets the gain of the mid band.
     */
    void setMidGain(float gain);
    /**
     * Sets the gain of the high band.
     */
    void setHighGain(float gain);
    void setLowFrequency(float frequency);
    void setHighFrequency(float frequency);
    ResonatorVoice* voice;
    // std::vector<chowdsp::StateVariableFilter<float, chowdsp::StateVariableFilterType::LowShelf>



private:
    int order;
    bool normalize;
    float lowFreq;
    float highFreq;
    float lowGain;
    float midGain;
    float highGain;
};


#endif //EQTHREE_H
