#ifndef __MULTI_OSCILLATOR_HPP__
#define __MULTI_OSCILLATOR_HPP__

#include "Oscillator.h"
#include "FloatArray.h"

/**
 * A class that contains multiple child oscillators and morphs between 2 of them
 */
template <size_t limit, typename OscillatorClass>
class MultiOscillatorTemplate : public OscillatorClass {
public:
    MultiOscillatorTemplate(float sr = 48000)
        : OscillatorClass()
        , mul(1.0 / sr)
        , nfreq(0.0)
        , phase(0) {
        oscillators = new OscillatorClass*[limit];
        setSampleRate(sr);
    }
    MultiOscillatorTemplate(float freq, float sr)
        : MultiOscillatorTemplate(sr) {
        setFrequency(freq);
    }
    ~MultiOscillatorTemplate() {
        for (size_t i = 0; i < num_oscillators; i++) {
            delete oscillators[i];
        }
        delete[] oscillators;
    }
    using OscillatorClass::generate;
    void reset() override {
        setPhase(0.0f);
    }
    virtual void setSampleRate(float sr) override {
        for (size_t i = 0; i < num_oscillators; i++) {
            oscillators[i]->setSampleRate(sr);
        }
        mul = 1.0f / sr;
    }
    void setFrequency(float freq) override {
        for (size_t i = 0; i < num_oscillators; i++) {
            oscillators[i]->setFrequency(freq);
        }
        nfreq = mul * freq;
    }
    float getFrequency() override {
        return nfreq / mul;
    }
    void setPhase(float phase) override {
        for (size_t i = 0; i < num_oscillators; i++) {
            oscillators[i]->setPhase(phase);
        }
        this->phase = phase;
    }
    float getPhase() override {
        return phase;
    }
    void setMorph(float morph) {
        morph *= (num_oscillators - 1);
        morph_idx = size_t(morph);
        oscillators[morph_idx]->setPhase(phase);
        oscillators[morph_idx + 1]->setPhase(phase);
        morph_frac = morph - float(morph_idx);
    }

    bool addOscillator(OscillatorClass* osc) {
        if (num_oscillators < limit) {
            oscillators[num_oscillators++] = osc;
            return true;
        }
        else {
            return false;
        }
    }
    size_t getSize() const {
        return num_oscillators;
    }
    float generate() override {
        float sample_a = oscillators[morph_idx]->generate();
        float sample_b = oscillators[morph_idx + 1]->generate();
        phase = oscillators[morph_idx]->getPhase();
        return sample_a + (sample_b - sample_a) * morph_frac;
    }
    virtual void generate(FloatArray output) override {
        // TODO
        for (size_t i = 0; i < output.getSize(); i++) {
            output[i] = generate();
        }
    }
    float generate(float fm) override {
        float sample_a = oscillators[morph_idx]->generate(fm);
        float sample_b = oscillators[morph_idx + 1]->generate(fm);
        phase = oscillators[morph_idx]->getPhase();
        return sample_a + (sample_b - sample_a) * morph_frac;
    }
    static MultiOscillatorTemplate* create(float sr) {
        return new MultiOscillatorTemplate(sr);
    }
    static MultiOscillatorTemplate* create(float freq, float sr) {
        return new MultiOscillatorTemplate(freq, sr);
    }
    static void destroy(MultiOscillatorTemplate* osc) {
        // We can't destroy child oscillators here, because this is a static method that
        // has no idea about their vptrs. Must be done separately by caller class.
        delete osc;
    }

protected:
    size_t num_oscillators = 0;
    OscillatorClass** oscillators;
    size_t morph_idx = 0;
    float morph_frac;
    float mul;
    float nfreq;
    float phase;
};

template <size_t limit>
class MultiOscillator : public MultiOscillatorTemplate<limit, Oscillator> {
public:
    // using MultiOscillatorTemplate<limit, Oscillator>::oscillators;
    // using MultiOscillatorTemplate<limit, Oscillator>::morph_idx;
    //    MultiOscillator()
    //        : MultiOscillatorTemplate<limit, Oscillator> {}
};

#if 0

#include "QuadratureOscillator.hpp"

class QuadratureMultiOscillator : public MultiOscillatorTemplate<QuadratureOscillator> {
public:
    QuadratureMultiOscillator(size_t limit, float sr = 48000)
        : MultiOscillatorTemplate<QuadratureOscillator>(limit, sr) {};
    QuadratureMultiOscillator(size_t limit, float freq, float sr)
        : MultiOscillatorTemplate<QuadratureOscillator>(limit, freq, sr) {
    }

    virtual void setSampleRate(float sr) override {
        for (size_t i = 0; i < num_oscillators; i++) {
            oscillators[i]->setSampleRate(sr);
        }
        mul = M_PI * 2.f / sr;
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
        ComplexFloat sample_a = oscillators[morph_idx]->generate();
        ComplexFloat sample_b = oscillators[morph_idx + 1]->generate();
        phase = oscillators[morph_idx]->getPhase();
        return ComplexFloat { sample_a.re + (sample_b.re - sample_a.re) * morph_frac,
            sample_a.im + (sample_b.im - sample_a.im) * morph_frac };
    }
    ComplexFloat generate(float fm) override {
        ComplexFloat sample_a = oscillators[morph_idx]->generate(fm);
        ComplexFloat sample_b = oscillators[morph_idx + 1]->generate(fm);
        phase = oscillators[morph_idx]->getPhase();
        return ComplexFloat { sample_a.re + (sample_b.re - sample_a.re) * morph_frac,
            sample_a.im + (sample_b.im - sample_a.im) * morph_frac };
    }
};
#endif
#endif
