#pragma once

#include "common.hpp"

float collideFloor(fvec3 pos, float height, float radius, bool &cz);

fvec2 collide2d(fvec2 position, int z, float radius, bool &cx, bool &cy);

void moveAndCollide(fvec3 &position, fvec3 &velocity, float delta, float radius, int height);

struct Player {
	fvec3 pos;
	fvec3 velocity;
	float rx, ry;

	void moveInput(float delta);
};