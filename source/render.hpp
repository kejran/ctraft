#pragma once

#include "common.hpp"

void render(fvec3 &playerPos, float rx, float ry, vec3<s32> *focus, float depthSlider);
void renderLoading();

void renderInit(bool bottomScreen);
void renderExit();
