#include "PluginProcessor.h"

#include <mach/mach.h>
#include <malloc/malloc.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <new>
#include <thread>

namespace realtime_allocations {

thread_local bool tracking = false;
thread_local std::uint64_t allocationCalls = 0;
thread_local std::uint64_t deallocationCalls = 0;

struct Counts {
  std::uint64_t allocations = 0;
  std::uint64_t deallocations = 0;

  [[nodiscard]] std::uint64_t total() const noexcept {
    return allocations + deallocations;
  }
};

inline void recordAllocation() noexcept {
  if (tracking) ++allocationCalls;
}

void begin() noexcept {
  allocationCalls = 0;
  deallocationCalls = 0;
  tracking = true;
}

inline void recordCAllocation() noexcept {
  recordAllocation();
}

Counts end() noexcept {
  tracking = false;
  return {allocationCalls, deallocationCalls};
}

void* allocate(std::size_t size) {
  recordAllocation();
  if (auto* result = malloc_zone_malloc(malloc_default_zone(), size == 0 ? 1 : size))
    return result;
  throw std::bad_alloc();
}

void* allocateAligned(std::size_t size, std::size_t alignment) {
  recordAllocation();
  if (auto* result = malloc_zone_memalign(malloc_default_zone(), alignment,
                                           size == 0 ? 1 : size))
    return result;
  throw std::bad_alloc();
}

void deallocate(void* pointer) noexcept {
  if (pointer != nullptr) {
    if (tracking) ++deallocationCalls;
    malloc_zone_free(malloc_default_zone(), pointer);
  }
}

inline void recordCDeallocation(void* pointer) noexcept {
  if (tracking && pointer != nullptr) ++deallocationCalls;
}

}  // namespace realtime_allocations

// Keep every C++ allocation form in this executable observable. The plugin's
// shared-code archive resolves its operator-new calls to these definitions.
__attribute__((visibility("default"))) void* operator new(std::size_t size) {
  return realtime_allocations::allocate(size);
}

__attribute__((visibility("default"))) void* operator new[](std::size_t size) {
  return realtime_allocations::allocate(size);
}

__attribute__((visibility("default"))) void* operator new(
    std::size_t size, const std::nothrow_t&) noexcept {
  try {
    return realtime_allocations::allocate(size);
  } catch (...) {
    return nullptr;
  }
}

__attribute__((visibility("default"))) void* operator new[](
    std::size_t size, const std::nothrow_t&) noexcept {
  try {
    return realtime_allocations::allocate(size);
  } catch (...) {
    return nullptr;
  }
}

__attribute__((visibility("default"))) void* operator new(
    std::size_t size, std::align_val_t alignment) {
  return realtime_allocations::allocateAligned(size,
                                                static_cast<std::size_t>(alignment));
}

__attribute__((visibility("default"))) void* operator new[](
    std::size_t size, std::align_val_t alignment) {
  return realtime_allocations::allocateAligned(size,
                                                static_cast<std::size_t>(alignment));
}

__attribute__((visibility("default"))) void* operator new(
    std::size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept {
  try {
    return realtime_allocations::allocateAligned(
        size, static_cast<std::size_t>(alignment));
  } catch (...) {
    return nullptr;
  }
}

__attribute__((visibility("default"))) void* operator new[](
    std::size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept {
  try {
    return realtime_allocations::allocateAligned(
        size, static_cast<std::size_t>(alignment));
  } catch (...) {
    return nullptr;
  }
}

__attribute__((visibility("default"))) void operator delete(void* pointer) noexcept {
  realtime_allocations::deallocate(pointer);
}

__attribute__((visibility("default"))) void operator delete[](void* pointer) noexcept {
  realtime_allocations::deallocate(pointer);
}

__attribute__((visibility("default"))) void operator delete(void* pointer,
                                                              std::size_t) noexcept {
  realtime_allocations::deallocate(pointer);
}

__attribute__((visibility("default"))) void operator delete[](void* pointer,
                                                                std::size_t) noexcept {
  realtime_allocations::deallocate(pointer);
}

__attribute__((visibility("default"))) void operator delete(
    void* pointer, std::align_val_t) noexcept {
  realtime_allocations::deallocate(pointer);
}

__attribute__((visibility("default"))) void operator delete[](
    void* pointer, std::align_val_t) noexcept {
  realtime_allocations::deallocate(pointer);
}

__attribute__((visibility("default"))) void operator delete(
    void* pointer, std::size_t, std::align_val_t) noexcept {
  realtime_allocations::deallocate(pointer);
}

__attribute__((visibility("default"))) void operator delete[](
    void* pointer, std::size_t, std::align_val_t) noexcept {
  realtime_allocations::deallocate(pointer);
}

// The Mach-O executable provides the C symbols directly. This catches calls
// from all statically linked plugin and JUCE objects; Darwin's two-level
// namespace keeps libSystem's own internal allocator calls on the real zone.
extern "C" __attribute__((visibility("default"))) void* malloc(
    std::size_t size) {
  realtime_allocations::recordCAllocation();
  return malloc_zone_malloc(malloc_default_zone(), size);
}

extern "C" __attribute__((visibility("default"))) void* calloc(
    std::size_t count, std::size_t size) {
  realtime_allocations::recordCAllocation();
  return malloc_zone_calloc(malloc_default_zone(), count, size);
}

extern "C" __attribute__((visibility("default"))) void* realloc(
    void* pointer, std::size_t size) {
  realtime_allocations::recordCAllocation();
  return malloc_zone_realloc(malloc_default_zone(), pointer, size);
}

extern "C" __attribute__((visibility("default"))) void* reallocf(
    void* pointer, std::size_t size) {
  realtime_allocations::recordCAllocation();
  auto* result = malloc_zone_realloc(malloc_default_zone(), pointer, size);
  if (result == nullptr && pointer != nullptr) {
    realtime_allocations::recordCDeallocation(pointer);
    malloc_zone_free(malloc_default_zone(), pointer);
  }
  return result;
}

extern "C" __attribute__((visibility("default"))) void free(void* pointer) {
  if (pointer != nullptr) {
    realtime_allocations::recordCDeallocation(pointer);
    malloc_zone_free(malloc_default_zone(), pointer);
  }
}

extern "C" __attribute__((visibility("default"))) void* valloc(
    std::size_t size) {
  realtime_allocations::recordCAllocation();
  return malloc_zone_valloc(malloc_default_zone(), size);
}

extern "C" __attribute__((visibility("default"))) void* aligned_alloc(
    std::size_t alignment, std::size_t size) {
  realtime_allocations::recordCAllocation();
  return malloc_zone_memalign(malloc_default_zone(), alignment, size);
}

extern "C" __attribute__((visibility("default"))) int posix_memalign(
    void** result, std::size_t alignment, std::size_t size) {
  if (result == nullptr || alignment < sizeof(void*) ||
      (alignment & (alignment - 1)) != 0) {
    return EINVAL;
  }

  realtime_allocations::recordCAllocation();
  *result = malloc_zone_memalign(malloc_default_zone(), alignment, size);
  return *result == nullptr ? ENOMEM : 0;
}

extern "C" __attribute__((visibility("default"))) char* strdup(
    const char* source) {
  const auto bytes = std::strlen(source) + 1;
  realtime_allocations::recordCAllocation();
  auto* result = static_cast<char*>(
      malloc_zone_malloc(malloc_default_zone(), bytes));
  if (result != nullptr) std::memcpy(result, source, bytes);
  return result;
}

extern "C" __attribute__((visibility("default"))) char* strndup(
    const char* source, std::size_t limit) {
  const auto length = strnlen(source, limit);
  realtime_allocations::recordCAllocation();
  auto* result = static_cast<char*>(
      malloc_zone_malloc(malloc_default_zone(), length + 1));
  if (result != nullptr) {
    std::memcpy(result, source, length);
    result[length] = '\0';
  }
  return result;
}

namespace {

constexpr int kSampleRate = 48'000;
constexpr int kBlockSize = 128;
constexpr std::uint64_t kPreparedResidentLimit = 512ULL * 1024ULL * 1024ULL;
constexpr std::uint64_t kStressResidentLimit = 8ULL * 1024ULL * 1024ULL;
constexpr int kStressEvents = 100'000;

struct CheckResult {
  const char* name = nullptr;
  std::uint64_t actual = 0;
  std::uint64_t limit = 0;
  bool exact = true;
};

struct GateResults {
  std::array<CheckResult, 96> checks{};
  int count = 0;
  bool failed = false;

  void requireZero(const char* name, std::uint64_t allocations) noexcept {
    checks[static_cast<std::size_t>(count++)] = {name, allocations, 0, true};
    if (allocations != 0) failed = true;
  }

  void requireEqual(const char* name, std::uint64_t actual,
                    std::uint64_t expected) noexcept {
    checks[static_cast<std::size_t>(count++)] = {name, actual, expected, true};
    if (actual != expected) failed = true;
  }

  void requireAtMost(const char* name, std::uint64_t actual,
                     std::uint64_t limit) noexcept {
    checks[static_cast<std::size_t>(count++)] = {name, actual, limit, false};
    if (actual > limit) failed = true;
  }
};

std::uint64_t residentBytes() noexcept {
  mach_task_basic_info_data_t information{};
  mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
  if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                reinterpret_cast<task_info_t>(&information), &count) != KERN_SUCCESS)
    return std::numeric_limits<std::uint64_t>::max();
  return static_cast<std::uint64_t>(information.resident_size);
}

std::uint64_t positiveGrowth(std::uint64_t before,
                             std::uint64_t after) noexcept {
  if (before == std::numeric_limits<std::uint64_t>::max() ||
      after == std::numeric_limits<std::uint64_t>::max())
    return std::numeric_limits<std::uint64_t>::max();
  return after > before ? after - before : 0;
}

void processUntracked(ResonariumProcessor& processor,
                      juce::AudioBuffer<float>& audio,
                      juce::MidiBuffer& midi) {
  audio.clear();
  processor.processBlock(audio, midi);
}

std::uint64_t processTracked(ResonariumProcessor& processor,
                             juce::AudioBuffer<float>& audio,
                             juce::MidiBuffer& midi) noexcept {
  audio.clear();
  realtime_allocations::begin();
  processor.processBlock(audio, midi);
  return realtime_allocations::end().total();
}

void queueChord(juce::MidiBuffer& midi) {
  for (int member = 0; member < 15; ++member)
    midi.addEvent(juce::MidiMessage::noteOn(member + 2, 48 + member,
                                            static_cast<juce::uint8>(96)),
                  0);
}

void setParameter(gin::Parameter& parameter, float value) {
  parameter.setUserValue(value);
}

bool publishTestSample(Sampler& sampler, const juce::AudioBuffer<float>& sample) {
  std::atomic<int> completed{0};
  const auto accepted = sampler.requestInstallForTesting(
      sample, kSampleRate, [&completed](Sampler::LoadResult result) {
        completed.store(result.success ? 1 : -1, std::memory_order_release);
      });
  if (!accepted) return false;
  while (completed.load(std::memory_order_acquire) == 0)
    std::this_thread::yield();
  return completed.load(std::memory_order_relaxed) == 1;
}

bool publishSampleClear(Sampler& sampler) {
  std::atomic<int> completed{0};
  sampler.requestClear([&completed](Sampler::LoadResult result) {
    completed.store(result.success ? 1 : -1, std::memory_order_release);
  });
  while (completed.load(std::memory_order_acquire) == 0)
    std::this_thread::yield();
  return completed.load(std::memory_order_relaxed) == 1;
}

}  // namespace

int main() {
  GateResults results;
  juce::ScopedJuceInitialiser_GUI juceInitialiser;

  // Prove that both independent interception paths are live before trusting
  // any zero count from the audio callback.
  auto* (*volatile mallocAllocator)(std::size_t) = &malloc;
  auto* (*volatile callocAllocator)(std::size_t, std::size_t) = &calloc;
  auto* (*volatile reallocAllocator)(void*, std::size_t) = &realloc;
  auto* (*volatile reallocfAllocator)(void*, std::size_t) = &reallocf;
  auto* (*volatile vallocAllocator)(std::size_t) = &valloc;
  auto* (*volatile alignedAllocator)(std::size_t, std::size_t) =
      &aligned_alloc;
  int (*volatile posixAllocator)(void**, std::size_t, std::size_t) =
      &posix_memalign;
  char* (*volatile duplicateAllocator)(const char*) = &strdup;
  char* (*volatile boundedDuplicateAllocator)(const char*, std::size_t) =
      &strndup;
  realtime_allocations::begin();
  void* mallocAllocation = mallocAllocator(64);
  void* callocAllocation = callocAllocator(1, 64);
  void* reallocAllocation = reallocAllocator(nullptr, 64);
  void* reallocfAllocation = reallocfAllocator(nullptr, 64);
  void* vallocAllocation = vallocAllocator(64);
  void* alignedAllocation = alignedAllocator(64, 64);
  void* posixAllocation = nullptr;
  const int posixResult = posixAllocator(&posixAllocation, 64, 64);
  char* duplicateAllocation = duplicateAllocator("resonarium");
  char* boundedDuplicateAllocation =
      boundedDuplicateAllocator("resonarium", 4);
  free(mallocAllocation);
  free(callocAllocation);
  free(reallocAllocation);
  free(reallocfAllocation);
  free(vallocAllocation);
  free(alignedAllocation);
  free(posixAllocation);
  free(duplicateAllocation);
  free(boundedDuplicateAllocation);
  const auto cSelfTest = realtime_allocations::end();
  results.requireEqual("allocator self-test: C allocations",
                       cSelfTest.allocations, 9);
  results.requireEqual("allocator self-test: C deallocations",
                       cSelfTest.deallocations, 9);
  results.requireEqual("allocator self-test: posix_memalign result",
                       static_cast<std::uint64_t>(posixResult), 0);

  // A callback window belongs exclusively to the thread that opened it.
  // Create the worker before tracking so std::thread setup is not part of the
  // measurement, then prove an allocation on that worker remains invisible.
  std::atomic<bool> startWorker{false};
  std::atomic<bool> workerFinished{false};
  std::thread worker([&] {
    while (!startWorker.load(std::memory_order_acquire)) {
    }
    auto* allocation = ::operator new(64);
    ::operator delete(allocation);
    workerFinished.store(true, std::memory_order_release);
  });
  realtime_allocations::begin();
  startWorker.store(true, std::memory_order_release);
  while (!workerFinished.load(std::memory_order_acquire)) {
  }
  const auto crossThreadSelfTest = realtime_allocations::end();
  worker.join();
  results.requireZero("allocator self-test: other-thread allocation",
                      crossThreadSelfTest.allocations);
  results.requireZero("allocator self-test: other-thread deallocation",
                      crossThreadSelfTest.deallocations);

  realtime_allocations::begin();
  void* cppAllocation = ::operator new(64);
  ::operator delete(cppAllocation);
  const auto cppSelfTest = realtime_allocations::end();
  results.requireEqual("allocator self-test: C++ new", cppSelfTest.allocations, 1);
  results.requireEqual("allocator self-test: C++ delete", cppSelfTest.deallocations, 1);

  realtime_allocations::begin();
  void* alignedCppAllocation =
      ::operator new(64, static_cast<std::align_val_t>(64));
  ::operator delete(alignedCppAllocation, static_cast<std::align_val_t>(64));
  const auto alignedCppSelfTest = realtime_allocations::end();
  results.requireEqual("allocator self-test: aligned C++ new",
                       alignedCppSelfTest.allocations, 1);
  results.requireEqual("allocator self-test: aligned C++ delete",
                       alignedCppSelfTest.deallocations, 1);

  const auto residentBeforeProcessor = residentBytes();

  ResonariumProcessor processor;
  processor.prepareToPlay(kSampleRate, kBlockSize);
  juce::AudioBuffer<float> audio(2, kBlockSize);
  juce::MidiBuffer midi;
  midi.ensureSize(16 * 1024);

  for (int block = 0; block < 64; ++block)
    processUntracked(processor, audio, midi);

  const auto residentAfterPrepare = residentBytes();
  results.requireAtMost("prepared resident growth",
                        positiveGrowth(residentBeforeProcessor,
                                       residentAfterPrepare),
                        kPreparedResidentLimit);

  auto* numVoices = processor.getParameter("numVoices");
  if (numVoices == nullptr) {
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

  midi.addEvent(juce::MidiMessage::noteOff(3, 49), 0);
  results.requireZero("natural note release",
                      processTracked(processor, audio, midi));

  midi.addEvent(juce::MidiMessage::controllerEvent(1, 64, 127), 0);
  results.requireZero("sustain down", processTracked(processor, audio, midi));
  midi.addEvent(juce::MidiMessage::noteOff(2, 48), 0);
  results.requireZero("sustained release", processTracked(processor, audio, midi));
  midi.addEvent(juce::MidiMessage::controllerEvent(1, 64, 0), 0);
  results.requireZero("sustain up", processTracked(processor, audio, midi));

  setParameter(*numVoices, 1.0f);
  results.requireZero("numVoices 16 to 1", processTracked(processor, audio, midi));
  setParameter(*numVoices, 16.0f);
  results.requireZero("numVoices 1 to 16", processTracked(processor, audio, midi));

  processor.synth.panic();
  results.requireZero("UI panic", processTracked(processor, audio, midi));

  setParameter(*numVoices, 1.0f);
  results.requireZero("apply one-voice limit",
                      processTracked(processor, audio, midi));
  midi.addEvent(juce::MidiMessage::noteOn(2, 60,
                                          static_cast<juce::uint8>(100)),
                0);
  results.requireZero("single note-on", processTracked(processor, audio, midi));
  results.requireZero("render stealing victim",
                      processTracked(processor, audio, midi));
  midi.addEvent(juce::MidiMessage::noteOn(3, 67,
                                          static_cast<juce::uint8>(100)),
                0);
  results.requireZero("oldest-voice steal",
                      processTracked(processor, audio, midi));

  midi.addEvent(juce::MidiMessage::controllerEvent(1, 123, 0), 0);
  results.requireZero("CC123 all notes off",
                      processTracked(processor, audio, midi));
  midi.addEvent(juce::MidiMessage::controllerEvent(1, 120, 0), 0);
  results.requireZero("CC120 all sound off",
                      processTracked(processor, audio, midi));

  // Cover the expensive signal path and every retained global-effect
  // transition. Parameter writes happen outside the tracked window; the next
  // callback must still update, reset and process without allocating.
  auto& voiceParams = processor.synth.params.voiceParams;
  auto& effectParams = processor.synth.params.effectChainParams;
  setParameter(*numVoices, 1.0f);
  setParameter(*voiceParams.impulseExciterParams[0].enabled, 1.0f);
  setParameter(
      *voiceParams.waveguideResonatorBankParams[0].resonatorParams[0].enabled,
      1.0f);
  setParameter(*effectParams.delayParams.enabled, 1.0f);
  setParameter(*effectParams.reverbParams.enabled, 1.0f);
  setParameter(*processor.synth.params.globalParams.legacyPolyEffectChain, 0.0f);
  processor.synth.panic();
  processUntracked(processor, audio, midi);

  // Keep a real assignment alive while the rack transitions below are
  // tracked. Without this note the callbacks exercise only the idle synth and
  // cannot prove first-enable/reset or generator/filter work is allocation-free.
  midi.addEvent(juce::MidiMessage::noteOn(2, 60,
                                          static_cast<juce::uint8>(100)),
                0);
  processUntracked(processor, audio, midi);

  // The fixed rack must process first-enable/reset, cached-filter, stochastic
  // sequence, and immutable sample publication transitions without callback
  // allocation or destruction.
  setParameter(*voiceParams.noiseExciterParams[0].enabled, 1.0f);
  results.requireZero("exciter noise first enable",
                      processTracked(processor, audio, midi));
  results.requireZero("exciter noise stationary controls",
                      processTracked(processor, audio, midi));
  setParameter(*voiceParams.noiseExciterParams[0].filterParams.type, 1.0f);
  setParameter(*voiceParams.noiseExciterParams[0].filterParams.frequency, 1777.0f);
  results.requireZero("exciter cached filter update",
                      processTracked(processor, audio, midi));
  setParameter(*voiceParams.noiseExciterParams[0].enabled, 0.0f);
  setParameter(*voiceParams.impulseTrainExciterParams[0].enabled, 1.0f);
  setParameter(*voiceParams.impulseTrainExciterParams[0].mode, 2.0f);
  results.requireZero("exciter source mask and sequence mode",
                      processTracked(processor, audio, midi));
  setParameter(*voiceParams.impulseTrainExciterParams[0].mode, 1.0f);
  results.requireZero("exciter stochastic sequence mode",
                      processTracked(processor, audio, midi));
  setParameter(*voiceParams.impulseTrainExciterParams[0].enabled, 0.0f);

  juce::AudioBuffer<float> testSample(1, 7);
  for (int sample = 0; sample < testSample.getNumSamples(); ++sample)
    testSample.setSample(0, sample, 0.1f * static_cast<float>(sample + 1));
  results.requireEqual("sampler worker publishes test snapshot",
                       publishTestSample(processor.globalState.sampler, testSample) ? 1 : 0, 1);
  results.requireZero("sampler snapshot block-boundary commit",
                      processTracked(processor, audio, midi));
  setParameter(*voiceParams.sampleExciterParams.enabled, 1.0f);
  setParameter(*voiceParams.sampleExciterParams.loop, 1.0f);
  midi.addEvent(juce::MidiMessage::noteOn(2, 60,
                                          static_cast<juce::uint8>(100)), 0);
  results.requireZero("sample exciter assignment acquire",
                      processTracked(processor, audio, midi));
  results.requireEqual("sampler worker publishes replacement",
                       publishTestSample(processor.globalState.sampler, testSample) ? 1 : 0, 1);
  results.requireZero("active-note sample replacement commit",
                      processTracked(processor, audio, midi));
  results.requireEqual("sampler worker publishes clear",
                       publishSampleClear(processor.globalState.sampler) ? 1 : 0, 1);
  results.requireZero("active-note sample clear commit",
                      processTracked(processor, audio, midi));
  midi.addEvent(juce::MidiMessage::controllerEvent(1, 120, 0), 0);
  results.requireZero("sample snapshot voice release",
                      processTracked(processor, audio, midi));
  results.requireEqual("sampler reclaim queue overflow",
                       processor.globalState.sampler.getTelemetry().reclaimOverflows, 0);

  midi.addEvent(juce::MidiMessage::noteOn(2, 60,
                                          static_cast<juce::uint8>(100)),
                0);
  processUntracked(processor, audio, midi);
  for (int block = 0; block < 24; ++block)
    processUntracked(processor, audio, midi);
  processor.synth.panic();
  processUntracked(processor, audio, midi);

  midi.addEvent(juce::MidiMessage::noteOn(2, 60,
                                          static_cast<juce::uint8>(100)),
                0);
  results.requireZero("active DSP note-on: global FX",
                      processTracked(processor, audio, midi));
  auto activeDspPeak = juce::jmax(
      audio.getMagnitude(0, 0, kBlockSize),
      audio.getMagnitude(1, 0, kBlockSize));
  std::uint64_t activeDspHeldAllocations = 0;
  for (int block = 0; block < 8; ++block) {
    activeDspHeldAllocations += processTracked(processor, audio, midi);
    activeDspPeak = juce::jmax(
        activeDspPeak,
        juce::jmax(audio.getMagnitude(0, 0, kBlockSize),
                   audio.getMagnitude(1, 0, kBlockSize)));
  }
  results.requireZero("active DSP held: global FX",
                      activeDspHeldAllocations);
  results.requireEqual("active DSP produced audio",
                       activeDspPeak > 1.0e-6f ? 1 : 0, 1);

  setParameter(*effectParams.chorusParams.enabled, 1.0f);
  results.requireZero("global chorus enable",
                      processTracked(processor, audio, midi));
  setParameter(*effectParams.chorusParams.rate, 7.25f);
  results.requireZero("global chorus parameter change",
                      processTracked(processor, audio, midi));
  setParameter(*effectParams.chorusParams.enabled, 0.0f);
  results.requireZero("global chorus disable",
                      processTracked(processor, audio, midi));

  setParameter(*effectParams.phaserParams.enabled, 1.0f);
  results.requireZero("global phaser enable",
                      processTracked(processor, audio, midi));
  setParameter(*effectParams.phaserParams.centreFrequency, 1234.0f);
  results.requireZero("global phaser parameter change",
                      processTracked(processor, audio, midi));
  setParameter(*effectParams.phaserParams.enabled, 0.0f);
  results.requireZero("global phaser disable",
                      processTracked(processor, audio, midi));

  setParameter(*effectParams.distortionParams.enabled, 1.0f);
  results.requireZero("global distortion enable",
                      processTracked(processor, audio, midi));
  setParameter(*effectParams.distortionParams.distortionMode, 5.0f);
  setParameter(*effectParams.distortionParams.drive, 12.0f);
  results.requireZero("global distortion mode and drive change",
                      processTracked(processor, audio, midi));
  setParameter(*effectParams.distortionParams.enabled, 0.0f);
  results.requireZero("global distortion disable",
                      processTracked(processor, audio, midi));

  setParameter(*effectParams.multiAmpParams.enabled, 1.0f);
  results.requireZero("global amp enable",
                      processTracked(processor, audio, midi));
  setParameter(*effectParams.multiAmpParams.mode, 5.0f);
  setParameter(*effectParams.multiAmpParams.paramA, 0.75f);
  results.requireZero("global amp mode and parameter change",
                      processTracked(processor, audio, midi));
  setParameter(*effectParams.multiAmpParams.enabled, 0.0f);
  results.requireZero("global amp disable",
                      processTracked(processor, audio, midi));

  setParameter(*effectParams.delayParams.timeL, 0.073f);
  setParameter(*effectParams.delayParams.timeR, 0.119f);
  results.requireZero("global delay time change",
                      processTracked(processor, audio, midi));
  setParameter(*effectParams.delayParams.lock, 0.0f);
  setParameter(*effectParams.delayParams.pingPongAmount, 1.0f);
  setParameter(*effectParams.delayParams.feedback, 0.83f);
  results.requireZero("global delay mode and feedback change",
                      processTracked(processor, audio, midi));
  setParameter(*effectParams.delayParams.enabled, 0.0f);
  results.requireZero("global delay disable",
                      processTracked(processor, audio, midi));
  setParameter(*effectParams.delayParams.enabled, 1.0f);
  results.requireZero("global delay re-enable and reset",
                      processTracked(processor, audio, midi));

  setParameter(*effectParams.compressorParams.enabled, 1.0f);
  results.requireZero("global compressor enable",
                      processTracked(processor, audio, midi));
  setParameter(*effectParams.compressorParams.threshold, -24.0f);
  setParameter(*effectParams.compressorParams.ratio, 7.0f);
  results.requireZero("global compressor parameter change",
                      processTracked(processor, audio, midi));
  setParameter(*effectParams.compressorParams.enabled, 0.0f);
  results.requireZero("global compressor disable",
                      processTracked(processor, audio, midi));

  setParameter(*effectParams.reverbParams.size, 0.91f);
  setParameter(*effectParams.reverbParams.predelay, 0.17f);
  setParameter(*effectParams.reverbParams.decay, 0.88f);
  results.requireZero("global reverb size predelay and decay change",
                      processTracked(processor, audio, midi));
  setParameter(*effectParams.reverbParams.enabled, 0.0f);
  results.requireZero("global reverb disable",
                      processTracked(processor, audio, midi));
  setParameter(*effectParams.reverbParams.enabled, 1.0f);
  results.requireZero("global reverb re-enable and reset",
                      processTracked(processor, audio, midi));

  setParameter(*effectParams.filterParams[0].enabled, 1.0f);
  results.requireZero("global filter 1 enable",
                      processTracked(processor, audio, midi));
  setParameter(*effectParams.filterParams[0].cutoff, 1777.0f);
  setParameter(*effectParams.filterParams[0].mode, 1.0f);
  results.requireZero("global filter 1 parameter change",
                      processTracked(processor, audio, midi));
  setParameter(*effectParams.filterParams[0].enabled, 0.0f);
  results.requireZero("global filter 1 disable",
                      processTracked(processor, audio, midi));

  setParameter(*effectParams.filterParams[1].enabled, 1.0f);
  results.requireZero("global filter 2 enable",
                      processTracked(processor, audio, midi));
  setParameter(*effectParams.filterParams[1].cutoff, 4321.0f);
  setParameter(*effectParams.filterParams[1].resonance, 3.0f);
  results.requireZero("global filter 2 parameter change",
                      processTracked(processor, audio, midi));
  setParameter(*effectParams.filterParams[1].enabled, 0.0f);
  results.requireZero("global filter 2 disable",
                      processTracked(processor, audio, midi));

  // The legacy internal parameter remains state-visible for preset/session
  // compatibility but is an audio tombstone. Changing it must not allocate or
  // reroute DSP.
  setParameter(*processor.synth.params.globalParams.legacyPolyEffectChain, 1.0f);
  results.requireZero("legacy poly FX tombstone to one",
                      processTracked(processor, audio, midi));
  setParameter(*processor.synth.params.globalParams.legacyPolyEffectChain, 0.0f);
  results.requireZero("legacy poly FX tombstone to zero",
                      processTracked(processor, audio, midi));
  results.requireZero("active DSP held after global FX changes",
                      processTracked(processor, audio, midi));

  // CC120 resets the enabled global delay/reverb at its exact MIDI boundary.
  // Include that newly added reset hook in the allocation window.
  midi.addEvent(juce::MidiMessage::controllerEvent(1, 120, 0), 37);
  results.requireZero("CC120 clears active global FX",
                      processTracked(processor, audio, midi));

  // Exercise the same bounded face-roll path before taking the stress RSS
  // baseline, so one-time page faults and lazy runtime setup are excluded.
  setParameter(*numVoices, 16.0f);
  processUntracked(processor, audio, midi);
  for (int event = 0; event < 2'048;) {
    const auto eventsThisBlock = juce::jmin(kBlockSize, 2'048 - event);
    for (int offset = 0; offset < eventsThisBlock; ++offset, ++event) {
      const auto channel = 2 + event % 15;
      const auto note = 24 + (event * 29) % 96;
      midi.addEvent(juce::MidiMessage::noteOn(
                        channel, note,
                        static_cast<juce::uint8>(70 + event % 58)),
                    offset);
    }
    processUntracked(processor, audio, midi);
  }
  processor.synth.panic();
  processUntracked(processor, audio, midi);
  for (int block = 0; block < 16; ++block)
    processUntracked(processor, audio, midi);

  const auto residentBeforeStress = residentBytes();
  std::uint64_t stressAllocationCalls = 0;
  for (int event = 0; event < kStressEvents;) {
    const auto eventsThisBlock = juce::jmin(kBlockSize, kStressEvents - event);
    for (int offset = 0; offset < eventsThisBlock; ++offset, ++event) {
      const auto channel = 2 + event % 15;
      const auto note = 24 + (event * 29 + (event / kBlockSize) * 11) % 96;
      midi.addEvent(juce::MidiMessage::noteOn(
                        channel, note,
                        static_cast<juce::uint8>(70 + event % 58)),
                    offset);
    }
    stressAllocationCalls += processTracked(processor, audio, midi);
  }
  const auto residentAfterStress = residentBytes();

  results.requireZero("100,000-event callback memory operations",
                      stressAllocationCalls);
  results.requireAtMost("100,000-event resident growth",
                        positiveGrowth(residentBeforeStress,
                                       residentAfterStress),
                        kStressResidentLimit);

  const auto telemetry = processor.synth.getVoiceTelemetry();
  results.requireEqual("dropped note-ons", telemetry.droppedNoteOns, 0);
  results.requireEqual("physical voice pool", telemetry.physicalPoolSize, 18);

  for (int index = 0; index < results.count; ++index) {
    const auto& check = results.checks[static_cast<std::size_t>(index)];
    const auto passed = check.exact ? check.actual == check.limit
                                    : check.actual <= check.limit;
    if (check.exact) {
      std::fprintf(stderr, "[%s] %s: %llu (expected %llu)\n",
                   passed ? "PASS" : "FAIL", check.name,
                   static_cast<unsigned long long>(check.actual),
                   static_cast<unsigned long long>(check.limit));
    } else {
      std::fprintf(stderr, "[%s] %s: %.2f MiB (limit %.2f MiB)\n",
                   passed ? "PASS" : "FAIL", check.name,
                   static_cast<double>(check.actual) / (1024.0 * 1024.0),
                   static_cast<double>(check.limit) / (1024.0 * 1024.0));
    }
  }

  return results.failed ? 1 : 0;
}
