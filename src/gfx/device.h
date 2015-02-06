/* Copyright (C) 2013-2014 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of FreeFT.
 */

#ifndef GFX_DEVICE_H
#define GFX_DEVICE_H

#include "base.h"
#include "gfx/texture.h"
#include "io/input.h"

namespace gfx
{

	namespace Key { using namespace io::Key; };

	// Device texture, no support for mipmaps,
	class DTexture: public Resource
	{
	public:
		DTexture();
		DTexture(DTexture&&);
		DTexture(const DTexture&) = delete;
		void operator=(const DTexture&) = delete;
		void operator=(DTexture&&);
		~DTexture();

		void load(Stream&);

		void bind() const;
		static void unbind();

		void resize(TextureFormat format, int width, int height);
		void clear();

		void set(const Texture&);
		void upload(const Texture &src, const int2 &target_pos = int2(0, 0));
		void upload(const void *pixels, const int2 &dimensions, const int2 &target_pos);
		void download(Texture &target) const;
		void blit(DTexture &target, const IRect &src_rect, const int2 &target_pos) const;

		int width() const { return m_width; }
		int height() const { return m_height; }
		const int2 dimensions() const { return int2(m_width, m_height); }
		const TextureFormat format() const { return m_format; }

		int id() const { return m_id; }
		bool isValid() const { return m_id > 0; }

		static ResourceMgr<DTexture> gui_mgr;
		static ResourceMgr<DTexture> mgr;

	private:
		int m_id;
		int m_width, m_height;
		TextureFormat m_format;
	};

	typedef Ptr<DTexture> PTexture;

	void initDevice();
	void freeDevice();

	double targetFrameTime();

	// Swaps frames and synchronizes frame rate
	void tick();

	void createWindow(int2 size, bool fullscreen);
	void destroyWindow();
	void printDeviceInfo();

	bool pollEvents();
	void swapBuffers();

	int2 getWindowSize();
	int2 getMaxWindowSize(bool is_fullscreen);
	void adjustWindowSize(int2 &size, bool is_fullscreen);

	void setWindowPos(const int2 &pos);
	void setWindowTitle(const char *title);

	void grabMouse(bool);
	void showCursor(bool);

	char getCharPressed();

	// if key is pressed, after small delay, generates keypresses
	// every period frames
	bool isKeyDownAuto(int key, int period = 1);
	bool isKeyPressed(int);
	bool isKeyDown(int);
	bool isKeyUp(int);

	bool isMouseKeyPressed(int);
	bool isMouseKeyDown(int);
	bool isMouseKeyUp(int);

	int2 getMousePos();
	int2 getMouseMove();

	int getMouseWheelPos();
	int getMouseWheelMove();
	
	// Generates events from keyboard & mouse input
	// Mouse move events are always first
	const vector<io::InputEvent> generateInputEvents();

	void lookAt(int2 pos);

	void drawQuad(int2 pos, int2 size, Color color = Color::white);
	inline void drawQuad(int x, int y, int w, int h, Color col = Color::white)
		{ drawQuad(int2(x, y), int2(w, h), col); }

	void drawQuad(int2 pos, int2 size, const float2 &uv0, const float2 &uv1, Color color = Color::white);

	void drawQuad(const FRect &rect, const FRect &uv_rect, Color colors[4]);
	void drawQuad(const FRect &rect, const FRect &uv_rect, Color color = Color::white);
	inline void drawQuad(const FRect &rect, Color color = Color::white) { drawQuad(rect, FRect(0, 0, 1, 1), color); }

	void drawBBox(const FBox &wbox, Color col = Color::white, bool is_filled = false);
	void drawBBox(const IBox &wbox, Color col = Color::white, bool is_filled = false);

	void drawRect(const IRect &box, Color col = Color::white);
	void drawLine(int3 wp1, int3 wp2, Color color = Color::white);
	void drawLine(int2 p1, int2 p2, Color color = Color::white);

	void clear(Color color);

	enum BlendingMode {
		bmDisabled,
		bmNormal,
	};

	void setBlendingMode(BlendingMode mode);
	void setScissorRect(const IRect &rect);
	const IRect getScissorRect();

	void setScissorTest(bool is_enabled);

}

#endif
