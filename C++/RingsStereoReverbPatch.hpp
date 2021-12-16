#include "DattorroStereoReverb.hpp"
#include "Patch.h"
#include "Nonlinearity.hpp"
#include "SmoothValue.h"
#include "BypassProcessor.hpp"

#define P_DECAY PARAMETER_A
#define P_DIFFUSION PARAMETER_B
#define P_DAMP PARAMETER_C
#define P_AMOUNT PARAMETER_D
#define P_MOD PARAMETER_E
#define P_GAIN PARAMETER_AA

using Saturator = AntialiasedThirdOrderPolynomial;
using RingsReverb = DattorroStereoReverb<>;

class RingsStereoReverbPatch : public Patch {
public:
    RingsReverb* reverb;
    SmoothFloat gain = SmoothFloat(0.99);
    SmoothFloat reverb_amount = SmoothFloat(0.99);
    SmoothFloat reverb_diffusion = SmoothFloat(0.99);
    SmoothFloat reverb_decay = SmoothFloat(0.98);
    SmoothFloat reverb_damping = SmoothFloat(0.98);
    SmoothFloat ext_mod = SmoothFloat(0.98);
    bool max_reverb = false;
    Saturator* saturators[2];

    RingsStereoReverbPatch() {
        registerParameter(P_AMOUNT, "Mix");
        setParameterValue(P_AMOUNT, 0.75);
        registerParameter(P_DECAY, "Decay");
        setParameterValue(P_DECAY, 0.7);
        registerParameter(P_DIFFUSION, "Diffusion");
        setParameterValue(P_DIFFUSION, 0.7);
        registerParameter(P_DAMP, "Brightness");
        setParameterValue(P_DAMP, 0.7);
        registerParameter(P_MOD, "Exp");
        setParameterValue(P_MOD, 0.0);
        registerParameter(P_GAIN, "Gain");
        setParameterValue(P_GAIN, 1.0);
        reverb = RingsReverb::create(getBlockSize(), getSampleRate(), rings_delays);
        reverb->setModulation(4460, 40, 6261, 50);
        saturators[0] = Saturator::create();
        saturators[1] = Saturator::create();
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
                max_reverb = !max_reverb;
            setButton(BUTTON_A, max_reverb, 0);
            break;
        default:
            break;
        }
    }
    void processAudio(AudioBuffer& buffer) {
        // Gain
        gain = getParameterValue(P_GAIN) * 0.5;
        buffer.multiply(gain);
        // Global external modulation - boost decay/damping
        ext_mod = getParameterValue(P_MOD);
        reverb_amount = getParameterValue(P_AMOUNT);
        reverb->setAmount(reverb_amount);
        float raw_decay = getParameterValue(P_DECAY);
        raw_decay += (0.998 - raw_decay) * ext_mod;
        reverb_decay = max_reverb ? 0.997 : raw_decay;
        reverb->setDecay(reverb_decay);
        // reverb->setDecay(0.35 + reverb_amount * 0.63);
        reverb_diffusion = getParameterValue(P_DIFFUSION);
        reverb->setDiffusion(reverb_diffusion);
        float raw_damping = getParameterValue(P_DAMP);
        raw_damping += (0.998 - raw_damping) * ext_mod;
        reverb_damping = max_reverb ? 0.997 : raw_damping;
        reverb->setDamping(reverb_damping);
        reverb->process(buffer, buffer);

        for (int i = 0; i < 2; i++) {
            FloatArray t = buffer.getSamples(i);
            saturators[i]->process(t, t);
        }
    }
};
