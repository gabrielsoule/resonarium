#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <utility>

#include <juce_gui_basics/juce_gui_basics.h>

/**
 * The editor's only steady animation clock.
 *
 * Fixed subscribers are installed while the editor is constructed; tab pages
 * register their visualizers as they are (re)built. A callback is eligible
 * only while its component is showing and intersects every parent clip, so
 * hidden tabs and controls outside a viewport do no frame work.
 */
class UIUpdateScheduler final : private juce::Timer
{
public:
    enum class Rate : std::uint8_t
    {
        hz4,
        hz10,
        hz30,
        hz60,
    };

    using Callback = std::function<void()>;

    explicit UIUpdateScheduler(juce::Component& ownerComponent)
        : owner(&ownerComponent)
    {
        startTimerHz(60);
    }

    ~UIUpdateScheduler() override
    {
        stopTimer();
        clear();
    }

    void setFrameBeginCallback(Callback callback)
    {
        frameBeginCallback = std::move(callback);
    }

    void setOwnerVisibilityCallback(std::function<void(bool)> callback)
    {
        ownerVisibilityCallback = std::move(callback);
    }

    void add(juce::Component& component, Rate rate, Callback callback)
    {
        jassert(callback != nullptr);

        // Recycle entries whose component has been destroyed, so a rebuilt
        // tab page can re-register in the same frame its predecessor died.
        for (auto& entry : entries)
        {
            if (entry.callback != nullptr && entry.component == nullptr)
            {
                entry.callback = {};
                entry.divisor = 1;
            }
        }

        for (auto& entry : entries)
        {
            if (entry.callback != nullptr)
                continue;

            entry.component = juce::Component::SafePointer<juce::Component>(&component);
            entry.callback = std::move(callback);
            entry.divisor = divisorFor(rate);
            return;
        }

        jassertfalse; // Increase maxEntries if the editor gains more animations.
    }

    void clear() noexcept
    {
        for (auto& entry : entries)
        {
            entry.callback = {};
            entry.component = {};
            entry.divisor = 1;
        }

        frameBeginCallback = {};
        ownerVisibilityCallback = {};
    }

    [[nodiscard]] static bool isActuallyVisible(const juce::Component& component) noexcept
    {
        if (!component.isShowing() || component.getWidth() <= 0 || component.getHeight() <= 0)
            return false;

        auto visibleBounds = component.getScreenBounds();
        for (auto* parent = component.getParentComponent(); parent != nullptr;
             parent = parent->getParentComponent())
        {
            if (!parent->isVisible())
                return false;

            visibleBounds = visibleBounds.getIntersection(parent->getScreenBounds());
            if (visibleBounds.isEmpty())
                return false;
        }

        return !visibleBounds.isEmpty();
    }

private:
    static constexpr std::size_t maxEntries = 48;

    struct Entry
    {
        juce::Component::SafePointer<juce::Component> component;
        Callback callback;
        std::uint8_t divisor = 1;
    };

    static constexpr std::uint8_t divisorFor(Rate rate) noexcept
    {
        switch (rate)
        {
            case Rate::hz4:  return 15;
            case Rate::hz10: return 6;
            case Rate::hz30: return 2;
            case Rate::hz60: return 1;
        }

        return 1;
    }

    void timerCallback() override
    {
        const auto ownerIsVisible = owner != nullptr && isActuallyVisible(*owner);
        if (ownerIsVisible != previousOwnerVisibility)
        {
            previousOwnerVisibility = ownerIsVisible;
            frameCounter = 0;
            if (ownerVisibilityCallback)
                ownerVisibilityCallback(ownerIsVisible);
        }

        if (!ownerIsVisible)
            return;

        const auto currentFrame = frameCounter++;
        if ((currentFrame % divisorFor(Rate::hz30)) == 0 && frameBeginCallback)
            frameBeginCallback();

        for (auto& entry : entries)
        {
            if (entry.callback == nullptr || (currentFrame % entry.divisor) != 0)
                continue;

            if (entry.component == nullptr)
            {
                entry.callback = {};
                continue;
            }

            if (isActuallyVisible(*entry.component))
                entry.callback();
        }
    }

    juce::Component::SafePointer<juce::Component> owner;
    std::array<Entry, maxEntries> entries {};
    Callback frameBeginCallback;
    std::function<void(bool)> ownerVisibilityCallback;
    std::uint64_t frameCounter = 0;
    bool previousOwnerVisibility = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UIUpdateScheduler)
};
