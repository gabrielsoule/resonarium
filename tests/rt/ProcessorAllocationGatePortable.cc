#include "PluginProcessor.h"
#include "../performance/AllocationProbe.h"

#if defined(_WIN32)
 #include <Windows.h>
 #include <Psapi.h>
#elif defined(__linux__)
 #include <unistd.h>
#endif

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <string_view>
#include <thread>

namespace
{
constexpr int sampleRate = 48'000;
constexpr int blockSize = 128;
constexpr int stressEvents = 100'000;
constexpr std::uint64_t preparedResidentLimit = 512ULL * 1024ULL * 1024ULL;
constexpr std::uint64_t stressResidentLimit = 8ULL * 1024ULL * 1024ULL;

struct CheckResult
{
    const char* name = nullptr;
    std::uint64_t actual = 0;
    std::uint64_t limit = 0;
    bool exact = true;
};

struct GateResults
{
    std::array<CheckResult, 96> checks {};
    int count = 0;
    bool failed = false;

    void requireZero(const char* name, std::uint64_t actual) noexcept
    {
        requireEqual(name, actual, 0);
    }

    void requireEqual(const char* name, std::uint64_t actual, std::uint64_t expected) noexcept
    {
        checks[static_cast<std::size_t>(count++)] = { name, actual, expected, true };
        failed = failed || actual != expected;
    }

    void requireAtMost(const char* name, std::uint64_t actual, std::uint64_t limit) noexcept
    {
        checks[static_cast<std::size_t>(count++)] = { name, actual, limit, false };
        failed = failed || actual > limit;
    }
};

std::uint64_t residentBytes() noexcept
{
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS counters {};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)) == 0)
        return std::numeric_limits<std::uint64_t>::max();
    return static_cast<std::uint64_t>(counters.WorkingSetSize);
#elif defined(__linux__)
    auto* file = std::fopen("/proc/self/statm", "r");
    if (file == nullptr)
        return std::numeric_limits<std::uint64_t>::max();
    unsigned long totalPages = 0;
    unsigned long residentPages = 0;
    const auto fields = std::fscanf(file, "%lu %lu", &totalPages, &residentPages);
    std::fclose(file);
    const auto pageSize = sysconf(_SC_PAGESIZE);
    if (fields != 2 || pageSize <= 0)
        return std::numeric_limits<std::uint64_t>::max();
    return static_cast<std::uint64_t>(residentPages) * static_cast<std::uint64_t>(pageSize);
#else
    return std::numeric_limits<std::uint64_t>::max();
#endif
}

std::uint64_t positiveGrowth(std::uint64_t before, std::uint64_t after) noexcept
{
    if (before == std::numeric_limits<std::uint64_t>::max()
        || after == std::numeric_limits<std::uint64_t>::max())
        return std::numeric_limits<std::uint64_t>::max();
    return after > before ? after - before : 0;
}

void setParameter(gin::Parameter& parameter, float value)
{
    parameter.setUserValue(value);
}

void processUntracked(ResonariumProcessor& processor,
                      juce::AudioBuffer<float>& audio,
                      juce::MidiBuffer& midi)
{
    audio.clear();
    processor.processBlock(audio, midi);
}

std::uint64_t processTracked(ResonariumProcessor& processor,
                             juce::AudioBuffer<float>& audio,
                             juce::MidiBuffer& midi) noexcept
{
    audio.clear();
    resonarium::test::beginAllocationProbe();
    processor.processBlock(audio, midi);
    return resonarium::test::endAllocationProbeCounts().totalCalls();
}

void queueChord(juce::MidiBuffer& midi)
{
    for (int member = 0; member < 15; ++member)
        midi.addEvent(juce::MidiMessage::noteOn(member + 2, 48 + member,
                                                static_cast<juce::uint8>(96)),
                      0);
}

bool publishTestSample(Sampler& sampler, const juce::AudioBuffer<float>& sample)
{
    std::atomic<int> completed { 0 };
    const auto accepted = sampler.requestInstallForTesting(
        sample, sampleRate, [&completed](Sampler::LoadResult result)
        {
            completed.store(result.success ? 1 : -1, std::memory_order_release);
        });
    if (!accepted)
        return false;
    while (completed.load(std::memory_order_acquire) == 0)
        std::this_thread::yield();
    return completed.load(std::memory_order_relaxed) == 1;
}

bool publishSampleClear(Sampler& sampler)
{
    std::atomic<int> completed { 0 };
    sampler.requestClear([&completed](Sampler::LoadResult result)
    {
        completed.store(result.success ? 1 : -1, std::memory_order_release);
    });
    while (completed.load(std::memory_order_acquire) == 0)
        std::this_thread::yield();
    return completed.load(std::memory_order_relaxed) == 1;
}

void runAllocatorSelfTest(GateResults& results)
{
    const auto coverage = std::string_view(resonarium::test::allocationProbeCoverage());
    const bool hasCInterposition = coverage == "linux_c_and_cpp_linker_wrap"
                                   || coverage == "windows_iat_and_cpp_interposition";
    results.requireEqual("allocator self-test: robust C/C++ coverage",
                         hasCInterposition ? 1 : 0, 1);

    if (hasCInterposition)
    {
        auto* (*volatile mallocFunction)(std::size_t) = &std::malloc;
        auto* (*volatile callocFunction)(std::size_t, std::size_t) = &std::calloc;
        auto* (*volatile reallocFunction)(void*, std::size_t) = &std::realloc;
        resonarium::test::beginAllocationProbe();
        auto* first = mallocFunction(64);
        auto* second = callocFunction(1, 64);
        auto* third = reallocFunction(nullptr, 64);
        std::free(first);
        std::free(second);
        std::free(third);
        const auto calls = resonarium::test::endAllocationProbeCounts();
        results.requireEqual("allocator self-test: C allocations",
                             calls.allocationCalls, 3);
        results.requireEqual("allocator self-test: C deallocations",
                             calls.deallocationCalls, 3);
    }
    else
    {
        std::fprintf(stderr,
                     "[FAIL] platform C allocation interception is incomplete: %s\n",
                     resonarium::test::allocationProbeCoverage());
    }

    // A callback window belongs exclusively to the thread that opened it.
    // Create the worker before tracking so std::thread setup is not part of the
    // measurement, then prove an allocation on that worker remains invisible.
    std::atomic<bool> startWorker { false };
    std::atomic<bool> workerFinished { false };
    std::thread worker([&]
    {
        while (!startWorker.load(std::memory_order_acquire))
        {
        }
        auto* allocation = ::operator new(64);
        ::operator delete(allocation);
        workerFinished.store(true, std::memory_order_release);
    });
    resonarium::test::beginAllocationProbe();
    startWorker.store(true, std::memory_order_release);
    while (!workerFinished.load(std::memory_order_acquire))
    {
    }
    const auto crossThreadCalls = resonarium::test::endAllocationProbeCounts();
    worker.join();
    results.requireZero("allocator self-test: other-thread allocation",
                        crossThreadCalls.allocationCalls);
    results.requireZero("allocator self-test: other-thread deallocation",
                        crossThreadCalls.deallocationCalls);

    resonarium::test::beginAllocationProbe();
    auto* plain = ::operator new(64);
    ::operator delete(plain);
    const auto plainCalls = resonarium::test::endAllocationProbeCounts();
    results.requireEqual("allocator self-test: C++ new", plainCalls.allocationCalls, 1);
    results.requireEqual("allocator self-test: C++ delete", plainCalls.deallocationCalls, 1);

    resonarium::test::beginAllocationProbe();
    auto* aligned = ::operator new(64, static_cast<std::align_val_t>(64));
    ::operator delete(aligned, static_cast<std::align_val_t>(64));
    const auto alignedCalls = resonarium::test::endAllocationProbeCounts();
    results.requireEqual("allocator self-test: aligned C++ new",
                         alignedCalls.allocationCalls, 1);
    results.requireEqual("allocator self-test: aligned C++ delete",
                         alignedCalls.deallocationCalls, 1);
}
} // namespace

int main()
{
    GateResults results;
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    runAllocatorSelfTest(results);

    const auto residentBeforeProcessor = residentBytes();
    ResonariumProcessor processor;
    processor.prepareToPlay(sampleRate, blockSize);
    juce::AudioBuffer<float> audio(2, blockSize);
    juce::MidiBuffer midi;
    midi.ensureSize(16 * 1024);

    for (int block = 0; block < 64; ++block)
        processUntracked(processor, audio, midi);

    results.requireAtMost("prepared resident growth",
                          positiveGrowth(residentBeforeProcessor, residentBytes()),
                          preparedResidentLimit);

    auto* numVoices = processor.getParameter("numVoices");
    if (numVoices == nullptr)
    {
        std::fputs("[FAIL] numVoices parameter is missing\n", stderr);
        return 1;
    }

    results.requireZero("silence", processTracked(processor, audio, midi));
    queueChord(midi);
    results.requireZero("15-note note-on", processTracked(processor, audio, midi));
    results.requireZero("held chord", processTracked(processor, audio, midi));

    midi.addEvent(juce::MidiMessage::pitchWheel(2, 10'000), 0);
    midi.addEvent(juce::MidiMessage::channelPressureChange(2, 80), 0);
    midi.addEvent(juce::MidiMessage::controllerEvent(2, 74, 96), 0);
    results.requireZero("MPE expression", processTracked(processor, audio, midi));

    midi.addEvent(juce::MidiMessage::controllerEvent(1, 64, 127), 0);
    midi.addEvent(juce::MidiMessage::noteOff(2, 48), 0);
    results.requireZero("sustained release", processTracked(processor, audio, midi));
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 64, 0), 0);
    results.requireZero("pedal release", processTracked(processor, audio, midi));

    setParameter(*numVoices, 1.0f);
    results.requireZero("numVoices 16 to 1", processTracked(processor, audio, midi));
    setParameter(*numVoices, 16.0f);
    results.requireZero("numVoices 1 to 16", processTracked(processor, audio, midi));
    processor.synth.panic();
    results.requireZero("panic", processTracked(processor, audio, midi));

    auto& voiceParams = processor.synth.params.voiceParams;
    auto& bank = voiceParams.waveguideResonatorBankParams[0];
    setParameter(*voiceParams.impulseExciterParams[0].enabled, 1.0f);
    for (int lane = 0; lane < NUM_RESONATORS; ++lane)
        setParameter(*bank.resonatorParams[static_cast<std::size_t>(lane)].enabled,
                     lane < 4 ? 1.0f : 0.0f);
    processUntracked(processor, audio, midi);
    midi.addEvent(juce::MidiMessage::noteOn(2, 60, static_cast<juce::uint8>(100)), 0);
    processUntracked(processor, audio, midi);

    for (int mode = 0; mode < 3; ++mode)
    {
        setParameter(*bank.couplingMode, static_cast<float>(mode));
        results.requireZero(mode == 0 ? "parallel topology update"
                                     : mode == 1 ? "interlinked topology update"
                                                 : "cascade topology update",
                            processTracked(processor, audio, midi));
    }

    setParameter(*bank.resonatorParams[0].pitch, 7.25f);
    results.requireZero("fractional retune", processTracked(processor, audio, midi));
    setParameter(*bank.resonatorParams[1].enabled, 0.0f);
    setParameter(*bank.resonatorParams[5].enabled, 1.0f);
    results.requireZero("sparse enabled-mask update", processTracked(processor, audio, midi));
    processor.synth.panic();
    results.requireZero("active waveguide reset", processTracked(processor, audio, midi));

    // Exercise the rack transitions on an actual voice. The preceding panic
    // retires every assignment, so leaving this out would measure only idle
    // processor callbacks and miss note-time reset/filter/PRNG work.
    midi.addEvent(juce::MidiMessage::noteOn(2, 60, static_cast<juce::uint8>(100)), 0);
    processUntracked(processor, audio, midi);

    // Fixed-rack source masks, first-enable logical resets, stationary filter
    // tuples, PRNG advancement, and immutable snapshot commits are all part of
    // the audio callback contract.
    setParameter(*voiceParams.noiseExciterParams[0].enabled, 1.0f);
    results.requireZero("exciter noise first enable", processTracked(processor, audio, midi));
    results.requireZero("exciter noise stationary controls", processTracked(processor, audio, midi));
    setParameter(*voiceParams.noiseExciterParams[0].filterParams.type, 1.0f);
    setParameter(*voiceParams.noiseExciterParams[0].filterParams.frequency, 1777.0f);
    results.requireZero("exciter cached filter update", processTracked(processor, audio, midi));
    setParameter(*voiceParams.noiseExciterParams[0].enabled, 0.0f);
    setParameter(*voiceParams.impulseTrainExciterParams[0].enabled, 1.0f);
    setParameter(*voiceParams.impulseTrainExciterParams[0].mode, 2.0f);
    results.requireZero("exciter source mask and sequence mode", processTracked(processor, audio, midi));
    setParameter(*voiceParams.impulseTrainExciterParams[0].mode, 1.0f);
    results.requireZero("exciter stochastic sequence mode", processTracked(processor, audio, midi));
    setParameter(*voiceParams.impulseTrainExciterParams[0].enabled, 0.0f);

    juce::AudioBuffer<float> testSample(1, 7);
    for (int sample = 0; sample < testSample.getNumSamples(); ++sample)
        testSample.setSample(0, sample, 0.1f * static_cast<float>(sample + 1));
    results.requireEqual("sampler worker publishes test snapshot",
                         publishTestSample(processor.globalState.sampler, testSample) ? 1 : 0, 1);
    results.requireZero("sampler snapshot block-boundary commit", processTracked(processor, audio, midi));
    setParameter(*voiceParams.sampleExciterParams.enabled, 1.0f);
    setParameter(*voiceParams.sampleExciterParams.loop, 1.0f);
    midi.addEvent(juce::MidiMessage::noteOn(2, 60, static_cast<juce::uint8>(100)), 0);
    results.requireZero("sample exciter assignment acquire", processTracked(processor, audio, midi));
    results.requireEqual("sampler worker publishes replacement",
                         publishTestSample(processor.globalState.sampler, testSample) ? 1 : 0, 1);
    results.requireZero("active-note sample replacement commit", processTracked(processor, audio, midi));
    results.requireEqual("sampler worker publishes clear",
                         publishSampleClear(processor.globalState.sampler) ? 1 : 0, 1);
    results.requireZero("active-note sample clear commit", processTracked(processor, audio, midi));
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 120, 0), 0);
    results.requireZero("sample snapshot voice release", processTracked(processor, audio, midi));
    results.requireEqual("sampler reclaim queue overflow", processor.globalState.sampler.getTelemetry().reclaimOverflows,
                         0);

    // Every retained global effect must be safe on its first enable, on mode
    // or coefficient changes, and on reset-producing re-enable transitions.
    // Parameter writes are deliberately outside the tracked callback window.
    auto& effectParams = processor.synth.params.effectChainParams;
    setParameter(*numVoices, 1.0f);
    setParameter(*voiceParams.impulseExciterParams[0].enabled, 1.0f);
    processor.synth.panic();
    processUntracked(processor, audio, midi);
    midi.addEvent(juce::MidiMessage::noteOn(2, 60,
                                            static_cast<juce::uint8>(100)),
                  0);
    processUntracked(processor, audio, midi);

    setParameter(*effectParams.chorusParams.enabled, 1.0f);
    results.requireZero("global chorus enable", processTracked(processor, audio, midi));
    setParameter(*effectParams.chorusParams.rate, 7.25f);
    results.requireZero("global chorus parameter change", processTracked(processor, audio, midi));
    setParameter(*effectParams.chorusParams.enabled, 0.0f);
    results.requireZero("global chorus disable", processTracked(processor, audio, midi));

    setParameter(*effectParams.phaserParams.enabled, 1.0f);
    results.requireZero("global phaser enable", processTracked(processor, audio, midi));
    setParameter(*effectParams.phaserParams.centreFrequency, 1234.0f);
    results.requireZero("global phaser parameter change", processTracked(processor, audio, midi));
    setParameter(*effectParams.phaserParams.enabled, 0.0f);
    results.requireZero("global phaser disable", processTracked(processor, audio, midi));

    setParameter(*effectParams.distortionParams.enabled, 1.0f);
    results.requireZero("global distortion enable", processTracked(processor, audio, midi));
    setParameter(*effectParams.distortionParams.distortionMode, 5.0f);
    setParameter(*effectParams.distortionParams.drive, 12.0f);
    results.requireZero("global distortion mode and drive change",
                        processTracked(processor, audio, midi));
    setParameter(*effectParams.distortionParams.enabled, 0.0f);
    results.requireZero("global distortion disable", processTracked(processor, audio, midi));

    setParameter(*effectParams.multiAmpParams.enabled, 1.0f);
    results.requireZero("global amp enable", processTracked(processor, audio, midi));
    setParameter(*effectParams.multiAmpParams.mode, 5.0f);
    setParameter(*effectParams.multiAmpParams.paramA, 0.75f);
    results.requireZero("global amp mode and parameter change",
                        processTracked(processor, audio, midi));
    setParameter(*effectParams.multiAmpParams.enabled, 0.0f);
    results.requireZero("global amp disable", processTracked(processor, audio, midi));

    setParameter(*effectParams.delayParams.enabled, 1.0f);
    results.requireZero("global delay enable", processTracked(processor, audio, midi));
    setParameter(*effectParams.delayParams.timeL, 0.073f);
    setParameter(*effectParams.delayParams.timeR, 0.119f);
    setParameter(*effectParams.delayParams.lock, 0.0f);
    setParameter(*effectParams.delayParams.pingPongAmount, 1.0f);
    results.requireZero("global delay mode and time change",
                        processTracked(processor, audio, midi));
    setParameter(*effectParams.delayParams.enabled, 0.0f);
    results.requireZero("global delay disable", processTracked(processor, audio, midi));
    setParameter(*effectParams.delayParams.enabled, 1.0f);
    results.requireZero("global delay re-enable and reset",
                        processTracked(processor, audio, midi));

    setParameter(*effectParams.compressorParams.enabled, 1.0f);
    results.requireZero("global compressor enable", processTracked(processor, audio, midi));
    setParameter(*effectParams.compressorParams.threshold, -24.0f);
    setParameter(*effectParams.compressorParams.ratio, 7.0f);
    results.requireZero("global compressor parameter change",
                        processTracked(processor, audio, midi));
    setParameter(*effectParams.compressorParams.enabled, 0.0f);
    results.requireZero("global compressor disable", processTracked(processor, audio, midi));

    setParameter(*effectParams.reverbParams.enabled, 1.0f);
    results.requireZero("global reverb enable", processTracked(processor, audio, midi));
    setParameter(*effectParams.reverbParams.size, 0.91f);
    setParameter(*effectParams.reverbParams.predelay, 0.17f);
    setParameter(*effectParams.reverbParams.decay, 0.88f);
    results.requireZero("global reverb size predelay and decay change",
                        processTracked(processor, audio, midi));
    setParameter(*effectParams.reverbParams.enabled, 0.0f);
    results.requireZero("global reverb disable", processTracked(processor, audio, midi));
    setParameter(*effectParams.reverbParams.enabled, 1.0f);
    results.requireZero("global reverb re-enable and reset",
                        processTracked(processor, audio, midi));

    setParameter(*effectParams.filterParams[0].enabled, 1.0f);
    results.requireZero("global filter 1 enable", processTracked(processor, audio, midi));
    setParameter(*effectParams.filterParams[0].cutoff, 1777.0f);
    setParameter(*effectParams.filterParams[0].mode, 1.0f);
    results.requireZero("global filter 1 parameter change",
                        processTracked(processor, audio, midi));
    setParameter(*effectParams.filterParams[0].enabled, 0.0f);
    results.requireZero("global filter 1 disable", processTracked(processor, audio, midi));

    setParameter(*effectParams.filterParams[1].enabled, 1.0f);
    results.requireZero("global filter 2 enable", processTracked(processor, audio, midi));
    setParameter(*effectParams.filterParams[1].cutoff, 4321.0f);
    setParameter(*effectParams.filterParams[1].resonance, 3.0f);
    results.requireZero("global filter 2 parameter change",
                        processTracked(processor, audio, midi));
    setParameter(*effectParams.filterParams[1].enabled, 0.0f);
    results.requireZero("global filter 2 disable", processTracked(processor, audio, midi));

    setParameter(*processor.synth.params.globalParams.legacyPolyEffectChain, 1.0f);
    results.requireZero("legacy poly FX tombstone to one",
                        processTracked(processor, audio, midi));
    setParameter(*processor.synth.params.globalParams.legacyPolyEffectChain, 0.0f);
    results.requireZero("legacy poly FX tombstone to zero",
                        processTracked(processor, audio, midi));

    midi.addEvent(juce::MidiMessage::controllerEvent(1, 120, 0), 37);
    results.requireZero("CC120 clears active global FX",
                        processTracked(processor, audio, midi));

    // Warm the same face-roll path before measuring resident growth.
    for (int event = 0; event < 2'048;)
    {
        const auto count = juce::jmin(blockSize, 2'048 - event);
        for (int offset = 0; offset < count; ++offset, ++event)
            midi.addEvent(juce::MidiMessage::noteOn(
                              2 + event % 15, 24 + (event * 29) % 96,
                              static_cast<juce::uint8>(70 + event % 58)),
                          offset);
        processUntracked(processor, audio, midi);
    }
    processor.synth.panic();
    processUntracked(processor, audio, midi);

    const auto residentBeforeStress = residentBytes();
    std::uint64_t stressAllocations = 0;
    for (int event = 0; event < stressEvents;)
    {
        const auto count = juce::jmin(blockSize, stressEvents - event);
        for (int offset = 0; offset < count; ++offset, ++event)
            midi.addEvent(juce::MidiMessage::noteOn(
                              2 + event % 15,
                              24 + (event * 29 + (event / blockSize) * 11) % 96,
                              static_cast<juce::uint8>(70 + event % 58)),
                          offset);
        stressAllocations += processTracked(processor, audio, midi);
    }
    results.requireZero("100,000-event callback memory operations", stressAllocations);
    results.requireAtMost("100,000-event resident growth",
                          positiveGrowth(residentBeforeStress, residentBytes()),
                          stressResidentLimit);

    const auto telemetry = processor.synth.getVoiceTelemetry();
    results.requireEqual("dropped note-ons", telemetry.droppedNoteOns, 0);
    results.requireEqual("physical voice pool", telemetry.physicalPoolSize, 18);

    std::fprintf(stderr, "allocator coverage: %s\n",
                 resonarium::test::allocationProbeCoverage());
    for (int index = 0; index < results.count; ++index)
    {
        const auto& check = results.checks[static_cast<std::size_t>(index)];
        const auto passed = check.exact ? check.actual == check.limit
                                        : check.actual <= check.limit;
        if (check.exact)
            std::fprintf(stderr, "[%s] %s: %llu (expected %llu)\n",
                         passed ? "PASS" : "FAIL", check.name,
                         static_cast<unsigned long long>(check.actual),
                         static_cast<unsigned long long>(check.limit));
        else
            std::fprintf(stderr, "[%s] %s: %.2f MiB (limit %.2f MiB)\n",
                         passed ? "PASS" : "FAIL", check.name,
                         static_cast<double>(check.actual) / (1024.0 * 1024.0),
                         static_cast<double>(check.limit) / (1024.0 * 1024.0));
    }
    return results.failed ? 1 : 0;
}
