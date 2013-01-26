/* Copyright (C) 2013 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of FreeFT.

   FreeFT is free software; you can redistribute it and/or modify it under the terms of the
   GNU General Public License as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   FreeFT is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
   even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along with this program.
   If not, see http://www.gnu.org/licenses/ . */

#include "game/tile.h"
#include "gfx/device.h"
#include "gfx/scene_renderer.h"
#include <cstring>
#include <GL/gl.h>
#include <algorithm>

using namespace gfx;

namespace game
{

	static int s_frame_counter = 0;

	void Tile::setFrameCounter(int counter) {
		s_frame_counter = counter;
	}

	struct TypeName { TileId::Type type; const char *name; } s_type_names[] = {
		{ TileId::floor,	"_floor_" },
		{ TileId::wall,		"_wall_" },
		{ TileId::roof,		"_roof_" },
		{ TileId::object,	"_object_" },
		{ TileId::floor,	"_cap_" },
		{ TileId::floor,	"_stair_" },
		{ TileId::floor,	"_step_" },
	};

	const IRect TileFrame::rect() const {
		return IRect(m_offset, m_texture.dimensions() + m_offset);
	}

	TileFrame::TileFrame(const TileFrame &rhs) :m_palette_ref(nullptr) {
		*this = rhs;
	}

	void TileFrame::operator=(const TileFrame &rhs) {
		m_texture = rhs.m_texture;
		m_offset = rhs.m_offset;
	}

	void TileFrame::serialize(Serializer &sr) {
		sr & m_offset & m_texture;
	}

	int2 TileFrame::textureSize() const {
		return m_texture.dimensions();
	}

	void TileFrame::cacheUpload(Texture &tex) const {
		m_texture.toTexture(tex, m_palette_ref->data(), m_palette_ref->size());
	}

	Texture TileFrame::texture() const {
		Texture out;
		m_texture.toTexture(out, m_palette_ref->data(), m_palette_ref->size());
		return out;
	}		

	PTexture TileFrame::deviceTexture(FRect &tex_rect) const {
		if(!getCache())
			bindToCache(TextureCache::main_cache);
		return accessTexture(tex_rect);
	}
	
	Tile::Tile() :m_type(TileId::unknown), m_first_frame(&m_palette) { }
			
	void Tile::legacyLoad(Serializer &sr) {
		ASSERT(sr.isLoading());

		{
			string lowercase = sr.name();
			std::transform(lowercase.begin(), lowercase.end(), lowercase.begin(), ::tolower);
			int slash = lowercase.rfind('/');

			m_type = TileId::unknown;
			for(int n = 0; n < COUNTOF(s_type_names); n++)
				if(lowercase.find(s_type_names[n].name, slash) != string::npos) {
					m_type = s_type_names[n].type;
					break;
				}
		}

		sr.signature("<tile>", 7);
		i16 type; sr & type;

		if(type == 0x3031) {
			char dummy;
			sr & dummy;
		}

		u8 size_x, size_y, size_z;
		sr(size_z, size_y, size_x);
		m_bbox.x = size_x;
		m_bbox.y = size_y;
		m_bbox.z = size_z;
		
		i32 posX, posY; sr(posX, posY);
		m_offset = int2(posX, posY);
		i32 width, height;
		sr(width, height);

		char unknown[5];
		int unk_size = type == '9'? 3 : type == '7'? 5 : type == '6'? 6 : 4;
		sr.data(unknown, unk_size);

		sr.signature("<tiledata>\0001", 12);
		u8 dummy2;
		i32 zar_count;
		sr(dummy2, zar_count);

		Palette first_pal;

		for(int n = 0; n < zar_count; n++) {
			TileFrame TileFrame(&m_palette);
			Palette palette;
			TileFrame.m_texture.legacyLoad(sr, palette);
			i32 off_x, off_y;
			sr(off_x, off_y);
			TileFrame.m_offset = int2(off_x, off_y);

			if(n == 0) {
				first_pal = palette;
				m_first_frame = TileFrame;
			}
			else {
				ASSERT(palette == first_pal);
				m_frames.push_back(TileFrame);
			}
		}

		m_palette.legacyLoad(sr);
		ASSERT(first_pal == m_palette);

		m_offset -= worldToScreen(int3(m_bbox.x, 0, m_bbox.z));
		updateMaxRect();
		
		ASSERT(sr.pos() == sr.size());
	}

	void Tile::serialize(Serializer &sr) {
		sr.signature("TILE", 4);
		sr(m_type, m_bbox, m_offset);
		if(sr.isLoading())
			ASSERT(TileId::isValid(m_type));
		sr & m_first_frame & m_frames & m_palette;

		if(sr.isLoading()) {
			for(int n = 0; n < (int)m_frames.size(); n++)
				m_frames[n].m_palette_ref = &m_palette;
			updateMaxRect();
		}
	}

	void Tile::draw(const int2 &pos, Color col) const {
		const TileFrame &TileFrame = accessFrame(s_frame_counter);
		FRect tex_coords;

		PTexture tex = TileFrame.deviceTexture(tex_coords);
		tex->bind();
		IRect rect = TileFrame.rect();
		drawQuad(pos + rect.min - m_offset, rect.size(), tex_coords.min, tex_coords.max, col);
	}

	void Tile::addToRender(SceneRenderer &renderer, const int3 &pos, Color color) const {
		const TileFrame &TileFrame = accessFrame(s_frame_counter);

		FRect tex_coords;
		PTexture tex = TileFrame.deviceTexture(tex_coords);
		renderer.add(tex, TileFrame.rect() - m_offset, pos, bboxSize(), color, tex_coords);
	}

	bool Tile::testPixel(const int2 &pos) const {
		return accessFrame(s_frame_counter).m_texture.testPixel(pos + m_offset);
	}

	const TileFrame &Tile::accessFrame(int frame_counter) const {
		if(!m_frames.empty()) {
			frame_counter = frame_counter % frameCount();
			return frame_counter == 0? m_first_frame : m_frames[frame_counter - 1];
		}
		return m_first_frame;
	}
		
	const IRect Tile::rect(int frame_id) const {
		return accessFrame(frame_id).rect() - m_offset;
	}

	void Tile::updateMaxRect() {
		m_max_rect = m_first_frame.rect();
		for(int n = 0; n < (int)m_frames.size(); n++)
			m_max_rect = sum(m_max_rect, m_frames[n].rect());
		m_max_rect -= m_offset;
	}

	ResourceMgr<Tile> Tile::mgr("data/tiles/", ".tile");

}