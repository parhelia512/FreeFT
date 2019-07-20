// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of FreeFT. See license.txt for details.

#include "sys/config.h"

Config::Config() : resolution(800, 600), window_pos(0, 0), fullscreen_on(false), profiler_on(false) {}

Config::Config(CXmlNode node) : Config() { load(node); }

Config::Config(const char *config_name) : Config() {
	XmlDocument doc;
	doc.load("data/config.xml");

	auto node = doc.child(config_name);
	if(!node)
		node = doc.child("default");
	if(node)
		load(node);
}

void Config::load(CXmlNode node) {
	DASSERT(node);
	resolution = node.attrib<int2>("res", int2());
	window_pos = node.attrib<int2>("window_pos", int2());
	fullscreen_on = node.attrib<bool>("fullscreen", false);
	profiler_on = node.attrib<bool>("profiler", false);
}
