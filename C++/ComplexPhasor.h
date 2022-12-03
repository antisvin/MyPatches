#ifndef __COMPLEX_PHASOR_H__
#define __COMPLEX_PHASOR_H__

#include "SignalGenerator.h"
#include "Oscillator.h"
#include "SineOscillator.h"

/**
 * A complex phasor is a ComplexSignalGenerator with 2 channels that
 * operates at a given frequency and that can be frequency modulated.
 *
 * Phase is represented as a ComplexFloat value, whose real and imaginary
 * parts can be used as cosine/sine pair at a given frequency. Phasor uses
 * complex product to calclulate its rotation, which is cheaper than
 * using trigonometric functions.
 * 
 * Phasor output is expected to match ComplexSineOscillator class.
 */
class ComplexPhasor : public ComplexSignalGenerator {
public:
    ComplexPhasor() {
        reset();
    }
    virtual ~ComplexPhasor() = default;
    using ComplexSignalGenerator::generate;
    /**
     * Set oscillator sample rate
     */
    virtual void setSampleRate(float sr) {
        float freq = getFrequency();
        mul = M_PI * 2 / sr;
        setFrequency(freq);
    }
    virtual float getSampleRate() {
        return M_PI * 2 / mul;
    }
    /**
     * Set phasor frequency in Hertz
     */
    virtual void setFrequency(float freq) {
        incr.setPolar(1.f, 0.5f * freq * mul);
        k1 = incr.im / incr.re;
        k2 = 2.f * k1 / (1 + k1 * k1);
    }
    /**
     * Get phasor frequency in Hertz
     */
    virtual float getFrequency() {
        return incr.getPhase() * 2 / mul;
    }
    /**
     * Set current phasor phase in radians
     * @param phase a value between 0 and 2*pi
     */
    virtual void setPhase(ComplexFloat phase) {
        phasor = phase;
    }
    /**
     * Get current phase in radians
     * @return a value between 0 and 2*pi
     */
    virtual ComplexFloat getPhase() {
        return phasor.getPhase();
    }
    /**
     * Reset phasor (typically resets phase)
     */
    virtual void reset() {
        phasor = {1.f, 0.f};
        ph = 0.f;
    }
    /**
     * Produce a sample with frequency modulation.
     */
    virtual ComplexFloat generate(float fm) {
        // Note: FM must be tested more thoroughly, not fully described in original paper.
        float t = phasor.re - k1 * phasor.im * (1.f + fm);
        phasor.im += k2 * t * (1.f + fm);
        phasor.re = t - k1 * phasor.im * (1.f + fm);
        return phasor;
    }
    virtual ComplexFloat generate() {
        /**
         * See "A New Recursive Quadrature Oscillator" by Martin Vicanek for formula derivation
         **/
        float t = phasor.re - k1 * phasor.im;
        phasor.im += k2 * t;
        phasor.re = t - k1 * phasor.im;
        return phasor;
    }
    void generate(AudioBuffer& buffer) {
        float* left = buffer.getSamples(0).getData();
        float* right = buffer.getSamples(1).getData();
        for (int i = 0; i < buffer.getSize(); i++) {
            ComplexFloat sample = generate();
            *left++ = sample.re;    
            *right++ = sample.im;    
        }
    }
    virtual void generate(ComplexFloatArray output) override {
        for (size_t i = 0; i < output.getSize(); ++i) {
            output[i] = generate();
        }
    }

    virtual void generate(ComplexFloatArray output, FloatArray fm) {
        for (size_t i = 0; i < output.getSize(); ++i) {
            output[i] = generate(fm[i]);
        }
    }

    static ComplexPhasor* create(float sr) {
        auto* osc = new ComplexPhasor();
        osc->reset();
        osc->setSampleRate(sr);
        osc->setFrequency(110.f);
        return osc;
    }

    static void destroy(ComplexPhasor* osc) {
        delete osc;
    }

protected:
    ComplexFloat phasor {};
    ComplexFloat incr {};
    float mul = 0.f;
    float ph = 0.f;
    float k1 = 0.f;
    float k2 = 0.f;
};
#endif
