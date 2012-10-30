#include "tile_map_editor.h"
#include "tile_group.h"
#include "gfx/device.h"
#include "gfx/font.h"

using namespace gfx;


TileMapEditor::TileMapEditor(IRect rect)
	:ui::Window(rect, Color(0, 0, 0)), m_show_grid(false), m_grid_size(1, 1), m_tile_map(0), m_new_tile(nullptr) {
	m_tile_group = nullptr;
	m_cursor_pos = int3(0, 0, 0);
	m_view_pos = int2(0, 0);
	m_is_selecting = false;
	m_mode = mSelecting;
}

void TileMapEditor::drawGrid(const IBox &box, int2 node_size, int y) {
	DTexture::Bind0();

	//TODO: proper drawing when y != 0
	for(int x = box.min.x - box.min.x % node_size.x; x <= box.max.x; x += node_size.x)
		DrawLine(int3(x, y, box.min.z), int3(x, y, box.max.z), Color(255, 255, 255, 64));
	for(int z = box.min.z - box.min.z % node_size.y; z <= box.max.z; z += node_size.y)
		DrawLine(int3(box.min.x, y, z), int3(box.max.x, y, z), Color(255, 255, 255, 64));
}

void TileMapEditor::setTileMap(TileMap *new_tile_map) {
	//TODO: do some cleanup within the old tile map?
	m_tile_map = new_tile_map;
}

void TileMapEditor::onInput(int2 mouse_pos) {
	Assert(m_tile_map);

	m_cursor_pos = AsXZY(ScreenToWorld(mouse_pos + m_view_pos), m_cursor_pos.y);
	if(m_show_grid) {
		m_cursor_pos.x -= m_cursor_pos.x % m_grid_size.x;
		m_cursor_pos.z -= m_cursor_pos.z % m_grid_size.y;
	}
	if(IsKeyDown(Key_kp_add))
		m_cursor_pos.y++;
	if(IsKeyDown(Key_kp_subtract))
		m_cursor_pos.y--;

	if(IsKeyDown('G')) {
		if(m_show_grid) {
			if(m_grid_size.x == 3)
				m_grid_size = int2(6, 6);
			else if(m_grid_size.x == 6)
				m_grid_size = int2(9, 9);
			else {
				m_grid_size = int2(1, 1);
				m_show_grid = false;
			}
		}
		else {
			m_grid_size = int2(3, 3);
			m_show_grid = true;
		}
	}

	if(IsKeyDown('S'))
		m_mode = mSelecting;
	if(IsKeyDown('P'))
		m_mode = mPlacing;
	if(IsKeyDown('R'))
		m_mode = mPlacingRandom;
	if(IsKeyDown('F'))
		m_mode = mAutoFilling;

	{
		KeyId actions[TileGroup::Group::sideCount] = {
			Key_kp_1, 
			Key_kp_2,
			Key_kp_3,
			Key_kp_6,
			Key_kp_9,
			Key_kp_8,
			Key_kp_7,
			Key_kp_4
		};
		
		for(int n = 0; n < COUNTOF(actions); n++)
			if(IsKeyDown(actions[n]))
				m_view_pos += WorldToScreen(TileGroup::Group::s_side_offsets[n] * m_grid_size.x);
	}


	if(IsKeyPressed(Key_del))
		m_tile_map->deleteSelected();
}
	
bool TileMapEditor::onMouseDrag(int2 start, int2 current, int key, bool is_final) {
	if((IsKeyPressed(Key_lctrl) && key == 0) || key == 2) {
		m_view_pos -= GetMouseMove();
		return true;
	}
	else if(key == 0) {
		int3 start_pos = AsXZ((int2)( ScreenToWorld(float2(start + m_view_pos)) + float2(0.5f, 0.5f) ));
		int3 end_pos = AsXZ((int2)( ScreenToWorld(float2(current + m_view_pos)) + float2(0.5f, 0.5f) ));
		if(start_pos.x > end_pos.x) Swap(start_pos.x, end_pos.x);
		if(start_pos.z > end_pos.z) Swap(start_pos.z, end_pos.z);

		if(m_show_grid) {
			start_pos.x -= start_pos.x % m_grid_size.x;
			start_pos.z -= start_pos.z % m_grid_size.y;
			end_pos += AsXZ(m_grid_size - int2(start_pos.x != end_pos.x, start_pos.z != end_pos.z));
			end_pos.x -= end_pos.x % m_grid_size.x;
			end_pos.z -= end_pos.z % m_grid_size.y;
		}

		m_selection = IBox(start_pos, end_pos);
		m_is_selecting = !is_final;
		if(is_final) {
			if(m_mode == mSelecting) {
				m_tile_map->select(IBox(m_selection.min, m_selection.max + int3(0, 1, 0)),
						IsKeyPressed(Key_lctrl)? SelectionMode::add : SelectionMode::normal);
			}
			else if(m_mode == mPlacing && m_new_tile) {
				m_tile_map->fill(*m_new_tile, IBox(m_selection.min, m_selection.max + int3(0, m_new_tile->bbox.y, 0)));
			}
			else if(m_mode == mPlacingRandom && m_new_tile && m_tile_group) {
				int entry_id = m_tile_group->findEntry(m_new_tile);
				int group_id = entry_id != -1? m_tile_group->entryGroup(entry_id) : -1;

				if(group_id != -1) {
					vector<int> entries;
					entries.reserve(m_tile_group->groupEntryCount(group_id));
					for(int n = 0; n < m_tile_group->entryCount(); n++)
						if(m_tile_group->entryGroup(n) == group_id)
							entries.push_back(n);

					int3 bbox = m_new_tile->bbox;

					for(int x = m_selection.min.x; x < m_selection.max.x; x += bbox.x)
						for(int z = m_selection.min.z; z < m_selection.max.z; z += bbox.z) {
							int random_id = rand() % entries.size();
							const gfx::Tile *tile = m_tile_group->entryTile(entries[random_id]);

							try { m_tile_map->addTile(*tile, int3(x, m_selection.min.y, z)); }
							catch(...) { }
						}
				}
			}
			else if(m_mode == mAutoFilling && m_tile_group && m_new_tile) {
				int3 bbox = m_new_tile->bbox;

				for(int n = 0; n < m_tile_group->entryCount(); n++)
					m_tile_group->entryTile(n)->m_temp = n;

				for(int x = m_selection.min.x; x < m_selection.max.x; x += bbox.x)
					for(int z = m_selection.min.z; z < m_selection.max.z; z += bbox.z) {
						int3 pos(x, m_selection.min.y, z);
						const TileInstance *neighbours[4] = {
							m_tile_map->at(pos + int3(0, 0, bbox.z)),
							m_tile_map->at(pos + int3(bbox.x, 0, 0)),
							m_tile_map->at(pos - int3(0, 0, bbox.z)),
							m_tile_map->at(pos - int3(bbox.x, 0, 0)) };

						int sides[8] = {-1, -1, -1, -1, -1, -1, -1, -1}; // -1: any, -2: error
						int ngroups[4];

						int soffset[4] = { 5, 7, 1, 3 };
						int doffset[4] = { 7, 1, 3, 5 };
						bool error = false;

						for(int n = 0; n < 4; n++) {
							int entry_id = neighbours[n]? m_tile_group->findEntry(neighbours[n]->m_tile) : -1;
							ngroups[n] = entry_id == -1? -1 : m_tile_group->entryGroup(entry_id);

							if(ngroups[n] != -1) {
								for(int s = 0; s < 3; s++) {
									int src_surf = m_tile_group->groupSurface(ngroups[n], (soffset[n] - s + 8) % 8);
									int dst_idx = (doffset[n] + s) % 8;

									if(sides[dst_idx] != -1 && sides[dst_idx] != src_surf)
										error = true;
									sides[dst_idx] = src_surf;
								}
							}
						}

						int any_count = 0;
						for(int n = 0; n < 8; n++) {
					//		printf("%d ", sides[n]);
							any_count += sides[n] == -1;
						}
						if(any_count > 4)
							error = true;
					//	printf(" err: %d\n", error?1 : 0);

						if(!error) {
							//TODO: speed up
							vector<int> entries;
							for(int n = 0; n < m_tile_group->entryCount(); n++) {
								int group_id = m_tile_group->entryGroup(n);
								const int *group_surf = m_tile_group->groupSurface(group_id);
								bool error = false;

								for(int s = 0; s < 8; s++)
									if(sides[s] != group_surf[s] && sides[s] != -1) {
										error = true;
										break;
									}
								if(!error)
									entries.push_back(n);
							}

							if(!entries.empty()) {
								int random_id = rand() % entries.size();
								const gfx::Tile *tile = m_tile_group->entryTile(entries[random_id]);
								try { m_tile_map->addTile(*tile, int3(x, m_selection.min.y, z)); }
								catch(...) { }
							}
						}
					}
			}
		}

		return true;
	}

	return false;
}
	
void TileMapEditor::drawContents() const {
	Assert(m_tile_map);

	IRect view_rect = clippedRect() - m_view_pos;
	LookAt(-view_rect.min);
	int2 wsize = view_rect.Size();

	m_tile_map->render(IRect(m_view_pos, m_view_pos + wsize));

	if(m_show_grid) {
		int2 p[4] = {
			ScreenToWorld(m_view_pos + int2(0, 0)),
			ScreenToWorld(m_view_pos + int2(0, wsize.y)),
			ScreenToWorld(m_view_pos + int2(wsize.x, wsize.y)),
			ScreenToWorld(m_view_pos + int2(wsize.x, 0)) };

		int2 min = Min(Min(p[0], p[1]), Min(p[2], p[3]));
		int2 max = Max(Max(p[0], p[1]), Max(p[2], p[3]));
		IBox box(min.x, 0, min.y, max.x, 0, max.y);
		IBox bbox = m_tile_map->boundingBox();
		box = IBox(Max(box.min, bbox.min), Min(box.max, bbox.max));

		drawGrid(box, m_grid_size, m_cursor_pos.y);
	}

	if(m_new_tile && IsKeyPressed(Key_lshift) && !m_is_selecting) {
		int3 pos = m_is_selecting? m_selection.min : m_cursor_pos;
		m_tile_map->drawPlacingHelpers(*m_new_tile, pos);
		m_tile_map->drawBoxHelpers(IBox(pos, pos + m_new_tile->bbox));
	}

	if(m_is_selecting) {
		m_tile_map->drawBoxHelpers(m_selection);
		DTexture::Bind0();
		DrawBBox(m_selection);
	}
	
	LookAt(-clippedRect().min);
	gfx::PFont font        = Font::mgr["font1"];
	gfx::PTexture font_tex = Font::tex_mgr["font1"];

	const char *mode_names[mCount] = {
		"selecting tiles",
		"placing new tiles",
		"placing new tiles (randomized)",
		"filling holes",
	};

	font_tex->Bind();
	font->SetSize(int2(35, 25));
	font->SetPos(int2(0, clippedRect().Height() - 25));

	char text[64];
	snprintf(text, sizeof(text), "Mode: %s", mode_names[m_mode]);
	font->Draw(text);
}
