#include "PluginProcessor.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <span>
#include <thread>

namespace
{
constexpr int editorCycles = 8;
constexpr int interCyclePumpMilliseconds = 10;
constexpr int visibleTimerPumpMilliseconds = 50;
constexpr int delayedCallbackPumpMilliseconds = 350;
constexpr int sampleRate = 48'000;
constexpr int blockSize = 128;
constexpr int snapshotStressBlocks = 256;
constexpr int visualPublicationRateHz = 30;

class LoadedWrapperScope
{
public:
    explicit LoadedWrapperScope(juce::AudioProcessor::WrapperType wrapper)
        : previous(juce::PluginHostType::getPluginLoadedAs())
    {
        juce::PluginHostType::jucePlugInClientCurrentWrapperType = wrapper;
        juce::AudioProcessor::setTypeOfNextNewPlugin(wrapper);
    }

    void processorConstructed()
    {
        juce::AudioProcessor::setTypeOfNextNewPlugin(
            juce::AudioProcessor::wrapperType_Undefined);
    }

    ~LoadedWrapperScope()
    {
        juce::AudioProcessor::setTypeOfNextNewPlugin(
            juce::AudioProcessor::wrapperType_Undefined);
        juce::PluginHostType::jucePlugInClientCurrentWrapperType = previous;
    }

private:
    juce::AudioProcessor::WrapperType previous;
};

bool pumpPendingTimersFor(int milliseconds)
{
    auto* messageManager = juce::MessageManager::getInstanceWithoutCreating();
    if (messageManager == nullptr || !messageManager->isThisTheMessageThread())
        return false;

    const auto deadline = std::chrono::steady_clock::now()
                          + std::chrono::milliseconds(milliseconds);
    do
    {
        juce::Timer::callPendingTimersSynchronously();
        juce::Thread::sleep(1);
    }
    while (std::chrono::steady_clock::now() < deadline);

    juce::Timer::callPendingTimersSynchronously();
    return true;
}

void renderNote(ResonariumProcessor& processor, int noteNumber)
{
    juce::AudioBuffer<float> audio(2, blockSize);
    audio.clear();
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(
                      2, noteNumber, static_cast<juce::uint8>(100)),
                  0);
    processor.processBlock(audio, midi);
}

void renderSilentBlock(ResonariumProcessor& processor)
{
    juce::AudioBuffer<float> audio(2, blockSize);
    audio.clear();
    juce::MidiBuffer midi;
    processor.processBlock(audio, midi);
}

bool validateVisualSnapshot(const ResonatorSynth::VisualSnapshot& snapshot,
                            const char*& failureReason)
{
    std::array<std::pair<int, float>, ResonatorSynth::getMaxEnvelopeVisualPhases()>
        envelopePhases {};
    std::array<float, ResonatorSynth::getMaxLFOVisualPhases()> lfoPhases {};
    std::array<float, ResonatorSynth::getMaxMSEGVisualPhases()> msegPhases {};

    const auto envelopeCount = snapshot.copyEnvelopePhases(0, envelopePhases);
    const auto lfoCount = snapshot.copyLFOPhases(0, lfoPhases);
    const auto msegCount = snapshot.copyMSEGPhases(0, msegPhases);
    const auto& telemetry = snapshot.getVoiceTelemetry();
    const auto activeVoiceCount = static_cast<std::size_t>(
        telemetry.musicalVoices + telemetry.fastFadeVoices);

    if (envelopeCount == 0 || envelopeCount != activeVoiceCount)
    {
        failureReason = "snapshot voice telemetry and envelope phase counts disagree";
        return false;
    }
    if (lfoCount != envelopeCount + 1 || msegCount != envelopeCount + 1)
    {
        failureReason = "snapshot mono/poly phase counts are incoherent";
        return false;
    }

    for (std::size_t index = 0; index < envelopeCount; ++index)
    {
        if (!std::isfinite(envelopePhases[index].second))
        {
            failureReason = "snapshot contains a non-finite envelope phase";
            return false;
        }
    }
    for (std::size_t index = 0; index < lfoCount; ++index)
    {
        if (!std::isfinite(lfoPhases[index]))
        {
            failureReason = "snapshot contains a non-finite LFO phase";
            return false;
        }
    }
    for (std::size_t index = 0; index < msegCount; ++index)
    {
        if (!std::isfinite(msegPhases[index]))
        {
            failureReason = "snapshot contains a non-finite MSEG phase";
            return false;
        }
    }

    constexpr float sentinel = -1234.5f;
    std::array<float, 2> boundedLFO { sentinel, sentinel };
    std::array<float, 2> boundedMSEG { sentinel, sentinel };
    std::array<std::pair<int, float>, 2> boundedEnvelope {
        std::pair { -99, sentinel }, std::pair { -99, sentinel }
    };

    if (snapshot.copyLFOPhases(0, std::span<float>(boundedLFO).first(1)) != 1
        || boundedLFO[1] != sentinel
        || snapshot.copyMSEGPhases(0, std::span<float>(boundedMSEG).first(1)) != 1
        || boundedMSEG[1] != sentinel
        || snapshot.copyEnvelopePhases(
               0, std::span<std::pair<int, float>>(boundedEnvelope).first(1)) != 1
        || boundedEnvelope[1] != std::pair { -99, sentinel })
    {
        failureReason = "snapshot helper wrote beyond its caller-owned span";
        return false;
    }

    if (snapshot.copyLFOPhases(-1, boundedLFO) != 0
        || snapshot.copyMSEGPhases(NUM_MSEGS, boundedMSEG) != 0
        || snapshot.copyEnvelopePhases(NUM_ENVELOPES, boundedEnvelope) != 0
        || snapshot.copyLFOPhases(0, std::span<float> {}) != 0)
    {
        failureReason = "snapshot helper did not reject an invalid index or empty span";
        return false;
    }

    return true;
}

bool stressConcurrentVisualSnapshots(ResonariumProcessor& processor,
                                     const char*& failureReason)
{
    std::atomic<bool> audioRunning { true };
    std::atomic<bool> audioFinite { true };
    std::thread audioThread([&]
    {
        juce::AudioBuffer<float> audio(2, blockSize);
        juce::MidiBuffer midi;
        for (int block = 0; block < snapshotStressBlocks; ++block)
        {
            audio.clear();
            processor.processBlock(audio, midi);
            for (int channel = 0; channel < audio.getNumChannels(); ++channel)
                for (int sample = 0; sample < audio.getNumSamples(); ++sample)
                    if (!std::isfinite(audio.getSample(channel, sample)))
                        audioFinite.store(false, std::memory_order_relaxed);
        }
        audioRunning.store(false, std::memory_order_release);
    });

    std::size_t successfulCopies = 0;
    bool snapshotsValid = true;
    ResonatorSynth::VisualSnapshot snapshot;
    while (audioRunning.load(std::memory_order_acquire))
    {
        if (processor.synth.copyVisualSnapshot(snapshot))
        {
            ++successfulCopies;
            if (!validateVisualSnapshot(snapshot, failureReason))
            {
                snapshotsValid = false;
                break;
            }
        }
        std::this_thread::yield();
    }
    audioThread.join();

    if (!audioFinite.load(std::memory_order_relaxed))
    {
        failureReason = "audio became non-finite during snapshot handoff stress";
        return false;
    }
    if (!snapshotsValid)
        return false;
    if (successfulCopies == 0)
    {
        failureReason = "no visual snapshot copy succeeded during concurrent publication";
        return false;
    }
    return true;
}

int fail(int cycle, const char* reason)
{
    std::fprintf(stderr, "editor lifecycle failure at cycle %d: %s\n", cycle, reason);
    return EXIT_FAILURE;
}
} // namespace

int main()
{
#if JUCE_LINUX
    if (std::getenv("DISPLAY") == nullptr)
    {
        std::fprintf(stderr,
                     "editor lifecycle skipped: JUCE GUI initialisation requires DISPLAY\n");
        return 77;
    }
#endif

    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    auto* messageManager = juce::MessageManager::getInstanceWithoutCreating();
    if (messageManager == nullptr || !messageManager->isThisTheMessageThread())
        return fail(0, "test is not running on the JUCE message thread");

    LoadedWrapperScope wrapperScope(juce::AudioProcessor::wrapperType_Standalone);
    {
        ResonariumProcessor processor;
        wrapperScope.processorConstructed();
        processor.prepareToPlay(sampleRate, blockSize);

        for (int cycle = 1; cycle <= editorCycles; ++cycle)
        {
            std::unique_ptr<juce::AudioProcessorEditor> editor(
                processor.createEditorIfNeeded());

            if (editor == nullptr)
                return fail(cycle, "createEditorIfNeeded returned null");
            if (processor.getActiveEditor() != editor.get())
                return fail(cycle, "processor did not register the new active editor");
            if (editor->getWidth() <= 0 || editor->getHeight() <= 0)
                return fail(cycle, "editor returned without valid dimensions");

            if (dynamic_cast<gin::ScaledPluginEditor*>(editor.get()) == nullptr)
                return fail(cycle, "editor was not wrapped by ScaledPluginEditor");

            editor->addToDesktop(juce::ComponentPeer::windowIsTemporary
                                 | juce::ComponentPeer::windowIgnoresKeyPresses
                                 | juce::ComponentPeer::windowIgnoresMouseClicks);
            editor->setVisible(true);
            if (!pumpPendingTimersFor(visibleTimerPumpMilliseconds))
                return fail(cycle, "visible scheduler callbacks left the JUCE message thread");

            renderNote(processor, 36 + (cycle - 1) * 2);
            if (!pumpPendingTimersFor(visibleTimerPumpMilliseconds))
                return fail(cycle, "visual snapshot was not pumped through the shared scheduler");

            const char* snapshotFailure = nullptr;
            ResonatorSynth::VisualSnapshot visibleSnapshot;
            if (!processor.synth.copyVisualSnapshot(visibleSnapshot))
                return fail(cycle, "visible editor did not publish a visual snapshot");
            if (!validateVisualSnapshot(visibleSnapshot, snapshotFailure))
                return fail(cycle, snapshotFailure);

            if (cycle == 1)
            {
                const auto firstPublishedStarts =
                    visibleSnapshot.getVoiceTelemetry().noteStarts;
                renderNote(processor, 72);

                ResonatorSynth::VisualSnapshot decimatedSnapshot;
                if (!processor.synth.copyVisualSnapshot(decimatedSnapshot))
                    return fail(cycle, "could not copy the decimated visual snapshot");
                if (decimatedSnapshot.getVoiceTelemetry().noteStarts
                    != firstPublishedStarts)
                {
                    return fail(cycle, "visual snapshot published again on the next block");
                }

                constexpr int blocksPerVisualPublication =
                    (sampleRate + visualPublicationRateHz * blockSize - 1)
                    / (visualPublicationRateHz * blockSize);
                for (int block = 1; block < blocksPerVisualPublication; ++block)
                    renderSilentBlock(processor);

                if (!processor.synth.copyVisualSnapshot(decimatedSnapshot)
                    || decimatedSnapshot.getVoiceTelemetry().noteStarts
                           <= firstPublishedStarts)
                {
                    return fail(cycle, "visual snapshot did not refresh at its 30 Hz deadline");
                }
                if (!validateVisualSnapshot(decimatedSnapshot, snapshotFailure))
                    return fail(cycle, snapshotFailure);

                if (!stressConcurrentVisualSnapshots(processor, snapshotFailure))
                    return fail(cycle, snapshotFailure);
            }

            ResonatorSynth::VisualSnapshot publishedBeforeClose;
            if (!processor.synth.copyVisualSnapshot(publishedBeforeClose))
                return fail(cycle, "could not copy the published snapshot before editor close");

            editor->setVisible(false);
            if (!pumpPendingTimersFor(visibleTimerPumpMilliseconds))
                return fail(cycle, "hidden scheduler callbacks left the JUCE message thread");
            editor->removeFromDesktop();

            editor.reset();
            if (processor.getActiveEditor() != nullptr)
                return fail(cycle, "processor retained the destroyed editor");
            if (!pumpPendingTimersFor(interCyclePumpMilliseconds))
                return fail(cycle, "post-destruction timer pump failed");

            renderNote(processor, 37 + (cycle - 1) * 2);
            ResonatorSynth::VisualSnapshot publishedWithoutEditor;
            if (!processor.synth.copyVisualSnapshot(publishedWithoutEditor))
                return fail(cycle, "could not copy the snapshot after editor destruction");
            if (publishedWithoutEditor.getVoiceTelemetry().noteStarts
                != publishedBeforeClose.getVoiceTelemetry().noteStarts)
            {
                return fail(cycle, "visual publication remained active after editor destruction");
            }
        }

        // Resonarium schedules onboarding work 300 ms after construction. Keep
        // the processor alive while every stale editor callback becomes due,
        // then invoke all pending timers synchronously on the message thread.
        bool delayedTimerProbeFired = false;
        juce::Timer::callAfterDelay(300, [&delayedTimerProbeFired]
        {
            delayedTimerProbeFired = true;
        });
        if (!pumpPendingTimersFor(delayedCallbackPumpMilliseconds))
            return fail(editorCycles, "delayed-callback timer pump failed");
        if (!delayedTimerProbeFired)
            return fail(editorCycles, "delayed timer probe did not run");
        if (processor.getActiveEditor() != nullptr)
            return fail(editorCycles, "delayed callbacks resurrected an editor");
    }

    // Also exercise callbacks that become pending as the processor itself is
    // destroyed, before JUCE's global GUI state is shut down.
    if (!pumpPendingTimersFor(interCyclePumpMilliseconds))
        return fail(editorCycles, "final timer pump failed");

    std::fprintf(stderr,
                 "editor lifecycle passed: cycles=%d delayed_timer_pump_ms=%d\n",
                 editorCycles, delayedCallbackPumpMilliseconds);
    return EXIT_SUCCESS;
}
