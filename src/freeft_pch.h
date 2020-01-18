// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of FreeFT. See license.txt for details.

#pragma once

#include <algorithm>
#include <fwk/dynamic.h>
#include <fwk/enum.h>
#include <fwk/format.h>
#include <fwk/gfx/color.h>
#include <fwk/gfx_base.h>
#include <fwk/index_range.h>
#include <fwk/math/box.h>
#include <fwk/math/constants.h>
#include <fwk/math/ray.h>
#include <fwk/math/segment.h>
#include <fwk/math_base.h>
#include <fwk/pod_vector.h>
#include <fwk/sys/memory_stream.h>
#include <fwk/sys/xml.h>
#include <map>

#define DUMP FWK_DUMP