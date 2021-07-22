#ifndef _ENGINE_CLIPPING_H_
#define _ENGINE_CLIPPING_H_

#include <cmath>
#include "SignalProcessor.h"

// Source: https://github.com/fabianesqueda/Agave/blob/master/src/dsp/Waveshaping.hpp

/*
Other clipping algos we could try:

Tangential soft clipper:
F0(x) = tanh(x)
F1(x) = ln(cosh(x))

tanh(x) can be approximated with sign(x) when |x| >> 1
then
F1(x) ~~ F1(x0 * sign(x)) + (x ** 2 - x0 ** 2) / 2 * sign(x) , if |x| >= x0 >> 1

-----

Exponential soft clipper:
|x| > 2/3 -> sgn(x)
else -> sgn(x) * (1 - (|3x / 2 - sgn(x)|) ** E)

Sinusoid soft clipper:
|x| > 2/3 -> sgn(x)
else -> sin(3 * PI * x / 4)

Two stage quadratic soft clipper
|x| > 2/3 -> sgn(x)
1/3 <= |x| <= 2/3 -> sgn(x) * (3 - (2 - |3 * x|) ** 2) / 3
- 1/3 < x < 1/3 -> 2 * x

Cubic:
|x| > 2/3 -> sgn(x)
else -> 9 * x / 4 - 27 * (x ** 3) / 16

Reciprocal:
sgn(x) * (1 - 1 / (|30 * x| + 1))
*/

static constexpr float onesixths = 1.0f / 6.0f;

class AbstractClipper : public SignalProcessor {
public:
    float process(float input) override {
        return antialiasedClipN1(input);
    }

    void process(FloatArray input, FloatArray output) {
        for (size_t i = 0; i < input.getSize(); i++) {
            output[i] = process(input[i]);
        }
    }


    void reset() {
        xn1 = 0.0f;
        Fn = 0.0f;
        Fn1 = 0.0f;
    }

protected:
    // Antialiasing variables
    float xn1;
    float Fn;
    float Fn1;
    static constexpr float thresh = 10.0e-2;

    virtual float clipN0(float x) = 0;
    virtual float clipN1(float x) = 0;
    virtual float clipN2(float x) = 0;

    inline float signum(float x) {
        return (x > 0.0f) ? 1.0f : ((x < 0.0f) ? -1.0f : 0.0f);
    }

    float antialiasedClipN1(float x) {
        Fn = clipN1(x);
        float tmp = 0.0;
        if (abs(x - xn1) < thresh) {
            tmp = clipN0(0.5f * (x + xn1));
        }
        else {
            tmp = (Fn - Fn1) / (x - xn1);
        }

        // Update states
        xn1 = x;
        Fn1 = Fn;

        return tmp;
    }
};

class QuadraticClipper : public AbstractClipper {
public:
    float clipN0(float x) override {
        if (abs(x) < 1) {
            return x * abs(x);
        }
        else {
            return signum(x);
        }
    }

    float clipN1(float x) override {
        if (abs(x) < 1) {
            return abs(x) * x * x / 3.0f;
        }
        else {
            return abs(x);
        }
    }
    float clipN2(float x) override {
        if (abs(x) < 1) {
            return abs(x) * x * x * x / 12.0f;
        }
        else {
            return 0.5 * x * abs(x);
        }
    }
};

class CubicClipper : public AbstractClipper {
public:

    float clipN0(float x) override {
        if (abs(x) >= 1.f) {
            return signum(x);
//            return 2.f / 3.f * signum(x);
        }
        else {
            return 1.5f * x - x * x * x / 2.f;
//            return x - x * x * x / 3.f;
        }
    }

    float clipN1(float x) override {
        if (abs(x) >= 1.f) {
//        if (abs(x) >= 2.f / 3.f) {
            return x * signum(x) - 0.5f; // abs(x) - 0.5 ?
        }
        else {
            0.75f * x * x - x * x * x * x / 8.f;
//            return x * x / 2.f - x * x * x * x / 12.f;
        }
    }

    float clipN2(float x) override {
        if (abs(x) >= 1.f) {
            return (0.5f * x * x - onesixths) * signum(x);
//            return x * abs(x) / 3.f;
        }
        else {
            return x * x * x / 4.f - x * x * x * x / 40.f;
//            return onesixth * x * x * x  -  x * x * x * x * x / 60.f;
        }
    }
};

class SineClipper : public AbstractClipper {
public:
    float clipN0(float x) override {
        return sinf(-M_PI -
            0.25f * M_PI * (signum(x + 1) * (x + 1) - signum(x - 1) * (x - 1)));
    }

    float clipN1(float x) override {
        return 4 / M_PI +
            4 / M_PI *
            cosf(-M_PI -
                0.25f * M_PI * (signum(x + 1) * (x + 1) - signum(x - 1) * (x - 1)));
    }

    float clipN2(float x) override {
        // second antiderivative of hardClipN0
        /// TODO
        return 0.f;
    }
};

class HardClipper : public AbstractClipper {
public:
    float clipN0(float x) override {
        // Hard clipping function
        return 0.5f * (signum(x + 1.0f) * (x + 1.0f) - signum(x - 1.0f) * (x - 1.0f));
    }

    float clipN1(float x) override {
        // First antiderivative of hardClipN0
        return 0.25f *
            (signum(x + 1.0f) * (x + 1.0f) * (x + 1.0f) -
                signum(x - 1.0f) * (x - 1.0f) * (x - 1.0f) - 2.0f);
    }

    float clipN2(float x) override {
        // second antiderivative of hardClipN0
        return oneTwelfth *
            (signum(x + 1.0f) * (x + 1.0f) * (x + 1.0f) * (x + 1.0f) -
                signum(x - 1.0f) * (x - 1.0f) * (x - 1.0f) * (x - 1.0f) - 6.0f * x);
    }

private:
    static constexpr float oneTwelfth = 1.0 / 12.0;
};

#endif
