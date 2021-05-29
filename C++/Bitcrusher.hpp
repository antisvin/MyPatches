#ifndef __BITCRUSHER_HPP__
#define __BITCRUSHER_HPP__

#include "SignalProcessor.h"

/**
 * We can bitcrush up to 31 bits (in theory). But considering that we'll be dealing with
 * f32 format as input, there won't be more than 24 bits of data to use. Also, there doesn't
 * seem to be noticeable effect until we crush 12+.
 **/
template<uint8_t crush_bits = 23>
class BitcrusherProcessor : public SignalProcessor {
public:
    BitcrusherProcessor()
        : mask(0) {};

    /**
     * Depth in bits
     **/
    void setDepth(uint8_t depth) {
        mask = ~((1 << depth) - 1);
    }

    float process(float input) {
        int32_t tmp = (int32_t)(input * max_int) & mask;
        return (float)tmp / max_int;
    }

    void process(FloatArray input, FloatArray output) {
        size_t size = input.getSize();
        for (size_t i = 0; i < size; i++) {
            int32_t tmp = (int32_t)(input[i] * max_int) & mask;
            output[i] = (float)tmp / max_int;
        }
    }

protected:
    uint32_t mask;
    static constexpr int max_int = (1 << (crush_bits + 1)) - 1;
};

template <uint8_t crush_bits>
class SmoothBitcrusherProcessor : public SignalProcessor {
public:
    SmoothBitcrusherProcessor()
        : mask1(0)
        , mask2(0)
        , morph(0.f) {
    }

    /**
     * Crush amount in [0..1] range
     **/
    void setCrush(float crush) {
        float depth_float = crush_bits * crush;
        uint8_t depth = depth_float;
        mask2 = ~((1 << depth) - 1);
        if (depth > 0)
            depth -= 1;
        mask1 = ~((1 << depth) - 1);
        morph = depth_float - depth;
    }

    float process(float input) {
        int32_t tmp1 = (int32_t)(input * max_int) & mask1;
        int32_t tmp2 = (int32_t)(input * max_int) & mask2;
        float a = (float)tmp1 / max_int;
        float b = (float)tmp2 / max_int;
        return a + (b - a) * morph;
    }

    void process(FloatArray input, FloatArray output) {
        size_t size = input.getSize();
        for (size_t i = 0; i < size; i++) {
            float sample = input[i];
            int32_t tmp1 = (int32_t)(sample * max_int) & mask1;
            int32_t tmp2 = (int32_t)(sample * max_int) & mask2;
            float a = (float)tmp1 / max_int;
            float b = (float)tmp2 / max_int;
            output[i] =  a + (b - a) * morph;
        }
    }

protected:
    float morph;
    uint32_t mask1, mask2;
    static constexpr int max_int = (1 << (crush_bits + 1)) - 1;
};

using Bitcrusher = SmoothBitcrusherProcessor<23>;
#endif
