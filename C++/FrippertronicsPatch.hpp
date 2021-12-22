#include "daisysp.h"
#include "DattorroStereoReverb.hpp"
#include "Patch.h"
#include "Nonlinearity.hpp"
#include "SmoothValue.h"
//#include "DryWetProcessor.h"

#define P_AMOUNT PARAMETER_A
#define P_DIFFUSION PARAMETER_B
#define P_DAMP PARAMETER_C
#define P_MIX PARAMETER_D
#define P_MOD PARAMETER_E
#define P_GAIN PARAMETER_AA

#define MAX_BUF_SIZE (4 * 1024 * 1024 - 1024) // In bytes, per channel
#define DELAY_CLEAR 500 // In ms

using Saturator = AntialiasedThirdOrderPolynomial;
using CloudsReverb = DattorroStereoReverb<>;

const char* looper_modes[] = {
    "Normal",
    "Onetime",
    "Replace",
    "Fripp",
};

enum LooperState {
    ST_NONE,
    ST_RECORDING,
    ST_OVERDUB,
};

class LooperProcessor : public MultiSignalProcessor {
public:
    LooperProcessor(daisysp::Looper** loopers, float* buf1, float* buf2, size_t max_size)
        : mix(0)
        , loopers(loopers) {
        buf[0] = buf1;
        buf[1] = buf2;
        loopers[0]->Init(buf1, max_size);
        loopers[1]->Init(buf2, max_size);
        loopers[0]->SetMode(daisysp::Looper::Mode::FRIPPERTRONICS);
        loopers[1]->SetMode(daisysp::Looper::Mode::FRIPPERTRONICS);
    }
    void process(AudioBuffer& input, AudioBuffer& output) override {
        size_t size = output.getSize();
        for (size_t i = 0; i < 2; i++) {
            FloatArray in = input.getSamples(i);
            FloatArray out = output.getSamples(i);
            auto looper = loopers[i];
            for (size_t j = 0; j < size; j++) {
                float in_sample = in[j];
                float sample = looper->Process(in_sample);
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
        debugMessage(looper_modes[uint32_t(loopers[0]->GetMode())]);
    }
    void toggleReverse() {
        loopers[0]->ToggleReverse();
        loopers[1]->ToggleReverse();
    }
    void toggleHalfSpeed() {
        loopers[0]->ToggleHalfSpeed();
        loopers[1]->ToggleHalfSpeed();
    }
    void clear() {
        loopers[0]->Clear();
        loopers[1]->Clear();
    }
    static LooperProcessor* create(size_t max_size) {
        max_size /= sizeof(float);
        auto loopers = new daisysp::Looper*[2];
        loopers[0] = new daisysp::Looper();
        loopers[1] = new daisysp::Looper();
        return new LooperProcessor(
            loopers, new float[max_size], new float[max_size], max_size);
    }
    static void destroy(LooperProcessor* processor) {
        for (int i = 0; i < 2; i++) {
            delete[] processor->buf[i];
        }
        delete[] processor->loopers;
        delete processor;
    }

private:
    daisysp::Looper** loopers;
    float* buf[2];
    float mix;
};

class FrippertronicsPatch : public Patch {
public:
    CloudsReverb* reverb;
    SmoothFloat gain;
    Saturator* saturators[2];
    LooperState state;
    LooperProcessor* looper;

    SmoothFloat reverb_amount = SmoothFloat(0.99);
    SmoothFloat reverb_diffusion = SmoothFloat(0.99);
    SmoothFloat reverb_damping = SmoothFloat(0.98);
    SmoothFloat ext_mod = SmoothFloat(0.98);

    bool is_record = false;
    bool is_half_speed = false;
    bool is_reverse = true;
    uint32_t rec_timer = 0;
    uint32_t delay_click;

    FrippertronicsPatch() {
        registerParameter(P_MIX, "Mix");
        setParameterValue(P_MIX, 0.5);
        registerParameter(P_AMOUNT, "Amount");
        setParameterValue(P_AMOUNT, 0.75);
        // registerParameter(P_DECAY, "Decay");
        // setParameterValue(P_DECAY, 0.7);
        registerParameter(P_DIFFUSION, "Diffusion");
        setParameterValue(P_DIFFUSION, 0.7);
        registerParameter(P_DAMP, "Damping");
        setParameterValue(P_DAMP, 0.7);
        registerParameter(P_MOD, "Exp");
        setParameterValue(P_MOD, 0.0);
        registerParameter(P_GAIN, "Gain");
        setParameterValue(P_GAIN, 1.0);
        reverb = CloudsReverb::create(getBlockSize(), getSampleRate(), rings_delays);
        reverb->setModulation(4460, 40, 6261, 50);
        saturators[0] = Saturator::create();
        saturators[1] = Saturator::create();
        looper = LooperProcessor::create(MAX_BUF_SIZE);
        state = ST_NONE;
        delay_click = getBlockRate() / 1000 * DELAY_CLEAR;
        debugMessage("CL", (int)delay_click);
    }
    ~FrippertronicsPatch() {
        LooperProcessor::destroy(looper);
        CloudsReverb::destroy(reverb);
        Saturator::destroy(saturators[0]);
        Saturator::destroy(saturators[1]);
    }
    void buttonChanged(PatchButtonId bid, uint16_t value, uint16_t samples) override {
        switch (bid) {
        case BUTTON_A:
            if (value) {
                if (rec_timer < delay_click) {
                    looper->clear();
                    is_record = false;
                    state = ST_NONE;
                    debugMessage("CLEAR");
                }
                else {
                    looper->trigRecord();
                    is_record = !is_record;
                    debugMessage("Rec", (int)is_record);
                }
                rec_timer = 0;
            }
            break;
        case BUTTON_B:
            if (value) {
                is_reverse = !is_reverse;
                setButton(BUTTON_B, is_reverse, 0);
                looper->toggleReverse();
                debugMessage("Rev", (int)is_reverse);
            }
            break;
        case BUTTON_C:
            if (value) {
                is_half_speed = !is_half_speed;
                setButton(BUTTON_B, is_half_speed, 0);
                looper->toggleHalfSpeed();
                debugMessage("Half", (int)is_half_speed);
            }
            break;
        case BUTTON_D:
            if (value) {
                looper->incMode();
                debugMessage("Mode");
            }
            break;
        default:
            break;
        }
    }
    void processAudio(AudioBuffer& buffer) {
        if (rec_timer < 0xffff)
            rec_timer++;

        gain = getParameterValue(P_GAIN) * 0.5;
        buffer.multiply(gain);

        looper->setMix(getParameterValue(P_MIX));
        looper->process(buffer, buffer);

        ext_mod = getParameterValue(P_MOD);
        float raw_amount = getParameterValue(P_AMOUNT);
        reverb_amount = raw_amount + (0.998 - raw_amount) * ext_mod;
        reverb_amount = getParameterValue(P_AMOUNT);
        reverb->setAmount(reverb_amount);
        reverb->setDecay(0.35 + reverb_amount * 0.63);
        reverb_diffusion = getParameterValue(P_DIFFUSION);
        reverb->setDiffusion(reverb_diffusion);
        float raw_damping = getParameterValue(P_DAMP);
        reverb_damping = raw_damping + (0.998 - raw_damping) * ext_mod;
        reverb->setDamping(reverb_damping);
        reverb->process(buffer, buffer);

        for (int i = 0; i < 2; i++) {
            FloatArray t = buffer.getSamples(i);
            saturators[i]->process(t, t);
        }
    }
};
