#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>

#include <gin_dsp/gin_dsp.h>
#include <juce_events/juce_events.h>

/**
 * Owns the editable MSEG curve and publishes immutable, versioned copies to the
 * audio thread. The message thread is the only writer of editorData; preset
 * loads may originate on another control thread and are marshalled back before
 * touching it. Audio consumers copy from a pinned published slot into their own
 * preallocated gin::MSEG::Data, so neither locks nor allocations are needed in
 * process().
 */
class MSEGDataStore : private juce::AsyncUpdater
{
public:
    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void msegEditorDataChanged() = 0;
    };

    MSEGDataStore();
    ~MSEGDataStore() override;

    [[nodiscard]] gin::MSEG::Data& getEditorData() noexcept { return editorData; }

    /** Applies a pending preset copy before a component binds to editorData. */
    void synchroniseEditorData();

    /** Publishes the current message-thread-owned editor copy. */
    void commitEditorData();

    /** Publishes preset data immediately and updates editorData on the message thread. */
    void replaceFromValueTree(const juce::ValueTree& tree);
    void resetToDefault();
    void writeToValueTree(juce::ValueTree& tree);

    [[nodiscard]] std::uint64_t getPublishedVersion() const noexcept
    {
        return publishedVersion.load(std::memory_order_acquire);
    }

    /**
     * Attempts one bounded copy of a stable published snapshot into
     * preallocated audio-owned storage. A failed pin leaves destination and
     * version unchanged so the caller can defer until its next process call.
     */
    [[nodiscard]] bool copyLatestAudioData(gin::MSEG::Data& destination,
                                           std::uint64_t& version) const noexcept;

    void addListener(Listener* listener) { listeners.add(listener); }
    void removeListener(Listener* listener) { listeners.remove(listener); }

private:
    static constexpr int numSnapshotSlots = 3;

    static void initialiseData(gin::MSEG::Data& data);
    static void copyData(gin::MSEG::Data& destination, const gin::MSEG::Data& source) noexcept;
    static gin::MSEG::Data dataFromValueTree(const juce::ValueTree& tree);

    [[nodiscard]] std::uint64_t publishLocked(const gin::MSEG::Data& source) noexcept;
    void replaceEditorDataIfCurrent();
    void handleAsyncUpdate() override;

    gin::MSEG::Data editorData;
    gin::MSEG::Data pendingEditorData;
    std::array<gin::MSEG::Data, numSnapshotSlots> snapshotSlots;
    std::array<std::atomic<std::uint64_t>, numSnapshotSlots> snapshotVersions {};
    mutable std::array<std::atomic<int>, numSnapshotSlots> snapshotUseStates {};

    mutable std::mutex controlMutex;
    std::atomic<int> publishedSlot { 0 };
    std::atomic<std::uint64_t> publishedVersion { 1 };
    std::uint64_t pendingEditorVersion = 0;
    bool hasPendingEditorData = false;

    juce::ListenerList<Listener> listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MSEGDataStore)
};
