#ifndef __NONLINEARITY_HPP__

#include "basicmaths.h"
#include <cmath>
#include "SignalProcessor.h"


class Nonlinearity {
public:
    static float getSample(float x);
    static float getAntiderivative1(float x);
    static float getAntiderivative2(
        float x); // May not be implemented for every nonlinearity

    static float signum(float x) {
        return (x > 0.0f) ? 1.0f : ((x < 0.0f) ? -1.0f : 0.0f);
        //        return (0.f < x) - (x < 0.f);
    }
};

/**
 * Output is in [-1..1] range
 **/
class HardClip : public Nonlinearity {
public:
    static float getSample(float x) {
        return 0.5f * (std::abs(x + 1.0f) - std::abs(x - 1.0f));
    }
    static float getAntiderivative1(float x) {
        float a = x + 1.0f;
        float b = x - 1.0f;
        return 0.25f * (std::abs(a) * a - std::abs(b) * b - 2.0f);
    }
    static float getAntiderivative2(float x) {
        float a = x + 1.0f;
        float b = x - 1.0f;
        return (std::abs(a) * a * a - std::abs(b) * b * b - 6.0f * x) / 12;
    }
};

class AsymmetricHardClip : public Nonlinearity {
public:
    static float getSample(float x) {
        if (std::abs(x) > 1)
            return signum(x);
        else
            return (std::abs(x) + std::abs(x + 1) - std::abs(x - 1) - 1.f) / 2;
    }
    static float getAntiderivative1(float x) {
        float a = x + 1.0f;
        float b = x - 1.0f;
        float xabs = std::abs(x);
        if (xabs > 1.f)
            return xabs - (2.f + signum(x)) / 4;
        else
            return (x * xabs + a * std::abs(a) - b * std::abs(b) - 2.f * a) / 4;
    }
    static float getAntiderivative2(float x) {
        float a = x + 1.0f;
        float b = x - 1.0f;
        float xabs = std::abs(x);
        if (xabs > 1.f)
            return (x * xabs - x) / 2 - xabs / 4 + signum(x) / 6 + 1.f / 12;
        else
            return (x * x * xabs + a * a * std::abs(a) - b * b * std::abs(b) -
                       3.f * x * (x + 2.f)) /
                12;
    }
};

/**
 * Output is truncated to [-2/3..2/3]
 **/
class CubicSoftClip : public Nonlinearity {
public:
    static float getSample(float x) {
        if (std::abs(x) >= 1.f)
            return signum(x) * 2 / 3;
        else
            return x - x * x * x / 3;
    }
    static float getAntiderivative1(float x) {
        float a = std::abs(x);
        if (a >= 1.f)
            return a * 2 / 3 - 0.25f;
        else {
            float a = x * x;
            return a * (6.f - a) / 12;
        }
    }
};

/**
 * Output is truncated to [-1..1]
 **/
class CubicSaturator : public Nonlinearity {
public:
    static float getSample(float x) {
        if (std::abs(x) >= 1.f)
            return signum(x);
        else
            return x * (3.f - x * x) / 2;
    }
    static float getAntiderivative1(float x) {
        float a = std::abs(x);
        if (a >= 1.f)
            return a - 3.f / 8.f;
        else {
            float a = x * x;
            return 3.f * a / 4 - a * a / 8;
        }
    }
};

class TanhSaturator : public Nonlinearity {
public:
    static float getSample(float x) {
        return tanh(x);
    }
    static float getAntiderivative1(float x) {
        return fast_logf((fast_expf(x) + fast_expf(-x)) * 0.5f); // Works well
        // return x - M_LN2 + fast_logf(1.f + M_E - 2.f * x); // Unstable
        // return fast_logf(cosh(x)); // Works, but more expensive to compute
    }
};

class ArctanSaturator : public Nonlinearity {
public:
    static float getSample(float x) {
        return atanf(x) / M_PI;
    }
    static float getAntiderivative1(float x) {
        return (2.f * x * atanf(x) - fast_logf(std::abs(x * x + 1))) / (2.f * M_PI);
    }
    static float getAntiderivative2(float x) {
        float a = x * x;
        return ((a - 1) * atanf(x) - x * fast_logf(a + 1) + x) / (M_PI * 2);
    }
};

class SineSaturator : public Nonlinearity {
public:
    static float getSample(float x) {
        if (std::abs(x) >= 1.f)
            return signum(x);
        else
            return sinf(x * M_PI_2);
    }
    static float getAntiderivative1(float x) {
        if (std::abs(x) >= 1.f)
            return std::abs(x) - 1.f + M_2_PI;
        else
            return M_2_PI - M_2_PI * cosf(x * M_PI_2);
    }
    static float getAntiderivative2(float x) {
        if (std::abs(x) >= 1.f)
            return 0.5f * x * std::abs(x) - x + M_2_PI * x -
                signum(x) * M_2_PI * M_2_PI + 0.5f * signum(x);
        else
            return M_2_PI * (x - M_2_PI * sin(M_PI_2 * x));
    }
};

class CubicSineSaturator : public Nonlinearity {
public:
    static float getSample(float x) {
        if (std::abs(x) >= 1)
            return signum(x);
        else {
            float a = sin(M_PI_2 * x);
            return a * a * a;
        }
    }
    static float getAntiderivative1(float x) {
        if (std::abs(x) >= 1)
            return std::abs(x) + -1.f + 4.f * M_PI / 3;
        else {
            float a = cos(M_PI_2 * x);
            return M_2_PI / 3 * (a * a * a - a * 3 + 2.f);
        }
    }
    static float getAntiderivative2(float x) {
        if (std::abs(x) >= 1)
            return x * std::abs(x) / 2 + x * (4.f - 3.f * M_PI) / M_PI / 3 - signum(x) * (0.5 - 28.f / 9 / M_PI / M_PI);
        else {
            float a = sin(M_PI_2 * x);
            return -4.f / (M_PI  * M_PI * 9) * a * a * a - 8.f / (3 * M_PI * M_PI) * a + 4.f / (3 * M_PI) * x;
        }
    }
};

class QuadraticSaturator : public Nonlinearity {
public:
    static float getSample(float x) {
        if (std::abs(x) > 1)
            return signum(x);
        else
            return x * (2.f - std::abs(x));
    }

    static float getAntiderivative1(float x) {
        float xabs = std::abs(x);
        if (xabs > 1.f)
            return xabs - 1.f / 3;
        else
            return x * x * (1.f - xabs / 3);
    }
};

/**
 * Based on 1 / x function
 */
class ReciprocalSaturator : public Nonlinearity {
public:
    float getSample(float x) {
        if (std::abs(x) > 0.5f)
            return signum(x) - 0.25f / x;
        else
            return x;
    }
    float getAntiderivative1(float x) {
        float xabs = std::abs(x);
        if (xabs > 0.5f)
            return xabs - 0.5f * fast_logf(xabs) - c1;
        else
            return x * x / 2;
    }
    float getAntiderivative2(float x) {
        float xabs = std::abs(x);
        if (xabs > 0.5f)
            return x * xabs / 2 - x * fast_logf(xabs) / 4 + c2 * x - signum(x) / 24;
        else
            return x * x * x / 6;
    }

protected:
    static constexpr float c1 = -3.f / 8 - M_LN2 / 4;
    static constexpr float c2 = -1.f / 8 - M_LN2 / 4;
};

template <typename Function>
class WaveshaperTemplate : public SignalProcessor, public Function {
public:
    WaveshaperTemplate() {
        reset();
    }
    ~WaveshaperTemplate() = default;
    float process(float input) override {
        return this->getSample(input);
    }
    void process(FloatArray input, FloatArray output) {
        for (size_t i = 0; i < input.getSize(); i++) {
            output[i] = this->getSample(input[i]);
        }
    }
    void reset() {
        xn1 = 0.0f;
        Fn = 0.0f;
        Fn1 = 0.0f;
    }

protected:
    float xn1, Fn, Fn1;
    static constexpr float thresh = 10.0e-2;
};

template <typename Function>
class AntialiasedWaveshaperTemplate : public SignalProcessor, public Function {
public:
    AntialiasedWaveshaperTemplate() {
        reset();
    }
    ~AntialiasedWaveshaperTemplate() = default;
    float process(float input) override {
        return antialiasedClipN1(input);
    }
    void process(FloatArray input, FloatArray output) {
        for (size_t i = 0; i < input.getSize(); i++) {
            output[i] = this->antialiasedClipN1(input[i]);
        }
    }
    float antialiasedClipN1(float x) {
        Fn = this->getAntiderivative1(x);
        float tmp = 0.0;
        if (std::abs(x - xn1) < thresh) {
            tmp = this->getSample(0.5f * (x + xn1));
        }
        else {
            tmp = (Fn - Fn1) / (x - xn1);
        }

        // Update states
        xn1 = x;
        Fn1 = Fn;

        return tmp;
    }
    void reset() {
        xn1 = 0.0f;
        Fn = 0.0f;
        Fn1 = 0.0f;
    }

protected:
    float xn1, Fn, Fn1;
    static constexpr float thresh = 10.0e-2;
};

using AntialiasedHardClipper = AntialiasedWaveshaperTemplate<HardClip>;
using AntialiasedAsymmetricHardClipper =
    AntialiasedWaveshaperTemplate<AsymmetricHardClip>;
using AntialiasedCubicSoftClipper = AntialiasedWaveshaperTemplate<CubicSoftClip>;
using AntialiasedCubicSaturator = AntialiasedWaveshaperTemplate<CubicSaturator>;
using AntialiasedQuadraticSaturator =
    AntialiasedWaveshaperTemplate<QuadraticSaturator>;
using AntialiasedArctanSaturator = AntialiasedWaveshaperTemplate<ArctanSaturator>;
using AntialiasedTanhSaturator = AntialiasedWaveshaperTemplate<TanhSaturator>;
using AntialiasedSineSaturator = AntialiasedWaveshaperTemplate<SineSaturator>;
using AntialiasedCubicSineSaturator = AntialiasedWaveshaperTemplate<CubicSineSaturator>;
using AntialiasedReciprocalSaturator =
    AntialiasedWaveshaperTemplate<ReciprocalSaturator>;
#endif
