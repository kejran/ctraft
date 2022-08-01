#pragma once

#include <array>
#include <cstdint>
#include <limits>

namespace dcs {

constexpr int PERMUTATION_TABLE_SIZE_EXPONENT = 8;

constexpr int PSIZE = 1 << PERMUTATION_TABLE_SIZE_EXPONENT;
constexpr int PMASK = PSIZE - 1;

constexpr float ROOT3 = 1.7320508075688772f;
constexpr float ROOT3OVER3 = 0.577350269189626f;

constexpr float RGRADIENTS_3D[] = {
    11.966937965122034, 44.66122049686031, 0.0, 0,
    -25.78516767619953, 6.90911485553875, -37.75210564132156, 0,
    25.78516767619953, -6.90911485553875, 37.75210564132156, 0,
    -11.966937965122034, -44.66122049686031, 0.0, 0,
    18.87605282066078, 18.87605282066078, -37.75210564132156, 0,
    -32.69428253173828, 32.69428253173828, 0.0, 0,
    32.69428253173828, -32.69428253173828, 0.0, 0,
    -18.87605282066078, -18.87605282066078, 37.75210564132156, 0,
    44.66122049686031, 11.966937965122034, 0.0, 0,
    -6.90911485553875, 25.78516767619953, 37.75210564132156, 0,
    6.90911485553875, -25.78516767619953, -37.75210564132156, 0,
    -44.66122049686031, -11.966937965122034, 0.0, 0,
    11.966937965122034, 44.66122049686031, 0.0, 0,
    -25.78516767619953, 6.90911485553875, -37.75210564132156, 0,
    25.78516767619953, -6.90911485553875, 37.75210564132156, 0,
    -11.966937965122034, -44.66122049686031, 0.0, 0,
    18.87605282066078, 18.87605282066078, -37.75210564132156, 0,
    -32.69428253173828, 32.69428253173828, 0.0, 0,
    32.69428253173828, -32.69428253173828, 0.0, 0,
    -18.87605282066078, -18.87605282066078, 37.75210564132156, 0,
    44.66122049686031, 11.966937965122034, 0.0, 0,
    -6.90911485553875, 25.78516767619953, 37.75210564132156, 0,
    6.90911485553875, -25.78516767619953, -37.75210564132156, 0,
    -44.66122049686031, -11.966937965122034, 0.0, 0,
    11.966937965122034, 44.66122049686031, 0.0, 0,
    -25.78516767619953, 6.90911485553875, -37.75210564132156, 0,
    25.78516767619953, -6.90911485553875, 37.75210564132156, 0,
    -11.966937965122034, -44.66122049686031, 0.0, 0,
    18.87605282066078, 18.87605282066078, -37.75210564132156, 0,
    -32.69428253173828, 32.69428253173828, 0.0, 0,
    32.69428253173828, -32.69428253173828, 0.0, 0,
    -18.87605282066078, -18.87605282066078, 37.75210564132156, 0,
    44.66122049686031, 11.966937965122034, 0.0, 0,
    -6.90911485553875, 25.78516767619953, 37.75210564132156, 0,
    6.90911485553875, -25.78516767619953, -37.75210564132156, 0,
    -44.66122049686031, -11.966937965122034, 0.0, 0,
    11.966937965122034, 44.66122049686031, 0.0, 0,
    -25.78516767619953, 6.90911485553875, -37.75210564132156, 0,
    25.78516767619953, -6.90911485553875, 37.75210564132156, 0,
    -11.966937965122034, -44.66122049686031, 0.0, 0,
    18.87605282066078, 18.87605282066078, -37.75210564132156, 0,
    -32.69428253173828, 32.69428253173828, 0.0, 0,
    32.69428253173828, -32.69428253173828, 0.0, 0,
    -18.87605282066078, -18.87605282066078, 37.75210564132156, 0,
    44.66122049686031, 11.966937965122034, 0.0, 0,
    -6.90911485553875, 25.78516767619953, 37.75210564132156, 0,
    6.90911485553875, -25.78516767619953, -37.75210564132156, 0,
    -44.66122049686031, -11.966937965122034, 0.0, 0,
    11.966937965122034, 44.66122049686031, 0.0, 0,
    -25.78516767619953, 6.90911485553875, -37.75210564132156, 0,
    25.78516767619953, -6.90911485553875, 37.75210564132156, 0,
    -11.966937965122034, -44.66122049686031, 0.0, 0,
    18.87605282066078, 18.87605282066078, -37.75210564132156, 0,
    -32.69428253173828, 32.69428253173828, 0.0, 0,
    32.69428253173828, -32.69428253173828, 0.0, 0,
    -18.87605282066078, -18.87605282066078, 37.75210564132156, 0,
    44.66122049686031, 11.966937965122034, 0.0, 0,
    -6.90911485553875, 25.78516767619953, 37.75210564132156, 0,
    6.90911485553875, -25.78516767619953, -37.75210564132156, 0,
    -44.66122049686031, -11.966937965122034, 0.0, 0,
    44.66122049686031, 11.966937965122034, 0.0, 0,
    -25.78516767619953, 6.90911485553875, -37.75210564132156, 0,
    -6.90911485553875, 25.78516767619953, 37.75210564132156, 0,
    -11.966937965122034, -44.66122049686031, 0.0, 0
};

struct Seed {

    std::array<short, PSIZE> perm;

    int gradIndex(int xrv, int yrv, int zrv) {
        return perm[perm[perm[xrv & PMASK] ^ (yrv & PMASK)] ^ (zrv & PMASK)] & 0xFC;
    }

    Seed(int64_t seed) {
        std::array<short, PSIZE> source;
        for (short i = 0; i < PSIZE; i++)
            source[i] = i;
        for (int i = PSIZE - 1; i >= 0; i--) {
            seed = seed * 6364136223846793005L + 1442695040888963407L;
            int r = (int)((seed + 31) % (i + 1));
            if (r < 0)
                r += (i + 1);
            perm[i] = source[r];
            source[r] = source[i];
        }
    }
};

inline int fastRound(float x) {
    return (int)(x >= 0 ? x + 0.5 : x - 0.5);
}

struct Generator {

private:

    float xs, zs, xzr;
    float localMinY, localMaxY;

    float g0b, g1b, g2b, g3b;
    float g0y, g1y, g2y, g3y;
    float y0, y1, y2, y3;
    float xzFalloff0, xzFalloff1, xzFalloff2, xzFalloff3;

    Seed &seed;

public:

    Generator(Seed &seed): seed(seed) {
        localMinY = std::numeric_limits<float>::infinity();
    }

    void reset(float x, float z) {

        // Domain rotation, start
        float xz = x + z;
        float s2 = xz * -0.211324865405187f;
        this->xs = x + s2;
        this->zs = z + s2;
        this->xzr = xz * -ROOT3OVER3;

        localMinY = std::numeric_limits<float>::infinity();
    }

    float sample(float y) {

        if (y < localMinY || y > localMaxY) {

            // Domain rotation, finish
            float yy = y * ROOT3OVER3;
            float xr = xs + yy;
            float zr = zs + yy;
            float yr = xzr + yy;

            // Grid base and bounds
            int xrb = fastRound(xr), yrb = fastRound(yr), zrb = fastRound(zr);
            float xri = xr - xrb, yri = yr - yrb, zri = zr - zrb;

            // -1 if positive, 1 if negative
            int xNSign = (int)(-1.0 - xri) | 1;
            int yNSign = (int)(-1.0 - yri) | 1;
            int zNSign = (int)(-1.0 - zri) | 1;

            // Absolute values, using the above
            float axri = xNSign * -xri;
            float ayri = yNSign * -yri;
            float azri = zNSign * -zri;

            for (int l = 0; ; l++)
            {
                // Get delta x and z in world-space coordinates away from grid base.
                // This way we can avoid re-computing the stuff for X and Z, and
                // only update for Y as Y updates.
                float s2i = (xri + zri) * -0.211324865405187f - (yri * ROOT3OVER3);
                float xi0 = xri + s2i;
                float zi0 = zri + s2i;
                float yi0 = (xri + zri) * ROOT3OVER3 + (yri * ROOT3OVER3);

                // Closest vertex on this half-grid
                int gi = seed.gradIndex(xrb, yrb, zrb);

                // Its gradient and falloff data
                g0b = RGRADIENTS_3D[gi | 0] * xi0 + RGRADIENTS_3D[gi | 1] * zi0;
                g0y = RGRADIENTS_3D[gi | 2];

                xzFalloff0 = 0.6f - xi0 * xi0 - zi0 * zi0;
                y0 = y - yi0;

                // Find second-closest vertex
                if (axri >= ayri && axri >= azri) {
                    gi = seed.gradIndex(xrb - xNSign, yrb, zrb);

                    // Position of this vertex, in world-space coordinates
                    float xi = xi0 + xNSign * 0.788675134594813f;
                    float zi = zi0 + xNSign * -0.211324865405187f;
                    float yi = yi0 + xNSign * ROOT3OVER3;

                    // Gradient and falloff data
                    g1b = RGRADIENTS_3D[gi | 0] * xi + RGRADIENTS_3D[gi | 1] * zi;
                    g1y = RGRADIENTS_3D[gi | 2];

                    xzFalloff1 = 0.6f - xi * xi - zi * zi;
                    y1 = y - yi;

                    // One of the planar boundaries where the state of the noise changes lies between the two
                    // diagonally-opposite vertices on the other half-grid which connect to that half-grid's
                    // closest vertex via edges perpendicular to the edge formed on this half-grid.
                    // We store to localMinY, but we don't yet know if it's the min or the max.
                    // We'll resolve that later.
                    localMinY = y - (ROOT3 / 2) * (yri + zri);

                } else if (ayri > axri && ayri >= azri) {

                    gi = seed.gradIndex(xrb, yrb - yNSign, zrb);
                    float xi = xi0 + yNSign * -ROOT3OVER3;
                    float zi = zi0 + yNSign * -ROOT3OVER3;
                    float yi = yi0 + yNSign * ROOT3OVER3;

                    g1b = RGRADIENTS_3D[gi | 0] * xi + RGRADIENTS_3D[gi | 1] * zi;
                    g1y = RGRADIENTS_3D[gi | 2];

                    xzFalloff1 = 0.6f - xi * xi - zi * zi;
                    y1 = y - yi;
                    localMinY = y - (ROOT3 / 2) * (xri + zri);

                } else {

                    gi = seed.gradIndex(xrb, yrb, zrb - zNSign);
                    float xi = xi0 + zNSign * -0.211324865405187f;
                    float zi = zi0 + zNSign * 0.788675134594813f;
                    float yi = yi0 + zNSign * ROOT3OVER3;

                    g1b = RGRADIENTS_3D[gi | 0] * xi + RGRADIENTS_3D[gi | 1] * zi;
                    g1y = RGRADIENTS_3D[gi | 2];

                    xzFalloff1 = 0.6f - xi * xi - zi * zi;
                    y1 = y - yi;
                    localMinY = y - (ROOT3 / 2) * (xri + yri);
                }

                if (l == 1) break;

                // Flip everyhing to reference the closest vertex on the other half-grid
                axri = 0.5f - axri;
                ayri = 0.5f - ayri;
                azri = 0.5f - azri;
                xri = xNSign * axri;
                yri = yNSign * ayri;
                zri = zNSign * azri;
                xrb -= (xNSign >> 1) - (PSIZE / 2);
                yrb -= (yNSign >> 1) - (PSIZE / 2);
                zrb -= (zNSign >> 1) - (PSIZE / 2);
                xNSign = -xNSign;
                yNSign = -yNSign;
                zNSign = -zNSign;

                // Shift these over to vacate them for the other iteration.
                g2b = g0b;
                g3b = g1b;
                g2y = g0y;
                g3y = g1y;
                y2 = y0;
                y3 = y1;
                xzFalloff2 = xzFalloff0;
                xzFalloff3 = xzFalloff1;
                localMaxY = localMinY;
            }

            // It isn't guaranteed which will be the min or max, so correct that here.
            if (localMinY > localMaxY) {

                float temp = localMaxY;
                localMaxY = localMinY;
                localMinY = temp;
            }
        }
        float value = 0;

        float dy0 = y - y0;
        float dy0Sq = dy0 * dy0;

        if (xzFalloff0 > dy0Sq) {
            float falloff = xzFalloff0 - dy0Sq;
            falloff *= falloff;
            value = falloff * falloff * (g0b + g0y * dy0);
        }

        float dy1 = y - y1;
        float dy1Sq = dy1 * dy1;

        if (xzFalloff1 > dy1Sq) {
            float falloff = xzFalloff1 - dy1Sq;
            falloff *= falloff;
            value += falloff * falloff * (g1b + g1y * dy1);
        }

        float dy2 = y - y2;
        float dy2Sq = dy2 * dy2;

        if (xzFalloff2 > dy2Sq) {
            float falloff = xzFalloff2 - dy2Sq;
            falloff *= falloff;
            value += falloff * falloff * (g2b + g2y * dy2);
        }

        float dy3 = y - y3;
        float dy3Sq = dy3 * dy3;

        if (xzFalloff3 > dy3Sq) {
            float falloff = xzFalloff3 - dy3Sq;
            falloff *= falloff;
            value += falloff * falloff * (g3b + g3y * dy3);
        }

        return value;
    }

};

struct DrvGenerator {

private:

    float xs, zs, xzr;
    float localMinY, localMaxY;

    float g0b, g0x, g0y, g0z;
    float g1b, g1x, g1y, g1z;
    float g2b, g2x, g2y, g2z;
    float g3b, g3x, g3y, g3z;
    float dx0, dz0, dx1, dz1;
    float dx2, dz2, dx3, dz3;

    float y0, y1, y2, y3;
    float xzFalloff0, xzFalloff1, xzFalloff2, xzFalloff3;

    Seed &seed;

public:

    DrvGenerator(Seed &seed): seed(seed) {
        localMinY = std::numeric_limits<float>::infinity();
    }

    void reset(float x, float z) {

        // Domain rotation, start
        float xz = x + z;
        float s2 = xz * -0.211324865405187f;
        this->xs = x + s2;
        this->zs = z + s2;
        this->xzr = xz * -ROOT3OVER3;

        localMinY = std::numeric_limits<float>::infinity();
    }

    void sampleDrv(std::array<float, 4> &values, float y) {

        if (y < localMinY || y > localMaxY) {

            // Domain rotation, finish
            float yy = y * ROOT3OVER3;
            float xr = xs + yy;
            float zr = zs + yy;
            float yr = xzr + yy;

            // Grid base and bounds
            int xrb = fastRound(xr), yrb = fastRound(yr), zrb = fastRound(zr);
            float xri = xr - xrb, yri = yr - yrb, zri = zr - zrb;

            // -1 if positive, 1 if negative
            int xNSign = (int)(-1.0 - xri) | 1;
            int yNSign = (int)(-1.0 - yri) | 1;
            int zNSign = (int)(-1.0 - zri) | 1;

            // Absolute values, using the above
            float axri = xNSign * -xri;
            float ayri = yNSign * -yri;
            float azri = zNSign * -zri;

            for (int l = 0; ; l++)
            {
                // Get delta x and z in world-space coordinates away from grid base.
                // This way we can avoid re-computing the stuff for X and Z, and
                // only update for Y as Y updates.
                float s2i = (xri + zri) * -0.211324865405187f - (yri * ROOT3OVER3);
                float xi0 = xri + s2i;
                float zi0 = zri + s2i;
                float yi0 = (xri + zri) * ROOT3OVER3 + (yri * ROOT3OVER3);
                this->dx0 = xi0; this->dz0 = zi0;

                // Closest vertex on this half-grid
                int gi = seed.gradIndex(xrb, yrb, zrb);

                // Its gradient and falloff data
                g0b = RGRADIENTS_3D[gi | 0] * xi0 + RGRADIENTS_3D[gi | 1] * zi0;
                g0x = RGRADIENTS_3D[gi | 0];
                g0z = RGRADIENTS_3D[gi | 1];
                g0y = RGRADIENTS_3D[gi | 2];

                xzFalloff0 = 0.6f - xi0 * xi0 - zi0 * zi0;
                y0 = y - yi0;

                // Find second-closest vertex
                if (axri >= ayri && axri >= azri) {
                    gi = seed.gradIndex(xrb - xNSign, yrb, zrb);

                    // Position of this vertex, in world-space coordinates
                    float xi = xi0 + xNSign * 0.788675134594813f, zi = zi0 + xNSign * -0.211324865405187f, yi = yi0 + xNSign * ROOT3OVER3;
                    this->dx1 = xi; this->dz1 = zi;

                    // Gradient and falloff data
                    g1b = RGRADIENTS_3D[gi | 0] * xi + RGRADIENTS_3D[gi | 1] * zi;
                    g1x = RGRADIENTS_3D[gi | 0];
                    g1z = RGRADIENTS_3D[gi | 1];
                    g1y = RGRADIENTS_3D[gi | 2];

                    xzFalloff1 = 0.6f - xi * xi - zi * zi;
                    y1 = y - yi;

                    // One of the planar boundaries where the state of the seed changes lies between the two
                    // diagonally-opposite vertices on the other half-grid which connect to that half-grid's
                    // closest vertex via edges perpendicular to the edge formed on this half-grid.
                    // We store to localMinY, but we don't yet know if it's the min or the max.
                    // We'll resolve that later.
                    localMinY = y - (ROOT3 / 2) * (yri + zri);
                } else if (ayri > axri && ayri >= azri) {
                    gi = seed.gradIndex(xrb, yrb - yNSign, zrb);
                    float xi = xi0 + yNSign * -ROOT3OVER3, zi = zi0 + yNSign * -ROOT3OVER3, yi = yi0 + yNSign * ROOT3OVER3;
                    this->dx1 = xi; this->dz1 = zi;

                    g1b = RGRADIENTS_3D[gi | 0] * xi + RGRADIENTS_3D[gi | 1] * zi;
                    g1x = RGRADIENTS_3D[gi | 0];
                    g1z = RGRADIENTS_3D[gi | 1];
                    g1y = RGRADIENTS_3D[gi | 2];

                    xzFalloff1 = 0.6f - xi * xi - zi * zi;
                    y1 = y - yi;
                    localMinY = y - (ROOT3 / 2) * (xri + zri);
                } else {
                    gi = seed.gradIndex(xrb, yrb, zrb - zNSign);
                    float xi = xi0 + zNSign * -0.211324865405187f, zi = zi0 + zNSign * 0.788675134594813f, yi = yi0 + zNSign * ROOT3OVER3;
                    this->dx1 = xi; this->dz1 = zi;

                    g1b = RGRADIENTS_3D[gi | 0] * xi + RGRADIENTS_3D[gi | 1] * zi;
                    g1x = RGRADIENTS_3D[gi | 0];
                    g1z = RGRADIENTS_3D[gi | 1];
                    g1y = RGRADIENTS_3D[gi | 2];

                    xzFalloff1 = 0.6f - xi * xi - zi * zi;
                    y1 = y - yi;
                    localMinY = y - (ROOT3 / 2) * (xri + yri);
                }

                if (l == 1) break;

                // Flip everyhing to reference the closest vertex on the other half-grid
                axri = 0.5f - axri;
                ayri = 0.5f - ayri;
                azri = 0.5f - azri;
                xri = xNSign * axri;
                yri = yNSign * ayri;
                zri = zNSign * azri;
                xrb -= (xNSign >> 1) - (PSIZE / 2);
                yrb -= (yNSign >> 1) - (PSIZE / 2);
                zrb -= (zNSign >> 1) - (PSIZE / 2);
                xNSign = -xNSign;
                yNSign = -yNSign;
                zNSign = -zNSign;

                // Shift these over to vacate them for the other iteration.
                g2b = g0b;
                g3b = g1b;
                g2x = g0x;
                g3x = g1x;
                g2z = g0z;
                g3z = g1z;
                g2y = g0y;
                g3y = g1y;
                dx2 = dx0;
                dx3 = dx1;
                dz2 = dz0;
                dz3 = dz1;
                y2 = y0;
                y3 = y1;
                xzFalloff2 = xzFalloff0;
                xzFalloff3 = xzFalloff1;
                localMaxY = localMinY;
            }

            // It isn't guaranteed which will be the min or max, so correct that here.
            if (localMinY > localMaxY) {
                float temp = localMaxY;
                localMaxY = localMinY;
                localMinY = temp;
            }
        }
        float value = 0, dx = 0, dy = 0, dz = 0;

        float dy0 = y - y0;
        float dy0Sq = dy0 * dy0;
        if (xzFalloff0 > dy0Sq) {
            float falloff = xzFalloff0 - dy0Sq;
            float falloffSq = falloff * falloff;
            float ramp = g0b + g0y * dy0;

            value = falloffSq * falloffSq * ramp;
            dx = (g0x * falloff - 8 * ramp * dx0) * (falloff * falloffSq);
            dy = (g0y * falloff - 8 * ramp * dy0) * (falloff * falloffSq);
            dz = (g0z * falloff - 8 * ramp * dz0) * (falloff * falloffSq);
        }

        float dy1 = y - y1;
        float dy1Sq = dy1 * dy1;
        if (xzFalloff1 > dy1Sq) {
            float falloff = xzFalloff1 - dy1Sq;
            float falloffSq = falloff * falloff;
            float ramp = g1b + g1y * dy1;

            value += falloffSq * falloffSq * ramp;
            dx += (g1x * falloff - 8 * ramp * dx1) * (falloff * falloffSq);
            dy += (g1y * falloff - 8 * ramp * dy1) * (falloff * falloffSq);
            dz += (g1z * falloff - 8 * ramp * dz1) * (falloff * falloffSq);
        }

        float dy2 = y - y2;
        float dy2Sq = dy2 * dy2;
        if (xzFalloff2 > dy2Sq) {
            float falloff = xzFalloff2 - dy2Sq;
            float falloffSq = falloff * falloff;
            float ramp = g2b + g2y * dy2;

            value += falloffSq * falloffSq * ramp;
            dx += (g2x * falloff - 8 * ramp * dx2) * (falloff * falloffSq);
            dy += (g2y * falloff - 8 * ramp * dy2) * (falloff * falloffSq);
            dz += (g2z * falloff - 8 * ramp * dz2) * (falloff * falloffSq);
        }

        float dy3 = y - y3;
        float dy3Sq = dy3 * dy3;
        if (xzFalloff3 > dy3Sq) {
            float falloff = xzFalloff3 - dy3Sq;
            float falloffSq = falloff * falloff;
            float ramp = g3b + g3y * dy3;

            value += falloffSq * falloffSq * ramp;
            dx += (g3x * falloff - 8 * ramp * dx3) * (falloff * falloffSq);
            dy += (g3y * falloff - 8 * ramp * dy3) * (falloff * falloffSq);
            dz += (g3z * falloff - 8 * ramp * dz3) * (falloff * falloffSq);
        }

        values[0] = value;
        values[1] = dx;
        values[2] = dy;
        values[3] = dz;
    }

    float sample(float y) {

        if (y < localMinY || y > localMaxY) {

            // Domain rotation, finish
            float yy = y * ROOT3OVER3;
            float xr = xs + yy;
            float zr = zs + yy;
            float yr = xzr + yy;

            // Grid base and bounds
            int xrb = fastRound(xr), yrb = fastRound(yr), zrb = fastRound(zr);
            float xri = xr - xrb, yri = yr - yrb, zri = zr - zrb;

            // -1 if positive, 1 if negative
            int xNSign = (int)(-1.0 - xri) | 1;
            int yNSign = (int)(-1.0 - yri) | 1;
            int zNSign = (int)(-1.0 - zri) | 1;

            // Absolute values, using the above
            float axri = xNSign * -xri;
            float ayri = yNSign * -yri;
            float azri = zNSign * -zri;

            for (int l = 0; ; l++)
            {
                // Get delta x and z in world-space coordinates away from grid base.
                // This way we can avoid re-computing the stuff for X and Z, and
                // only update for Y as Y updates.
                float s2i = (xri + zri) * -0.211324865405187f - (yri * ROOT3OVER3);
                float xi0 = xri + s2i;
                float zi0 = zri + s2i;
                float yi0 = (xri + zri) * ROOT3OVER3 + (yri * ROOT3OVER3);
                this->dx0 = xi0; this->dz0 = zi0;

                // Closest vertex on this half-grid
                int gi = seed.gradIndex(xrb, yrb, zrb);

                // Its gradient and falloff data
                g0b = RGRADIENTS_3D[gi | 0] * xi0 + RGRADIENTS_3D[gi | 1] * zi0;
                g0x = RGRADIENTS_3D[gi | 0]; g0z = RGRADIENTS_3D[gi | 1]; g0y = RGRADIENTS_3D[gi | 2];
                xzFalloff0 = 0.6f - xi0 * xi0 - zi0 * zi0;
                y0 = y - yi0;

                // Find second-closest vertex
                if (axri >= ayri && axri >= azri) {
                    gi = seed.gradIndex(xrb - xNSign, yrb, zrb);

                    // Position of this vertex, in world-space coordinates
                    float xi = xi0 + xNSign * 0.788675134594813f, zi = zi0 + xNSign * -0.211324865405187f, yi = yi0 + xNSign * ROOT3OVER3;
                    this->dx1 = xi; this->dz1 = zi;

                    // Gradient and falloff data
                    g1b = RGRADIENTS_3D[gi | 0] * xi + RGRADIENTS_3D[gi | 1] * zi;
                    g1x = RGRADIENTS_3D[gi | 0]; g1z = RGRADIENTS_3D[gi | 1]; g1y = RGRADIENTS_3D[gi | 2];
                    xzFalloff1 = 0.6f - xi * xi - zi * zi;
                    y1 = y - yi;

                    // One of the planar boundaries where the state of the seed changes lies between the two
                    // diagonally-opposite vertices on the other half-grid which connect to that half-grid's
                    // closest vertex via edges perpendicular to the edge formed on this half-grid.
                    // We store to localMinY, but we don't yet know if it's the min or the max.
                    // We'll resolve that later.
                    localMinY = y - (ROOT3 / 2) * (yri + zri);
                } else if (ayri > axri && ayri >= azri) {
                    gi = seed.gradIndex(xrb, yrb - yNSign, zrb);
                    float xi = xi0 + yNSign * -ROOT3OVER3, zi = zi0 + yNSign * -ROOT3OVER3, yi = yi0 + yNSign * ROOT3OVER3;
                    this->dx1 = xi; this->dz1 = zi;
                    g1b = RGRADIENTS_3D[gi | 0] * xi + RGRADIENTS_3D[gi | 1] * zi;
                    g1x = RGRADIENTS_3D[gi | 0]; g1z = RGRADIENTS_3D[gi | 1]; g1y = RGRADIENTS_3D[gi | 2];
                    xzFalloff1 = 0.6f - xi * xi - zi * zi;
                    y1 = y - yi;
                    localMinY = y - (ROOT3 / 2) * (xri + zri);
                } else {
                    gi = seed.gradIndex(xrb, yrb, zrb - zNSign);
                    float xi = xi0 + zNSign * -0.211324865405187f, zi = zi0 + zNSign * 0.788675134594813f, yi = yi0 + zNSign * ROOT3OVER3;
                    this->dx1 = xi; this->dz1 = zi;
                    g1b = RGRADIENTS_3D[gi | 0] * xi + RGRADIENTS_3D[gi | 1] * zi;
                    g1x = RGRADIENTS_3D[gi | 0]; g1z = RGRADIENTS_3D[gi | 1]; g1y = RGRADIENTS_3D[gi | 2];
                    xzFalloff1 = 0.6f - xi * xi - zi * zi;
                    y1 = y - yi;
                    localMinY = y - (ROOT3 / 2) * (xri + yri);
                }

                if (l == 1) break;

                // Flip everyhing to reference the closest vertex on the other half-grid
                axri = 0.5f - axri;
                ayri = 0.5f - ayri;
                azri = 0.5f - azri;
                xri = xNSign * axri;
                yri = yNSign * ayri;
                zri = zNSign * azri;
                xrb -= (xNSign >> 1) - (PSIZE / 2);
                yrb -= (yNSign >> 1) - (PSIZE / 2);
                zrb -= (zNSign >> 1) - (PSIZE / 2);
                xNSign = -xNSign;
                yNSign = -yNSign;
                zNSign = -zNSign;

                // Shift these over to vacate them for the other iteration.
                g2b = g0b;
                g3b = g1b;
                g2x = g0x;
                g3x = g1x;
                g2z = g0z;
                g3z = g1z;
                g2y = g0y;
                g3y = g1y;
                dx2 = dx0;
                dx3 = dx1;
                dz2 = dz0;
                dz3 = dz1;
                y2 = y0;
                y3 = y1;
                xzFalloff2 = xzFalloff0;
                xzFalloff3 = xzFalloff1;
                localMaxY = localMinY;
            }

            // It isn't guaranteed which will be the min or max, so correct that here.
            if (localMinY > localMaxY) {
                float temp = localMaxY;
                localMaxY = localMinY;
                localMinY = temp;
            }
        }
        float value = 0;

        float dy0 = y - y0;
        float dy0Sq = dy0 * dy0;
        if (xzFalloff0 > dy0Sq) {
            float falloff = xzFalloff0 - dy0Sq;
            float falloffSq = falloff * falloff;
            float ramp = g0b + g0y * dy0;
            value = falloffSq * falloffSq * ramp;
        }

        float dy1 = y - y1;
        float dy1Sq = dy1 * dy1;
        if (xzFalloff1 > dy1Sq) {
            float falloff = xzFalloff1 - dy1Sq;
            float falloffSq = falloff * falloff;
            float ramp = g1b + g1y * dy1;
            value += falloffSq * falloffSq * ramp;
        }

        float dy2 = y - y2;
        float dy2Sq = dy2 * dy2;
        if (xzFalloff2 > dy2Sq) {
            float falloff = xzFalloff2 - dy2Sq;
            float falloffSq = falloff * falloff;
            float ramp = g2b + g2y * dy2;
            value += falloffSq * falloffSq * ramp;
        }

        float dy3 = y - y3;
        float dy3Sq = dy3 * dy3;
        if (xzFalloff3 > dy3Sq) {
            float falloff = xzFalloff3 - dy3Sq;
            float falloffSq = falloff * falloff;
            float ramp = g3b + g3y * dy3;
            value += falloffSq * falloffSq * ramp;
        }

        return value;
    }
};

}