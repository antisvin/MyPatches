#include "DattorroReverb.hpp"
#include "Patch.h"
#include "Nonlinearity.hpp"

#define P_AMOUNT    PARAMETER_A
#define P_DECAY     PARAMETER_B
#define P_DIFFUSION PARAMETER_C
#define P_DAMP      PARAMETER_D
#define P_GAIN      PARAMETER_AA

using Saturator = AntialiasedCubicSaturator;
//using CloudsReverb = DattorroReverb<true>;
using CloudsReverb = DattorroReverb<true, Saturator>;

class CloudsReverbPatch : public Patch {
public:
    CloudsReverb* reverb;
    SmoothFloat gain;

    CloudsReverbPatch() {
        registerParameter(P_AMOUNT, "Amount");
        setParameterValue(P_AMOUNT, 0.75);
        registerParameter(P_DECAY, "Decay");
        setParameterValue(P_DECAY, 0.7);
        registerParameter(P_DIFFUSION, "Diffusion");
        setParameterValue(P_DIFFUSION, 0.7);
        registerParameter(P_DAMP, "Damping");
        setParameterValue(P_DAMP, 0.7);
        registerParameter(P_GAIN, "Gain");
        setParameterValue(P_GAIN, 0.5);
        reverb = CloudsReverb::create(getSampleRate(), clouds_delays);
        reverb->setModulation(10, 60, 4680, 100);
    }
    ~CloudsReverbPatch() {
        CloudsReverb::destroy(reverb);
    }
    void processAudio(AudioBuffer& buffer) {
        FloatArray left = buffer.getSamples(0);
        FloatArray right = buffer.getSamples(1);
        gain = getParameterValue(P_GAIN) * 2;
        buffer.multiply(gain);
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
