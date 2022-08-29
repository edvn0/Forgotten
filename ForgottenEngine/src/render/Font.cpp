#include "fg_pch.hpp"

#include "render/Font.hpp"

#include "render/MSDFData.hpp"
#include "utilities/FileSystem.hpp"

namespace ForgottenEngine {

	using namespace msdf_atlas;

	struct FontInput {
		const char* font_filename;
		GlyphIdentifierType glyph_type;
		const char* charset_filename;
		double font_scale;
		const char* font_name;
	};

	struct Configuration {
		ImageType image_type;
		msdf_atlas::ImageFormat image_format;
		YDirection y_direction;
		int width, height;
		double em_size;
		double px_range;
		double angle_threshold;
		double miter_limit;
		void (*edgeColoring)(msdfgen::Shape&, double, unsigned long long);
		bool expensive_colouring;
		unsigned long long colouring_seed;
		GeneratorAttributes generator_attributes;
	};

	constexpr auto DEFAULT_ANGLE_THRESHOLD = 3.0;
	constexpr auto DEFAULT_MITER_LIMIT = 1.0;
	constexpr auto LCG_MULTIPLIER = 6364136223846793005ull;
	constexpr auto LCG_INCREMENT = 1442695040888963407ull;
	constexpr auto THREADS = 8;

	namespace Utils {

		static std::filesystem::path get_cache_directory() { return Assets::slashed_string_to_filepath("resources/fonts/cache/font_atlases"); }

		static void create_cache_directory_if_needed()
		{
			std::filesystem::path cacheDirectory = get_cache_directory();
			if (!std::filesystem::exists(cacheDirectory))
				std::filesystem::create_directories(cacheDirectory);
		}
	} // namespace Utils

	struct AtlasHeader {
		uint32_t Type = 0;
		uint32_t Width, Height;
	};

	static bool try_read_font_atlas_from_cache(
		const std::string& font_name, float font_size, AtlasHeader& header, void*& pixels, Buffer& storage_buffer)
	{
		std::string filename = fmt::format("{0}-{1}.hfa", font_name, font_size);
		std::filesystem::path filepath = Utils::get_cache_directory() / filename;

		if (std::filesystem::exists(filepath)) {
			storage_buffer = FileSystem::read_bytes(filepath);
			header = *storage_buffer.as<AtlasHeader>();
			pixels = (uint8_t*)storage_buffer.data + sizeof(AtlasHeader);
			return true;
		}
		return false;
	}

	static void cache_font_atlas(const std::string& font_name, float font_size, AtlasHeader header, const void* pixels)
	{
		Utils::create_cache_directory_if_needed();

		std::string filename = fmt::format("{0}-{1}.hfa", font_name, font_size);
		std::filesystem::path filepath = Utils::get_cache_directory() / filename;

		std::ofstream stream(filepath, std::ios::binary | std::ios::trunc);
		if (!stream) {
			stream.close();
			CORE_ERROR("Failed to cache font atlas to {0}", filepath.string());
			return;
		}

		stream.write((char*)&header, sizeof(AtlasHeader));
		stream.write((char*)pixels, header.Width * header.Height * sizeof(float) * 4);
	}

	template <typename T, typename S, int N, GeneratorFunction<S, N> GEN_FN>
	static Reference<Texture2D> create_cache_atlas(const std::string& font_name, float font_size, const std::vector<GlyphGeometry>& glyphs,
		const FontGeometry& fontGeometry, const Configuration& config)
	{
		ImmediateAtlasGenerator<S, N, GEN_FN, BitmapAtlasStorage<T, N>> generator(config.width, config.height);
		generator.setAttributes(config.generator_attributes);
		generator.setThreadCount(THREADS);
		generator.generate(glyphs.data(), glyphs.size());

		msdfgen::BitmapConstRef<T, N> bitmap = (msdfgen::BitmapConstRef<T, N>)generator.atlasStorage();

		AtlasHeader header;
		header.Width = bitmap.width;
		header.Height = bitmap.height;
		cache_font_atlas(font_name, font_size, header, bitmap.pixels);

		TextureProperties props;
		props.GenerateMips = false;
		props.SamplerWrap = TextureWrap::Clamp;
		props.DebugName = "FontAtlas";
		Reference<Texture2D> texture = Texture2D::create(ImageFormat::RGBA32F, header.Width, header.Height, bitmap.pixels, props);
		return texture;
	}

	static Reference<Texture2D> create_cached_atlas(AtlasHeader header, const void* pixels)
	{
		TextureProperties props;
		props.GenerateMips = false;
		props.SamplerWrap = TextureWrap::Clamp;
		props.DebugName = "FontAtlas";
		Reference<Texture2D> texture = Texture2D::create(ImageFormat::RGBA32F, header.Width, header.Height, pixels, props);
		return texture;
	}

	Font::Font(const std::filesystem::path& filepath)
		: file_path(filepath)
		, msdf_data(new MSDFData())
	{
		int result = 0;
		FontInput fontInput = {};
		Configuration config = {};
		fontInput.glyph_type = GlyphIdentifierType::UNICODE_CODEPOINT;
		fontInput.font_scale = -1;
		config.image_type = ImageType::MSDF;
		config.image_format = msdf_atlas::ImageFormat::BINARY_FLOAT;
		config.y_direction = YDirection::BOTTOM_UP;
		config.edgeColoring = msdfgen::edgeColoringInkTrap;
		const char* image_formatName = nullptr;
		int fixedWidth = -1, fixedHeight = -1;
		config.generator_attributes.config.overlapSupport = true;
		config.generator_attributes.scanlinePass = true;
		double minEmSize = 0;
		double rangeValue = 2.0;
		TightAtlasPacker::DimensionsConstraint atlasSizeConstraint = TightAtlasPacker::DimensionsConstraint::MULTIPLE_OF_FOUR_SQUARE;
		config.angle_threshold = DEFAULT_ANGLE_THRESHOLD;
		config.miter_limit = DEFAULT_MITER_LIMIT;
		config.image_type = ImageType::MTSDF;

		std::string font_filepath = file_path.string();
		fontInput.font_filename = font_filepath.c_str();

		config.em_size = 40;

		// Load fonts
		bool anyCodepointsAvailable = false;
		class FontHolder {
			msdfgen::FreetypeHandle* ft;
			msdfgen::FontHandle* font;
			const char* font_filename;

		public:
			FontHolder()
				: ft(msdfgen::initializeFreetype())
				, font(nullptr)
				, font_filename(nullptr)
			{
			}
			~FontHolder()
			{
				if (ft) {
					if (font)
						msdfgen::destroyFont(font);
					msdfgen::deinitializeFreetype(ft);
				}
			}
			bool load(const char* font_filename)
			{
				if (ft && font_filename) {
					if (this->font_filename && !strcmp(this->font_filename, font_filename))
						return true;
					if (font)
						msdfgen::destroyFont(font);
					if ((font = msdfgen::loadFont(ft, font_filename))) {
						this->font_filename = font_filename;
						return true;
					}
					this->font_filename = nullptr;
				}
				return false;
			}
			operator msdfgen::FontHandle*() const { return font; }
		} font;

		if (!font.load(fontInput.font_filename))
			CORE_ASSERT_BOOL(false);
		if (fontInput.font_scale <= 0)
			fontInput.font_scale = 1;

		// Load character set
		fontInput.glyph_type = GlyphIdentifierType::UNICODE_CODEPOINT;
		Charset charset;

		// From ImGui
		static const uint32_t charset_ranges[] = {
			0x0020,
			0x00FF, // Basic Latin + Latin Supplement
			0x0400,
			0x052F, // Cyrillic + Cyrillic Supplement
			0x2DE0,
			0x2DFF, // Cyrillic Extended-A
			0xA640,
			0xA69F, // Cyrillic Extended-B
			0,
		};

		for (int range = 0; range < 8; range += 2) {
			for (int c = charset_ranges[range]; c <= charset_ranges[range + 1]; c++)
				charset.add(c);
		}

		// Load glyphs
		msdf_data->font_geometry = FontGeometry(&msdf_data->glyphs);
		int glyphsLoaded = -1;
		switch (fontInput.glyph_type) {
		case GlyphIdentifierType::GLYPH_INDEX:
			glyphsLoaded = msdf_data->font_geometry.loadGlyphset(font, fontInput.font_scale, charset);
			break;
		case GlyphIdentifierType::UNICODE_CODEPOINT:
			glyphsLoaded = msdf_data->font_geometry.loadCharset(font, fontInput.font_scale, charset);
			anyCodepointsAvailable |= glyphsLoaded > 0;
			break;
		}

		if (glyphsLoaded < 0)
			CORE_ASSERT_BOOL(false);
		CORE_TRACE("Loaded geometry of {0} out of {1} glyphs", glyphsLoaded, (int)charset.size());
		// List missing glyphs
		if (glyphsLoaded < (int)charset.size()) {
			CORE_WARN("Missing {0} {1}", (int)charset.size() - glyphsLoaded,
				fontInput.glyph_type == GlyphIdentifierType::UNICODE_CODEPOINT ? "codepoints" : "glyphs");
		}

		if (fontInput.font_name)
			msdf_data->font_geometry.setName(fontInput.font_name);

		// NOTE(Yan): we still need to "pack" the font to determine atlas metadata, though this could also be cached.
		//            The most intensive part is the actual atlas generation, which is what we do cache - it takes
		//            around 96% of total time spent in this Font constructor.

		// Determine final atlas dimensions, scale and range, pack glyphs
		double px_range = rangeValue;
		bool fixedDimensions = fixedWidth >= 0 && fixedHeight >= 0;
		bool fixedScale = config.em_size > 0;
		TightAtlasPacker atlasPacker;
		if (fixedDimensions)
			atlasPacker.setDimensions(fixedWidth, fixedHeight);
		else
			atlasPacker.setDimensionsConstraint(atlasSizeConstraint);
		atlasPacker.setPadding(config.image_type == ImageType::MSDF || config.image_type == ImageType::MTSDF ? 0 : -1);
		// TODO: In this case (if padding is -1), the border pixels of each glyph are black, but still computed. For
		// floating-point output, this may play a role.
		if (fixedScale)
			atlasPacker.setScale(config.em_size);
		else
			atlasPacker.setMinimumScale(minEmSize);
		atlasPacker.setPixelRange(px_range);
		atlasPacker.setMiterLimit(config.miter_limit);
		if (int remaining = atlasPacker.pack(msdf_data->glyphs.data(), msdf_data->glyphs.size())) {
			if (remaining < 0) {
				CORE_ASSERT_BOOL(false);
			} else {
				CORE_ERROR("Error: Could not fit {0} out of {1} glyphs into the atlas.", remaining, (int)msdf_data->glyphs.size());
				CORE_ASSERT_BOOL(false);
			}
		}
		atlasPacker.getDimensions(config.width, config.height);
		CORE_ASSERT(config.width > 0 && config.height > 0, "");
		config.em_size = atlasPacker.getScale();
		config.px_range = atlasPacker.getPixelRange();
		if (!fixedScale)
			CORE_TRACE("Glyph size: {0} pixels/EM", config.em_size);
		if (!fixedDimensions)
			CORE_TRACE("Atlas dimensions: {0} x {1}", config.width, config.height);

		// Edge coloring
		if (config.image_type == ImageType::MSDF || config.image_type == ImageType::MTSDF) {
			if (config.expensive_colouring) {
				Workload(
					[&glyphs = msdf_data->glyphs, &config](int i, int threadNo) -> bool {
						unsigned long long glyphSeed = (LCG_MULTIPLIER * (config.colouring_seed ^ i) + LCG_INCREMENT) * !!config.colouring_seed;
						glyphs[i].edgeColoring(config.edgeColoring, config.angle_threshold, glyphSeed);
						return true;
					},
					msdf_data->glyphs.size())
					.finish(THREADS);
			} else {
				unsigned long long glyphSeed = config.colouring_seed;
				for (GlyphGeometry& glyph : msdf_data->glyphs) {
					glyphSeed *= LCG_MULTIPLIER;
					glyph.edgeColoring(config.edgeColoring, config.angle_threshold, glyphSeed);
				}
			}
		}

		std::string font_name = filepath.filename().string();

		// Check cache here
		Buffer storage_buffer;
		AtlasHeader header;
		void* pixels;
		if (try_read_font_atlas_from_cache(font_name, (float)config.em_size, header, pixels, storage_buffer)) {
			texture_atlas = create_cached_atlas(header, pixels);
			storage_buffer.release();
		} else {
			bool floatingPointFormat = true;
			Reference<Texture2D> texture;
			switch (config.image_type) {
			case ImageType::MSDF:
				if (floatingPointFormat)
					texture = create_cache_atlas<float, float, 3, msdfGenerator>(
						font_name, (float)config.em_size, msdf_data->glyphs, msdf_data->font_geometry, config);
				else
					texture = create_cache_atlas<byte, float, 3, msdfGenerator>(
						font_name, (float)config.em_size, msdf_data->glyphs, msdf_data->font_geometry, config);
				break;
			case ImageType::MTSDF:
				if (floatingPointFormat)
					texture = create_cache_atlas<float, float, 4, mtsdfGenerator>(
						font_name, (float)config.em_size, msdf_data->glyphs, msdf_data->font_geometry, config);
				else
					texture = create_cache_atlas<byte, float, 4, mtsdfGenerator>(
						font_name, (float)config.em_size, msdf_data->glyphs, msdf_data->font_geometry, config);
				break;
			default:
				CORE_ASSERT_BOOL(false);
			}

			texture_atlas = texture;
		}
	}

	Font::~Font() { delete msdf_data; }

	Reference<Font> Font::default_font;

	void Font::init()
	{
		auto path = Assets::find_resources_by_path(Assets::slashed_string_to_filepath("fonts/Olive Days.ttf"));

		CORE_ASSERT(path, "Could not find font file under {}", (*path).string());

		default_font = Reference<Font>::create(*path);
	}

	void Font::shutdown() { default_font.reset(); }

	Reference<Font> Font::get_default_font() { return default_font; }

} // namespace ForgottenEngine
