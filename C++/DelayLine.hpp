#ifndef __DelayLine_hpp__
#define __DelayLine_hpp__

#undef min
#undef max
#undef abs
#undef rand
#include <algorithm>

template <typename T, unsigned int max_delay>
class DelayLine {
public:
  DelayLine() {};

  DelayLine(T* buffer) : buffer(buffer) {
  };

  /**
   * delay line can allocate memory instead of using preallocated buffer
   */
  void create() {
    buffer = new T[max_delay];
    reset();
  }

  /**
   * this must always be called if buffer was allocated by create()
   */
  void destroy() {
    delete[] buffer;
  }

  void reset() {
    std::fill(&buffer[0], &buffer[max_delay], T(0));
    writeIndex = 0;
  }

  /** 
   * write to the tail of the circular buffer 
   */
  inline void write(const T value) {
    buffer[writeIndex] = value;
    writeIndex = ++writeIndex % max_delay;
  }

  /**
   * read the value @param index steps back from the head of the circular buffer
   */
  inline const T read(int index) {
    return buffer[(writeIndex + index + max_delay) % max_delay];
  }

  /**
   * return a value interpolated to a floating point index
   */
  inline const T interpolate(float index) {
    int idx = (int)index;
    float low = read(idx);
    float high = read(idx + 1);
    float frac = index - idx;
    return low + (high - low) * frac;
  }

  /**
   * return a value interpolated to a floating point index
   */
  inline const T interpolateHermite(float index) {
    int idx = (int)index;
    float frac = index - idx;
    int32_t t = (writeIndex + idx + max_delay);
    const T xm1 = buffer[(t - 1) % max_delay];
    const T x0 = buffer[(t) % max_delay];
    const T x1 = buffer[(t + 1) % max_delay];
    const T x2 = buffer[(t + 2) % max_delay];
    const float c = (x1 - xm1) * 0.5f;
    const float v = x0 - x1;
    const float w = c + v;
    const float a = w + v + (x2 - x0) * 0.5f;
    const float b_neg = w + a;
    const float f = frac;
    return (((a * f) - b_neg) * f + c) * f + x0;    
  }

  inline const T allpass(const T value, int delay, const T coefficient) {
    T read_value = read(delay);
    T write_value = value + coefficient * read_value;
    write(write_value);
    return -write_value * coefficient + read_value;
  }

  /**
   * get the value at the head of the circular buffer
   */
  inline const T head() {
    return read(-1);
  }

  /** 
   * get the most recently written value 
   */
  inline const T tail() {
    return read(0);
  }

  /**
   * get the capacity of the circular buffer
   */
  inline const unsigned int getSize() {
    return max_delay;
  }

private:
  T* buffer;  
  unsigned int writeIndex;
};

#endif // __DelayLine_hpp__
