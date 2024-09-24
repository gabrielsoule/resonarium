#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
    A custom Viewport that shows its scrollbars on top of the content component
    and hides them when no user interactions is performed.
*/
class AnimatedScrollBarsViewport  : public juce::Viewport
{
public:
    //==============================================================================
    AnimatedScrollBarsViewport(const juce::String& componentName = juce::String())
    : juce::Viewport(componentName)
    {
        setScrollBarsShown(false, false, true, true);

        scrollBarListener.updateFunction = [=](juce::ScrollBar* bar, double newRangeStart)
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

        for (auto bar : { &hBar, &vBar })
        {
            addChildComponent(bar);
            bar->addListener(&scrollBarListener);
        }

        addMouseListener(this, true);
    }

    //==============================================================================
    /** Viewport */

    void resized() override
    {
        juce::Viewport::resized();
        updateBars();
    }

    void mouseEnter(const juce::MouseEvent& e) override
    {
        if(!isMouseOver)
        {
            isMouseOver = true;
            for (auto bar : { &hBar, &vBar })
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
            for (auto bar : { &hBar, &vBar })
            {
                animator.fadeOut(bar, 400);
            }
        }
    }

    void visibleAreaChanged(const juce::Rectangle<int>& newVisibleArea) override
    {
        updateBars();
    }

private:
    //==============================================================================
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
    
    juce::ScrollBar hBar { false };
    juce::ScrollBar vBar { true };
    ScrollBarListenerImpl scrollBarListener;
    juce::ComponentAnimator animator;
    bool isMouseOver = false;
};