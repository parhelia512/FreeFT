/* Copyright (C) 2013-2014 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of FreeFT.
 */

#include "game/pc_controller.h"
#include "game/character.h"
#include "game/actor.h"


namespace game {

	PCController::PCController(World &world, const PlayableCharacter &pc)
		:m_world(world), m_pc(pc) {
		const Actor *actor = this->actor();
		m_target_stance = actor? actor->stance() : -1;
	}
		
	PCController::~PCController() { }
		
	const Actor *PCController::actor() const {
		return m_world.refEntity<Actor>(m_pc.entityRef());
	}
	
	void PCController::sendOrder(game::POrder &&order) {
		m_world.sendOrder(std::move(order), m_pc.entityRef());
	}
		
	bool PCController::hasActor() const {
		return actor() != nullptr;
	}

	bool PCController::canChangeStance() const {
		return hasActor();
	}

	void PCController::setStance(Stance::Type stance) {
		DASSERT(canChangeStance());
		sendOrder(new ChangeStanceOrder(stance));
		m_target_stance = stance;
	}
		
	void PCController::reload() {
		const Actor *actor = this->actor();
		if(!actor)
			return;

		const ActorInventory &inventory = actor->inventory();
		const Weapon &weapon = inventory.weapon();
		if(weapon.needAmmo() && inventory.ammo().count < weapon.maxAmmo()) {
			int item_id = inventory.find(inventory.ammo().item);
			if(item_id == -1) for(int n = 0; n < inventory.size(); n++)
				if(weapon.canUseAmmo(inventory[n].item)) {
					item_id = n;
					break;
				}
			if(item_id != -1)
				sendOrder(new EquipItemOrder(inventory[item_id].item));
		}
	}

}