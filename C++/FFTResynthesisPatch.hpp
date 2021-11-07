#include "OpenWareLibrary.h"
#include "FFTProcessor.hpp"

class ComplexBypass : public ComplexSignalProcessor {
public:
    void process(ComplexFloatArray input, ComplexFloatArray output) {
        // Yup, do nothing at all!
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

using FFTResynthesisProcessor = FFTProcessor<ComplexBypass, 2048>;

class FFTResynthesisPatch : public Patch {
public:
    FFTResynthesisProcessor* processor;
    FFTResynthesisPatch() {
        processor = FFTResynthesisProcessor::create(getBlockSize());
    }
    ~FFTResynthesisPatch() {
        FFTResynthesisProcessor::destroy(processor);
    }
    void processAudio(AudioBuffer& buffer) {
        processor->process(buffer.getSamples(0), buffer.getSamples(0));
    }
};
