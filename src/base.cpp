/* Copyright (C) 2013 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of FTremake.

   FTremake is free software; you can redistribute it and/or modify it under the terms of the
   GNU General Public License as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   FTremake is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
   even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along with this program.
   If not, see http://www.gnu.org/licenses/ . */

#include "base.h"
#include <cmath>
#include <cstdlib>

#ifdef WIN32
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

