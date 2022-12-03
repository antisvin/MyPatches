#ifndef __SAW_OSCILLATOR_HPP__
#define __SAW_OSCILLATOR_HPP

#include "Oscillator.h"

class SawOscillator : public BandlimitedOscillator {
public:
    SawOscillator(float sr = 48000)
        : BandlimitedOscillator(sr) {
    }
    SawOscillator(float freq, float sr)
        : BandlimitedOscillator(freq, sr) {
    }
    static SawOscillator* create(float sr) {
        return new SawOscillator(sr);
    }
    static void destroy(SawOscillator *osc) {
        delete osc;
    }
protected:
    void render(size_t size, float* out) override {
        while (size--) {
            float this_sample = next_sample;
            next_sample = 0.0f;

            phase += nfreq;

            // Handle negative frequencies for TZFM
            while (phase < 0.0) {
                phase += 1.0;
            }

            if (!high) {
                float t = 1.0 - phase / nfreq;
                high = true;
                this_sample += stmlib::ThisBlepSample(t);
                next_sample += stmlib::NextBlepSample(t);                
            }
            if (phase >= 1.0) {
                phase -= 1.0;
                float t = phase / nfreq;
                this_sample -= stmlib::ThisBlepSample(t);
                next_sample -= stmlib::NextBlepSample(t);
                high = false;
            }

            next_sample += renderNaive();

            *out++ = (2.0f * this_sample - 1.0f);
        }
    }

    float next_sample = 0.0;
    bool high = false;

    inline float renderNaive(){
        return phase;
    }    
};

#endif
