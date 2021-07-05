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

protected:
    float* table_start;
};


template <size_t size, size_t x_dim, size_t y_dim, InterpolationMethod im = LINEAR_INTERPOLATION>
class WaveTableOscillator2D : public WaveTableOscillator<size, 2, im> {
public:
    WaveTableOscillator2D(float* data) : WaveTableOscillator<size, 2, im>(data) {}

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
