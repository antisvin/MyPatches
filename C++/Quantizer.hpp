#ifndef __QUANTIZER_HPP__
#define __QUANTIZER_HPP__

/**
 * Quantize float value in range [0..1] into num_steps steps.
 * Optionally check for output range.
 */
template<size_t num_steps, bool check_limits=false>
class Quantizer : public SignalProcessor {
public:
    Quantizer() = default;
    ~Quantizer() = default;
    float process(float input) override {
        if (abs(value - input) >= delta) {
            value = round(input * num_steps) * delta;
            if (check_limits) {
                if (value < 0.0)
                    value = 0.0;
                else if(value > 1.0) {
                    value = 1.0;
                }
            }
        }
        return value;
    }

protected:
    static constexpr float delta = 1.0 / num_steps;
    float value;
};

using Quantizer12TET = Quantizer<12>;
using Quantizer19TET = Quantizer<19>;
using Quantizer24TET = Quantizer<24>;
using Quantizer31TET = Quantizer<31>;
#endif
