#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>
#include "PluginProcessor.h"

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

    // Optional: Add string representation for Python
    std::string toString() const
    {
        return name + " [" + std::to_string(currentValue) + "]";
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
        info.defaultValue = param->getDefaultValue();

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
            info.defaultValue = param->getDefaultValue();
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
            throw std::runtime_error("Parameter value " + std::to_string(value) + " is out of range; valid range is " + std::to_string(param->getUserRangeStart()) + " to " + std::to_string(param->getUserRangeEnd()));
        }

        param->setUserValueNotifingHost(value);
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
};

PYBIND11_MODULE(resonarium, m)
{
    m.doc() = "Resonarium synthesizer Python bindings";

    py::class_<PyParameterInfo>(m, "ParameterInfo")
        .def_readonly("id", &PyParameterInfo::id)
        .def_readonly("name", &PyParameterInfo::name)
        .def_readonly("value", &PyParameterInfo::currentValue)
        .def_readonly("default_value", &PyParameterInfo::defaultValue)
        .def_readonly("min_value", &PyParameterInfo::minValue)
        .def_readonly("max_value", &PyParameterInfo::maxValue)
        .def("__repr__", &PyParameterInfo::toString);

    py::class_<ResonariumWrapper>(m, "Resonarium")
        .def(py::init<double, int>(),
             py::arg("sampleRate") = 44100.0,
             py::arg("blockSize") = 512)
        .def("get_sample_rate", &ResonariumWrapper::getSampleRate)
        .def("get_block_size", &ResonariumWrapper::getBlockSize)
        .def("get_all_params", &ResonariumWrapper::getAllParams)
        .def("reset", &ResonariumWrapper::reset)
        .def("get_param", &ResonariumWrapper::getParamInfo,
            py::arg("uid"))
        .def("set_param", &ResonariumWrapper::setParam,
            py::arg("uid"), py::arg("value"))
        .def("create_multi_block", &ResonariumWrapper::createMultiBlock)
        .def("process_multi_block", &ResonariumWrapper::processMultiBlock,
             py::arg("buffer"),
             py::arg("startBlock") = 0,
             py::arg("numBlocks") = -1)
        .def("play_note", &ResonariumWrapper::playNote,
             py::arg("channel"),
             py::arg("note"),
             py::arg("velocity") = 64)
        .def("release_note", &ResonariumWrapper::releaseNote)
        .def("all_notes_off", &ResonariumWrapper::allNotesOff);

    m.def("get_version", []() { return "0.1.0"; });
}
