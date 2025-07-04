#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

// No forward declarations needed for simple string-based detection

/**
    A custom Viewport that shows its scrollbars on top of the content component
    and hides them when no user interactions is performed.
*/
class AnimatedScrollBarsViewport : public juce::Viewport
{
public:
    AnimatedScrollBarsViewport(const juce::String& componentName = juce::String())
        : juce::Viewport(componentName)
    {
        setScrollBarsShown(false, false, true, true);

        scrollBarListener.updateFunction = [this](juce::ScrollBar* bar, double newRangeStart)
        {
            auto newRangeStartInt = juce::roundToInt(newRangeStart);

            if (bar == &hBar)
            {
                setViewPosition(newRangeStartInt, getViewPositionY());
            }
            else if (bar == &vBar)
            {
                setViewPosition(getViewPositionX(), newRangeStartInt);
            }
        };

        for (auto bar : {&hBar, &vBar})
        {
            addChildComponent(bar);
            bar->addListener(&scrollBarListener);
        }

        addMouseListener(this, true);
    }

    void resized() override
    {
        juce::Viewport::resized();
        updateBars();
    }

    void mouseEnter(const juce::MouseEvent& e) override
    {
        if (!isMouseOver)
        {
            isMouseOver = true;
            for (auto bar : {&hBar, &vBar})
            {
                animator.fadeIn(bar, 400);
                bar->setVisible(true);
            }
        }
    }

    void mouseExit(const juce::MouseEvent& e) override
    {
        //check if mouse is outside the viewport bounds
        if (!getLocalBounds().contains(e.getEventRelativeTo(this).getPosition()))
        {
            isMouseOver = false;
            for (auto bar : {&hBar, &vBar})
            {
                animator.fadeOut(bar, 400);
            }
        }
    }

    void visibleAreaChanged(const juce::Rectangle<int>& newVisibleArea) override
    {
        updateBars();
    }

    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        // Recursion guard to prevent infinite loops
        if (isHandlingWheelEvent)
            return;
            
        // If Alt is pressed, try to find a knob under the mouse and delegate to it
        if (e.mods.isAltDown())
        {
            if (handleKnobWheelEvent(e, wheel))
                return; // Event was handled by a knob
        }
        
        // Otherwise, use normal viewport scrolling
        juce::Viewport::mouseWheelMove(e, wheel);
    }

private:
    /**
        The Viewport is already a ScrollBar::Listener,
        this subclass is a workaround so we can
        listen also to our custom scrollbars changes.
    */
    class ScrollBarListenerImpl : public juce::ScrollBar::Listener
    {
    public:
        std::function<void(juce::ScrollBar*, double)> updateFunction;

        void scrollBarMoved(juce::ScrollBar* bar, double newRangeStart) override
        {
            if (updateFunction)
            {
                updateFunction(bar, newRangeStart);
            }
        }
    };

    void updateBars()
    {
        auto viewportBounds = getBounds();
        auto scrollbarWidth = getScrollBarThickness();
        auto contentBounds = getViewedComponent()->getBounds();
        auto singleStepSize = 16;

        hBar.setBounds(0, viewportBounds.getHeight() - scrollbarWidth, viewportBounds.getWidth(), scrollbarWidth);
        hBar.setRangeLimits(0.0, contentBounds.getWidth());
        hBar.setCurrentRange(getViewPositionX(), viewportBounds.getWidth());
        hBar.setSingleStepSize(singleStepSize);
        hBar.cancelPendingUpdate();

        vBar.setBounds(viewportBounds.getWidth() - scrollbarWidth, 0, scrollbarWidth, viewportBounds.getHeight());
        vBar.setRangeLimits(0.0, contentBounds.getHeight());
        vBar.setCurrentRange(getViewPositionY(), viewportBounds.getHeight());
        vBar.setSingleStepSize(singleStepSize);
        vBar.cancelPendingUpdate();
    }

    bool handleKnobWheelEvent(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
    {
        // Get the position relative to the viewport's content
        auto contentComponent = getViewedComponent();
        if (!contentComponent)
            return false;
            
        // Convert mouse position to content coordinates
        auto mousePositionInContent = e.getEventRelativeTo(contentComponent).getPosition();
        
        // Find the deepest component at this position
        auto* targetComponent = contentComponent->getComponentAt(mousePositionInContent);
        if (!targetComponent)
            return false;
        
        // Check if the target component is a knob or contains a knob
        auto* knobComponent = findKnobComponent(targetComponent);
        if (!knobComponent)
            return false;
        
        // Create a new mouse event relative to the knob component
        auto relativeEvent = e.getEventRelativeTo(knobComponent);
        
        // Set recursion guard before delegating
        isHandlingWheelEvent = true;
        
        // Delegate the wheel event to the knob
        knobComponent->mouseWheelMove(relativeEvent, wheel);
        
        // Clear recursion guard after delegating
        isHandlingWheelEvent = false;
        
        return true;
    }
    
    juce::Component* findKnobComponent(juce::Component* component)
    {
        if (!component)
            return nullptr;
            
        // Check if the current component is a knob
        if (isKnobComponent(component))
            return component;
            
        // Search up the parent hierarchy to find a knob
        for (auto* parent = component->getParentComponent(); parent != nullptr; parent = parent->getParentComponent())
        {
            if (isKnobComponent(parent))
                return parent;
                
            // Don't search beyond the viewport's content
            if (parent == getViewedComponent())
                break;
        }
        
        return nullptr;
    }
    
    bool isKnobComponent(juce::Component* component);

    juce::ScrollBar hBar{false};
    juce::ScrollBar vBar{true};
    ScrollBarListenerImpl scrollBarListener;
    juce::ComponentAnimator animator;
    bool isMouseOver = false;
    bool isHandlingWheelEvent = false;
};
