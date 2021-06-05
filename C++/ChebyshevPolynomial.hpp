#ifndef __CHEBYSHEV_POLYNOMIAL_HPP__
#define __CHEBYSHEV_POLYNOMIAL_HPP__

#include "SignalProcessor.h"

template<bool use_trigonometry=false>
class ChebyshevPolynomial : public SignalProcessor{
public:
    ChebyshevPolynomial() : order(0) {};
    ChebyshevPolynomial(size_t oredre) : order(order) {};

    void setOrder(size_t order) {
        this->order = order;
    }
    float process(float input) override;

private:
    size_t order;
};

template<>
float ChebyshevPolynomial<true>::process(float input) {
    return cos(order * acos(input));
}


template<>
float ChebyshevPolynomial<false>::process(float input) {
    float t2 = 1.f;
    float t1 = input;
    if (order == 0)
        return t2;
    else if (order == 1) {
        return t1;
    }
    else {
        float t;
        for (size_t i = 2; i <= order; i++) {
            t = 2.f * t1 * input - t2;
            t1 = t;
            t2 = t1;
        }
        return t;
    }
}


template<bool use_trigonometry=false>
class MorphingChebyshevPolynomial : public SignalProcessor {
public:
    void setOffset(float offset) {
        this->offset = offset;
    }
    void setMaxOrder(size_t order) {
        max_order = order;
    }
    void setMorph(float morph){
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
    float offset = 0;
    ChebyshevPolynomial<use_trigonometry> cheby1, cheby2;
};
#endif
