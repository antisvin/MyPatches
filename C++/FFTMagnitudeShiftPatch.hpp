#include "OpenWareLibrary.h"
#include "FFTProcessor.hpp"

#define FFT_SIZE 1024

class ComplexShift : public ComplexSignalProcessor {
public:
    ComplexShift() = default;
    ComplexShift(FloatArray magnitudes)
        : magnitudes(magnitudes)
        , shift(0) {
    }
    void process(ComplexFloatArray input, ComplexFloatArray output) {
        output.copyFrom(input);
        input.getMagnitudeValues(magnitudes);
        magnitudes.move(0, shift, magnitudes.getSize() - shift);
        magnitudes.subArray(0, shift).clear();
        output.setMagnitude(magnitudes);
    }
    void setShift(float shift) {
        this->shift = shift;
    }

    ComplexFloat process(ComplexFloat input) {
        return ComplexFloat();
    }
    static ComplexShift* create(size_t fft_size) {
        return new ComplexShift(FloatArray::create(fft_size / 2));
    }
    static void destroy(ComplexShift* processor) {
        FloatArray::destroy(processor->magnitudes);
        delete processor;
    }

private:
    float shift;
    FloatArray magnitudes;
};

using FFTMagnitudeShiftProcessor = FFTProcessor<ComplexShift, FFT_SIZE>;

class FFTMagnitudeShiftPatch : public Patch {
public:
    FFTMagnitudeShiftProcessor* processor;
    FFTMagnitudeShiftPatch() {
        processor = FFTMagnitudeShiftProcessor::create(getBlockSize(),
            Window::HammingWindow, Window::TriangularWindow, FFT_SIZE);
    }
    ~FFTMagnitudeShiftPatch() {
        FFTMagnitudeShiftProcessor::destroy(processor);
    }
    void processAudio(AudioBuffer& buffer) {
        auto bins_processor = processor->getBinsProcessor();
        bins_processor->setShift(getParameterValue(PARAMETER_A) * FFT_SIZE / 8);
        processor->process(buffer.getSamples(0), buffer.getSamples(0));
    }
};
