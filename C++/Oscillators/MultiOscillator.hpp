#ifndef __MULTI_OSCILLATOR_HPP__
#define __MULTI_OSCILLATOR_HPP__

#include "Oscillator.h"
#include "FloatArray.h"


/**
 * A class that contains multiple child oscillators and morphs between 2 of them
 */
class MultiOscillator : public Oscillator {
public:
    MultiOscillator(size_t limit, float sr = 48000)
        : limit(limit)
        , mul(1.0 / sr)
        , nfreq(0.0)
        , phase(0) {
        oscillators = new Oscillator*[limit];
    }
    MultiOscillator(size_t limit, float freq, float sr) : MultiOscillator(limit, sr) {
        setFrequency(freq);
    }

    ~MultiOscillator(){
        for (int i = 0; i < num_oscillators; i++){
            delete oscillators[i];
        }
        delete[] oscillators;
    }
    void reset() override {
        setPhase(0.0f);
    }
    void setSampleRate(float sr) override {
        for (int i = 0; i < num_oscillators; i++){
            oscillators[i]->setSampleRate(sr);
        }
        mul = 1.0f / sr;
    }
    float getSampleRate() override {
        return 1.0f / mul;
    }
    void setFrequency(float freq) override {
        for (int i = 0; i < num_oscillators; i++){
            oscillators[i]->setFrequency(freq);
        }
        nfreq = mul * freq;
    }
    float getFrequency() override {
        return nfreq / mul;
    }
    void setPhase(float phase) override {
        for (int i = 0; i < num_oscillators; i++){
            oscillators[i]->setPhase(phase);
        }         
        this->phase = phase / (M_PI * 2.0);
    }
    float getPhase() override {
        return phase * 2.0 * M_PI;
    }
    void setMorph(float morph){
        morph *= (num_oscillators - 1);
        size_t morph_new = size_t(morph);
        if (morph_idx != morph_new){
            // Index changed
            if (morph_idx + 1 != morph_new) {
                // Don't reset if we changed to previously active voice
                oscillators[morph_new + 1]->setPhase(getPhase());
            }
            if (morph_idx != morph_new + 1)
                oscillators[morph_new]->setPhase(getPhase());
            morph_idx = morph_new;
        }
        morph_frac = morph - float(morph_new);
    }
    float generate() override {
        phase += nfreq;
        while (phase >= 1.0) {
            phase -= 1.0;
        }
        float sample_a = oscillators[morph_idx]->generate();
        float sample_b = oscillators[morph_idx + 1]->generate();
        return sample_a + (sample_b - sample_a) * morph_frac;
    }
    virtual void generate(FloatArray output) override {
        phase += nfreq * output.getSize();
        while (phase >= 1.0) {
            phase -= 1.0;
        }
        for (int i = 0; i < output.getSize(); i++){
            output[i] = generate();
        }
    }
    float generate(float fm) override {        
        float sample_a = oscillators[morph_idx]->generate();
        float sample_b = oscillators[morph_idx + 1]->generate();
        phase += fm;
        phase += nfreq;
        while (phase >= 1.0) {
            phase -= 1.0;
        }        
        return sample_a + (sample_b - sample_a) * morph_frac;
    }
    bool addOscillator(Oscillator* osc){
        if (num_oscillators < limit){
            oscillators[num_oscillators++] = osc;
            return true;
        }
        else {
            return false;
        }
    }
    size_t getSize() const {
        return num_oscillators;
    }
    static MultiOscillator* create(size_t limit, float sr){
        return new MultiOscillator(limit, sr);
    }
    static MultiOscillator* create(size_t limit, float freq, float sr){
        return new MultiOscillator(limit, freq, sr);
    }
    static void destroy(MultiOscillator* osc){
        // We can't destroy child oscillators here, because this is a static method that
        // has no idea about their vptrs. Must be done separately by caller class.
        delete osc;
    }    
protected:
    size_t limit = 0;
    size_t num_oscillators = 0;
    Oscillator** oscillators;
    size_t morph_idx = 0;
    float morph_frac;
    float mul;
    float nfreq;
    float phase;
};


/**
 * An extra buffer is used by this class to implement block-based version of ::generate()
 */
class BufferedMultiOscillator : public MultiOscillator {
public:
    BufferedMultiOscillator(size_t limit, float sr)
        : MultiOscillator(limit, sr) {}
    BufferedMultiOscillator(size_t limit, float freq, float sr)
        : MultiOscillator(limit, freq, sr) {}
    void generate(FloatArray output) override {
        this->oscillators[this->morph_idx]->generate(output);
        this->oscillators[this->morph_idx + 1]->generate(buf);
        buf.subtract(output);
        buf.multiply(this->morph_frac);
        output.add(buf);
    }
    static BufferedMultiOscillator* create(size_t limit, float sr, size_t buffer_size){
        auto osc = new BufferedMultiOscillator(limit, sr);
        osc->buf = FloatArray::create(buffer_size);
        return osc;
    }
    static BufferedMultiOscillator* create(size_t limit, float freq, float sr, size_t buffer_size){
        auto osc = new BufferedMultiOscillator(limit, freq, sr);
        osc->buf = FloatArray::create(buffer_size);
        return osc;
    }
    static void destroy(BufferedMultiOscillator* osc){
        FloatArray::destroy(osc->buf);
        MultiOscillator::destroy(osc);
    }  
protected:
    FloatArray buf;
};

#endif
