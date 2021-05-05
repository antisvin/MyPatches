#ifndef __ROSE_OSCILLATOR_HPP__
#define __ROSE_OSCILLATOR_HPP__

#include "QuadratureOscillator.hpp"

/**
 * A complex (quadrature) oscillator based on Rose (rhodonea curve).
 * See https://en.wikipedia.org/wiki/Rose_(mathematics) for details
 */

class RoseOscillator : public QuadratureOscillator {
public:
    RoseOscillator(float sr = 48000)
        : mul(2 * M_PI / sr) {
        feedback2.re = 0.f;
        feedback2.im = 0.f;
    }
    RoseOscillator(float freq, float sr)
        : RoseOscillator(sr) {
        setFrequency(freq);
    }
    void reset() {
        phase = 0.0f;
    }
    void setRatio(float k) {
        this->k = k;
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
    void setFeedback1(float feedback) {
        feedback1 = feedback;
    }
    void setFeedback2(float magnitude, float phase) {
        feedback2.setPolar(magnitude, phase);
    }
    void setHarmonics(float harmonics) {
        this->harmonics = harmonics;
    }
    void generate(AudioBuffer& output) override {
        render<false>(output.getSize(), NULL, output.getSamples(0).getData(),
            output.getSamples(1).getData());
    }
    virtual ComplexFloat generate(float fm) override {
        ComplexFloat sample;
        render<true>(1, &fm, (float*)&sample.re, (float*)&sample.im);
        return sample;
    }
    void generate(AudioBuffer& output, FloatArray fm) override {
        render<true>(output.getSize(), fm.getData(),
            output.getSamples(0).getData(), output.getSamples(1).getData());
    }

protected:
    float mul = 0.f;
    float incr = 0.f;
    float phase = 0.f;
    float harmonics = 1.f;
    float k = 1.f;
    float modPhase = 0.f;
    float feedback1 = 0.f;
    ComplexFloat feedback2;
    float last_x = 0.f, last_y = 0.f, last_t = 0.f;

    template <bool with_fm>
    void render(size_t size, float* fm, float* out_x, float* out_y) {
        while (size--) {
            float t = 1.f - harmonics + harmonics * cosf(modPhase + last_t * feedback1);
            last_t = t;
            float x = cosf(phase + last_x * feedback2.re) * t;
            float y = sinf(phase + last_y * feedback2.im) * t;

            phase += incr;
            modPhase += k * incr;
            if (with_fm) {
                //                modPhase += *fm;
                phase += *fm++;
            }
            if (phase >= M_PI * 2.f)
                phase -= M_PI * 2.f;
            else if (phase < 0.f)
                phase += M_PI * 2.f;
            if (modPhase >= M_PI * 2.f) {
                modPhase -= M_PI * 2.f;
            }

            *out_x++ = x;
            last_x = x;
            *out_y++ = y;
            last_y = y;
        }
    }
};

#endif
