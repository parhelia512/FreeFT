#ifndef UI_TILE_MAP_EDITOR_H
#define UI_TILE_MAP_EDITOR_H

#include "gfx/tile.h"
#include "ui/window.h"
#include "tile_map.h"

class TileGroup;

namespace ui {

	class TileMapEditor: public ui::Window
	{
	public:
		TileMapEditor(IRect rect);

		void setTileMap(TileMap*);
		void setTileGroup(const TileGroup *tile_group) { m_tile_group = tile_group; }
		void setNewTile(const gfx::Tile *tile) { m_new_tile = tile; }
			
		virtual void drawContents() const;
		virtual void onInput(int2 mouse_pos);
		virtual bool onMouseDrag(int2 start, int2 current, int key, int is_final);

		static void drawGrid(const IBox &box, int2 nodeSize, int y);

		enum SelectionMode {
			selection_normal,
			selection_union,
			selection_intersection,
			selection_difference,

			selection_mode_count,
		} m_selection_mode;

		//TODO: naming
		enum Mode {
			mode_selection,
			mode_placing,
			mode_placing_random,
			mode_filling,

			mode_count,
		} m_mode;
		
		float m_dirty_percent;

	private:
		TileMap *m_tile_map;
		const TileGroup *m_tile_group;
		const gfx::Tile *m_new_tile;
		vector<int> m_selected_ids;
		
		void fill(const IBox &fill_box);
		void fillRandomized(int group_id, const IBox &fill_box);
		void fillHoles(int main_group_id, const IBox &fill_box);

		int findAt(const int3 &pos) const;

		void drawBoxHelpers(const IBox &box) const;
		IBox computeCursor(int2 start, int2 end) const;
		void clampViewPos();
		
		int m_cursor_height, m_grid_height;
		IBox m_selection;
		int2 m_view_pos;

		int2 m_grid_size;
		bool m_show_grid, m_is_selecting;
	};

	typedef Ptr<TileMapEditor> PTileMapEditor;

}


#endif
