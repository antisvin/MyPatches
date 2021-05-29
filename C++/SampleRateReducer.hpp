#ifndef __SAMPLE_RATE_REDUCER_HPP__
#define __SAMPLE_RATE_REDUCER_HPP__

#include "SignalProcessor.h"

template <uint32_t max_reduction = 8>
class SampleRateReducer : public SignalProcessor {
public:
    SampleRateReducer()
        : last_step(0)
        , last_sample(0.f) {
    }

    void setFactor(float factor) {
        factor *= max_reduction;
        mask = (1 << (uint32_t)factor) - 1;
    }

    float process(float input) {
        ++last_step &= mask;
        if (last_step == 0)
            last_sample = input;
        return last_sample;
    }

    void process(FloatArray input, FloatArray output) {
        float sample = last_sample;
        size_t size = input.getSize();
        size_t step = last_step;
        for (size_t i = 0; i < size; i++) {
            ++step &= mask;
            if (step == 0)
                sample = input[i];
            output[i] = sample;
        }
        last_step = step;
        last_sample = sample;
    }

protected:
    float last_sample;
    size_t last_step;
    size_t mask;
};

template <uint8_t max_reduction = 8>
class SmoothSampleRateReducer : public SignalProcessor {
public:
    void setFactor(float factor) {
        factor *= max_reduction;
        mask2 = (1 << (uint8_t)factor) - 1;
        if (factor >= 1.f)
            factor -= 1.f;
        mask1 = (1 << (uint8_t)factor) - 1;
        morph = factor - int(factor);
    }

    float process(float input) {
        ++last_step1 &= mask1;
        if (last_step1 == 0)
            last_sample1 = input;
        ++last_step2 &= mask2;
        if (last_step2 == 0)
            last_sample2 = input;
        return last_sample1 + (last_sample2 - last_sample1) * morph;
    }

    void process(FloatArray input, FloatArray output) {
        size_t size = input.getSize();
        float sample1 = last_sample1;
        float sample2 = last_sample2;
        size_t step1 = last_step1;
        size_t step2 = last_step2;
        for (size_t i = 0; i < size; i++) {
            ++step1 &= mask1;
            if (step1 == 0)
                sample1 = input[i];
            ++step2 &= mask2;
            if (step2 == 0)
                sample2 = input[i];
            output[i] = sample1 + (sample2 - sample1) * morph;
        }
        last_step1 = step1;
        last_step2 = step2;
        last_sample1 = sample1;
        last_sample2 = sample2;
    }

protected:
    size_t mask1, mask2;
    float last_sample1, last_sample2;
    size_t last_step1, last_step2;
    float morph;
};

#endif
