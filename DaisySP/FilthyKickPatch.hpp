#ifndef __FILTHY_KICK_PATCH_HPP__
#define __FILTHY_KICK_PATCH_HPP__

#include "Patch.h"
#include "daisysp.h"        // Import DaisySP header

using namespace daisysp;    // and use its namespace

class FilthyKickPatch : public Patch {
public:
    FilthyKickPatch(){
        drum.Init(getSampleRate());
        decimator.Init();

        // Drum
        registerParameter(PARAMETER_A, "Frequency"); // Combined based frequency + tone control
        setParameterValue(PARAMETER_A, 0.2);
        registerParameter(PARAMETER_B, "Filth"); // Mix clean and crushed drum
        setParameterValue(PARAMETER_B, 0.2);

        // Decimator
        registerParameter(PARAMETER_C, "Crush");
        setParameterValue(PARAMETER_C, 0.2);
        registerParameter(PARAMETER_D, "Reduce");
        setParameterValue(PARAMETER_D, 0.2);

        // Various params for drum
        registerParameter(PARAMETER_E, "Decay");
        setParameterValue(PARAMETER_E, 0.7);
        registerParameter(PARAMETER_F, "AttackFm");
        setParameterValue(PARAMETER_F, 0.1);
        registerParameter(PARAMETER_G, "SelfFm");
        setParameterValue(PARAMETER_G, 0.1);
        registerParameter(PARAMETER_H, "Accent");
        setParameterValue(PARAMETER_H, 0.8);
        registerParameter(PARAMETER_AA, "Dirtiness"); // Unrelated to filth!
        setParameterValue(PARAMETER_AA, 0.3);

        // For Magus - until we have registerButton() support ;-)
        registerParameter(PARAMETER_AB, "Trig");
    };

    ~FilthyKickPatch(){
    }

    void processAudio(AudioBuffer& buffer) override {
        // Fake trigger input in case if no button is available on device
        if (getParameterValue(PARAMETER_AB) > 0.1) {
            if (!is_high){
                drum.Trig();
                is_high = true;
            }
        }
        else {
            is_high = false;
        }

        // First, update all parameters
        float tone = getParameterValue(PARAMETER_A);
        drum.SetFreq(40.0f * fast_exp2f(tone));
        drum.SetTone(0.2 + tone * 0.5);
        drum.SetDecay(getParameterValue(PARAMETER_E));
        drum.SetDirtiness(getParameterValue(PARAMETER_AA));
        drum.SetFmEnvelopeAmount(PARAMETER_F);
        drum.SetFmEnvelopeDecay(PARAMETER_G);
        decimator.SetBitcrushFactor(getParameterValue(PARAMETER_C));
        decimator.SetDownsampleFactor(getParameterValue(PARAMETER_D));
        drum.SetAccent(getParameterValue(PARAMETER_H));
        
        FloatArray left = buffer.getSamples(LEFT_CHANNEL);
        float mix = getParameterValue(PARAMETER_B);

        // Second, process samples
        for (int i = 0; i < buffer.getSize(); i++){
            float sample = drum.Process();
            left[i] = sample + (decimator.Process(sample) - sample) * mix;
        }
        buffer.getSamples(RIGHT_CHANNEL).copyFrom(left);
        //That's it, enjoy the FILTH
    }

    void buttonChanged(PatchButtonId bid, uint16_t value, uint16_t samples) override {
        // Callback for devices with hardware button
        if (bid == PUSHBUTTON && value) {
            drum.Trig();
        }
    }

private:
    bool is_high;
    SyntheticBassDrum drum;
    Decimator decimator;
};

#endif
