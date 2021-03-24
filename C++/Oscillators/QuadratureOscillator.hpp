#ifndef __QUADRATURE_OSCILLATOR_HPP__
#define __QUADRATURE_OSCILLATOR_HPP__

#include "ComplexFloatArray.h"
#include "SignalGenerator.h"

/**
 * A QuadratureOscillator is a MultiSignalGenerator that operates at a given frequency
 * and that can be frequency modulated. It output 2 channels of audio with 90 degree
 * phase difference.
 */
class QuadratureOscillator : public MultiSignalGenerator {
public:
    QuadratureOscillator(float sr=48000) {
    }
    virtual ~QuadratureOscillator() {
    }
    using MultiSignalGenerator::generate;
    /**
     * Set oscillator sample rate
     */
    virtual void setSampleRate(float value) {
    }
    /**
     * Get oscillator sample rate
     */
    virtual float getSampleRate() {
        return 0.0f;
    }
    /**
     * Set oscillator frequency in Hertz
     */
    virtual void setFrequency(float value) {
    }
    /**
     * Get oscillator frequency in Hertz
     */
    virtual float getFrequency() {
        return 0.0f;
    }
    /**
     * Set current oscillator phase in radians
     * @param phase a value between 0 and 2*pi
     */
    virtual void setPhase(float phase) = 0;
    /**
     * Get current oscillator phase in radians
     * @return a value between 0 and 2*pi
     */
    virtual float getPhase() = 0;
    /**
     * Reset oscillator (typically resets phase)
     */
    virtual void reset() {
    }
    /**
     * Produce a sample with frequency modulation.
     */
    virtual ComplexFloat generate(float fm) = 0;
    /**
     * Produce a block of samples with frequency modulation.
     */
    virtual void generate(AudioBuffer& output, FloatArray fm) {
        size_t len = output.getSize();
        float* re = output.getSamples(0);
        float* im = re + len;
        float* mod = (float*)fm;
        for (size_t i = 0; i < len; ++i) {
            ComplexFloat sample = generate(*mod++);
            *re++ = sample.re;
            *im++ = sample.im;
        }
    }
};

#endif
