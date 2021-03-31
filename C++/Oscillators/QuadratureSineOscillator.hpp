#ifndef __QUADRATURE_SINE_OSCILLATOR_HPP__
#define __QUADRATURE_SINE_OSCILLATOR_HPP__

#include "QuadratureOscillator.hpp"

class QuadratureSineOscillator : public QuadratureOscillator {
public:
    QuadratureSineOscillator(float sr = 48000)
        : mul(2 * M_PI / sr)
        , phase(0)
        , incr(0) {
    }
    QuadratureSineOscillator(float freq, float sr)
        : mul(2 * M_PI / sr)
        , phase(0.0f) {
        setFrequency(freq);
    }
    void reset() {
        phase = 0.0f;
    }
    void setSampleRate(float sr) {
        float freq = getFrequency();
        mul = 2 * M_PI / sr;
        setFrequency(freq);
    }
    float getSampleRate() {
        return (2 * M_PI) / mul;
    }
    void setFrequency(float freq) {
        incr = freq * mul;
    }
    float getFrequency() {
        return incr / mul;
    }
    void setPhase(float ph) {
        phase = ph;
    }
    float getPhase() {
        return phase;
    }
    ComplexFloat generate() {
        ComplexFloat sample { sinf(phase), cos(phase) };
        phase += incr;
        if (phase >= 2 * M_PI)
            phase -= 2 * M_PI;
        return sample;
    }
    void generate(AudioBuffer& output) override {
        size_t len = output.getSize();
        // Real channel
        float* out = output.getSamples(0).getData();
        for (size_t i = 0; i < len; ++i) {
            *out++ = sinf(phase);
            phase += incr; // allow phase to overrun
        }
        // Imaginary channel. Pointer to output is already in place
        out = output.getSamples(1).getData();
        for (size_t i = 0; i < len; ++i) {
            *out++ = cosf(phase);
            phase += incr; // allow phase to overrun
        }
        phase = fmodf(phase, 2 * M_PI);
    }
    ComplexFloat generate(float fm) override {
        ComplexFloat sample { sinf(phase), cosf(phase) };
        phase += incr + fm;
        if (phase >= 2 * M_PI)
            phase -= 2 * M_PI;
        return sample;
    }

protected:
    float mul;
    float incr;
    float phase;
};

#endif
