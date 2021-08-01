#ifndef __TRANSFORM_MATRIX_PATCH_HPP__
#define __TRANSFORM_MATRIX_PATCH_HPP__

#include "Patch.h"
#include "QuadratureSineOscillator.h"

#include "ComplexTransform.h"

class TransformMatrixPatch : public Patch {
public:
    using Transform = InterpolatedCompositeTransform<3>;
    FeedbackQuadratureSineOscillator* osc;
    AffineScale2D* scale;
    AffineShear2D* shear;
    AffineTranslation2D* translation;
    AffineRotation2D* rotation;
    Transform* composite;
    ComplexFloatArray transform_buf;

    TransformMatrixPatch() {
        osc = FeedbackQuadratureSineOscillator::create(getSampleRate());
        transform_buf = ComplexFloatArray::create(getBlockSize());
        scale = AffineScale2D::create();
        rotation = AffineRotation2D::create();
        translation = AffineTranslation2D::create();
        shear = AffineShear2D::create();
        composite = Transform::create(scale, translation, rotation, shear);
        osc->setFrequency(110.f);
        osc->setFeedback(0.5f);
        scale->scale(0.5);
        registerParameter(PARAMETER_A, "Shear");
        registerParameter(PARAMETER_B, "Rotation");
        registerParameter(PARAMETER_C, "Transation X");
        registerParameter(PARAMETER_D, "Translation Y");
    }
    ~TransformMatrixPatch() {
        FeedbackQuadratureSineOscillator::destroy(osc);
        ComplexFloatArray::destroy(transform_buf);
        AffineScale2D::destroy(scale);
        AffineRotation2D::destroy(rotation);
        AffineTranslation2D::destroy(translation);
        AffineShear2D::destroy(shear);
        Transform::destroy(composite);
    }

    void processAudio(AudioBuffer& buffer) {
        osc->generate(transform_buf);

        shear->shearX(getParameterValue(PARAMETER_A) * 2 - 1);
        rotation->rotate(M_PI * 2 * getParameterValue(PARAMETER_B));
        translation->translate(
            getParameterValue(PARAMETER_C) - 0.5f,
            getParameterValue(PARAMETER_D) - 0.5f
        );

        composite->process(transform_buf, transform_buf);

        transform_buf.copyTo(buffer);
    }
};

#endif
