#pragma once

#include "../global.h"
#include "../opengles2/opengl_stuff.h"
#include "../shaders/mono.h"
#include "../RWops.h"

#include <cmath>
#include <cstdint>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_BITMAP_H

#include <memory>
#include <deque>
#include <vector>
#include <map>
#include <array>
#include <bitset>

// I don't like enum classes, but here it's useful.
enum class FONT_RESULT
{
	SUCCESS = 0,
	NOT_FOUND = 1,
	ERROR = -1
};

enum
{
	FONT_STYLE_NORMAL = 0,
	FONT_STYLE_BOLD = 1,
	FONT_STYLE_ITALICS = 2,
	FONT_STYLE_OUTLINE = 4,
	FONT_STYLE_MASK =
		(FONT_STYLE_NORMAL | FONT_STYLE_BOLD | FONT_STYLE_ITALICS | FONT_STYLE_OUTLINE)
};

enum
{
	HEX_HALF_WIDTH = 8,
	HEX_FULL_WIDTH = 16,
	HEX_HEIGHT = 16,
	// the number of hex glyphs loaded in chunks
	HEX_CHUNK_GLYPHS = 16 * 16
};

#define TTF_SetFTError(msg, error) internal_TTF_SetFTError(msg, error, __func__, __FILE__, __LINE__)
void internal_TTF_SetFTError(
	const char* msg, FT_Error error, const char* function, const char* file, int line);

struct FT_Done_Glyph_wrapper
{
	void operator()(FT_Glyph del)
	{
		FT_Done_Glyph(del);
	}
};
typedef std::unique_ptr<FT_GlyphRec, FT_Done_Glyph_wrapper> unique_ft_glyph;

enum class FONT_ENTRY : uint16_t
{
	// undefined is used internally to define initialization.
	UNDEFINED,
	// normal means this is an vector image
	NORMAL,
	// you should scale using the metrics in face->glyph
	// this should only be used for bitmaps in TTF
	BITMAP,
	// scale this assuming it's 16 pixels high.
	HEXFONT,
	// there is no texture, use advance.
	SPACE
};

// glyph stored in the atlas
struct font_glyph_entry
{
	font_glyph_entry() = default;
	// emplace_back annoyance.
	font_glyph_entry(
		FONT_ENTRY type_,
		uint16_t rect_x_,
		uint16_t rect_y_,
		uint16_t rect_w_,
		uint16_t rect_h_,
		int16_t advance_,
		int16_t xmin_,
		int16_t ymin_)
	: type(type_)
	, rect_x(rect_x_)
	, rect_y(rect_y_)
	, rect_w(rect_w_)
	, rect_h(rect_h_)
	, advance(advance_)
	, xmin(xmin_)
	, ymin(ymin_)
	{
	}

	FONT_ENTRY type;

	// location on atlas
	uint16_t rect_x;
	uint16_t rect_y;
	uint16_t rect_w;
	uint16_t rect_h;

	// metrics
	int16_t advance;
	int16_t xmin;
	int16_t ymin;
};

struct font_atlas
{
	uint32_t atlas_size = 0;

	// the grain size of pixels each span can hold.
	// needs to be a power of 2
	uint32_t span_granularity = 8;

	// so I don't need to loop through span_buckets to get the size.
	uint32_t spans_allocated = 0;

	struct span_bucket
	{
		// emplace_back annoyance.
		span_bucket(uint32_t start, uint32_t count, uint32_t depth)
		: span_start(start)
		, span_count(count)
		, bucket_depth(depth)
		{
		}
		// the chunk offset (in spans)
		uint32_t span_start;
		// the number of spans of width (in spans)
		uint32_t span_count;
		// the ammount of the chunk that is allocated (in pixels)
		uint32_t bucket_depth;
	};

	// a buckets holds the number of spans (of span_granularity)
	// TODO: maybe make the buckets into a 2D grid
	// bad fragmentation (and allocation errors) will occur
	// once all the buckets have been allocated
	// and you mix glyphs of different sizes.
	// it does mean that the largest sized glyph must be smaller than the grid.
	std::deque<span_bucket> span_buckets;

	bool find_atlas_slot(uint32_t w_in, uint32_t h_in, uint32_t* x_out, uint32_t* y_out);
};

// stores the entire Basic Multilingual Plane of unicode
// It's possible to store more (emojis), but not done here.
// this is also embedded into the font_manager as a global fallback for all fonts.
struct hex_font_data
{
	// this is a hex font glyph.
	struct hex_glyph_entry
	{
		// does the glyph exist?
		uint8_t hex_found : 1;
		// is the glyph full or half width?
		uint8_t hex_full : 1;
		// is hex_glyph initialized?
		uint8_t hex_init : 1;
		// I don't like unions, but this is very big, and any size shrink can help.
		union
		{
			// the pixel data of the glyphs.
			// it is oversized to fit both half and full width
			uint8_t hex_data[HEX_FULL_WIDTH * HEX_HEIGHT / 8];
			// if hex_init is true, hex_glyph will contain the location in the atlas.
			struct
			{
				// this is missing bold and italics.
				// even though it's possible to bold and skew
				// it isn't worth it because of readablity at 16px.
				// the outline does help with readablity though.
				font_glyph_entry normal;
				font_glyph_entry outline;
			} hex_glyph;
			static_assert(std::is_pod<decltype(hex_glyph)>::value);
		} u;
	};

	// first parse the whole hex file
	// and copy the offsets for the beginning of each chunk.
	// chunk size is HEX_CHUNK_COUNT
	struct hex_block_chunk
	{
		RW_ssize_t offset;
		std::unique_ptr<hex_glyph_entry[]> glyphs;
	};

	std::vector<hex_block_chunk> hex_block_chunks;

	Unique_RWops hex_font_file;

	bool init(Unique_RWops file);
	bool destroy();

	FONT_RESULT load_hex_glyph(
		font_atlas* atlas, char32_t codepoint, bool outline, font_glyph_entry* glyph);

	bool load_hex_block(hex_block_chunk* chunk);
};

struct font_manager_state
{
	FT_Library FTLibrary = NULL;

	GLuint gl_atlas_tex_id = 0;

    // to simplify the code a bit, I am considering just combining 
    // font_atlas, hex_font_data, and font_manager_state together
	font_atlas atlas;

	// a fallback that supports pretty much all unicode if a font_style doesn't support it.
	hex_font_data hex_font;

    // for drawing primitives, this is a single white pixel.
    float white_uv_x = -1;
	float white_uv_w = -1;
	float white_uv_y = -1;
	float white_uv_h = -1; 

	bool create();
	bool destroy();
};

// todo: post processing
#if 0
	//this is a hack that removes the gamma correction that freetype implements
	//only matters if you are using GL_SRGB_ALPHA8 + glEnable(GL_FRAMEBUFFER_SRGB);
	//you will clearly see the difference if you render red text on green.
	//note the texture that you put the glyph onto must be GL_SRGB_ALPHA8
	bool undo_gamma_correction = false;

	//draw the glyph twice, but the second time it is shifted to the right by 1 pixel
	//good for tiny fonts.
	bool mono_bold_hack = false;
#endif

struct font_ttf_face_settings
{
	float point_size = 0;

	float bold_x = 0, bold_y = 0;

	float outline_size = 0;

	// outline_only will only make an outline around the glyph,
	// this will also only set the outline bitmap, not the FT_GlyphSlot.
	// I don't really like the way it looks, and something I want but
	// can't really implement easily is non-baked outlines,
	// where you draw the outline in a pass, because outlines can be too big.
	// And this depends on outline which is bad code.
	// bool outline_only = false;

	float italics_skew = 0.207f; // cos(((90.0-x)/360)*2*MY_PI)

	// sometimes a bitmap font has outlines, but the outline are worse.
	// will try to find the best bitmap size to the point_size.
	bool force_bitmap = false;

	// supported modes are:
	// FT_RENDER_MODE_LIGHT
	// FT_RENDER_MODE_MONO
	FT_Render_Mode render_mode = FT_RENDER_MODE_NORMAL;

	// supported options are:
	// FT_LOAD_TARGET_MONO
	// FT_LOAD_TARGET_LIGHT
	// FT_LOAD_NO_AUTOHINT
	// FT_LOAD_FORCE_AUTOHINT
	// FT_LOAD_COLOR
	FT_Int32 load_flags = FT_LOAD_DEFAULT;
};

// yea I know freetype supports more than ttf.
struct font_ttf_rasterizer
{
	FT_Library FTLibrary = NULL;

	FT_Face face = NULL;

	// FT_Stream, make sure to zero out
	FT_StreamRec_ stream;

	Unique_RWops font_file;

	// normal glyphs live inside the global glyph slot,
	// but to make outlines, you need to allocate it separately.
	// FT_Glyph outline_glyph = NULL;

	const font_ttf_face_settings* face_settings = NULL;

	bool create(FT_Library ftstate, Unique_RWops file);
	bool destroy();
    ~font_ttf_rasterizer();

	// for every style you must set this before loading glyphs.
	// the lifetime of the settings must live as long as you need glyphs.
	bool set_face_settings(const font_ttf_face_settings* face_settings);

	// get the relevant metrics global to the face
	// note that these values are not scaled for bitmap fonts,
	// so if you are scaling, check FT_IS_SCALABLE(this->face)
	// and apply the same scale factor you use for rendering.
	void get_face_metrics_px(int* ascent, int* height);

	// face->glyph->format == FT_GLYPH_FORMAT_OUTLINE
	// if no error, you must call FT_Done_Glyph on glyph_out,
	// and glyph_out needs to be casted to FT_BitmapGlyph.
	bool render_glyph(FT_Glyph* glyph_out, unsigned style_flags = FONT_STYLE_NORMAL);
};

// cached in the atlas.
struct font_bitmap_cache
{
	enum
	{
		// to make loading glyphs fast I use random access indexes
		// but if you load a unicode emoji it would load like 40mb
		// if the memory was in one continuous array, so I cut it into blocks.
		FONT_CACHE_CHUNK_GLYPHS = 16 * 16
	};

	struct font_cache_block
	{
        // quick bit check if a glyph is known to not exist.
        // I really like std::bitset, but I probably need to replace this
        // with a C style bitset in a unique_ptr or a std::vector<bool>
        // because I want to store FONT_CACHE_CHUNK_GLYPHS in a cvar.
		std::bitset<FONT_CACHE_CHUNK_GLYPHS> bad_indexes;

		// FONT_ENTRY::UNDEFINED is used to check if initialized.
		std::unique_ptr<font_glyph_entry[]> glyphs[FONT_STYLE_MASK + 1];
	};

	std::vector<font_cache_block> font_cache_blocks;

	// RAII_FT_Bitmap convert_bitmap;
	FT_Bitmap convert_bitmap;

	font_manager_state* font_manager = NULL;
	font_ttf_rasterizer* current_rasterizer = NULL;
	int current_style = FONT_STYLE_NORMAL;

	void init(font_manager_state* font_manager_, font_ttf_rasterizer* rasterizer);
    bool destroy();
	~font_bitmap_cache();

	void set_style(int style)
	{
		current_style = style;
	}

	// you must set opengl's GL_UNPACK_ALIGNMENT to 1 for this to work.
	FONT_RESULT get_glyph(char32_t codepoint, font_glyph_entry* glyph_out);

	// internal use only
	// returns NULL if failed to load.
	// index is the index from FT_Get_Char_Index
	// ftglyph holds the lifetime of bitmap returned,
	// but sometimes the bitmap is stored in convert_bitmap
	// if the glyph was not an outline or FT_PIXEL_MODE_GRAY.
	FT_Bitmap* render_tf_glyph(FT_UInt glyph_index, unique_ft_glyph& ftglyph);
};

// the anchor for which side to align to.
// TOP_LEFT lets you draw at 0,0)
// TOP_RIGHT lets you draw at X,0
// this does not change the direction newlines move
// (if the newline is supported)
// TOP_LEFT is the default anchor.
// CENTER positions will not moveFONT_CACHE_CHUNK_GLYPHSend or add a new line,
// A workaround for CENTER is to read the BBOX,
// and change the anchor & cursor based on it.
// EX: TOP_LEFT 0,0 = draw text topleft of screen
//  append will add to the right of the text
// EX: BOTTOM_RIGHT "screen_w", "screen_h" 
//  = draw text bottom right of screen,
//  append will add to the left of the text
// EX: CENTER_LEFT: 0, "screen_h / 2"
//  = draw text at the left of the screen
//  append will add text to the right
//  probably printing a warning?

enum class TEXT_ANCHOR : uint8_t
{
    TOP_LEFT,
    TOP_RIGHT,
    BOTTOM_LEFT,
    BOTTOM_RIGHT,
    CENTER_PERFECT,
    CENTER_TOP,
    CENTER_BOTTOM,
    CENTER_LEFT,
    CENTER_RIGHT
};

enum TEXT_FLAGS : uint8_t
{
    NONE,
    NEWLINE
    //FORMATTING
};

struct font_sprite_batcher
{
	enum
	{
		FONT_BATCH_VERTS = 6
	};
	enum class FONT_TYPES : uint8_t
	{
		UNDEFINED = 0,
		TTF,
		HEX
	};

	std::vector<gl_mono_vertex> font_vertex_buffer;

	size_t flush_cursor = 0;
	size_t newline_cursor = 0;

    float anchor_x = 0;
    float anchor_y = 0;

	float draw_cur_x = 0;
    float draw_cur_y = 0;

	std::array<uint8_t, 4> cur_color{255, 255, 255, 255};

	TEXT_ANCHOR current_anchor = TEXT_ANCHOR::TOP_LEFT;
	TEXT_FLAGS current_flags = TEXT_FLAGS::NONE;
	FONT_TYPES font_type = FONT_TYPES::UNDEFINED;
	union
	{
		struct
		{
			font_bitmap_cache* font;
		} ttf;
		struct
		{
			font_manager_state* font_manager;
			float height;
			bool outline;
		} hex;
	} font_u;

	void SetAnchor(TEXT_ANCHOR anchor)
	{
		current_anchor = anchor;
	}

	void SetFlag(TEXT_FLAGS flag)
	{
		current_flags = flag;
	}

	size_t vertex_count() const
	{
		return font_vertex_buffer.size();
	}

    //you can use this for knowing where the string left off.
    float draw_x_offset() const
    {
        return draw_cur_x - anchor_x;
    }
    float draw_y_offset() const
    {
        return draw_cur_y - anchor_y;
    }
    float draw_x_pos() const
    {
        return draw_cur_x;
    }
    float draw_y_pos() const
    {
        return draw_cur_y;
    }

    void insert_padding(float x)
    {
        draw_cur_x += x;
    }

	// set the current color
	// the default is black.
	void set_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
	{
		cur_color = {r, g, b, a};
	}
    void set_color(std::array<uint8_t, 4> color)
	{
		cur_color = color;
	}

	// clear previous batch, define the buffer size
	// the buffer size is the max number of sprites,
	// and also the snprintf buffer size
	// (this means that non ascii characters will
	// not use the entire buffer size if truncated!
	// workaround: don't use the formatting API)
	// note: the buffer must be bound.
	// and you need to bind the atlas texture
	// and set glPixelStorei(GL_UNPACK_ALIGNMENT, 1)
	// while drawing the text.
	void begin();

	// this will upload the buffer to opengl
	// note: the buffer must be bound.
	void end(GLenum type = GL_DYNAMIC_DRAW);

	void SetXY(float x, float y)
	{
        // fix the y axis alignment of any previous batch
        internal_flush();
        anchor_x = std::floor(x);
        anchor_y = std::floor(y);
		draw_cur_x = anchor_x;
		draw_cur_y = anchor_y;
	}

    // If you use an anchor that offsets the y axis
    // (BOTTOM_LEFT+RIGHT, CENTER_PERFECT+LEFT+RIGHT+BOTTOM)
    // you need to call flush at the end to correct the offset.
    // this is done during end() and SetXY()
    void internal_flush();

    // You should use the newline flag if you need it,
    // but you want a newline without '\n', use this.
    void Newline();

	float GetLineSkip() const;

	// This will render the glyph which means you must bind the atlas and set GL_UNPACK_ALIGNMENT
	float GetAdvance(char32_t codepoint) const;

	void SetFont(font_bitmap_cache* font)
	{
		// ASSERT(font != NULL);
		font_type = FONT_TYPES::TTF;
		font_u.ttf.font = font;
	}

	// hexfont is more convenient for placeholders.
	void SetHex(font_manager_state* font_manager, float height, bool outline)
	{
		// ASSERT(font_manager != NULL);
		font_type = FONT_TYPES::HEX;
		font_u.hex.font_manager = font_manager;
		font_u.hex.height = height;
		font_u.hex.outline = outline;
	}

	// You must call this between begin() and end()
	bool draw_format(const char* fmt, ...) __attribute__((format(printf, 2, 3)));

	// you can use null terminated strings if size = 0, but strlen will be used.
	bool draw_text(const char* text, size_t size = 0);


	FONT_RESULT load_glyph_verts(char32_t codepoint);

	// using a single pixel in the atlas, you can draw primitives,
	// I use this for drawing the text selection, and potentially strikeout, underline.
	void draw_rect(float minx, float maxx, float miny, float maxy);

    // index references the vertex
    // you can use vertex_count() to reference the index.
	void move_rect(size_t index, float minx, float maxx, float miny, float maxy);
};

