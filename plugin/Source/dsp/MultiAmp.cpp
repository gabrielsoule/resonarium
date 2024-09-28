//
// Created by Gabriel Soule on 8/2/24.
//

#include "MultiAmp.h"

MultiAmp::MultiAmp(ResonariumProcessor& p, MultiAmpParams params) : proc(p),
                                                                      params(params),
                                                                      bassAmp(FXBaseCallback([this]
                                                                      {
                                                                          return proc.getSampleRate();
                                                                      })),
                                                                      bigAmp(FXBaseCallback([this]
                                                                      {
                                                                          return proc.getSampleRate();
                                                                      })),
                                                                      deRez2(FXBaseCallback([this]
                                                                      {
                                                                          return proc.getSampleRate();
                                                                      })),
                                                                      fireAmp(FXBaseCallback([this]
                                                                      {
                                                                          return proc.getSampleRate();
                                                                      })),
                                                                      grindAmp(FXBaseCallback([this]
                                                                      {
                                                                          return proc.getSampleRate();
                                                                      })),
                                                                      leadAmp(FXBaseCallback([this]
                                                                      {
                                                                          return proc.getSampleRate();
                                                                      }))
{
}

void MultiAmp::reset()
{
    bassAmp.reset();
    bigAmp.reset();
    deRez2.reset();
    fireAmp.reset();
    grindAmp.reset();
    leadAmp.reset();
}

juce::String MultiAmp::getParameterName(Mode mode, int index)
{
    char* name = new char[32];

    switch (mode)
    {
    case LEAD:
        LeadAmp::getParameterName(index, name);
        break;
    case FIRE:
        FireAmp::getParameterName(index, name);
        break;
    case GRIND:
        GrindAmp::getParameterName(index, name);
        break;
    case BIG:
        BigAmp::getParameterName(index, name);
        break;
    case DEREZ:
        DeRez2::getParameterName(index, name);
        break;
    case BASS:
        BassAmp::getParameterName(index, name);
        break;
    default:
        jassertfalse;
    }

    return juce::String(name, 32);
}

void MultiAmp::process(juce::dsp::ProcessContextReplacing<float> context)
{
    jassert(context.getOutputBlock().getNumSamples() == context.getInputBlock().getNumSamples());
    float* channelL = context.getOutputBlock().getChannelPointer(0);
    float* channelR = context.getOutputBlock().getChannelPointer(1);
    float* channels[] = {channelL, channelR};
    switch (mode)
    {
    case LEAD:
        leadAmp.processReplacing(channels, channels, context.getOutputBlock().getNumSamples());
        break;
    case FIRE:
        fireAmp.processReplacing(channels, channels, context.getOutputBlock().getNumSamples());
        break;
    case GRIND:
        grindAmp.processReplacing(channels, channels, context.getOutputBlock().getNumSamples());
        break;
    case BIG:
        bigAmp.processReplacing(channels, channels, context.getOutputBlock().getNumSamples());
        break;
    case DEREZ:
        deRez2.processReplacing(channels, channels, context.getOutputBlock().getNumSamples());
        break;
    case BASS:
        bassAmp.processReplacing(channels, channels, context.getOutputBlock().getNumSamples());
        break;
    default:
        jassertfalse;
        break;
    }
}
