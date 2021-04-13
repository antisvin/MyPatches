#ifndef __SQUARE_OSCILLATOR_HPP__
#define __SQUARE_OSCILLATOR_HPP__

#include "BandlimitedOscillator.hpp"

class SquareOscillator : public PWBandlimitedOscillator {
public:
    SquareOscillator(float sr = 48000)
        : PWBandlimitedOscillator(sr)
        , previous_pw(0.5)
        , next_sample(0.0)  {}
    SquareOscillator(float freq, float sr)
        : PWBandlimitedOscillator(freq, sr)
        , previous_pw(0.5)
        , next_sample(0.0) {}

protected:
    float previous_pw;
    bool high;
    float next_sample;

    void render(size_t size, float* out) override {
        while (size--) {
            float this_sample = next_sample;
            next_sample = 0.0f;

            phase += nfreq;

            // Handle negative frequencies for TZFM
            while (phase < 0){
                phase += 1.0;
            }            

            if (!high && phase >= pw) {
                float t = (phase - pw) / (previous_pw - pw + phase);

                this_sample += stmlib::ThisBlepSample(t);
                next_sample += stmlib::NextBlepSample(t);
                high = true;
            }

            if (high && phase >= 1.0) {
                phase -= 1.0;

                float t = phase / nfreq;
                    
                this_sample -= stmlib::ThisBlepSample(t);
                next_sample -= stmlib::NextBlepSample(t);
                high = false;
            }

            next_sample += renderNaiveSample();
            previous_pw = pw;

            *out++ = (2.0f * this_sample - 1.0f);
        }
    }
    inline float renderNaiveSample(){
        return phase < pw ? 0.0f : 1.0f;
    }
};

#endif
