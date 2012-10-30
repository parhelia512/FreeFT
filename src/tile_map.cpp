#include "tile_map.h"
#include "gfx/tile.h"
#include "gfx/device.h"
#include <algorithm>
#include "sys/profiler.h"

TileInstance::TileInstance(const gfx::Tile *tile, const int3 &pos)
	:m_flags(0), m_tile(tile) {
	DAssert(m_tile);
	setPos(pos);
}


int3 TileInstance::pos() const {
	return int3(int(m_xz) & 15, int(m_y), int(m_xz) >> 4);
}

IBox TileInstance::boundingBox() const {
	int3 pos = this->pos();
	return IBox(pos, pos + m_tile->bbox);
}

IRect TileInstance::screenRect() const {
	return m_tile->GetBounds() + int2(WorldToScreen(pos()));
}

void TileInstance::setPos(int3 pos) {
	DAssert(pos.x < TileMapNode::sizeX && pos.y < TileMapNode::sizeY && pos.z < TileMapNode::sizeZ);
	DAssert(pos.x >= 0 && pos.y >= 0 && pos.z >= 0);
	m_xz = u8(pos.x | (pos.z << 4));
	m_y  = u8(pos.y);
}

void TileMap::resize(int2 newSize) {
	clear();

	//TODO: properly covert m_nodes to new coordinates
	m_size = int2(newSize.x / Node::sizeX, newSize.y / Node::sizeZ);
	m_nodes.resize(m_size.x * m_size.y);
}

void TileMap::clear() {
	m_size = int2(0, 0);
	m_nodes.clear();
}

bool TileMapNode::isColliding(const IBox &box) const {
	if(!m_instances.size() || !Overlaps(box, m_bounding_box))
		return false;

	for(uint i = 0; i < m_instances.size(); i++) {
		const TileInstance &instance = m_instances[i];
		DAssert(instance.m_tile);

		int3 tilePos = instance.pos();
		IBox tileBox(tilePos, tilePos + instance.m_tile->bbox);

		if(Overlaps(tileBox, box))
			return true;
	}

	return false;
}

bool TileMapNode::isInside(const int3 &point) const {
	return IBox(0, 0, 0, sizeX, sizeY, sizeZ).IsInside(point);
}
	
const TileInstance *TileMapNode::at(int3 pos) const {
	for(int n = 0; n < (int)m_instances.size(); n++)
		if(m_instances[n].pos() == pos)
			return &m_instances[n];
	return nullptr;
}

void TileMapNode::addTile(const gfx::Tile &tile, int3 pos, bool test_for_collision) {
	if(test_for_collision && isColliding(IBox(pos, pos + tile.bbox)))
		return;
	
	DAssert(isInside(pos));

	TileInstance inst(&tile, pos);
	m_instances.push_back(inst);

	std::sort(m_instances.begin(), m_instances.end());

	m_screen_rect += inst.screenRect();
	m_bounding_box += inst.boundingBox();
}

void TileMapNode::select(const IBox &box, SelectionMode::Type mode) {
	if(!m_instances.size())
		return;
		
	if(mode == SelectionMode::normal) {
		for(uint i = 0; i < m_instances.size(); i++) {
			TileInstance &instance = m_instances[i];
			instance.select(Overlaps(box, instance.boundingBox()));
		}
	}
	else if(mode == SelectionMode::add) {
		for(uint i = 0; i < m_instances.size(); i++) {
			TileInstance &instance = m_instances[i];
			bool overlaps = Overlaps(box, instance.boundingBox());
			instance.select(overlaps || instance.isSelected());
		}
	}
	else if(mode == SelectionMode::subtract && m_any_selected) {
		for(uint i = 0; i < m_instances.size(); i++) {
			TileInstance &instance = m_instances[i];
			bool overlaps = Overlaps(box, instance.boundingBox());
			instance.select(!overlaps && instance.isSelected());
		}
	}
	
	m_any_selected = false;
	for(uint i = 0; i < m_instances.size(); i++)
		m_any_selected |= m_instances[i].isSelected();
}

void TileMapNode::deleteSelected() {
	if(!m_any_selected)
		return;

	for(uint i = 0; i < m_instances.size(); i++)
		if(m_instances[i].isSelected()) {
			m_instances[i--] = m_instances.back();
			m_instances.pop_back();
		}

	if(m_instances.size()) {
		m_screen_rect = m_instances[0].screenRect();
		m_bounding_box = m_instances[0].boundingBox();

		for(uint n = 1; n < m_instances.size(); n++) {
			m_screen_rect += m_instances[n].screenRect();
			m_bounding_box += m_instances[n].boundingBox();
		}

		sort(m_instances.begin(), m_instances.end());
	}
	else {
		m_screen_rect = IRect(0, 0, 0, 0);
		m_bounding_box = IBox(0, 0, 0, 0, 0, 0);
	}
	
	m_any_selected = false;
}

using namespace rapidxml;

void TileMap::loadFromXML(const XMLDocument &doc) {
	XMLNode *mnode = doc.first_node("map");
	Assert(mnode);
	int2 size(getIntAttribute(mnode, "size_x"), getIntAttribute(mnode, "size_y"));
	Assert(size.x > 0 && size.y > 0 && size.x <= 16 * 1024 && size.y <= 16 * 1024);
	resize(size);

	std::map<int, const gfx::Tile*> tile_indices; //TODO: convert to vector

	XMLNode *tnode = doc.first_node("tile");
	while(tnode) {
		tile_indices[getIntAttribute(tnode, "id")] = &*gfx::Tile::mgr[tnode->value()];
		tnode = tnode->next_sibling("tile"); 
	}

	XMLNode *inode = doc.first_node("instance");
	while(inode) {
		int id = getIntAttribute(inode, "id");
		int3 pos(getIntAttribute(inode, "pos_x"), getIntAttribute(inode, "pos_y"), getIntAttribute(inode, "pos_z"));
		auto it = tile_indices.find(id);
		Assert(it != tile_indices.end());

		addTile(*it->second, pos, false);

		inode = inode->next_sibling("instance");
	}
}

void TileMap::saveToXML(XMLDocument &doc) const {
	std::map<const gfx::Tile*, int> tile_indices;

	XMLNode *mnode = doc.allocate_node(node_element, "map");
	doc.append_node(mnode);
	addAttribute(mnode, "size_x", m_size.x * Node::sizeX);
	addAttribute(mnode, "size_y", m_size.y * Node::sizeZ);

	for(int n = 0; n < (int)m_nodes.size(); n++) {
		const TileMapNode &node = m_nodes[n];
		for(int i = 0; i < (int)node.m_instances.size(); i++) {
			const gfx::Tile *tile = node.m_instances[i].m_tile;
			auto it = tile_indices.find(tile);
			if(it == tile_indices.end())
				tile_indices[tile] = (int)tile_indices.size();
		}
	}

	for(auto it = tile_indices.begin(); it != tile_indices.end(); ++it) {
		XMLNode *node = doc.allocate_node(node_element, "tile", doc.allocate_string(it->first->name.c_str()));
		doc.append_node(node);
		addAttribute(node, "id", it->second);
	}

	for(int n = 0; n < (int)m_nodes.size(); n++) {
		const TileMapNode &node = m_nodes[n];
		int3 node_pos = nodePos(n);

		for(int i = 0; i < (int)node.m_instances.size(); i++) {
			const TileInstance &inst = node.m_instances[i];
			int id = tile_indices[inst.m_tile];
			int3 pos = node_pos + inst.pos();
			XMLNode *node = doc.allocate_node(node_element, "instance");
			doc.append_node(node);
			addAttribute(node, "id", id);
			addAttribute(node, "pos_x", pos.x);
			addAttribute(node, "pos_y", pos.y);
			addAttribute(node, "pos_z", pos.z);
		}
	}
}

IBox TileMap::boundingBox() const {
	return IBox(0, 0, 0, m_size.x * Node::sizeX, 64, m_size.y * Node::sizeZ);
}

void TileMap::render(const IRect &view) const {
	PROFILE(tRendering)

	gfx::DTexture::Bind0();
	gfx::DrawBBox(boundingBox());

	int vNodes = 0, vTiles = 0;
	{
		PROFILE(tRenderingPreparation)
	}
	//TODO: selecting visible nodes
	//TODO: sorting tiles
	
	for(uint n = 0; n < m_nodes.size(); n++) {
		const Node &node = m_nodes[n];
		if(!node.instanceCount())
			continue;

		int3 node_pos = nodePos(n);
		IRect screenRect = node.screenRect() + WorldToScreen(node_pos);
		// possible error from rounding node & tile positions
		screenRect.min -= int2(2, 2);
		screenRect.max += int2(2, 2);
//		if(!Overlaps(screenRect, view))
//			continue;
		vNodes++;

		for(uint i = 0; i < node.m_instances.size(); i++) {
			const TileInstance &instance = node.m_instances[i];
			const gfx::Tile *tile = instance.m_tile;
			int3 pos = instance.pos() + node_pos;
			int2 screenPos = WorldToScreen(pos);

			vTiles++;
			tile->Draw(screenPos);
			if(instance.isSelected()) {
				gfx::DTexture::Bind0();
				gfx::DrawBBox(IBox(pos, pos + tile->bbox));
			}
		}
	}

	Profiler::UpdateCounter(Profiler::cRenderedNodes, vNodes);
	Profiler::UpdateCounter(Profiler::cRenderedTiles, vTiles);
}

void TileMap::addTile(const gfx::Tile &tile, int3 pos, bool test_for_collision) {
	if(!testPosition(pos, tile.bbox))
		return;

	int2 nodeCoord(pos.x / Node::sizeX, pos.z / Node::sizeZ);
	int3 node_pos = pos - int3(nodeCoord.x * Node::sizeX, 0, nodeCoord.y * Node::sizeZ);

	(*this)(nodeCoord).addTile(tile, node_pos, test_for_collision);
}

bool TileMap::testPosition(int3 pos, int3 box) const {
	IRect nodeRect(pos.x / Node::sizeX, pos.z / Node::sizeZ,
					(pos.x + box.x - 1) / Node::sizeX, (pos.z + box.x - 1) / Node::sizeZ);
	IBox worldBox(pos, pos + box);

	if(pos.x < 0 || pos.y < 0 || pos.z < 0)
		return false;

	if(worldBox.max.x > m_size.x * Node::sizeX ||
		worldBox.max.y > Node::sizeY || worldBox.max.z > m_size.y * Node::sizeZ)
		return false;

	for(uint n = 0; n < m_nodes.size(); n++) {
		IBox bbox(pos, pos + box);
		bbox -= nodePos(n);
		if(m_nodes[n].isColliding(bbox))
			return false;
	}

	return true;
}

void TileMap::drawPlacingHelpers(const gfx::Tile &tile, int3 pos) const {
	bool collides = !testPosition(pos, tile.bbox);

	Color color = collides? Color(255, 0, 0) : Color(255, 255, 255);

	tile.Draw(int2(WorldToScreen(pos)), color);
	gfx::DTexture::Bind0();
	gfx::DrawBBox(IBox(pos, pos + tile.bbox));
}

void TileMap::drawBoxHelpers(const IBox &box) const {
	gfx::DTexture::Bind0();

	int3 pos = box.min, bbox = box.max - box.min;
	int3 tsize(m_size.x * Node::sizeX, Node::sizeY, m_size.y * Node::sizeZ);

	gfx::DrawLine(int3(0, pos.y, pos.z), int3(tsize.x, pos.y, pos.z), Color(0, 255, 0, 127));
	gfx::DrawLine(int3(0, pos.y, pos.z + bbox.z), int3(tsize.x, pos.y, pos.z + bbox.z), Color(0, 255, 0, 127));
	
	gfx::DrawLine(int3(pos.x, pos.y, 0), int3(pos.x, pos.y, tsize.z), Color(0, 255, 0, 127));
	gfx::DrawLine(int3(pos.x + bbox.x, pos.y, 0), int3(pos.x + bbox.x, pos.y, tsize.z), Color(0, 255, 0, 127));

	int3 tpos(pos.x, 0, pos.z);
	gfx::DrawBBox(IBox(tpos, tpos + int3(bbox.x, pos.y, bbox.z)), Color(0, 0, 255, 127));
	
	gfx::DrawLine(int3(0, 0, pos.z), int3(tsize.x, 0, pos.z), Color(0, 0, 255, 127));
	gfx::DrawLine(int3(0, 0, pos.z + bbox.z), int3(tsize.x, 0, pos.z + bbox.z), Color(0, 0, 255, 127));
	
	gfx::DrawLine(int3(pos.x, 0, 0), int3(pos.x, 0, tsize.z), Color(0, 0, 255, 127));
	gfx::DrawLine(int3(pos.x + bbox.x, 0, 0), int3(pos.x + bbox.x, 0, tsize.z), Color(0, 0, 255, 127));
}

void TileMap::fill(const gfx::Tile &tile, const IBox &box) {
	int3 bbox = tile.bbox;

	for(int x = box.min.x; x < box.max.x; x += bbox.x)
		for(int y = box.min.y; y < box.max.y; y += bbox.y)
			for(int z = box.min.z; z < box.max.z; z += bbox.z) {
				try { addTile(tile, int3(x, y, z)); }
				catch(...) { }
			}
}

void PrintBox(IBox box) {
	printf("%d %d %d %d %d %d",
			box.min.x, box.min.y, box.min.z,
			box.max.x, box.max.y, box.max.z);
}

void TileMap::select(const IBox &box, SelectionMode::Type mode) {
	for(uint n = 0, count = m_nodes.size(); n < count; n++)
		m_nodes[n].select(box - nodePos(n), mode);
	//TODO: faster selection for other modes
}

const TileInstance *TileMap::at(int3 pos) const {
	int id = pos.x / Node::sizeX + (pos.z / Node::sizeZ) * m_size.x;
	DAssert(id >= 0 && id < (int)m_nodes.size());

	return m_nodes[id].at(pos - nodePos(id));
}

void TileMap::deleteSelected() {
	for(uint n = 0, count = m_nodes.size(); n < count; n++)
		m_nodes[n].deleteSelected();
}

void TileMap::moveSelected(int3 offset) {
	//TODO: write me
}
