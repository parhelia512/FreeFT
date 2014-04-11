/* Copyright (C) 2013-2014 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of FreeFT.
 */

#include "game/orders/die.h"
#include "game/actor.h"
#include "game/world.h"

namespace game {

	DieOrder::DieOrder(DeathId::Type death_id) :m_death_id(death_id), m_is_dead(false) {
	}

	DieOrder::DieOrder(Stream &sr) :OrderImpl(sr) {
		sr >> m_death_id >> m_is_dead;
	}

	void DieOrder::save(Stream &sr) const {
		OrderImpl::save(sr);
		sr << m_death_id << m_is_dead;
	}

	bool Actor::handleOrder(DieOrder &order, ActorEvent::Type event, const ActorEventParams &params) {
		bool play_sound = event == ActorEvent::sound;

		if(event == ActorEvent::init_order) {
			bool is_fallen = isOneOf(m_action, Action::fall_forward, Action::fall_back, Action::fallen_back, Action::fallen_forward);
			bool special_death = isOneOf(order.m_death_id, DeathId::explode, DeathId::fire, DeathId::electrify, DeathId::melt);

			if((is_fallen || m_stance == Stance::prone) && !special_death) {
				order.m_death_id = DeathId::normal;
				play_sound = true;
				if(m_stance == Stance::prone && !is_fallen)
					animate(Action::fall_forward);
				else
					order.m_is_dead = true;
			}
			else if(!animateDeath(order.m_death_id))
				animateDeath(DeathId::normal);
		}

		if(play_sound) {
			SoundId sound_id = m_actor.sounds[m_sound_variation].death[order.m_death_id];
			if(sound_id == -1)
				sound_id = m_actor.sounds[m_sound_variation].death[DeathId::normal];
			world()->playSound(sound_id, pos());

			if(m_actor.is_alive)
				world()->playSound(m_actor.human_death_sounds[order.m_death_id], pos());
		}
		if(event == ActorEvent::anim_finished)
			order.m_is_dead = true;

		return true;
	}

}
