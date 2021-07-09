#ifndef __DISCRETE_SUMMATION_OSCILLATOR_HPP__
#define __DISCRETE_SUMMATION_OSCILLATOR_HPP__

#include "QuadratureOscillator.hpp"

enum DSFShape {
    DSF1,
    DSF2,
    DSF3,
    DSF4,
};

/**
 * Quadrature oscillator based on Moorer's discrete summation formulas
 *
 * See The Synthesis of Complex Audio Spectra by Means of Discrete Summation Formulas - James A. Moorer.
 *
 * https://www.desmos.com/calculator/xhuksewprf
 *
 * DSF1: finite series, one-sided spectrum (-b != b)
 * DSF2: infinite series, one-sided spectrum (-b != b)
 * DSF3: finite series, two-sided spectrum (-b == b)
 * DSF4: infinite series, two-sided spectrum (-b == b)
 **/
template <DSFShape shape>
class DiscreteSummationOscillator : public QuadratureOscillator {
public:
    DiscreteSummationOscillator(float sr = 48000)
        : mul(2 * M_PI / sr)
        , phase(0)
        , modPhase(0)
        , incr(0) {
    }
    DiscreteSummationOscillator(float freq, float sr)
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

    void setA(float a) {
        this->a = a;
    }
    void setB(float b) {
        this->b = b;
    }
    void setN(float n) {
        this->n = n;
    }

    void generate(AudioBuffer& output) override {
        render<false>(output.getSize(), NULL, output.getSamples(0).getData(),
            output.getSamples(1).getData());
    }
    ComplexFloat generate() override {
        ComplexFloat sample;
        render<false>(1, nullptr, (float*)&sample.re, (float*)&sample.im);
        return sample;
    }
    ComplexFloat generate(float fm) override {
        ComplexFloat sample;
        render<true>(1, &fm, (float*)&sample.re, (float*)&sample.im);
        return sample;
    }
    void generate(AudioBuffer& output, FloatArray fm) override {
        render<true>(output.getSize(), fm.getData(),
            output.getSamples(0).getData(), output.getSamples(1).getData());
    }

protected:
    float mul;
    float incr;
    float phase;
    float modPhase;
    float a, b, n;

    template <bool with_fm>
    void render(size_t size, float* fm, float* out_x, float* out_y);
};

template <>
template <bool with_fm>
void DiscreteSummationOscillator<DSF1>::render(
    size_t size, float* fm, float* out_x, float* out_y) {
    float a1 = powf(a, n + 1.f);
    float a2 = n + 1.f;
    float a3 = (1.f + a * a);
    float a4 = 2.f * a;
    float gain = (a - 1.f) / (a1 - 1.f);
    float s = modPhase;
    
    while (size--) {
        *out_x++ = gain * (sin(phase) - a * sin(phase - s) -
                       a1 * (sin(phase + s * a2) - a * sin(phase + n * s))) /
            (a3 - a4 * cos(s));
        *out_y++ = gain * (cos(phase) - a * cos(phase - s) -
                       a1 * (cos(phase + s * a2) - a * cos(phase + n * s))) /
            (a3 - a4 * cos(s));
        phase += incr;
        s += b * incr;
        if (with_fm)
            phase += *fm++;
    }
    phase = fmodf(phase, M_PI * 2.f);
    modPhase = fmodf(s, M_PI * 2.f);
}

template <>
template <bool with_fm>
void DiscreteSummationOscillator<DSF2>::render(
    size_t size, float* fm, float* out_x, float* out_y) {
    float a1 = (1.f + a * a);
    float a2 = 2.f * a;
    float gain = (1.f - a);
    float s = modPhase;
    while (size--) {
        *out_x++ = gain * (sin(phase) - a * sin(phase - s)) / (a1 - a2 * cos(s));
        *out_y++ = gain * (cos(phase) - a * cos(phase - s)) / (a1 - a2 * cos(s));
        phase += incr;
        s += b * incr;
        if (with_fm)
            phase += *fm++;
    }
    phase = fmodf(phase, M_PI * 2.f);
    modPhase = fmodf(s, M_PI * 2.f);
}

template <>
template <bool with_fm>
void DiscreteSummationOscillator<DSF3>::render(
    size_t size, float* fm, float* out_x, float* out_y) {
    float a1 = powf(a, n + 1.f);
    float a2 = n + 1.f;
    float a3 = 1.f + a * a;
    float a4 = 2.f * a;
    float gain = (a - 1.f) / (2 * a1 - a - 1.f);
    float s = modPhase;
    while (size--) {
        float b1 = 1.f / (a3 - a4 * cos(s));
        *out_x++ = gain * (sin(phase) - a * sin(phase - s) -
            a1 * (sin(phase + s * a2) - a * sin(phase + n * s))) * b1;
        *out_y++ = gain * (cos(phase) - a * cos(phase - s) -
            a1 * (cos(phase + s * a2) - a * cos(phase + n * s))) * b1;

        phase += incr;
        s += b * incr;
        if (with_fm) {
            float fm_sample = *fm++;
            phase += fm_sample;
            //phase = fmodf(phase + 2.f * M_PI, M_PI * 2.f);
            //s += (*fm++) * b;
        }
    }
    phase = fmodf(phase, M_PI * 2.f);
    modPhase = fmodf(s, M_PI * 2.f);
}

template <>
template <bool with_fm>
void DiscreteSummationOscillator<DSF4>::render(
    size_t size, float* fm, float* out_x, float* out_y) {
    float a1 = 1.f - a * a;
    float a2 = 1.f + a * a;
    float a3 = 2.f * a;
    float gain = (1.f - a) / (1.f + a);
    float s = modPhase;
    while (size--) {
        float b1 = 1.f / (a2 - a3 * cos(s));
        *out_x++ = gain * a1 * sin(phase) * b1;
        *out_y++ = gain * a1 * cos(phase) * b1;
        phase += incr;
        s += b * incr;

        if (with_fm) {
            //modPhase += *fm * b;
            phase += *fm++;
        }
    }
    phase = fmodf(phase, M_PI * 2.f);
    modPhase = fmodf(s, M_PI * 2.f);
}

#endif
