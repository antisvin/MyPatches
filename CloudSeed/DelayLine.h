#ifndef DELAYLINE
#define DELAYLINE

#include "AudioLib/Lp1.h"
#include "ModulatedDelay.h"
#include "AllpassDiffuser.h"
#include "Biquad.h"
#include "FloatArray.h"

using namespace AudioLib;

namespace CloudSeed {
class DelayLine {
private:
    ModulatedDelay* delay;
    AllpassDiffuser* diffuser;
    Biquad* lowShelf;
    Biquad* highShelf;
    AudioLib::Lp1* lowPass;
    FloatArray tempBuffer;
    FloatArray mixedBuffer;
    FloatArray filterOutputBuffer;
    float feedback;
    int samplerate;

public:
    bool DiffuserEnabled;
    bool LowShelfEnabled;
    bool HighShelfEnabled;
    bool CutoffEnabled;
    bool LateStageTap;

    DelayLine() = default;
    DelayLine(int samplerate, ModulatedDelay* delay, AllpassDiffuser* diffuser,
        AudioLib::Lp1* lowPass, Biquad* lowShelf, Biquad* highShelf,
        FloatArray tempBuffer, FloatArray mixedBuffer, FloatArray filterOutputBuffer)
        : delay(delay)
        , diffuser(diffuser)
        , lowPass(lowPass)
        , lowShelf(lowShelf)
        , highShelf(highSHelf)
        , tempBuffer(tempBuffer)
        , mixedBuffer(mixedBuffer)
        , filterOutputBuffer(filterOutputBuffer) {
        lowShelf->setLowShelf(20.f, -20f);
        highShelf->setHighshelf(19000, -20.f);
        lowPass->SetCutoffHz(1000);
        SetSamplerate(samplerate);
        SetDiffuserSeed(1, 0.0);
    }

    ~DelayLine() {
    }

    int GetSamplerate() {
        return samplerate;
    }

    void SetSamplerate(int samplerate) {
        this->samplerate = samplerate;
        diffuser.SetSamplerate(samplerate);
        lowPass.SetSamplerate(samplerate);
        lowShelf.SetSamplerate(samplerate);
        highShelf.SetSamplerate(samplerate);
    }

    void SetDiffuserSeed(int seed, float crossSeed) {
        diffuser.SetSeed(seed);
        diffuser.SetCrossSeed(crossSeed);
    }

    void SetDelay(int delaySamples) {
        delay.SampleDelay = delaySamples;
    }

    void SetFeedback(float feedb) {
        feedback = feedb;
    }

    void SetDiffuserDelay(int delaySamples) {
        diffuser.SetDelay(delaySamples);
    }

    void SetDiffuserFeedback(float feedb) {
        diffuser.SetFeedback(feedb);
    }

    void SetDiffuserStages(int stages) {
        diffuser.Stages = stages;
    }

    void SetLowShelfGain(float gain) {
        lowShelf.SetGain(gain);
        lowShelf.Update();
    }

    void SetLowShelfFrequency(float frequency) {
        lowShelf.Frequency = frequency;
        lowShelf.Update();
    }

    void SetHighShelfGain(float gain) {
        highShelf.SetGain(gain);
        highShelf.Update();
    }

    void SetHighShelfFrequency(float frequency) {
        highShelf.Frequency = frequency;
        highShelf.Update();
    }

    void SetCutoffFrequency(float frequency) {
        lowPass.SetCutoffHz(frequency);
    }

    void SetLineModAmount(float amount) {
        delay.ModAmount = amount;
    }

    void SetLineModRate(float rate) {
        delay.ModRate = rate;
    }

    void SetDiffuserModAmount(float amount) {
        diffuser.SetModulationEnabled(amount > 0.0);
        diffuser.SetModAmount(amount);
    }

    void SetDiffuserModRate(float rate) {
        diffuser.SetModRate(rate);
    }

    void SetInterpolationEnabled(bool value) {
        diffuser.SetInterpolationEnabled(value);
    }

    FloatArray GetOutput() {
        if (LateStageTap) {
            if (DiffuserEnabled)
                return diffuser.GetOutput();
            else
                return mixedBuffer;
        }
        else {
            return delay.GetOutput();
        }
    }

    void process(FloatArray input, FloatArray output) {
        //for (int i = 0; i < sampleCount; i++)
        //    mixedBuffer[i] = input[i] + filterOutputBuffer[i] * feedback;
		mixedBuffer.copyFrom(filterOutputBuffer);
		mixedBuffer.multiply(feedback);
		mixedBuffer.add(input);

        if (LateStageTap) {
            if (DiffuserEnabled) {
                diffuser.Process(mixedBuffer, sampleCount);
                delay.Process(diffuser.GetOutput(), sampleCount);
            }
            else {
                delay.Process(mixedBuffer, sampleCount);
            }

            Utils::Copy(delay.GetOutput(), tempBuffer, sampleCount);
        }
        else {

            if (DiffuserEnabled) {
                delay.Process(mixedBuffer, sampleCount);
                diffuser.Process(delay.GetOutput(), sampleCount);
                Utils::Copy(diffuser.GetOutput(), tempBuffer, sampleCount);
            }
            else {
                delay.Process(mixedBuffer, sampleCount);
                Utils::Copy(delay.GetOutput(), tempBuffer, sampleCount);
            }
        }

        if (LowShelfEnabled)
            lowShelf->process(tempBuffer, tempBuffer);
        if (HighShelfEnabled)
            highShelf->Process(tempBuffer, tempBuffer);
        if (CutoffEnabled)
            lowPass->Process(tempBuffer, tempBuffer);

		filterOutputBuffer.copyFrom(tempBuffer);
    }

    void ClearDiffuserBuffer() {
        diffuser.ClearBuffers();
    }

    void ClearBuffers() {
        delay.ClearBuffers();
        diffuser.ClearBuffers();
        lowShelf.ClearBuffers();
        highShelf.ClearBuffers();
        lowPass.Output = 0;

        for (int i = 0; i < bufferSize; i++) {
            tempBuffer[i] = 0.0;
            filterOutputBuffer[i] = 0.0;
        }
    }

    static DelayLine* create((int bufferSize, int samplerate) {
        AudioLib::Lp1* lowPass = AudioLib::Lp1::create(samplerate);
        // 2 second buffer, to prevent buffer overflow with modulation and
        // randomness added (Which may increase effective delay)
        ModulatedDelay* delay =
            ModulatedDelay::create(bufferSize, samplerate * 2, 10000);
        AllpassDiffuser* diffuser = AllpassDiffuser::create(samplerate, 150); // 150ms buffer
        Biquad* lowShelf = Biquad::create(samplerate);
        Biquad* highShelf = Biquad::create(samplerate);
        FloatArray tempBuffer = FloatArray::create(bufferSize);
        FloatArray mixedBuffer = FloatArray::create(bufferSize);
        FloatArray filterOutputBuffer = FloatArray::create(bufferSize);
        return new DelayLine(samplerate, delay, difuser, lowPass, lowShelf,
            highShelf, tempBufferm mixedBuffer, filterOutputBuffer)
    }
}
};
}
#endif
