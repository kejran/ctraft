#pragma once 

#include "common.hpp"
#include "world.hpp"

/*
	3
      --- // 2.4 -> 2 -> 0.1
	2

	1
      --- // 0.5 -> 0 -> 1
	0
*/

// todo: ceiling is broken
inline float collideFloor(fvec3 pos, float height, float radius, bool &cz) {

	float tx = pos.x;
	float ty = pos.y;

	int l = fastFloor(tx - radius);
	int r = fastFloor(tx + radius);
	int b = fastFloor(ty - radius);
	int f = fastFloor(ty + radius);

	height -= 0.1f;

	float zz = pos.z;

	for (int _y = b; _y <= f; ++_y)
	 	for (int _x = l; _x <= r; ++_x) {
			int t = fastFloor(pos.z + height);
			if (tryGetBlock(_x, _y, t).isSolid()) { // todo: make isblocking or similar
				zz = t - height;
				cz = true;
			}
			int b = fastFloor(pos.z);
			if (tryGetBlock(_x, _y, b).isSolid()) {
				zz = b + 1;
				cz = true;
			}
		}
	return zz;
}

//	0	1	2	3
//	|	|	|	|
//  |   (*)	|	| // 1.2 -> 0.7 -> 0
// 	|	| (*)	| // 1.8 -> 2.3 -> 2

fvec2 collide2d(fvec2 position, int z, float radius, bool &cx, bool &cy) {

	cx = false; cy = false;
	float dx = 0, dy = 0;
	float absdx = 0, absdy = 0;

	// any distance closer than radius + half-block are a collision
	float colDist = radius + 0.5f;

	float tx = position.x;
	float ty = position.y;

	int l = fastFloor(tx - radius);
	int r = fastFloor(tx + radius);
	int b = fastFloor(ty - radius);
	int f = fastFloor(ty + radius);

	for (int _y = b; _y <= f; ++_y)
	 	for (int _x = l; _x <= r; ++_x)
			if (tryGetBlock(_x, _y, z).isSolid()) {
				// distances to centers of blocks
				float distX = _x - tx + 0.5f; // vector from player to centre of block
				float distY = _y - ty + 0.5f;
				float aDistX = fabsf(distX);
				float aDistY = fabsf(distY);
				if (aDistX < colDist && aDistX > absdx) {
					dx = distX;
					absdx = aDistX;
					cx = true;
				}
				if (aDistY < colDist && aDistY > absdy) {
					dy = distY;
					absdy = aDistY;
					cy = true;
				}
			}

	if (dx > 0) dx -= 0.01f; else dx += 0.01f;
	if (dy > 0) dy -= 0.01f; else dy += 0.01f;

	return {
		position.x + ((cx && (!cy || (absdx > absdy))) ? ((dx > 0) ? (dx - radius - 0.5f) : (dx + radius + 0.5f)) : 0),
		position.y + ((cy && (!cx || (absdy > absdx))) ? ((dy > 0) ? (dy - radius - 0.5f) : (dy + radius + 0.5f)) : 0)
	};
}

// todo dynamic height?
void moveAndCollide(fvec3 &position, fvec3 &velocity, float delta, float radius, int height) {
	fvec3 displacement {
		velocity.x * delta,
		velocity.y * delta,
		velocity.z * delta
	};

	int baseZ = fastFloor(position.z + 0.01f);

	float above = position.z - baseZ;
	if (above > 0.1f)
		++height;

	vec2 target { position.x + displacement.x, position.y + displacement.y };

	bool cx = false, cy = false, cz = false;

	for (int z = 0; z < height; ++z) {
		bool lcx, lcy;

		target = collide2d(target, z + baseZ, radius, lcx, lcy);

		if (lcx || lcy)
			target = collide2d(target, z + baseZ, radius, lcx, lcy);

		cx |= lcx;
		cy |= lcy;

	}
	position.x = target.x;
	position.y = target.y;
	position.z += displacement.z;
	position.z = collideFloor(position, height, radius, cz);

	if (cx)
		velocity.x = 0;
	if (cy)
		velocity.y = 0;
	if (cz)
		velocity.z = 0;

}

struct Player {
    fvec3 pos;
    fvec3 velocity;
    float rx, ry;

    void moveInput(float delta) {

        // rotation

        circlePosition cp;
        hidCstickRead(&cp);

        float dx = cp.dx * 0.01f;
        float dy = cp.dy * 0.01f;
        float t = 0.1f;

        if (dx > t || dx < -t) dx = dx > 0 ? dx - t : dx + t; else dx = 0;
        if (dy > t || dy < -t) dy = dy > 0 ? dy - t : dy + t; else dy = 0;

        rx += dx * delta * 4;
        ry -= dy * delta * 4;

        if (rx > M_PI_2) ry = M_PI_2;
        if (ry < -M_PI_2) ry = -M_PI_2;

    	// movement

        u32 kDown = hidKeysDown();
        u32 kHeld = hidKeysDown();

        float walkSpeed = (kHeld & KEY_B) ? 10 : 5;
        float walkAcc = 100;

        hidCircleRead(&cp);
        float angleM = atan2f(cp.dx, cp.dy) + rx;
        float mag2 = cp.dx*cp.dx + cp.dy*cp.dy;

        float targetX = 0;
        float targetY = 0;

        if (mag2 > 40*40) {
            if (mag2 > 140*140)
                mag2 = 140*140;

            float mag = sqrtf(mag2);
            mag -= 40;
            float targetMag = walkSpeed * mag / 100;

            targetX = sinf(angleM) * targetMag;
            targetY = cosf(angleM) * targetMag;
        }

        float vdx = targetX - velocity.x;
        float vdy = targetY - velocity.y;
        float maxDelta = walkAcc * delta;
        float vmag2 = vdx*vdx+vdy*vdy;

        if (maxDelta*maxDelta < vmag2) {
            float imag = maxDelta / sqrtf(vmag2);
            vdx *= imag;
            vdy *= imag;
        }
        
        velocity.x += vdx;
        velocity.y += vdy;

        if ((kDown & KEY_A) && velocity.z < 0.1f)
            // todo raycast down
            velocity.z = 4.8f; // adjust until it feels ok
        else
            velocity.z -= 9.81f * delta;

        if (velocity.z < -15)
            velocity.z = -15;

    	moveAndCollide(pos, velocity, delta, 0.4f, 2);
    }
};