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
 * Unclipped signal is not distorted
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


/**
 * This is based on the classic cubic softclip, but without scaling it to [-2/3..2/3]
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
    static float getAntiderivative2(float x) {
        float a = std::abs(x);
        if (a >= 1.f)
            return a * x / 2 - x * 3.f / 8.f + signum(x) / 10;
        else {
            float a = x * x;
            return a * x / 4 - a * a * x / 40;
        }
    }
};

/**
 * Based on x^2 function, multiplied by sign to preserve symmetry
 **/
class SecondOrderPolynomial : public Nonlinearity {
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
    static float getAntiderivative2(float x) {
        float xabs = std::abs(x);
        if (xabs > 1.f)
            return x * xabs / 2 - x / 3 + signum(x) / 12;
        else
            return x * x * x * (1.f / 3 - xabs / 12);
    }
};

/**
 * Based on Andrew Simper coefficients for third order polynomial
 **/
class ThirdOrderPolynomial : public Nonlinearity {
public:
    static float getSample(float x) {
        if (std::abs(x) >= 1.5f)
            return signum(x);
        else
            return x - x * x * x * 4 / 27;
    }
    static float getAntiderivative1(float x) {
        float a = std::abs(x);
        if (a >= 1.5f)
            return a - 9.f / 16.f;
        else {
            float b = x * x;
            return b / 2 - b * b / 27;
        }
    }
    static float getAntiderivative2(float x) {
        float a = std::abs(x);
        if (a >= 1.5f)
            return a * x / 2 - x * 9 / 16 + signum(x) * 36 / 160;
        else {
            float b = a * a;
            return b * x / 6 - b * b * x / 135;
        }
    }
};

class FourthOrderPolynomial : public Nonlinearity {
public:
    static float getSample(float x) {
        if (std::abs(x) >= 1.f)
            return signum(x);
        else {
            return x * x * x * (4 - std::abs(x)) - x * std::abs(x) * 6 + x * 4;
        }
    }
    static float getAntiderivative1(float x) {
        float a = std::abs(x);
        if (a >= 1.f)
            return a - 0.2f;
        else {
            float b = x * x;
            float c = b * b;
            return -a * c / 5 + c + b * 2 * (1.f - a);
        }
    }
    static float getAntiderivative2(float x) {
        float a = std::abs(x);
        if (a >= 1.f)
            return x * a / 2 - x / 5 + signum(x) / 30.f;
        else {
            float b = a * a;
            return b * b * x * (-a / 30 + 1 / 5) + x * b * (-a / 2 + 2.f / 3);
        }
    }
};


/**
 * Uses x / sqrt(1 + x ^ 2) function
 **/
class AlgebraicSaturator : public Nonlinearity {
public:
    static float getSample(float x) {
        return x / sqrtf(1 + x * x);
    }
    static float getAntiderivative1(float x) {
        return sqrtf(1.f + x * x) - 1.f;
    }
    static float getAntiderivative2(float x) {
        float a = sqrtf(1.f + x * x);
        return 0.5f * (x * a + fast_logf(x + a)) - x;
    }
};

/**
 * Popular tanh saturator.
 * Note: Computing second derivative is too untrivial and expensive
 **/
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

/**
 * Distortion based on arctangent function.
 * 
 * Note: can't be used for wavefolding as it doesn't reach +-1
 **/
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

/**
 * Sinusoid function distortion
 **/
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


/**
 * Based on sin^2 function
 **/
class QuadraticSineSaturator : public Nonlinearity {
public:
    static float getSample(float x) {
        if (std::abs(x) >= 1)
            return signum(x);
        else {
            float a = sin(M_PI_2 * x);
            return std::abs(a) * a;
        }
    }
    static float getAntiderivative1(float x) {
        if (std::abs(x) >= 1)
            return std::abs(x) - 0.5f;
        else {
            return signum(x) * 0.5f * (x - sin(M_PI * x) / M_PI);
        }
    }
    static float getAntiderivative2(float x) {
        if (std::abs(x) >= 1)
            return x * std::abs(x) / 2 - x / 2 + signum(x) * (0.25 - 1.f / (M_PI * M_PI));
        else {
            return signum(x) / (M_PI * M_PI * 2) * (M_PI * M_PI * x * x / 2 + cos(M_PI * x) - 1.f);
        }
    }
};

/**
 * A rather extreme distortion based on sin^3 function
 **/
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


/**
 * Based on 1 / 2x function
 */
class ReciprocalSaturator : public Nonlinearity {
public:
    static float getSample(float x) {
        if (std::abs(x) > 0.5f)
            return signum(x) - 0.25 / x;
        else
            return x;
    }
    static float getAntiderivative1(float x) {
        float xabs = std::abs(x);
        if (xabs > 0.5f)
            return xabs - 0.25f * fast_logf(xabs) - 0.5f - M_LN2 / 4 + 0.125;
        else
            return x * x / 2;
    }
    static float getAntiderivative2(float x) {
        float xabs = std::abs(x);
        if (xabs > 0.5f)
            return x * xabs / 2 - x * fast_logf(xabs) / 4 - x * (1.f / 8 + M_LN2 / 4) - signum(x) / 24;
        else
            return x * x * x / 6;
    }
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
class AliasingWaveshaperTemplate : public SignalProcessor {
public:
    AliasingWaveshaperTemplate() = default;
    ~AliasingWaveshaperTemplate() = default;
    float process(float input) override {
        return Function::getSample(input);
    }

    static AliasingWaveshaperTemplate* create() {
        return new AliasingWaveshaperTemplate();
    }

    static void destroy(AliasingWaveshaperTemplate* waveshaper) {
        delete waveshaper;
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
        Fn = Function::getAntiderivative1(x);
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

    static AntialiasedWaveshaperTemplate* create() {
        return new AntialiasedWaveshaperTemplate();
    }

    static void destroy(AntialiasedWaveshaperTemplate* waveshaper) {
        delete waveshaper;
    }
protected:
    float xn1, Fn, Fn1;
    static constexpr float thresh = 10.0e-2;
};


using AliasingHardClipper = WaveshaperTemplate<HardClip>;
using AliasingCubicSaturator = WaveshaperTemplate<CubicSaturator>;
using AliasingSecondOrderPolynomial = WaveshaperTemplate<SecondOrderPolynomial>;
using AliasingThirdOrderPolynomial = WaveshaperTemplate<ThirdOrderPolynomial>;
using AliasingFourthOrderPolynomial = WaveshaperTemplate<FourthOrderPolynomial>;
using AliasingAlgebraicSaturator = WaveshaperTemplate<AlgebraicSaturator>;
using AliasingTanhSaturator = WaveshaperTemplate<TanhSaturator>;
using AliasingArctanSaturator = WaveshaperTemplate<ArctanSaturator>;
using AliasingSineSaturator = WaveshaperTemplate<SineSaturator>;
using AliasingQuadraticSineSaturator = WaveshaperTemplate<QuadraticSineSaturator>;
using AliasingCubicSineSaturator = WaveshaperTemplate<CubicSineSaturator>;
using AliasingReciprocalSaturator = WaveshaperTemplate<ReciprocalSaturator>;

using AntialiasedHardClipper = AntialiasedWaveshaperTemplate<HardClip>;
using AntialiasedCubicSaturator = AntialiasedWaveshaperTemplate<CubicSaturator>;
using AntialiasedSecondOrderPolynomial = AntialiasedWaveshaperTemplate<SecondOrderPolynomial>;
using AntialiasedThirdOrderPolynomial = AntialiasedWaveshaperTemplate<ThirdOrderPolynomial>;
using AntialiasedFourthOrderPolynomial = AntialiasedWaveshaperTemplate<FourthOrderPolynomial>;
using AntialiasedAlgebraicSaturator = AntialiasedWaveshaperTemplate<AlgebraicSaturator>;
using AntialiasedTanhSaturator = AntialiasedWaveshaperTemplate<TanhSaturator>;
using AntialiasedArctanSaturator = AntialiasedWaveshaperTemplate<ArctanSaturator>;
using AntialiasedSineSaturator = AntialiasedWaveshaperTemplate<SineSaturator>;
using AntialiasedQuadraticSineSaturator = AntialiasedWaveshaperTemplate<QuadraticSineSaturator>;
using AntialiasedCubicSineSaturator = AntialiasedWaveshaperTemplate<CubicSineSaturator>;
using AntialiasedReciprocalSaturator = AntialiasedWaveshaperTemplate<ReciprocalSaturator>;

#endif
