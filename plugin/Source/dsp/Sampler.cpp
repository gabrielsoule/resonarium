#include "Sampler.h"

Sampler::Sampler() : sampleRate(44100.0)
{
    formatManager.registerBasicFormats();
    path = "";
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
        path = file.getFullPathName();
        sampleName = file.getFileName();
        DBG("Sampler: loaded sample file: " + file.getFileName());
        DBG("Sampler: length in samples : " + juce::String(reader->lengthInSamples));
        loaded = true;
        return true;
    }
    DBG("Sampler: failed to load sample file: " + file.getFileName());
    loaded = false;
    return false;
}

void Sampler::clear()
{
    sampleBuffer.clear();
    fileSampleRate = 0.0;
    path = "";
    loaded = false;
}

float Sampler::getSample(int channel, int position) const
{
    if (position >= 0 && position < sampleBuffer.getNumSamples())
        return sampleBuffer.getSample(channel, position);
    return 0.0f;
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
    return loaded;
}

double Sampler::getFileSampleRate() const
{
    return fileSampleRate;
}

juce::String& Sampler::getFilePath()
{
    return path;
}

juce::String& Sampler::getSampleName()
{
    return sampleName;
}
