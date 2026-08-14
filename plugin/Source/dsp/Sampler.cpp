#include "Sampler.h"

#include <chrono>
#include <exception>
#include <new>
#include <utility>

SampleSnapshot::SampleSnapshot(juce::AudioBuffer<float>&& stereoAudio,
                               double sourceSampleRate,
                               std::uint64_t snapshotGeneration,
                               juce::String sourcePath,
                               juce::String sourceName) noexcept
    : audio(std::move(stereoAudio)),
      fileSampleRate(sourceSampleRate),
      generation(snapshotGeneration),
      filePath(std::move(sourcePath)),
      sampleName(std::move(sourceName))
{
    jassert(audio.getNumChannels() == 2);
}

Sampler::Sampler()
{
    formatManager.registerBasicFormats();
    worker = std::thread([this] { workerLoop(); });
}

Sampler::~Sampler()
{
    {
        const std::scoped_lock lock(requestMutex);
        stopWorker.store(true, std::memory_order_release);
        newestRequest.reset();
    }
    requestCondition.notify_one();

    if (worker.joinable())
        worker.join();

    // Processing and all voices have stopped before GlobalState destruction. It
    // is therefore safe to finish ownership cleanup on this non-audio thread.
    Publication publication;
    while (publications.pop(publication))
        delete publication.snapshot;

    SampleSnapshot* retired = nullptr;
    while (reclaims.pop(retired))
        delete retired;

    delete activeSnapshot;
}

void Sampler::prepare(const juce::dsp::ProcessSpec& spec) noexcept
{
    sampleRate = spec.sampleRate;
}

bool Sampler::requestLoadFile(const juce::File& file, CompletionCallback completion)
{
    try
    {
        Request request;
        request.kind = RequestKind::loadFile;
        request.file = file;
        request.desiredStatePath = file.getFullPathName();
        request.updatesStatePath = true;
        request.completion = std::move(completion);
        enqueueRequest(std::move(request));
        return true;
    }
    catch (...)
    {
        telemetry.workerFailures.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
}

void Sampler::requestClear(CompletionCallback completion)
{
    try
    {
        Request request;
        request.kind = RequestKind::clear;
        request.updatesStatePath = true;
        request.completion = std::move(completion);
        enqueueRequest(std::move(request));
    }
    catch (...)
    {
        telemetry.workerFailures.fetch_add(1, std::memory_order_relaxed);
    }
}

bool Sampler::requestInstallForTesting(const juce::AudioBuffer<float>& source,
                                       double sourceSampleRate,
                                       CompletionCallback completion)
{
    if (source.getNumChannels() <= 0 || source.getNumSamples() <= 0
        || !std::isfinite(sourceSampleRate) || sourceSampleRate <= 0.0)
        return false;

    if (!isDecodedLengthSupported(source.getNumSamples()))
    {
        telemetry.oversizedRejected.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    try
    {
        Request request;
        request.kind = RequestKind::installForTesting;
        request.testAudio = std::make_unique<juce::AudioBuffer<float>>(source);
        request.testSampleRate = sourceSampleRate;
        request.completion = std::move(completion);
        enqueueRequest(std::move(request));
        return true;
    }
    catch (...)
    {
        telemetry.workerFailures.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
}

void Sampler::enqueueRequest(Request request)
{
    std::optional<Request> superseded;
    {
        const std::scoped_lock lock(requestMutex);
        request.generation = newestRequestedGeneration.fetch_add(1, std::memory_order_acq_rel) + 1;
        telemetry.requests.fetch_add(1, std::memory_order_relaxed);
        if (newestRequest.has_value())
        {
            superseded.emplace(std::move(*newestRequest));
            telemetry.coalesced.fetch_add(1, std::memory_order_relaxed);
        }
        newestRequest.emplace(std::move(request));

        // State serialization follows the exact same total order as request
        // generations. Keeping both updates under requestMutex prevents two
        // control producers from publishing path B while request A is actually
        // the newest request. Audio never takes either mutex.
        if (newestRequest->updatesStatePath)
        {
            const std::scoped_lock displayLock(displayMutex);
            statePath = std::move(newestRequest->desiredStatePath);
        }
    }

    if (superseded.has_value() && superseded->completion)
    {
        LoadResult result;
        result.generation = superseded->generation;
        result.superseded = true;
        setFailureMessageNoThrow(result, "Superseded by a newer sample request");
        invokeCompletionNoThrow(superseded->completion, std::move(result));
    }

    requestCondition.notify_one();
}

void Sampler::workerLoop()
{
    for (;;)
    {
        drainReclaimsOnWorker();

        std::optional<Request> request;
        {
            std::unique_lock lock(requestMutex);
            requestCondition.wait_for(lock, std::chrono::milliseconds(5), [this] {
                return stopWorker.load(std::memory_order_acquire) || newestRequest.has_value();
            });

            if (stopWorker.load(std::memory_order_acquire))
                break;

            if (newestRequest.has_value())
            {
                request.emplace(std::move(*newestRequest));
                newestRequest.reset();
            }
        }

        if (!request.has_value())
            continue;

        LoadResult result;
        result.generation = request->generation;
        std::unique_ptr<SampleSnapshot> snapshot;

        const auto publishFailedLoadAsSilence = [&]() noexcept
        {
            if (request->kind != RequestKind::loadFile
                || request->generation
                       != newestRequestedGeneration.load(std::memory_order_acquire))
                return;

            try
            {
                std::unique_ptr<SampleSnapshot> emptySnapshot;
                static_cast<void>(publishFromWorker(emptySnapshot, *request, result));
            }
            catch (...)
            {
                telemetry.workerFailures.fetch_add(1, std::memory_order_relaxed);
            }
        };

        try
        {
            snapshot = decodeRequest(*request, result);

            if (request->generation != newestRequestedGeneration.load(std::memory_order_acquire))
            {
                telemetry.coalesced.fetch_add(1, std::memory_order_relaxed);
                result.success = false;
                result.superseded = true;
                setFailureMessageNoThrow(result, "Superseded by a newer sample request");
                if (snapshot != nullptr)
                {
                    snapshot.reset();
                    telemetry.destroyedOffAudioThread.fetch_add(1, std::memory_order_relaxed);
                }
            }
            else if (request->kind == RequestKind::clear || snapshot != nullptr)
            {
                result.success = publishFromWorker(snapshot, *request, result);
            }
            else if (request->kind == RequestKind::loadFile)
            {
                // The newest requested file could not be decoded. Publish an
                // empty generation so new assignments are silent, while
                // existing voice handles retain the previous immutable sample.
                // Keep result.success false so the control/UI side reports the
                // decode failure instead of mistaking the clear for a load.
                static_cast<void>(publishFromWorker(snapshot, *request, result));
            }
        }
        catch (const std::bad_alloc&)
        {
            result.success = false;
            telemetry.workerFailures.fetch_add(1, std::memory_order_relaxed);
            setFailureMessageNoThrow(result, "Not enough memory to decode this sample");
            publishFailedLoadAsSilence();
        }
        catch (const std::exception&)
        {
            result.success = false;
            telemetry.workerFailures.fetch_add(1, std::memory_order_relaxed);
            setFailureMessageNoThrow(result, "The sample decoder failed");
            publishFailedLoadAsSilence();
        }
        catch (...)
        {
            result.success = false;
            telemetry.workerFailures.fetch_add(1, std::memory_order_relaxed);
            setFailureMessageNoThrow(result, "The sample decoder failed unexpectedly");
            publishFailedLoadAsSilence();
        }

        if (snapshot != nullptr)
        {
            snapshot.reset();
            telemetry.destroyedOffAudioThread.fetch_add(1, std::memory_order_relaxed);
        }

        invokeCompletionNoThrow(request->completion, std::move(result));
    }

    drainReclaimsOnWorker();
}

std::unique_ptr<SampleSnapshot> Sampler::decodeRequest(const Request& request,
                                                       LoadResult& result)
{
    if (request.kind == RequestKind::clear)
        return nullptr;

    juce::AudioBuffer<float> stereo;
    double sourceSampleRate = request.testSampleRate;
    juce::String path;
    juce::String name;

    if (request.kind == RequestKind::installForTesting)
    {
        if (request.testAudio == nullptr)
        {
            result.error = "No test sample data";
            return nullptr;
        }

        stereo = makeStereoCopy(*request.testAudio);
        name = "In-memory sample";
    }
    else
    {
        std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(request.file));
        if (reader == nullptr)
        {
            result.error = "Unsupported or unreadable audio file";
            return nullptr;
        }

        if (!isDecodedLengthSupported(reader->lengthInSamples))
        {
            telemetry.oversizedRejected.fetch_add(1, std::memory_order_relaxed);
            result.error = "Sample exceeds the 256 MiB decoded stereo limit";
            return nullptr;
        }

        stereo.setSize(2, static_cast<int>(reader->lengthInSamples), false, false, true);
        if (!reader->read(&stereo, 0, stereo.getNumSamples(), 0, true, true))
        {
            result.error = "Could not decode audio file";
            return nullptr;
        }

        sourceSampleRate = reader->sampleRate;
        path = request.file.getFullPathName();
        name = request.file.getFileName();
    }

    if (stereo.getNumSamples() <= 0)
    {
        result.error = "Decoded sample is empty";
        return nullptr;
    }

    telemetry.decoded.fetch_add(1, std::memory_order_relaxed);
    result.filePath = path;
    result.sampleName = name;
    return std::unique_ptr<SampleSnapshot>(new SampleSnapshot(std::move(stereo),
                                                              sourceSampleRate,
                                                              request.generation,
                                                              std::move(path),
                                                              std::move(name)));
}

bool Sampler::publishFromWorker(std::unique_ptr<SampleSnapshot>& snapshot,
                                const Request& request,
                                LoadResult& result)
{
    Publication publication { snapshot.get(), request.generation };

    // Copy allocating metadata while the snapshot is still locally owned. It is
    // moved into the UI snapshot only after audio ownership is published.
    auto newDisplayInfo = makeDisplayInfo(publication.snapshot, request.generation);

    for (;;)
    {
        bool superseded = false;
        bool published = false;
        {
            // Request producers increment newestRequestedGeneration while
            // holding this same control/worker mutex. Keep the final freshness
            // check and SPSC publication indivisible with respect to enqueue so
            // a newer request cannot arrive in the gap and let a stale decode
            // reach the audio thread. This mutex is never touched by audio.
            const std::scoped_lock lock(requestMutex);
            superseded = request.generation
                         != newestRequestedGeneration.load(std::memory_order_relaxed);
            if (!superseded)
                published = publications.push(publication);
        }

        if (superseded)
        {
            telemetry.coalesced.fetch_add(1, std::memory_order_relaxed);
            result.superseded = true;
            result.error = "Superseded before publication";
            if (snapshot != nullptr)
            {
                snapshot.reset();
                telemetry.destroyedOffAudioThread.fetch_add(1, std::memory_order_relaxed);
            }
            return false;
        }

        if (published)
            break;

        drainReclaimsOnWorker();

        if (stopWorker.load(std::memory_order_acquire))
        {
            result.error = "Sampler is shutting down";
            if (snapshot != nullptr)
            {
                snapshot.reset();
                telemetry.destroyedOffAudioThread.fetch_add(1, std::memory_order_relaxed);
            }
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    snapshot.release();
    telemetry.published.fetch_add(1, std::memory_order_relaxed);
    updateDisplayInfoFromWorker(std::move(newDisplayInfo));
    return true;
}

void Sampler::invokeCompletionNoThrow(CompletionCallback& completion,
                                      LoadResult result) noexcept
{
    if (!completion)
        return;

    try
    {
        completion(std::move(result));
    }
    catch (...)
    {
        telemetry.completionFailures.fetch_add(1, std::memory_order_relaxed);
    }
}

void Sampler::setFailureMessageNoThrow(LoadResult& result, const char* message) noexcept
{
    try
    {
        result.error = message;
    }
    catch (...)
    {
        // Reporting must never let a secondary allocation failure escape the
        // sampler worker's exception boundary.
    }
}

Sampler::DisplayInfo Sampler::makeDisplayInfo(const SampleSnapshot* snapshot,
                                              std::uint64_t clearGeneration)
{
    DisplayInfo info;
    info.generation = snapshot != nullptr ? snapshot->getGeneration() : clearGeneration;

    if (snapshot != nullptr)
    {
        info.filePath = snapshot->getFilePath();
        info.sampleName = snapshot->getSampleName();
        info.numSamples = snapshot->getNumSamples();
        info.numChannels = snapshot->getNumChannels();
        info.fileSampleRate = snapshot->getSampleRate();
        info.loaded = true;
    }

    return info;
}

void Sampler::updateDisplayInfoFromWorker(DisplayInfo info) noexcept
{
    try
    {
        const std::scoped_lock lock(displayMutex);
        displayInfo = std::move(info);
        displayGeneration.store(displayInfo.generation, std::memory_order_release);
    }
    catch (...)
    {
        // The audio publication remains authoritative. A later successful
        // publication will refresh display metadata if mutex acquisition fails.
    }
}

bool Sampler::commitPendingAtBlockBoundary() noexcept
{
    Publication publication;
    Publication selected;
    bool found = false;

    while (publications.pop(publication))
    {
        if (found && selected.snapshot != nullptr)
            queueReclaimFromAudioThread(selected.snapshot);

        selected = publication;
        found = true;
    }

    if (!found)
        return false;

    if (activeSnapshot != nullptr)
        retireActiveFromAudioThread(activeSnapshot);

    activeSnapshot = selected.snapshot;
    if (activeSnapshot != nullptr)
    {
        activeSnapshot->retiredFromActive = false;
        activeSnapshot->reclaimQueued = false;
    }

    telemetry.activeGeneration.store(selected.generation, std::memory_order_release);
    telemetry.committed.fetch_add(1, std::memory_order_relaxed);
    return true;
}

Sampler::AudioHandle Sampler::acquireForVoice() noexcept
{
    if (activeSnapshot == nullptr)
        return {};

    ++activeSnapshot->voiceReferenceCount;
    return { activeSnapshot, activeSnapshot->getGeneration() };
}

void Sampler::releaseFromVoice(AudioHandle& handle) noexcept
{
    if (handle.snapshot == nullptr)
        return;

    auto* snapshot = const_cast<SampleSnapshot*>(handle.snapshot);
    jassert(snapshot->generation == handle.generation);
    jassert(snapshot->voiceReferenceCount > 0);

    if (snapshot->voiceReferenceCount > 0)
        --snapshot->voiceReferenceCount;

    handle = {};

    if (snapshot->voiceReferenceCount == 0 && snapshot->retiredFromActive)
        queueReclaimFromAudioThread(snapshot);
}

void Sampler::retireActiveFromAudioThread(SampleSnapshot* snapshot) noexcept
{
    snapshot->retiredFromActive = true;
    if (snapshot->voiceReferenceCount == 0)
        queueReclaimFromAudioThread(snapshot);
}

void Sampler::queueReclaimFromAudioThread(SampleSnapshot* snapshot) noexcept
{
    if (snapshot == nullptr || snapshot->reclaimQueued)
        return;

    // Mark before publishing the pointer. The worker is allowed to pop and
    // delete immediately after the queue's release-store becomes visible.
    snapshot->reclaimQueued = true;
    if (reclaims.push(snapshot))
    {
        telemetry.reclaimed.fetch_add(1, std::memory_order_relaxed);
    }
    else
    {
        snapshot->reclaimQueued = false;
        // Capacity is deliberately greater than the maximum 18 voice-held
        // versions plus the eight pending publications. An overflow indicates a
        // violated lifecycle invariant; never destroy on the audio thread.
        telemetry.reclaimOverflows.fetch_add(1, std::memory_order_relaxed);
        jassertfalse;
    }
}

void Sampler::drainReclaimsOnWorker() noexcept
{
    SampleSnapshot* snapshot = nullptr;
    while (reclaims.pop(snapshot))
    {
        delete snapshot;
        telemetry.destroyedOffAudioThread.fetch_add(1, std::memory_order_relaxed);
    }
}

juce::AudioBuffer<float> Sampler::makeStereoCopy(const juce::AudioBuffer<float>& source)
{
    const auto numSamples = source.getNumSamples();
    juce::AudioBuffer<float> stereo(2, numSamples);
    stereo.clear();

    if (source.getNumChannels() > 0 && numSamples > 0)
    {
        stereo.copyFrom(0, 0, source, 0, 0, numSamples);
        stereo.copyFrom(1, 0, source, source.getNumChannels() > 1 ? 1 : 0, 0, numSamples);
    }

    return stereo;
}

Sampler::DisplayInfo Sampler::getDisplayInfo() const
{
    const std::scoped_lock lock(displayMutex);
    return displayInfo;
}

juce::String Sampler::getStatePath() const
{
    const std::scoped_lock lock(displayMutex);
    return statePath;
}

Sampler::Telemetry Sampler::getTelemetry() const noexcept
{
    Telemetry result;
    result.requests = telemetry.requests.load(std::memory_order_relaxed);
    result.decoded = telemetry.decoded.load(std::memory_order_relaxed);
    result.published = telemetry.published.load(std::memory_order_relaxed);
    result.committed = telemetry.committed.load(std::memory_order_relaxed);
    result.coalesced = telemetry.coalesced.load(std::memory_order_relaxed);
    result.reclaimed = telemetry.reclaimed.load(std::memory_order_relaxed);
    result.destroyedOffAudioThread = telemetry.destroyedOffAudioThread.load(std::memory_order_relaxed);
    result.reclaimOverflows = telemetry.reclaimOverflows.load(std::memory_order_relaxed);
    result.oversizedRejected = telemetry.oversizedRejected.load(std::memory_order_relaxed);
    result.workerFailures = telemetry.workerFailures.load(std::memory_order_relaxed);
    result.completionFailures = telemetry.completionFailures.load(std::memory_order_relaxed);
    result.activeGeneration = telemetry.activeGeneration.load(std::memory_order_acquire);
    return result;
}
