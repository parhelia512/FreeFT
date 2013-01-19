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

#ifndef UI_LIST_BOX_H
#define UI_LIST_BOX_H

#include "ui/window.h"
#include "gfx/font.h"

namespace ui {

	// When created as a popup, it behaves a bit differently,
	// so it can be used in a combo box
	class ListBox: public ui::Window
	{
	public:
		ListBox(const IRect &rect, Color color = Color::gui_dark);
		virtual const char *typeName() const { return "ListBox"; }

		struct Entry {
			Color color;
			string text;
			mutable bool is_selected;
		};
		
		virtual bool onEvent(const Event&);
		virtual void drawContents() const;
		virtual void onInput(int2 mouse_pos);
		virtual bool onMouseDrag(int2 start, int2 end, int key, int is_final);

		void addEntry(const char *text, Color col = Color::white);
		int findEntry(const char*) const;
		int selectedId() const;
		void selectEntry(int id);
		void clear();

		int size() const { return (int)m_entries.size(); }
		const Entry& operator[](int idx) const { return m_entries[idx]; }
		Entry& operator[](int idx) { return m_entries[idx]; }

		int lineHeight() const { return m_line_height; }

	private:
		void update();
		IRect entryRect(int id) const;
		int2 visibleEntriesIds() const;
		int entryId(int2 pos) const;

		vector<Entry> m_entries;
		gfx::PFont m_font;
		int m_line_height;
		int m_over_id, m_dragging_id;
	};

	typedef Ptr<ListBox> PListBox;


}


#endif
