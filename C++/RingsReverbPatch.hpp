#include "DattorroReverb.hpp"
#include "Nonlinearity.hpp"
#include "Patch.h"
#include "SmoothValue.h"
#include "BypassProcessor.hpp"

#define P_AMOUNT    PARAMETER_A
#define P_DECAY     PARAMETER_B
#define P_DIFFUSION PARAMETER_C
#define P_DAMP      PARAMETER_D
#define P_GAIN      PARAMETER_AA

using Saturator = BypassProcessor<AntialiasedThirdOrderPolynomial>;
using RingsReverb = DattorroReverb<false, Saturator>;

class RingsReverbPatch : public Patch {
public:
    RingsReverb* reverb;
    AudioBuffer* tmp;
    SmoothFloat gain;
    bool bypassed = true;
    
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
        reverb->getProcessor(0).setBypass(bypassed);
        reverb->getProcessor(1).setBypass(bypassed);
    }
    ~RingsReverbPatch() {
        RingsReverb::destroy(reverb);
    }

    void buttonChanged(PatchButtonId bid, uint16_t value, uint16_t samples) override {
        switch (bid) {
        case BUTTON_A:
            if (value)
                bypassed = !bypassed;
            setButton(BUTTON_A, bypassed, 0);
            reverb->getProcessor(0).setBypass(bypassed);
            reverb->getProcessor(1).setBypass(bypassed);
            break;
        default:
            break;
        }
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
    }
};
