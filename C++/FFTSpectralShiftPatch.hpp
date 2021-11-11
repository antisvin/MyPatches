#include "OpenWareLibrary.h"
#include "FFTProcessor.hpp"

#define FFT_SIZE 512

class ComplexShift : public ComplexSignalProcessor {
public:
    ComplexShift()
        : shift(0) {
    }
    void process(ComplexFloatArray input, ComplexFloatArray output) {
        output.copyFrom(input);
        if (shift > 0) {
            output.move(0, shift, input.getSize() - shift);
            output.subArray(0, shift).clear();
        }
        else if (shift < 0) {
            output.move(-shift, 0, input.getSize() + shift);
            output.subArray(input.getSize() + shift, -shift).clear();
        }
    }
    void setShift(float shift) {
        this->shift = shift;
    }

    ComplexFloat process(ComplexFloat input) {
        return ComplexFloat();
    }
    static ComplexShift* create(size_t fft_size) {
        return new ComplexShift();
    }
    static void destroy(ComplexShift* processor) {
        delete processor;
    }

private:
    float shift;
};

using FFTSpectralShiftProcessor = FFTProcessor<ComplexShift, FFT_SIZE, 256>;

class FFTSpectralShiftPatch : public Patch {
public:
    FFTSpectralShiftProcessor* processor;
    FFTSpectralShiftPatch() {
        registerParameter(PARAMETER_A, "Shift");
        setParameterValue(PARAMETER_A, 0.5);
        processor = FFTSpectralShiftProcessor::create(
            getBlockSize(), Window::HannWindow, Window::HannWindow, FFT_SIZE);
    }
    ~FFTSpectralShiftPatch() {
        FFTSpectralShiftProcessor::destroy(processor);
    }
    void processAudio(AudioBuffer& buffer) {
        auto bins_processor = processor->getFrequencyDomainProcessor();
        bins_processor->setShift(
            (getParameterValue(PARAMETER_A) * 2 - 1) * FFT_SIZE / 8);
        processor->process(buffer.getSamples(0), buffer.getSamples(0));
    }
};
