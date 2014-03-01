/* Copyright (C) 2013-2014 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of FreeFT.
 */

#include "base.h"
#include <cmath>
#include <cstdlib>

const char* strcasestr(const char *a, const char *b) {
	DASSERT(a && b);

	while(*a) {
		if(strcasecmp(a, b) == 0)
			return a;
		a++;
	}

	return nullptr;
}

int strcasecmp(const char *a, const char *b) {
	DASSERT(a && b);
	while(*a && tolower(*a) == tolower(*b)) {
		a++;
		b++;
	}

	return *a < *b? -1 : *a == *b? 0 : 1;
}

bool toBool(const char *input) {
	CString str(input);
	if(caseEqual(str, "true"))
		return true;
	if(caseEqual(str, "false"))
		return false;
	return toInt(input) != 0;
}

int toInt(const char *str) {
	DASSERT(str);
	int out;
	if(sscanf(str, "%d", &out) != 1)
		THROW("Error while converting string \"%s\" to int", str);
	return out;
}

static void toFloat(const char *input, int count, float *out) {
	DASSERT(input);
	if(sscanf(input, "%f %f %f %f" + (4 - count) * 3, out + 0, out + 1, out + 2, out + 3) != count)
		THROW("Error while converting string \"%s\" to float%d", input, count);
}

float toFloat(const char *input) {
	float out;
	toFloat(input, 1, &out);
	return out;
}

float2 toFloat2(const char *input) {
	float2 out;
	toFloat(input, 2, &out.x);
	return out;
}

float3 toFloat3(const char *input) {
	float3 out;
	toFloat(input, 3, &out.x);
	return out;
}


float4 toFloat4(const char *input) {
	float4 out;
	toFloat(input, 4, &out.x);
	return out;
}

uint toFlags(const char *input, const char **strings, int num_strings, uint first_flag) {
	const char *iptr = input;

	uint out_value = 0;
	while(*iptr) {
		const char *next_space = strchr(iptr, ' ');
		int len = next_space? next_space - iptr : strlen(iptr);

		bool found = false;
		for(int e = 0; e < num_strings; e++)
			if(strncmp(iptr, strings[e], len) == 0 && strings[e][len] == 0) {
				out_value |= first_flag << e;
				found = true;
				break;
			}

		if(!found) {
			char flags[1024], *ptr = flags;
			for(int i = 0; i < num_strings; i++)
				ptr += snprintf(ptr, sizeof(flags) - (ptr - flags), "%s ", strings[i]);
			if(num_strings)
				ptr[-1] = 0;

			THROW("Error while converting string \"%s\" to flags (%s)", input, flags);
		}

		if(!next_space)
			break;
		iptr = next_space + 1;
	}

	return out_value;
}

int fromString(const char *str, const char **strings, int count) {
	DASSERT(str);
	for(int n = 0; n < count; n++)
		if(strcmp(str, strings[n]) == 0)
			return n;
	
	char tstrings[1024], *ptr = tstrings;
	for(int i = 0; i < count; i++)
		ptr += snprintf(ptr, sizeof(tstrings) - (ptr - tstrings), "%s ", strings[i]);
	if(count)
		ptr[-1] = 0;

	THROW("Error while finding string \"%s\" in array (%s)", str, tstrings);
	return -1;
}

BitVector::BitVector(int size) :m_data((size + base_size - 1) / base_size), m_size(size) { }

void BitVector::resize(int new_size, bool clear_value) {
	PodArray<u32> new_data(new_size);
	memcpy(new_data.data(), m_data.data(), sizeof(base_type) * min(new_size, m_data.size()));
	if(new_data.size() > m_data.size())
		memset(new_data.data() + m_data.size(), clear_value? 0xff : 0, (new_data.size() - m_data.size()) * sizeof(base_type));
	m_data.swap(new_data);
}
	
void BitVector::clear(bool value) {
	memset(m_data.data(), value? 0xff : 0, m_data.size() * sizeof(base_type));
}

#ifdef _WIN32
void sincosf(float rad, float *s, float *c) {
	DASSERT(s && c);
	*s = sin(rad);
	*c = cos(rad);
}
#endif

template <class T>
bool areOverlapping(const Box<T> &a, const Box<T> &b) {
	return	(b.min.x < a.max.x && a.min.x < b.max.x) &&
			(b.min.y < a.max.y && a.min.y < b.max.y) &&
			(b.min.z < a.max.z && a.min.z < b.max.z);
}

template bool areOverlapping<int3>(const Box<int3>&, const Box<int3>&);
template bool areOverlapping<float3>(const Box<float3>&, const Box<float3>&);

float vectorToAngle(const float2 &normalized_vec) {
	float ang = acos(normalized_vec.x);
	return normalized_vec.y < 0? 2.0f * constant::pi -ang : ang;
}

const float2 angleToVector(float radians) {
	float s, c;
	sincosf(radians, &s, &c);
	return float2(c, s);
}

const float2 rotateVector(const float2 &vec, float radians) {
	float s, c;
	sincosf(radians, &s, &c);
	return float2(c * vec.x - s * vec.y, c * vec.y + s * vec.x);
}

const Box<int3> enclosingIBox(const Box<float3> &fbox) {
	return Box<int3>(floorf(fbox.min.x), floorf(fbox.min.y), floorf(fbox.min.z),
					  ceilf(fbox.max.x),  ceilf(fbox.max.y),  ceilf(fbox.max.z));
};

const Box<float3> rotateY(const Box<float3> &box, const float3 &origin, float angle) {
	float3 corners[8];
	box.getCorners(corners);
	float2 xz_origin = origin.xz();

	for(int n = 0; n < COUNTOF(corners); n++)
		corners[n] = asXZY(rotateVector(corners[n].xz() - xz_origin, angle) + xz_origin, corners[n].y);
	
	Box<float3> out(corners[0], corners[0]);
	for(int n = 0; n < COUNTOF(corners); n++) {
		out.min = min(out.min, corners[n]);
		out.max = max(out.max, corners[n]);
	}

	return out;
}

float g_FloatParam[16];

/*
typedef float Matrix3[3][3];

bool inverse(const Matrix3 &mat) {
	float3 out[3];

    out[0].x = mat[1][1] * mat[2][2] - mat[1][2] * mat[2][1];
    out[0].y = mat[0][2] * mat[2][1] - mat[0][1] * mat[2][2];
    out[0].z = mat[0][1] * mat[1][2] - mat[0][2] * mat[1][1];
    out[1].x = mat[1][2] * mat[2][0] - mat[1][0] * mat[2][2];
    out[1].y = mat[0][0] * mat[2][2] - mat[0][2] * mat[2][0];
    out[1].z = mat[0][2] * mat[1][0] - mat[0][0] * mat[1][2];
    out[2].x = mat[1][0] * mat[2][1] - mat[1][1] * mat[2][0];
    out[2].y = mat[0][1] * mat[2][0] - mat[0][0] * mat[2][1];
    out[2].z = mat[0][0] * mat[1][1] - mat[0][1] * mat[1][0];

	float det = mat[0][0] * out[0].x + mat[0][1] * out[1].x + mat[0][2] * out[2].x;
	//TODO what if det close to 0?
	float idet = 1.0f / det;

	out[0] = out[0] * idet;
	out[1] = out[1] * idet;
	out[2] = out[2] * idet;

	for(int i = 0; i < 3; i++) {
		printf("%f %f %f\n", out[i].x, out[i].y, out[i].z);
	}
	exit(0);
	return 1;
}

float mat[3][3] = {
	{6, 3, 7},
	{0, -7, 6},
	{-6, 3, 7}
};*/

/* World To Screen Matrix:
 * 			    | 6  3  7|
 * 		*	    | 0 -7  6|
 * 			    |-6  3  7|
 *
 * [WX WY WZ]   [SX SY SZ]
 *
 */

const float2 worldToScreen(const float3 &pos) {
	return float2(	6.0f * (pos.x - pos.z),
					3.0f * (pos.x + pos.z) - 7.0f * pos.y);
				//	7.0f * (pos.x + pos.z) + 6.0f * pos.y);
}

const int2 worldToScreen(const int3 &pos) {
	return int2(	6 * (pos.x - pos.z),
					3 * (pos.x + pos.z) - 7 * pos.y);
				//	7 * (pos.x + pos.z) - 6 * pos.y);
}

const float2 screenToWorld(const float2 &pos) {
	float x = pos.x * (1.0f / 12.0f);
	float y = pos.y * (1.0f / 6.0f);

	return float2(y + x, y - x);
}

const int2 screenToWorld(const int2 &pos) {
	int x = pos.x / 12;
	int y = pos.y / 6;

	return int2(y + x, y - x);
}


float intersection(const Ray &ray, const Box<float3> &box) {
	//TODO: check if works correctly for (+/-)INF
	float3 inv_dir = ray.invDir();
	float3 origin = ray.origin();

	float l1   = inv_dir.x * (box.min.x - origin.x);
	float l2   = inv_dir.x * (box.max.x - origin.x);
	float lmin = min(l1, l2);
	float lmax = max(l1, l2);

	l1   = inv_dir.y * (box.min.y - origin.y);
	l2   = inv_dir.y * (box.max.y - origin.y);
	lmin = max(min(l1, l2), lmin);
	lmax = min(max(l1, l2), lmax);

	l1   = inv_dir.z * (box.min.z - origin.z);
	l2   = inv_dir.z * (box.max.z - origin.z);
	lmin = max(min(l1, l2), lmin);
	lmax = min(max(l1, l2), lmax);

	return lmin <= lmax? lmin : constant::inf;
}

float intersection(const Segment &segment, const Box<float3> &box) {
	//TODO: check if works correctly for (+/-)INF
	float3 inv_dir = segment.invDir();
	float3 origin = segment.origin();

	float l1   = inv_dir.x * (box.min.x - origin.x);
	float l2   = inv_dir.x * (box.max.x - origin.x);
	float lmin = min(l1, l2);
	float lmax = max(l1, l2);

	l1   = inv_dir.y * (box.min.y - origin.y);
	l2   = inv_dir.y * (box.max.y - origin.y);
	lmin = max(min(l1, l2), lmin);
	lmax = min(max(l1, l2), lmax);

	l1   = inv_dir.z * (box.min.z - origin.z);
	l2   = inv_dir.z * (box.max.z - origin.z);
	lmin = max(min(l1, l2), lmin);
	lmax = min(max(l1, l2), lmax);
	lmin = max(lmin, segment.min);
	lmax = min(lmax, segment.max);

	return lmin < lmax? lmin : constant::inf;
}

const Ray screenRay(const int2 &screen_pos) {
	float3 origin = asXZ(screenToWorld((float2)screen_pos));
	float3 dir = float3(-1.0f / 6.0f, -1.0f / 7.0f, -1.0f / 6.0f);
	return Ray(origin, dir / length(dir));
}

float dot(const float2 &a, const float2 &b) { return a.x * b.x + a.y * b.y; }
float dot(const float3 &a, const float3 &b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
float dot(const float4 &a, const float4 &b) { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }

float lengthSq(const float2 &v) { return dot(v, v); }
float lengthSq(const float3 &v) { return dot(v, v); }
float distanceSq(const float3 &a, const float3 &b) { return lengthSq(a - b); }
float distanceSq(const float2 &a, const float2 &b) { return lengthSq(a - b); }

float length(const float2 &v) { return sqrt(lengthSq(v)); }
float length(const float3 &v) { return sqrt(lengthSq(v)); }
float distance(const float3 &a, const float3 &b) { return sqrt(distanceSq(a, b)); }
float distance(const float2 &a, const float2 &b) { return sqrt(distanceSq(a, b)); }

float angleDistance(float a, float b) {
	float diff = fabs(a - b);
	return min(diff, constant::pi * 2.0f - diff);
}

float blendAngles(float initial, float target, float step) {
	if(initial != target) {
		float new_ang1 = initial + step, new_ang2 = initial - step;
		if(new_ang1 < 0.0f)
			new_ang1 += constant::pi * 2.0f;
		if(new_ang2 < 0.0f)
			new_ang2 += constant::pi * 2.0f;
		float new_angle = angleDistance(new_ang1, target) < angleDistance(new_ang2, target)?
				new_ang1 : new_ang2;
		if(angleDistance(initial, target) < step)
			new_angle = target;
		return new_angle;
	}

	return initial;
}

bool areAdjacent(const IRect &a, const IRect &b) {
	if(b.min.x < a.max.x && a.min.x < b.max.x)
		return a.max.y == b.min.y || a.min.y == b.max.y;
	if(b.min.y < a.max.y && a.min.y < b.max.y)
		return a.max.x == b.min.x || a.min.x == b.max.x;
	return false;
}

float distanceSq(const Rect<float2> &a, const Rect<float2> &b) {
	float2 p1 = clamp(b.center(), a.min, a.max);
	float2 p2 = clamp(p1, b.min, b.max);
	return distanceSq(p1, p2);
}

MoveVector::MoveVector(const int2 &start, const int2 &end) {
	int2 diff = end - start;
	vec.x = diff.x < 0? -1 : diff.x > 0? 1 : 0;
	vec.y = diff.y < 0? -1 : diff.y > 0? 1 : 0;
	dx = abs(diff.x);
	dy = abs(diff.y);
	ddiag = min(dx, dy);
	dx -= ddiag;
	dy -= ddiag;
}
MoveVector::MoveVector() :vec(0, 0), dx(0), dy(0), ddiag(0) { }

/*

#include "../libs/lz4/lz4.h"
#include "../libs/lz4/lz4hc.h"

enum { max_size = 16 * 1024 * 1024 };

//TODO: testme
void compress(const PodArray<char> &in, PodArray<char> &out, bool hc) {
	ASSERT(in.size() <= max_size);

	PodArray<char> temp(LZ4_compressBound(in.size()));
	int size = (hc? LZ4_compressHC : LZ4_compress)(in.data(), temp.data(), in.size());
	//int size = in.size(); memcpy(temp.data(), in.data(), size);

	out.resize(size + 4);
	memcpy(out.data(), &size, 4);
	memcpy(out.data() + 4, temp.data(), size);
}

void decompress(const PodArray<char> &in, PodArray<char> &out) {
	i32 size;
	ASSERT(in.size() >= 4);
	memcpy(&size, in.data(), 4);
	ASSERT(size <= max_size);
	out.resize(size);

//	memcpy(out.data(), in.data() + 4, size);
//	int decompressed_bytes = size;
	int decompressed_bytes = LZ4_uncompress(in.data() + 4, out.data(), size);
	ASSERT(decompressed_bytes == size);
}

*/
