#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>
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

class ResonariumVoiceWrapper
{
private:
    ResonatorVoice* wrappedVoice; // Raw pointer - ownership remains with the synth
    juce::AudioBuffer<float> tempBuffer; // Buffer for processing
    double sampleRate;
    int blockSize;

public:
    ResonariumVoiceWrapper(ResonatorVoice* voice, double sampleRate = 44100.0, int blockSize = 512)
        : wrappedVoice(voice), tempBuffer(2, blockSize), sampleRate(sampleRate), blockSize(blockSize)
    {
        tempBuffer.clear();
    }

    ~ResonariumVoiceWrapper()
    {
        // Voice is managed by the synth, so no need to delete it here
    }

    void reset()
    {
        if (wrappedVoice)
        {
            // Reset internal state
            juce::dsp::ProcessSpec spec;
            spec.sampleRate = sampleRate;
            spec.maximumBlockSize = blockSize;
            spec.numChannels = 2;
            wrappedVoice->noteStopped(false);
            wrappedVoice->prepare(spec);
        }
    }

    void playNote(int noteNumber, int velocity)
    {
        if (wrappedVoice)
        {
            // Create a MIDI note using proper factory methods
            juce::MPENote note(
                1, // channel
                noteNumber,
                juce::MPEValue::fromUnsignedFloat(velocity / 127.0f), // velocity
                juce::MPEValue::fromUnsignedFloat(0.0f), // pressure
                juce::MPEValue::fromUnsignedFloat(0.0f), // timbre
                juce::MPEValue::fromUnsignedFloat(0.5f), // pitchbend (centered)
                juce::MPENote::keyDown // noteOnState
            );

            // Start the note
            wrappedVoice->currentMidiNote = noteNumber;
            wrappedVoice->noteReleased = false;
            wrappedVoice->silenceCount = 0;
            wrappedVoice->numBlocksSinceNoteOn = 0;

            // Set the currently playing note and start it
            wrappedVoice->setCurrentlyPlayingNote(note);
            wrappedVoice->noteStarted();
        }
    }

    void releaseNote()
    {
        if (wrappedVoice)
        {
            wrappedVoice->noteStopped(true); // Allow tail-off
        }
    }

    bool isActive() const
    {
        return wrappedVoice && wrappedVoice->isVoiceActive();
    }

    py::array_t<float> processBlock(int numSamples = -1)
    {
        // Default to using blockSize if numSamples is not specified
        if (numSamples <= 0)
            numSamples = blockSize;

        // Create output buffer for Python
        auto result = py::array_t<float>({2, numSamples});
        py::buffer_info buf = result.request();
        float* ptr = static_cast<float*>(buf.ptr);

        // Clear the result buffer
        std::memset(ptr, 0, 2 * numSamples * sizeof(float));

        if (wrappedVoice && wrappedVoice->isVoiceActive())
        {
            // Make sure tempBuffer is large enough
            if (tempBuffer.getNumSamples() < numSamples)
            {
                tempBuffer.setSize(2, numSamples, false, true);
            }

            // Clear the buffer
            tempBuffer.clear();

            // Process the audio
            wrappedVoice->renderNextBlock(tempBuffer, 0, numSamples);

            // Copy to the output buffer
            float* outL = ptr;
            float* outR = ptr + numSamples;

            std::memcpy(outL, tempBuffer.getReadPointer(0), numSamples * sizeof(float));
            std::memcpy(outR, tempBuffer.getReadPointer(1), numSamples * sizeof(float));
        }

        return result;
    }

    /**
     * Process multiple blocks at once, similar to the processor's processMultiBlock.
     * Fills the provided buffer with audio from the voice.
     */
    void processMultiBlock(py::array_t<float>& buffer, int startBlock = 0, int numBlocks = -1)
    {
        py::buffer_info buf = buffer.request();

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
            throw std::runtime_error(
                "Buffer length (" + std::to_string(buf.shape[1]) + ") must be multiple of block size (" +
                std::to_string(blockSize) + ")");
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

        // Nothing to do if the voice isn't active
        if (!wrappedVoice || !wrappedVoice->isVoiceActive())
            return;

        // Get pointers to the buffer
        float* outL = static_cast<float*>(buf.ptr) + startBlock * blockSize;
        float* outR = outL + buf.shape[1];

        // Make sure tempBuffer is large enough
        if (tempBuffer.getNumSamples() < blockSize)
        {
            tempBuffer.setSize(2, blockSize, false, true);
        }

        // Process each block
        for (int i = 0; i < blocksToProcess; ++i)
        {
            // Clear audio buffer
            tempBuffer.clear();

            // Process audio with the voice
            wrappedVoice->renderNextBlock(tempBuffer, 0, blockSize);

            // Copy to output buffer
            std::memcpy(outL, tempBuffer.getReadPointer(0), blockSize * sizeof(float));
            std::memcpy(outR, tempBuffer.getReadPointer(1), blockSize * sizeof(float));

            outL += blockSize;
            outR += blockSize;
        }
    }
};

class ResonariumWrapper
{
private:
    std::unique_ptr<ResonariumProcessor> processor;
    juce::AudioBuffer<float> tempBuffer; //holds a single block of memory
    juce::MidiBuffer midiBuffer;

public:
    ResonariumWrapper(double sampleRate = 44100.0, int blockSize = 512)
        : tempBuffer(2, blockSize)
    {
        processor = std::make_unique<ResonariumProcessor>();
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
    void playNote(int channel, int note, int velocity)
    {
        // Queue note-on message
        auto msg = juce::MidiMessage::noteOn(channel + 1, note, static_cast<uint8_t>(velocity));
        midiBuffer.addEvent(msg, 0);
    }

    void releaseNote(int channel, int note, int velocity)
    {
        auto msg = juce::MidiMessage::noteOff(channel + 1, note, static_cast<uint8_t>(velocity));
        midiBuffer.addEvent(msg, 0);
    }

    void allNotesOff()
    {
        for (int channel = 1; channel <= 16; ++channel)
        {
            auto msg = juce::MidiMessage::allNotesOff(channel);
            midiBuffer.addEvent(msg, 0);
        }
    }

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

    // Acquire a voice wrapper
    std::shared_ptr<ResonariumVoiceWrapper> getVoice(int index)
    {
        // Access the ResonariumProcessor's synth
        auto* synth = processor->getSynth();
        if (!synth)
        {
            throw std::runtime_error("Failed to get synthesizer");
        }

        // Find a free voice or steal the oldest one
        auto* voicePtr = dynamic_cast<ResonatorVoice*>(synth->getVoice(index));
        if (!voicePtr)
        {
            throw std::runtime_error("Failed to acquire a voice");
        }

        // Create and return a voice wrapper (we return a shared_ptr to the wrapper, but the voice itself is a raw pointer)
        return std::make_shared<
            ResonariumVoiceWrapper>(voicePtr, processor->getSampleRate(), processor->getBlockSize());
    }
};


PYBIND11_MODULE(resonarium, m)
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

    // Register ResonatorVoiceWrapper class
    py::class_<ResonariumVoiceWrapper, std::shared_ptr<ResonariumVoiceWrapper>>(m, "ResonatorVoice")
        .def("reset", &ResonariumVoiceWrapper::reset)
        .def("play_note", &ResonariumVoiceWrapper::playNote,
             py::arg("note"),
             py::arg("velocity") = 64)
        .def("release_note", &ResonariumVoiceWrapper::releaseNote)
        .def("is_active", &ResonariumVoiceWrapper::isActive)
        .def("process_block", &ResonariumVoiceWrapper::processBlock,
             py::arg("num_samples") = -1)
        .def("process_multi_block", &ResonariumVoiceWrapper::processMultiBlock,
             py::arg("buffer"),
             py::arg("startBlock") = 0,
             py::arg("numBlocks") = -1);

    py::class_<ResonariumWrapper>(m, "Resonarium")
        .def(py::init<double, int>(),
             py::arg("sampleRate") = 44100.0,
             py::arg("blockSize") = 512)
        .def("get_sample_rate", &ResonariumWrapper::getSampleRate)
        .def("sample_rate", &ResonariumWrapper::getSampleRate)
        .def("get_block_size", &ResonariumWrapper::getBlockSize)
        .def("get_all_params", &ResonariumWrapper::getAllParams)
        .def("reset", &ResonariumWrapper::reset)
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
        .def("play_note", &ResonariumWrapper::playNote,
             py::arg("channel"),
             py::arg("note"),
             py::arg("velocity") = 64)
        .def("release_note", &ResonariumWrapper::releaseNote,
             py::arg("channel"), py::arg("note"), py::arg("velocity") = 0)
        .def("all_notes_off", &ResonariumWrapper::allNotesOff)
        // Voice management
        .def("get_voice", &ResonariumWrapper::getVoice, py::arg("index") = 0);

    m.def("get_version", []() { return "0.1.0"; });
}
