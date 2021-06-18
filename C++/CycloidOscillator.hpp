#ifndef __CYCLOID_OSCILLATOR_HPP__
#define __CYCLOID_OSCILLATOR_HPP__

#include "QuadratureOscillator.hpp"

/**
 * A complex (quadrature) oscillator based on epicycloid/hypocycloid
 * See http://xahlee.info/SpecialPlaneCurves_dir/EpiHypocycloid_dir/epiHypocycloid.html for details
 * about this curve
 */

class CycloidOscillator : public QuadratureOscillator {
public:
    CycloidOscillator(float sr = 48000)
        : mul(2 * M_PI / sr) {
        //feedback2.re = 0.f;
        //feedback2.im = 0.f;
    }
    CycloidOscillator(float freq, float sr)
        : CycloidOscillator(sr) {
        setFrequency(freq);
    }
    void reset() {
        phase = 0.0f;
        modPhase = 0.0f;
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
        modPhase += (ph - phase) * ratio;
        phase = ph;
    }
    float getPhase() {
        return phase;
    }
    /*void setFeedback1(float feedback) {
        feedback1 = feedback;
    }*/
    void setFeedback(float magnitude, float phase, float a = 1.f, float b = 1.f) {
        feedback.setPolar(magnitude, phase);
        fb_a_amt = a;
        fb_b_amt = b;
    }
    void setHarmonics(float harmonics) {
        this->harmonics = harmonics;
    }
    void setRatio(float ratio) {
        this->ratio = ratio;
        ratioIncr = incr * ratio;
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
    float ratioIncr = 0.f;
    float phase = 0.f;
    float harmonics = 1.f;
    float ratio = 1.f;
    float modPhase = 0.f;
    ComplexFloat feedback;
    float fb_a_amt = 1.f, fb_b_amt = 1.f;
    float last_x = 0.f, last_y = 0.f, last_t = 0.f;

    template <bool with_fm>
    void render(size_t size, float* fm, float* out_x, float* out_y) {
        float h = harmonics;
        float h_rev = 1.f - harmonics;
        float ph = phase;
        float mph = modPhase;
        while (size--) {
//            float x = h_rev * cos(ph) + h * cos(mph + last_x * feedback.re);
//            float y = h_rev * sin(ph) + h * sin(mph + last_y * feedback.im);
            float fb_x = last_x * feedback.re;
            float fb_y = last_y * feedback.im;
            float x = h_rev * cos(ph + fb_x * fb_a_amt) + h * cos(mph + fb_x * fb_b_amt);
            float y = h_rev * sin(ph + fb_y * fb_a_amt) + h * sin(mph + fb_y * fb_b_amt);

//            float x = cosf(phase + last_x * feedback2.re) * t;
//            float y = sinf(phase + last_y * feedback2.im) * t;

            ph += incr;
            mph += ratioIncr;
            if (with_fm) {
                //                modPhase += *fm;
                phase += *fm;
                modPhase *= *fm++;
            }
        
            *out_x++ = x;
            last_x = x;
            *out_y++ = y;
            last_y = y;
        }
        while (ph >= M_PI * 2.f)
            ph -= M_PI * 2.f;
        while (ph < 0.f)
            ph += M_PI * 2.f;
        phase = ph;
        while (mph >= M_PI * 2.f)
            mph -= M_PI * 2.f;
        while (mph < 0)
            mph += M_PI * 2.f;
        modPhase = mph;
    }
};

#endif
