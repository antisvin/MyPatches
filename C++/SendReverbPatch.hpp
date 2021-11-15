#include "DattorroReverb.hpp"
#include "Nonlinearity.hpp"
#include "Patch.h"
#include "SmoothValue.h"
#include "daisysp.h"

#define P_AMOUNT PARAMETER_A
#define P_DECAY PARAMETER_B
#define P_DIFFUSION PARAMETER_C
#define P_DAMP PARAMETER_D
//#define P_DUCK PARAMETER_E
#define P_COMP_RATIO PARAMETER_AA
#define P_COMP_THRESH PARAMETER_AB
#define P_COMP_ATTACK PARAMETER_AC
#define P_COMP_RELEASE PARAMETER_AD
#define P_COMP_MAKEUP PARAMETER_AE
#define P_PRE_DELAY PARAMETER_AF

#define MAX_PRE_DELAY 120 // In ms

using Saturator = AntialiasedReciprocalSaturator;
//using Saturator = AliasingTanhSaturator;
//using Saturator = AntialiasedThirdOrderPolynomial;
//using Saturator = AntialiasedCubicSaturator;
using RingsReverb = DattorroReverb<false>;

class SendReverbPatch : public Patch {
public:
    RingsReverb* reverb;
    SmoothFloat gain = SmoothFloat(0.98, 0);
    FloatArray comp_buffer;
    const size_t pre_delay_max;
    SmoothStiffInt pre_delay = SmoothStiffInt(0.98, 16);
    Saturator* saturator;
    daisysp::Compressor* compressor;
    bool automakeup = false;

    SendReverbPatch()
        : pre_delay_max(float(MAX_PRE_DELAY) / 1000 * getSampleRate()) {
        setParameterValue(P_AMOUNT, 0.75);
        registerParameter(P_DECAY, "Decay");
        setParameterValue(P_DECAY, 0.9);
        registerParameter(P_DIFFUSION, "Diffusion");
        setParameterValue(P_DIFFUSION, 0.7);
        registerParameter(P_DAMP, "Damping");
        setParameterValue(P_DAMP, 0.6);
        registerParameter(P_AMOUNT, "Amount");
        setParameterValue(P_AMOUNT, 0.5);
        //registerParameter(P_DUCK, "Duck");
        //setParameterValue(P_DUCK, 0);
        registerParameter(P_PRE_DELAY, "Pre-delay");
        setParameterValue(P_PRE_DELAY, 0.4);
        registerParameter(P_COMP_RATIO, "Comp ratio");
        setParameterValue(P_COMP_RATIO, 2.0 / 40);
        registerParameter(P_COMP_THRESH, "Comp thresh");
        setParameterValue(P_COMP_THRESH, -12.0 / -80);
        registerParameter(P_COMP_ATTACK, "Comp attack");
        setParameterValue(P_COMP_ATTACK, 0.1);
        registerParameter(P_COMP_RELEASE, "Comp release");
        setParameterValue(P_COMP_RELEASE, 0.1);
        registerParameter(P_COMP_MAKEUP, "Comp makeup");
        setParameterValue(P_COMP_MAKEUP, 0.0);
        comp_buffer = FloatArray::create(getBlockSize());
        reverb = RingsReverb::create(pre_delay_max + getBlockSize(),
            getBlockSize(), getSampleRate(), rings_delays);
        reverb->setModulation(4460, 40, 6261, 50);
        compressor = new daisysp::Compressor();
        compressor->Init(getSampleRate());
        compressor->AutoMakeup(false);
        saturator = Saturator::create();
    }
    ~SendReverbPatch() {
        RingsReverb::destroy(reverb);
        FloatArray::destroy(comp_buffer);
        delete compressor;
        Saturator::destroy(saturator);
    }
    void buttonChanged(PatchButtonId bid, uint16_t value, uint16_t samples) override {
        switch (bid) {
        case BUTTON_A:
            if (value)
                automakeup = !automakeup;
            setButton(BUTTON_A, automakeup, 0);
            compressor->AutoMakeup(automakeup);
            break;
        default:
            break;
        }
    }
    void processAudio(AudioBuffer& buffer) {
        FloatArray left = buffer.getSamples(0);
        FloatArray right = buffer.getSamples(1);

        compressor->SetRatio(1.0 + getParameterValue(P_COMP_RATIO) * 39);
        compressor->SetAttack(0.001 + getParameterValue(P_COMP_ATTACK) * 9.999);
        compressor->SetRelease(0.001 + getParameterValue(P_COMP_RELEASE) * 9.999);
        compressor->SetThreshold(getParameterValue(P_COMP_THRESH) * -80);
        if (!automakeup)
            compressor->SetMakeup(getParameterValue(P_COMP_MAKEUP) * 80);
        compressor->ProcessBlock(left.getData(), left.getData(), right.getData(), getBlockSize());

        saturator->process(left, left);

        //comp_buffer.copyFrom(right);
        right.copyFrom(left);
        float reverb_amount = getParameterValue(P_AMOUNT);
        reverb->setAmount(reverb_amount);
        reverb->setDecay(getParameterValue(P_DECAY));
        // reverb->setDecay(0.35 + reverb_amount * 0.63);
        reverb->setDiffusion(getParameterValue(P_DIFFUSION));
        reverb->setDamping(getParameterValue(P_DAMP) * 1.1);
        pre_delay = getParameterValue(P_PRE_DELAY) * pre_delay_max;
        reverb->setPreDelay(pre_delay);
        reverb->process(buffer, buffer);        

        /*
        for (size_t i = 0; i < getBlockSize(); i++) {
            compressor->Process(comp_buffer[i]);
            left[i] = compressor->Apply(left[i]);
            right[i] = compressor->Apply(left[i]);
        }*/

        //saturators[0]->process(left, left);
        //saturators[1]->process(right, right);

        //left.add(comp_buffer);
        //right.add(comp_buffer);
    }
};
