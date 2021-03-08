#ifndef __MULTI_OSCILLATOR_HPP__
#define __MULTI_OSCILLATOR_HPP__

#include "Oscillator.h"
#include "FloatArray.h"


template<size_t limit>
class MultiOscillator : public Oscillator {
public:
    MultiOscillator(float sr = 48000)
        : mul(1.0 / sr)
        , nfreq(0.0)
        , phase(0) {
    }
    MultiOscillator(float freq, float sr = 48000) : MultiOscillator(sr) {
        setFrequency(freq);
    }

    ~MultiOsicllator(){
        for (int i = 0; i < num_oscillators; i++){
            delete oscillators[i];
        }
    }
    void reset() override {
        setPhase(0.0f);
    }
    void setSampleRate(float sr) override {
        for (int i = 0; i < num_oscillators; i++){
            oscillators[i]->setSamplerate(sr);
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
        morph *= (limit - 1);
        size_t morph_new = size_t(morph);
        if (morph_idx != morph_new){
            // Index changed
            if (morph_idx + 1 != morph_new) {
                // Don't reset if we changed to previously active voice
                oscillators[morph_new + 1].setPhase(getPhase());
            }
            oscillators[morph_new].setPhase(getPhase());
            morph_idx = morph_new;
        }
        else if ()
        this->morph = morph;
    }
    float generate() override {
        float sample_a = oscillators[morph_idx].generate();
        float sample_b = oscillators[morph_idx + 1].generate();
        return sample_a + (sample_b - sample_a) * morph_frac;
    }
    void generate(FloatArray output) override {
        render(output.getSize(), output.getData());
    }
    float generate(float fm) override {        
        float sample_a = oscillators[morph_idx].generate();
        float sample_b = oscillators[morph_idx + 1].generate();
        phase += fm;
        return sample_a + (sample_b - sample_a) * morph_frac;
    }
    bool addOscillator(Oscillator osc){
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
    static MultiOscillator* create(float sr){
        return new MultiOscillator(sr);
    }
    static MultiOscillator* create(float freq, float sr){
        return new MultiOscillator(freq, sr);
    }
    static void destroy(MultiOscillator* osc){
        // We can't destroy child oscillators here, because this is a static method that
        // has no idea about their vptrs
        delete osc;
    }    
protected:
    size_t num_oscillators = 0;
    Oscillator* oscillators[limit] = {};
    size_t morph_idx = 0;
    size_t morph_frac;
    float mul;
    float nfreq;
    float phase;
};


template<size_t limit>
class BufferedMultiOscillator : public MultiOscillator<limit> {
public:
    static BufferedMultiOscillator* create(float sr, size_t buffer_size){
        auto osc = Multioscillator<limit>::create(sr);
        osc->buf = FloatArray::create(buffer_size);
        return osc;
    }
    static BufferMultiOscillator* create(float freq, float sr, size_t buffer_size){
        auto osc = MultiOscillator<limit>::create(freq, sr);
        osc->buf = FloatArray::create(buffer_size);
        return osc;
    }
    static void destroy(BufferedMultiOscillator* osc){
        FloatArray::destroy(osc->buf);
        MultiOscillator<limit>::destroy(osc);
    }  
protected:
    FloatArray buf;
};

#endif
