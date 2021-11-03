#include "DattorroReverb.hpp"
#include "Nonlinearity.hpp"
#include "Patch.h"
#include "SmoothValue.h"

#define P_AMOUNT    PARAMETER_A
#define P_DECAY     PARAMETER_B
#define P_DIFFUSION PARAMETER_C
#define P_DAMP      PARAMETER_D
#define P_GAIN      PARAMETER_AA

using Saturator = AntialiasedCubicSaturator;
using RingsReverb = DattorroReverb<false, Saturator>;

class RingsReverbPatch : public Patch {
public:
    RingsReverb* reverb;
    AudioBuffer* tmp;
    Saturator saturators[2];
    SmoothFloat gain;
    
    RingsReverbPatch() {
        setParameterValue(P_AMOUNT, 0.75);
        registerParameter(P_DECAY, "Decay");
        setParameterValue(P_DECAY, 0.7);
        registerParameter(P_DIFFUSION, "Diffusion");
        setParameterValue(P_DIFFUSION, 0.7);
        registerParameter(P_DAMP, "Damping");
        setParameterValue(P_DAMP, 0.7);
        registerParameter(P_GAIN, "Gain");
        setParameterValue(P_GAIN, 0.5);
        registerParameter(P_AMOUNT, "Amount");
        reverb = RingsReverb::create(getSampleRate(), rings_delays);
        reverb->setModulation(4460, 40, 6261, 50);
    }
    ~RingsReverbPatch() {
        RingsReverb::destroy(reverb);
    }
    void processAudio(AudioBuffer& buffer) {     
        FloatArray left = buffer.getSamples(0);
        FloatArray right = buffer.getSamples(1);
        gain = getParameterValue(P_GAIN) * 2;
        buffer.multiply(gain);
        saturators[0].process(left, left);
        saturators[1].process(right, right);
        float reverb_amount = getParameterValue(P_AMOUNT);
        reverb->setAmount(reverb_amount);
        reverb->setDecay(getParameterValue(P_DECAY));
        //reverb->setDecay(0.35 + reverb_amount * 0.63);
        reverb->setDiffusion(getParameterValue(P_DIFFUSION));
        reverb->setDamping(getParameterValue(P_DAMP));
        reverb->process(buffer, buffer);
        buffer.multiply(0.75); // Leav some headroom for Jesus
    }
};
