#ifndef __MONOCHROME_PREVIEW_HPP__
#define __MONOCHROME_PREVIEW_HPP__

#include "CircularBuffer.h"

template<typename T>
class PointTemplate {
public:
    T x, y;
    PointTemplate() = default;
    PointTemplate(int v) : PointTemplate(v, v) {};
    PointTemplate(T x, T y)
        : x(x)
        , y(y) {};
    friend PointTemplate operator+(PointTemplate lhs, const PointTemplate& rhs) {
        return PointTemplate(lhs.x + rhs.x, lhs.y + rhs.y);
    }
    friend PointTemplate operator-(PointTemplate lhs, const PointTemplate& rhs) {
        return PointTemplate(lhs.x - rhs.x, lhs.y - rhs.y);
    }
    PointTemplate operator*(T val) {
        return PointTemplate(x * val, y * val);
    }
};

/**
 * Updating display every 20ms should generate ~962 samples at 64 buffer size
 * 
 * To reduce amount of draws, some samples can be skipped
 **/
template<size_t size=256, size_t decimate=4, typename T=uint8_t>
class MonochromeQuadraturePreview {
public:
    using Point = PointTemplate<T>;
    using Buffer = CircularBuffer<Point>;

    Buffer* buffer;

    MonochromeQuadraturePreview() : half_width(0), half_height(0), center_x(0), center_y(0) {}
    MonochromeQuadraturePreview(Buffer* buffer) : buffer(buffer) {}
    ~MonochromeQuadraturePreview() = default;
    void ingestData(AudioBuffer& audio_buffer) {
        float* x = audio_buffer.getSamples(0).getData();
        float* y = audio_buffer.getSamples(1).getData();
        for (size_t i = 0; i < audio_buffer.getSize(); i += decimate) {
            addSample(x[i], y[i]);
        }
    }
    void addSample(float x, float y) {
        buffer->write(Point(x * half_width + center_x, y * half_height + center_y));
    }
    void setDimensions(T max_width, T max_height) {
        center_x = max_width / 2 - 1;
        center_y = max_height / 2 - 1;
        half_width = std::min<T>(center_x, half_width + 1);
        half_height = std::min<T>(center_y, half_height + 1);
    }
    static MonochromeQuadraturePreview* create() {        
        Buffer* buffer = Buffer::create(size);
        return new MonochromeQuadraturePreview(buffer);
    }
    static void destroy(MonochromeQuadraturePreview* preview) {
        Buffer::destroy(preview->buffer);
        delete preview;
    }
    size_t getSize() {
        return size;
    }
protected:
    T half_width, half_height;
    T center_x, center_y;
};

#endif
