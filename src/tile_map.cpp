#include "tile_map.h"
#include "gfx/tile.h"
#include "gfx/scene_renderer.h"
#include "sys/profiler.h"
#include "sys/xml.h"
#include <algorithm>


TileMap::TileMap(const int2 &dimensions)
	:Grid(dimensions), m_occluder_map(this) { }

void TileMap::resize(const int2 &new_size) {
	TileMap new_map(new_size);
	for(int n = 0; n < size(); n++) {
		const ObjectDef &obj = Grid::operator[](n);
		if(obj.ptr && new_map.isInside(obj.bbox))
			new_map.Grid::add(obj);
	}
	Grid::swap(new_map);
}

int TileMap::add(const gfx::Tile *tile, const int3 &pos) {
	DASSERT(tile);

	FBox bbox(pos, pos + tile->bboxSize());
	IRect rect = tile->rect() + worldToScreen(pos);
	ASSERT(findAny(bbox) == -1);
	return Grid::add(ObjectDef(tile, bbox, rect, -1));
}

void TileMap::remove(int idx) {
	DASSERT(idx >= 0 && idx < size());

	//TODO: speed up somehow?
	if((*this)[idx].ptr) {
		int occluder_id = (*this)[idx].occluder_id;
		OccluderMap::Occluder &occluder = m_occluder_map[occluder_id];
		for(int n = 0; n < (int)occluder.objects.size(); n++)
			if(occluder.objects[n] == idx) {
				occluder.objects[n] = occluder.objects.back();
				occluder.objects.pop_back();
			}
	}
	Grid::remove(idx);
}
	
int TileMap::pixelIntersect(const int2 &pos, int flags) const {
	return Grid::pixelIntersect(pos,
		[](const Grid::ObjectDef &object, const int2 &pos)
			{ return ((const gfx::Tile*)object.ptr)->testPixel(pos - worldToScreen((int3)object.bbox.min)); },
		   flags);
}

void TileMap::updateVisibility(const FBox &main_bbox) {
	m_occluder_map.updateVisibility(main_bbox);

	//TODO: update only occluders that has changed
	for(int n = 0; n < size(); n++) {
		Object &object = m_objects[n];
		if(object.ptr && object.occluder_id != -1) {
			if(m_occluder_map[object.occluder_id].is_visible)
				object.flags |= visibility_flag;
			else
				object.flags &= ~visibility_flag;
		}
	}
	updateNodes();
}

void TileMap::loadFromXML(const XMLDocument &doc) {
	XMLNode main_node = doc.child("tile_map");
	ASSERT(main_node);

	clear();

	int2 size = main_node.int2Attrib("size");
	int tile_count = main_node.intAttrib("tile_count");

	ASSERT(size.x > 0 && size.y > 0 && size.x <= 16 * 1024 && size.y <= 16 * 1024);
	resize(size);

	XMLNode tnode = main_node.child("tile");
	while(tnode) {
		const gfx::Tile *tile = &*gfx::Tile::mgr[tnode.attrib("name")];
		XMLNode inode = tnode.child("i");
		while(inode) {
			int3 pos = inode.int3Attrib("pos");
			add(tile, pos);
			inode = inode.sibling("i");
		}
		tnode = tnode.sibling("tile");
	}

	m_occluder_map.loadFromXML(doc);
}

void TileMap::saveToXML(XMLDocument &doc) const {
	XMLNode main_node = doc.addChild("tile_map");
	main_node.addAttrib("size", dimensions());

	std::vector<int> indices;
	indices.reserve(size());
	for(int n = 0; n < size(); n++)
		if((*this)[n].ptr)
			indices.push_back(n);
	main_node.addAttrib("tile_count", (int)indices.size());

	std::sort(indices.begin(), indices.end(), [this](int a, int b) {
		const auto &obj1 = (*this)[a];
		const auto &obj2 = (*this)[b];

		int cmp = strcmp(obj1.ptr->name.c_str(), obj2.ptr->name.c_str());
		if(cmp == 0) {
			const float3 p1 = obj1.bbox.min, p2 = obj2.bbox.min;
			return p1.x == p2.x? p1.y == p2.y? p1.z < p2.z : p1.y < p2.y : p1.x < p2.x;
		}

		return cmp < 0;
	} );
	
	const gfx::Tile *prev = nullptr;
	XMLNode tile_node;

	PodArray<int> tile_ids(size());
	for(int n = 0; n < size(); n++)
		tile_ids[n] = -1;
	int object_id = 0;

	for(int n = 0; n < (int)indices.size(); n++) {
		const TileDef &object = (*this)[indices[n]];
		tile_ids[indices[n]] = object_id++;

		if(object.ptr != prev) {
			tile_node = main_node.addChild("tile");
			ASSERT(!object.ptr->name.empty());
		   	tile_node.addAttrib("name", doc.own(object.ptr->name.c_str()));
			prev = object.ptr;
		}

		XMLNode instance = tile_node.addChild("i");
		instance.addAttrib("pos", int3(object.bbox.min));
	}

	m_occluder_map.saveToXML(tile_ids, doc);
}

void TileMap::serialize(Serializer &sr) {
	XMLDocument doc;
	if(sr.isSaving())
		saveToXML(doc);
	sr & doc;
	if(sr.isLoading())
		loadFromXML(doc);
}
