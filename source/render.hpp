#pragma once

#include "common.hpp"

void render(fvec3 &playerPos, float rx, float ry, vec3<s32> *focus, float depthSlider);
void renderInit();
void renderExit();