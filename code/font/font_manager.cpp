#include "../global_pch.h"
#include "../global.h"

#include "font_manager.h"

#include "utf8_stuff.h"

// for reading files, since I like the stream API.
#include "../BS_Archive/BS_stream.h"
#include "../cvar.h"
#include "../app.h" //for cv_ui_scale for the font painter

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>

#include <ft2build.h>
#include <type_traits>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_STROKER_H
#include FT_BITMAP_H
#include FT_OUTLINE_H
// I can't really support color because my atlas is black and white for GPU memory reasons.
//#include FT_COLOR_H

// TODO(dootsie): I need to make error messages more informational, like print the codepoint into
// utf8.
// TODO: I could add in a lineskip scale (also font_scale advance should really be floor/ceil'd...)

#define FT_CEIL(X) ((((X) + 63) & -64) / 64)
#define FT_FLOOR(X) (((X) & -64) / 64)

static REGISTER_CVAR_INT(
	cv_font_atlas_size, 16384, "the texture size, must be a power of 2", CVAR_T::STARTUP);

// this is an annoying warning because the glyph will still use the unifont fallback,
// and if the unifont also doesn't have the font, it turns into an error.
static REGISTER_CVAR_INT(
	cv_font_fallback_warning,
	0,
	"0 = off, 1 = on, show a warning for using the unifont fallback",
	CVAR_T::RUNTIME);

// this is referenced from SDL_TTF
void internal_TTF_SetFTError(
	const char* msg, FT_Error error, const char* function, const char* file, int line)
{
#undef __FTERRORS_H__
#define FT_ERRORDEF(e, v, s) {e, s},
	static const struct
	{
		int err_code;
		const char* err_msg;
	} ft_errors[] = {
#include <freetype/fterrors.h>
	};
	unsigned int i;
	const char* err_msg;

	err_msg = NULL;
	for(i = 0; i < ((sizeof ft_errors) / (sizeof ft_errors[0])); ++i)
	{
		if(error == ft_errors[i].err_code)
		{
			err_msg = ft_errors[i].err_msg;
			break;
		}
	}
	if(err_msg == NULL)
	{
		err_msg = "unknown FreeType error";
	}

	serrf(
		"%s: %s\n"
		"File: %s\n"
		"Line: %d\n"
		"Func: %s\n",
		msg,
		err_msg,
		file,
		line,
		function);
}

static FT_ULong ttf_RWread(FT_Stream stream, FT_ULong offset, unsigned char* buffer, FT_ULong count)
{
	RWops* src = static_cast<RWops*>(stream->descriptor.pointer);

	ASSERT(src != NULL);

	// NOLINTNEXTLINE(bugprone-narrowing-conversions)
	if(src->seek(offset, SEEK_SET) != 0)
	{
		return 0;
	}

	if(src->tell() == -1)
	{
		return 0;
	}

	return src->read(buffer, 1, count);
}

#if 0
RAII_FT_Bitmap::RAII_FT_Bitmap(FT_Library ftlibrary_)
: ftlibrary(ftlibrary_)
{
	FT_Bitmap_Init(&bitmap);
}
FT_Error RAII_FT_Bitmap::close()
{
	FT_Error error = FT_Bitmap_Done(ftlibrary, &bitmap);
	ftlibrary = NULL;
	return error;
}
RAII_FT_Bitmap::~RAII_FT_Bitmap()
{
	// note that this does not print to serr
	// but
	if(ftlibrary != NULL)
	{
		FT_Bitmap_Done(ftlibrary, &bitmap);
		ftlibrary = NULL;
	}
}
#endif

static void convert_glyph_format(
	font_style_interface* font, font_glyph_entry* in, font_style_result* out, float font_scale)
{
	float atlas_size = static_cast<float>(font->get_font_atlas()->atlas_size);
	out->atlas_xmin = static_cast<float>(in->rect_x) / atlas_size;
	out->atlas_ymin = static_cast<float>(in->rect_y) / atlas_size;
	out->atlas_xmax = static_cast<float>(in->rect_x + in->rect_w) / atlas_size;
	out->atlas_ymax = static_cast<float>(in->rect_y + in->rect_h) / atlas_size;

	out->glyph_xmin = static_cast<float>(in->xmin) * font_scale;
	out->glyph_ymin = font->get_ascent(font_scale) - (static_cast<float>(in->ymin) * font_scale);
	out->glyph_xmax = out->glyph_xmin + static_cast<float>(in->rect_w) * font_scale;
	out->glyph_ymax = out->glyph_ymin + (static_cast<float>(in->rect_h) * font_scale);
	out->advance = static_cast<float>(in->advance) * font_scale;
}

bool font_manager_state::create()
{
	FT_Error error;
	if((error = FT_Init_FreeType(&FTLibrary)) != 0)
	{
		TTF_SetFTError("font_manager", error);
		return false;
	}

	FT_Int major_ver = 0;
	FT_Int minor_ver = 0;
	FT_Int patch_ver = 0;
	FT_Library_Version(FTLibrary, &major_ver, &minor_ver, &patch_ver);
	if(FREETYPE_MAJOR != major_ver || FREETYPE_MINOR != minor_ver || FREETYPE_PATCH != patch_ver)
	{
		slogf(
			"mismatching freetype version, got: %d.%d.%d expected: %d.%d.%d\n",
			major_ver,
			minor_ver,
			patch_ver,
			FREETYPE_MAJOR,
			FREETYPE_MINOR,
			FREETYPE_PATCH);
	}

	int gl_max_texture_size = 0;
	ctx.glGetIntegerv(GL_MAX_TEXTURE_SIZE, &gl_max_texture_size);

	atlas.atlas_size = std::min(cv_font_atlas_size.data, gl_max_texture_size);

	// create the texture for the atlas
	ctx.glGenTextures(1, &gl_atlas_tex_id);
	if(gl_atlas_tex_id == 0)
	{
		serrf("%s error: glGenTextures failed\n", __func__);
		return false;
	}

	ctx.glActiveTexture(GL_TEXTURE0);
	ctx.glBindTexture(GL_TEXTURE_2D, gl_atlas_tex_id);
	ctx.glTexImage2D(
		GL_TEXTURE_2D,
		0,
		GL_R8,
		atlas.atlas_size, // NOLINT(bugprone-narrowing-conversions)
		atlas.atlas_size, // NOLINT(bugprone-narrowing-conversions)
		0,
		GL_RED,
		GL_UNSIGNED_BYTE,
		NULL);

	// Set texture parameters
	ctx.glTexParameteri(
		GL_TEXTURE_2D,
		GL_TEXTURE_MAG_FILTER,
		(cv_font_linear_filtering.data == 1 ? GL_LINEAR : GL_NEAREST));
	ctx.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	ctx.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	ctx.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	ctx.glBindTexture(GL_TEXTURE_2D, 0);

// webgl will always clear the texture for security reasons.
#ifndef __EMSCRIPTEN__

#if 0
	static GLuint query = 0;
	TIMER_U t1 = timer_now();
	if(cv_has_EXT_disjoint_timer_query.data == 1)
	{
		// static GLuint query = 0;
		if(query == 0)
		{
			ctx.glGenQueriesEXT(1, &query);
		}
		ctx.glBeginQueryEXT(GL_TIME_ELAPSED_EXT, query);
	}
#endif

	// because of filtering, I need to pad textures in the atlas,
	// but unwritten areas will have garbage, so clear the texture to be zero's.

	unsigned int fbo;
	ctx.glGenFramebuffers(1, &fbo);
	if(fbo == 0)
	{
		serrf("%s error: glGenFramebuffers failed\n", __func__);
		return false;
	}
	ctx.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
	ctx.glFramebufferTexture2D(
		GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gl_atlas_tex_id, 0);
	GLenum color_attachment = GL_COLOR_ATTACHMENT0;
	ctx.glDrawBuffers(1, &color_attachment);
	// note this is a GL_RED texture, and only the RED value matters.
	GLfloat clearColor[4] = {0, 0, 0, 0};
	ctx.glClearBufferfv(GL_COLOR, 0, clearColor);
	ctx.glDrawBuffers(0, NULL);
	ctx.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	ctx.glDeleteFramebuffers(1, &fbo);
	fbo = 0;

#if 0
    // yes this takes a long time...
	if(cv_has_EXT_disjoint_timer_query.data == 1)
	{
		TIMER_U t2 = timer_now();
		ctx.glEndQueryEXT(GL_TIME_ELAPSED_EXT);

		GLuint64 elapsed_time;
		ctx.glGetQueryObjectui64vEXT(query, GL_QUERY_RESULT, &elapsed_time);
		slogf(
			"atlas fbo time: %f, cpu: %f\n",
			static_cast<double>(elapsed_time) / 1000000.0,
			timer_delta_ms(t1, t2));
	}
#endif
#endif

	// upload 4 pixels for padding
	// technically I could use one pixel but see cool_fade

	uint32_t offset = 0;
	if(cv_font_linear_filtering.data == 1)
	{
		offset = 1;
	}

	uint32_t x_out;
	uint32_t y_out;
	// 2x2 for the colors of the corners, and +1 because of cv_font_linear_filtering
	if(!atlas.find_atlas_slot(2 + offset, 2 + offset, &x_out, &y_out))
	{
		return false;
	}

	float atlas_size = static_cast<float>(atlas.atlas_size);
	atlas.white_uv[0] = (static_cast<float>(x_out) + 0.5f) / atlas_size;
	atlas.white_uv[2] = (static_cast<float>(x_out) + 1.5f) / atlas_size;
	atlas.white_uv[1] = (static_cast<float>(y_out) + 0.5f) / atlas_size;
	atlas.white_uv[3] = (static_cast<float>(y_out) + 1.5f) / atlas_size;

	uint8_t cool_fade = 255;
	if(cv_font_linear_filtering.data == 1)
	{
		// I like the look, this affects all the boxes to have some alpha.
		cool_fade = 170;
	}
	uint8_t pixel[4] = {255, 255, cool_fade, cool_fade};
	ctx.glBindTexture(GL_TEXTURE_2D, gl_atlas_tex_id);
	ctx.glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	// NOLINTNEXTLINE(bugprone-narrowing-conversions)
	ctx.glTexSubImage2D(GL_TEXTURE_2D, 0, x_out, y_out, 2, 2, GL_RED, GL_UNSIGNED_BYTE, pixel);
	ctx.glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	ctx.glBindTexture(GL_TEXTURE_2D, 0);

	return GL_CHECK(__func__) == GL_NO_ERROR;
}

bool font_manager_state::destroy()
{
	bool success = true;

	success = hex_font.destroy() && success;

	if(FTLibrary != NULL)
	{
		FT_Error error;
		if((error = FT_Done_FreeType(FTLibrary)) != 0)
		{
			TTF_SetFTError("font_manager", error);
			success = false;
		}
		FTLibrary = NULL;
	}

	SAFE_GL_DELETE_TEXTURE(gl_atlas_tex_id);

	return GL_CHECK(__func__) == GL_NO_ERROR && success;
}

bool font_ttf_rasterizer::create(FT_Library ftstate, Unique_RWops file)
{
	ASSERT(ftstate != NULL);
	ASSERT(file);

	FTLibrary = ftstate;
	font_file = std::move(file);

	FT_Error error;
	int index = 0;

	memset(&stream, 0, sizeof(stream));
	stream.read = ttf_RWread;
	stream.descriptor.pointer = font_file.get();
	stream.pos = 0;
	stream.size = font_file->size();

	FT_Open_Args args;
	memset(&args, 0, sizeof(args));
	args.flags = FT_OPEN_STREAM;
	args.stream = &stream;

	if((error = FT_Open_Face(FTLibrary, &args, index, &face)) != 0)
	{
		TTF_SetFTError(font_file->name(), error);
		return false;
	}
	// this is copied from SDL_TTF, mainly because this just makes .FON files work
	// Set charmap for loaded font
	FT_CharMap found = 0;
	if(found == 0)
	{
		for(int i = 0; i < face->num_charmaps; i++)
		{
			FT_CharMap charmap = face->charmaps[i];
			if(charmap->platform_id == 3 && charmap->encoding_id == 10)
			{ // UCS-4 Unicode
				found = charmap;
				break;
			}
		}
	}
	if(found == 0)
	{
		for(int i = 0; i < face->num_charmaps; i++)
		{
			FT_CharMap charmap = face->charmaps[i];
			if((charmap->platform_id == 3 && charmap->encoding_id == 1) // Windows Unicode
			   || (charmap->platform_id == 3 && charmap->encoding_id == 0) // Windows Symbol
			   || (charmap->platform_id == 2 && charmap->encoding_id == 1) // ISO Unicode
			   || (charmap->platform_id == 0))
			{ // Apple Unicode
				found = charmap;
				break;
			}
		}
	}
	if(found != 0)
	{
		if((error = FT_Set_Charmap(face, found)) != 0)
		{
			TTF_SetFTError(file->name(), error);
			return false;
		}
	}
	return true;
}

bool font_ttf_rasterizer::destroy()
{
	bool success = true;
	FT_Error err;
	if(face != NULL)
	{
		if((err = FT_Done_Face(face)) != 0)
		{
			TTF_SetFTError(font_file->name(), err);
			success = false;
		}
		face = NULL;
	}

	if(font_file)
	{
		if(!font_file->close())
		{
			success = false;
		}
		font_file.reset();
	}

	/*
		if(outline_glyph != NULL)
		{
			FT_Done_Glyph(outline_glyph);
		}*/
	return success;
}
font_ttf_rasterizer::~font_ttf_rasterizer()
{
	if(face != NULL)
	{
		FT_Done_Face(face);
		face = NULL;
	}
	face_settings = NULL;
}

bool font_ttf_rasterizer::set_face_settings(const font_ttf_face_settings* settings)
{
	ASSERT(face != NULL);
	ASSERT(settings != NULL);
	face_settings = NULL;

	FT_Error error;

	if(face->num_fixed_sizes != 0 && (!FT_IS_SCALABLE(face) || settings->force_bitmap))
	{
		// I stole this from some github code that showed an example of how to use emojis with
		// freetype2
		// note that height isn't an accurate representation of point size, but close enough.
		int best_match = 0;
		// NOLINTNEXTLINE(bugprone-narrowing-conversions)
		int diff = std::abs(settings->point_size - face->available_sizes[0].height);
		for(int i = 0; i < face->num_fixed_sizes; ++i)
		{
			int ndiff =
				std::abs(static_cast<int>(settings->point_size) - face->available_sizes[i].height);
			if(ndiff < diff)
			{
				best_match = i;
				diff = ndiff;
			}
		}

		if((error = FT_Select_Size(face, best_match)) != 0)
		{
			TTF_SetFTError(font_file->name(), error);
			return false;
		}
	}
	else
	{
		ASSERT(FT_IS_SCALABLE(face));
		// 72 means pixels per inch, microsoft fonts prefer 96 but you can scale 12px to 16px
		if((error = FT_Set_Char_Size(
				face, 0, static_cast<FT_F26Dot6>(settings->point_size * 64), 72, 72)) != 0)
		{
			TTF_SetFTError(font_file->name(), error);
			return false;
		}
	}

	face_settings = settings;

	return true;
}

bool font_ttf_rasterizer::render_glyph(FT_Glyph* glyph_out, unsigned style_flags)
{
	ASSERT(face != NULL);
	ASSERT(face_settings != NULL);
	ASSERT(glyph_out != NULL);

	FT_Error error;

	bool use_bold = (style_flags & FONT_STYLE_BOLD) != 0;
	bool use_italics = (style_flags & FONT_STYLE_ITALICS) != 0;
	bool use_outline = (style_flags & FONT_STYLE_OUTLINE) != 0;

	// You must call FT_Done_Glyph on this.
	if((error = FT_Get_Glyph(face->glyph, glyph_out)) != 0)
	{
		TTF_SetFTError(font_file->name(), error);
		return false;
	}

	ASSERT((*glyph_out)->format == FT_GLYPH_FORMAT_OUTLINE);

	FT_OutlineGlyph tmp_outline = reinterpret_cast<FT_OutlineGlyph>(*glyph_out);

	if(use_bold)
	{
		if((error = FT_Outline_EmboldenXY(
				&tmp_outline->outline,
				static_cast<FT_Pos>(face_settings->bold_x * 64),
				static_cast<FT_Pos>(face_settings->bold_y * 64))) != 0)
		{
			TTF_SetFTError(font_file->name(), error);
			return false;
		}
	}

	if(use_italics)
	{
		// set italics
		FT_Matrix italics_matrix{
			1 << 16, // xx
			static_cast<FT_Fixed>(face_settings->italics_skew * (1 << 16)), // xy
			0, // yx
			1 << 16 // yy
		};
		FT_Outline_Transform(&tmp_outline->outline, &italics_matrix);
	}

	if(use_outline)
	{
		// you must call FT_Stroker_Done on this
		// performance: would it be faster if I didn't allocate a new stroker every time?
		FT_Stroker stroker;
		if((error = FT_Stroker_New(FTLibrary, &stroker)) != 0)
		{
			TTF_SetFTError(font_file->name(), error);
			return false;
		}

		FT_Stroker_Set(
			stroker,
			static_cast<FT_Fixed>(face_settings->outline_size * 64),
			FT_STROKER_LINECAP_ROUND,
			FT_STROKER_LINEJOIN_ROUND,
			0);

#if 0
		if(face_settings->outline_only)
		{
			// this will make just the line around the glyph.
			if((error = FT_Glyph_Stroke(&outline_glyph, stroker, 1)) != 0)
			{
				TTF_SetFTError(font_file->name(), error);
			}
		}
		else
#endif
		{
			// this adds the line and fills in a glyph,
			// and replaces the glyph with the stroke.
			if((error = FT_Glyph_StrokeBorder(glyph_out, stroker, 0, 1)) != 0)
			{
				TTF_SetFTError(font_file->name(), error);
			}
		}
		FT_Stroker_Done(stroker);
		if(error != 0)
		{
			return false;
		}
	}

	// Render the glyph
	if((error = FT_Glyph_To_Bitmap(glyph_out, face_settings->render_mode, 0, 1)) != 0)
	{
		TTF_SetFTError(font_file->name(), error);
		return false;
	}

	return true;
}

#if defined(__clang__)
// ubsan is triggered by -(span_granularity)
__attribute__((no_sanitize("unsigned-integer-overflow")))
#endif
bool font_atlas::find_atlas_slot(uint32_t w_in, uint32_t h_in, uint32_t* x_out, uint32_t* y_out)
{
	ASSERT(x_out != NULL);
	ASSERT(y_out != NULL);
	// NOTE: probably could flip the bitmaps and compare which wastes less space.
	if(w_in == 0)
	{
		serrf("%s: width 0\n", __func__);
		return false;
	}
	if(h_in == 0)
	{
		serrf("%s: height 0\n", __func__);
		return false;
	}
	if(w_in > atlas_size)
	{
		serrf("%s: width (%u) > atlas (%u)\n", __func__, w_in, atlas_size);
		return false;
	}
	if(h_in > atlas_size)
	{
		serrf("%s: height (%u) > atlas (%u)\n", __func__, h_in, atlas_size);
		return false;
	}

	// size_t size = ceil(float(w) / float(span_granularity)) * span_granularity
	// this wont work with non power of 2's
	uint32_t pixel_size = (w_in - 1u + span_granularity) & -(span_granularity);
	uint32_t span_size = pixel_size / span_granularity;

	// closest bucket to the size of the glyph that is larger than the size of the entry.
	span_bucket* closest_bucket = NULL;

	// this is a brute force lookup, but should be negligible.
	for(span_bucket& bucket : span_buckets)
	{
		if(bucket.bucket_depth + h_in > atlas_size)
		{
			continue;
		}
		if(bucket.span_count == span_size)
		{
			*x_out = bucket.span_start * span_granularity;
			*y_out = bucket.bucket_depth;
			bucket.bucket_depth += h_in;
			return true;
		}
		if(bucket.span_count > span_size)
		{
			if(closest_bucket == NULL || bucket.span_count < closest_bucket->span_count)
			{
				closest_bucket = &bucket;
			}
		}
	}

	// allocate a new bucket because you can.
	if((spans_allocated + span_size) * span_granularity <= atlas_size)
	{
		span_buckets.emplace_back(spans_allocated, span_size, 0);
		spans_allocated += span_size;

		*x_out = span_buckets.back().span_start * span_granularity;
		*y_out = span_buckets.back().bucket_depth;

		span_buckets.back().bucket_depth += h_in;

		return true;
	}

	// canabalize a bigger bucket.
	if(closest_bucket != NULL)
	{
		// integer trunc is desired.
		if(closest_bucket->span_count / span_size == 0)
		{
			// don't split because more than half is used.
			*x_out = closest_bucket->span_start * span_granularity;
			*y_out = closest_bucket->bucket_depth;
			closest_bucket->bucket_depth += h_in;
			return true;
		}

		// split
		span_buckets.emplace_back(
			closest_bucket->span_start + closest_bucket->span_count,
			closest_bucket->span_count - span_size,
			closest_bucket->bucket_depth);

		*x_out = closest_bucket->span_start * span_granularity;
		*y_out = closest_bucket->bucket_depth;

		// std::deque won't invalidate
		closest_bucket->span_count = span_size;
		closest_bucket->bucket_depth += h_in;

		return true;
	}

	serrf(
		"%s: ran out of atlas space (free %u, size %u)\n",
		__func__,
		(atlas_size / span_granularity) - spans_allocated,
		span_size);
	return false;
}

bool hex_font_data::init(Unique_RWops file, font_atlas* atlas_)
{
	ASSERT(file);
	ASSERT(atlas_);
	atlas = atlas_;
	hex_font_file = std::move(file);
	char internal_buffer[2048];
	BS_ReadStream stream(hex_font_file.get(), internal_buffer, sizeof(internal_buffer));

	// scan the hex file to find the file position of all the blocks.
	while(true)
	{
		// NOLINTNEXTLINE(bugprone-narrowing-conversions)
		RW_ssize_t stream_cursor = stream.Tell();
		if(stream_cursor < 0)
		{
			return false;
		}
		char32_t codepoint;
		// get the character XXXX:...
		{
			// in hexadecimal, 8 characters = 4 bytes.
			char codepoint_hex[9];
			size_t i = 0;
			for(; i < sizeof(codepoint_hex) - 1; ++i)
			{
				codepoint_hex[i] = stream.Take();
				if(codepoint_hex[i] == '\0')
				{
					if(i == 0)
					{
						// this is linux inserting a newline at the end of the file
						// even though the file doesn't end with a newline
						return true;
					}
					serrf("%s: unexpected end or null\n", hex_font_file->name());
					return false;
				}
				if(codepoint_hex[i] == ':')
				{
					break;
				}
			}
			// terminator
			codepoint_hex[i] = '\0';

			char* buffer_end;

			pop_errno_t pop_errno;
			// I am casting a long to int, but it shouldn't be possible to use more than 4 bytes.
			codepoint = strtoul(codepoint_hex, &buffer_end, 16);
			if(errno == ERANGE)
			{
				serrf("%s: out of range\n", hex_font_file->name());
				return false;
			}

			if(buffer_end != codepoint_hex + i || codepoint_hex == buffer_end)
			{
				serrf("%s: failed to convert hex\n", hex_font_file->name());
				return false;
			}
		}
		// get the bitmap XXXX:XXXXXXXXXXXXXXXX...
		{
			if((codepoint / HEX_CHUNK_GLYPHS) >= hex_block_chunks.size())
			{
				// slogf("block: %zu, offset %ld\n", hex_block_chunks.size(), stream_cursor);
				hex_block_chunks.resize(codepoint / HEX_CHUNK_GLYPHS + 1);
				hex_block_chunks.back().offset = stream_cursor;
			}

			// if the codepoint is using a block that is before the back() block.
			if((codepoint / HEX_CHUNK_GLYPHS) + 1 < hex_block_chunks.size())
			{
				serrf("%s: hex unsorted %u\n", hex_font_file->name(), codepoint);
				return false;
			}

			size_t i = 0;
			char c;
			for(;; ++i)
			{
				if(i > HEX_FULL_WIDTH * HEX_HEIGHT / 8 * 2)
				{
					serrf("%s: line too long\n", hex_font_file->name());
					return false;
				}
				c = stream.Take();

				if(c == '\n' || c == '\0')
				{
					break;
				}
			}

			if(i != (HEX_HALF_WIDTH * HEX_HEIGHT / 8) * 2 &&
			   i != (HEX_FULL_WIDTH * HEX_HEIGHT / 8) * 2)
			{
				serrf("%s: bad size\n", hex_font_file->name());
				return false;
			}
			if(c == '\0')
			{
				return true;
			}
		}
	}
	// return true;
}
bool hex_font_data::destroy()
{
	bool success = true;
	if(hex_font_file)
	{
		if(!hex_font_file->close())
		{
			success = false;
		}
		hex_font_file.reset();
	}
	return success;
}

FONT_RESULT
hex_font_data::get_glyph(
	char32_t codepoint, font_style_type style, font_style_result* glyph, float font_scale)
{
	ASSERT(atlas != NULL);
	ASSERT(glyph != NULL);
	// TODO (dootsie): should print a warning if you use bold or italics? but spam...
	bool outline = (style & FONT_STYLE_OUTLINE) != 0;

	if(!hex_font_file)
	{
		return FONT_RESULT::NOT_FOUND;
	}

	if(hex_block_chunks.empty())
	{
		return FONT_RESULT::NOT_FOUND;
	}

	// there isn't any flag in hex that says this is a blank glyph.
	// I could try to check if the glyph is just zeros, but too lazy.
	// there might be more space characters, but only these really matter.
	if(codepoint == ' ')
	{
		glyph->advance = static_cast<int>(HEX_HALF_WIDTH) * font_scale;
		return FONT_RESULT::SPACE;
	}
	if(codepoint == 0x3000)
	{
		glyph->advance = static_cast<int>(HEX_FULL_WIDTH) * font_scale;
		return FONT_RESULT::SPACE;
	}

	size_t block_index = codepoint / HEX_CHUNK_GLYPHS;
	if(block_index >= hex_block_chunks.size())
	{
		// serrf("%s codepoint out of bounds: U+%X\n", __func__, codepoint);
		return FONT_RESULT::NOT_FOUND;
	}
	hex_block_chunk* current_chunk = &hex_block_chunks[block_index];

	if(current_chunk->glyphs == NULL)
	{
		current_chunk->glyphs =
			std::make_unique<decltype(current_chunk->glyphs)::element_type[]>(HEX_CHUNK_GLYPHS);
		memset(
			current_chunk->glyphs.get(),
			0,
			sizeof(decltype(current_chunk->glyphs)::element_type) * HEX_CHUNK_GLYPHS);
		switch(load_hex_block(block_index))
		{
		case FONT_BASIC_RESULT::SUCCESS: break;
		case FONT_BASIC_RESULT::NOT_FOUND: return FONT_RESULT::NOT_FOUND;
		case FONT_BASIC_RESULT::ERROR: return FONT_RESULT::ERROR;
		}
	}

	hex_glyph_entry& current = current_chunk->glyphs[codepoint % HEX_CHUNK_GLYPHS];

	if(current.hex_init)
	{
		//*glyph = (outline ? current.u.hex_glyph.outline : current.u.hex_glyph.normal);
		font_glyph_entry* input =
			(outline ? &current.u.hex_glyph.outline : &current.u.hex_glyph.normal);
		convert_glyph_format(this, input, glyph, font_scale);
		return FONT_RESULT::SUCCESS;
	}

	if(!current.hex_found)
	{
		return FONT_RESULT::NOT_FOUND;
	}

	GLsizei height = HEX_HEIGHT;
	GLsizei width;
	if(current.hex_full)
	{
		width = HEX_FULL_WIDTH;
	}
	else
	{
		width = HEX_HALF_WIDTH;
	}

	uint8_t rgba_tex[HEX_HEIGHT * HEX_FULL_WIDTH];
	uint8_t* img_cur = rgba_tex;

	for(size_t j = 0, size = height * width / 8; j < size; ++j)
	{
		for(size_t bit = 0; bit < 8; ++bit)
		{
			uint8_t fill = ((current.u.hex_data[j] & (1 << (7 - bit))) == 0) ? 0 : 255;
			*img_cur++ = fill;
			//*img_cur++ = fill;
			//*img_cur++ = fill;
			//*img_cur++ = fill;
		}
	}

	// apply a outline effect
	size_t hex_w = (current.hex_full ? HEX_FULL_WIDTH : HEX_HALF_WIDTH);
	size_t hex_h = HEX_HEIGHT;
	unsigned char outline_image_data[(HEX_FULL_WIDTH + 2) * (HEX_HEIGHT + 2)]{};
	for(size_t y = 0; y < hex_h + 2; ++y)
	{
		for(size_t x = 0; x < hex_w + 2; ++x)
		{
			for(size_t ky = 0; ky < 3; ++ky)
			{
				for(size_t kx = 0; kx < 3; ++kx)
				{
					// if the coordinates are within bounds.
					// also note that here I am just putting in random numbers
					// and I seem to have stumbled upon these values by chance.
					if(y + ky <= 1 || x + kx <= 1 || y + ky >= hex_h + 2 || x + kx >= hex_w + 2)
					{
						continue;
					}
					if(rgba_tex[(y + ky - 2) * hex_w + (x + kx - 2)] == 255)
					{
						outline_image_data[(y) * (hex_w + 2) + (x)] = 255;
						// hacky escape... probably should put this in it's own function
						ky = 3;
						kx = 3;
						break;
					}
				}
			}
		}
	}

	// needs one pixel of padding for nearest neighbor filtering (if scaled)
	uint32_t padding = 1;
	int32_t offset = 0;

	// you will still get atlas bleed if you use nearest neighbor
	// but nearest neighbor looks like shit with non pixel perfect scaling.
	if(cv_font_linear_filtering.data == 1)
	{
		padding = 2;
		offset = 1;
	}

	uint32_t x_out;
	uint32_t y_out;
	if(!atlas->find_atlas_slot(width + padding, height + padding, &x_out, &y_out))
	{
		return FONT_RESULT::ERROR;
	}

	uint32_t outline_x_out;
	uint32_t outline_y_out;

	if(!atlas->find_atlas_slot(
		   width + padding + 2, height + padding + 2, &outline_x_out, &outline_y_out))
	{
		return FONT_RESULT::ERROR;
	}

	// ctx.glBindTexture(GL_TEXTURE_2D, gl_atlas_tex_id);
	// note the outline requires GL_UNPACK_ALIGNMENT set to 1
	ctx.glTexSubImage2D(
		GL_TEXTURE_2D,
		0,
		x_out + offset, // NOLINT(bugprone-narrowing-conversions)
		y_out + offset, // NOLINT(bugprone-narrowing-conversions)
		width,
		height,
		GL_RED,
		GL_UNSIGNED_BYTE,
		rgba_tex);

	ctx.glTexSubImage2D(
		GL_TEXTURE_2D,
		0,
		outline_x_out + offset, // NOLINT(bugprone-narrowing-conversions)
		outline_y_out + offset, // NOLINT(bugprone-narrowing-conversions)
		width + 2,
		height + 2,
		GL_RED,
		GL_UNSIGNED_BYTE,
		outline_image_data);
	// ctx.glBindTexture(GL_TEXTURE_2D, 0);

	if(GL_RUNTIME(__func__) != GL_NO_ERROR)
	{
		return FONT_RESULT::ERROR;
	}

	current.u.hex_glyph.normal = font_glyph_entry(
		FONT_ENTRY::GLYPH,
		x_out,
		y_out,
		width + padding,
		height + padding,
		width, // NOLINT(bugprone-narrowing-conversions)
		-offset, // xmin  // NOLINT(bugprone-narrowing-conversions)
		height + offset // ymin //NOLINT(bugprone-narrowing-conversions)
	);

	current.u.hex_glyph.outline = font_glyph_entry(
		FONT_ENTRY::GLYPH,
		outline_x_out,
		outline_y_out,
		width + padding + 2,
		height + padding + 2,
		width, // NOLINT(bugprone-narrowing-conversions)
		-offset - 1, // xmin  // NOLINT(bugprone-narrowing-conversions)
		height + 1 + offset // ymin //NOLINT(bugprone-narrowing-conversions)
	);

	//*glyph = (outline ? current.u.hex_glyph.outline : current.u.hex_glyph.normal);
	font_glyph_entry* input =
		(outline ? &current.u.hex_glyph.outline : &current.u.hex_glyph.normal);
	convert_glyph_format(this, input, glyph, font_scale);

	current.hex_init = true;

	return FONT_RESULT::SUCCESS;
}
FONT_BASIC_RESULT hex_font_data::get_advance(char32_t codepoint, float* advance, float font_scale)
{
	// this is 100% copy pasted from get_glyph
	ASSERT(atlas);
	ASSERT(advance != NULL);

	if(!hex_font_file)
	{
		return FONT_BASIC_RESULT::NOT_FOUND;
	}

	// I probably should set the rect to 0,0,0,0 to avoid rendering, but I am too lazy.
	// if(codepoint == ' ')

	size_t block_index = codepoint / HEX_CHUNK_GLYPHS;
	if(block_index >= hex_block_chunks.size())
	{
		// serrf("%s codepoint out of bounds: U+%X\n", __func__, codepoint);
		return FONT_BASIC_RESULT::NOT_FOUND;
	}
	hex_block_chunk* current_chunk = &hex_block_chunks[block_index];

	if(current_chunk->glyphs == NULL)
	{
		current_chunk->glyphs =
			std::make_unique<decltype(current_chunk->glyphs)::element_type[]>(HEX_CHUNK_GLYPHS);
		memset(
			current_chunk->glyphs.get(),
			0,
			sizeof(decltype(current_chunk->glyphs)::element_type) * HEX_CHUNK_GLYPHS);
		FONT_BASIC_RESULT ret = load_hex_block(block_index);
		if(ret != FONT_BASIC_RESULT::SUCCESS)
		{
			return ret;
		}
	}

	hex_glyph_entry& current = current_chunk->glyphs[codepoint % HEX_CHUNK_GLYPHS];

	if(current.hex_init)
	{
		*advance = static_cast<float>(current.u.hex_glyph.normal.advance) * font_scale;
		return FONT_BASIC_RESULT::SUCCESS;
	}

	if(!current.hex_found)
	{
		return FONT_BASIC_RESULT::NOT_FOUND;
	}

	if(current.hex_full)
	{
		*advance = static_cast<int>(HEX_FULL_WIDTH) * font_scale;
	}
	else
	{
		*advance = static_cast<int>(HEX_HALF_WIDTH) * font_scale;
	}
	return FONT_BASIC_RESULT::SUCCESS;
}

FONT_BASIC_RESULT hex_font_data::load_hex_block(size_t block_index)
{
	hex_block_chunk* chunk = &hex_block_chunks[block_index];
	ASSERT(chunk);
	ASSERT(chunk->offset != -2);

	if(chunk->offset == -2)
	{
		// TODO(dootsie): would be better if the offset wasn't overridden...
		serrf(
			"%s: chunk already loaded (block: %zu, offset: %ld)\n",
			hex_font_file->name(),
			block_index,
			chunk->offset);
	}

	if(chunk->offset < 0)
	{
		return FONT_BASIC_RESULT::NOT_FOUND;
	}

	auto temp_offset = chunk->offset;
	chunk->offset = -2;

	if(hex_font_file->seek(temp_offset, RW_SEEK_SET) < 0)
	{
		return FONT_BASIC_RESULT::ERROR;
	}

	char internal_buffer[2048];
	BS_ReadStream stream(hex_font_file.get(), internal_buffer, sizeof(internal_buffer));

	while(true)
	{
		char32_t codepoint;
		// in hexadecimal, 8 characters = 4 bytes.
		char codepoint_hex[9];
		// get the character XXXX:...
		{
			size_t i = 0;
			for(; i < sizeof(codepoint_hex) - 1; ++i)
			{
				codepoint_hex[i] = stream.Take();
				if(codepoint_hex[i] == '\0')
				{
					if(i == 0)
					{
						// this is linux inserting a newline at the end of the file
						// even though the file doesn't end with a newline
						return FONT_BASIC_RESULT::SUCCESS;
					}
					serrf(
						"%s: unexpected end or null (block: %zu, offset: %ld)\n",
						hex_font_file->name(),
						block_index,
						temp_offset);
					return FONT_BASIC_RESULT::ERROR;
				}
				if(codepoint_hex[i] == ':')
				{
					break;
				}
			}
			// terminator
			codepoint_hex[i] = '\0';

			char* buffer_end;

			pop_errno_t pop_errno;
			// I am casting a long to int, but it shouldn't be possible to use more than 4 bytes.
			codepoint = strtoul(codepoint_hex, &buffer_end, 16);
			if(errno == ERANGE)
			{
				serrf(
					"%s: out of range (block: %zu, offset: %ld)\n",
					hex_font_file->name(),
					block_index,
					temp_offset);
				return FONT_BASIC_RESULT::ERROR;
			}

			if(buffer_end != codepoint_hex + i || codepoint_hex == buffer_end)
			{
				serrf(
					"%s: failed to convert hex (block: %zu, offset: %ld)\n",
					hex_font_file->name(),
					block_index,
					temp_offset);
				return FONT_BASIC_RESULT::ERROR;
			}
		}
		// get the bitmap XXXX:XXXXXXXXXXXXXXXX...
		{
			if(codepoint / HEX_CHUNK_GLYPHS == block_index + 1)
			{
				return FONT_BASIC_RESULT::SUCCESS;
			}
			if(codepoint / HEX_CHUNK_GLYPHS != block_index)
			{
				serrf(
					"%s: hex out of range (cp: %u, block: %zu, offset: %ld)\n",
					hex_font_file->name(),
					codepoint,
					block_index,
					temp_offset);
				return FONT_BASIC_RESULT::ERROR;
			}

			hex_glyph_entry& current_entry = chunk->glyphs[codepoint % HEX_CHUNK_GLYPHS];

			size_t i = 0;
			size_t size = 0;
			char c;
			unsigned char first_pair = '\0';
			for(;; ++i)
			{
				if(i > HEX_FULL_WIDTH * HEX_HEIGHT / 8 * 2)
				{
					serrf(
						"%s: line too long (block: %zu, offset: %ld)\n",
						hex_font_file->name(),
						block_index,
						temp_offset);
					return FONT_BASIC_RESULT::ERROR;
				}
				unsigned char temp;
				c = stream.Take();

				if(c == '\n' || c == '\0')
				{
					break;
				}

				if(c >= '0' && c <= '9')
				{
					temp = c - '0';
				}
				else if(c >= 'A' && c <= 'F')
				{
					temp = c - 'A' + 10;
				}
				else
				{
					serrf(
						"%s: bad hex (block: %zu, offset: %ld)\n",
						hex_font_file->name(),
						block_index,
						temp_offset);
					return FONT_BASIC_RESULT::ERROR;
				}
				if((i % 2) == 0)
				{
					first_pair = temp;
				}
				else
				{
					current_entry.u.hex_data[size++] = (first_pair << 4) | temp;
				}
			}

			if(size != HEX_HALF_WIDTH * HEX_HEIGHT / 8 && size != HEX_FULL_WIDTH * HEX_HEIGHT / 8)
			{
				serrf(
					"%s: bad size (block: %zu, offset: %ld)\n",
					hex_font_file->name(),
					block_index,
					temp_offset);
				return FONT_BASIC_RESULT::ERROR;
			}

			current_entry.hex_found = true;

			if(size == HEX_FULL_WIDTH * HEX_HEIGHT / 8)
			{
				current_entry.hex_full = true;
			}
			if(c == '\0')
			{
				return FONT_BASIC_RESULT::SUCCESS;
			}
		}
	}
	// return true;
}

void font_bitmap_cache::init(font_manager_state* font_manager, font_ttf_rasterizer* rasterizer)
{
	ASSERT(font_manager != NULL);
	ASSERT(rasterizer != NULL);

	atlas = &font_manager->atlas;
	current_rasterizer = rasterizer;
	fallback = &font_manager->hex_font;

	FT_Face face = current_rasterizer->face;
	const font_ttf_face_settings* face_settings = current_rasterizer->face_settings;
	if(face->num_fixed_sizes != 0 && (!FT_IS_SCALABLE(face) || face_settings->force_bitmap))
	{
		int bitmap_height = FT_CEIL(face->size->metrics.height);
		bitmap_scale = face_settings->point_size / static_cast<float>(bitmap_height);
	}

	FT_Bitmap_Init(&convert_bitmap);
}

bool font_bitmap_cache::destroy()
{
	if(current_rasterizer != NULL)
	{
		FT_Error error = FT_Bitmap_Done(current_rasterizer->FTLibrary, &convert_bitmap);
		if(error != 0)
		{
			TTF_SetFTError(current_rasterizer->font_file->name(), error);
			current_rasterizer = NULL;
			return false;
		}
		current_rasterizer = NULL;
	}

	return true;
}

font_bitmap_cache::~font_bitmap_cache()
{
	if(current_rasterizer != NULL)
	{
		// the error is ignored
		FT_Error error = FT_Bitmap_Done(current_rasterizer->FTLibrary, &convert_bitmap);
		ASSERT(error == 0);
		(void)error;
		current_rasterizer = NULL;
	}
}

FT_Bitmap* font_bitmap_cache::render_tf_glyph(
	FT_UInt glyph_index, font_style_type style, unique_ft_glyph& ftglyph)
{
	FT_Error error;

	// load the outline or bitmap
	if((error = FT_Load_Glyph(
			current_rasterizer->face,
			glyph_index,
			current_rasterizer->face_settings->load_flags)) != 0)
	{
		TTF_SetFTError(current_rasterizer->font_file->name(), error);
		return NULL;
	}
	{
		FT_Glyph tmp_ftglyph;
		// You must call FT_Done_Glyph on this.
		if((error = FT_Get_Glyph(current_rasterizer->face->glyph, &tmp_ftglyph)) != 0)
		{
			TTF_SetFTError(current_rasterizer->font_file->name(), error);
			return NULL;
		}
		ftglyph.reset(tmp_ftglyph);
	}
	bool was_bitmap = ftglyph->format == FT_GLYPH_FORMAT_BITMAP;
	bool use_outline = (style & FONT_STYLE_OUTLINE) != 0;
	if(ftglyph->format == FT_GLYPH_FORMAT_OUTLINE)
	{
		int temp_style = style;
		if(current_rasterizer->face_settings->force_bitmap && use_outline)
		{
			// force the outline to not be rendered, so that I can make the outline use the bitmap
			// routine.
			temp_style = style & ~FONT_STYLE_OUTLINE;
		}
		FT_Glyph tmp_ftglyph;
		// You must call FT_Done_Glyph on this.
		if(!current_rasterizer->render_glyph(&tmp_ftglyph, temp_style))
		{
			return NULL;
		}
		// this will destroy the original outline and move the bitmap into it
		ftglyph.reset(tmp_ftglyph);
	}
	if(ftglyph->format != FT_GLYPH_FORMAT_BITMAP)
	{
		serrf("%s not a bitmap?\n", current_rasterizer->font_file->name());
		return NULL;
	}

	// FT_BitmapGlyph is a subclass of FT_Glyph if ftglyph->format == FT_GLYPH_FORMAT_BITMAP
	FT_BitmapGlyph ftglyph_bitmap = reinterpret_cast<FT_BitmapGlyph>(ftglyph.get());

	// this could be either ftglyph or convert_bitmap
	FT_Bitmap* bitmap = &ftglyph_bitmap->bitmap;

	if(bitmap->width == 0)
	{
		// this is a space
		return bitmap;
	}

	bool use_bitmap_embold =
		use_outline && (was_bitmap || current_rasterizer->face_settings->force_bitmap);

	if(ftglyph_bitmap->bitmap.pixel_mode != FT_PIXEL_MODE_GRAY || use_bitmap_embold)
	{
		if((error = FT_Bitmap_Convert(
				current_rasterizer->FTLibrary, &ftglyph_bitmap->bitmap, &convert_bitmap, 1)) != 0)
		{
			TTF_SetFTError(current_rasterizer->font_file->name(), error);
			return NULL;
		}
		bitmap = &convert_bitmap;
	}

	// I could also try to implement bold as well here,
	// but I want to be consistant with hexfont so I dont
	if(use_bitmap_embold)
	{
		// this returns an error for "space" characters
		if((error = FT_Bitmap_Embolden(
				current_rasterizer->FTLibrary,
				&convert_bitmap,
				static_cast<FT_Pos>(current_rasterizer->face_settings->outline_size * 64 * 2),
				static_cast<FT_Pos>(current_rasterizer->face_settings->outline_size * 64 * 2))) !=
		   0)
		{
			TTF_SetFTError(current_rasterizer->font_file->name(), error);
			return NULL;
		}
		// I probably shouldn't do this, but it works!
		ftglyph_bitmap->left -=
			static_cast<FT_Int>(current_rasterizer->face_settings->outline_size);
		ftglyph_bitmap->top += static_cast<FT_Int>(current_rasterizer->face_settings->outline_size);
	}

	if(ftglyph_bitmap->bitmap.pixel_mode == FT_PIXEL_MODE_MONO)
	{
		// this will contain 0-1 instead of 0-255...
		unsigned char* buffer = convert_bitmap.buffer;
		size_t length = convert_bitmap.width * convert_bitmap.rows;

		for(size_t j = 0; j < length; ++j)
		{
			*buffer++ *= 255;
		}
	}

	return bitmap;
}

float font_bitmap_cache::get_ascent(float font_scale)
{
	FT_Face face = current_rasterizer->face;
	const font_ttf_face_settings* face_settings = current_rasterizer->face_settings;
	if(face->num_fixed_sizes != 0 && (!FT_IS_SCALABLE(face) || face_settings->force_bitmap))
	{
		// int bitmap_height = FT_CEIL(face->size->metrics.height);
		int bitmap_ascent = FT_CEIL(face->size->metrics.ascender);
		return static_cast<float>(bitmap_ascent) * font_scale;
	}
	// Get the scalable font metrics for this font
	// FT_Fixed scale = face->size->metrics.y_scale;
	// NOLINTNEXTLINE(bugprone-integer-division
	// return FT_CEIL(FT_MulFix(face->ascender, scale)) * font_scale;

	// only problem with this is that it looks bad with FSEX300.ttf
	// can be fixed with force_bitmap
	return get_point_size() * font_scale;
}
float font_bitmap_cache::get_lineskip(float font_scale)
{
	FT_Face face = current_rasterizer->face;
#if 0
	const font_ttf_face_settings* face_settings = current_rasterizer->face_settings;
    if(face->num_fixed_sizes != 0 && (!FT_IS_SCALABLE(face) || face_settings->force_bitmap))
	{
		int bitmap_height = FT_CEIL(face->size->metrics.height);
		return static_cast<float>(bitmap_height) * font_scale;
		// return std::ceil(face_settings->point_size * font_scale);
	}
#endif

	// I really want to use this, but cant due to NotoSans-Regular.ttf.
	// Maybe I can use the underline instead and give a fake descent?
	// return get_point_size() * font_scale;

	FT_Fixed scale = face->size->metrics.y_scale;

	int ascent = FT_CEIL(FT_MulFix(face->ascender, scale));

	// this is technically the "correct" way
	// but the problem is that I want all fonts to look good with unifont,
	// but unifont really wants pointsize = lineskip.
	// and the real lineskip is too big.
	// int ascent = FT_CEIL(FT_MulFix(face->bbox.yMax, scale));
	// int descent = FT_CEIL(FT_MulFix(face->bbox.yMin, scale));
	// return static_cast<float>(ascent - descent + /* baseline */ 1) * font_scale;

	// make room for the bottom part of the letter because we use pointsize for the ascent
	int height = FT_CEIL(FT_MulFix(face->height, scale));
	// NOLINTNEXTLINE(bugprone-integer-division)
	return (static_cast<float>(height) + (get_point_size() - static_cast<float>(ascent))) *
		   font_scale;
}

float font_bitmap_cache::get_point_size()
{
	const font_ttf_face_settings* face_settings = current_rasterizer->face_settings;
	return face_settings->point_size;
}
FONT_BASIC_RESULT
font_bitmap_cache::get_advance(char32_t codepoint, float* advance, float font_scale)
{
	ASSERT(atlas != NULL);
	ASSERT(current_rasterizer != NULL);
	ASSERT(advance != NULL);

	// 10FFFF is the last unicode character I think
	// so a glyph past this is probably garbage.
	const char32_t unicode_end = 0x10FFFF;
	if(codepoint > unicode_end)
	{
#if 0
		serrf(
			"%s unicode out of bounds: U+%X, maximum: U+%X\n",
			current_rasterizer->font_file->name(),
			codepoint,
			unicode_end);
#endif
		return FONT_BASIC_RESULT::NOT_FOUND;
	}

	size_t block_chunk = codepoint / FONT_CACHE_CHUNK_GLYPHS;
	size_t block_index = codepoint % FONT_CACHE_CHUNK_GLYPHS;

	// load the blocks if it's not already loaded
	if(block_chunk >= font_cache_blocks.size())
	{
		font_cache_blocks.resize(block_chunk + 1);
	}

	font_cache_block& block = font_cache_blocks[block_chunk];

	// fast path for cached glyphs.
	if(block.glyphs[FONT_STYLE_NORMAL])
	{
		font_glyph_entry& glyph = block.glyphs[FONT_STYLE_NORMAL][block_index];
		switch(glyph.type)
		{
		case FONT_ENTRY::UNDEFINED: break;
		case FONT_ENTRY::GLYPH:
		case FONT_ENTRY::SPACE:
			*advance = static_cast<float>(glyph.advance) * font_scale * bitmap_scale;
			return FONT_BASIC_RESULT::SUCCESS;
		}
	}

	FT_UInt glyph_index = 0;
	if(!block.bad_indexes.test(block_index))
	{
		glyph_index = FT_Get_Char_Index(current_rasterizer->face, codepoint);
		if(glyph_index == 0)
		{
			if(cv_font_fallback_warning.data != 0)
			{
				slogf(
					"info: %s fallback not found: U+%X\n",
					current_rasterizer->font_file->name(),
					codepoint);
			}
			block.bad_indexes.set(block_index);
		}
	}

	if(glyph_index == 0)
	{
		float fallback_scale = (get_point_size() * font_scale) / fallback->get_point_size();
		return fallback->get_advance(codepoint, advance, fallback_scale);
	}

#if 0
    //getting the advance is sort of slow, but not that slow.
	TIMER_U tick1;
	TIMER_U tick2;
	tick1 = timer_now();
#endif
	FT_Error error;

	FT_Int32 ftflags = FT_LOAD_ADVANCE_ONLY;

	// with mono hinting the offset is off.
	// hopefully FT_LOAD_ADVANCE_ONLY overrides any possible rasterization.
	// I haven't tested though
	ftflags |= current_rasterizer->face_settings->load_flags;

	// load the advance only
	if((error = FT_Load_Glyph(current_rasterizer->face, glyph_index, ftflags)) != 0)
	{
		TTF_SetFTError(current_rasterizer->font_file->name(), error);
		return FONT_BASIC_RESULT::ERROR;
	}
#if 0
	tick2 = timer_now();
	slogf("advance time = %f\n", timer_delta_ms(tick1, tick2));
#endif

	int font_advance = FT_FLOOR(current_rasterizer->face->glyph->advance.x);
	*advance = static_cast<float>(font_advance) * font_scale * bitmap_scale;

	return FONT_BASIC_RESULT::SUCCESS;
}

FONT_RESULT font_bitmap_cache::get_glyph(
	char32_t codepoint, font_style_type style, font_style_result* glyph_out, float font_scale)
{
	ASSERT(atlas != NULL);
	ASSERT(current_rasterizer != NULL);
	ASSERT(glyph_out != NULL);

	// 10FFFF is the last unicode character I think
	// so a glyph past this is probably garbage.
	const char32_t unicode_end = 0x10FFFF;
	if(codepoint > unicode_end)
	{
#if 0
		serrf(
			"%s unicode out of bounds: U+%X, maximum: U+%X\n",
			current_rasterizer->font_file->name(),
			codepoint,
			unicode_end);
#endif
		return FONT_RESULT::NOT_FOUND;
	}

	size_t block_chunk = codepoint / FONT_CACHE_CHUNK_GLYPHS;
	size_t block_index = codepoint % FONT_CACHE_CHUNK_GLYPHS;

	// load the blocks if it's not already loaded
	if(block_chunk >= font_cache_blocks.size())
	{
		font_cache_blocks.resize(block_chunk + 1);
	}

	font_cache_block& block = font_cache_blocks[block_chunk];

	// fast path for cached glyphs.
	if(block.glyphs[style])
	{
		font_glyph_entry* glyph_in = &block.glyphs[style][block_index];
		switch(glyph_in->type)
		{
		case FONT_ENTRY::UNDEFINED: break;
		case FONT_ENTRY::GLYPH:
			convert_glyph_format(this, glyph_in, glyph_out, font_scale * bitmap_scale);
			return FONT_RESULT::SUCCESS;
		case FONT_ENTRY::SPACE:
			glyph_out->advance = static_cast<float>(glyph_in->advance) * font_scale * bitmap_scale;
			return FONT_RESULT::SPACE;
		}
	}

	FT_UInt glyph_index = 0;
	if(!block.bad_indexes.test(block_index))
	{
		glyph_index = FT_Get_Char_Index(current_rasterizer->face, codepoint);
		if(glyph_index == 0)
		{
			if(cv_font_fallback_warning.data != 0)
			{
				slogf(
					"info: %s fallback not found: U+%X\n",
					current_rasterizer->font_file->name(),
					codepoint);
			}
			block.bad_indexes.set(block_index);
		}
	}

	if(glyph_index == 0)
	{
		float fallback_scale = (get_point_size() * font_scale) / fallback->get_point_size();
		FONT_RESULT ret = fallback->get_glyph(codepoint, style, glyph_out, fallback_scale);
		if(ret == FONT_RESULT::SUCCESS)
		{
			// shift the y axis if the lineskip is different.
			float yoffset = std::ceil(
				(get_lineskip(font_scale) - fallback->get_lineskip(fallback_scale)) / 2.f);
			glyph_out->glyph_ymin += yoffset;
			glyph_out->glyph_ymax += yoffset;
		}
		return ret;
	}

	// this owns the FT_Bitmap memory (but sometimes it's convert_bitmap)
	unique_ft_glyph ftglyph;

	// TODO (dootsie): Maybe load normal and outline together
	// because I can avoid rasterizing twice if I use a bitmap embolden.
	// but it should only be a "hint" because I might not use the outline...
	FT_Bitmap* bitmap = render_tf_glyph(glyph_index, style, ftglyph);

	/// load the glyph
	if(bitmap == NULL)
	{
		// render_tf_glyph won't print the codepoint, so might as well include it here.
		serrf("%s error: U+%X\n", current_rasterizer->font_file->name(), codepoint);
		// next time just load the fallback
		block.bad_indexes.set(block_index);
		return FONT_RESULT::ERROR;
	}

	if(!block.glyphs[style])
	{
		// allocate the style array
		block.glyphs[style] = std::make_unique<font_glyph_entry[]>(FONT_CACHE_CHUNK_GLYPHS);
		// zero initialize.
		memset(block.glyphs[style].get(), 0, sizeof(font_glyph_entry) * FONT_CACHE_CHUNK_GLYPHS);
	}

	// this slot should be undefined.
	ASSERT(block.glyphs[style][block_index].type == FONT_ENTRY::UNDEFINED);

	font_glyph_entry* glyph_in = &block.glyphs[style][block_index];

	int font_advance = FT_FLOOR(current_rasterizer->face->glyph->advance.x);

	// this is a space character.
	if(bitmap->width == 0)
	{
		// use this to signal this is a space
		glyph_in->type = FONT_ENTRY::SPACE;

		// NOLINTNEXTLINE(bugprone-narrowing-conversions)
		glyph_in->advance = font_advance;

		// glyph_convert(glyph_in, glyph_out, font_scale);
		glyph_out->advance = static_cast<float>(glyph_in->advance) * font_scale * bitmap_scale;
		return FONT_RESULT::SPACE;
	}

	// needs one pixel of padding for nearest neighbor filtering (if scaled)
	uint32_t padding = 1;
	int32_t offset = 0;

	if(cv_font_linear_filtering.data == 1)
	{
		padding = 2;
		offset = 1;
	}

	unsigned int x_out;
	unsigned int y_out;
	if(!atlas->find_atlas_slot(bitmap->pitch + padding, bitmap->rows + padding, &x_out, &y_out))
	{
		// I could use the fallback, but I want to only use it to show the glyph
		// isn't found.
		serrf("%s atlas out of space: U+%X\n", current_rasterizer->font_file->name(), codepoint);
		// it->second[raster_mask] = font_manager.error_glyph;
		//*glyph = it->second[raster_mask];
		// next time just load the fallback
		block.bad_indexes.set(block_index);
		return FONT_RESULT::ERROR;
	}
#if 0
    if(cv_has_EXT_disjoint_timer_query.data == 1)
    {
        static GLuint query = 0;
        if(query == 0)
        {
            ctx.glGenQueriesEXT(1, &query);
        }
        ctx.glBeginQueryEXT(GL_TIME_ELAPSED_EXT,query);
        TIMER_U t1 = timer_now();
    }
#endif
	ctx.glTexSubImage2D(
		GL_TEXTURE_2D,
		0,
		x_out + offset, // NOLINT(bugprone-narrowing-conversions)
		y_out + offset, // NOLINT(bugprone-narrowing-conversions)
		bitmap->pitch,
		bitmap->rows, // NOLINT(bugprone-narrowing-conversions)
		GL_RED,
		GL_UNSIGNED_BYTE,
		bitmap->buffer);
#if 0
    if(cv_has_EXT_disjoint_timer_query.data == 1)
    {
        TIMER_U t2 = timer_now();
        ctx.glEndQueryEXT(GL_TIME_ELAPSED_EXT);

        GLuint64 elapsed_time;
        ctx.glGetQueryObjectui64vEXT(query, GL_QUERY_RESULT, &elapsed_time);
        slogf("glTexSubImage2D time: %f, wait: %f\n", elapsed_time / 1000000.0, timer_delta_ms(t1, t2));
    }
#endif
	if(GL_RUNTIME(__func__) != GL_NO_ERROR)
	{
		return FONT_RESULT::ERROR;
	}

	glyph_in->rect_x = x_out;
	glyph_in->rect_y = y_out;
	glyph_in->rect_w = bitmap->width + padding;
	glyph_in->rect_h = bitmap->rows + padding;
	// NOLINTNEXTLINE(bugprone-narrowing-conversions)
	glyph_in->advance = font_advance;
	FT_BitmapGlyph ftglyph_bitmap = reinterpret_cast<FT_BitmapGlyph>(ftglyph.get());
	// NOLINTNEXTLINE(bugprone-narrowing-conversions)
	glyph_in->xmin = (ftglyph_bitmap->left) - offset;
	// NOLINTNEXTLINE(bugprone-narrowing-conversions)
	glyph_in->ymin = (ftglyph_bitmap->top) + offset;
	glyph_in->type = FONT_ENTRY::GLYPH;
	convert_glyph_format(this, glyph_in, glyph_out, font_scale * bitmap_scale);

	return FONT_RESULT::SUCCESS;
}

FONT_BASIC_RESULT
internal_font_painter_state::load_glyph_verts(
	char32_t codepoint, std::array<uint8_t, 4> color, font_style_type style, float font_scale)
{
	ASSERT(font != NULL);
	ASSERT(batcher != NULL);

	font_style_result glyph;

	switch(font->get_glyph(codepoint, style, &glyph, font_scale))
	{
	case FONT_RESULT::SUCCESS: {
		std::array<float, 4> uv{
			glyph.atlas_xmin, glyph.atlas_ymin, glyph.atlas_xmax, glyph.atlas_ymax};

		std::array<float, 4> pos{
			draw_x_pos + glyph.glyph_xmin,
			draw_y_pos + glyph.glyph_ymin,
			draw_x_pos + glyph.glyph_xmax,
			draw_y_pos + glyph.glyph_ymax};
		batcher->draw_rect(pos, uv, color);
		draw_x_pos += glyph.advance;
		return FONT_BASIC_RESULT::SUCCESS;
	}
	case FONT_RESULT::SPACE: draw_x_pos += glyph.advance; return FONT_BASIC_RESULT::SUCCESS;
	case FONT_RESULT::ERROR: return FONT_BASIC_RESULT::ERROR;
	case FONT_RESULT::NOT_FOUND: return FONT_BASIC_RESULT::NOT_FOUND;
	}

	return FONT_BASIC_RESULT::SUCCESS;
}

float font_sprite_painter::get_scale() const
{
	return raw_font_scale * static_cast<float>(cv_ui_scale.data);
}

void font_sprite_painter::set_scale(float font_scale)
{
	raw_font_scale = font_scale;
}

void font_sprite_painter::begin()
{
	newline_cursor = state.batcher->get_current_vertex_count();
	flush_cursor = state.batcher->get_current_vertex_count();
}

void font_sprite_painter::end()
{
	// fix the y axis alignment
	internal_flush();
}

bool font_sprite_painter::draw_format(const char* fmt, ...)
{
	ASSERT(fmt != NULL);

	char buffer[10000];
	size_t size = sizeof buffer;

	pop_errno_t pop_errno;
	va_list args;
	va_start(args, fmt);
	int ret = vsnprintf(buffer, size, fmt, args);
	va_end(args);
	if(ret < 0)
	{
		// on linux I could also use explain_vsnprintf
		serrf(
			"%s: %s\n"
			"expression: `%s`\n",
			__func__,
			strerror(errno),
			fmt);
		return false;
	}

	size_t trunc_size = std::min<size_t>(ret, size - 1);

	if(static_cast<size_t>(ret) >= size)
	{
		slogf(
			"info: %s truncated string format buffer (size: %zu, ret: %i)\n"
			"expression: `%s`\n",
			__func__,
			size,
			ret,
			fmt);
#if 0

		// NOT_ENOUGH_ROOM is expected when we truncate a string...
		// and I don't treat truncation as a "error"
		// but to be polite, I should trim the end off.
		const char* str_cur = buffer;
		const char* str_end = buffer + trunc_size;
		while(str_cur != str_end)
		{
			uint32_t codepoint;
			utf8::internal::utf_error err_code =
				utf8::internal::validate_next(str_cur, str_end, codepoint);
			if(err_code == utf8::internal::NOT_ENOUGH_ROOM)
			{
				slogf(
					"info: %s utf8: %s\n"
					"expression: `%s`\n",
					__func__,
					cpputf_get_error(err_code),
					fmt);
				trunc_size = (str_cur - buffer);
				break;
			}
			if(err_code != utf8::internal::UTF8_OK)
			{
				// even though an error should be returned
				// I want the string to get drawn
				// up to where the error happens.
				break;
			}
		}
#endif
	}
	return draw_text(buffer, trunc_size);
}

bool font_sprite_painter::measure_text_bounds(
	const char* text, size_t size, float* w_out, float* h_out)
{
	ASSERT(state.font != NULL);

	// I really don't think I need to do this, but it's here...
	size = (size == 0) ? strlen(text) : size;

	const char* str_cur = text;
	const char* str_end = text + size;

	float max_width = 0;
	float current_line_width = 0;
	float line_count = 1;

	while(str_cur != str_end)
	{
		uint32_t codepoint;
		utf8::internal::utf_error err_code =
			utf8::internal::validate_next(str_cur, str_end, codepoint);
		if(err_code != utf8::internal::UTF8_OK)
		{
			// this will render some invalid glyph,
			// unifont makes it very descriptive.
			codepoint = static_cast<unsigned char>(*str_cur++);
		}
#if 0
		if(err_code != utf8::internal::UTF8_OK)
		{
			serrf("%s bad utf8: %s\n", __func__, cpputf_get_error(err_code));
			return false;
		}
#endif

		if((current_flags & TEXT_FLAGS::NEWLINE) != 0 && codepoint == '\n')
		{
			max_width = std::max(current_line_width, max_width);
			current_line_width = 0;
			line_count += 1;
		}
		else if((current_flags & TEXT_FLAGS::NEWLINE) != 0 && codepoint == '\r')
		{
			// assume that this is a carriage return (common on windows files),
			// but I should check if the next character is a newline...
		}
		else
		{
			float advance = 0;
			switch(state.font->get_advance(codepoint, &advance, get_scale()))
			{
			case FONT_BASIC_RESULT::NOT_FOUND:
				// serrf("%s glyph not found: U+%X\n", __func__, codepoint);
				{
					// TODO(dootsie): this is copy pasted between the prompt and painter
					float padding = 1.f * (16.f / state.font->get_point_size());
					float width = (state.font->get_point_size() / 2.f);
					current_line_width += std::ceil(width + padding * 2.f) * get_scale();
					break;
				}
			case FONT_BASIC_RESULT::ERROR: return false;
			case FONT_BASIC_RESULT::SUCCESS: current_line_width += advance; break;
			}
		}
	}
	max_width = std::max(current_line_width, max_width);

	if(w_out != NULL)
	{
		*w_out = max_width;
	}
	if(h_out != NULL)
	{
		*h_out = line_count * get_lineskip();
	}

	return true;
}

bool font_sprite_painter::draw_text(const char* text, size_t size)
{
	ASSERT(state.font != NULL);
	size = (size == 0) ? strlen(text) : size;

	const char* str_cur = text;
	const char* str_end = text + size;

	while(str_cur != str_end)
	{
		uint32_t codepoint;
		utf8::internal::utf_error err_code =
			utf8::internal::validate_next(str_cur, str_end, codepoint);
		if(err_code != utf8::internal::UTF8_OK)
		{
			// this will render some invalid glyph,
			// unifont makes it very descriptive.
			codepoint = static_cast<unsigned char>(*str_cur++);
		}
#if 0
		if(err_code != utf8::internal::UTF8_OK)
		{
			serrf("%s bad utf8: %s\n", __func__, cpputf_get_error(err_code));
			return false;
		}
#endif

		if((current_flags & TEXT_FLAGS::NEWLINE) != 0 && codepoint == '\n')
		{
			newline();
		}
		else if((current_flags & TEXT_FLAGS::NEWLINE) != 0 && codepoint == '\r')
		{
			// assume that this is a carriage return (common on windows files),
			// but I should check if the next character is a newline...
		}
		else
		{
			switch(state.load_glyph_verts(codepoint, cur_color, current_style, get_scale()))
			{
			case FONT_BASIC_RESULT::NOT_FOUND:
				// serrf("%s glyph not found: U+%X\n", __func__, codepoint);
				// TODO(dootsie): this is copy pasted between the prompt and painter
				{
					float padding = 1.f * (16.f / state.font->get_point_size());
					float width = (state.font->get_point_size() / 2.f);
					float height = state.font->get_point_size() * get_scale();
					std::array<float, 4> pos{
						state.draw_x_pos + (padding)*get_scale(),
						state.draw_y_pos,
						state.draw_x_pos + (padding + width) * get_scale(),
						state.draw_y_pos + height};
					state.batcher->draw_rect(
						pos, state.font->get_font_atlas()->white_uv, cur_color);
					state.draw_x_pos += std::ceil(width + padding * 2.f) * get_scale();
				}
				break;
			case FONT_BASIC_RESULT::ERROR: return false;
			case FONT_BASIC_RESULT::SUCCESS: break;
			}
		}
	}

	return true;
}

void font_sprite_painter::internal_flush()
{
	// finish the x axis alignment of any leftover newline
	newline();

	size_t size = state.batcher->get_current_vertex_count();

	switch(current_anchor)
	{
	// topleft how the text is formatted from load_glyph_verts
	case TEXT_ANCHOR::TOP_LEFT:
	case TEXT_ANCHOR::TOP_RIGHT: break;
	case TEXT_ANCHOR::BOTTOM_LEFT: {
		float off_h = state.draw_y_pos - anchor_y;
		for(; flush_cursor < size; ++flush_cursor)
		{
			state.batcher->buffer[flush_cursor].pos[1] -= off_h;
		}
	}
	break;
	case TEXT_ANCHOR::BOTTOM_RIGHT: {
		float off_h = state.draw_y_pos - anchor_y;
		for(; flush_cursor < size; ++flush_cursor)
		{
			state.batcher->buffer[flush_cursor].pos[1] -= off_h;
		}
	}
	break;
	case TEXT_ANCHOR::CENTER_PERFECT: {
		float off_h = std::floor((state.draw_y_pos - anchor_y) / 2);
		for(; flush_cursor < size; ++flush_cursor)
		{
			state.batcher->buffer[flush_cursor].pos[1] -= off_h;
		}
	}
	break;
	case TEXT_ANCHOR::CENTER_TOP: break;
	case TEXT_ANCHOR::CENTER_BOTTOM: {
		float off_h = state.draw_y_pos - anchor_y;
		for(; flush_cursor < size; ++flush_cursor)
		{
			state.batcher->buffer[flush_cursor].pos[1] -= off_h;
		}
	}
	break;
	case TEXT_ANCHOR::CENTER_LEFT: {
		float off_h = std::floor((state.draw_y_pos - anchor_y) / 2);
		for(; flush_cursor < size; ++flush_cursor)
		{
			state.batcher->buffer[flush_cursor].pos[1] -= off_h;
		}
	}
	break;
	case TEXT_ANCHOR::CENTER_RIGHT: {
		float off_h = std::floor((state.draw_y_pos - anchor_y) / 2);
		for(; flush_cursor < size; ++flush_cursor)
		{
			state.batcher->buffer[flush_cursor].pos[1] -= off_h;
		}
	}
	break;
	}
	// for any cases that didn't move the flush_cursor
	flush_cursor = size;
}

void font_sprite_painter::newline()
{
	size_t size = state.batcher->get_current_vertex_count();

	switch(current_anchor)
	{
	// topleft how the text is formatted from load_glyph_verts
	case TEXT_ANCHOR::TOP_LEFT: break;
	case TEXT_ANCHOR::TOP_RIGHT: {
		float off_w = state.draw_x_pos - anchor_x;
		for(; newline_cursor < size; ++newline_cursor)
		{
			state.batcher->buffer[newline_cursor].pos[0] -= off_w;
		}
	}
	break;
	case TEXT_ANCHOR::BOTTOM_LEFT: break;
	case TEXT_ANCHOR::BOTTOM_RIGHT: {
		float off_w = state.draw_x_pos - anchor_x;
		for(; newline_cursor < size; ++newline_cursor)
		{
			state.batcher->buffer[newline_cursor].pos[0] -= off_w;
		}
	}
	break;
	case TEXT_ANCHOR::CENTER_PERFECT: {
		float off_w = std::floor((state.draw_x_pos - anchor_x) / 2);
		for(; newline_cursor < size; ++newline_cursor)
		{
			state.batcher->buffer[newline_cursor].pos[0] -= off_w;
		}
	}
	break;
	case TEXT_ANCHOR::CENTER_TOP: {
		float off_w = std::floor((state.draw_x_pos - anchor_x) / 2);
		for(; newline_cursor < size; ++newline_cursor)
		{
			state.batcher->buffer[newline_cursor].pos[0] -= off_w;
		}
	}
	break;
	case TEXT_ANCHOR::CENTER_BOTTOM: {
		float off_w = std::floor((state.draw_x_pos - anchor_x) / 2);
		for(; newline_cursor < size; ++newline_cursor)
		{
			state.batcher->buffer[newline_cursor].pos[0] -= off_w;
		}
	}
	break;
	case TEXT_ANCHOR::CENTER_LEFT: break;
	case TEXT_ANCHOR::CENTER_RIGHT: {
		float off_w = state.draw_x_pos - anchor_x;
		for(; newline_cursor < size; ++newline_cursor)
		{
			state.batcher->buffer[newline_cursor].pos[0] -= off_w;
		}
	}
	break;
	}
	// for any cases that didn't move the newline_cursor
	newline_cursor = size;

	state.draw_x_pos = anchor_x;
	state.draw_y_pos += get_lineskip();
}
