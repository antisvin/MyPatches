#include "daisysp.h"
#include "DattorroStereoReverb.hpp"
#include "Patch.h"
#include "Nonlinearity.hpp"
#include "SmoothValue.h"
#include "BypassProcessor.hpp"
#include "DryWetProcessor.h"

#define P_MIX PARAMETER_A
#define P_AMOUNT PARAMETER_B
#define P_DECAY PARAMETER_C
#define P_DIFFUSION PARAMETER_D
#define P_DAMP PARAMETER_E
#define P_GAIN PARAMETER_F
#define P_SHIFT PARAMETER_G
#define P_SHIFT_AMT PARAMETER_H

#define MAX_BUF_SIZE (4 * (1024 - 33) * 1024) // In bytes, per channel

using Saturator = AntialiasedThirdOrderPolynomial;
using CloudsReverb = DattorroStereoReverb<>;

const char* looper_modes[] = {
    "Normal",
    "Onetime",
    "Replace",
    "Fripp",
};

class LooperProcessor : public MultiSignalProcessor {
public:
    LooperProcessor(daisysp::Looper** loopers, daisysp::PitchShifter** shifters,
        float* buf1, float* buf2, size_t max_size)
        : mix(0)
        , loopers(loopers)
        , shifters(shifters) {
        buf[0] = buf1;
        buf[1] = buf2;
        loopers[0]->Init(buf1, max_size);
        loopers[1]->Init(buf2, max_size);
    }
    void process(AudioBuffer& input, AudioBuffer& output) override {
        debugMessage(looper_modes[uint32_t(loopers[0]->GetMode())], int(shift));
        size_t size = output.getSize();
        for (size_t i = 0; i < 2; i++) {
            FloatArray in = input.getSamples(i);
            FloatArray out = output.getSamples(i);
            auto looper = loopers[i];
            auto shifter = shifters[i];
            for (size_t j = 0; j < size; j++) {
                float in_sample = in[j];
                float sample = looper->Process(in_sample);
                sample += (sample - shifter->Process(sample)) * shift_amount;
                out[j] = in_sample + (sample - in_sample) * mix;
            }
        }
    }
    void setMix(float mix) {
        this->mix = mix;
    }
    void trigRecord() {
        loopers[0]->TrigRecord();
        loopers[1]->TrigRecord();
    }
    void incMode() {
        loopers[0]->IncrementMode();
        loopers[1]->IncrementMode();
    }
    void toggleReverse() {
        loopers[0]->ToggleReverse();
        loopers[1]->ToggleReverse();
    }
    void toggleHalfSpeed() {
        loopers[0]->ToggleHalfSpeed();
        loopers[1]->ToggleHalfSpeed();
    }
    void setPitchShift(float amount, size_t semitones) {
        shift = semitones;
        shift_amount = amount;
        shifters[0]->SetTransposition(semitones);
        shifters[1]->SetTransposition(semitones);
    }
    static LooperProcessor* create(float sr, size_t max_size) {
        max_size /= sizeof(float);
        auto loopers = new daisysp::Looper*[2];
        loopers[0] = new daisysp::Looper();
        loopers[1] = new daisysp::Looper();
        auto shifters = new daisysp::PitchShifter*[2];
        shifters[0] = new daisysp::PitchShifter();
        shifters[0]->Init(sr);
        shifters[0]->SetDelSize(2400);
        shifters[0]->SetFun(0.1);
        shifters[1] = new daisysp::PitchShifter();
        shifters[1]->Init(sr);
        shifters[1]->SetDelSize(2400);
        shifters[1]->SetFun(0.1);
        return new LooperProcessor(loopers, shifters, new float[max_size],
            new float[max_size], max_size);
    }
    static void destroy(LooperProcessor* processor) {
        for (int i = 0; i < 2; i++) {
            delete[] processor->buf[i];
            delete processor->loopers[i];
            delete processor->shifters[i];
        }
        delete[] processor->loopers;
        delete[] processor->shifters;
        delete processor;
    }

private:
    daisysp::Looper** loopers;
    daisysp::PitchShifter** shifters;
    float* buf[2];
    float mix;
    size_t shift;
    float shift_amount;
};

class LoopShiftPatch : public Patch {
public:
    CloudsReverb* reverb;
    SmoothFloat gain;
    bool bypassed = false;
    Saturator* saturators[2];
    LooperProcessor* looper;

    bool is_record = false;
    bool is_half_speed = false;
    bool is_reverse = false;

    LoopShiftPatch() {
        registerParameter(P_MIX, "Mix");
        setParameterValue(P_MIX, 0.5);
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
        registerParameter(P_SHIFT, "Pitch shift");
        setParameterValue(P_SHIFT, 0.5);
        registerParameter(P_SHIFT_AMT, "Shift amount");
        setParameterValue(P_SHIFT_AMT, 0.5);
        reverb = CloudsReverb::create(getBlockSize(), getSampleRate(), rings_delays);
        reverb->setModulation(4460, 40, 6261, 50);
        saturators[0] = Saturator::create();
        saturators[1] = Saturator::create();
        looper = LooperProcessor::create(getSampleRate(), MAX_BUF_SIZE);
    }
    ~LoopShiftPatch() {
        LooperProcessor::destroy(looper);
        CloudsReverb::destroy(reverb);
        Saturator::destroy(saturators[0]);
        Saturator::destroy(saturators[1]);
    }
    void buttonChanged(PatchButtonId bid, uint16_t value, uint16_t samples) override {
        switch (bid) {
        case BUTTON_A:
            if (value)
                is_record = !is_record;
            setButton(BUTTON_A, is_record, 0);
            looper->trigRecord();
            break;
        case BUTTON_B:
            if (value) {
                looper->incMode();
            }
            break;
        case BUTTON_C:
            if (value) {
                is_reverse = !is_reverse;
            }
            setButton(BUTTON_C, is_reverse, 0);
            looper->toggleReverse();
            break;
        case BUTTON_D:
            if (value) {
                is_half_speed = !is_half_speed;
            }
            setButton(BUTTON_D, is_half_speed, 0);
            looper->toggleHalfSpeed();
            break;
        default:
            break;
        }
    }
    void processAudio(AudioBuffer& buffer) {
        gain = getParameterValue(P_GAIN) * 0.5;
        buffer.multiply(gain);

        looper->setMix(getParameterValue(P_MIX));
        looper->setPitchShift(getParameterValue(P_SHIFT_AMT),
            size_t(getParameterValue(P_SHIFT) * 25) - 12);
        looper->process(buffer, buffer);

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
