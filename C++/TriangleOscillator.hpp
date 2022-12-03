#ifndef __TRIANGLE_OSCILLATOR_HPP__
#define __TRIANGLE_OSCILLATOR_HPP__

#include "BandlimitedOscillator.hpp"

class TriangleOscillator : public PWBandlimitedOscillator {
public:
    TriangleOscillator(float sr = 48000)
        : PWBandlimitedOscillator(sr)
        , previous_pw(0.5)
        , next_sample(0.0) {
    }
    TriangleOscillator(float freq, float sr)
        : PWBandlimitedOscillator(freq, sr)
        , previous_pw(0.5)
        , next_sample(0.0) {
    }
    static TriangleOscillator* create(float sr) {
        return new TriangleOscillator(sr);
    }
    static void destroy(TriangleOscillator *osc) {
        delete osc;
    }
protected:
    float previous_pw;
    bool high;
    float next_sample;

    void render(size_t size, float* out) override {
        while (size--) {
            float this_sample = next_sample;
            next_sample = 0.0f;

            const float slope_up = 1.0f / (pw);
            const float slope_down = 1.0f / (1.0f - pw);

            phase += nfreq;

            // Handle negative frequencies for TZFM
            while (phase < 0.0) {
                phase += 1.0;
            }

            if (!high && phase >= pw) {
                // BLEP for first discontinuty after triangle upper peak
                const float t = (phase - pw) / (previous_pw - pw + nfreq);
                const float triangle_step = (slope_up + slope_down) * nfreq;
                this_sample -= triangle_step * stmlib::ThisIntegratedBlepSample(t);
                next_sample -= triangle_step * stmlib::NextIntegratedBlepSample(t);
                high = true;
            }
            if (high && phase >= 1.0) {
                // BLEP for second discontinuty after triangle lower peak
                // Wrap phase here
                //while (phase >= 1.0)
                    phase -= 1.0f;
                const float t = phase / nfreq;
                const float triangle_step = (slope_up + slope_down) * nfreq;
                this_sample += triangle_step * stmlib::ThisIntegratedBlepSample(t);
                next_sample += triangle_step * stmlib::NextIntegratedBlepSample(t);
                high = false;
            }

            next_sample += renderNaiveSample(slope_up, slope_down);
            previous_pw = pw;

            *out++ = (2.0f * this_sample - 1.0f);
        }
    }

    inline float renderNaiveSample(float slope_up, float slope_down) {
        return phase < pw ? phase * slope_up : 1.0f - (phase - pw) * slope_down;
    }
};

#endif
