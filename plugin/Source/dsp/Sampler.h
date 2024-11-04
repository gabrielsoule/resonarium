#pragma once

#include <JuceHeader.h>

class Sampler
{
public:
    Sampler();
    ~Sampler();

    void prepare(const juce::dsp::ProcessSpec& spec);
    bool loadFile(const juce::File& file);
    void clear();
    float getSample(int channel, int position) const;
    juce::dsp::AudioBlock<float> getSubBlock(int start, int length);
    juce::AudioFormatManager& getFormatManager();
    int getNumSamples() const;
    int getNumChannels() const;
    double getSampleRate() const;
    bool isLoaded() const;
    double getFileSampleRate() const;
    juce::String& getFilePath();
    juce::String& getSampleName();

private:
    juce::AudioBuffer<float> sampleBuffer;
    juce::AudioFormatManager formatManager;
    double sampleRate;
    double fileSampleRate;
    juce::String sampleName = "";
    juce::String path = "";
    bool loaded = false;
};