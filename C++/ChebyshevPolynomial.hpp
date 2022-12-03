#ifndef __CHEBYSHEV_POLYNOMIAL_HPP__
#define __CHEBYSHEV_POLYNOMIAL_HPP__

#include "SignalProcessor.h"

template <int kind>
class ChebyshevPolynomial : public SignalProcessor {
public:
    ChebyshevPolynomial()
        : order(0) {};
    ChebyshevPolynomial(size_t order)
        : order(order) {};

    void setOrder(size_t order) {
        this->order = order;
    }
    float process(float input) override;

private:
    size_t order;
    inline float compute(float input) {
        float t0 = 1.0;
        float t1 = input;
        if (order == 0)
            return t0;
        else if (order == 1) {
            return t1;
        }
        else {
            float t;
            for (size_t i = 2; i <= order; i++) {
                t = 2.f * t1 * input - t0;
                t0 = t1;
                t1 = t;
            }
            return t;
        }
    }
};

/**
 * @brief Chebyshev polynomial of a first kind:
 *
 * T0(x) = 1
 * T1(x) = x
 * Tn(x) = 2 * Tn-1(x) - Tn-2(x)
 *
 * @tparam
 * @param input
 * @return float
 */
template <>
float ChebyshevPolynomial<1>::process(float input) {
    return compute(input);
}

/**
 * @brief Chebyshev polynomial of a second kind
 *
 * U0(x) = 1
 * U1(x) = 2 * x
 * Un(x) = 2 * x * Un-1(x) - Un-2(x)
 *
 * @tparam
 * @param input
 * @return float
 */
template <>
float ChebyshevPolynomial<2>::process(float input) {
    return compute(input * 2);
}

/**
 * @brief Chebyshev polynomial of a third kind
 *
 * V0(x) = 1
 * V1(x) = 2 * x - 1
 * Vn(x) = 2 * x * Vn-1(x) - Vn-2(x)
 *
 * @tparam
 * @param input
 * @return float
 */
template <>
float ChebyshevPolynomial<3>::process(float input) {
    return compute(input * 2.f - 1.f);
}

/**
 * @brief Chebyshev polynomial of a fourth kind
 *
 * W0(x) = 1
 * W1(x) = 2 * x + 1
 * Wn(x) = 2 * x * Wn-1(x) - Wn-2(x)
 *
 * @tparam
 * @param input
 * @return float
 */
template <>
float ChebyshevPolynomial<4>::process(float input) {
    return compute(input * 2.f + 1.f);
}

template <int kind = 1>
class MorphingChebyshevPolynomial : public SignalProcessor {
public:
    void setOffset(size_t offset) {
        this->offset = offset;
    }
    void setMaxOrder(size_t order) {
        max_order = order;
    }
    void setMorph(float morph) {
        morph *= (max_order - 1.f - offset);
        morph += offset;
        size_t morph_order = morph;
        cheby1.setOrder(morph_order);
        cheby2.setOrder(morph_order + 1);
        morph_frac = morph - float(morph_order);
    }
    float process(float input) override {
        float a = cheby1.process(input);
        return a + (cheby2.process(input) - a) * morph_frac;
    }
    void process(FloatArray input, FloatArray output) {
        size_t size = input.getSize();
        for (size_t i = 0; i < size; i++) {
            float in = input[i];
            float a = cheby1.process(in);
            output[i] = a + (cheby2.process(in) - a) * morph_frac;
        }
    }

private:
    float morph_frac = 0.f;
    float max_order = 0;
    size_t offset = 0;
    ChebyshevPolynomial<kind> cheby1, cheby2;
};

/**
 * Polynomials sum:
 * a0 * x ^ n + a1 * x ^ (n - 1) + ... + an
 **/
template <size_t max_order, class Sample, class Base, class T, typename CoefType = float>
class WeightedChebyshevPolynomialTemplate : public Base {
public:
    WeightedChebyshevPolynomialTemplate()
        : mult(0.f)
        , coefficients {}
        , coefficients_prev {} {};
    WeightedChebyshevPolynomialTemplate(size_t block_size)
        : block_size(block_size)
        , mult(1.f / block_size) {
    }

    void setCoefficient(size_t index, CoefType coefficient) {
        coefficients[index] = coefficient;
    }
    void setHarmonic(size_t index, CoefType harmonic) {
        coefficients[max_order - index] = harmonic;
    }
    void updateGain() {
        float _gain = 0.f;
        for (size_t i = 0; i < max_order + 1; i++) {
            _gain += abs(coefficients[i]);
        }
        if (_gain > 0)
            _gain = 1.f / _gain;
        gain = _gain;
    }
    void normalize() {
        updateGain();
        for (size_t i = 0; i < max_order + 1; i++) {
            coefficients[i] *= gain;
            deltas[i] = (coefficients[i] - coefficients_prev[i]) * mult;
        }
        memcpy(coefficients_prev, coefficients, sizeof(CoefType) * (max_order + 1));
        for (size_t i = 0; i < max_order + 1; i++) {
            coefficients[i] -= deltas[i] * block_size;
        }
    }
    float getGain() const {
        return gain;
    }
    Sample process(Sample input) override {
        static_cast<T*>(this)->updatePolynomials(input);
        Sample output = 0;
        for (size_t i = 0; i < max_order; i++) {
            output += polynomials[i] * coefficients[i];
        }
        return output;
    }
    static T* create(size_t block_size) {
        return new T(block_size);
    }

    static void destroy(T* poly) {
        delete poly;
    }
    void updatePolynomials(Sample input);

protected:
    Sample polynomials[max_order + 1] {};
    CoefType coefficients[max_order + 1] {};
    CoefType coefficients_prev[max_order + 1] {};
    CoefType deltas[max_order + 1] {};
    float mult, gain;
    size_t block_size;
};

template <size_t max_order>
class RealWeightedPolynomials
    : public WeightedChebyshevPolynomialTemplate<max_order, float,
          SignalProcessor, RealWeightedPolynomials<max_order>> {
public:
    using Base = WeightedChebyshevPolynomialTemplate<max_order, float,
        SignalProcessor, RealWeightedPolynomials<max_order>>;
    using Base::create;
    using Base::destroy;
    using Base::process;
    using Base::WeightedChebyshevPolynomialTemplate;
    void process(FloatArray input, FloatArray output) {
        for (size_t i = 0; i < input.getSize(); i++) {
            for (size_t j = 0; j < max_order + 1; j++) {
                coefficients[j] += deltas[j];
            }
            updatePolynomials(input[i]);
            output[i] = 0;
            for (size_t j = 0; j < max_order; j++) {
                output[i] += polynomials[j] * coefficients[j];
            }
        }
    }
    void updatePolynomials(float input) {
        polynomials[max_order] = 1.f;
        polynomials[max_order - 1] = input;
        for (int i = max_order - 2; i > 0; i--) {
            polynomials[i] = 2.f * input * polynomials[i + 1] - polynomials[i + 2];
        }
    }

protected:
    using Base::coefficients;
    using Base::deltas;
    using Base::polynomials;
};

template <size_t max_order>
class ComplexWeightedPolynomials
    : public WeightedChebyshevPolynomialTemplate<max_order, ComplexFloat,
          ComplexSignalProcessor, ComplexWeightedPolynomials<max_order>> {
public:
    using Base = WeightedChebyshevPolynomialTemplate<max_order, ComplexFloat,
        ComplexSignalProcessor, ComplexWeightedPolynomials<max_order>>;
    using Base::create;
    using Base::destroy;
    using Base::process;
    using Base::WeightedChebyshevPolynomialTemplate;
    void process(ComplexFloatArray input, ComplexFloatArray output) {
        for (size_t i = 0; i < input.getSize(); i++) {
            for (size_t j = 0; j < max_order + 1; j++) {
                coefficients[j] += deltas[j];
            }
            updatePolynomials(input[i]);
            output[i] = 0;
            for (size_t j = 0; j < max_order; j++) {
                output[i] += polynomials[j] * coefficients[j];
            }
        }
    }
    void updatePolynomials(ComplexFloat input) {
        polynomials[max_order] = { 1.f, 1.f };
        polynomials[max_order - 1] = input;
        for (int i = max_order - 2; i > 0; i--) {
            polynomials[i] = input * polynomials[i + 1] * 2.f - polynomials[i + 2];
            /*
            polynomials[i] = {
                2.f * input.re * polynomials[i + 1].re - polynomials[i + 2].re,
                2.f * input.im * polynomials[i + 1].im - polynomials[i + 2].im,
            };
            */
        }
    }

protected:
    using Base::coefficients;
    using Base::deltas;
    using Base::polynomials;
};

#endif
