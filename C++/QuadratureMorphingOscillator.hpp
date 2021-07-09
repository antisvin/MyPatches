#ifndef __QUADRATURE_MORPHING_OSCILLATOR__
#define __QUADRATURE_MORPHING_OSCILLATOR__

template <typename OscA, typename OscB>
class QuadratureMorphingOscillator : public QuadratureOscillator {
public:
    OscA osc_a;
    OscB osc_b;

    QuadratureMorphingOscillator() : QuadratureOscillator(), morph(0) {}

    void setSampleRate(float sr) override {
        osc_a.setSampleRate(sr);
        osc_b.setSampleRate(sr);
    }
    void setFrequency(float freq) override {
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
    virtual void generate(AudioBuffer& output) override {
        FloatArray left = output.getSamples(0);
        FloatArray right = output.getSamples(1);
        for (size_t i = 0; i < output.getSize(); i++) {
            ComplexFloat sample = generate();
            left[i] = sample.re;
            right[i] = sample.im;
        }
    }
    virtual void generate(AudioBuffer& output, FloatArray fm) override {
        FloatArray left = output.getSamples(0);
        FloatArray right = output.getSamples(1);
        for (size_t i = 0; i < output.getSize(); i++) {
            ComplexFloat sample = generate(fm[i]);
            left[i] = sample.re;
            right[i] = sample.im;
        }
    }
    ComplexFloat generate() {
        ComplexFloat sample_a = osc_a.generate();
        ComplexFloat sample_b = osc_b.generate();
        return ComplexFloat { sample_a.re + (sample_b.re - sample_a.re) * morph,
            sample_a.im + (sample_b.im - sample_a.im) * morph };
    }
    ComplexFloat generate(float fm) override {
        ComplexFloat sample_a = osc_a.generate(fm);
        ComplexFloat sample_b = osc_b.generate(fm);
        return ComplexFloat { sample_a.re + (sample_b.re - sample_a.re) * morph,
            sample_a.im + (sample_b.im - sample_a.im) * morph };
    }

protected:
    float morph;
};
#endif
