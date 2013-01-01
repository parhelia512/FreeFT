#include "gfx/sprite.h"
#include <cstring>
#include <cstdio>

namespace gfx
{
			
	int Sprite::MultiPalette::size(int layer) const {
		int next = layer == layer_count - 1? (int)colors.size() - offset[layer_count - 1] : offset[layer + 1];
		return next - offset[layer];
	}

	const Color *Sprite::MultiPalette::access(int layer) const {
		return colors.data() + offset[layer];
	}

	bool Sprite::MultiPalette::operator==(const Sprite::MultiPalette &rhs) const {
		return colors == rhs.colors && memcmp(offset, rhs.offset, sizeof(offset)) == 0;
	}

	int Sprite::Frame::paramCount(char id) {
		if(id == ev_stop_anim || id == ev_jump_to_frame || id == ev_time_of_display)
			return 1;
		if(id == ev_fire)
			return 3;
		return 0;
	}

	Sprite::Sprite() :m_bbox(0, 0, 0), m_offset(0, 0) { }

	void Sprite::Sequence::serialize(Serializer &sr) {
		sr & name;
		sr(frame_count, dir_count, first_frame, palette_id);
	}

	void Sprite::MultiPalette::serialize(Serializer &sr) {
		sr & colors;
		sr.data(offset, sizeof(offset));
	}

	void Sprite::MultiImage::serialize(Serializer &sr) {
		for(int l = 0; l < layer_count; l++)
			sr & images[l];
		sr.data(points, sizeof(points));
		sr & rect;

		if(sr.isLoading())
			bitmap.clear();
	}

	void Sprite::serialize(Serializer &sr) {
		sr.signature("SPRITE", 6);
		//TODO optimize format (add compression)

		if(sr.isLoading())
			m_name = sr.name();
		sr & m_sequences & m_frames & m_palettes & m_images;
		sr(m_offset, m_bbox);
	}

	void Sprite::clear() {
		m_sequences.clear();
		m_frames.clear();
		m_palettes.clear();
		m_images.clear();

		m_name.clear();

		m_offset = int2(0, 0);
		m_bbox = int3(0, 0, 0);
	}

	Texture Sprite::MultiImage::toTexture(const MultiPalette &palette) const {
		Texture out;

		//TODO: there are still some bugs here
		int2 size(0, 0);
		for(int l = 0; l < layer_count; l++)
			size = max(size, points[l] + images[l].dimensions());

		out.resize(size.x, size.y);

		for(int l = 0; l < layer_count; l++) {
			if(!images[l].width() || !images[l].height())
				continue;

			int2 lsize = images[l].dimensions();

			const Color *pal = palette.access(l);
			int pal_size = palette.size(l);
			//TODO: add assertions that colors[x] < palette_size

			PalTexture tex;
			images[l].decompress(tex);

			const u8 *colors = &tex.m_colors[0];
			const u8 *alphas = &tex.m_alphas[0];
			Color *dst = &out(points[l].x, points[l].y);

			for(int y = 0; y < lsize.y; y++) {
				for(int x = 0; x < lsize.x; x++) {
					if(alphas[x]) {
						if(dst[x].a) {
							float4 dstCol = dst[x];
							float4 srcCol = pal[colors[x]];
							float4 col = (srcCol - dstCol) * float(alphas[x]) * (1.0f / 255.0f) + dstCol;
							dst[x] = Color(col.x, col.y, col.z, 1.0f);
						}
						else {
							dst[x] = pal[colors[x]];
							dst[x].a = alphas[x];
						}
					}
				}

				colors += lsize.x;
				alphas += lsize.x;
				dst += size.x;
			}
		}

		return out;
	}

	bool Sprite::MultiImage::testPixel(const int2 &screen_pos) const {
		//TODO: rect test, bitmap test
		return rect.isInside(screen_pos);
	}

	bool Sprite::isSequenceLooped(int seq_id) const {
		const Sequence &seq = m_sequences[seq_id];
		for(int n = 0; n < seq.frame_count; n++)
			if(m_frames[seq.first_frame + n].id == ev_repeat_all)
				return true;
		return false;
	}

	int Sprite::imageIndex(int seq_id, int frame_id, int dir_id) const {
		DASSERT(seq_id >= 0 && seq_id < (int)m_sequences.size());

		const Sequence &seq = m_sequences[seq_id];
		DASSERT(frame_id >= 0 && frame_id < seq.frame_count);
		DASSERT(dir_id >= 0 && dir_id < seq.dir_count);

		const Frame &frame = m_frames[seq.first_frame + frame_id];
		DASSERT(frame.id >= 0);

		int out = frame.id + dir_id;
		DASSERT(out < (int)m_images.size());
		return out;
	}

	Texture Sprite::getFrame(int seq_id, int frame_id, int dir_id) const {
		const MultiPalette &palette = m_palettes[m_sequences[seq_id].palette_id];
		return m_images[imageIndex(seq_id, frame_id, dir_id)].toTexture(palette);
	}

	IRect Sprite::getRect(int seq_id, int frame_id, int dir_id) const {
		return m_images[imageIndex(seq_id, frame_id, dir_id)].rect - m_offset;
	}

	IRect Sprite::getMaxRect() const {
		if(m_images.empty())
			return IRect::empty();

		IRect out = m_images[0].rect;
		for(int n = 1; n < (int)m_images.size(); n++)
			out = sum(out, m_images[n].rect);
		return out - m_offset;
	}
		
	bool Sprite::testPixel(const int2 &screen_pos, int seq_id, int frame_id, int dir_id) const {
		const MultiImage &image = m_images[imageIndex(seq_id, frame_id, dir_id)];
		return image.testPixel(screen_pos + m_offset);
	}

	int Sprite::findSequence(const char *name) const {
		for(int n = 0; n < (int)m_sequences.size(); n++)
			if(m_sequences[n].name == name)
				return n;
		return -1;
	}
		
	int Sprite::findDir(int seq_id, float radians) const {
		DASSERT(seq_id >= 0 && seq_id < size());
//		DASSERT(radians >= 0.0f && radians < constant::pi * 2.0f);

		//TODO: wtf???
		float2 vec = angleToVector(radians);
		radians = vectorToAngle(-vec);
		
		int dir_count = dirCount(seq_id);
		float dir = radians * float(dir_count) * (0.5f / constant::pi) + 0.5f;
		return (int(dir) + dir_count) % dir_count;
	}

	void Sprite::printInfo() const {
		size_t bytes = sizeof(Sprite) + sizeof(Frame) * m_frames.size(), img_bytes = 0;

		for(int n = 0; n < (int)m_sequences.size(); n++) {
			const Sequence &seq = m_sequences[n];
			bytes += sizeof(seq) + seq.name.size() + 1;
		}
		for(int i = 0; i < (int)m_palettes.size(); i++)
			bytes += sizeof(MultiPalette) + m_palettes[i].colors.size() * sizeof(Color);
		for(int n = 0; n < (int)m_images.size(); n++) {
			const MultiImage &image = m_images[n];
			bytes += sizeof(image);
			for(int i = 0; i < layer_count; i++)
				img_bytes += image.images[i].memorySize() - sizeof(CompressedTexture);
		}

		printf("Sprite %s memory:\nstuff: %d KB\nimages: %d KB\n",
				m_name.c_str(), (int)(bytes/1024), (int)(img_bytes/1024));
	}

	void Sprite::printSequencesInfo() const {
		for(int s = 0; s < (int)m_sequences.size(); s++) {
			const Sprite::Sequence &seq = m_sequences[s];
			printf("Sequence %d: %s (%d frames)\n", s, seq.name.c_str(), (int)seq.frame_count);
		}
	}

	void Sprite::printSequenceInfo(int seq_id) const {
		DASSERT(seq_id >= 0 && seq_id < (int)m_sequences.size());
		const Sequence &seq = m_sequences[seq_id];

		printf("Sequence %d (%s)\n", seq_id, seq.name.c_str());
		for(int n = 0; n < (int)seq.frame_count; n++)
			printf("frame #%d: %d\n", n, (int)m_frames[seq.first_frame + n].id);
		printf("\n");
	}

	ResourceMgr<Sprite> Sprite::mgr("data/sprites/", ".sprite");

}
