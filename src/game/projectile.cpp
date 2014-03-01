/* Copyright (C) 2013-2014 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of FreeFT.
 */

#include "game/projectile.h"
#include "sys/xml.h"

namespace game {

	ProjectileProto::ProjectileProto(const TupleParser &parser) :ProtoImpl(parser) {
		impact = parser("impact_id");
		blend_angles = toBool(parser("blend_angles"));
		speed = toFloat(parser("speed"));
	}

	void ProjectileProto::connect() {
		impact.connect();
	}

	Projectile::Projectile(const ProjectileProto &proto, const float3 &pos,
							float initial_angle, const float3 &target, EntityRef spawner)
		:EntityImpl(proto), m_dir(target - pos), m_spawner(spawner) {
			m_dir *= 1.0f / length(m_dir);
			setPos(pos);
			setDirAngle(initial_angle);
			m_target_angle = vectorToAngle(m_dir.xz());
			if(!proto.blend_angles)
				setDirAngle(m_target_angle);
			m_speed = proto.speed;
			m_frame_count = 0;
//			printf("Spawning projectile at: (%.0f %.0f %.0f) -> %.2f %.2f\n",
//					this->pos().x, this->pos().y, this->pos().z, m_dir.x, m_dir.z);
	}

	Projectile::Projectile(Stream &sr) :EntityImpl(sr) {
		sr.unpack(m_dir, m_speed, m_frame_count, m_target_angle);
		sr >> m_spawner;
	}

	void Projectile::save(Stream &sr) const {
		EntityImpl::save(sr);
		sr.pack(m_dir, m_speed, m_frame_count, m_target_angle);
		sr << m_spawner;
	}
		
	XMLNode Projectile::save(XMLNode& parent) const {
		return Entity::save(parent);
	}
	
	void Projectile::nextFrame() {
		Entity::nextFrame();
		setDirAngle(blendAngles(dirAngle(), m_target_angle, constant::pi * 0.01f));
		m_frame_count++;
	}

	void Projectile::think() {
		float time_delta = timeDelta();
		Ray ray(pos(), m_dir);
		float ray_pos = m_speed * time_delta;

		if(m_frame_count < 2)
			return;

		Intersection isect = trace(Segment(ray, 0.0f, ray_pos), refEntity(m_spawner), collider_solid);
		float3 new_pos = ray.at(min(isect.distance(), ray_pos));
		setPos(new_pos);

		if(isect.distance() < ray_pos) {
			if(m_proto.impact.isValid()) {
				addEntity(new Impact(*m_proto.impact, new_pos));
				Entity *entity = refEntity(isect);

				if(entity) {
					float damage = 100.0f;
					entity->onImpact(0, damage);
				}
			}
			remove();
		}
	}

	Impact::Impact(const ImpactProto &proto, const float3 &pos)
		:EntityImpl(proto) {
		setPos(pos);
	}
	
	Impact::Impact(Stream &sr) :EntityImpl(sr) { }

	void Impact::save(Stream &sr) const {
		EntityImpl::save(sr);
	}
	
	XMLNode Impact::save(XMLNode& parent) const {
		return Entity::save(parent);
	}

	void Impact::onAnimFinished() {
		remove();
	}


}
