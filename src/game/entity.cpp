/* Copyright (C) 2013-2014 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of FreeFT.
 */

#include "game/entity.h"
#include "game/sprite.h"
#include "game/world.h"
#include "gfx/device.h"
#include "gfx/scene_renderer.h"
#include "sys/profiler.h"
#include "sys/xml.h"

using namespace gfx;


namespace game {


	EntityProto::EntityProto(const TupleParser &parser) :Proto(parser) {
		sprite_name = parser("sprite_name");
		if(!is_dummy)
			sprite = &Sprite::getPartial(sprite_name);
		else 
			sprite = nullptr;
	}
		
	Entity::Entity(const Sprite &sprite)
		:m_sprite(sprite), m_pos(0.0f, 0.0f, 0.0f) {
		DASSERT(m_sprite.isFullyLoaded());
		resetAnimState();
	}

	//TODO: redundant initialization?
	Entity::Entity(const Sprite &sprite, const XMLNode &node) :m_sprite(sprite) {
		DASSERT(m_sprite.isFullyLoaded());
		m_pos = node.float3Attrib("pos");
		resetAnimState();
		setDirAngle(node.floatAttrib("angle"));
	}

	XMLNode Entity::save(XMLNode &parent) const {
		XMLNode node = parent.addChild(EntityId::toString(entityType()));
		node.addAttrib("pos", m_pos);
		node.addAttrib("angle", m_dir_angle);
		return node;
	}
	
	enum {
		flag_compressed = 1,
		flag_is_looped = 2,
		flag_is_finished = 4,
	};

	Entity::Entity(const Sprite &sprite, Stream &sr) :EntityWorldProxy(sr), m_sprite(sprite) {
		resetAnimState();

		u8 flags;
		sr.unpack(flags, m_pos, m_dir_angle);

		m_is_seq_finished = flags & flag_is_finished;
		m_is_seq_looped = flags & flag_is_looped;

		if(flags & flag_compressed) {
			u8 frame_idx, seq_idx, dir_idx;
			sr.unpack(seq_idx, frame_idx, dir_idx);
			m_frame_idx = frame_idx;
			m_seq_idx = seq_idx;
			m_dir_idx = dir_idx;
		}
		else
			sr.unpack(m_seq_idx, m_frame_idx, m_dir_idx);
	}

	void Entity::resetAnimState() {
		m_dir_angle = 0.0f;
		m_seq_idx = -1;
		playSequence(0, false);
	}

	void Entity::save(Stream &sr) const {
		EntityWorldProxy::save(sr);

		bool can_compress =
			m_seq_idx >= 0 && m_seq_idx <= 255 &&
			m_frame_idx >= 0 && m_frame_idx <= 255 &&
			m_dir_idx >= 0 && m_dir_idx <= 255;

		u8 flags =	(can_compress? flag_compressed : 0) |
					(m_is_seq_looped? flag_is_looped : 0) |
					(m_is_seq_finished? flag_is_finished : 0);

		sr.pack(flags, m_pos, m_dir_angle);

		if(can_compress)
			sr.pack(u8(m_seq_idx), u8(m_frame_idx), u8(m_dir_idx));
		else
			sr.pack(m_seq_idx, m_frame_idx, m_dir_idx);
	}

	Entity::Entity(const Entity &rhs) :m_sprite(rhs.m_sprite) {
		m_pos = rhs.m_pos;
		
		m_dir_angle = rhs.m_dir_angle;
		m_dir_idx = rhs.m_dir_idx;
		m_seq_idx = rhs.m_seq_idx;
		m_frame_idx = rhs.m_frame_idx;
		m_is_seq_looped = rhs.m_is_seq_looped;
		m_is_seq_finished = rhs.m_is_seq_finished;
	}
	
	Entity::~Entity() {
	}

	void Entity::roundPos() {
		m_pos = (int3)(m_pos + float3(0.5f, 0, 0.5f));
	}

	void Entity::setPos(const float3 &new_pos) {
		m_pos = new_pos;
	}

	const FBox Entity::boundingBox() const {
		return FBox(m_pos, m_pos + float3(m_sprite.bboxSize()));
	}

	const IRect Entity::screenRect() const {
		return m_sprite.getMaxRect() + (int2)worldToScreen(pos());
	}
	
	const IRect Entity::currentScreenRect() const {
		//TODO: float based results
		return m_sprite.getRect(m_seq_idx, m_frame_idx, m_dir_idx) + (int2)worldToScreen(pos());
	}

	void Entity::addToRender(gfx::SceneRenderer &out) const {
		//PROFILE("Entity::addToRender");
		IRect rect = m_sprite.getRect(m_seq_idx, m_frame_idx, m_dir_idx);
		if(!areOverlapping(out.targetRect(), rect + (int2)worldToScreen(m_pos)))
			return;

		FBox bbox = boundingBox() - pos();
		if(shrinkRenderedBBox())
			bbox.min.y = min(bbox.min.y + 1.0f, bbox.max.y - 0.5f);

		FRect tex_rect;
		PTexture tex = m_sprite.getFrame(m_seq_idx, m_frame_idx, m_dir_idx, tex_rect);
		out.add(tex, rect, m_pos, bbox, Color::white, tex_rect);

		bbox += pos();
		if(world() && world()->findAny(bbox, this))
			out.addBox(bbox, Color::red);
	}
		
	bool Entity::testPixel(const int2 &screen_pos) const {
		return m_sprite.testPixel(screen_pos - (int2)worldToScreen(pos()), m_seq_idx, m_frame_idx, m_dir_idx);
	}

	void Entity::handleEventFrame(const Sprite::Frame &frame) {
		DASSERT(frame.id <= Sprite::ev_first_specific);

		if(frame.id == Sprite::ev_fire)
			onFireEvent(int3(frame.params[0], frame.params[1], frame.params[2]));
		else if(frame.id == Sprite::ev_hit)
			onHitEvent();
		else if(frame.id == Sprite::ev_sound)
			onSoundEvent();
		else if(frame.id == Sprite::ev_step_left || frame.id == Sprite::ev_step_right)
			onStepEvent(frame.id == Sprite::ev_step_left);
		else if(frame.id == Sprite::ev_pickup)
			onPickupEvent();
	}

	void Entity::playSequence(int seq_idx, bool handle_events) {
		DASSERT(seq_idx >= 0 && seq_idx < m_sprite.size());

		m_is_seq_finished = false;
		if(seq_idx != m_seq_idx || !m_is_seq_looped) {
			int frame_count = m_sprite.frameCount(seq_idx);
			m_frame_idx = 0;

			while(m_frame_idx < frame_count && m_sprite.frame(seq_idx, m_frame_idx).id < 0) {
				if(m_sprite.frame(seq_idx, m_frame_idx).id <= Sprite::ev_first_specific)
					if(handle_events)
						handleEventFrame(m_sprite.frame(seq_idx, m_frame_idx));
				m_frame_idx++;
			}

			DASSERT(m_frame_idx < frame_count);
		}
		if(seq_idx != m_seq_idx) {
			m_seq_idx = seq_idx;
			m_dir_idx = m_sprite.findDir(m_seq_idx, m_dir_angle);
			m_is_seq_looped = m_sprite.isSequenceLooped(seq_idx);
		}
	}

	void Entity::nextFrame() {
		if(m_is_seq_finished)
			return;

		int prev_frame = m_frame_idx;
		m_frame_idx++;
		int frame_count = m_sprite.frameCount(m_seq_idx);

		while(m_frame_idx < frame_count && m_sprite.frame(m_seq_idx, m_frame_idx).id < 0) {
			const Sprite::Frame &frame = m_sprite.frame(m_seq_idx, m_frame_idx);
			if(frame.id == Sprite::ev_repeat_all)
				m_frame_idx = 0;
			else if(frame.id == Sprite::ev_jump_to_frame) 
				m_frame_idx = frame.params[0];
			else {
				if(frame.id <= Sprite::ev_first_specific)
					handleEventFrame(frame);
				m_frame_idx++;
			}
		}

		if(m_frame_idx == frame_count) {
			m_is_seq_finished = true;
			m_frame_idx = prev_frame;
			onAnimFinished();
		}
	}

	void Entity::setDir(const float2 &vec) {
		setDirAngle(vectorToAngle(vec));
	}

	void Entity::setDirAngle(float angle) {
		m_dir_angle = angle;
		m_dir_idx = m_sprite.findDir(m_seq_idx, angle);
	}

	const float2 Entity::dir() const {
		return angleToVector(dirAngle());
	}

	float Entity::actualDirAngle() const {
		int dir_count = m_sprite.dirCount(m_seq_idx);
		return float(m_dir_idx) * (constant::pi * 2.0f) / float(dir_count) + constant::pi;
	}

	const float2 Entity::actualDir() const {
		return angleToVector(actualDirAngle());
	}

	bool areAdjacent(const Entity &a, const Entity &b) {
		FBox box_a = a.boundingBox(), box_b = b.boundingBox();

		if(box_a.max.y < box_b.min.y || box_b.max.y < box_b.min.y)
			return false;

		return distanceSq(	FRect(box_a.min.xz(), box_a.max.xz()),
							FRect(box_b.min.xz(), box_b.max.xz())) <= 1.0f;
	}

}
