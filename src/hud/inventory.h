/* Copyright (C) 2013-2014 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of FreeFT.
 */

#ifndef HUD_INVENTORY_H
#define HUD_INVENTORY_H

#include "hud/layer.h"
#include "hud/widget.h"
#include "game/item.h"
#include "game/inventory.h"

namespace hud
{

	class HudItemDesc: public HudWidget {
	public:
		HudItemDesc(const FRect &rect);
		void setItem(const Item &item) { m_item = item; }
		void draw() const override;

	protected:
		Item m_item;
	};

	class HudInventoryItem: public HudWidget {
	public:
		HudInventoryItem(const FRect &rect);
		void setEntry(const Inventory::Entry &entry) { m_entry = entry; }
		const Inventory::Entry &entry() const { return m_entry; }
		void draw() const override;

	protected:
		Inventory::Entry m_entry;
	};

	class HudInventory: public HudLayer {
	public:
		HudInventory(PWorld world, EntityRef actor_ref, const FRect &target_rect);
		~HudInventory();

		float preferredHeight() const;
		void update(bool is_active, double time_diff) override;

	private:
		game::PWorld m_world;
		game::EntityRef m_actor_ref;

		vector<PHudInventoryItem> m_buttons;
		PHudItemDesc m_item_desc;
	};

}

#endif
