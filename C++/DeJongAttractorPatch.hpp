#ifndef __DE_JONG_ATTRACTOR_PATCH_HPP__
#define __DE_JONG_ATTRACTOR_PATCH_HPP__

#include "Patch.h"
#include "DeJongAttractor.h"

class DeJongAttractorPatch : public Patch {
public:
    DeJongAttractor attr;
    bool isLocked;
    DeJongAttractorPatch()
        : isLocked(true) {
        attr.setCoefficients(-0.310, 1.278, 0.338, -2.924);
        setButton(BUTTON_A, true, 0);
        registerParameter(PARAMETER_A, "A");
        registerParameter(PARAMETER_B, "B");
        registerParameter(PARAMETER_C, "C");
        registerParameter(PARAMETER_D, "D");
    }
    ~DeJongAttractorPatch() {
    }

    void buttonChanged(PatchButtonId bid, uint16_t value, uint16_t samples) override {
        switch (bid) {
        case BUTTON_A:
            if (value)
                isLocked = !isLocked;
            setButton(BUTTON_A, isLocked, 0);
            break;
        case BUTTON_B:
            if (value)
                attr.setState(ComplexFloat(0.f, 0.f));
        default:
            break;
        }
    }

    void processAudio(AudioBuffer& buffer) {
        if (!isLocked) {
            attr.setCoefficients(getParameterValue(PARAMETER_A) * 10.f - 5.f,
                getParameterValue(PARAMETER_B) * 10.f - 5.f,
                getParameterValue(PARAMETER_C) * 10.f - 5.f,
                getParameterValue(PARAMETER_D) * 10.f - 5.f);
        }
        attr.generate(buffer);
        buffer.getSamples(0).multiply(0.5);
        buffer.getSamples(1).multiply(0.5);
    }
};

#endif
