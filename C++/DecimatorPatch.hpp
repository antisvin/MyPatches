#ifndef __DECIMATOR_PATCH_HPP__
#define __DECIMATOR_PATCH_HPP__

#include "Patch.h"
#include "Bitcrusher.hpp"
#include "SampleRateReducer.hpp"

#define CRUSH_SMOOTH
#define REDUCE_SMOOTH

class DecimatorPatch : public Patch {
public:
#ifdef CRUSH_SMOOTH
    Bitcrusher crush;
#else
    BitcrusherProcessor crush;
#endif

#ifdef REDUCE_SMOOTH
    SmoothSampleRateReducer<> reducer;
#else
    SampleRateReducer<> reducer;
#endif
    DecimatorPatch() {
        registerParameter(PARAMETER_A, "Crush");
        registerParameter(PARAMETER_B, "Reduce");
    }

    void processAudio(AudioBuffer& buffer) override {
#ifdef CRUSH_SMOOTH
        crush.setCrush(getParameterValue(PARAMETER_A));
#else
        crush.setDepth(getParameterValue(PARAMETER_A) * 22);
#endif
        FloatArray left = buffer.getSamples(0);
        crush.process(left, left);

        reducer.setFactor(getParameterValue(PARAMETER_B));
        FloatArray right = buffer.getSamples(1);
        //for (int i = 0; i < buffer.getSize(); i++)
        //    right[i] = reducer.process(right[i]);
        //reducer.process(right, right);
    }
};

#endif
