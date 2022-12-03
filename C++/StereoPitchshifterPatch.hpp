#include "OpenWareLibrary.h"
#include "daisysp.h"

#define P_MIX PARAMETER_A
#define P_TRANSPOSE PARAMETER_B
#define P_MOD PARAMETER_C
#define DELAY_MS 30 // Shift can be 30-100 ms lets just start with 50 for now.
#define SEMITONES 12

class PitchShifterProcessor : public SignalProcessor {
public:
    PitchShifterProcessor() = default;
    PitchShifterProcessor(daisysp::PitchShifter* pitchshifter, DcBlockingFilter* dc)
        : pitchshifter(pitchshifter)
        , dc(dc) {
    }
    using SignalProcessor::process;
    float process(float input) override {
        float t = dc->process(input);
        t = pitchshifter->Process(t);
        return t;
    }
    daisysp::PitchShifter* getPitchShifter() {
        return pitchshifter;
    }
    static PitchShifterProcessor* create(float sr) {        
        auto pitchshifter = new daisysp::PitchShifter();
        pitchshifter->Init(sr);
        pitchshifter->SetDelSize(sr * DELAY_MS / 1000);
        return new PitchShifterProcessor(pitchshifter, DcBlockingFilter::create());
    }
    static void destroy(PitchShifterProcessor* processor) {
        delete processor->pitchshifter;
        DcBlockingFilter::destroy(processor->dc);
        delete processor;
    }

private:
    daisysp::PitchShifter* pitchshifter;
    DcBlockingFilter* dc;
};


class StereoPitchshifterPatch : public Patch { 
public:
    PitchShifterProcessor* pitchshifters[2];
    FloatArray tmp;
    StereoPitchshifterPatch() {
        registerParameter(P_MIX, "Mix");
        registerParameter(P_TRANSPOSE, "Semitones");
        registerParameter(P_MOD, "Modulation");
        pitchshifters[0] = PitchShifterProcessor::create(getSampleRate());
        pitchshifters[1] = PitchShifterProcessor::create(getSampleRate());
        tmp = FloatArray::create(getBlockSize());
    }
    ~StereoPitchshifterPatch() {
        PitchShifterProcessor::destroy(pitchshifters[0]);
        PitchShifterProcessor::destroy(pitchshifters[1]);
        FloatArray::destroy(tmp);
    }
    void processAudio(AudioBuffer& buffer) {
        int transposition = (getParameterValue(P_TRANSPOSE) * 2 - 1.0) * (SEMITONES + 1) + 1;
        debugMessage("#Semitones =", transposition);
        float mix = getParameterValue(P_MIX);
        float mod = getParameterValue(P_MOD);
        for (int i = 0; i < 2; i++) {
            auto processor = pitchshifters[i];
            auto pitchshifter = processor->getPitchShifter();
            pitchshifter->SetTransposition(transposition);
            pitchshifter->SetFun(mod);
            FloatArray channel = buffer.getSamples(i);
            processor->process(channel, tmp);            
            tmp.subtract(channel);
            tmp.multiply(mix);
            tmp.add(channel, channel);
        }
    }
};
