#pragma once

// Editorial-collage fragment transforms: pure, GL-free math so it can be
// unit-tested without a window. The source texture is never rotated or
// re-rendered; a fragment is one textured quad whose four UV corners are
// permuted (quarter-turn), then swapped (flips).

#include <cstdint>

enum class QuarterTurn : uint8_t { Deg0,
	Deg90,
	Deg180,
	Deg270 };

struct CollageVec2 {
	float x = 0, y = 0;
};

// Corner order everywhere: 0=TL, 1=TR, 2=BR, 3=BL of the *destination* quad.
// c[i] is the source UV sampled at destination corner i.
struct CollageUvQuad {
	CollageVec2 c[4];
};

inline bool isOddTurn(QuarterTurn t) { return t == QuarterTurn::Deg90 || t == QuarterTurn::Deg270; }

// Source crop (u0,v0)-(u1,v1) -> UVs for the destination quad. Rotation is
// clockwise as seen on screen; flips are applied afterwards in screen space.
inline CollageUvQuad makeCollageUvQuad(float u0, float v0, float u1, float v1,
	QuarterTurn turn, bool flipX, bool flipY) {
	const CollageVec2 src[4] = { { u0, v0 }, { u1, v0 }, { u1, v1 }, { u0, v1 } }; // TL TR BR BL
	const int k = static_cast<int>(turn);
	CollageUvQuad q;
	for (int i = 0; i < 4; i++) {
		q.c[i] = src[(i - k + 4) % 4]; // 90 CW: dest TL shows source BL
	}
	if (flipX) {
		CollageVec2 t0 = q.c[0], t3 = q.c[3];
		q.c[0] = q.c[1];
		q.c[1] = t0;
		q.c[3] = q.c[2];
		q.c[2] = t3;
	}
	if (flipY) {
		CollageVec2 t0 = q.c[0], t1 = q.c[1];
		q.c[0] = q.c[3];
		q.c[3] = t0;
		q.c[1] = q.c[2];
		q.c[2] = t1;
	}
	return q;
}

// UV at normalized destination position (s,t), (0,0)=TL, (1,1)=BR. Exact for
// an axis-aligned crop under any quarter-turn/flip, which lets a partial
// wipe reveal or a circular mask sample the same image without distortion.
inline CollageVec2 bilerpCollageUv(const CollageUvQuad & q, float s, float t) {
	auto lerp = [](const CollageVec2 & a, const CollageVec2 & b, float k) {
		return CollageVec2 { a.x + (b.x - a.x) * k, a.y + (b.y - a.y) * k };
	};
	CollageVec2 top = lerp(q.c[0], q.c[1], s);
	CollageVec2 bot = lerp(q.c[3], q.c[2], s);
	return lerp(top, bot, t);
}
