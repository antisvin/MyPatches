#ifndef __WAVEFOLDER_HPP__
#define __WAVEFOLDER_HPP__

#include "SignalProcessor.h"
#include "Clipping.hpp"

// Source: https://github.com/fabianesqueda/Agave/blob/master/src/dsp/Waveshaping.hpp

template <typename Function>
class AntialiasedWaveFolder : public SignalProcessor {
private:
    // Antialiasing state variables
    float xn1 = 0.0;
    float xn2 = 0.0;
    float Fn = 0.0;
    float Fn1 = 0.0;
    float Gn = 0.0;
    float Gn1 = 0.0;

    // Ill-conditioning threshold
    const float thresh = 10.0e-2;

    const float oneSixth = 1.0 / 6.0;
public:
    AntialiasedWaveFolder() = default;
    ~AntialiasedWaveFolder() = default;
    virtual void process(FloatArray input, FloatArray output) {
        for (size_t i = 0; i < output.getSize(); ++i)
            output[i] = process(input[i]);
    }
    float process(float input) {
        return antialiasedFoldN2(input);
    }
    float foldFunctionN0(float x) {
        // Folding function
        return (2.0f * Function::getSample(x) - x);
    }
    float foldFunctionN1(float x) {
        // First antiderivative of the folding function
        return (2.0f * Function::getAntiderivative1(x) - 0.5f * x * x);
    }
    float foldFunctionN2(float x) {
        // Second antiderivative of the folding function
        return (2.0f * Function::getAntiderivative2(x) - oneSixth * (x * x * x));
    }
    float antialiasedFoldN1(float x) {
        // Folding with 1st-order antialiasing (not recommended)
        Fn = foldFunctionN1(x);
        float tmp = 0.0;
        if (abs(x - xn1) < thresh) {
            tmp = foldFunctionN0(0.5f * (x + xn1));
        }
        else {
            tmp = (Fn - Fn1) / (x - xn1);
        }

        // Update states
        xn1 = x;
        Fn1 = Fn;

        return tmp;
    }
    float antialiasedFoldN2(float x) {

        // Folding with 2nd-order antialiasing
        Fn = foldFunctionN2(x);
        float tmp = 0.0;
        if (abs(x - xn1) < thresh) {
            // First-order escape rule
            Gn = foldFunctionN1(0.5f * (x + xn1));
        }
        else {
            Gn = (Fn - Fn1) / (x - xn1);
        }

        if (abs(x - xn2) < thresh) {
            // Second-order escape
            float delta = 0.5f * (x - 2.0f * xn1 + xn2);
            if (abs(delta) < thresh) {
                tmp = foldFunctionN0(0.25f * (x + 2.0f * xn1 + xn2));
            }
            else {
                float tmp1 = foldFunctionN1(0.5f * (x + xn2));
                float tmp2 = foldFunctionN2(0.5f * (x + xn2));
                tmp = (2.0f / delta) * (tmp1 + (Fn1 - tmp2) / delta);
            }
        }
        else {
            tmp = 2.0f * (Gn - Gn1) / (x - xn2);
        }

        // Update state variables
        Fn1 = Fn;
        Gn1 = Gn;
        xn2 = xn1;
        xn1 = x;

        return tmp;
    }
};

#endif
