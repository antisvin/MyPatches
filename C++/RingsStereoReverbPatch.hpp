#include "DattorroStereoReverb.hpp"
#include "Patch.h"
#include "Nonlinearity.hpp"
#include "SmoothValue.h"
#include "BypassProcessor.hpp"

#define P_AMOUNT PARAMETER_A
#define P_DECAY PARAMETER_B
#define P_DIFFUSION PARAMETER_C
#define P_DAMP PARAMETER_D
#define P_GAIN PARAMETER_AA

using Saturator = BypassProcessor<AntialiasedThirdOrderPolynomial>;
using RingsReverb = DattorroStereoReverb<>;

class RingsStereoReverbPatch : public Patch {
public:
    RingsReverb* reverb;
    SmoothFloat gain;
    bool bypassed = false;
    Saturator* saturators[2];

    RingsStereoReverbPatch() {
        registerParameter(P_AMOUNT, "Amount");
        setParameterValue(P_AMOUNT, 0.75);
        registerParameter(P_DECAY, "Decay");
        setParameterValue(P_DECAY, 0.7);
        registerParameter(P_DIFFUSION, "Diffusion");
        setParameterValue(P_DIFFUSION, 0.7);
        registerParameter(P_DAMP, "Damping");
        setParameterValue(P_DAMP, 0.7);
        registerParameter(P_GAIN, "Gain");
        setParameterValue(P_GAIN, 1.0);
        reverb = RingsReverb::create(getBlockSize(), getSampleRate(), rings_delays);
        reverb->setModulation(4460, 40, 6261, 50);
        saturators[0] = Saturator::create();
        saturators[0]->setBypass(bypassed);
        saturators[1] = Saturator::create();
        saturators[1]->setBypass(bypassed);
    }
    ~RingsStereoReverbPatch() {
        RingsReverb::destroy(reverb);
        Saturator::destroy(saturators[0]);
        Saturator::destroy(saturators[1]);
    }
    void buttonChanged(PatchButtonId bid, uint16_t value, uint16_t samples) override {
        switch (bid) {
        case BUTTON_A:
            if (value)
                bypassed = !bypassed;
            debugMessage("BYPASS", int(bypassed));
            setButton(BUTTON_A, bypassed, 0);
            saturators[0]->setBypass(bypassed);
            saturators[1]->setBypass(bypassed);
            break;
        default:
            break;
        }
    }
    void processAudio(AudioBuffer& buffer) {
        gain = getParameterValue(P_GAIN) * 0.5;
        buffer.multiply(gain);
        float reverb_amount = getParameterValue(P_AMOUNT);
        reverb->setAmount(reverb_amount);
        reverb->setDecay(getParameterValue(P_DECAY));
        // reverb->setDecay(0.35 + reverb_amount * 0.63);
        reverb->setDiffusion(getParameterValue(P_DIFFUSION));
        reverb->setDamping(getParameterValue(P_DAMP));
        reverb->process(buffer, buffer);

        for (int i = 0; i < 2; i++) {
            FloatArray t = buffer.getSamples(i);
            saturators[i]->process(t, t);
        }
    }
};
