#pragma once

#undef INFINITE
#include "msdf-atlas-gen.h"

#include <vector>

namespace ForgottenEngine {

	struct MSDFData {
		msdf_atlas::FontGeometry font_geometry;
		std::vector<msdf_atlas::GlyphGeometry> glyphs;
	};

} // namespace ForgottenEngine
