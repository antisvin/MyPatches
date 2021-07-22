#ifndef __TRANSFORM_PATCH_HPP__
#define __TRANSFORM_PATCH_HPP__

#include "Patch.h"
#include "QuadratureSineOscillator.h"
#include "ComplexTransform.h"


class TransformPatch : public Patch {
public:
    FeedbackQuadratureSineOscillator* osc;
    Scale2D scale;
    Rotation2D rotation;
    Reflection2D reflection;
    ShearX2D shear;
    ComplexFloatArray transform_buf;

    TransformPatch() {
        osc = FeedbackQuadratureSineOscillator::create(getSampleRate());
        osc->setFrequency(110.f);
        scale.setFactor(0.5);
        transform_buf = ComplexFloatArray::create(getBlockSize());
        registerParameter(PARAMETER_A, "Reflect");
        registerParameter(PARAMETER_B, "Shear");
        registerParameter(PARAMETER_C, "Rotate");
        registerParameter(PARAMETER_D, "Feedback");
    }
    ~TransformPatch() {
        FeedbackQuadratureSineOscillator::destroy(osc);
        ComplexFloatArray::destroy(transform_buf);
    }

    void processAudio(AudioBuffer& buffer) {
        osc->setFeedback(getParameterValue(PARAMETER_D));
        osc->generate(transform_buf);

        scale.process(transform_buf, transform_buf);

        shear.setFactor(getParameterValue(PARAMETER_B));
        shear.process(transform_buf, transform_buf);

        reflection.setAngle(M_PI * 2 * getParameterValue(PARAMETER_A));        
        reflection.process(transform_buf, transform_buf);

        rotation.setAngle(M_PI * 2 * getParameterValue(PARAMETER_C));
        rotation.process(transform_buf, transform_buf);

        transform_buf.copyTo(buffer);
    }
};

#endif
