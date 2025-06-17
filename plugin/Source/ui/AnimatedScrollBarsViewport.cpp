#include "AnimatedScrollBarsViewport.h"

bool AnimatedScrollBarsViewport::isKnobComponent(juce::Component* component)
{
    if (!component)
        return false;
        
    // Simple string-based detection - check if the component name contains "Knob"
    auto className = juce::String(typeid(*component).name());
    return className.contains("Knob");
}