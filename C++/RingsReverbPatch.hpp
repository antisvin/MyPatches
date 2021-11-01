#include "DattorroReverb.hpp"
#include "Patch.h"

#define P_AMOUNT    PARAMETER_A
#define P_TIME      PARAMETER_B
#define P_DIFFUSION PARAMETER_C
#define P_LPF       PARAMETER_D
#define P_GAIN      PARAMETER_AA

using RingsReverb = DattorroReverb<true>;

class RingsReverbPatch : public Patch {
public:
    RingsReverb* reverb;
    RingsReverbPatch() {
        registerParameter(P_AMOUNT, "Amount");
        setParameterValue(P_AMOUNT, 0.75);
        registerParameter(P_TIME, "Time");
        setParameterValue(P_TIME, 0.7);
        registerParameter(P_DIFFUSION, "Diffusion");
        setParameterValue(P_DIFFUSION, 0.7);
        registerParameter(P_LPF, "LPF");
        setParameterValue(P_LPF, 0.7);
        registerParameter(P_GAIN, "Gain");
        setParameterValue(P_GAIN, 0.5);
        reverb = RingsReverb::create(getSampleRate(), rings_delays);
    }
    ~RingsReverbPatch() {
        RingsReverb::destroy(reverb);
    }
    void processAudio(AudioBuffer& buffer) {
        buffer.getSamples(0).add(buffer.getSamples(1));
        buffer.getSamples(0).multiply(getParameterValue(P_GAIN));

        float reverb_amount = getParameterValue(P_AMOUNT);
        reverb->setAmount(reverb_amount);
        reverb->setTime(0.35f + 0.63f * reverb_amount);
        reverb->setTime(getParameterValue(P_TIME));
        reverb->setDiffusion(getParameterValue(P_DIFFUSION));
        reverb->setLp(getParameterValue(P_LPF));
        reverb->process(buffer, buffer);
    }
};
