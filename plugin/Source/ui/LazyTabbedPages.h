#pragma once

#include <functional>
#include <memory>

#include <gin_plugin/gin_plugin.h>

/**
 * Controller that keeps exactly one page of a tabbed group alive.
 *
 * The classic gin pattern builds every sibling page up front and lets each
 * ParamBox show or hide itself from the shared header-select parameter, so
 * hidden pages permanently hold components, parameter listeners and
 * mod-matrix listeners. This controller instead builds only the selected
 * page and rebuilds on tab switches: the deselected page is torn down, so a
 * hidden tab costs nothing. All control state lives in the parameters, which
 * makes a rebuilt page identical to the one it replaces.
 *
 * The swap runs inside a single AsyncUpdater callback on the message thread:
 * the outgoing page is destroyed and the incoming page created before any
 * repaint can run. (The classic pattern's hide and show already straddle two
 * async callbacks, so this is strictly tighter.)
 *
 * Destruction of the outgoing page is delegated to the Disposer because a
 * page can be the source of an in-flight drag-and-drop: gin's header tab
 * buttons switch tabs on drag-hover, so the drag's source component may sit
 * inside the page being deselected, and deleting it would cancel the drag.
 * The owner can park such pages until the drag completes; a parked page has
 * already been removed from its parent here, so it can never paint or be
 * hit-tested again even though its own header logic may still toggle its
 * visibility flag.
 */
class LazyTabbedPages final : private gin::Parameter::ParameterListener,
                              private juce::AsyncUpdater
{
public:
    /** Builds the page for one tab index, fully positioned and configured. */
    using Factory = std::function<std::unique_ptr<juce::Component>(int pageIndex)>;

    /** Takes ownership of a deselected page. May defer the actual deletion. */
    using Disposer = std::function<void(std::unique_ptr<juce::Component>)>;

    LazyTabbedPages(juce::Component& parentComponent, gin::Parameter::Ptr selectParameter,
                    Factory pageFactory, Disposer pageDisposer)
        : parent(parentComponent), selectParam(selectParameter),
          factory(std::move(pageFactory)), disposer(std::move(pageDisposer))
    {
        jassert(selectParam != nullptr && factory != nullptr && disposer != nullptr);

        // Registering before the page is built keeps the notification order
        // "controller first, page second" for every page this controller
        // ever creates: the swap always runs before the outgoing page's own
        // (now cancelled) parameter reaction.
        selectParam->addListener(this);
        createPageForCurrentSelection();
    }

    ~LazyTabbedPages() override
    {
        cancelPendingUpdate();
        selectParam->removeListener(this);
    }

    juce::Component* getCurrentPage() const noexcept { return page.get(); }
    int getCurrentIndex() const noexcept { return currentIndex; }

private:
    void valueUpdated(gin::Parameter*) override
    {
        triggerAsyncUpdate();
    }

    void handleAsyncUpdate() override
    {
        if (selectParam->getUserValueInt() == currentIndex)
            return;

        if (page != nullptr)
        {
            // A user-driven switch can never have an open menu (the click
            // that switched tabs dismissed it), so this only fires on
            // parameter-driven switches such as preset loads - where a menu
            // targeting the vanishing page must not outlive it.
            juce::PopupMenu::dismissAllActiveMenus();

            page->setVisible(false);
            parent.removeChildComponent(page.get());
            disposer(std::move(page));
        }

        createPageForCurrentSelection();
    }

    void createPageForCurrentSelection()
    {
        currentIndex = selectParam->getUserValueInt();
        page = factory(currentIndex);
        jassert(page != nullptr);

        // Tab pages never overlap their always-alive siblings, so they join
        // the back of the z-order and existing overlays stay above pages
        // created mid-session.
        parent.addChildComponent(page.get(), 0);

        // JUCE only delivers lookAndFeelChanged() through an explicit
        // setLookAndFeel()/sendLookAndFeelChange(); the editor's initial
        // children received that pass from the enclosing wrapper after
        // construction. Pages built mid-session need the same pass, because
        // components such as SVG buttons and glow layers configure their
        // colours only in lookAndFeelChanged().
        page->sendLookAndFeelChange();

        page->setVisible(true);
    }

    juce::Component& parent;
    gin::Parameter::Ptr selectParam;
    Factory factory;
    Disposer disposer;
    std::unique_ptr<juce::Component> page;
    int currentIndex = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LazyTabbedPages)
};
