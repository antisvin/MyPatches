#ifndef __COMPLEX_DOMAIN_OSCILLATOR_H__
#define __COMPLEX_DOMAIN_OSCILLATOR_H__

#include "SignalGenerator.h"
#include "Oscillator.h"
#include "SineOscillator.h"

/**
 * A complex oscillator is a MultiSignalGenerator with 2 channels that
 * operates at a given frequency and that can be frequency modulated.
 *
 * A single sample is represented as a ComplexFloat value, while blocks
 * of audio are stored in an AudioBuffer with 2 channels.
 */
class ComplexDomainOscillator : public ComplexSignalGenerator {
public:
    ComplexDomainOscillator() {
        reset();
    }
    virtual ~ComplexDomainOscillator() = default;
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
     * Set oscillator frequency in Hertz
     */
    virtual void setFrequency(float freq) {
        incr2.setPolar(1.f, 0.5f * freq * mul);
        k1 = incr2.im / incr2.re;
        k2 = 2.f * k1 / (1 + k1 * k1);
    }
    /**
     * Get oscillator frequency in Hertz
     */
    virtual float getFrequency() {
        return incr2.getPhase() * 2 / mul;
    }
    /**
     * Set current oscillator phase in radians
     * @param phase a value between 0 and 2*pi
     */
    virtual void setPhase(ComplexFloat phase) {
        phasor = phase;
    }
    /**
     * Get current oscillator phase in radians
     * @return a value between 0 and 2*pi
     */
    virtual ComplexFloat getPhase() {
        return phasor.getPhase();
    }
    /**
     * Reset oscillator (typically resets phase)
     */
    virtual void reset() {
        phasor = {1.f, 0.f};
        ph = 0.f;
    }
    /**
     * Produce a sample with frequency modulation.
     */
    virtual ComplexFloat generate(float fm) {
        ComplexFloat result = phasor;
        updatePhase(fm);
        return phasor;
    }
    virtual ComplexFloat generate() {
        ComplexFloat result = phasor;
        updatePhase(0.f);
        return phasor;
    }
    virtual void generate(ComplexFloatArray output) {
        for (size_t i = 0; i < output.getSize(); ++i) {
            output[i] = generate();
        }
    }

    virtual void generate(ComplexFloatArray output, FloatArray fm) {
        for (size_t i = 0; i < output.getSize(); ++i) {
            output[i] = generate(fm[i]);
        }
    }

    static ComplexDomainOscillator* create(float sr) {
        auto* osc = new ComplexDomainOscillator();
        osc->setSampleRate(sr);
        return osc;
    }

    static void destroy(ComplexDomainOscillator* osc) {
        delete osc;
    }

protected:
    ComplexFloat phasor {};
    ComplexFloat incr2 {};
    float mul = 0.f;
    float ph = 0.f;
    float k1 = 0.f;
    float k2 = 0.f;

    void updatePhase(float fm) {
        /**
         * See "A New Recursive Quadrature Oscillator" by Martin Vicanek for formula derivation
         **/
        float t = phasor.re - k1 * phasor.im * (1.f + fm);
        phasor.im += k2 * t * (1.f + fm);
        phasor.re = t - k1 * phasor.im * (1.f + fm);
    }
};
#endif
