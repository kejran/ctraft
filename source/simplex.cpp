/**
 * K.jpg's OpenSimplex 2, smooth variant ("SuperSimplex")
 * 
 * Translated from c# code
 */


#include "simplex.hpp"
#include <cstdint>

namespace {
    static constexpr int64_t PRIME_X = 0x5205402B9270C86FL;
    static constexpr int64_t PRIME_Y = 0x598CD327003817B5L;
    static constexpr int64_t PRIME_Z = 0x5BCC226E9FA0BACBL;
    static constexpr int64_t PRIME_W = 0x56CC5227E58F554BL;
    static constexpr int64_t HASH_MULTIPLIER = 0x53A3F72DEEC546F5L;
    static constexpr int64_t SEED_FLIP_3D = -0x52D547B2E96ED629L;

    static constexpr double ROOT2OVER2 = 0.7071067811865476;
    static constexpr double SKEW_2D = 0.366025403784439;
    static constexpr double UNSKEW_2D = -0.21132486540518713;

    static constexpr double ROOT3OVER3 = 0.577350269189626;
    static constexpr double FALLBACK_ROTATE3 = 2.0 / 3.0;
    static constexpr double ROTATE3_ORTHOGONALIZER = UNSKEW_2D;

    static constexpr float SKEW_4D = 0.309016994374947f;
    static constexpr float UNSKEW_4D = -0.138196601125011f;

    static constexpr int32_t N_GRADS_2D_EXPONENT = 7;
    static constexpr int32_t N_GRADS_3D_EXPONENT = 8;
    static constexpr int32_t N_GRADS_4D_EXPONENT = 9;
    static constexpr int32_t N_GRADS_2D = 1 << N_GRADS_2D_EXPONENT;
    static constexpr int32_t N_GRADS_3D = 1 << N_GRADS_3D_EXPONENT;
    static constexpr int32_t N_GRADS_4D = 1 << N_GRADS_4D_EXPONENT;

    static constexpr double NORMALIZER_2D = 0.05481866495625118;
    static constexpr double NORMALIZER_3D = 0.2781926117527186;
    static constexpr double NORMALIZER_4D = 0.11127401889945551;

    static constexpr float RSQUARED_2D = 2.0f / 3.0f;
    static constexpr float RSQUARED_3D = 3.0f / 4.0f;
    static constexpr float RSQUARED_4D = 4.0f / 5.0f;

    /*
     * Lookup Tables & Gradients
     */

    float GRADIENTS_2D[N_GRADS_2D * 2];
    float GRADIENTS_3D[N_GRADS_3D * 4];
    
    /*
     * Utility
     */

    inline float Grad(int64_t seed, int64_t xsvp, int64_t ysvp, float dx, float dy)
    {
        int64_t hash = seed ^ xsvp ^ ysvp;
        hash *= HASH_MULTIPLIER;
        hash ^= hash >> (64 - N_GRADS_2D_EXPONENT + 1);
        int gi = (int)hash & ((N_GRADS_2D - 1) << 1);
        return GRADIENTS_2D[gi | 0] * dx + GRADIENTS_2D[gi | 1] * dy;
    }

    inline float Grad(int64_t seed, int64_t xrvp, int64_t yrvp, int64_t zrvp, float dx, float dy, float dz)
    {
        int64_t hash = (seed ^ xrvp) ^ (yrvp ^ zrvp);
        hash *= HASH_MULTIPLIER;
        hash ^= hash >> (64 - N_GRADS_3D_EXPONENT + 2);
        int gi = (int)hash & ((N_GRADS_3D - 1) << 2);
        return GRADIENTS_3D[gi | 0] * dx + GRADIENTS_3D[gi | 1] * dy + GRADIENTS_3D[gi | 2] * dz;
    }

    inline int FastFloor(double x)
    {
        int xi = (int)x;
        return x < xi ? xi - 1 : xi;
    }

}
/*
 * Noise Evaluators
 */

/**
 * 2D  OpenSimplex2S/SuperSimplex noise base.
 */
float Noise2_UnskewedBase(int64_t seed, double xs, double ys)
{
    // Get base points and offsets.
    int xsb = FastFloor(xs), ysb = FastFloor(ys);
    float xi = (float)(xs - xsb), yi = (float)(ys - ysb);

    // Prime pre-multiplication for hash.
    int64_t xsbp = xsb * PRIME_X, ysbp = ysb * PRIME_Y;

    // Unskew.
    float t = (xi + yi) * (float)UNSKEW_2D;
    float dx0 = xi + t, dy0 = yi + t;

    // First vertex.
    float a0 = RSQUARED_2D - dx0 * dx0 - dy0 * dy0;
    float value = (a0 * a0) * (a0 * a0) * Grad(seed, xsbp, ysbp, dx0, dy0);
    
    // Second vertex.
    float a1 = (float)(2 * (1 + 2 * UNSKEW_2D) * (1 / UNSKEW_2D + 2)) * t + ((float)(-2 * (1 + 2 * UNSKEW_2D) * (1 + 2 * UNSKEW_2D)) + a0);
    float dx1 = dx0 - (float)(1 + 2 * UNSKEW_2D);
    float dy1 = dy0 - (float)(1 + 2 * UNSKEW_2D);
    value += (a1 * a1) * (a1 * a1) * Grad(seed, xsbp + PRIME_X, ysbp + PRIME_Y, dx1, dy1);

    // Third and fourth vertices.
    // Nested conditionals were faster than compact bit logic/arithmetic.
    float xmyi = xi - yi;
    if (t < UNSKEW_2D)
    {
        if (xi + xmyi > 1)
        {
            float dx2 = dx0 - (float)(3 * UNSKEW_2D + 2);
            float dy2 = dy0 - (float)(3 * UNSKEW_2D + 1);
            float a2 = RSQUARED_2D - dx2 * dx2 - dy2 * dy2;
            if (a2 > 0)
            {
                value += (a2 * a2) * (a2 * a2) * Grad(seed, xsbp + (PRIME_X << 1), ysbp + PRIME_Y, dx2, dy2);
            }
        }
        else
        {
            float dx2 = dx0 - (float)UNSKEW_2D;
            float dy2 = dy0 - (float)(UNSKEW_2D + 1);
            float a2 = RSQUARED_2D - dx2 * dx2 - dy2 * dy2;
            if (a2 > 0)
            {
                value += (a2 * a2) * (a2 * a2) * Grad(seed, xsbp, ysbp + PRIME_Y, dx2, dy2);
            }
        }

        if (yi - xmyi > 1)
        {
            float dx3 = dx0 - (float)(3 * UNSKEW_2D + 1);
            float dy3 = dy0 - (float)(3 * UNSKEW_2D + 2);
            float a3 = RSQUARED_2D - dx3 * dx3 - dy3 * dy3;
            if (a3 > 0)
            {
                value += (a3 * a3) * (a3 * a3) * Grad(seed, xsbp + PRIME_X, ysbp + (PRIME_Y << 1), dx3, dy3);
            }
        }
        else
        {
            float dx3 = dx0 - (float)(UNSKEW_2D + 1);
            float dy3 = dy0 - (float)UNSKEW_2D;
            float a3 = RSQUARED_2D - dx3 * dx3 - dy3 * dy3;
            if (a3 > 0)
            {
                value += (a3 * a3) * (a3 * a3) * Grad(seed, xsbp + PRIME_X, ysbp, dx3, dy3);
            }
        }
    }
    else
    {
        if (xi + xmyi < 0)
        {
            float dx2 = dx0 + (float)(1 + UNSKEW_2D);
            float dy2 = dy0 + (float)UNSKEW_2D;
            float a2 = RSQUARED_2D - dx2 * dx2 - dy2 * dy2;
            if (a2 > 0)
            {
                value += (a2 * a2) * (a2 * a2) * Grad(seed, xsbp - PRIME_X, ysbp, dx2, dy2);
            }
        }
        else
        {
            float dx2 = dx0 - (float)(UNSKEW_2D + 1);
            float dy2 = dy0 - (float)UNSKEW_2D;
            float a2 = RSQUARED_2D - dx2 * dx2 - dy2 * dy2;
            if (a2 > 0)
            {
                value += (a2 * a2) * (a2 * a2) * Grad(seed, xsbp + PRIME_X, ysbp, dx2, dy2);
            }
        }

        if (yi < xmyi)
        {
            float dx2 = dx0 + (float)UNSKEW_2D;
            float dy2 = dy0 + (float)(UNSKEW_2D + 1);
            float a2 = RSQUARED_2D - dx2 * dx2 - dy2 * dy2;
            if (a2 > 0)
            {
                value += (a2 * a2) * (a2 * a2) * Grad(seed, xsbp, ysbp - PRIME_Y, dx2, dy2);
            }
        }
        else
        {
            float dx2 = dx0 - (float)UNSKEW_2D;
            float dy2 = dy0 - (float)(UNSKEW_2D + 1);
            float a2 = RSQUARED_2D - dx2 * dx2 - dy2 * dy2;
            if (a2 > 0)
            {
                value += (a2 * a2) * (a2 * a2) * Grad(seed, xsbp, ysbp + PRIME_Y, dx2, dy2);
            }
        }
    }

    return value;
}

/**
 * 2D OpenSimplex2S/SuperSimplex noise, standard lattice orientation.
 */
float Noise2(int64_t seed, double x, double y)
{
    // Get points for A2* lattice
    double s = SKEW_2D * (x + y);
    double xs = x + s, ys = y + s;

    return Noise2_UnskewedBase(seed, xs, ys);
}

/**
 * 2D OpenSimplex2S/SuperSimplex noise, with Y pointing down the main diagonal.
 * Might be better for a 2D sandbox style game, where Y is vertical.
 * Probably slightly less optimal for heightmaps or continent maps,
 * unless your map is centered around an equator. It's a slight
 * difference, but the option is here to make it easy.
 */
float Noise2_ImproveX(int64_t seed, double x, double y)
{
    // Skew transform and rotation baked into one.
    double xx = x * ROOT2OVER2;
    double yy = y * (ROOT2OVER2 * (1 + 2 * SKEW_2D));

    return Noise2_UnskewedBase(seed, yy + xx, yy - xx);
}

/**
 * Generate overlapping cubic lattices for 3D Re-oriented BCC noise.
 * Lookup table implementation inspired by DigitalShadow.
 * It was actually faster to narrow down the points in the loop itself,
 * than to build up the index with enough info to isolate 8 points.
 */
float Noise3_UnrotatedBase(int64_t seed, double xr, double yr, double zr)
{
    // Get base points and offsets.
    int xrb = FastFloor(xr), yrb = FastFloor(yr), zrb = FastFloor(zr);
    float xi = (float)(xr - xrb), yi = (float)(yr - yrb), zi = (float)(zr - zrb);

    // Prime pre-multiplication for hash. Also flip seed for second lattice copy.
    int64_t xrbp = xrb * PRIME_X, yrbp = yrb * PRIME_Y, zrbp = zrb * PRIME_Z;
    int64_t seed2 = seed ^ -0x52D547B2E96ED629L;

    // -1 if positive, 0 if negative.
    int xNMask = (int)(-0.5f - xi), yNMask = (int)(-0.5f - yi), zNMask = (int)(-0.5f - zi);

    // First vertex.
    float x0 = xi + xNMask;
    float y0 = yi + yNMask;
    float z0 = zi + zNMask;
    float a0 = RSQUARED_3D - x0 * x0 - y0 * y0 - z0 * z0;
    float value = (a0 * a0) * (a0 * a0) * Grad(seed,
        xrbp + (xNMask & PRIME_X), yrbp + (yNMask & PRIME_Y), zrbp + (zNMask & PRIME_Z), x0, y0, z0);

    // Second vertex.
    float x1 = xi - 0.5f;
    float y1 = yi - 0.5f;
    float z1 = zi - 0.5f;
    float a1 = RSQUARED_3D - x1 * x1 - y1 * y1 - z1 * z1;
    value += (a1 * a1) * (a1 * a1) * Grad(seed2,
        xrbp + PRIME_X, yrbp + PRIME_Y, zrbp + PRIME_Z, x1, y1, z1);

    // Shortcuts for building the remaining falloffs.
    // Derived by subtracting the polynomials with the offsets plugged in.
    float xAFlipMask0 = ((xNMask | 1) << 1) * x1;
    float yAFlipMask0 = ((yNMask | 1) << 1) * y1;
    float zAFlipMask0 = ((zNMask | 1) << 1) * z1;
    float xAFlipMask1 = (-2 - (xNMask << 2)) * x1 - 1.0f;
    float yAFlipMask1 = (-2 - (yNMask << 2)) * y1 - 1.0f;
    float zAFlipMask1 = (-2 - (zNMask << 2)) * z1 - 1.0f;

    bool skip5 = false;
    float a2 = xAFlipMask0 + a0;
    if (a2 > 0)
    {
        float x2 = x0 - (xNMask | 1);
        float y2 = y0;
        float z2 = z0;
        value += (a2 * a2) * (a2 * a2) * Grad(seed,
            xrbp + (~xNMask & PRIME_X), yrbp + (yNMask & PRIME_Y), zrbp + (zNMask & PRIME_Z), x2, y2, z2);
    }
    else
    {
        float a3 = yAFlipMask0 + zAFlipMask0 + a0;
        if (a3 > 0)
        {
            float x3 = x0;
            float y3 = y0 - (yNMask | 1);
            float z3 = z0 - (zNMask | 1);
            value += (a3 * a3) * (a3 * a3) * Grad(seed,
                xrbp + (xNMask & PRIME_X), yrbp + (~yNMask & PRIME_Y), zrbp + (~zNMask & PRIME_Z), x3, y3, z3);
        }

        float a4 = xAFlipMask1 + a1;
        if (a4 > 0)
        {
            float x4 = (xNMask | 1) + x1;
            float y4 = y1;
            float z4 = z1;
            value += (a4 * a4) * (a4 * a4) * Grad(seed2,
                xrbp + (xNMask & (PRIME_X * 2)), yrbp + PRIME_Y, zrbp + PRIME_Z, x4, y4, z4);
            skip5 = true;
        }
    }

    bool skip9 = false;
    float a6 = yAFlipMask0 + a0;
    if (a6 > 0)
    {
        float x6 = x0;
        float y6 = y0 - (yNMask | 1);
        float z6 = z0;
        value += (a6 * a6) * (a6 * a6) * Grad(seed,
            xrbp + (xNMask & PRIME_X), yrbp + (~yNMask & PRIME_Y), zrbp + (zNMask & PRIME_Z), x6, y6, z6);
    }
    else
    {
        float a7 = xAFlipMask0 + zAFlipMask0 + a0;
        if (a7 > 0)
        {
            float x7 = x0 - (xNMask | 1);
            float y7 = y0;
            float z7 = z0 - (zNMask | 1);
            value += (a7 * a7) * (a7 * a7) * Grad(seed,
                xrbp + (~xNMask & PRIME_X), yrbp + (yNMask & PRIME_Y), zrbp + (~zNMask & PRIME_Z), x7, y7, z7);
        }

        float a8 = yAFlipMask1 + a1;
        if (a8 > 0)
        {
            float x8 = x1;
            float y8 = (yNMask | 1) + y1;
            float z8 = z1;
            value += (a8 * a8) * (a8 * a8) * Grad(seed2,
                xrbp + PRIME_X, yrbp + (yNMask & (PRIME_Y << 1)), zrbp + PRIME_Z, x8, y8, z8);
            skip9 = true;
        }
    }

    bool skipD = false;
    float aA = zAFlipMask0 + a0;
    if (aA > 0)
    {
        float xA = x0;
        float yA = y0;
        float zA = z0 - (zNMask | 1);
        value += (aA * aA) * (aA * aA) * Grad(seed,
            xrbp + (xNMask & PRIME_X), yrbp + (yNMask & PRIME_Y), zrbp + (~zNMask & PRIME_Z), xA, yA, zA);
    }
    else
    {
        float aB = xAFlipMask0 + yAFlipMask0 + a0;
        if (aB > 0)
        {
            float xB = x0 - (xNMask | 1);
            float yB = y0 - (yNMask | 1);
            float zB = z0;
            value += (aB * aB) * (aB * aB) * Grad(seed,
                xrbp + (~xNMask & PRIME_X), yrbp + (~yNMask & PRIME_Y), zrbp + (zNMask & PRIME_Z), xB, yB, zB);
        }

        float aC = zAFlipMask1 + a1;
        if (aC > 0)
        {
            float xC = x1;
            float yC = y1;
            float zC = (zNMask | 1) + z1;
            value += (aC * aC) * (aC * aC) * Grad(seed2,
                xrbp + PRIME_X, yrbp + PRIME_Y, zrbp + (zNMask & (PRIME_Z << 1)), xC, yC, zC);
            skipD = true;
        }
    }

    if (!skip5)
    {
        float a5 = yAFlipMask1 + zAFlipMask1 + a1;
        if (a5 > 0)
        {
            float x5 = x1;
            float y5 = (yNMask | 1) + y1;
            float z5 = (zNMask | 1) + z1;
            value += (a5 * a5) * (a5 * a5) * Grad(seed2,
                xrbp + PRIME_X, yrbp + (yNMask & (PRIME_Y << 1)), zrbp + (zNMask & (PRIME_Z << 1)), x5, y5, z5);
        }
    }

    if (!skip9)
    {
        float a9 = xAFlipMask1 + zAFlipMask1 + a1;
        if (a9 > 0)
        {
            float x9 = (xNMask | 1) + x1;
            float y9 = y1;
            float z9 = (zNMask | 1) + z1;
            value += (a9 * a9) * (a9 * a9) * Grad(seed2,
                xrbp + (xNMask & (PRIME_X * 2)), yrbp + PRIME_Y, zrbp + (zNMask & (PRIME_Z << 1)), x9, y9, z9);
        }
    }

    if (!skipD)
    {
        float aD = xAFlipMask1 + yAFlipMask1 + a1;
        if (aD > 0)
        {
            float xD = (xNMask | 1) + x1;
            float yD = (yNMask | 1) + y1;
            float zD = z1;
            value += (aD * aD) * (aD * aD) * Grad(seed2,
                xrbp + (xNMask & (PRIME_X << 1)), yrbp + (yNMask & (PRIME_Y << 1)), zrbp + PRIME_Z, xD, yD, zD);
        }
    }

    return value;
}

/**
 * 3D OpenSimplex2S/SuperSimplex noise, with better visual isotropy in (X, Y).
 * Recommended for 3D terrain and time-varied animations.
 * The Z coordinate should always be the "different" coordinate in whatever your use case is.
 * If Y is vertical in world coordinates, call Noise3_ImproveXZ(x, z, Y) or use Noise3_XZBeforeY.
 * If Z is vertical in world coordinates, call Noise3_ImproveXZ(x, y, Z).
 * For a time varied animation, call Noise3_ImproveXY(x, y, T).
 */
float Noise3_ImproveXY(int64_t seed, double x, double y, double z)
{
    // Re-orient the cubic lattices without skewing, so Z points up the main lattice diagonal,
    // and the planes formed by XY are moved far out of alignment with the cube faces.
    // Orthonormal rotation. Not a skew transform.
    double xy = x + y;
    double s2 = xy * ROTATE3_ORTHOGONALIZER;
    double zz = z * ROOT3OVER3;
    double xr = x + s2 + zz;
    double yr = y + s2 + zz;
    double zr = xy * -ROOT3OVER3 + zz;

    // Evaluate both lattices to form a BCC lattice.
    return Noise3_UnrotatedBase(seed, xr, yr, zr);
}

/**
 * 3D OpenSimplex2S/SuperSimplex noise, with better visual isotropy in (X, Z).
 * Recommended for 3D terrain and time-varied animations.
 * The Y coordinate should always be the "different" coordinate in whatever your use case is.
 * If Y is vertical in world coordinates, call Noise3_ImproveXZ(x, Y, z).
 * If Z is vertical in world coordinates, call Noise3_ImproveXZ(x, Z, y) or use Noise3_ImproveXY.
 * For a time varied animation, call Noise3_ImproveXZ(x, T, y) or use Noise3_ImproveXY.
 */
float Noise3_ImproveXZ(int64_t seed, double x, double y, double z)
{
    // Re-orient the cubic lattices without skewing, so Y points up the main lattice diagonal,
    // and the planes formed by XZ are moved far out of alignment with the cube faces.
    // Orthonormal rotation. Not a skew transform.
    double xz = x + z;
    double s2 = xz * -0.211324865405187;
    double yy = y * ROOT3OVER3;
    double xr = x + s2 + yy;
    double zr = z + s2 + yy;
    double yr = xz * -ROOT3OVER3 + yy;

    // Evaluate both lattices to form a BCC lattice.
    return Noise3_UnrotatedBase(seed, xr, yr, zr);
}

/**
 * 3D OpenSimplex2S/SuperSimplex noise, fallback rotation option
 * Use Noise3_ImproveXY or Noise3_ImproveXZ instead, wherever appropriate.
 * They have less diagonal bias. This function's best use is as a fallback.
 */
float Noise3_Fallback(int64_t seed, double x, double y, double z)
{
    // Re-orient the cubic lattices via rotation, to produce a familiar look.
    // Orthonormal rotation. Not a skew transform.
    double r = FALLBACK_ROTATE3 * (x + y + z);
    double xr = r - x, yr = r - y, zr = r - z;

    // Evaluate both lattices to form a BCC lattice.
    return Noise3_UnrotatedBase(seed, xr, yr, zr);
}

void initSimplex() {
    float grad2[] = {
        0.38268343236509f,   0.923879532511287f,
        0.923879532511287f,  0.38268343236509f,
        0.923879532511287f, -0.38268343236509f,
        0.38268343236509f,  -0.923879532511287f,
        -0.38268343236509f,  -0.923879532511287f,
        -0.923879532511287f, -0.38268343236509f,
        -0.923879532511287f,  0.38268343236509f,
        -0.38268343236509f,   0.923879532511287f,
        //-------------------------------------//
        0.130526192220052f,  0.99144486137381f,
        0.608761429008721f,  0.793353340291235f,
        0.793353340291235f,  0.608761429008721f,
        0.99144486137381f,   0.130526192220051f,
        0.99144486137381f,  -0.130526192220051f,
        0.793353340291235f, -0.60876142900872f,
        0.608761429008721f, -0.793353340291235f,
        0.130526192220052f, -0.99144486137381f,
        -0.130526192220052f, -0.99144486137381f,
        -0.608761429008721f, -0.793353340291235f,
        -0.793353340291235f, -0.608761429008721f,
        -0.99144486137381f,  -0.130526192220052f,
        -0.99144486137381f,   0.130526192220051f,
        -0.793353340291235f,  0.608761429008721f,
        -0.608761429008721f,  0.793353340291235f,
        -0.130526192220052f,  0.99144486137381f,
    };
    for (int i = 0; i < 48; i++)
        grad2[i] = (float)(grad2[i] / NORMALIZER_2D);

    for (int i = 0, j = 0; i < 256; i++, j++)
    {
        if (j == 48) j = 0;
        GRADIENTS_2D[i] = grad2[j];
    }

    float grad3[] = {
        2.22474487139f,       2.22474487139f,      -1.0f,                 0.0f,
        2.22474487139f,       2.22474487139f,       1.0f,                 0.0f,
        3.0862664687972017f,  1.1721513422464978f,  0.0f,                 0.0f,
        1.1721513422464978f,  3.0862664687972017f,  0.0f,                 0.0f,
        -2.22474487139f,       2.22474487139f,      -1.0f,                 0.0f,
        -2.22474487139f,       2.22474487139f,       1.0f,                 0.0f,
        -1.1721513422464978f,  3.0862664687972017f,  0.0f,                 0.0f,
        -3.0862664687972017f,  1.1721513422464978f,  0.0f,                 0.0f,
        -1.0f,                -2.22474487139f,      -2.22474487139f,       0.0f,
        1.0f,                -2.22474487139f,      -2.22474487139f,       0.0f,
        0.0f,                -3.0862664687972017f, -1.1721513422464978f,  0.0f,
        0.0f,                -1.1721513422464978f, -3.0862664687972017f,  0.0f,
        -1.0f,                -2.22474487139f,       2.22474487139f,       0.0f,
        1.0f,                -2.22474487139f,       2.22474487139f,       0.0f,
        0.0f,                -1.1721513422464978f,  3.0862664687972017f,  0.0f,
        0.0f,                -3.0862664687972017f,  1.1721513422464978f,  0.0f,
        //--------------------------------------------------------------------//
        -2.22474487139f,      -2.22474487139f,      -1.0f,                 0.0f,
        -2.22474487139f,      -2.22474487139f,       1.0f,                 0.0f,
        -3.0862664687972017f, -1.1721513422464978f,  0.0f,                 0.0f,
        -1.1721513422464978f, -3.0862664687972017f,  0.0f,                 0.0f,
        -2.22474487139f,      -1.0f,                -2.22474487139f,       0.0f,
        -2.22474487139f,       1.0f,                -2.22474487139f,       0.0f,
        -1.1721513422464978f,  0.0f,                -3.0862664687972017f,  0.0f,
        -3.0862664687972017f,  0.0f,                -1.1721513422464978f,  0.0f,
        -2.22474487139f,      -1.0f,                 2.22474487139f,       0.0f,
        -2.22474487139f,       1.0f,                 2.22474487139f,       0.0f,
        -3.0862664687972017f,  0.0f,                 1.1721513422464978f,  0.0f,
        -1.1721513422464978f,  0.0f,                 3.0862664687972017f,  0.0f,
        -1.0f,                 2.22474487139f,      -2.22474487139f,       0.0f,
        1.0f,                 2.22474487139f,      -2.22474487139f,       0.0f,
        0.0f,                 1.1721513422464978f, -3.0862664687972017f,  0.0f,
        0.0f,                 3.0862664687972017f, -1.1721513422464978f,  0.0f,
        -1.0f,                 2.22474487139f,       2.22474487139f,       0.0f,
        1.0f,                 2.22474487139f,       2.22474487139f,       0.0f,
        0.0f,                 3.0862664687972017f,  1.1721513422464978f,  0.0f,
        0.0f,                 1.1721513422464978f,  3.0862664687972017f,  0.0f,
        2.22474487139f,      -2.22474487139f,      -1.0f,                 0.0f,
        2.22474487139f,      -2.22474487139f,       1.0f,                 0.0f,
        1.1721513422464978f, -3.0862664687972017f,  0.0f,                 0.0f,
        3.0862664687972017f, -1.1721513422464978f,  0.0f,                 0.0f,
        2.22474487139f,      -1.0f,                -2.22474487139f,       0.0f,
        2.22474487139f,       1.0f,                -2.22474487139f,       0.0f,
        3.0862664687972017f,  0.0f,                -1.1721513422464978f,  0.0f,
        1.1721513422464978f,  0.0f,                -3.0862664687972017f,  0.0f,
        2.22474487139f,      -1.0f,                 2.22474487139f,       0.0f,
        2.22474487139f,       1.0f,                 2.22474487139f,       0.0f,
        1.1721513422464978f,  0.0f,                 3.0862664687972017f,  0.0f,
        3.0862664687972017f,  0.0f,                 1.1721513422464978f,  0.0f,
    };
    for (int i = 0; i < 192; i++)
        grad3[i] = (float)(grad3[i] / NORMALIZER_3D);

    for (int i = 0, j = 0; i < 1024; i++, j++)
    {
        if (j == 192) j = 0;
        GRADIENTS_3D[i] = grad3[j];
    }
}

