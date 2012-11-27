#include "actor.h"
#include "gfx/device.h"
#include "gfx/scene_renderer.h"
#include "tile_map.h"
#include "navigation_map.h"
#include <cmath>
#include <cstdio>

using namespace gfx;

namespace ActionId {
	static const bool s_is_looped[count] = {
		true,
		true,
		true,

		false,
		false,

		false,
		false,
	};

	bool isLooped(Type val) {
		DASSERT(val >= 0 && val < count);
		return s_is_looped[val];
	}
}


Order moveOrder(int3 target_pos, bool run)	{
	Order new_order(OrderId::move);
	new_order.move = Order::Move{target_pos, run};
	return new_order;
}
Order doNothingOrder() {
	Order new_order(OrderId::do_nothing);
	return new_order;
}
Order changeStanceOrder(int next_stance) {
	Order new_order(OrderId::change_stance);
	new_order.change_stance = Order::ChangeStance{next_stance};
	return new_order;
}
Order attackOrder(int attack_mode, const int3 &target_pos) {
	Order new_order(OrderId::attack);
	new_order.attack = Order::Attack{target_pos, attack_mode};
	return new_order;
}
Order changeWeaponOrder(WeaponClassId::Type target_weapon) {
	Order new_order(OrderId::change_weapon);
	new_order.change_weapon = Order::ChangeWeapon{target_weapon};
	return new_order;
}

static const char *s_wep_names[WeaponClassId::count] = {
	"",
	"Club",
	"Heavy",
	"Knife",
	"Minigun",
	"Pistol",
	"Rifle",
	"Rocket",
	"SMG",
	"Spear",
};

static const char *s_attack_names[WeaponClassId::count * 2] = {
	"UnarmedOne",
	"UnarmedTwo",
	"ClubSwing",
	nullptr,
	"HeavyBurst",
	"HeavySingle",
	"KnifeSlash",
	nullptr,
	"MinigunBurst",
	nullptr,
	"PistolSingle",
	nullptr,
	"RifleBurst",
	"RifleSingle", 
	"RocketSingle",
	nullptr,
	"SMGBurst",
	"SMGSingle",
	"SpearThrow",
	nullptr,
};

// W konstruktorze wyszukac wszystkie animacje i trzymac id-ki zamiast nazw
static const char *s_seq_names[ActionId::count][StanceId::count] = {
	// standing, 		crouching,			prone,
	{ "Stand%s",		"Crouch%s",			"Prone%s" },			// standing
	{ "StandWalk%s",	"CrouchWalk",		"ProneWalk" },		// walking
	{ "StandRun",		nullptr,			nullptr },			// running

	{ nullptr,			"CrouchStand", 		"ProneCrouch" },	// stance up
	{ "StandCrouch",	"CrouchProne", 		nullptr },			// stance down

	{ "StandAttack%s",	"CrouchAttack%s",	"ProneAttack%s" },		// attack 1
	{ "StandAttack%s",	"CrouchAttack%s",	"ProneAttack%s" },		// attack 2
};

AnimationMap::AnimationMap(gfx::PSprite sprite) {
	DASSERT(sprite);
	m_seq_ids.resize(ActionId::count * WeaponClassId::count * StanceId::count, -1);

	for(int a = 0; a < ActionId::count; a++)
		for(int w = 0; w < WeaponClassId::count; w++)
			for(int s = 0; s < StanceId::count; s++) {
				const string &name = sequenceName((StanceId::Type)s, (ActionId::Type)a, (WeaponClassId::Type)w);
				if(!name.empty())
					m_seq_ids[s + (a * WeaponClassId::count + w) * StanceId::count] =
						sprite->findSequence(name.c_str());
			}
}

string AnimationMap::sequenceName(StanceId::Type stance, ActionId::Type action, WeaponClassId::Type weapon) const {
	char seq_name[64] = "";

	if(action == ActionId::attack1 || action == ActionId::attack2) {
		int attack_id = weapon * 2 + (action == ActionId::attack1? 0 : 1);
		if(s_attack_names[attack_id])
			snprintf(seq_name, sizeof(seq_name), s_seq_names[action][stance], s_attack_names[attack_id]);
	}
	else if(s_seq_names[action][stance])
		snprintf(seq_name, sizeof(seq_name), s_seq_names[action][stance], s_wep_names[weapon]);

	return seq_name;
}

int AnimationMap::sequenceId(StanceId::Type stance, ActionId::Type action, WeaponClassId::Type weapon) const {
	if(m_seq_ids.empty())
		return -1;
	return m_seq_ids[stance + (action * WeaponClassId::count + weapon) * StanceId::count];
}

Actor::Actor(const char *spr_name, int3 pos) :Entity(int3(1, 1, 1), pos) {
	m_sprite = Sprite::mgr[spr_name];
	m_sprite->printInfo();
	m_anim_map = AnimationMap(m_sprite);

	m_bbox = m_sprite->m_bbox;
	m_issue_next_order = false;
	m_stance_id = StanceId::standing;

	m_weapon_id = WeaponClassId::unarmed;
	setSequence(ActionId::standing);
	lookAt(int3(0, 0, 0));
	m_order = m_next_order = doNothingOrder();
}

void Actor::setNextOrder(const Order &order) {
	m_next_order = order;
}

void Actor::think(double current_time, double time_delta) {
	if(m_issue_next_order)
		issueNextOrder();

	OrderId::Type order_id = m_order.id;
	if(order_id == OrderId::do_nothing) {
		fixPos();
		m_issue_next_order = true;
	}
	else if(order_id == OrderId::move) {
		DASSERT(!m_path.empty() && m_path_pos >= 0 && m_path_pos < (int)m_path.size());
		
		float speed = m_order.move.run? 20.0f:
			m_stance_id == StanceId::standing? 10.0f : m_stance_id == StanceId::crouching? 6.0f : 3.5f;

		float dist = speed * time_delta;
		m_issue_next_order = false;

		while(dist > 0.0001f) {
			int3 target = m_path[m_path_pos];
			int3 diff = target - m_last_pos;
			float3 diff_vec(diff); diff_vec = diff_vec / length(diff_vec);
			float3 cur_pos = float3(m_last_pos) + float3(diff) * m_path_t;
			float tdist = distance(float3(target), cur_pos);

			if(tdist < dist) {
				dist -= tdist;
				m_last_pos = target;
				m_path_t = 0.0f;

				if(++m_path_pos == (int)m_path.size() || m_next_order.id != OrderId::do_nothing) {
					lookAt(target);
					setPos(target);
					m_path.clear();
					m_issue_next_order = true;
					break;
				}
			}
			else {
				float new_x = cur_pos.x + diff_vec.x * dist;
				float new_z = cur_pos.z + diff_vec.z * dist;
				m_path_t = diff.x? (new_x - m_last_pos.x) / float(diff.x) : (new_z - m_last_pos.z) / float(diff.z);
				float3 new_pos = (float3)m_last_pos + float3(diff) * m_path_t;
				lookAt(target);
				setPos(new_pos);
				break;
			}
		}
	}

	animate(current_time);
}

void Actor::addToRender(gfx::SceneRenderer &out) const {
	Sprite::Rect rect;

	//TODO: do not allocate texture every frame
	PTexture spr_tex = new DTexture;
	gfx::Texture tex = m_sprite->getFrame(m_seq_id, m_frame_id, m_dir, &rect);
	spr_tex->setSurface(tex);

	out.add(spr_tex, IRect(rect.left, rect.top, rect.right, rect.bottom) - m_sprite->m_offset, m_pos, m_bbox);
	ASSERT(!m_tile_map->isOverlapping(boundingBox()));
//	out.addBox(boundingBox(), m_tile_map && m_tile_map->isOverlapping(boundingBox())?
//				Color(255, 0, 0) : Color(255, 255, 255));
}

void Actor::issueNextOrder() {
	if(m_order.id == OrderId::change_stance) {
		m_stance_id = (StanceId::Type)(m_stance_id - m_order.change_stance.next_stance);
		//TODO: different bboxes for stances
//		m_bbox = m_sprite->m_bbox;
//		if(m_stance_id == StanceId::crouching && m_bbox.y == 9)
//			m_bbox.y = 5;
//		if(m_stance_id == StanceId::prone && m_bbox.y == 9)
//			m_bbox.y = 2;
		DASSERT(m_stance_id >= 0 && m_stance_id < StanceId::count);
	}

	if(m_next_order.id == OrderId::move)
		issueMoveOrder();
	else if(m_next_order.id == OrderId::change_weapon) {
		m_order = doNothingOrder();
		setWeapon(m_next_order.change_weapon.target_weapon);
	}
	else {
		if(m_next_order.id == OrderId::change_stance) {
			int next_stance = m_next_order.change_stance.next_stance;

			if(next_stance > 0 && m_stance_id != StanceId::standing)
				setSequence(ActionId::stance_up);
			else if(next_stance < 0 && m_stance_id != StanceId::prone)
				setSequence(ActionId::stance_down);
			else
				m_next_order = doNothingOrder();
		}
		else if(m_next_order.id == OrderId::attack) {
			fixPos();
			lookAt(m_next_order.attack.target_pos);
			setSequence(ActionId::attack1);
		}

		m_order = m_next_order;
	}
	
	if(m_order.id == OrderId::do_nothing)	
		setSequence(ActionId::standing);

	m_issue_next_order = false;
	m_next_order = doNothingOrder();
}

void Actor::issueMoveOrder() {
	OrderId::Type order_id = m_next_order.id;
	int3 new_pos = m_next_order.move.target_pos;
	DASSERT(order_id == OrderId::move && m_navigation_map);

	new_pos = max(new_pos, int3(0, 0, 0)); //TODO: clamp to map extents

	int3 cur_pos = (int3)m_pos;
	vector<int2> tmp_path = m_navigation_map->findPath(cur_pos.xz(), new_pos.xz());

	if(cur_pos == new_pos || tmp_path.empty()) {
		m_order = doNothingOrder();
		return;
	}

	m_last_pos = cur_pos;

	m_path.clear();
	m_path_t = 0;
	m_path_pos = 0;
	fixPos();

	m_order = m_next_order;

	for(int n = 1; n < (int)tmp_path.size(); n++) {
		cur_pos = asXZY(tmp_path[n - 1], 1);
		new_pos = asXZY(tmp_path[n], 1);
		if(new_pos == cur_pos)
			continue;

		MoveVector mvec(tmp_path[n - 1], tmp_path[n]);

		while(mvec.dx) {
			int step = min(mvec.dx, 3);
			m_path.push_back(cur_pos += int3(mvec.vec.x * step, 0, 0));
			mvec.dx -= step;
		}
		while(mvec.dy) {
			int step = min(mvec.dy, 3);
			m_path.push_back(cur_pos += int3(0, 0, mvec.vec.y * step));
			mvec.dy -= step;
		}
		while(mvec.ddiag) {
			int dstep = min(mvec.ddiag, 3);
			m_path.push_back(cur_pos += asXZ(mvec.vec) * dstep);
			mvec.ddiag -= dstep;
		}
	}
		
	if(m_path.size() <= 1 || m_stance_id != StanceId::standing)
		m_order.move.run = 0;
	setSequence(m_order.move.run? ActionId::running : ActionId::walking);

	DASSERT(!m_path.empty());
}

void Actor::setWeapon(WeaponClassId::Type weapon) {
	m_weapon_id = weapon;
	setSequence(m_action_id);
}

// sets seq_id, frame_id and seq_name
void Actor::setSequence(ActionId::Type action_id) {
	DASSERT(action_id < ActionId::count && m_stance_id < StanceId::count);
	
	int seq_id = m_anim_map.sequenceId(m_stance_id, action_id, m_weapon_id);
	if(seq_id == -1) {
		seq_id = m_anim_map.sequenceId(m_stance_id, action_id, WeaponClassId::unarmed);
		if(seq_id == -1)
			printf("Sequence: %s not found!\n", m_anim_map.sequenceName(m_stance_id, action_id, m_weapon_id).c_str());
		ASSERT(seq_id != -1);
	}

	bool reset_anim = true;
	if(action_id == ActionId::walking || action_id == ActionId::running)
		if(m_action_id == action_id && m_seq_id == seq_id)
			reset_anim = false;

	m_action_id = action_id;
	m_seq_id = seq_id;
	if(reset_anim)
		m_frame_id = 0;
	m_looped_anim = ActionId::isLooped(action_id);
}

// sets direction
void Actor::lookAt(int3 pos) { //TODO: rounding
	float2 dir(pos.x - m_pos.x, pos.z - m_pos.z);
	dir = dir / length(dir);

	int dx = 0, dz = 0;

	if(fabs(dir.x) > 0.382683432f)
		dx = dir.x < 0? -1 : 1;
	if(fabs(dir.y) > 0.382683432f)
		dz = dir.y < 0? -1 : 1;

	//TODO: sprites have varying number of directions
	// maybe a vector (int2) should be passed to sprite, and it should
	// figure out a best direction by itself
	m_dir = Sprite::findDir(dx, dz);
}

void Actor::animate(double current_time) {
	//const Sprite::Sequence &seq	= m_sprite->m_sequences[m_seq_id];
	//const Sprite::Animation &anim = m_sprite->m_anims[seq.m_anim_id];
	
	double diff_time = current_time - m_last_time;
	if(diff_time > 1 / 15.0) {
		int frame_count = m_sprite->frameCount(m_seq_id);
		int next_frame_id = (m_frame_id + 1) % frame_count;
		m_last_time = current_time;

		bool finished = next_frame_id < m_frame_id;
		if(finished)
			onAnimFinished();

		if(!finished || m_looped_anim)
			m_frame_id = next_frame_id;
	}
}

void Actor::onAnimFinished() {
	if(m_order.id == OrderId::change_stance || m_order.id == OrderId::attack)
		m_issue_next_order = true;
}

void Actor::printStatusInfo() const {
	printf("order:%d next:%d seq:%d\n", (int)m_order.id, (int)m_next_order.id, (int)m_seq_id);
}
