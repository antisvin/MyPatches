#ifndef __QUADRATURE_MORPHING_OSCILLATOR__
#define __QUADRATURE_MORPHING_OSCILLATOR__

//#include "StereoOscillator.h"
#include "SignalGenerator.h"

template <typename OscA, typename OscB>
class StereoMorphingOscillator : public ComplexSignalGenerator {
public:
    OscA osc_a;
    OscB osc_b;

    StereoMorphingOscillator() : morph(0) {}

    void setSampleRate(float sr) {
        osc_a.setSampleRate(sr);
        osc_b.setSampleRate(sr);
    }
    void setFrequency(float freq) {
        osc_a.setFrequency(freq);
        osc_b.setFrequency(freq);
    }
    void setMorph(float morph) {
        this->morph = morph;
    }
    void setPhase(float phase) {
        osc_a.setPhase(phase);
        osc_b.setPhase(phase);
    }
    float getPhase() {
        return osc_a.getPhase();
    }
    virtual void generate(ComplexFloatArray output) {
        for (size_t i = 0; i < output.getSize(); i++) {
            output[i] = generate();
        }
    }
    virtual void generate(ComplexFloatArray output, FloatArray fm) {
        for (size_t i = 0; i < output.getSize(); i++) {
            output[i] = generate(fm[i]);
        }
    }
    ComplexFloat generate() {
        ComplexFloat sample_a = osc_a.generate();
        ComplexFloat sample_b = osc_b.generate();
        return ComplexFloat { sample_a.re + (sample_b.re - sample_a.re) * morph,
            sample_a.im + (sample_b.im - sample_a.im) * morph };
    }
    ComplexFloat generate(float fm) {
        ComplexFloat sample_a = osc_a.generate(fm);
        ComplexFloat sample_b = osc_b.generate(fm);
        return ComplexFloat { sample_a.re + (sample_b.re - sample_a.re) * morph,
            sample_a.im + (sample_b.im - sample_a.im) * morph };
    }

protected:
    float morph;
};
#endif
