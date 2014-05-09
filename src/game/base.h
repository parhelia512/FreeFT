/* Copyright (C) 2013-2014 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of FreeFT.
 */

#ifndef GAME_BASE_H
#define GAME_BASE_H

#include "../base.h"
#include "sys/data_sheet.h"

namespace game {

	class Entity;
	class Tile;

	class Order;
	class POrder;

	DECLARE_ENUM(WeaponClass,
		unarmed,
		club,
		heavy,
		knife,
		minigun,
		pistol,
		rifle,
		rocket,
		smg,
		spear
	);

	DECLARE_ENUM(ArmourClass,
		none,
		leather,
		metal,
		environmental,
		power
	);

	DECLARE_ENUM(DamageType,
		undefined = -1,
		bludgeoning,
		slashing,
		piercing,
		bullet,
		fire,
		plasma,
		laser,
		electric,
		explosive
	);

	DECLARE_ENUM(DeathId,
		normal,
		big_hole,
		cut_in_half,
		electrify,
		explode,
		fire,
		melt,
		riddled
	);

	DECLARE_ENUM(EntityId,
		container,
		door,
		actor,
		item,
		projectile,
		impact,
		trigger
	);

	DECLARE_ENUM(TileId,
		wall,
		floor,
		object,
		stairs,
		roof,
		unknown
	);

	DECLARE_ENUM(SurfaceId,
		stone,
		gravel,
		metal,
		wood,
		water,
		snow,
		unknown
	);

	DECLARE_ENUM(Stance,
		prone,
		crouch,
		stand
	);

	DECLARE_ENUM(SentryMode,
		passive,
		defensive,
		aggresive
	);

	DECLARE_ENUM(AttackMode,
		undefined = -1,
		single = 0,
		burst,
		thrust,
		slash,
		swing,
		throwing,
		punch,
		kick
	);

	DECLARE_ENUM(GameMode,
		death_match
	);
	
	namespace AttackMode {
		inline constexpr bool isRanged(Type t) { return t == single || t == burst || t == throwing; }
		inline constexpr bool isMelee(Type t) { return !isRanged(t); }
		inline constexpr uint toFlags(Type t) { return t == undefined? 0 : 1 << t; }
	};

	namespace AttackModeFlags {
		enum Type {
			single		= toFlags(AttackMode::single),
			burst		= toFlags(AttackMode::burst),
			thrust		= toFlags(AttackMode::thrust),
			slash		= toFlags(AttackMode::slash),
			swing		= toFlags(AttackMode::swing),
			throwing	= toFlags(AttackMode::throwing),
			punch		= toFlags(AttackMode::punch),
			kick		= toFlags(AttackMode::kick)
		};

		uint fromString(const char*);

		AttackMode::Type getFirst(uint flags);
	};
	
	namespace Flags { enum Type : unsigned; };

	inline constexpr Flags::Type entityIdToFlag(EntityId::Type id) { return (Flags::Type)(1u << (4 + id)); }
	inline constexpr Flags::Type tileIdToFlag(TileId::Type id) { return (Flags::Type)(1u << (16 + id)); }

	static_assert(EntityId::count <= 12, "Flag limit reached");
	static_assert(TileId::count <= 8, "Flag limit reached");

	namespace Flags {
		enum Type :unsigned {
			//Generic flags: when testing, at least one flag must match
			static_entity	= 0x0001,
			dynamic_entity	= 0x0002, // can change position and/or bounding box

			container		= entityIdToFlag(EntityId::container),
			door			= entityIdToFlag(EntityId::door),
			actor			= entityIdToFlag(EntityId::actor),
			item			= entityIdToFlag(EntityId::item),
			projectile		= entityIdToFlag(EntityId::projectile),
			impact			= entityIdToFlag(EntityId::impact),
			trigger			= entityIdToFlag(EntityId::trigger),

			entity			= 0xffff,

			wall_tile		= tileIdToFlag(TileId::wall),
			floor_tile		= tileIdToFlag(TileId::floor),
			object_tile		= tileIdToFlag(TileId::object),
			stairs_tile		= tileIdToFlag(TileId::stairs),
			roof_tile		= tileIdToFlag(TileId::roof),
			walkable_tile	= floor_tile | stairs_tile | roof_tile,

			tile			= 0xff0000,

			all				= 0xffffff,
	
			//Functional flags: when testing, all of the selected flags must match	
			visible			= 0x01000000,
			occluding		= 0x02000000,
			colliding		= 0x04000000,
		};

		inline constexpr Type operator|(Type a, Type b) { return (Type)((unsigned)a | (unsigned)b); }
		inline constexpr Type operator&(Type a, Type b) { return (Type)((unsigned)a & (unsigned)b); }
		inline constexpr Type operator~(Type a) { return (Type)(~(unsigned)a); }

		inline constexpr bool test(unsigned object_flags, Type test) {
			return (object_flags & test & 0xffffff) && ((object_flags & test & 0xff000000) == (test & 0xff000000));
		}
	
	}

	class SoundId {
	public:
		SoundId() :m_id(-1) { }
		SoundId(const char *sound_name);
		operator int() const { return m_id; }
		bool isValid() const { return m_id != -1; }

	protected:
		int m_id;
	};

	// Object reference contains only pure indices, so they may be invalid
	// and refEntity / refTile methods in World may return null even if
	// isEntity / isTile returns true
	class ObjectRef {
	public:
		ObjectRef() :m_index(-1) { }
		explicit operator bool() const { return !isEmpty(); }

		bool operator==(const ObjectRef &rhs) const
			{ return m_index == rhs.m_index && m_is_entity == rhs.m_is_entity; }

		bool isEmpty() const { return m_index == -1; }
		bool isEntity() const { return !isEmpty() && m_is_entity; }
		bool isTile() const { return !isEmpty() && !m_is_entity; }
		int index() const { return m_index; }

	private:
		ObjectRef(int index, bool is_entity) :m_index(index), m_is_entity(is_entity? 1 : 0) { }

		int m_index: 31;
		int m_is_entity: 1;
		friend class World;
		friend class EntityRef;
		friend class WorldViewer;
	};

	class Intersection {
	public:
		Intersection(ObjectRef ref = ObjectRef(), float distance = constant::inf)
			:m_ref(ref), m_distance(distance) { }
		explicit operator bool() const { return !isEmpty(); }
		bool operator==(const Intersection &rhs) const
			{ return isEmpty()? rhs.isEmpty() : m_ref == rhs.m_ref && m_distance == rhs.m_distance; }

		operator const ObjectRef() const { return m_ref; }
		bool isEmpty() const { return m_ref.isEmpty(); }
		bool isEntity() const { return m_ref.isEntity(); }
		bool isTile() const { return m_ref.isTile(); }

		float distance() const { return m_distance; }

	private:
		ObjectRef m_ref;
		float m_distance;
	};

}

#include "game/proto.h"

#endif
