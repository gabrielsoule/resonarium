#pragma once

#include <JuceHeader.h>

class Sampler
{
public:
    Sampler();
    ~Sampler();

    void prepare(const juce::dsp::ProcessSpec& spec);
    bool loadFile(const juce::File& file);
    float getSample(int channel, int position) const;
    float getSampleInterpolated(int channel, double position) const;
    juce::dsp::AudioBlock<float> getSubBlock(int start, int length);
    int getNumSamples() const;
    int getNumChannels() const;
    double getSampleRate() const;
    bool isLoaded() const;
    double getFileSampleRate();

private:
    juce::AudioBuffer<float> sampleBuffer;
    juce::AudioFormatManager formatManager;
    double sampleRate;
    double fileSampleRate;
};