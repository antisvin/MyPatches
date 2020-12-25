#ifndef __ResSamplerPatch_hpp__
#define __ResSamplerPatch_hpp__

#include "Patch.h"
#include "Resource.h"
#include "Envelope.h"
#include "Oscillator.h"
#include "VoltsPerOctave.h"
#include "Control.h"
#include "SmoothValue.h"
#include "ScreenBuffer.h"

#define SAMPLE_NAME "sample.wav"
#define MIDDLE_C 261.6

// Choose one of the 3 options above
//#define RESOURCE_MEMORY_MAPPED
#define RESOURCE_LOADED


/*
  need a buffer that can be read in a circular fashion
 */
class SampleOscillator : public Oscillator {
private:
    const float fs;
    const FloatArray buffer;
    float pos = 0.0f;
    uint16_t size;
    float rate = 1.0f;

public:
    SampleOscillator(float sr, const FloatArray sample)
        : fs(sr)
        , buffer(sample) {
        size = buffer.getSize() - 1;
    }
    void setFrequency(float freq) {
        rate = freq / MIDDLE_C;
    }
    // void setPortamento(float portamento){
    //   rate.lambda = portamento*0.19 + 0.8;
    // }
    void reset() {
        pos = 0.0f;
    }
    void increment() {
        pos += rate;
        if (pos < 0.0f)
            pos += size;
        else if (pos > size)
            pos -= size;
    }
    static float interpolate(float index, const FloatArray& data) {
        int idx = (int)index;
        float low = data[idx];
        float high = data[idx + 1];
        float frac = index - idx;
        return low * frac + high * (1.0 - frac);
    }
    float getNextSample() {
        increment();
        return interpolate(pos, buffer);
    }
    uint16_t findZeroCrossing(uint16_t index) {
        uint16_t limit = buffer.getSize() - 1;
        uint16_t i = min(index, limit);
        if (buffer[i] > 0)
            while (i < limit && buffer[i] > 0)
                i++;
        else
            while (i < limit && buffer[i] < 0)
                i++;
        return i;
    }
    void setDuration(uint16_t samples) {
        size = findZeroCrossing(samples);
    }
    /* @return the audio length in samples */
    uint16_t getSampleLength() {
        return buffer.getSize();
    }
    // void getSamples(FloatArray out){
    //   // unwind interpolation
    // }
};

class SamplerVoice {
private:
    SampleOscillator osc;
    AdsrEnvelope env;
    float gain;
    Control<PARAMETER_AA> attack = 0.0f;
    Control<PARAMETER_AB> decay = 0.0f;
    Control<PARAMETER_AC> sustain = 1.0f;
    Control<PARAMETER_AD> release = 0.0f;
    // Control<PARAMETER_AE> portamento = 0.0f;
    Control<PARAMETER_AF> duration = 1.0f;

public:
    SamplerVoice(float sr, const FloatArray& buf)
        : osc(sr, buf)
        , env(sr)
        , gain(1.0f) {
        env.setSustain(1.0);
        env.setDecay(0.0);
        env.setRelease(0.0);
    }
    static SamplerVoice* create(float sr, const FloatArray& buf) {
        return new SamplerVoice(sr, buf);
    }
    static void destroy(SamplerVoice* voice) {
        delete voice;
    }
    void setFrequency(float freq) {
        // osc.setPortamento(portamento); // portamento won't work across voices
        osc.setFrequency(freq);
    }
    void setEnvelope(float att, float rel) {
        // env.setAttack(att);
        // env.setRelease(rel);
        env.setAttack((float)attack + att);
        env.setDecay(decay);
        env.setSustain(sustain);
        env.setRelease((float)release + rel);
    }
    void setGain(float value) {
        gain = value;
    }
    void setGate(bool state, int delay) {
        env.gate(state, delay);
        if (state)
            osc.reset();
    }
    void getSamples(FloatArray samples) {
        osc.setDuration(duration * osc.getSampleLength());
        osc.getSamples(samples);
        samples.multiply(gain);
        env.attenuate(samples);
    }
};

template <int VOICES>
class Voices {
private:
    static const uint8_t EMPTY = 0xff;
    static const uint16_t TAKEN = 0xffff;
    SamplerVoice* voice[VOICES];
    uint8_t notes[VOICES];
    uint16_t allocation[VOICES];
    uint16_t allocated;
    FloatArray buffer;

private:
    void take(uint8_t ch, uint8_t note, uint16_t velocity, uint16_t samples) {
        notes[ch] = note;
        allocation[ch] = TAKEN;
        voice[ch]->setGain(velocity / (4095.0f * (VOICES / 4)));
        voice[ch]->setGate(true, samples);
    }

    void release(uint8_t ch, uint16_t samples) {
        // notes[ch] = EMPTY;
        allocation[ch] = ++allocated;
        voice[ch]->setGate(false, samples);
    }

public:
    Voices() = default;
    Voices(float sr, int bs, const FloatArray& buf)
        : allocated(0) {
        for (int i = 0; i < VOICES; ++i) {
            voice[i] = SamplerVoice::create(sr, buf);
            notes[i] = 69; // middle A, 440Hz
            allocation[i] = 0;
        }
        buffer = FloatArray::create(bs);
    }

    ~Voices() {
        for (int i = 0; i < VOICES; ++i)
            SamplerVoice::destroy(voice[i]);
        FloatArray::destroy(buffer);
    }

    int getAllocatedCount() {
        int count = 0;
        for (int i = 0; i < VOICES; ++i)
            count += allocation[i] == TAKEN;
        return count;
    }

    void allNotesOff() {
        for (int i = 0; i < VOICES; ++i)
            release(i, 0);
        allocated = 0;
    }

    void allNotesOn() {
        for (int i = 0; i < VOICES; ++i) {
            voice[i]->setGain(0.6 / VOICES);
            voice[i]->setGate(true, 0);
        }
    }

    void noteOn(uint8_t note, uint16_t velocity, uint16_t samples) {
        uint16_t minval = allocation[0];
        uint8_t minidx = 0;
        // take oldest free voice, to allow voices to ring out
        for (int i = 1; i < VOICES; ++i) {
            if (notes[i] == note) {
                minidx = i;
                break;
            }
            if (allocation[i] < minval) {
                minidx = i;
                minval = allocation[i];
            }
        }
        // take oldest voice
        take(minidx, note, velocity, samples);
    }

    void noteOff(uint8_t note, uint16_t samples) {
        for (int i = 0; i < VOICES; ++i)
            if (notes[i] == note)
                release(i, samples);
    }

    void setParameters(float attack, float release, float pb) {
        for (int i = 0; i < VOICES; ++i) {
            float freq = 440.0f * exp2f((notes[i] - 69 + pb * 2) / 12.0);
            voice[i]->setFrequency(freq);
            voice[i]->setEnvelope(attack, release);
        }
    }

    void getSamples(FloatArray samples) {
        voice[0]->getSamples(samples);
        for (int i = 1; i < VOICES; ++i) {
            voice[i]->getSamples(buffer);
            samples.add(buffer);
        }
    }
};

class ResSamplerPatch : public Patch {
private:
    Voices<8> voices;
    bool is_loaded;

    #ifdef RESOURCE_MEMORY_MAPPED
    const Resource* resource;
    #else
    Resource resource;
    #endif

#ifdef USE_SCREEN
    static constexpr int8_t preview_scale = 50;
    int8_t preview_hi[128];
    int8_t preview_lo[128];

    void storePreview(const FloatArray& sample){
        // This ended up not particularly pretty, but at least gives a hint about loaded data
        size_t length = sample.getSize();
        size_t step = length / 128;
        for (int i = 0; i < 128; i++) {
            float max_val = 0, min_val = 0;
            for (size_t j = i * step; j < (i + 1) * step; j++){
                float val = sample[j];
                max_val = max(val, max_val);
                min_val = min(val, min_val);
            }
            preview_hi[i] = max_val * preview_scale;
            preview_lo[i] = min_val * preview_scale;
        }
    }
#endif
public:
#ifdef USE_SCREEN
    void processScreen(ScreenBuffer& screen){
#ifdef RESOURCE_MEMORY_MAPPED
        screen.print(110, 10, "F");
#elif defined(RESOURCE_LOADED)
        screen.print(110, 10, "M");
#endif
        for (int i = 0; i < screen.getWidth(); i++) {
            screen.setPixel(i, 20 + preview_hi[i], WHITE);
            screen.setPixel(i, 20 + preview_lo[i], WHITE);
        }
        screen.setCursor(110, 44);
        screen.print(voices.getAllocatedCount());
        screen.print("v.");
   }
#endif
    ResSamplerPatch() : Patch() {
        registerParameter(PARAMETER_A, "Pitchbend");
        registerParameter(PARAMETER_D, "Envelope");

        registerParameter(PARAMETER_AA, "Env Attack");
        registerParameter(PARAMETER_AB, "Env Decay");
        registerParameter(PARAMETER_AC, "Env Sustain");
        registerParameter(PARAMETER_AD, "Env Release");
        //registerParameter(PARAMETER_AE, "Portamento");
        registerParameter(PARAMETER_AF, "Duration");

        #ifdef RESOURCE_MEMORY_MAPPED
        resource = Resource::get(SAMPLE_NAME);
        if (resource != NULL){
            const FloatArray sample = resource->asArray<FloatArray, float>();
            //FloatArray* sample_ptr = const_cast<FloatArray*>(&sample);
            voices = Voices<8>(getSampleRate(), getBlockSize(), sample);
#ifdef USE_SCREEN
            storePreview(sample);
#endif
            voices = Voices<8>(getSampleRate(), getBlockSize(), sample);
            is_loaded = true;
        }
        #elif defined(RESOURCE_LOADED)
        FloatArray sample;
        resource = Resource::load(SAMPLE_NAME);
        if (resource.hasData()){
            sample = resource.asArray<FloatArray, float>();
#ifdef USE_SCREEN
            storePreview(sample);
#endif
            is_loaded = true;
            // Sample is immutable if we read from flash
            float divisor = max(abs(sample.getMaxValue()), abs(sample.getMinValue()));
            if (divisor > 0.0f)
                sample.multiply(1.0f / divisor);
            voices = Voices<8>(getSampleRate(), getBlockSize(), sample);
        }
        #endif

    }
    ~ResSamplerPatch() {
#ifdef RESOURCE_MEMORY_MAPPED
        if (resource != NULL)
            delete resource;
#elif defined(RESOURCE_LOADED)
        Resource::destroy(resource);
#endif
    }
    
    void buttonChanged(PatchButtonId bid, uint16_t value, uint16_t samples) {
        if (!is_loaded)
            return;

        if (bid == PUSHBUTTON) {
            if (value)
                voices.allNotesOn();
            else
                voices.allNotesOff();
        }
    }

    void processMidi(MidiMessage msg){
        if(msg.isNoteOn())
	        voices.noteOn(msg.getNote(), msg.getVelocity(), 0);
        else if (msg.isNoteOff()) // note off
	        voices.noteOff(msg.getNote(), 0);
    }    

    void processAudio(AudioBuffer& buffer) {
        if (!is_loaded)
            return;

        float env = getParameterValue(PARAMETER_D);
        float pitchbend = getParameterValue(PARAMETER_G); // MIDI Pitchbend
        pitchbend += getParameterValue(PARAMETER_A);
        float attack = 0.0f;
        float release = 0.0f;
        if (env < 0.5)
            attack = 2 * (0.5 - env);
        else
            release = 2 * (env - 0.5);

        FloatArray left = buffer.getSamples(LEFT_CHANNEL);
        FloatArray right = buffer.getSamples(RIGHT_CHANNEL);
        voices.setParameters(attack, release, pitchbend);
        voices.getSamples(left);
        right.copyFrom(left);
    }
};

#endif // __ResSamplerPatch_hpp__
