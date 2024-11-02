#include "Sampler.h"

Sampler::Sampler() : sampleRate(44100.0)
{
    formatManager.registerBasicFormats();
}

Sampler::~Sampler()
{
}

void Sampler::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
}

bool Sampler::loadFile(const juce::File& file)
{
    auto* reader = formatManager.createReaderFor(file);

    if (reader != nullptr)
    {
        std::unique_ptr<juce::AudioFormatReader> readerPtr(reader);

        sampleBuffer.setSize(reader->numChannels, reader->lengthInSamples);
        reader->read(&sampleBuffer,
                    0,
                    reader->lengthInSamples,
                    0,
                    true,    // read left channel
                    true);   // read right channel

        fileSampleRate = reader->sampleRate;
        DBG("Loaded sample file: " + file.getFileName());
        DBG("Length in samples : " + juce::String(reader->lengthInSamples));
        return true;
    }
    DBG("Failed to load sample file: " + file.getFileName());
    return false;
}
float Sampler::getSample(int channel, int position) const
{
    if (position >= 0 && position < sampleBuffer.getNumSamples())
        return sampleBuffer.getSample(channel, position);
    return 0.0f;
}

float Sampler::getSampleInterpolated(int channel, double position) const
{
    int pos1 = static_cast<int>(std::floor(position));
    int pos2 = pos1 + 1;
    float frac = position - pos1;

    float sample1 = getSample(channel, pos1);
    float sample2 = getSample(channel, pos2);

    return sample1 + frac * (sample2 - sample1);
}

juce::dsp::AudioBlock<float> Sampler::getSubBlock(int start, int length)
{
    jassert(start >= 0 && length > 0);
    jassert(start + length <= sampleBuffer.getNumSamples());

    float* const* channelArray = sampleBuffer.getArrayOfWritePointers();
    return juce::dsp::AudioBlock<float>(channelArray,
                                      sampleBuffer.getNumChannels(),
                                      start,      // sample offset
                                      length);    // number of samples
}

juce::AudioFormatManager& Sampler::getFormatManager()
{
    return formatManager;
}

int Sampler::getNumSamples() const
{
    return sampleBuffer.getNumSamples();
}

int Sampler::getNumChannels() const
{
    return sampleBuffer.getNumChannels();
}

double Sampler::getSampleRate() const
{
    return sampleRate;
}

bool Sampler::isLoaded() const
{
    return sampleBuffer.getNumSamples() > 0;
}

double Sampler::getFileSampleRate()
{
    return fileSampleRate;
}
