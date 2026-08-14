#include "MSEGDataStore.h"

static_assert(std::atomic<int>::is_always_lock_free);
static_assert(std::atomic<std::uint64_t>::is_always_lock_free);

MSEGDataStore::MSEGDataStore()
{
    initialiseData(editorData);
    initialiseData(pendingEditorData);

    for (auto& slot : snapshotSlots)
    {
        initialiseData(slot);
        copyData(slot, editorData);
    }

    for (size_t i = 0; i < snapshotSlots.size(); ++i)
    {
        snapshotVersions[i].store(1, std::memory_order_relaxed);
        snapshotUseStates[i].store(0, std::memory_order_relaxed);
    }
}

MSEGDataStore::~MSEGDataStore()
{
    cancelPendingUpdate();
}

void MSEGDataStore::initialiseData(gin::MSEG::Data& data)
{
    data.points.resize(data.maxPoints);
    data.reset();
}

void MSEGDataStore::copyData(gin::MSEG::Data& destination, const gin::MSEG::Data& source) noexcept
{
    jassert(destination.points.size() >= destination.maxPoints);

    const auto maxPoints = juce::jmin(destination.maxPoints, destination.points.size());
    destination.numPoints = juce::jlimit(2, maxPoints, source.numPoints);
    destination.startIndex = juce::jlimit(0, destination.numPoints - 2, source.startIndex);
    destination.endIndex = juce::jlimit(destination.startIndex + 1,
                                        destination.numPoints - 1,
                                        source.endIndex);

    for (int i = 0; i < destination.numPoints; ++i)
        destination.points.setUnchecked(i, source.points.getUnchecked(i));
}

gin::MSEG::Data MSEGDataStore::dataFromValueTree(const juce::ValueTree& tree)
{
    gin::MSEG::Data result;
    initialiseData(result);

    const auto numPoints = tree.getNumChildren();
    if (!tree.isValid() || numPoints < 2 || numPoints > result.maxPoints)
        return result;

    result.numPoints = numPoints;
    for (int i = 0; i < numPoints; ++i)
    {
        const auto point = tree.getChild(i);
        result.points.setUnchecked(i, {
            static_cast<float>(point.getProperty("t")),
            static_cast<float>(point.getProperty("v")),
            static_cast<float>(point.getProperty("c")),
        });
    }

    result.startIndex = juce::jlimit(0, numPoints - 2,
                                     static_cast<int>(tree.getProperty("startIndex", 0)));
    result.endIndex = juce::jlimit(result.startIndex + 1, numPoints - 1,
                                   static_cast<int>(tree.getProperty("endIndex", numPoints - 1)));
    return result;
}

std::uint64_t MSEGDataStore::publishLocked(const gin::MSEG::Data& source) noexcept
{
    const auto currentSlot = publishedSlot.load(std::memory_order_acquire);

    int targetSlot = -1;
    for (int i = 0; i < numSnapshotSlots; ++i)
    {
        if (i == currentSlot)
            continue;

        auto expected = 0;
        if (snapshotUseStates[static_cast<size_t>(i)].compare_exchange_strong(
                expected, -1, std::memory_order_acq_rel, std::memory_order_acquire))
        {
            targetSlot = i;
            break;
        }
    }

    jassert(targetSlot >= 0);
    if (targetSlot < 0)
        return publishedVersion.load(std::memory_order_relaxed);

    copyData(snapshotSlots[static_cast<size_t>(targetSlot)], source);
    const auto version = publishedVersion.load(std::memory_order_relaxed) + 1;
    snapshotVersions[static_cast<size_t>(targetSlot)].store(version, std::memory_order_relaxed);
    snapshotUseStates[static_cast<size_t>(targetSlot)].store(0, std::memory_order_release);
    publishedSlot.store(targetSlot, std::memory_order_release);
    publishedVersion.store(version, std::memory_order_release);
    return version;
}

void MSEGDataStore::commitEditorData()
{
    jassert(juce::MessageManager::getInstanceWithoutCreating() == nullptr
            || juce::MessageManager::getInstanceWithoutCreating()->isThisTheMessageThread());

    const std::scoped_lock lock(controlMutex);
    static_cast<void>(publishLocked(editorData));
}

void MSEGDataStore::replaceFromValueTree(const juce::ValueTree& tree)
{
    auto replacement = dataFromValueTree(tree);

    {
        const std::scoped_lock lock(controlMutex);
        pendingEditorVersion = publishLocked(replacement);
        copyData(pendingEditorData, replacement);
        hasPendingEditorData = true;
    }

    auto* messageManager = juce::MessageManager::getInstanceWithoutCreating();
    if (messageManager == nullptr || messageManager->isThisTheMessageThread())
        replaceEditorDataIfCurrent();
    else
        triggerAsyncUpdate();
}

void MSEGDataStore::resetToDefault()
{
    replaceFromValueTree({});
}

void MSEGDataStore::writeToValueTree(juce::ValueTree& tree)
{
    const std::scoped_lock lock(controlMutex);
    snapshotSlots[static_cast<size_t>(publishedSlot.load(std::memory_order_acquire))].toValueTree(tree);
}

bool MSEGDataStore::copyLatestAudioData(gin::MSEG::Data& destination,
                                        std::uint64_t& version) const noexcept
{
    const auto slot = publishedSlot.load(std::memory_order_acquire);
    auto expected = 0;
    if (!snapshotUseStates[static_cast<size_t>(slot)].compare_exchange_strong(
            expected, 1, std::memory_order_acq_rel, std::memory_order_acquire))
    {
        return false;
    }

    copyData(destination, snapshotSlots[static_cast<size_t>(slot)]);
    version = snapshotVersions[static_cast<size_t>(slot)].load(std::memory_order_acquire);
    snapshotUseStates[static_cast<size_t>(slot)].store(0, std::memory_order_release);
    return true;
}

void MSEGDataStore::synchroniseEditorData()
{
    jassert(juce::MessageManager::getInstanceWithoutCreating() == nullptr
            || juce::MessageManager::getInstanceWithoutCreating()->isThisTheMessageThread());
    replaceEditorDataIfCurrent();
}

void MSEGDataStore::replaceEditorDataIfCurrent()
{
    bool editorChanged = false;
    {
        const std::scoped_lock lock(controlMutex);
        if (hasPendingEditorData
            && pendingEditorVersion == publishedVersion.load(std::memory_order_acquire))
        {
            copyData(editorData, pendingEditorData);
            editorChanged = true;
        }

        hasPendingEditorData = false;
    }

    if (editorChanged)
        listeners.call([](Listener& listener) { listener.msegEditorDataChanged(); });
}

void MSEGDataStore::handleAsyncUpdate()
{
    replaceEditorDataIfCurrent();
}
