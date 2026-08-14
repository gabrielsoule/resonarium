#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>
#include <chrono>
#include <future>
#include <limits>
#include "PluginProcessor.h"
#include "ResonatorVoice.h"

namespace py = pybind11;

/**
* A read-only struct that holds information about a parameter.
*/
struct PyParameterInfo
{
    std::string id;
    std::string name;
    float currentValue; //current (user) value
    float defaultValue; //default (user) value
    float minValue; //min (user) value
    float maxValue; //max (user) value

    std::string toString() const
    {
        return name + " [" + std::to_string(currentValue) + "]";
    }
};

/**
* A struct that provides information about a modulation source.
*/
struct PyModSourceInfo
{
    std::string id;
    std::string name;
    bool isPoly;
    bool isBipolar;

    std::string toString() const
    {
        return name + " [" + id + "]";
    }
};

class ResonariumWrapper
{
private:
    std::unique_ptr<ResonariumProcessor> processor;
    juce::AudioBuffer<float> tempBuffer; //holds a single block of memory
    juce::MidiBuffer midiBuffer;

    std::uint64_t processInjectedInputBlock(const float* left, const float* right, bool measure)
    {
        tempBuffer.copyFrom(0, 0, left, getBlockSize());
        tempBuffer.copyFrom(1, 0, right, getBlockSize());

        // Exercise the real processor callback, including constant-note/MIDI
        // reconciliation and exactly the same shared-input capture as Effect.
        const auto started = std::chrono::steady_clock::now();
        processor->processBlock(tempBuffer, midiBuffer);
        const auto finished = std::chrono::steady_clock::now();
        midiBuffer.clear();
        return measure ? static_cast<std::uint64_t>(
                             std::chrono::duration_cast<std::chrono::nanoseconds>(finished - started).count())
                       : 0;
    }

public:
    ResonariumWrapper(double sampleRate = 44100.0, int blockSize = 512)
        : tempBuffer(2, blockSize)
    {
        processor = std::make_unique<ResonariumProcessor>();
        auto layout = processor->getBusesLayout();
        if (layout.inputBuses.isEmpty() || layout.outputBuses.isEmpty())
            throw std::runtime_error("Python processor has no main stereo buses");
        layout.inputBuses.set(0, juce::AudioChannelSet::stereo());
        layout.outputBuses.set(0, juce::AudioChannelSet::stereo());
        if (!processor->setBusesLayout(layout))
            throw std::runtime_error("Python processor rejected stereo input/output buses");
        processor->prepareToPlay(sampleRate, blockSize);
        processor->setRateAndBufferSizeDetails(sampleRate, blockSize);
    }

    ~ResonariumWrapper()
    {
        if (processor)
        {
            processor->releaseResources();
        }
    }

    double getSampleRate() const
    {
        return processor->getSampleRate();
    }

    int getBlockSize() const
    {
        return processor->getBlockSize();
    }

    void reset() const
    {
        processor->reset();
        processor->prepareToPlay(processor->getSampleRate(), processor->getBlockSize());
    }

    // MIDI Control
    void playNote(int channel, int note, int velocity, int sampleOffset)
    {
        if (sampleOffset < 0 || sampleOffset >= getBlockSize())
            throw std::runtime_error("MIDI sample offset must be within the next processing block");

        // Queue note-on message
        auto msg = juce::MidiMessage::noteOn(channel + 1, note, static_cast<uint8_t>(velocity));
        midiBuffer.addEvent(msg, sampleOffset);
    }

    void releaseNote(int channel, int note, int velocity, int sampleOffset)
    {
        if (sampleOffset < 0 || sampleOffset >= getBlockSize())
            throw std::runtime_error("MIDI sample offset must be within the next processing block");

        auto msg = juce::MidiMessage::noteOff(channel + 1, note, static_cast<uint8_t>(velocity));
        midiBuffer.addEvent(msg, sampleOffset);
    }

    void allNotesOff(int sampleOffset)
    {
        if (sampleOffset < 0 || sampleOffset >= getBlockSize())
            throw std::runtime_error("MIDI sample offset must be within the next processing block");

        for (int channel = 1; channel <= 16; ++channel)
        {
            auto msg = juce::MidiMessage::allNotesOff(channel);
            midiBuffer.addEvent(msg, sampleOffset);
        }
    }

    /**
     * Queue one raw MIDI 1.0 channel message for the next processing block.
     *
     * The channel nibble in the status byte is the real MIDI channel (zero based,
     * as specified by MIDI), unlike playNote()'s historical Python channel
     * argument.  Keeping this adapter byte-exact lets the lifecycle harness cover
     * MPE pressure, timbre, pitch bend, sustain and panic without adding a new
     * binding for every message type.
     */
    void queueMidi(py::bytes bytes, int sampleOffset)
    {
        if (sampleOffset < 0 || sampleOffset >= getBlockSize())
            throw std::runtime_error("MIDI sample offset must be within the next processing block");

        const auto data = static_cast<std::string>(bytes);
        if (data.empty() || data.size() > 3)
            throw std::runtime_error("Raw MIDI adapter accepts one 1-3 byte channel message");

        const auto status = static_cast<uint8_t>(data.front());
        if (status < 0x80 || status >= 0xf0)
            throw std::runtime_error("Raw MIDI adapter requires a MIDI channel-message status byte");

        const auto messageType = static_cast<uint8_t>(status & 0xf0);
        const auto expectedSize = (messageType == 0xc0 || messageType == 0xd0) ? 2u : 3u;
        if (data.size() != expectedSize)
            throw std::runtime_error("Raw MIDI message has the wrong number of data bytes");

        for (size_t i = 1; i < data.size(); ++i)
            if ((static_cast<uint8_t>(data[i]) & 0x80u) != 0)
                throw std::runtime_error("Raw MIDI data bytes must be in the range 0-127");

        midiBuffer.addEvent(juce::MidiMessage(data.data(), static_cast<int>(data.size())), sampleOffset);
    }

    std::string getStateXml() const
    {
        return processor->getStateXml().toStdString();
    }

    std::vector<std::string> getProgramNames() const
    {
        std::vector<std::string> names;
        for (const auto* program : processor->getPrograms())
            names.push_back(program->name.toStdString());
        return names;
    }

    void setCurrentProgram(const std::string& name) const
    {
        processor->setCurrentProgram(juce::String(name));
    }

    py::dict getVoiceTelemetry() const
    {
        const auto telemetry = processor->getSynth()->getVoiceTelemetry();
        py::dict result;
        result["configured_voice_limit"] = telemetry.configuredVoiceLimit;
        result["musical_voices"] = telemetry.musicalVoices;
        result["fast_fade_voices"] = telemetry.fastFadeVoices;
        result["rendered_voices"] = telemetry.renderedVoices;
        result["physical_pool_size"] = telemetry.physicalPoolSize;
        result["note_starts"] = telemetry.noteStarts;
        result["steals"] = telemetry.steals;
        result["forced_fade_retirements"] = telemetry.forcedFadeRetirements;
        result["tail_retirements"] = telemetry.tailRetirements;
        result["dropped_note_ons"] = telemetry.droppedNoteOns;
        return result;
    }

    py::dict getExciterTelemetry() const
    {
        std::array<std::uint64_t, static_cast<std::size_t>(ExciterKind::count)> generated {};
        std::uint64_t filterRebuilds = 0;
        std::uint64_t externalVoiceReads = 0;
        std::uint64_t assignments = 0;
        std::uint64_t snapshotAcquires = 0;
        std::uint64_t snapshotReleases = 0;
        py::list voices;

        for (int index = 0; index < processor->getSynth()->getNumVoices(); ++index)
        {
            const auto* voice = dynamic_cast<const ResonatorVoice*>(processor->getSynth()->getVoice(index));
            if (voice == nullptr)
                continue;
            const auto& telemetry = voice->getExciterRackForTesting().getTelemetry();
            py::dict item;
            item["voice_index"] = index;
            item["enabled_mask"] = telemetry.enabledMask;
            item["initialized_mask"] = telemetry.initializedMask;
            item["pending_reset_mask"] = telemetry.pendingResetMask;
            voices.append(std::move(item));
            for (std::size_t kind = 0; kind < generated.size(); ++kind)
                generated[kind] += telemetry.sourceSamplesGenerated[kind];
            filterRebuilds += telemetry.filterCoefficientRebuilds;
            externalVoiceReads += telemetry.externalVoiceReads;
            assignments += telemetry.assignments;
            snapshotAcquires += telemetry.snapshotAcquires;
            snapshotReleases += telemetry.snapshotReleases;
        }

        py::list sourceSamples;
        for (const auto count : generated)
            sourceSamples.append(count);
        py::dict result;
        result["voices"] = std::move(voices);
        result["source_samples_generated"] = std::move(sourceSamples);
        result["filter_coefficient_rebuilds"] = filterRebuilds;
        result["external_voice_reads"] = externalVoiceReads;
        result["assignments"] = assignments;
        result["snapshot_acquires"] = snapshotAcquires;
        result["snapshot_releases"] = snapshotReleases;
        return result;
    }

    void clearExciterTelemetry() const
    {
        for (int index = 0; index < processor->getSynth()->getNumVoices(); ++index)
            if (auto* voice = dynamic_cast<ResonatorVoice*>(processor->getSynth()->getVoice(index)))
                voice->getExciterRackForTesting().clearTelemetry();
    }

#if defined (RESONARIUM_TEST_INSTRUMENTATION)
    void setPlayModeForTesting(const std::string& mode) const
    {
        if (mode == "poly")
            processor->getSynth()->setPlayMode(gin::PlayMode::poly);
        else if (mode == "mono_retrigger")
            processor->getSynth()->setPlayMode(gin::PlayMode::monoRetrigger);
        else if (mode == "mono_legato")
            processor->getSynth()->setPlayMode(gin::PlayMode::monoLegato);
        else
            throw std::runtime_error(
                "Play mode must be poly, mono_retrigger, or mono_legato");
    }

    py::array_t<float> renderExciterPrngForTesting(int voiceId, std::uint64_t generation,
                                                   int kind, std::uint32_t substream,
                                                   int numSamples) const
    {
        if (kind < 0 || kind >= static_cast<int>(ExciterKind::count))
            throw std::runtime_error("Exciter kind must be in the range 0-4");
        if (numSamples < 0 || numSamples > 1'000'000)
            throw std::runtime_error("PRNG test length must be in the range 0-1000000");
        py::array_t<float> result(numSamples);
        auto view = result.mutable_unchecked<1>();
        VoiceExciterRack::renderPrngSequenceForTesting(
            voiceId, generation, static_cast<ExciterKind>(kind), substream,
            std::span<float>(view.mutable_data(0), static_cast<std::size_t>(numSamples)));
        return result;
    }

    py::dict getGlobalEffectTestCallCounters() const
    {
        const auto counters = processor->getSynth()->getGlobalEffectTestCallCounters();
        py::dict result;
        result["resets"] = counters.resets;
        result["parameter_updates"] = counters.parameterUpdates;
        result["process_calls"] = counters.processCalls;
        return result;
    }

    void clearGlobalEffectTestCallCounters() const
    {
        processor->getSynth()->clearGlobalEffectTestCallCounters();
    }

    py::dict getEffectChainTopology() const
    {
        const auto topology = processor->getSynth()->getEffectChainTopology();
        py::dict result;
        result["global_instances"] = topology.globalInstances;
        result["voice_instances"] = topology.voiceInstances;
        result["total_instances"] = topology.totalInstances;
        return result;
    }

    py::dict getExternalInputTelemetry() const
    {
        const auto& telemetry = processor->getSynth()->getExternalInputFrame().getTelemetry();
        py::dict result;
        result["host_blocks"] = telemetry.hostBlocks;
        result["valid_blocks"] = telemetry.validBlocks;
        result["source_samples_copied"] = telemetry.sourceSamplesCopied;
        return result;
    }

    void clearExternalInputTelemetry() const
    {
        processor->getSynth()->getExternalInputFrameForTesting().clearTelemetry();
    }

    std::uint64_t installSampleForTesting(
        py::array_t<float, py::array::c_style | py::array::forcecast> source,
        double sourceSampleRate) const
    {
        const auto view = source.request();
        if (view.ndim != 2 || view.shape[0] < 1 || view.shape[1] < 1)
            throw std::runtime_error("Test sample must have shape (channels, samples)");
        if (view.shape[0] > std::numeric_limits<int>::max()
            || view.shape[1] > std::numeric_limits<int>::max())
            throw std::runtime_error("Test sample dimensions exceed the native adapter limits");

        const auto channels = static_cast<int>(view.shape[0]);
        const auto samples = static_cast<int>(view.shape[1]);
        const auto* data = static_cast<const float*>(view.ptr);
        juce::AudioBuffer<float> audio(channels, samples);
        for (int channel = 0; channel < channels; ++channel)
            audio.copyFrom(channel, 0, data + static_cast<std::size_t>(channel) * samples, samples);

        auto completion = std::make_shared<std::promise<Sampler::LoadResult>>();
        auto future = completion->get_future();
        const auto accepted = processor->globalState.sampler.requestInstallForTesting(
            audio, sourceSampleRate,
            [completion](Sampler::LoadResult result) { completion->set_value(std::move(result)); });
        if (!accepted)
            throw std::runtime_error("Sampler rejected the in-memory test snapshot");
        if (future.wait_for(std::chrono::seconds(5)) != std::future_status::ready)
            throw std::runtime_error("Timed out waiting for the sampler worker");
        const auto result = future.get();
        if (!result.success)
            throw std::runtime_error("Sampler worker failed: " + result.error.toStdString());
        return result.generation;
    }

    std::uint64_t clearSampleForTesting() const
    {
        auto completion = std::make_shared<std::promise<Sampler::LoadResult>>();
        auto future = completion->get_future();
        processor->globalState.sampler.requestClear(
            [completion](Sampler::LoadResult result) { completion->set_value(std::move(result)); });
        if (future.wait_for(std::chrono::seconds(5)) != std::future_status::ready)
            throw std::runtime_error("Timed out waiting for the sampler worker");
        const auto result = future.get();
        if (!result.success)
            throw std::runtime_error("Sampler clear failed: " + result.error.toStdString());
        return result.generation;
    }

    py::dict getSampleTelemetry() const
    {
        const auto telemetry = processor->globalState.sampler.getTelemetry();
        py::dict result;
        result["requests"] = telemetry.requests;
        result["decoded"] = telemetry.decoded;
        result["published"] = telemetry.published;
        result["committed"] = telemetry.committed;
        result["coalesced"] = telemetry.coalesced;
        result["reclaimed"] = telemetry.reclaimed;
        result["destroyed_off_audio_thread"] = telemetry.destroyedOffAudioThread;
        result["reclaim_overflows"] = telemetry.reclaimOverflows;
        result["oversized_rejected"] = telemetry.oversizedRejected;
        result["worker_failures"] = telemetry.workerFailures;
        result["completion_failures"] = telemetry.completionFailures;
        result["active_generation"] = telemetry.activeGeneration;
        return result;
    }

    py::tuple getVoiceExciterBuses(int voiceIndex) const
    {
        if (voiceIndex < 0 || voiceIndex >= processor->getSynth()->getNumVoices())
            throw std::runtime_error("Voice index is outside the physical pool");
        const auto* voice = dynamic_cast<const ResonatorVoice*>(processor->getSynth()->getVoice(voiceIndex));
        if (voice == nullptr)
            throw std::runtime_error("Physical voice is not a ResonatorVoice");

        const auto samples = getBlockSize();
        auto excitation = py::array_t<float>({2, samples});
        auto direct = py::array_t<float>({2, samples});
        auto excitationView = excitation.mutable_unchecked<2>();
        auto directView = direct.mutable_unchecked<2>();
        for (int channel = 0; channel < 2; ++channel)
        {
            for (int sample = 0; sample < samples; ++sample)
            {
                excitationView(channel, sample) = voice->exciterBuffer.getSample(channel, sample);
                directView(channel, sample) = voice->tempBuffer.getSample(channel, sample);
            }
        }
        return py::make_tuple(std::move(excitation), std::move(direct));
    }

#endif

    PyParameterInfo getParamInfo(std::string& id)
    {
        gin::Parameter* param = processor->getParameter(id);
        if (param == nullptr)
        {
            throw std::runtime_error("Parameter not found: " + id);
        }

        PyParameterInfo info;
        info.id = param->getUid().toStdString();
        info.name = param->getName(50).toStdString();
        info.currentValue = param->getUserValue();
        info.minValue = param->getUserRangeStart();
        info.maxValue = param->getUserRangeEnd();
        info.defaultValue = param->getUserDefaultValue();

        return info;
    }

    std::vector<PyParameterInfo> getAllParams()
    {
        std::vector<PyParameterInfo> params;
        for (auto* param : processor->getPluginParameters())
        {
            PyParameterInfo info;
            info.id = param->getUid().toStdString();
            info.name = param->getName(50).toStdString();
            info.currentValue = param->getUserValue();
            info.minValue = param->getUserRangeStart();
            info.maxValue = param->getUserRangeEnd();
            info.defaultValue = param->getUserDefaultValue();
            params.push_back(info);
        }

        return params;
    }

    void setParam(std::string& id, float value)
    {
        gin::Parameter* param = processor->getParameter(id);
        if (param == nullptr)
        {
            throw std::runtime_error("Parameter not found: " + id);
        }

        if (value < param->getUserRangeStart() || value > param->getUserRangeEnd())
        {
            //throw an error whose message contains the start and end range
            throw std::runtime_error(
                "Parameter value " + std::to_string(value) + " is out of range; valid range is " +
                std::to_string(param->getUserRangeStart()) + " to " + std::to_string(param->getUserRangeEnd()));
        }

        param->setUserValueNotifingHost(value);
    }

    /**
     * Get all available modulation sources
     */
    std::vector<PyModSourceInfo> getAllModSources()
    {
        std::vector<PyModSourceInfo> sources;
        auto& modMatrix = processor->globalState.modMatrix;

        for (int i = 0; i < modMatrix.getNumModSources(); ++i)
        {
            gin::ModSrcId srcId(i);
            PyModSourceInfo info;
            info.id = modMatrix.getModSrcName(srcId).toStdString();
            info.name = info.id; // Name and ID are the same in this implementation
            info.isPoly = modMatrix.getModSrcPoly(srcId);
            info.isBipolar = modMatrix.getModSrcBipolar(srcId);
            sources.push_back(info);
        }

        return sources;
    }

    /**
     * Connect a modulation source to a destination parameter
     */
    void connectModulation(const std::string& sourceId, const std::string& destId, float depth,
                           const std::string& functionName = "linear", bool bipolarMapping = false)
    {
        auto& modMatrix = processor->globalState.modMatrix;
        gin::Parameter* destParam = processor->getParameter(destId);

        if (destParam == nullptr)
        {
            throw std::runtime_error("Destination parameter not found: " + destId);
        }

        // Find the source ID
        gin::ModSrcId srcId;
        bool sourceFound = false;

        for (int i = 0; i < modMatrix.getNumModSources(); ++i)
        {
            gin::ModSrcId testId(i);
            if (modMatrix.getModSrcName(testId).toStdString() == sourceId)
            {
                srcId = testId;
                sourceFound = true;
                break;
            }
        }

        if (!sourceFound)
        {
            throw std::runtime_error("Modulation source not found: " + sourceId);
        }

        // Get the destination ID
        gin::ModDstId dstId(destParam->getModIndex());
        if (dstId.id < 0)
        {
            throw std::runtime_error("Parameter is not modulation-capable: " + destId);
        }

        // Set modulation function if provided
        gin::ModMatrix::Function func = gin::ModMatrix::linear;
        if (!functionName.empty())
        {
            if (functionName == "linear") func = gin::ModMatrix::linear;
            else if (functionName == "quadraticIn") func = gin::ModMatrix::quadraticIn;
            else if (functionName == "quadraticInOut") func = gin::ModMatrix::quadraticInOut;
            else if (functionName == "quadraticOut") func = gin::ModMatrix::quadraticOut;
            else if (functionName == "sineIn") func = gin::ModMatrix::sineIn;
            else if (functionName == "sineInOut") func = gin::ModMatrix::sineInOut;
            else if (functionName == "sineOut") func = gin::ModMatrix::sineOut;
            else if (functionName == "exponentialIn") func = gin::ModMatrix::exponentialIn;
            else if (functionName == "exponentialInOut") func = gin::ModMatrix::exponentialInOut;
            else if (functionName == "exponentialOut") func = gin::ModMatrix::exponentialOut;
            else if (functionName == "invLinear") func = gin::ModMatrix::invLinear;
            else if (functionName == "invQuadraticIn") func = gin::ModMatrix::invQuadraticIn;
            else if (functionName == "invQuadraticInOut") func = gin::ModMatrix::invQuadraticInOut;
            else if (functionName == "invQuadraticOut") func = gin::ModMatrix::invQuadraticOut;
            else if (functionName == "invSineIn") func = gin::ModMatrix::invSineIn;
            else if (functionName == "invSineInOut") func = gin::ModMatrix::invSineInOut;
            else if (functionName == "invSineOut") func = gin::ModMatrix::invSineOut;
            else if (functionName == "invExponentialIn") func = gin::ModMatrix::invExponentialIn;
            else if (functionName == "invExponentialInOut") func = gin::ModMatrix::invExponentialInOut;
            else if (functionName == "invExponentialOut") func = gin::ModMatrix::invExponentialOut;
            else throw std::runtime_error("Unknown modulation function: " + functionName);
        }

        // Set the modulation depth
        modMatrix.setModDepth(srcId, dstId, depth);

        // Set the mapping mode and function
        modMatrix.setModBipolarMapping(srcId, dstId, bipolarMapping);
        modMatrix.setModFunction(srcId, dstId, func);
    }

    /**
     * Disconnect a modulation source from a destination parameter
     */
    void disconnectModulation(const std::string& sourceId, const std::string& destId)
    {
        auto& modMatrix = processor->globalState.modMatrix;
        gin::Parameter* destParam = processor->getParameter(destId);

        if (destParam == nullptr)
        {
            throw std::runtime_error("Destination parameter not found: " + destId);
        }

        // Find the source ID
        gin::ModSrcId srcId;
        bool sourceFound = false;

        for (int i = 0; i < modMatrix.getNumModSources(); ++i)
        {
            gin::ModSrcId testId(i);
            if (modMatrix.getModSrcName(testId).toStdString() == sourceId)
            {
                srcId = testId;
                sourceFound = true;
                break;
            }
        }

        if (!sourceFound)
        {
            throw std::runtime_error("Modulation source not found: " + sourceId);
        }

        // Get the destination ID
        gin::ModDstId dstId(destParam->getModIndex());
        if (dstId.id < 0)
        {
            throw std::runtime_error("Parameter is not modulation-capable: " + destId);
        }

        // Clear the modulation connection
        modMatrix.clearModDepth(srcId, dstId);
    }

    /**
     * Get all active modulation connections for a parameter
     */
    std::vector<std::tuple<std::string, float, std::string, bool>> getParameterModulations(const std::string& destId)
    {
        auto& modMatrix = processor->globalState.modMatrix;
        gin::Parameter* destParam = processor->getParameter(destId);

        if (destParam == nullptr)
        {
            throw std::runtime_error("Parameter not found: " + destId);
        }

        // Get the destination ID
        gin::ModDstId dstId(destParam->getModIndex());
        if (dstId.id < 0)
        {
            throw std::runtime_error("Parameter is not modulation-capable: " + destId);
        }

        std::vector<std::tuple<std::string, float, std::string, bool>> connections;
        auto depths = modMatrix.getModDepths(dstId);

        for (auto& [srcId, depth] : depths)
        {
            std::string sourceName = modMatrix.getModSrcName(srcId).toStdString();
            gin::ModMatrix::Function func = modMatrix.getModFunction(srcId, dstId);
            bool bipolar = modMatrix.getModBipolarMapping(srcId, dstId);

            // Convert function to string
            std::string funcName = "linear";
            if (func == gin::ModMatrix::quadraticIn) funcName = "quadraticIn";
            else if (func == gin::ModMatrix::quadraticInOut) funcName = "quadraticInOut";
            else if (func == gin::ModMatrix::quadraticOut) funcName = "quadraticOut";
            else if (func == gin::ModMatrix::sineIn) funcName = "sineIn";
            else if (func == gin::ModMatrix::sineInOut) funcName = "sineInOut";
            else if (func == gin::ModMatrix::sineOut) funcName = "sineOut";
            else if (func == gin::ModMatrix::exponentialIn) funcName = "exponentialIn";
            else if (func == gin::ModMatrix::exponentialInOut) funcName = "exponentialInOut";
            else if (func == gin::ModMatrix::exponentialOut) funcName = "exponentialOut";
            else if (func == gin::ModMatrix::invLinear) funcName = "invLinear";
            else if (func == gin::ModMatrix::invQuadraticIn) funcName = "invQuadraticIn";
            else if (func == gin::ModMatrix::invQuadraticInOut) funcName = "invQuadraticInOut";
            else if (func == gin::ModMatrix::invQuadraticOut) funcName = "invQuadraticOut";
            else if (func == gin::ModMatrix::invSineIn) funcName = "invSineIn";
            else if (func == gin::ModMatrix::invSineInOut) funcName = "invSineInOut";
            else if (func == gin::ModMatrix::invSineOut) funcName = "invSineOut";
            else if (func == gin::ModMatrix::invExponentialIn) funcName = "invExponentialIn";
            else if (func == gin::ModMatrix::invExponentialInOut) funcName = "invExponentialInOut";
            else if (func == gin::ModMatrix::invExponentialOut) funcName = "invExponentialOut";

            connections.push_back(std::make_tuple(sourceName, depth, funcName, bipolar));
        }

        return connections;
    }

    /**
     * Allocate numBlocks worth of memory for a stereo audio buffer.
     */
    py::array_t<float> createMultiBlock(int numBlocks)
    {
        const int blockSize = getBlockSize();
        const int numChannels = 2; // Stereo

        auto result = py::array_t<float>({numChannels, numBlocks * blockSize});

        // Zero the buffer
        py::buffer_info buf = result.request();
        float* ptr = static_cast<float*>(buf.ptr);
        memset(ptr, 0, numChannels * numBlocks * blockSize * sizeof(float));

        return result;
    }

    // Audio Processing
    void processMultiBlock(py::array_t<float>& buffer, int startBlock = 0, int numBlocks = -1)
    {
        py::buffer_info buf = buffer.request();
        const int blockSize = getBlockSize();

        // Error checking
        if (buf.ndim != 2)
        {
            throw std::runtime_error("Buffer must be 2-dimensional");
        }

        if (buf.shape[0] != 2)
        {
            throw std::runtime_error("Buffer must have 2 channels");
        }

        if (buf.shape[1] % blockSize != 0)
        {
            throw std::runtime_error("Buffer length must be multiple of block size");
        }

        int maxBlocks = buf.shape[1] / blockSize;
        if (startBlock >= maxBlocks)
        {
            throw std::runtime_error("Start block beyond buffer end");
        }

        int blocksToProcess = (numBlocks < 0) ? (maxBlocks - startBlock) : numBlocks;
        if (startBlock + blocksToProcess > maxBlocks)
        {
            throw std::runtime_error("Requested blocks exceed buffer size");
        }

        // Get pointers to the buffer
        float* outL = static_cast<float*>(buf.ptr) + startBlock * blockSize;
        float* outR = outL + buf.shape[1];

        // Process each block
        for (int i = 0; i < blocksToProcess; ++i)
        {
            // Clear audio buffer but keep MIDI events
            tempBuffer.clear();

            // Process audio with accumulated MIDI events
            processor->processBlock(tempBuffer, midiBuffer);

            // Copy to output buffer
            memcpy(outL, tempBuffer.getReadPointer(0), blockSize * sizeof(float));
            memcpy(outR, tempBuffer.getReadPointer(1), blockSize * sizeof(float));

            outL += blockSize;
            outR += blockSize;
        }

        // Clear MIDI buffer after processing all blocks
        midiBuffer.clear();
    }

    /**
     * Process exactly one host block and report only the C++ callback duration.
     *
     * This intentionally excludes Python dispatch, NumPy buffer inspection and
     * output copying.  It is a test-adapter entry point for the deterministic
     * real-time regression gate; normal render clients should continue to use
     * processMultiBlock().
     */
    std::uint64_t benchmarkProcessBlockNs()
    {
        tempBuffer.clear();

        const auto started = std::chrono::steady_clock::now();
        processor->processBlock(tempBuffer, midiBuffer);
        const auto finished = std::chrono::steady_clock::now();

        return static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(finished - started).count());
    }

    py::array_t<float> processInputBlock(
        py::array_t<float, py::array::c_style | py::array::forcecast> input)
    {
        const auto view = input.request();
        if (view.ndim != 2 || view.shape[0] != 2 || view.shape[1] != getBlockSize())
            throw std::runtime_error("Input block must have shape (2, block_size)");
        const auto* source = static_cast<const float*>(view.ptr);
        processInjectedInputBlock(source, source + getBlockSize(), false);

        py::array_t<float> output({2, getBlockSize()});
        auto outputView = output.mutable_unchecked<2>();
        for (int channel = 0; channel < 2; ++channel)
            for (int sample = 0; sample < getBlockSize(); ++sample)
                outputView(channel, sample) = tempBuffer.getSample(channel, sample);
        return output;
    }

    std::uint64_t benchmarkInputBlockNs(
        py::array_t<float, py::array::c_style | py::array::forcecast> input)
    {
        const auto view = input.request();
        if (view.ndim != 2 || view.shape[0] != 2 || view.shape[1] != getBlockSize())
            throw std::runtime_error("Input block must have shape (2, block_size)");
        const auto* source = static_cast<const float*>(view.ptr);
        return processInjectedInputBlock(source, source + getBlockSize(), true);
    }

};


static void bindResonariumModule(py::module_& m)
{
    m.doc() = "Resonarium synthesizer Python bindings";

    py::class_<PyParameterInfo>(m, "ParameterInfo")
        .def_readonly("id", &PyParameterInfo::id)
        .def_readonly("name", &PyParameterInfo::name)
        .def_readonly("value", &PyParameterInfo::currentValue)
        .def_readonly("default_value", &PyParameterInfo::defaultValue)
        .def_readonly("default", &PyParameterInfo::defaultValue)
        .def_readonly("min", &PyParameterInfo::minValue)
        .def_readonly("max", &PyParameterInfo::maxValue)
        .def("__repr__", &PyParameterInfo::toString);

    py::class_<PyModSourceInfo>(m, "ModSourceInfo")
        .def_readonly("id", &PyModSourceInfo::id)
        .def_readonly("name", &PyModSourceInfo::name)
        .def_readonly("is_poly", &PyModSourceInfo::isPoly)
        .def_readonly("is_bipolar", &PyModSourceInfo::isBipolar)
        .def("__repr__", &PyModSourceInfo::toString);

    py::class_<ResonariumWrapper>(m, "Resonarium")
        .def(py::init<double, int>(),
             py::arg("sampleRate") = 44100.0,
             py::arg("blockSize") = 512)
        .def("get_sample_rate", &ResonariumWrapper::getSampleRate)
        .def("sample_rate", &ResonariumWrapper::getSampleRate)
        .def("get_block_size", &ResonariumWrapper::getBlockSize)
        .def("get_all_params", &ResonariumWrapper::getAllParams)
        .def("reset", &ResonariumWrapper::reset)
        .def("get_state_xml", &ResonariumWrapper::getStateXml)
        .def("get_program_names", &ResonariumWrapper::getProgramNames)
        .def("set_current_program", &ResonariumWrapper::setCurrentProgram, py::arg("name"))
        .def("get_voice_telemetry", &ResonariumWrapper::getVoiceTelemetry,
             "Return the last audio-block voice-manager telemetry snapshot")
#if defined (RESONARIUM_TEST_INSTRUMENTATION)
        .def("set_play_mode_for_testing", &ResonariumWrapper::setPlayModeForTesting,
             py::arg("mode"),
             "Select a setup-only Gin play mode for assignment-continuity tests")
        .def("get_global_effect_test_call_counters", &ResonariumWrapper::getGlobalEffectTestCallCounters,
             "Return test-only global effect-chain call totals")
        .def("clear_global_effect_test_call_counters", &ResonariumWrapper::clearGlobalEffectTestCallCounters,
             "Clear test-only global effect-chain call totals")
        .def("get_effect_chain_topology", &ResonariumWrapper::getEffectChainTopology,
             "Return test-only global/per-voice effect-chain instance counts")
        .def("get_external_input_telemetry", &ResonariumWrapper::getExternalInputTelemetry,
             "Return test-only once-per-host-block external-input capture counters")
        .def("clear_external_input_telemetry", &ResonariumWrapper::clearExternalInputTelemetry,
             "Clear test-only external-input capture counters")
        .def("get_exciter_telemetry", &ResonariumWrapper::getExciterTelemetry,
             "Return aggregate and per-voice concrete-rack counters")
        .def("clear_exciter_telemetry", &ResonariumWrapper::clearExciterTelemetry,
             "Clear every physical voice's concrete-rack counters")
        .def("render_exciter_prng_for_testing", &ResonariumWrapper::renderExciterPrngForTesting,
             py::arg("voice_id"), py::arg("generation"), py::arg("kind"),
             py::arg("substream"), py::arg("num_samples"),
             "Render the specified deterministic per-assignment xoshiro stream")
        .def("install_sample_for_testing", &ResonariumWrapper::installSampleForTesting,
             py::arg("audio"), py::arg("sample_rate"),
             "Publish an immutable in-memory sample through the sampler worker")
        .def("clear_sample_for_testing", &ResonariumWrapper::clearSampleForTesting,
             "Publish an empty immutable sampler state through the worker")
        .def("get_sample_telemetry", &ResonariumWrapper::getSampleTelemetry,
             "Return test-only immutable-snapshot transport and reclamation counters")
        .def("get_voice_exciter_buses", &ResonariumWrapper::getVoiceExciterBuses,
             py::arg("voice_index"),
             "Copy the last private excitation/direct buses of one physical voice")
#endif
        .def("get_param", &ResonariumWrapper::getParamInfo,
             py::arg("uid"))
        .def("set_param", &ResonariumWrapper::setParam,
             py::arg("uid"), py::arg("value"))
        // Modulation methods
        .def("get_all_mod_sources", &ResonariumWrapper::getAllModSources)
        .def("connect_modulation", &ResonariumWrapper::connectModulation,
             py::arg("source_id"), py::arg("dest_id"), py::arg("depth"),
             py::arg("function_name") = "linear", py::arg("bipolar_mapping") = false)
        .def("disconnect_modulation", &ResonariumWrapper::disconnectModulation,
             py::arg("source_id"), py::arg("dest_id"))
        .def("get_parameter_modulations", &ResonariumWrapper::getParameterModulations,
             py::arg("dest_id"))
        // Audio processing methods
        .def("create_multi_block", &ResonariumWrapper::createMultiBlock)
        .def("process_multi_block", &ResonariumWrapper::processMultiBlock,
             py::arg("buffer"),
             py::arg("startBlock") = 0,
             py::arg("numBlocks") = -1)
        .def("benchmark_process_block_ns", &ResonariumWrapper::benchmarkProcessBlockNs,
             "Process one block and return the C++ processBlock duration in nanoseconds",
             py::call_guard<py::gil_scoped_release>())
        .def("process_input_block", &ResonariumWrapper::processInputBlock,
             py::arg("input"), "Process one asymmetric stereo host-input block")
        .def("benchmark_input_block_ns", &ResonariumWrapper::benchmarkInputBlockNs,
             py::arg("input"), "Process one host-input block and return C++ callback nanoseconds")
        .def("play_note", &ResonariumWrapper::playNote,
             py::arg("channel"),
             py::arg("note"),
             py::arg("velocity") = 64,
             py::arg("sample_offset") = 0)
        .def("release_note", &ResonariumWrapper::releaseNote,
             py::arg("channel"), py::arg("note"), py::arg("velocity") = 0,
             py::arg("sample_offset") = 0)
        .def("all_notes_off", &ResonariumWrapper::allNotesOff,
             py::arg("sample_offset") = 0)
        .def("queue_midi", &ResonariumWrapper::queueMidi,
             py::arg("data"), py::arg("sample_offset") = 0,
             "Queue one raw MIDI 1.0 channel message in the next audio block");

    m.def("get_version", []() { return "0.1.0"; });
}

#if defined (RESONARIUM_PYTHON_TEST_MODULE)
PYBIND11_MODULE(resonarium_test, m)
{
    bindResonariumModule(m);
}
#else
PYBIND11_MODULE(resonarium, m)
{
    bindResonariumModule(m);
}
#endif
