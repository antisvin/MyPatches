#ifndef __SAW_OSCILLATOR_HPP__
#define __SAW_OSCILLATOR_HPP

#include "Oscillator.h"

class SawOscillator : public BandlimitedOscillator {
public:
    SawOscillator(float sr = 48000)
        : BandlimitedOscillator(sr)
        , next_sample(0.0) {
    }
    SawOscillator(float freq, float sr)
        : BandlimitedOscillator(freq, sr)
        , next_sample(0.0) {
    }
protected:
    void render(size_t size, float* out) {
        size_t size = output.getSize();
        float* out = output.getData();
        bool reset = false;

        while (size--) {
            float this_sample = next_sample;
            next_sample = 0.0f;

            phase += incr;
            float ph_norm = phase / M_PI / 2.0;
            while (!reset) {
                if (ph_norm < 1.0)
                    break;
                phase -= M_PI * 2.0;
                ph_norm -= 1.0;
                float t = ph_norm / incr;
                this_sample -= stmlib::ThisBlepSample(t);
                next_sample -= stmlib::NextBlepSample(t);
            }
            next_sample += renderNaive(ph_norm);

            *out++ = (2.0f * this_sample - 1.0f);
        }
    }

    float mul;
    float phase;
    float incr;
    float nfreq;
    float next_sample;
    bool high;

    inline float renderNaive(float ph_norm){
        return ph_norm;
    }    
};

#endif
