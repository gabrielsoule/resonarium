#include <catch2/generators/catch_generators.hpp>
#include <catch2/catch_test_macros.hpp>
#include <gin_dsp/gin_dsp.h>

#include <array>
#include <cstdint>
#include <string>

namespace {

struct VoiceLog {
  int starts = 0;
  int stops = 0;
  int tailStops = 0;
  int hardStops = 0;
  int retriggers = 0;
  int pressureChanges = 0;
  int pitchChanges = 0;
  int timbreChanges = 0;
  int retirements = 0;
  std::uint64_t lastStopGeneration = 0;
  std::uint64_t lastPressureGeneration = 0;
  gin::VoiceEndReason lastEndReason = gin::VoiceEndReason::naturalTail;
};

class ConstantVoice final : public gin::SynthesiserVoice {
 public:
  explicit ConstantVoice(VoiceLog& logToUse, bool finishWhenStopped = false)
      : log(logToUse), finishWhenStopped(finishWhenStopped) {}

  void noteStarted() override { ++log.starts; }
  void noteStopped(bool allowTailOff) override {
    ++log.stops;
    if (allowTailOff)
      ++log.tailStops;
    else
      ++log.hardStops;
    log.lastStopGeneration = getVoiceGeneration();
    if (finishWhenStopped) finishTail();
  }
  void noteRetriggered() override { ++log.retriggers; }
  void notePressureChanged() override {
    ++log.pressureChanges;
    log.lastPressureGeneration = getVoiceGeneration();
  }
  void notePitchbendChanged() override { ++log.pitchChanges; }
  void noteTimbreChanged() override { ++log.timbreChanges; }
  void noteKeyStateChanged() override {}

  float getCurrentNote() override {
    return static_cast<float>(getCurrentlyPlayingNote().initialNote);
  }

  void renderNextBlock(juce::AudioBuffer<float>& buffer, int startSample,
                       int numSamples) override {
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel) {
      auto* samples = buffer.getWritePointer(channel, startSample);
      for (int sample = 0; sample < numSamples; ++sample) samples[sample] += 1.0f;
    }
  }

  void voiceRetired(gin::VoiceEndReason reason) override {
    ++log.retirements;
    log.lastEndReason = reason;
  }

 private:
  VoiceLog& log;
  bool finishWhenStopped = false;
};

void sendNoteOn(gin::Synthesiser& synth, int channel, int note) {
  synth.processDirectMidiEvent(juce::MidiMessage::noteOn(channel, note, 1.0f));
}

void render(gin::Synthesiser& synth, juce::AudioBuffer<float>& output,
            int numSamples) {
  juce::MidiBuffer midi;
  output.clear();
  synth.renderNextBlock(output, midi, 0, numSamples);
}

}  // namespace

TEST_CASE("Voice manager requires and constructs an exact physical pool",
          "[voice][lifecycle][pool]") {
  gin::VoicePoolConfig config;
  config.maxPolyphony = 2;
  config.fastFadeSlots = 1;
  config.initialScratchSamples = 128;

  std::array<VoiceLog, 3> incompleteLogs;
  gin::Synthesiser incomplete(config);
  incomplete.addVoice(new ConstantVoice(incompleteLogs[0]));
  incomplete.addVoice(new ConstantVoice(incompleteLogs[1]));
  REQUIRE_FALSE(incomplete.prepareVoiceManager(48'000.0, 128, 2));

  std::array<VoiceLog, 3> completeLogs;
  gin::Synthesiser complete(config);
  int factoryCalls = 0;
  REQUIRE(complete.initialiseVoicePool([&](int index) -> gin::SynthesiserVoice* {
    ++factoryCalls;
    return new ConstantVoice(completeLogs[static_cast<std::size_t>(index)]);
  }));
  REQUIRE(factoryCalls == 3);
  REQUIRE(complete.getVoiceTelemetry().physicalPoolSize == 3);
  REQUIRE(complete.prepareVoiceManager(48'000.0, 128, 2));
}

TEST_CASE("Sliced rendering publishes one coherent host-block snapshot",
          "[voice][lifecycle][telemetry]") {
  gin::VoicePoolConfig config;
  config.maxPolyphony = 2;
  config.fastFadeSlots = 0;
  config.initialScratchSamples = 128;

  std::array<VoiceLog, 2> logs;
  gin::Synthesiser synth(config);
  REQUIRE(synth.initialiseVoicePool([&](int index) -> gin::SynthesiserVoice* {
    return new ConstantVoice(logs[static_cast<std::size_t>(index)]);
  }));
  REQUIRE(synth.prepareVoiceManager(48'000.0, 128, 2));
  synth.setVoiceLimit(2);

  sendNoteOn(synth, 2, 60);
  sendNoteOn(synth, 3, 64);

  juce::AudioBuffer<float> output(2, 128);
  juce::MidiBuffer midi;
  synth.startBlock();
  synth.beginVoiceHostBlock();
  synth.renderNextBlock(output, midi, 0, 32);
  synth.panic();
  synth.renderNextBlock(output, midi, 32, 32);
  synth.endBlock(64);

  const auto telemetry = synth.getVoiceTelemetry();
  REQUIRE(telemetry.renderedVoices == 2);
  REQUIRE(telemetry.musicalVoices == 0);
  REQUIRE(telemetry.physicalPoolSize == 2);
}

TEST_CASE("Voice manager publishes its fixed-pool contract",
          "[voice][lifecycle]") {
  gin::VoicePoolConfig config;
  config.maxPolyphony = 2;
  config.fastFadeSlots = 2;
  config.initialScratchSamples = 128;

  gin::Synthesiser synth(config);
  std::array<VoiceLog, 4> logs;
  for (auto& log : logs) synth.addVoice(new ConstantVoice(log));
  REQUIRE(synth.prepareVoiceManager(48'000.0, 128, 2));
  synth.setVoiceLimit(2);

  auto telemetry = synth.getVoiceTelemetry();
  REQUIRE(telemetry.configuredVoiceLimit == 2);
  REQUIRE(telemetry.physicalPoolSize == 4);
  REQUIRE(telemetry.noteStarts == 0);
  REQUIRE(telemetry.droppedNoteOns == 0);

  sendNoteOn(synth, 2, 60);
  sendNoteOn(synth, 3, 64);
  juce::AudioBuffer<float> output(2, 128);
  render(synth, output, 128);

  // Two replacements exhaust both overlap slots. Render one sample so the
  // replacement voices themselves become eligible for a bounded fade, then a
  // third replacement must force-retire the oldest fade without dropping.
  sendNoteOn(synth, 4, 67);
  sendNoteOn(synth, 5, 71);
  render(synth, output, 1);
  sendNoteOn(synth, 6, 74);

  telemetry = synth.getVoiceTelemetry();
  REQUIRE(telemetry.noteStarts == 5);
  REQUIRE(telemetry.steals == 3);
  REQUIRE(telemetry.forcedFadeRetirements == 1);
  REQUIRE(telemetry.droppedNoteOns == 0);
  REQUIRE(telemetry.musicalVoices == 2);
  REQUIRE(telemetry.fastFadeVoices == 2);
  REQUIRE(telemetry.musicalVoices <= telemetry.configuredVoiceLimit);
  REQUIRE(telemetry.fastFadeVoices <= config.fastFadeSlots);

  REQUIRE(logs[0].retirements == 1);
  REQUIRE(logs[0].lastEndReason == gin::VoiceEndReason::forcedFastFade);
}

TEST_CASE("Voice stealing applies an exact three-millisecond linear fade",
          "[voice][lifecycle][fade]") {
  const auto sampleRate = GENERATE(44'100.0, 48'000.0, 96'000.0);
  const auto fadeSamples = juce::roundToInt(sampleRate * 0.003);

  gin::VoicePoolConfig config;
  config.maxPolyphony = 1;
  config.fastFadeSlots = 1;
  config.fastFadeMs = 3.0;
  config.initialScratchSamples = fadeSamples;

  gin::Synthesiser synth(config);
  std::array<VoiceLog, 2> logs;
  synth.addVoice(new ConstantVoice(logs[0], true));
  synth.addVoice(new ConstantVoice(logs[1]));
  REQUIRE(synth.prepareVoiceManager(sampleRate, fadeSamples, 2));
  synth.setVoiceLimit(1);

  juce::AudioBuffer<float> output(2, fadeSamples);
  sendNoteOn(synth, 2, 60);
  render(synth, output, 1);  // A rendered voice is eligible for overlap fading.
  sendNoteOn(synth, 3, 67);
  render(synth, output, fadeSamples);

  for (int channel = 0; channel < output.getNumChannels(); ++channel) {
    REQUIRE(output.getSample(channel, 0) == 2.0f);
    REQUIRE(output.getSample(channel, fadeSamples - 1) == 1.0f);
  }

  const auto telemetry = synth.getVoiceTelemetry();
  REQUIRE(telemetry.noteStarts == 2);
  REQUIRE(telemetry.fastFadeVoices == 0);
  REQUIRE(telemetry.musicalVoices == 1);
  REQUIRE(telemetry.renderedVoices == 2);
  REQUIRE(logs[0].retirements == 1);
  REQUIRE(logs[0].lastEndReason == gin::VoiceEndReason::fastFadeComplete);
}

TEST_CASE("Released victims are not sent duplicate release callbacks",
          "[voice][lifecycle][generation]") {
  gin::VoicePoolConfig config;
  config.maxPolyphony = 1;
  config.fastFadeSlots = 1;
  config.initialScratchSamples = 256;

  gin::Synthesiser synth(config);
  std::array<VoiceLog, 2> logs;
  for (auto& log : logs) synth.addVoice(new ConstantVoice(log));
  REQUIRE(synth.prepareVoiceManager(48'000.0, 256, 2));
  synth.setVoiceLimit(1);

  juce::AudioBuffer<float> output(2, 256);
  sendNoteOn(synth, 2, 60);
  render(synth, output, 64);
  synth.processDirectMidiEvent(juce::MidiMessage::noteOff(2, 60));
  REQUIRE(logs[0].stops == 1);

  sendNoteOn(synth, 2, 60);
  REQUIRE(logs[0].stops == 1);
  REQUIRE(synth.getVoiceTelemetry().noteStarts == 2);
  REQUIRE(synth.getVoiceTelemetry().droppedNoteOns == 0);
}

TEST_CASE("One hundred same-identity retriggers route only to the newest generation",
          "[voice][lifecycle][generation][mpe]") {
  gin::VoicePoolConfig config;
  config.maxPolyphony = 1;
  config.fastFadeSlots = 2;
  config.initialScratchSamples = 16;

  gin::Synthesiser synth(config);
  std::array<VoiceLog, 3> logs;
  REQUIRE(synth.initialiseVoicePool([&](int index) -> gin::SynthesiserVoice* {
    return new ConstantVoice(logs[static_cast<std::size_t>(index)]);
  }));
  REQUIRE(synth.prepareVoiceManager(48'000.0, 16, 2));
  synth.setVoiceLimit(1);

  juce::AudioBuffer<float> output(2, 16);
  sendNoteOn(synth, 2, 60);
  render(synth, output, 1);

  for (int retrigger = 0; retrigger < 100; ++retrigger) {
    sendNoteOn(synth, 2, 60);
    render(synth, output, 1);
  }

  const auto telemetry = synth.getVoiceTelemetry();
  REQUIRE(telemetry.noteStarts == 101);
  REQUIRE(telemetry.droppedNoteOns == 0);
  REQUIRE(telemetry.musicalVoices == 1);
  REQUIRE(telemetry.fastFadeVoices == 2);

  int newestVoiceIndex = -1;
  std::uint64_t newestGeneration = 0;
  std::array<int, 3> pressureBefore{};
  std::array<int, 3> stopsBefore{};

  for (int index = 0; index < 3; ++index) {
    const auto* voice = synth.getVoice(index);
    pressureBefore[static_cast<std::size_t>(index)] =
        logs[static_cast<std::size_t>(index)].pressureChanges;
    stopsBefore[static_cast<std::size_t>(index)] =
        logs[static_cast<std::size_t>(index)].stops;

    if (voice->getManagerVoiceState() == gin::VoiceState::held) {
      REQUIRE(newestVoiceIndex == -1);
      newestVoiceIndex = index;
      newestGeneration = voice->getVoiceGeneration();
    }
  }

  REQUIRE(newestVoiceIndex >= 0);
  REQUIRE(newestGeneration > 100);

  synth.processDirectMidiEvent(
      juce::MidiMessage::channelPressureChange(2, 91));

  for (int index = 0; index < 3; ++index) {
    const auto& log = logs[static_cast<std::size_t>(index)];
    REQUIRE(log.pressureChanges ==
            pressureBefore[static_cast<std::size_t>(index)] +
                (index == newestVoiceIndex ? 1 : 0));
  }
  REQUIRE(logs[static_cast<std::size_t>(newestVoiceIndex)]
              .lastPressureGeneration == newestGeneration);

  synth.processDirectMidiEvent(juce::MidiMessage::noteOff(2, 60));

  for (int index = 0; index < 3; ++index) {
    const auto& log = logs[static_cast<std::size_t>(index)];
    REQUIRE(log.stops == stopsBefore[static_cast<std::size_t>(index)] +
                             (index == newestVoiceIndex ? 1 : 0));
  }
  REQUIRE(logs[static_cast<std::size_t>(newestVoiceIndex)]
              .lastStopGeneration == newestGeneration);

  // Once the newest binding is released, duplicate identity events cannot
  // reach either its private tail or any older stolen/fading generation.
  const auto logsAfterRelease = logs;
  synth.processDirectMidiEvent(
      juce::MidiMessage::channelPressureChange(2, 37));
  synth.processDirectMidiEvent(juce::MidiMessage::noteOff(2, 60));

  for (std::size_t index = 0; index < logs.size(); ++index) {
    REQUIRE(logs[index].pressureChanges ==
            logsAfterRelease[index].pressureChanges);
    REQUIRE(logs[index].stops == logsAfterRelease[index].stops);
  }
}

TEST_CASE("CC123 naturally releases sustained notes only from the MPE master",
          "[voice][lifecycle][mpe][sustain][panic]") {
  gin::VoicePoolConfig config;
  config.maxPolyphony = 2;
  config.fastFadeSlots = 2;
  config.initialScratchSamples = 16;

  gin::Synthesiser synth(config);
  std::array<VoiceLog, 4> logs;
  REQUIRE(synth.initialiseVoicePool([&](int index) -> gin::SynthesiserVoice* {
    return new ConstantVoice(logs[static_cast<std::size_t>(index)], true);
  }));
  REQUIRE(synth.prepareVoiceManager(48'000.0, 16, 2));

  sendNoteOn(synth, 2, 60);
  sendNoteOn(synth, 3, 64);
  synth.processDirectMidiEvent(
      juce::MidiMessage::controllerEvent(1, 64, 127));
  synth.processDirectMidiEvent(juce::MidiMessage::noteOff(2, 60));
  synth.processDirectMidiEvent(juce::MidiMessage::noteOff(3, 64));

  REQUIRE(synth.getVoiceTelemetry().musicalVoices == 2);
  for (const auto& log : logs) REQUIRE(log.stops == 0);

  // CC123 on an MPE member is out of scope; on the zone master it releases
  // every sustained member naturally and permits each private tail to finish.
  synth.processDirectMidiEvent(juce::MidiMessage::allNotesOff(2));
  REQUIRE(synth.getVoiceTelemetry().musicalVoices == 2);
  for (const auto& log : logs) REQUIRE(log.stops == 0);

  synth.processDirectMidiEvent(juce::MidiMessage::allNotesOff(1));
  REQUIRE(synth.getVoiceTelemetry().musicalVoices == 0);
  REQUIRE(synth.getVoiceTelemetry().fastFadeVoices == 0);

  int naturalRetirements = 0;
  int tailStops = 0;
  int hardStops = 0;
  for (const auto& log : logs) {
    naturalRetirements += log.retirements;
    tailStops += log.tailStops;
    hardStops += log.hardStops;
    if (log.retirements > 0)
      REQUIRE(log.lastEndReason == gin::VoiceEndReason::naturalTail);
  }
  REQUIRE(naturalRetirements == 2);
  REQUIRE(tailStops == 2);
  REQUIRE(hardStops == 0);
}

TEST_CASE("CC120 and panic immediately hard-retire sustained voices",
          "[voice][lifecycle][mpe][sustain][panic]") {
  gin::VoicePoolConfig config;
  config.maxPolyphony = 2;
  config.fastFadeSlots = 2;
  config.initialScratchSamples = 16;

  gin::Synthesiser synth(config);
  std::array<VoiceLog, 4> logs;
  REQUIRE(synth.initialiseVoicePool([&](int index) -> gin::SynthesiserVoice* {
    return new ConstantVoice(logs[static_cast<std::size_t>(index)], true);
  }));
  REQUIRE(synth.prepareVoiceManager(48'000.0, 16, 2));

  sendNoteOn(synth, 2, 60);
  sendNoteOn(synth, 3, 64);
  synth.processDirectMidiEvent(
      juce::MidiMessage::controllerEvent(1, 64, 127));
  synth.processDirectMidiEvent(juce::MidiMessage::noteOff(2, 60));
  synth.processDirectMidiEvent(juce::MidiMessage::noteOff(3, 64));
  REQUIRE(synth.getVoiceTelemetry().musicalVoices == 2);

  synth.processDirectMidiEvent(juce::MidiMessage::allSoundOff(1));
  REQUIRE(synth.getVoiceTelemetry().musicalVoices == 0);
  REQUIRE(synth.getVoiceTelemetry().fastFadeVoices == 0);

  int panicRetirements = 0;
  int tailStops = 0;
  int hardStops = 0;
  for (const auto& log : logs) {
    panicRetirements += log.retirements;
    tailStops += log.tailStops;
    hardStops += log.hardStops;
    if (log.retirements > 0)
      REQUIRE(log.lastEndReason == gin::VoiceEndReason::panic);
  }
  REQUIRE(panicRetirements == 2);
  REQUIRE(tailStops == 0);
  REQUIRE(hardStops == 2);

  // The UI/audio command path uses the same hard-retirement contract.
  synth.processDirectMidiEvent(
      juce::MidiMessage::controllerEvent(1, 64, 0));
  sendNoteOn(synth, 4, 67);
  const auto retirementsBeforePanic = panicRetirements;
  const auto hardStopsBeforePanic = hardStops;
  synth.panic();
  REQUIRE(synth.getVoiceTelemetry().musicalVoices == 0);
  REQUIRE(synth.getVoiceTelemetry().fastFadeVoices == 0);

  panicRetirements = 0;
  hardStops = 0;
  for (const auto& log : logs) {
    panicRetirements += log.retirements;
    hardStops += log.hardStops;
    if (log.retirements > 0)
      REQUIRE(log.lastEndReason == gin::VoiceEndReason::panic);
  }
  REQUIRE(panicRetirements == retirementsBeforePanic + 1);
  REQUIRE(hardStops == hardStopsBeforePanic + 1);
}

TEST_CASE("Mono same-note presses create new generations",
          "[voice][lifecycle][mono][generation]") {
  const auto mode = GENERATE(gin::PlayMode::monoLegato,
                             gin::PlayMode::monoRetrigger);

  gin::VoicePoolConfig config;
  config.maxPolyphony = 1;
  config.fastFadeSlots = 1;
  config.initialScratchSamples = 128;

  gin::Synthesiser synth(config);
  std::array<VoiceLog, 2> logs;
  for (auto& log : logs) synth.addVoice(new ConstantVoice(log));
  REQUIRE(synth.prepareVoiceManager(48'000.0, 128, 2));
  synth.setPlayMode(mode);

  juce::AudioBuffer<float> output(2, 128);
  sendNoteOn(synth, 2, 60);
  render(synth, output, 64);
  sendNoteOn(synth, 2, 60);
  render(synth, output, 64);

  REQUIRE(synth.getVoiceTelemetry().noteStarts == 2);
  REQUIRE(synth.getVoiceTelemetry().musicalVoices == 1);
  REQUIRE(synth.getVoiceTelemetry().droppedNoteOns == 0);

  if (mode == gin::PlayMode::monoLegato) {
    REQUIRE(logs[0].starts == 1);
    REQUIRE(logs[0].retriggers == 1);
  } else {
    REQUIRE(logs[0].stops == 1);
    REQUIRE(logs[0].starts + logs[1].starts == 2);
  }
}

TEST_CASE("Lower-zone master notes are rejected while master expression remains active",
          "[voice][lifecycle][mpe]") {
  gin::VoicePoolConfig config;
  config.maxPolyphony = 1;
  config.fastFadeSlots = 1;
  config.initialScratchSamples = 128;

  gin::Synthesiser synth(config);
  std::array<VoiceLog, 2> logs;
  for (auto& log : logs) synth.addVoice(new ConstantVoice(log));
  REQUIRE(synth.prepareVoiceManager(48'000.0, 128, 2));

  sendNoteOn(synth, 1, 60);
  REQUIRE(synth.getVoiceTelemetry().noteStarts == 0);
  REQUIRE(synth.getVoiceTelemetry().musicalVoices == 0);

  sendNoteOn(synth, 2, 60);
  REQUIRE(synth.getVoiceTelemetry().noteStarts == 1);
  REQUIRE(synth.getVoiceTelemetry().musicalVoices == 1);

  synth.processDirectMidiEvent(juce::MidiMessage::pitchWheel(1, 10'000));
  REQUIRE(logs[0].pitchChanges + logs[1].pitchChanges == 1);

  // Incoming RPN configuration must not replace the product's fixed 15-member
  // lower zone or its bend ranges.
  const auto attemptedLayout = juce::MPEMessages::setLowerZone(1, 12, 12);
  for (const auto metadata : attemptedLayout)
    synth.processDirectMidiEvent(metadata.getMessage());

  sendNoteOn(synth, 16, 67);
  REQUIRE(synth.getVoiceTelemetry().noteStarts == 2);
  REQUIRE(synth.getVoiceTelemetry().droppedNoteOns == 0);
}

TEST_CASE("One hundred thousand direct events preserve every voice bound",
          "[voice][lifecycle][stress]") {
  gin::VoicePoolConfig config;
  config.maxPolyphony = 16;
  config.fastFadeSlots = 2;
  config.initialScratchSamples = 16;

  gin::Synthesiser synth(config);
  std::array<VoiceLog, 18> logs;
  REQUIRE(synth.initialiseVoicePool([&](int index) -> gin::SynthesiserVoice* {
    return new ConstantVoice(logs[static_cast<std::size_t>(index)]);
  }));
  REQUIRE(synth.prepareVoiceManager(48'000.0, 16, 2));

  juce::AudioBuffer<float> output(2, 16);
  std::uint32_t random = 0x6d2b79f5u;
  std::uint64_t acceptedNoteOns = 0;
  std::string failure;

  const auto nextRandom = [&]() {
    random ^= random << 13;
    random ^= random >> 17;
    random ^= random << 5;
    return random;
  };

  for (int eventIndex = 0; eventIndex < 100'000; ++eventIndex) {
    if ((eventIndex % 1024) == 0)
      synth.setVoiceLimit(1 + (eventIndex / 1024) % 16);

    const auto value = nextRandom();
    const auto channel = 2 + static_cast<int>((value >> 8) % 15);
    const auto note = 24 + static_cast<int>((value >> 16) % 96);
    const auto operation = static_cast<int>(value % 20);

    if ((eventIndex % 4096) == 4095) {
      synth.processDirectMidiEvent(
          juce::MidiMessage::controllerEvent(1, 120, 0));
    } else if (operation < 12) {
      sendNoteOn(synth, channel, note);
      ++acceptedNoteOns;
    } else if (operation < 16) {
      synth.processDirectMidiEvent(juce::MidiMessage::noteOff(channel, note));
    } else if (operation < 18) {
      synth.processDirectMidiEvent(
          juce::MidiMessage::pitchWheel(channel, 4096 + static_cast<int>(value % 8192)));
    } else if (operation == 18) {
      synth.processDirectMidiEvent(
          juce::MidiMessage::controllerEvent(1, 64, (value & 1u) != 0u ? 127 : 0));
    } else {
      synth.processDirectMidiEvent(
          juce::MidiMessage::channelPressureChange(channel, static_cast<int>(value % 128)));
    }

    if ((eventIndex % 16) == 15)
      render(synth, output, 16);

    const auto telemetry = synth.getVoiceTelemetry();
    const auto withinBounds =
        telemetry.musicalVoices <= telemetry.configuredVoiceLimit &&
        telemetry.fastFadeVoices <= 2 &&
        telemetry.musicalVoices + telemetry.fastFadeVoices <=
            telemetry.configuredVoiceLimit + 2 &&
        telemetry.physicalPoolSize == 18 && telemetry.droppedNoteOns == 0 &&
        telemetry.noteStarts == acceptedNoteOns;

    if (!withinBounds) {
      failure = "voice invariant failed after event " +
                std::to_string(eventIndex);
      break;
    }
  }

  REQUIRE(failure.empty());
  synth.processDirectMidiEvent(juce::MidiMessage::controllerEvent(1, 120, 0));
  REQUIRE(synth.getVoiceTelemetry().musicalVoices == 0);
  REQUIRE(synth.getVoiceTelemetry().fastFadeVoices == 0);
}
