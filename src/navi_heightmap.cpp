/* Copyright (C) 2013-2014 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of FreeFT.
 */

#include "navi_heightmap.h"
#include "grid.h"
#include <algorithm>


enum { max_levels = 16 };

NaviHeightmap::NaviHeightmap(const int2 &size) {
	DASSERT(size.x >= 0 && size.y >= 0);
	m_size = size;
	m_level_count = 0;
}

void NaviHeightmap::update(const vector<IBox> &walkable, const vector<IBox> &blockers) {
	m_level_count = 0;
	m_data.clear();

	PodArray<IBox> bboxes(walkable.size());
	for(int n = 0; n < bboxes.size(); n++) {
		IBox bbox = walkable[n];
		bbox.min = max(bbox.min, int3(0, 0, 0));
		bbox.max = min(bbox.max, int3(m_size.x, 255, m_size.y));
		bboxes[n] = bbox;
	}

	std::sort(bboxes.data(), bboxes.end(), [](const IBox &a, const IBox &b)
		{ return a.min.y == b.min.y? a.min.x == b.min.x?
			a.min.z < b.min.z : a.min.x < b.min.x : a.min.y < b.min.y; } );
		
	for(int n = 0; n < bboxes.size(); n++) {
		const IBox &bbox = bboxes[n];
		int min_y = bbox.min.y, max_y = bbox.max.y;

		for(int z = 0; z < bbox.depth(); z++)
			for(int x = 0; x < bbox.width(); x++) {
				int level = 0;
				int px = x + bbox.min.x;
				int pz = z + bbox.min.z;

				while(level < m_level_count) {
					int value = m_data[index(px, pz, level)];
					if(value == invalid_value || value >= min_y - 4)
						break;
					level++;
				}
				if(level == m_level_count) {
					if(level == max_levels)
						continue;
					addLevel();
				}

				m_data[index(px, pz, level)] = max_y;
			}
	}

	for(int n = 0; n < (int)blockers.size(); n++) {
		IBox blocker(
				max(blockers[n].min, int3(0, 0, 0)),
				min(blockers[n].max, int3(m_size.x, 255, m_size.y)));
		u8 min_y = max(0, blocker.min.y - 4), max_y = blocker.max.y;

		for(int z = 0; z < blocker.depth(); z++)
			for(int x = 0; x < blocker.width(); x++) {
				int level = 0;
				int px = x + blocker.min.x;
				int pz = z + blocker.min.z;

				while(level < m_level_count) {
					u8 &value = m_data[index(px, pz, level++)];
					if(value >= min_y && value <= max_y)
						value = invalid_value;
				}
			}
	}
}
	
Texture NaviHeightmap::toTexture(int level) const {
	Texture out(m_size.x, m_size.y);

	for(int y = 0; y < m_size.y; y++)
		for(int x = 0; x < m_size.x; x++) {
			short height = m_data[index(x, y, level)];
			bool ok = test(x, y, level, 3);
			out(x, y) = height == invalid_value? Color(255, 0, 0) : Color(ok? height : 255, height, height);
		}

	return out;
}

void NaviHeightmap::saveLevels() const {
	for(int n = 0; n < m_level_count; n++) {
		Texture tex = toTexture(n);
		char name[100];
		sprintf(name, "level%d.tga", n);
		Saver(name) << tex;
	}
}

void NaviHeightmap::printInfo() const {
	int level_mem = m_size.x * m_size.y / 1024;
	printf("NaviHeightmap(%dx%d):\n  levels: %d\n  memory: %dKB * %d = %dKB\n",
			m_size.x, m_size.y, m_level_count, level_mem, m_level_count, level_mem * m_level_count);
}

void NaviHeightmap::addLevel() {
	ASSERT(m_level_count < max_levels);
	m_level_count++;
	m_data.resize(m_size.x * m_size.y * m_level_count, invalid_value);
}

bool NaviHeightmap::test(int x, int y, int level, int extents) const {
	enum { max_extents = 8};

	DASSERT(level >= 0 && level < m_level_count);
	DASSERT(extents <= max_extents);

	if(x < 0 || y < 0 || x + extents > m_size.x || y + extents > m_size.y)
		return false;
	if(m_data[index(x, y, level)] == invalid_value)
		return false;

	u8 heights[max_extents][max_extents];
	heights[0][0] = m_data[index(x, y, level)];

	for(int ty = 0; ty < extents; ty++) {
		u8 prev = heights[ty == 0? 0 : ty - 1][0];
		for(int tx = 0; tx < extents; tx++) {
			u8 height = m_data[index(x + tx, y + ty, level)];
			if(fwk::abs(height - prev) > 1 || height == invalid_value)
				for(int l = 0; l < m_level_count; l++) {
					height = m_data[index(x + tx, y + ty, l)];
					if(height >= 0 && fwk::abs(height - prev) <= 1)
						break;
				}
			if(fwk::abs(height - prev) > 1 || height == invalid_value)
				return false;
			heights[ty][tx] = prev = height;
		}
	}

	extents--;
	for(int ty = 0; ty < extents; ty++)
		for(int tx = 0; tx < extents; tx++)
			if(	fwk::abs(heights[ty][tx] - heights[ty + 1][tx + 0]) > 1 ||
				fwk::abs(heights[ty][tx] - heights[ty + 1][tx + 1]) > 1)
				return false;

	return true;
}

