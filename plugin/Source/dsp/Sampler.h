#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <condition_variable>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <type_traits>

/** Immutable, always-stereo sample data published by Sampler.

    A SampleSnapshot is constructed and destroyed by Sampler's loader thread. The
    audio thread may only read it and modify the private audio-thread-only lifetime
    fields through Sampler.
*/
class SampleSnapshot final
{
public:
    [[nodiscard]] int getNumSamples() const noexcept { return audio.getNumSamples(); }
    [[nodiscard]] int getNumChannels() const noexcept { return 2; }
    [[nodiscard]] double getSampleRate() const noexcept { return fileSampleRate; }
    [[nodiscard]] std::uint64_t getGeneration() const noexcept { return generation; }
    [[nodiscard]] const juce::String& getFilePath() const noexcept { return filePath; }
    [[nodiscard]] const juce::String& getSampleName() const noexcept { return sampleName; }

    [[nodiscard]] const float* getReadPointer(int channel) const noexcept
    {
        return audio.getReadPointer(juce::jlimit(0, 1, channel));
    }

    [[nodiscard]] float getSample(int channel, int position) const noexcept
    {
        if (position < 0 || position >= audio.getNumSamples())
            return 0.0f;

        return audio.getSample(juce::jlimit(0, 1, channel), position);
    }

private:
    friend class Sampler;

    SampleSnapshot(juce::AudioBuffer<float>&& stereoAudio,
                   double sourceSampleRate,
                   std::uint64_t snapshotGeneration,
                   juce::String sourcePath,
                   juce::String sourceName) noexcept;

    juce::AudioBuffer<float> audio;
    double fileSampleRate = 0.0;
    std::uint64_t generation = 0;
    juce::String filePath;
    juce::String sampleName;

    // Audio-thread-owned lifetime state. These are deliberately non-atomic.
    std::uint32_t voiceReferenceCount = 0;
    bool retiredFromActive = false;
    bool reclaimQueued = false;
};

class Sampler final
{
public:
    static constexpr std::size_t publicationQueueCapacity = 8;
    static constexpr std::size_t reclaimQueueCapacity = 64;
    static constexpr std::uint64_t maxDecodedBytes = 256ULL * 1024ULL * 1024ULL;
    static constexpr std::int64_t maxDecodedFrames =
        static_cast<std::int64_t>(maxDecodedBytes / (2ULL * sizeof(float)));

    [[nodiscard]] static constexpr bool isDecodedLengthSupported(std::int64_t frames) noexcept
    {
        return frames > 0 && frames <= maxDecodedFrames;
    }

    struct AudioHandle final
    {
        const SampleSnapshot* snapshot = nullptr;
        std::uint64_t generation = 0;

        [[nodiscard]] explicit operator bool() const noexcept { return snapshot != nullptr; }
        [[nodiscard]] int getNumSamples() const noexcept
        {
            return snapshot != nullptr ? snapshot->getNumSamples() : 0;
        }
        [[nodiscard]] int numSamples() const noexcept { return getNumSamples(); }
        [[nodiscard]] const float* getReadPointer(int channel) const noexcept
        {
            return snapshot != nullptr ? snapshot->getReadPointer(channel) : nullptr;
        }
        [[nodiscard]] const float* left() const noexcept { return getReadPointer(0); }
        [[nodiscard]] const float* right() const noexcept { return getReadPointer(1); }
        [[nodiscard]] float getSample(int channel, int position) const noexcept
        {
            return snapshot != nullptr ? snapshot->getSample(channel, position) : 0.0f;
        }
    };

    static_assert(std::is_trivially_copyable_v<AudioHandle>);

    struct DisplayInfo final
    {
        juce::String filePath;
        juce::String sampleName;
        int numSamples = 0;
        int numChannels = 0;
        double fileSampleRate = 0.0;
        std::uint64_t generation = 0;
        bool loaded = false;
    };

    struct LoadResult final
    {
        std::uint64_t generation = 0;
        bool success = false;
        bool superseded = false;
        juce::String filePath;
        juce::String sampleName;
        juce::String error;
    };

    struct Telemetry final
    {
        std::uint64_t requests = 0;
        std::uint64_t decoded = 0;
        std::uint64_t published = 0;
        std::uint64_t committed = 0;
        std::uint64_t coalesced = 0;
        std::uint64_t reclaimed = 0;
        std::uint64_t destroyedOffAudioThread = 0;
        std::uint64_t reclaimOverflows = 0;
        std::uint64_t oversizedRejected = 0;
        std::uint64_t workerFailures = 0;
        std::uint64_t completionFailures = 0;
        std::uint64_t activeGeneration = 0;
    };

    using CompletionCallback = std::function<void(LoadResult)>;

    Sampler();
    ~Sampler();

    void prepare(const juce::dsp::ProcessSpec& spec) noexcept;

    /** Queue file decoding on the single sampler worker. Completion never runs
        on the audio thread, but may run on the requester while coalescing or on
        the worker; UI callers must marshal it to the message thread. */
    bool requestLoadFile(const juce::File& file, CompletionCallback completion = {});
    void requestClear(CompletionCallback completion = {});

    /** Test/embedding seam. Input is copied on the calling (non-audio) thread,
        normalized to stereo by the worker, and published like a decoded file. */
    bool requestInstallForTesting(const juce::AudioBuffer<float>& source,
                                  double sourceSampleRate,
                                  CompletionCallback completion = {});

    /** Audio thread: commit only the newest queued publication at a host-block
        boundary. This method performs no allocation, destruction, or locking. */
    bool commitPendingAtBlockBoundary() noexcept;

    /** Audio thread: explicit snapshot lifetime for a full voice assignment.
        Retargets must retain the handle. Every successful acquisition must be
        paired with releaseFromVoice() at final retirement or before overwrite. */
    [[nodiscard]] AudioHandle acquireForVoice() noexcept;
    void releaseFromVoice(AudioHandle& handle) noexcept;

    [[nodiscard]] DisplayInfo getDisplayInfo() const;
    [[nodiscard]] std::uint64_t getDisplayGeneration() const noexcept
    {
        return displayGeneration.load(std::memory_order_acquire);
    }
    /** Control-thread state value. This tracks the newest requested file path
        independently of editor lifetime and remains available if decoding
        fails, so the unchanged samplePath schema can round-trip its intent. */
    [[nodiscard]] juce::String getStatePath() const;
    [[nodiscard]] Telemetry getTelemetry() const noexcept;
    [[nodiscard]] double getSampleRate() const noexcept { return sampleRate; }

private:
    enum class RequestKind : std::uint8_t
    {
        loadFile,
        clear,
        installForTesting
    };

    struct Request final
    {
        RequestKind kind = RequestKind::clear;
        std::uint64_t generation = 0;
        juce::File file;
        juce::String desiredStatePath;
        bool updatesStatePath = false;
        std::unique_ptr<juce::AudioBuffer<float>> testAudio;
        double testSampleRate = 0.0;
        CompletionCallback completion;
    };

    struct Publication final
    {
        SampleSnapshot* snapshot = nullptr;
        std::uint64_t generation = 0;
    };

    template <typename Item, std::size_t Capacity>
    class SpscQueue final
    {
    public:
        static_assert(Capacity > 0);

        [[nodiscard]] bool push(const Item& item) noexcept
        {
            const auto write = writeIndex.load(std::memory_order_relaxed);
            const auto next = increment(write);
            if (next == readIndex.load(std::memory_order_acquire))
                return false;

            items[write] = item;
            writeIndex.store(next, std::memory_order_release);
            return true;
        }

        [[nodiscard]] bool pop(Item& item) noexcept
        {
            const auto read = readIndex.load(std::memory_order_relaxed);
            if (read == writeIndex.load(std::memory_order_acquire))
                return false;

            item = items[read];
            readIndex.store(increment(read), std::memory_order_release);
            return true;
        }

    private:
        static constexpr std::size_t increment(std::size_t index) noexcept
        {
            return (index + 1) % (Capacity + 1);
        }

        std::array<Item, Capacity + 1> items {};
        alignas(64) std::atomic<std::size_t> writeIndex { 0 };
        alignas(64) std::atomic<std::size_t> readIndex { 0 };
    };

    struct AtomicTelemetry final
    {
        std::atomic<std::uint64_t> requests { 0 };
        std::atomic<std::uint64_t> decoded { 0 };
        std::atomic<std::uint64_t> published { 0 };
        std::atomic<std::uint64_t> committed { 0 };
        std::atomic<std::uint64_t> coalesced { 0 };
        std::atomic<std::uint64_t> reclaimed { 0 };
        std::atomic<std::uint64_t> destroyedOffAudioThread { 0 };
        std::atomic<std::uint64_t> reclaimOverflows { 0 };
        std::atomic<std::uint64_t> oversizedRejected { 0 };
        std::atomic<std::uint64_t> workerFailures { 0 };
        std::atomic<std::uint64_t> completionFailures { 0 };
        std::atomic<std::uint64_t> activeGeneration { 0 };
    };

    void enqueueRequest(Request request);
    void workerLoop();
    void drainReclaimsOnWorker() noexcept;
    [[nodiscard]] std::unique_ptr<SampleSnapshot> decodeRequest(const Request& request,
                                                                LoadResult& result);
    [[nodiscard]] bool publishFromWorker(std::unique_ptr<SampleSnapshot>& snapshot,
                                         const Request& request,
                                         LoadResult& result);
    [[nodiscard]] static DisplayInfo makeDisplayInfo(const SampleSnapshot* snapshot,
                                                     std::uint64_t clearGeneration);
    void updateDisplayInfoFromWorker(DisplayInfo info) noexcept;
    void queueReclaimFromAudioThread(SampleSnapshot* snapshot) noexcept;
    void retireActiveFromAudioThread(SampleSnapshot* snapshot) noexcept;
    void invokeCompletionNoThrow(CompletionCallback& completion, LoadResult result) noexcept;
    static void setFailureMessageNoThrow(LoadResult& result, const char* message) noexcept;
    static juce::AudioBuffer<float> makeStereoCopy(const juce::AudioBuffer<float>& source);

    juce::AudioFormatManager formatManager;
    double sampleRate = 44100.0;

    // Control/worker request handoff. This is never touched by the audio thread.
    std::mutex requestMutex;
    std::condition_variable requestCondition;
    std::optional<Request> newestRequest;
    std::atomic<bool> stopWorker { false };
    std::thread worker;
    std::atomic<std::uint64_t> newestRequestedGeneration { 0 };

    // Exactly one producer and one consumer for each bounded queue.
    SpscQueue<Publication, publicationQueueCapacity> publications;
    SpscQueue<SampleSnapshot*, reclaimQueueCapacity> reclaims;

    // Audio-thread-owned pointer and reference counts.
    SampleSnapshot* activeSnapshot = nullptr;

    mutable std::mutex displayMutex;
    DisplayInfo displayInfo;
    std::atomic<std::uint64_t> displayGeneration { 0 };
    juce::String statePath;
    AtomicTelemetry telemetry;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Sampler)
};
