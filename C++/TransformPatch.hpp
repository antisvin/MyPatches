#ifndef __TRANSFORM_PATCH_HPP__
#define __TRANSFORM_PATCH_HPP__

#include "Patch.h"
#include "QuadratureSineOscillator.h"
#include "ComplexTransform.h"

/**
 * Static allocation with matrix in 3x3 float buffer - 9%
 * Same, with dynamic allocation - 13%
 * FloatMatrix - also 13%
 * 
 * Composite - 8%
 */

class TransformPatch : public Patch {
public:
    FeedbackQuadratureSineOscillator* osc;
    AffineScale2D scale;
    AffineRotation2D rotation;
    AffineReflection2D reflection;
    AffineShearX2D shear;
    ComplexFloatArray transform_buf;
    using Transform = CompositeTransform<3, AffineScale2D, AffineShearX2D, AffineReflection2D, AffineRotation2D>;
    Transform* composite;

    TransformPatch() {
        osc = FeedbackQuadratureSineOscillator::create(getSampleRate());
        transform_buf = ComplexFloatArray::create(getBlockSize());
        //scale = Scale2D::create();
        //rotation = Rotation2D::create();
        //reflection = Reflection2D::create();
        //shear = ShearX2D::create();        
        composite = Transform::create(&scale, &shear, &reflection, &rotation);
        osc->setFrequency(110.f);
        scale.setFactor(0.5);
        registerParameter(PARAMETER_A, "Reflect");
        registerParameter(PARAMETER_B, "Shear");
        registerParameter(PARAMETER_C, "Rotate");
        registerParameter(PARAMETER_D, "Feedback");
    }
    ~TransformPatch() {
        FeedbackQuadratureSineOscillator::destroy(osc);
        ComplexFloatArray::destroy(transform_buf);
        Transform::destroy(composite);

        //Scale2D::destroy(scale);
        //Rotation2D::destroy(rotation);
        //Reflection2D::destroy(reflection);
        //ShearX2D::destroy(shear);
    }

    void processAudio(AudioBuffer& buffer) {
        osc->setFeedback(getParameterValue(PARAMETER_D));
        osc->generate(transform_buf);

        shear.setFactor(getParameterValue(PARAMETER_B));
        reflection.setAngle(M_PI * 2 * getParameterValue(PARAMETER_A));        
        rotation.setAngle(M_PI * 2 * getParameterValue(PARAMETER_C));

        //scale.process(transform_buf, transform_buf);
        //shear.process(transform_buf, transform_buf);
        //reflection.process(transform_buf, transform_buf);
        //rotation.process(transform_buf, transform_buf);

        composite->process(transform_buf, transform_buf);

        transform_buf.copyTo(buffer);
    }
};

#endif
