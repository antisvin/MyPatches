#ifndef __BYPASS_PROCESSOR_HPP__
#define __BYPASS_PROCESSOR_HPP__

#include "FloatArray.h"
#include "Interpolator.h"

template <typename Processor, uint8_t interpolate_samples = 64>
class BypassProcessor : public Processor {
private:
    inline static float constexpr transition_step = 1.0 / interpolate_samples;

public:
    BypassProcessor()
        : bypassed(true)
        , transitioned(0) {
    }
    void setBypass(bool state) {
        if (state != bypassed)
            transitioned = 64;
        bypassed = state;
    }

    bool getBypass() const {
        return bypassed;
    }

    float process(float input) {
        float output;
        if (bypassed) {
            if (transitioned > 0) {
                transitioned--;
                output = Interpolator::linear(Processor::process(input), input,
                    float(interpolate_samples + 1 - transitioned) * transition_step);
            }
            else {
                output = input;
            }
        }
        else {
            output = Processor::process(input);
            if (transitioned > 0) {
                transitioned--;
                output = Interpolator::linear(input, output,
                    float(interpolate_samples + 1 - transitioned) * transition_step);
            }
        }
        return output;
    }

    void process(FloatArray input, FloatArray output) {
        // Not tested yet
        if (bypassed) {
            if (transitioned) {
                Processor::process(input, output);
                input.scale(0, 1);
                output.scale(1, 0);
                output.add(input);
                transitioned = 0;
            }
            else {
                output.copyFrom(input);
            }
        }
        else {
            Processor::process(input, output);
            if (transitioned) {
                output.scale(0, 1);
                input.scale(1, 0);
                output.add(input);
                transitioned = 0;
            }
        }
    }

    static BypassProcessor* create() {
        return new BypassProcessor();
    }
    static void destroy(BypassProcessor* processor) {
        delete processor;
    }

protected:
    bool bypassed;
    uint8_t transitioned;
};

#endif
