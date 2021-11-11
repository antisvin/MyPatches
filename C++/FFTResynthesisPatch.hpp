#include "OpenWareLibrary.h"
#include "FFTProcessor.h"

class ComplexBypass : public ComplexSignalProcessor {
public:
    void process(ComplexFloatArray input, ComplexFloatArray output) {
        output.copyFrom(input);
    }
    ComplexFloat process(ComplexFloat input) {
        return ComplexFloat();
    }
    static ComplexBypass* create() {
        return new ComplexBypass();
    }
    static void destroy(ComplexBypass* bypass) {
        delete bypass;
    }
};

using FFTResynthesisProcessor = FFTProcessor<ComplexBypass, 2048, 256>;

class FFTResynthesisPatch : public Patch {
public:
    FFTResynthesisProcessor* processor;
    FFTResynthesisPatch() {
        processor = FFTResynthesisProcessor::create(
            getBlockSize(), Window::HannWindow, Window::HannWindow);
    }
    ~FFTResynthesisPatch() {
        FFTResynthesisProcessor::destroy(processor);
    }
    void processAudio(AudioBuffer& buffer) {
        buffer.getSamples(1).copyFrom(buffer.getSamples(0));
        processor->process(buffer.getSamples(0), buffer.getSamples(0));
    }
};
