#ifndef __WAVETABLE_PATCH_HPP__
#define __WAVETABLE_PATCH_HPP__

#include "Patch.h"
#include "WaveTableOscillator.hpp"

using WT2D = WaveTableOscillator2D<256, 8, 8, HERMITE_INTERPOLATION>;

class WaveTablePatch : public Patch {
public:
    WT2D* wt;

    WaveTablePatch() {
        wt = WT2D::create("wavetable1.wav");
        wt->setSampleRate(getSampleRate());
        wt->setFrequency(60);

        registerParameter(PARAMETER_A, "Tune");
        registerParameter(PARAMETER_B, "Y");
        registerParameter(PARAMETER_C, "X");
    }
    ~WaveTablePatch() {
        WT2D::destroy(wt);
    }

    void processAudio(AudioBuffer& buffer) override {
        wt->setTable(size_t(getParameterValue(PARAMETER_C) * 8), size_t(getParameterValue(PARAMETER_B) * 8));
        FloatArray left = buffer.getSamples(0);
        wt->generate(left, left);
    }
};

#endif
