#include "noise.hpp"

#define FNL_IMPL
#include "FastNoiseLite.h"

float noise3d(int seed, float x, float y, float z) {
    return _fnlSingleOpenSimplex23D(seed, x, y, z);
}

float noise2d(int seed, float x, float y) {
    return _fnlSingleSimplex2D(seed, x, y);
}
