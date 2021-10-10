#ifndef __DISCRETE_SUMMATION_OSCILLATOR_HPP__
#define __DISCRETE_SUMMATION_OSCILLATOR_HPP__

#include "ComplexOscillator.h"

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
class DiscreteSummationOscillator
    : public ComplexOscillatorTemplate<DiscreteSummationOscillator<shape>> {
public:
    static constexpr float begin_phase = 0.f;
    static constexpr float end_phase = M_PI * 2;

    DiscreteSummationOscillator()
        : mul(0)
        , phase(0)
        , modPhase(0)
        , incr(0)
        , last_x(0)
        , last_y(0)
        , ComplexOscillatorTemplate<DiscreteSummationOscillator<shape>>() {
    }
    DiscreteSummationOscillator(float freq, float sr)
        : DiscreteSummationOscillator() {
        setFrequency(freq);
        setFeedback(0, 0);
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

    void setFeedback(float magnitude, float phase) {
        feedback.setPolar(magnitude, phase);
    }

    ComplexFloat getSample();

    void generate(ComplexFloatArray output) override {
        render<false>(output.getSize(), NULL, output.getData());
    }
    ComplexFloat generate() override {
        ComplexFloat sample;
        render<false>(1, nullptr, &sample);
        return sample;
    }
    ComplexFloat generate(float fm) override {
        ComplexFloat sample;
        render<true>(1, &fm, &sample);
        return sample;
    }
    void generate(ComplexFloatArray output, FloatArray fm) {
        render<true>(output.getSize(), fm.getData(), output.getData());
    }

protected:
    float mul;
    float incr;
    float phase;
    float modPhase;
    float a, b, n;
    ComplexFloat feedback;
    float last_x, last_y;

    template <bool with_fm>
    void render(size_t size, float* fm, ComplexFloat* out);
};

template <>
template <bool with_fm>
void DiscreteSummationOscillator<DSF1>::render(
    size_t size, float* fm, ComplexFloat* out) {
    float a1 = powf(a, n + 1.f);
    float a2 = n + 1.f;
    float a3 = (1.f + a * a);
    float a4 = 2.f * a;
    float gain = (a - 1.f) / (a1 - 1.f);
    float s = modPhase;
    float _last_x = last_x;
    float _last_y = last_y;
    while (size--) {
        float phase_re = phase + _last_x * feedback.re;
        float phase_im = phase + _last_y * feedback.im;
        if (with_fm) {
            float fm_mod = M_PI * 2.f * *fm++;
            phase_re += fm_mod;
            phase_im += fm_mod;
        }
        out->re = gain *
            (cos(phase_re) - a * cos(phase_re - s) -
                a1 * (cos(phase_re + s * a2) - a * cos(phase_re + n * s))) /
            (a3 - a4 * cos(s));
        out->im = gain *
            (sin(phase_im) - a * sin(phase_im - s) -
                a1 * (sin(phase_im + s * a2) - a * sin(phase_im + n * s))) /
            (a3 - a4 * cos(s));
        out++;
        phase += incr;
        s += b * incr;
    }
    phase = fmodf(phase, M_PI * 2.f);
    modPhase = fmodf(s, M_PI * 2.f);
    last_x = _last_x;
    last_y = _last_y;
}

template <>
template <bool with_fm>
void DiscreteSummationOscillator<DSF2>::render(
    size_t size, float* fm, ComplexFloat* out) {
    float a1 = (1.f + a * a);
    float a2 = 2.f * a;
    float gain = (1.f - a);
    float s = modPhase;
    float _last_x = last_x;
    float _last_y = last_y;
    while (size--) {
        float phase_re = phase + _last_x * feedback.re;
        float phase_im = phase + _last_y * feedback.im;
        if (with_fm) {
            float fm_mod = M_PI * 2.f * *fm++;
            phase_re += fm_mod;
            phase_im += fm_mod;
        }
        _last_x =
            gain * (cos(phase_re) - a * cos(phase_re - s)) / (a1 - a2 * cos(s));
        out->re = _last_x;
        _last_y =
            gain * (sin(phase_im) - a * sin(phase_im - s)) / (a1 - a2 * cos(s));
        out->im = _last_y;
        out++;
        phase += incr;
        s += b * incr;
    }
    phase = fmodf(phase, M_PI * 2.f);
    modPhase = fmodf(s, M_PI * 2.f);
    last_x = _last_x;
    last_y = _last_y;
}

template <>
template <bool with_fm>
void DiscreteSummationOscillator<DSF3>::render(
    size_t size, float* fm, ComplexFloat* out) {
    float a1 = powf(a, n + 1.f);
    float a2 = n + 1.f;
    float a3 = 1.f + a * a;
    float a4 = 2.f * a;
    float gain = (a - 1.f) / (2 * a1 - a - 1.f);
    float s = modPhase;
    float _last_x = last_x;
    float _last_y = last_y;
    while (size--) {
        float phase_re = phase + _last_x * feedback.re;
        float phase_im = phase + _last_y * feedback.im;
        if (with_fm) {
            float fm_mod = M_PI * 2.f * *fm++;
            phase_re += fm_mod;
            phase_im += fm_mod;
        }
        float b1 = 1.f / (a3 - a4 * cos(s));
        _last_x = gain *
            (cos(phase_re) - a * cos(phase_re - s) -
                a1 * (cos(phase_re + s * a2) - a * cos(phase_re + n * s))) *
            b1;
        out->re = _last_x;
        _last_y = gain *
            (sin(phase_im) - a * sin(phase_im - s) -
                a1 * (sin(phase_im + s * a2) - a * sin(phase_im + n * s))) *
            b1;
        out->im = _last_y;
        out++;
        phase += incr;
        s += b * incr;
    }
    phase = fmodf(phase, M_PI * 2.f);
    modPhase = fmodf(s, M_PI * 2.f);
    last_x = _last_x;
    last_y = _last_y;
}

template <>
template <bool with_fm>
void DiscreteSummationOscillator<DSF4>::render(
    size_t size, float* fm, ComplexFloat* out) {
    float a1 = 1.f - a * a;
    float a2 = 1.f + a * a;
    float a3 = 2.f * a;
    float gain = (1.f - a) / (1.f + a);
    float s = modPhase;
    float _last_x = last_x;
    float _last_y = last_y;
    while (size--) {
        float phase_re = phase + _last_x * feedback.re;
        float phase_im = phase + _last_y * feedback.im;
        if (with_fm) {
            float fm_mod = M_PI * 2.f * *fm++;
            phase_re += fm_mod;
            phase_im += fm_mod;
        }
        float b1 = 1.f / (a2 - a3 * cos(s));
        _last_x = gain * a1 * cos(phase_re) * b1;
        out->re = _last_x;
        _last_y = gain * a1 * sin(phase_im) * b1;
        out->im = last_y;
        out++;
        phase += incr;
        s += b * incr;
    }
    phase = fmodf(phase, M_PI * 2.f);
    modPhase = fmodf(s, M_PI * 2.f);
    last_x = _last_x;
    last_y = _last_y;
}

#endif
