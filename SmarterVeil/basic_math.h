#pragma once

#include "basic_math_types.h"

//TODO(fran): no templates

template<typename T>
internal T absolute_value(T v) {
	T res = v >= T(0) ? v : -v;
	return res;
}

template<typename T>
internal T distance(T a, T b) {
	return absolute_value(a - b);
}

template<typename T>
internal T maximum(T a, T b) {
	return (a > b) ? a : b;
}

template<typename T>
internal T minimum(T a, T b) {
	return (a < b) ? a : b;
}

template<typename T>
internal T safe_ratioN(T dividend, T divisor, T n) {
	T res;
	if (divisor != 0) res = dividend / divisor;
	else res = n;
	return res;
}

template<typename T>
internal T safe_ratio1(T dividend, T divisor) {
	return safe_ratioN(dividend, divisor, T(1));
}

template<typename T>
internal T safe_ratio0(T dividend, T divisor) {
	return safe_ratioN(dividend, divisor, T(0));
}

f32 squared(f32 n) {
	f32 res = n * n;
	return res;
}

f32 clamp(f32 min, f32 n, f32 max) {
	f32 res = n;
	if (res < min) res = min;
	else if (res > max) res = max;
	return res;
}

f32 clamp01(f32 n) {
	return clamp(0.f, n, 1.f);
}

internal f32 lerp(f32 n1, f32 t, f32 n2) {
	return (1.f - t) * n1 + t * n2;
}

internal f32 percentage_between(f32 min, f32 val, f32 max) {
	f32 res = (val - min) / (max - min);
	return res;
}

internal i32 round_to_i32(f32 n) {
	//Reference: https://stackoverflow.com/questions/9695329/c-how-to-round-a-double-to-an-int
	//TODO(fran): intrinsic
	i32 res = (i32)(n + .5f - (n < 0));
	return res;
}

internal u32 round_to_u32(f32 n) {
	assert(n >= 0.f);
	//TODO(fran): intrinsic
	u32 res = (u32)(n + .5f);
	return res;
}

#include <random>//TODO(fran): PCG random numbers
#ifdef DEBUG_BUILD
struct _random_seeder {
	_random_seeder() { srand(86132584); }
} static __random_seeder;
#endif
internal f32 random_between(f32 min, f32 max) {
	f32 res = lerp(min, (f32)rand() / (f32)RAND_MAX, max);
	return res;
}

internal f32 random01() {
	return random_between(0.f, 1.f);
}

//TODO(fran): leave rectangle math alone until we decide on the coordinate system
internal bool test_pt_rc(v2 p, const rc2& rc) {
	bool res = p.y >= rc.top && p.y < rc.bottom() && p.x >= rc.left && p.x < rc.right();
	return res;
}

v2 hadamard(v2 a, v2 b) {
	//Per component product
	v2 res;
	res.x = a.x * b.x;
	res.y = a.y * b.y;
	return res;
}

internal rc2 add_rc(rc2 r1, rc2 r2) {
	rc2 res =
	{
		.x = minimum(r1.x,r2.x),
		.y = minimum(r1.y,r2.y),
		.w = (r1.right()>r2.right())? distance(r1.right(),res.x) : distance(r2.right(),res.x),
		.h = (r1.bottom() > r2.bottom()) ? distance(r1.bottom(),res.y) : distance(r2.bottom(),res.y),
	};
	return res;
}

internal rc2 translate_rc(rc2 rc, f32 dX, f32 dY) {
	rc2 res =
	{
		.x = rc.x + dX,
		.y = rc.y + dY,
		.w = rc.w,
		.h = rc.h,
	};
	return res;
}

internal rc2 inflate_rc(rc2 rc, f32 constant) {
	rc2 res =
	{
		.x = rc.x - constant * .5f,
		.y = rc.y - constant * .5f,
		.w = rc.w + constant,
		.h = rc.h + constant,
	};
	return res;
}

internal rc2 round_rc(rc2 rc) {
	rc2 res =
	{
		.x = (f32)round_to_i32(rc.x),
		.y = (f32)round_to_i32(rc.y),
		.w = (f32)round_to_i32(rc.w),
		.h = (f32)round_to_i32(rc.h),
	};
	return res;
}

internal rc2 scalefromcenter_rc(rc2 rc, v2 percentage) {
	rc2 res = rc_center_diameter(rc.center(), hadamard(rc.wh, percentage));
	return res;
}

internal rc2 scalefromcenter_rc(rc2 rc, f32 percentageX, f32 percentageY) {
	return scalefromcenter_rc(rc, { percentageX, percentageY });
}

internal rc2 scalefromcenterconst_rc(rc2 rc, sz2 constant) {
	rc2 res = rc_center_diameter(rc.center(), rc.wh + constant);
	return res;
}

internal rc2 scalefromcenterconst_rc(rc2 rc, f32 constantX, f32 constantY) {
	return scalefromcenterconst_rc(rc, { constantX, constantY });
}