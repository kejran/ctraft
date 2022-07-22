#pragma once 

#include <cstdint>

float Noise2_UnskewedBase(int64_t seed, double xs, double ys);
float Noise2(int64_t seed, double x, double y);
float Noise2_ImproveX(int64_t seed, double x, double y);
float Noise3_UnrotatedBase(int64_t seed, double xr, double yr, double zr);
float Noise3_ImproveXY(int64_t seed, double x, double y, double z);
float Noise3_ImproveXZ(int64_t seed, double x, double y, double z);
float Noise3_Fallback(int64_t seed, double x, double y, double z);

void initSimplex();