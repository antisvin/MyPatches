#ifndef __ResSamplerPatch_hpp__
#define __ResSamplerPatch_hpp__

#include "Patch.h"
#include "ResourceStorage.h"
#include "Envelope.h"
#include "Oscillator.h"
#include "VoltsPerOctave.h"
#include "Control.h"
#include "SmoothValue.h"

#define SAMPLE_NAME "sample.wav"
#define MIDDLE_C 261.6

// Choose one of the 3 options above
//#define RESOURCE_MEMORY_MAPPED
//#define RESOURCE_DYNAMIC
#define RESOURCE_STATIC

// Must be set for static resource test (size in bytes):
#define RESOURCE_SIZE 51972

#if defined(RESOURCE_STATIC) && defined(RESOURCE_SIZE)
// Some wizardry for static allocation to SRAM
static constexpr uint32_t resource_length = RESOURCE_SIZE / sizeof(float); // Elements count
using ResourceBuffer = float[resource_length];
using SampleResource = MemoryResource<ResourceBuffer>;
#endif

/*
  need a buffer that can be read in a circular fashion
 */
class SampleOscillator : public Oscillator {
private:
    const float fs;
    FloatArray buffer;
    float pos = 0.0f;
    uint16_t size;
    float rate = 1.0f;

public:
    SampleOscillator(float sr, FloatArray sample)
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
    static float interpolate(float index, FloatArray data) {
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
    SamplerVoice(float sr, FloatArray buf)
        : osc(sr, buf)
        , env(sr)
        , gain(1.0f) {
        env.setSustain(1.0);
        env.setDecay(0.0);
        env.setRelease(0.0);
    }
    static SamplerVoice* create(float sr, FloatArray buf) {
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
        voice[ch]->setGain(velocity / (4095.0f * (VOICES / 2)));
        voice[ch]->setGate(true, samples);
    }

    void release(uint8_t ch, uint16_t samples) {
        // notes[ch] = EMPTY;
        allocation[ch] = ++allocated;
        voice[ch]->setGate(false, samples);
    }

public:
    Voices() = default;
    Voices(float sr, int bs, FloatArray buf)
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

#ifdef RESOURCE_MEMORY_MAPPED
Resource* resource;
#elif defined(RESOUCE_DYNAMIC)
DynamicMemoryResource resource;
#elif defined(RESOURCE_STATIC)
// This is a global object to make sure it's allocated on SRAM.
SampleResource resource;
#endif


class ResSamplerPatch : public Patch {
private:
    Voices<8> voices;
    ResourceStorage storage;
    bool is_loaded;

public:
    ResSamplerPatch() : Patch() {
        registerParameter(PARAMETER_A, "Pitchbend");
        registerParameter(PARAMETER_D, "Envelope");
        FloatArray sample;

        #ifdef RESOURCE_MEMORY_MAPPED
        resource = storage.getResource(SAMPLE_NAME);
        if (resource != NULL){
            sample = resource->asArray<FloatArray, float>();
            is_loaded = true;
        }
        #elif defined(RESOURCE_DYNAMIC)
        if (storage.loadResource(SAMPLE_NAME, resource)){
            sample = resource.asArray<FloatArray, float>();
            is_loaded = true;
        }
        #elif defined(RESOURCE_STATIC)
        if (storage.loadResource<ResourceBuffer>(SAMPLE_NAME, resource, 0)){
            sample = resource.asArray<FloatArray, float>();
            is_loaded = true;
        }
        #endif

        if (is_loaded){
            debugMessage("Sample loaded");
            
            #if defined(RESOURCE_DYNAMIC) || defined(RESOURCE_STATIC)
            // Sample is immutable if we read from flash
            float divisor = max(abs(sample.getMaxValue()), abs(sample.getMinValue()));
            if (divisor > 0.0f)
                sample.multiply(1.0f / divisor);
            #endif

            voices = Voices<8>(getSampleRate(), getBlockSize(), sample);
        }
        else {
            debugMessage("Sample NOT loaded");
        }
    }
    ~ResSamplerPatch() {
    }
    
    void buttonChanged(PatchButtonId bid, uint16_t value, uint16_t samples) {
        if (!is_loaded)
            return;

        if (bid == PUSHBUTTON) {
            debugMessage("push", bid, value, samples);
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
