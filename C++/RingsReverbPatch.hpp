#include "DattorroReverb.hpp"
#include "Nonlinearity.hpp"
#include "Patch.h"
#include "SmoothValue.h"

#define P_AMOUNT PARAMETER_A
#define P_DECAY PARAMETER_B
#define P_DIFFUSION PARAMETER_C
#define P_DAMP PARAMETER_D
#define P_GAIN PARAMETER_AA
#define P_PRE_DELAY PARAMETER_AB

#define MAX_PRE_DELAY 120 // In ms

using Saturator = AntialiasedThirdOrderPolynomial;
using RingsReverb = DattorroReverb<false>;

class RingsReverbPatch : public Patch {
public:
    RingsReverb* reverb;
    SmoothFloat gain = SmoothFloat(0.98, 0);
    FloatArray tmp;
    const size_t pre_delay_max;
    SmoothStiffInt pre_delay = SmoothStiffInt(0.98, 16);
    Saturator* saturators[2];

    RingsReverbPatch()
        : pre_delay_max(float(MAX_PRE_DELAY) / 1000 * getSampleRate()) {
        setParameterValue(P_AMOUNT, 0.75);
        registerParameter(P_DECAY, "Decay");
        setParameterValue(P_DECAY, 0.9);
        registerParameter(P_DIFFUSION, "Diffusion");
        setParameterValue(P_DIFFUSION, 0.7);
        registerParameter(P_DAMP, "Damping");
        setParameterValue(P_DAMP, 0.6);
        registerParameter(P_GAIN, "Gain");
        setParameterValue(P_GAIN, 0.5);
        registerParameter(P_AMOUNT, "Amount");
        setParameterValue(P_AMOUNT, 0.5);
        registerParameter(P_PRE_DELAY, "Pre-delay");
        setParameterValue(P_PRE_DELAY, 0.4);
        reverb = RingsReverb::create(
            pre_delay_max + getBlockSize(), getBlockSize(), getSampleRate(), rings_delays);
        reverb->setModulation(4460, 40, 6261, 50);
        tmp = FloatArray::create(getBlockSize());
        saturators[0] = Saturator::create();
        saturators[1] = Saturator::create();
    }
    ~RingsReverbPatch() {
        RingsReverb::destroy(reverb);
        Saturator::destroy(saturators[0]);
        Saturator::destroy(saturators[1]);
    }
    #if 0
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
    #endif
    void processAudio(AudioBuffer& buffer) {
        FloatArray left = buffer.getSamples(0);
        FloatArray right = buffer.getSamples(1);
        gain = getParameterValue(P_GAIN) * 2;
        buffer.multiply(gain);
        float reverb_amount = getParameterValue(P_AMOUNT);
        reverb->setAmount(reverb_amount);
        reverb->setDecay(getParameterValue(P_DECAY));
        // reverb->setDecay(0.35 + reverb_amount * 0.63);
        reverb->setDiffusion(getParameterValue(P_DIFFUSION));
        reverb->setDamping(getParameterValue(P_DAMP) * 1.1);
        pre_delay = getParameterValue(P_PRE_DELAY) * pre_delay_max;
        reverb->setPreDelay(pre_delay);
        reverb->process(buffer, buffer);
        saturators[0]->process(left, left);
        saturators[1]->process(right, right);
    }
};
