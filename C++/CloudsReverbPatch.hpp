#include "DattorroReverb.hpp"
#include "Patch.h"

#define P_AMOUNT    PARAMETER_A
#define P_DECAY     PARAMETER_B
#define P_DIFFUSION PARAMETER_C
#define P_LPF       PARAMETER_D
#define P_GAIN      PARAMETER_AA

using CloudsReverb = DattorroReverb<true>;

class CloudsReverbPatch : public Patch {
public:
    CloudsReverb* reverb;
    CloudsReverbPatch() {
        registerParameter(P_AMOUNT, "Amount");
        setParameterValue(P_AMOUNT, 0.75);
        registerParameter(P_DECAY, "Decay");
        setParameterValue(P_DECAY, 0.7);
        registerParameter(P_DIFFUSION, "Diffusion");
        setParameterValue(P_DIFFUSION, 0.7);
        registerParameter(P_LPF, "Damping");
        setParameterValue(P_LPF, 0.7);
        registerParameter(P_GAIN, "Gain");
        setParameterValue(P_GAIN, 0.5);
        reverb = CloudsReverb::create(getSampleRate(), clouds_delays);
        reverb->setModulation(10, 60, 4680, 100);
    }
    ~CloudsReverbPatch() {
        CloudsReverb::destroy(reverb);
    }
    void processAudio(AudioBuffer& buffer) {
        buffer.getSamples(0).add(buffer.getSamples(1));
        buffer.getSamples(0).multiply(getParameterValue(P_GAIN));

        float reverb_amount = getParameterValue(P_AMOUNT);
        reverb->setAmount(reverb_amount);
        reverb->setDecay(getParameterValue(P_DECAY));
        reverb->setDiffusion(getParameterValue(P_DIFFUSION));
        reverb->setDamping(getParameterValue(P_LPF));
        reverb->process(buffer, buffer);
    }
};
