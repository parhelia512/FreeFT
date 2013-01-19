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

#include "game/item.h"
#include "sys/xml.h"
#include <map>

static const char *s_seq_names[3] = {
	"default",
	"guibig",
	"guismall",
};

namespace game {

	ItemEntity::ItemEntity(const Item &item, const float3 &pos)
		:Entity(item.spriteName(), pos), m_item(item) {
		DASSERT(item.isValid());

		setBBox(FBox(float3(0.0f, 0.0f, 0.0f), asXZY(bboxSize().xz(), 0.0f)));

		for(int n = 0; n < COUNTOF(m_seq_ids); n++) {
			m_seq_ids[n] = m_sprite->findSequence(s_seq_names[n]);
			ASSERT(m_seq_ids[n] != -1);
		}
	}

	gfx::PTexture ItemEntity::guiImage(bool small, FRect &tex_rect) const {
		return m_sprite->getFrame(m_seq_ids[small?2 : 1], 0, 0, tex_rect);
	}

	static std::map<string, PItemDesc> s_items;
	static bool s_are_items_loaded = false;

	void ItemDesc::loadItems() {
		if(s_are_items_loaded)
			return;

		XMLDocument doc;
		doc.load("data/items.xml");

		XMLNode node = doc.child("item");
		while(node) {
			ItemTypeId::Type type = ItemTypeId::fromString(node.attrib("type"));
			std::unique_ptr<ItemDesc> item;
			const char *id = nullptr;

			if(type == ItemTypeId::weapon)
				item = PItemDesc(new WeaponDesc);
			else if(type == ItemTypeId::ammo)
				item = PItemDesc(new AmmoDesc);
			else if(type == ItemTypeId::armour)
				item = PItemDesc(new ArmourDesc);
			else {
				ASSERT(type == ItemTypeId::other);
				item = PItemDesc(new ItemDesc);
			}

			try {
				id = node.attrib("id");
				item->name = node.attrib("name");
				item->sprite_name = node.attrib("sprite_name");
				item->description = node.attrib("description");
				item->weight = node.floatAttrib("weight");

				if(type == ItemTypeId::weapon) {
					WeaponDesc *weapon = static_cast<WeaponDesc*>(item.get());
					weapon->projectile_type_id = ProjectileTypeId::fromString(node.attrib("projectile_type"));
					weapon->damage = node.floatAttrib("damage");
					weapon->projectile_speed = node.floatAttrib("projectile_speed");
					weapon->class_id = WeaponClassId::fromString(node.attrib("class"));
				}
				else if(type == ItemTypeId::ammo) {
					AmmoDesc *ammo = static_cast<AmmoDesc*>(item.get());
					ammo->damage_modifier = node.floatAttrib("damage_modifier");
				}
				else if(type == ItemTypeId::armour) {
					ArmourDesc *armour = static_cast<ArmourDesc*>(item.get());
					armour->damage_resistance = node.floatAttrib("damage_resistance");
					armour->class_id = ArmourClassId::fromString(node.attrib("class"));
				}

				s_items[id] = std::move(item);
			}
			catch(const Exception &ex) {
				THROW("Error while parsing item with id: %s, type: %s\n%s",
						id? id : "unknown", ItemTypeId::toString(type), ex.what());
			}

			node = node.sibling("item");
		}

		s_are_items_loaded = true;
	}

	const ItemDesc *ItemDesc::find(const char *name) {
		DASSERT(s_are_items_loaded);

		auto it = s_items.find(name);
		if(it != s_items.end())
			return it->second.get();
		return nullptr;
	}

	void ItemDesc::initialize(ItemParameter *params) const {
		for(int n = 0; n < param_count; n++)
			params[n].i = 0;
	}
		
	Item::Item(const ItemDesc *desc) :m_desc(desc) {
		if(m_desc)
			m_desc->initialize(params);
	}
		
	float Item::weight() const {
		DASSERT(isValid());
		return m_desc->weight;
	}

	ItemTypeId::Type Item::typeId() const {
		return isValid()? m_desc->type() : ItemTypeId::invalid;
	}

	const char *Item::spriteName() const {
		DASSERT(isValid());
		return m_desc->sprite_name.c_str();
	}

	const char *Item::name() const {
		DASSERT(isValid());
		return m_desc->name.c_str();
	}

	bool Item::operator==(const Item &rhs) const {
		if(m_desc != rhs.m_desc)
			return false;
		return memcmp(params, rhs.params, sizeof(params)) == 0;
	}

	Weapon::Weapon(const Item &item) :Item(item) {
		if(isValid())
			DASSERT(typeId() == ItemTypeId::weapon);
	}
	
	Armour::Armour(const Item &item) :Item(item) {
		if(isValid())
			DASSERT(typeId() == ItemTypeId::armour);
	}

	ProjectileTypeId::Type Weapon::projectileTypeId() const {
		DASSERT(isValid());
		return desc()->projectile_type_id; 
	}
	
	float Weapon::projectileSpeed() const {
		DASSERT(isValid());
		return desc()->projectile_speed; 
	}

	WeaponClassId::Type Weapon::classId() const {
		DASSERT(isValid());
		return desc()->class_id; 
	}

	ArmourClassId::Type Armour::classId() const {
		DASSERT(isValid());
		return desc()->class_id; 
	}

}
