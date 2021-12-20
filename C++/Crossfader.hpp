#ifndef __CROSSFADER_HPP__
#define __CROSSFADER_HPP__

#include "basicmaths.h"

/**
 * Crossfade shapes
 * 
 * - CrossfadeShape::CROSSFADE_LINEAR - constant voltage for correlated signals
 * - CrossfadeShape::CROSSFADE_HANN - constant voltage based on Hann window transients
 * TODO: flattened Hann?
 * - CrossfadeShape::CROSSFADE_SQRT - constant power based on square root function
 * - CrossfadeShape::CROSSFADE_COS - constant power based on cosine function
 * - CrossfadeShape::CROSSFADE_PARABOLIC - cheap to compute and slightly exceeds constant power
 */
enum CrossfadeShape {
    CROSSFADE_LINEAR,
    CROSSFADE_HANN,
    CROSSFADE_SQRT,
    CROSSFADE_COS,
    CROSSFADE_PARABOLIC,
};

template<CrossfadeShape cf>
class Crossfader {
public:
    static float crossfade(float a, float b, float t);
};

template<>
float Crossfader<CROSSFADE_LINEAR>::crossfade(float a, float b, float t) {
    return a + (b - a) * t;
}

template<>
float Crossfader<CROSSFADE_HANN>::crossfade(float a, float b, float t) {
    return 0.5 * (a + b + cosf(M_PI * t) * (a - b));
}

template<>
float Crossfader<CROSSFADE_SQRT>::crossfade(float a, float b, float t) {
    return sqrtf(1.0 - t) * a + sqrtf(t) * b;
}

template<>
float Crossfader<CROSSFADE_COS>::crossfade(float a, float b, float t) {
    return cosf(M_PI_2 * t) * a + cosf(M_PI_2 * (1.0 - t)) * b;
}

template<>
float Crossfader<CROSSFADE_PARABOLIC>::crossfade(float a, float b, float t) {
    return (1.0 - t * t) * a + (2.0 * t - t * t) * b;
}
#endif
