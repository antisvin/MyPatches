#ifndef __WAVE_TABLE_OSCILLATOR_HPP__
#define __WAVE_TABLE_OSCILLATOR_HPP__

#include "Oscillator.h"
#include "InterpolatingCircularBuffer.h"
#include "WavLoader.hpp"

template <InterpolationMethod im = LINEAR_INTERPOLATION>
class WaveTableBuffer : public InterpolatingCircularFloatBuffer<im> {
public:
    void setData(float* data) {
        this->data = data;
    }
    void setSize(size_t size) {
        this->size = size;
    }
};

class Integrator : public SignalProcessor {
private:
    float lambda;
    float y;

public:
    Integrator(float lambda = 0.995)
        : lambda(lambda)
        , y(0) {
    }
    void setSampleRate(float sr) {
        mul = 1.f / sr;
    }
    void setCutoff(float fc) {
        lambda = fc * mul;
        if (lambda > 1.f)
            lambda = 1.f;
    }
    /* process a single sample and return the result */
    float process(float x) {
        y = y * lambda + x * (1.0f - lambda);
        return y;
    }

    void process(float* input, float* output, size_t size) {
        float x;
        while (size--) {
            x = *input++;
            y = y * lambda + x * (1.0f - lambda);
            *output++ = y;
        }
    }

    /* perform in-place processing */
    void process(float* buf, int size) {
        process(buf, buf, size);
    }

    void process(FloatArray in) {
        process(in, in, in.getSize());
    }

    void process(FloatArray in, FloatArray out) {
        ASSERT(out.getSize() >= in.getSize(),
            "output array must be at least as long as input");
        process(in, out, in.getSize());
    }

    static SmoothingFilter* create(float lambda) {
        return new SmoothingFilter(lambda);
    }

    static void destroy(SmoothingFilter* obj) {
        delete obj;
    }

protected:
    float mul = 1.f / 48000;
};

template <size_t size, size_t dims, InterpolationMethod im = LINEAR_INTERPOLATION>
class WaveTableOscillator
    : public OscillatorTemplate<WaveTableOscillator<size, dims, im>> {
public:
    size_t dimensions[dims];
    WaveTableBuffer<im> samples;

    WaveTableOscillator(float* data)
        : OscillatorTemplate<WaveTableOscillator<size, dims, im>>() {
        table_start = data;
        samples.setData(table_start);
        samples.setSize(size);
    }

    static constexpr float begin_phase = 0.f;
    static constexpr float end_phase = 1.f;
    float getSample() {
        return samples.readAt(size * this->phase);
    }

    void generate(FloatArray input, FloatArray output) override {
        OscillatorTemplate<WaveTableOscillator<size, dims, im>>::generate(input, output);
        integrator.process(input, output);
    }

    void setFrequency(float freq) override {
        OscillatorTemplate<WaveTableOscillator<size, dims, im>>::setFrequency(freq);
        integrator.setCutoff(freq);
    }

protected:
    float* table_start;
    Integrator integrator;
};

template <size_t size, size_t x_dim, size_t y_dim, InterpolationMethod im = LINEAR_INTERPOLATION>
class WaveTableOscillator2D : public WaveTableOscillator<size, 2, im> {
public:
    WaveTableOscillator2D(float* data)
        : WaveTableOscillator<size, 2, im>(data) {
    }

    void setTable(size_t x, size_t y) {
        this->samples.setData(this->table_start + (y * y_dim + x) * size);
    }

    static WaveTableOscillator2D* create(const char* name) {
        FloatArray samples = WavLoader::load(name);
        if (samples.getSize() == x_dim * y_dim * size) {
            return new WaveTableOscillator2D(samples.getData());
        }
        else {
            return NULL;
        }
    }

    static void destroy(WaveTableOscillator2D* table) {
        delete[] table->table_start;
        delete table;
    }
};

#endif
