#include "TooltipManager.h"

TooltipManager::TooltipManager()
{
    int size = BinaryData::tooltips_jsonSize;
    const char* jsonData = BinaryData::tooltips_json;

    if (jsonData != nullptr && size > 0)
    {
        juce::String jsonContent(jsonData, size);
        tooltipData = juce::JSON::parse(jsonContent);
    }
    else
    {
        juce::Logger::writeToLog("Failed to load tooltips.json from BinaryData");
    }
    DBG("Successfully loaded tooltips.json");
    DBG(getTooltip("exciters.impulseExciter.gain"));
    DBG(tooltipData.toString());
}

juce::String TooltipManager::getTooltip(const juce::String& path)
{
    juce::StringArray pathComponents;
    pathComponents.addTokens(path, ".", "");

    const juce::var* current = &tooltipData;

    for (const auto& component : pathComponents)
    {
        if (current->isObject())
        {
            auto* obj = current->getDynamicObject();
            if (obj && obj->hasProperty(component))
            {
                current = &(obj->getProperty(component));
            }
            else
            {
                return "No tooltip available for " + path;
            }
        }
        else
        {
            return "No tooltip available for " + path;
        }
    }

    if (current->isString())
    {
        return current->toString();
    }

    return "No tooltip available for " + path;
}
