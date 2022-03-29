#pragma once
//TODO(fran): mark this functions as internal?
union v2 {
    struct { f32 x, y; };
    struct { f32 u, v; };
    struct { f32 w, h; }; //TODO(fran): define a new type, maybe d2 (dimension 2)
    f32 comp[2];

    v2& operator+=(const v2& rhs); v2& operator-=(const v2& rhs); v2& operator*=(f32 rhs_scalar);
};

v2 operator*(v2 v, f32 scalar) { v2 res; res.x = v.x * scalar; res.y = v.y * scalar; return res; }

//NOTE: C++ dissapoints again this time with C++20 where they did _not_ add two way operators, but they did add the magical spaceship <=> operator, so great!
v2 operator*(f32 scalar, v2 v) { v2 res = v * scalar; return res; }

v2 operator/(v2 v, f32 scalar) { v2 res; res.x = v.x / scalar; res.y = v.y / scalar; return res; }

v2 operator-(v2 a, v2 b) { v2 res; res.x = a.x - b.x; res.y = a.y - b.y; return res; }

v2 operator+(v2 a, v2 b) { v2 res; res.x = a.x + b.x; res.y = a.y + b.y; return res; }

v2 operator-(v2 v) { v2 res; res.x = -v.x; res.y = -v.y; return res; }

v2& v2::operator-=(const v2& rhs) { *this = *this - rhs; return *this; }

//TODO(fran): do I need to use const& instead of a simple copy?
v2& v2::operator+=(const v2& rhs) { *this = *this + rhs; return *this; }

v2& v2::operator*=(f32 rhs_scalar) { *this = *this * rhs_scalar; return *this; }

v2 v2_from(i32 x, i32 y) { v2 res; res.x = (f32)x; res.y = (f32)y; return res; }

typedef v2 sz2; //2D size (width & height) //TODO(fran): maybe 'd2' (dimension 2) is better?

struct v2i {
    i32 x, y;

    v2i& operator+=(const v2i& rhs); v2i& operator-=(const v2i& rhs); v2i& operator*=(f32 rhs_scalar);
};

v2i operator*(v2i v, f32 scalar) { v2i res; res.x = v.x * scalar; res.y = v.y * scalar; return res; }

//NOTE: C++ dissapoints again this time with C++20 where they did _not_ add two way operators, but they did add the magical spaceship <=> operator, so great!
v2i operator*(f32 scalar, v2i v) { v2i res = v * scalar; return res; }

v2i operator/(v2i v, f32 scalar) { v2i res; res.x = v.x / scalar; res.y = v.y / scalar; return res; }

v2i operator-(v2i a, v2i b) { v2i res; res.x = a.x - b.x; res.y = a.y - b.y; return res; }

v2i operator+(v2i a, v2i b) { v2i res; res.x = a.x + b.x; res.y = a.y + b.y; return res; }

v2i operator-(v2i v) { v2i res; res.x = -v.x; res.y = -v.y; return res; }

v2i& v2i::operator-=(const v2i& rhs) { *this = *this - rhs; return *this; }

//TODO(fran): do I need to use const& instead of a simple copy?
v2i& v2i::operator+=(const v2i& rhs) { *this = *this + rhs; return *this; }

v2i& v2i::operator*=(f32 rhs_scalar) { *this = *this * rhs_scalar; return *this; }


union v3 {
	struct { f32 x, y, z; };
	struct { v2 xy; f32 _; };
	struct { f32 r, g, b; };
	f32 comp[3];

	v3& operator+=(const v3& rhs); v3& operator-=(const v3& rhs); v3& operator*=(f32 rhs_scalar);
};

v3 operator/(v3 v, f32 scalar) { v3 res; res.x = v.x / scalar; res.y = v.y / scalar; res.z = v.z / scalar; return res; }

v3 operator-(v3 v) { v3 res; res.x = -v.x; res.y = -v.y; res.z = -v.z; return res; }

v3 operator*(v3 v, f32 scalar) { v3 res; res.x = v.x * scalar; res.y = v.y * scalar; res.z = v.z * scalar; return res; }

v3 operator*(f32 scalar, v3 v) { v3 res = v * scalar; return res; }

v3 operator-(v3 a, v3 b) { v3 res; res.x = a.x - b.x; res.y = a.y - b.y; res.z = a.z - b.z; return res; }

v3 operator+(v3 a, v3 b) { v3 res; res.x = a.x + b.x; res.y = a.y + b.y; res.z = a.z + b.z; return res; }

v3& v3::operator-=(const v3& rhs) { *this = *this - rhs; return *this; }

//TODO(fran): do I need to use const& instead of a simple copy?
v3& v3::operator+=(const v3& rhs) { *this = *this + rhs; return *this; }

v3& v3::operator*=(f32 rhs_scalar) { *this = *this * rhs_scalar; return *this; }

v3 v3_from(v2 v, f32 z) { v3 res; res.x = v.x; res.y = v.y; res.z = z; return res; }


union v4 {
    struct { f32 x, y, z, w; };
    struct { v3 xyz; f32 _; };
    struct { v2 xy; f32 __, ___; };
    struct { f32 r, g, b, a; };
    struct { v3 rgb; f32 ____; };
    f32 comp[4];

    v4& operator*=(f32 rhs_scalar);
};

v4 operator*(v4 v, f32 scalar) { v4 res; res.x = v.x * scalar; res.y = v.y * scalar; res.z = v.z * scalar; res.w = v.w * scalar; return res; }

v4 operator*(f32 scalar, v4 v) { v4 res = v * scalar; return res; }

v4 operator+(v4 a, v4 b) { v4 res; res.x = a.x + b.x; res.y = a.y + b.y; res.z = a.z + b.z; res.w = a.w + b.w; return res; }

v4& v4::operator*=(f32 rhs_scalar) { *this = *this * rhs_scalar; return *this; }

v4 operator/(v4 v, f32 scalar) { v4 res; res.x = v.x / scalar; res.y = v.y / scalar; res.z = v.z / scalar; res.w = v.w / scalar; return res; }

v4 V4(v3 v, f32 alpha) { v4 res = { .x = v.x, .y = v.y, .z = v.z, .w = alpha }; return res; }

struct rc2 { //IMPORTANT: this rectangles are meant for use in UI code, and as such have top-left for position, since they should be used in coordinate systems with X going right and Y going _down_ //TODO(fran): correct this if we end up using other coordinate system
	union {
		struct { f32 x, y, w, h; };
		struct { f32 left, top, __, ___; };
		struct { v2 xy, wh; };
		struct { v2 p, _; };
		f32 comp[4];
	};

	f32 right() const { return left + w; }
	f32 bottom() const { return top + h; }
	v2 bottom_right() const { return { right(), bottom() }; }
	v2 bottom_left() const { return { left, bottom() }; }
	v2 top_right() const { return { right(), top }; }
	f32 centerX() const { return left + w / 2; }
	f32 centerY() const { return top + h / 2; }
	v2 center() const { return v2{ centerX(), centerY() }; }
};

rc2 rc2_from(v2 p, v2 wh) { rc2 res; res.x = p.x; res.y = p.y, res.w = wh.x; res.h = wh.y; return res; }

rc2 rc_center_radius(v2 center, v2 radius) {
	rc2 res;
	res.xy = center - radius;
	res.wh = radius * 2.f;
	return res;
}

rc2 rc_center_diameter(v2 center, v2 diameter) {
	return rc_center_radius(center, diameter * .5f);
}