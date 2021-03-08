#ifndef __BANDLIMITED_OSCILLATOR_HPP__
#define __BANDLIMITED_OSCILLATOR_HPP__

#include "Oscillator.h"
#include "polyblep/dsp/polyblep.h"


class BandlimitedOscillator : public Oscillator {
public:
    BandlimitedOscillator(float sr = 48000)
        : mul(1.0 / sr)
        , nfreq(0.0)
        , phase(0) {
    }
    BandlimitedOscillator(float freq, float sr) : BandlimitedOscillator(sr) {
        BandlimitedOscillator::setFrequency(freq);
    }
    void reset() override {
        phase = 0.0f;
    }
    void setSampleRate(float sr) override {
        mul = 1.0f / sr;
    }
    float getSampleRate() override {
        return 1.0f / mul;
    }
    void setFrequency(float freq) override {
        nfreq = mul * freq;
    }
    float getFrequency() override {
        return nfreq / mul;
    }
    void setPhase(float phase) override {
        this->phase = phase / (M_PI * 2.0);
    }
    float getPhase() override {
        return phase * 2.0 * M_PI;
    }
    float generate() override {
        float sample;
        render(1, &sample);
        return sample;        
    }
    void generate(FloatArray output) override {
        render(output.getSize(), output.getData());
    }
    float generate(float fm) override {
        float sample;
        render(1, &sample);
        phase += fm;
        return sample;
    }
protected:
    float mul;
    float nfreq;
    float phase;
    virtual void render(size_t size, float* out) = 0;
};


class PWBandlimitedOscillator : public BandlimitedOscillator {
public:
    PWBandlimitedOscillator(float sr = 48000)
        : BandlimitedOscillator(sr)
        , pw(0.5) {
    }
    PWBandlimitedOscillator(float freq, float sr)
        : BandlimitedOscillator(freq, sr)
        , pw(0.5) {
    }
    void setPulseWidth(float value) {
        pw = value;
    } 
protected:
    float pw;
};
#endif
