#ifndef RESONATORBANK_H
#define RESONATORBANK_H

#include<JuceHeader.h>

class ResonatorVoice;

class ResonatorBank {
public:
    ResonatorBank(ResonatorVoice& parentVoice) : voice(parentVoice) {}
    virtual ~ResonatorBank() = default;

    virtual void process(juce::dsp::AudioBlock<float>& exciterBlock, juce::dsp::AudioBlock<float>& outputBlock) = 0;
    virtual void reset() = 0;
    virtual void prepare(const juce::dsp::ProcessSpec& spec) = 0;
    virtual void updateParameters(float newFrequency, int numSamples) = 0;

    ResonatorVoice& voice;
    int resonatorBankIndex = -1;
};



#endif //RESONATORBANK_H
