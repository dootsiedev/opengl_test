#include "global_pch.h"
#include "global.h"

#include "demo.h"

#include "opengles2/opengl_stuff.h"

#include "RWops.h"
#include "font/font_manager.h"
#include "app.h"
#include "debug_tools.h"
#include "keybind.h"

#include <SDL2/SDL.h>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/mat4x4.hpp>

// Include GLM matrix extensions:
#include <glm/ext/matrix_float4x4.hpp> // mat4
#include <glm/ext/matrix_transform.hpp> // perspective, translate, rotate

#include <glm/gtc/type_ptr.hpp>
#include <limits>
#include <string>

REGISTER_CVAR_DOUBLE(
	cv_mouse_sensitivity, 0.4, "mouse move speed while in first person", CVAR_T::RUNTIME);
REGISTER_CVAR_DOUBLE(
	cv_camera_speed, 20.0, "direction move speed while in first person", CVAR_T::RUNTIME);
REGISTER_CVAR_INT(
	cv_mouse_invert, 0, "invert while in first person, 0 = off, 1 = invert", CVAR_T::RUNTIME);

static REGISTER_CVAR_STRING(
	cv_hexfile_path,
	"unifont-full.hex",
	"the fallback font used for rare unicode",
	CVAR_T::STARTUP);

static REGISTER_CVAR_STRING(
	cv_string,
	"test\n"
	"f1 - open console\n"
	"alt+enter - fullscreen\n"
	"wasd - move\n"
	"/?- open options",
	"the string to display",
	CVAR_T::STARTUP);
REGISTER_CVAR_DOUBLE(cv_string_pt, 16.0, "the point size of the string", CVAR_T::STARTUP);
REGISTER_CVAR_STRING(
	cv_string_font,
	"seguiemj.ttf",
	"the font of the string, \"unifont\" is a special font.",
	CVAR_T::STARTUP);
REGISTER_CVAR_DOUBLE(
	cv_string_outline, 1, "outline thickness in pixels (if there is an outline)", CVAR_T::STARTUP);
REGISTER_CVAR_INT(cv_string_mono, 0, "0 = off, 1 = use mono rasterizer", CVAR_T::STARTUP);
REGISTER_CVAR_INT(
	cv_string_force_bitmap,
	0,
	"0 = off, 1 = on, can't bold or italics, but looks different",
	CVAR_T::STARTUP);

// if the pixel's alpha is is greater/equal than the reference value, draw the pixel.
REGISTER_CVAR_DOUBLE(
	cv_string_alpha_test, -1, "-1 = no alpha testing, 0-1 = alpha test", CVAR_T::STARTUP);

// keybinds
REGISTER_CVAR_KEY_BIND_KEY(cv_bind_move_forward, SDLK_w, false, "move forward");
REGISTER_CVAR_KEY_BIND_KEY(cv_bind_move_backward, SDLK_s, false, "move backward");
REGISTER_CVAR_KEY_BIND_KEY(cv_bind_move_left, SDLK_a, false, "move left");
REGISTER_CVAR_KEY_BIND_KEY(cv_bind_move_right, SDLK_d, false, "move right");
REGISTER_CVAR_KEY_BIND_KEY(cv_bind_move_jump, SDLK_SPACE, false, "jump or lift");
REGISTER_CVAR_KEY_BIND_KEY(cv_bind_move_crouch, SDLK_c, false, "crouch or decend");
REGISTER_CVAR_KEY_BIND_KEY_AND_MOD(
	cv_bind_fullscreen, SDLK_RETURN, KMOD_ALT, false, "toggle fullscreen");

REGISTER_CVAR_KEY_BIND_KEY(cv_bind_open_console, SDLK_F1, false, "open console overlay");
REGISTER_CVAR_KEY_BIND_KEY(cv_bind_open_options, SDLK_SLASH, false, "open option menu");
REGISTER_CVAR_KEY_BIND_KEY(
	cv_bind_reset_window_size,
	SDLK_F5,
	false,
	"restore the window size to cv_startup_screen_width/height");
REGISTER_CVAR_KEY_BIND_KEY(cv_bind_toggle_text, SDLK_F2, false, "hide the help and fps text");
REGISTER_CVAR_KEY_BIND_KEY(
	cv_bind_soft_reboot,
	SDLK_F6,
	false,
	"if you need to restart to change font settings or something, you can use this.");

// TODO: emscripten code should really be in "app".
// maybe I should split the state between app / base / "demo"
// -app = owns the SDL_Init, SDL window, freetype state?, openal state?,
//      maybe split the console between the UI and data, and store log data in app,
//      because on soft reboot the console data disappears.
// -base = owns the opengl context, font atlas, console, prompt, options menu, fps text,
//      deals with emscripten specific code
//      controls the SDL_PollEvent and feeds it into "demo".
//      the difference between app and base is that on a "soft reboot", only the base gets
//      destroyed.
// -"demo" = the main content of the application.
// ALSO split the cvar type for STARTUP into SOFT_STARTUP and HARD_STARTUP
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>

#if 0
static EM_BOOL on_pointerlockchange(int eventType, const EmscriptenPointerlockChangeEvent *e, void *userData)
{
    slogf("%s: isActive = %d, nodeName: %s\n", __func__, e->isActive, e->nodeName);
    if(e->isActive)
    {
        //hide mouse
    }
    else
    {
        //show mouse
    }
    return 1;
}
#endif
static EM_BOOL on_mouse_callback(int eventType, const EmscriptenMouseEvent* e, void* userData)
{
#if 0
	 slogf("%s, screen: (%ld,%ld), client: (%ld,%ld),%s%s%s%s button: %hu, buttons: %hu, movement: (%ld,%ld), target: (%ld, %ld)\n", emscripten_event_type_to_string(eventType), e->screenX,
	 e->screenY, e->clientX, e->clientY, e->ctrlKey ? " CTRL" : "", e->shiftKey ? " SHIFT" : "",
	 e->altKey ? " ALT" : "", e->metaKey ? " META" : "", e->button, e->buttons, e->movementX,
	 e->movementY, e->targetX, e->targetY);
     slogf("canvas: (%ld, %ld)\n",  e->canvasX, e->canvasY);
#endif
	if(eventType == EMSCRIPTEN_EVENT_MOUSEUP)
	{
		ASSERT(userData != NULL);
		// I probably could make the timestamp work by getting the delta between SDL's timer and
		// html5's
		SDL_Event ev;
		memset(&ev, 0, sizeof(ev));
		ev.type = SDL_MOUSEBUTTONUP;
		ev.button.button =
			(e->button == 0 ? SDL_BUTTON_LEFT : (e->button == 2 ? SDL_BUTTON_RIGHT : 0));
		ev.button.state = SDL_RELEASED;
#if 0
		// this is based on SDL's code
		/*
		unfortunately this can't handle the sitatuation where I fullscreen with the inspector open
		in firefox. because the cavnas will maintain it's aspect ratio which means 0,0 points in the
		black bar... to fix it I would do something like this: function  getMousePos(canvas, evt) {
		  var rect = canvas.getBoundingClientRect(), // abs. size of element
			scaleX = canvas.width / rect.width,    // relationship bitmap vs. element for x
			scaleY = canvas.height / rect.height;  // relationship bitmap vs. element for y

		  return {
			x: (evt.clientX - rect.left) * scaleX,   // scale mouse coordinates after they have
			y: (evt.clientY - rect.top) * scaleY     // been adjusted to be relative to element
		  }

		I probably should handle ALL the mouse events anyways to prevent SDL2 from making a breaking
		change.
		}

		*/
		double xscale;
		double yscale;
		double client_w = 1;
		double client_h = 1;
		int window_w = 1;
		int window_h = 1;
		EMSCRIPTEN_RESULT em_ret =
			emscripten_get_canvas_element_size("#canvas", &window_w, &window_h);
		if(em_ret != EMSCRIPTEN_RESULT_SUCCESS)
		{
			slogf(
				"%s returned %s.\n",
				"emscripten_get_element_css_size",
				emscripten_result_to_string(em_ret));
		}
		em_ret = emscripten_get_element_css_size("#canvas", &client_w, &client_h);
		if(em_ret != EMSCRIPTEN_RESULT_SUCCESS)
		{
			slogf(
				"%s returned %s.\n",
				"emscripten_get_element_css_size",
				emscripten_result_to_string(em_ret));
		}
		xscale = window_w / client_w;
		yscale = window_h / client_h;
		// slogf("cw: %f, ch: %f, w: %d, h: %d\n", client_w, client_h, window_w, window_h);
		ev.button.x = e->targetX * xscale;
		ev.button.y = e->targetY * yscale;
#endif

		SDL_GetMouseState(&ev.button.x, &ev.button.y);

		// TODO: I really should handle this error, but I need some sort of way to signal
		// to the main thread that an error propogated...
		// should put bool exit inside of g_app I guess.
		static_cast<demo_state*>(userData)->input(ev);
		return 1;
	}
	slogf("%s: unhandled type\n", __func__);
	return 0;
}

// exported functions called by JS
// CTRL+C, CTRL+V, and CTRL+X will manually trigger these through "keydown" events
// all return 1 if the event was eaten, 0 if nothing (MAYBE -1 for an error, but for what purpose?)

// I don't really have a choice
static demo_state* em_global_demo = NULL;
extern "C" {
extern int32_t paste_clipboard(const char* text)
{
	// slogf("pasted: %s\n", text);
	{
		if(SDL_SetClipboardText(text) != 0)
		{
			slogf("info: Failed to set clipboard! SDL Error: %s\n", SDL_GetError());
		}
	}
	if(em_global_demo == NULL)
	{
		return 0;
	}
	SDL_Event fake_event;
	fake_event.type = SDL_KEYDOWN;
	fake_event.key.state = SDL_PRESSED;
	fake_event.key.repeat = 0;
	fake_event.key.keysym.sym = SDLK_v;
	fake_event.key.keysym.mod = KMOD_CTRL;
	fake_event.key.keysym.scancode = SDL_SCANCODE_UNKNOWN;
	em_global_demo->input(fake_event);
	if(fake_event.type != SDL_KEYDOWN)
	{
		// eaten
		return 1;
	}
	return 0;
}
// you must call free() on the returned pointer.
extern char* copy_clipboard()
{
	if(em_global_demo == NULL)
	{
		return NULL;
	}
	SDL_Event fake_event;
	fake_event.type = SDL_KEYDOWN;
	fake_event.key.state = SDL_PRESSED;
	fake_event.key.repeat = 0;
	fake_event.key.keysym.sym = SDLK_c;
	fake_event.key.keysym.mod = KMOD_CTRL;
	fake_event.key.keysym.scancode = SDL_SCANCODE_UNKNOWN;
	em_global_demo->input(fake_event);
	if(fake_event.type != SDL_KEYDOWN)
	{
#ifndef HAVE_MALLOC
#error SDL sets HAVE_MALLOC if it uses malloc() and free() for SDL_malloc and SDL_free, I depend on this behavior.
#endif
		// NOTE: I assume SDL_Free is just an alias for free() in javascript.
		// one of the solutions is to just write my own SDL_Set/GetClipboard that uses malloc.
		// since SDL is doing absolutely nothing here.
		// and ATM this assumes that SDL's clipboard uses a fake clipboard (it can't access the real
		// clipboard) if it gets fixed, I could just remove this whole hack (but it's very unlikely
		// due to security contexts)
		char* text = SDL_GetClipboardText();
		if(text == NULL)
		{
			slogf("info: Failed to get clipboard! SDL Error: %s\n", SDL_GetError());
			return NULL;
		}
		// slogf("copied:`%s`\n", text);
		// eaten
		return text;
	}
	return NULL;
}
// same as copy but it's cut
extern char* cut_clipboard()
{
	if(em_global_demo == NULL)
	{
		return NULL;
	}
	SDL_Event fake_event;
	fake_event.type = SDL_KEYDOWN;
	fake_event.key.state = SDL_PRESSED;
	fake_event.key.repeat = 0;
	fake_event.key.keysym.sym = SDLK_x;
	fake_event.key.keysym.mod = KMOD_CTRL;
	fake_event.key.keysym.scancode = SDL_SCANCODE_UNKNOWN;
	em_global_demo->input(fake_event);
	if(fake_event.type != SDL_KEYDOWN)
	{
		char* text = SDL_GetClipboardText();
		if(text == NULL)
		{
			slogf("info: Failed to get clipboard! SDL Error: %s\n", SDL_GetError());
			return NULL;
		}
		// eaten
		return text;
	}
	return NULL;
}
}
#endif

struct gl_point_vertex
{
	GLfloat coords[3];
	GLubyte color[4];
	GLint inst_id;
};

// creates a pattern like this with divisions = 1
//|\|\|
//|\|\|
// coordinates 0 to 1 origin topleft
// vbo must be have (divisions+2)^2 * 3 elements
// and ibo must have (divisions+1)^2 * 6 elements
static void create_grid(unsigned int divisions, glm::vec3* vbo_out, GLushort* ibo_out)
{
	unsigned int points = divisions + 2;
	unsigned int quads = divisions + 1;

	float fpoints = static_cast<float>(points);

	for(unsigned int i = 0, y = 0; y < points; ++y)
	{
		for(unsigned int x = 0; x < points; ++x)
		{
			float fx = static_cast<float>(x) / (fpoints - 1.f);
			float fy = static_cast<float>(y) / (fpoints - 1.f);
			float fz = fminf(sin(glm::radians(fx * 180.f)), sin(glm::radians(fy * 180.f))) * 0.05f;
			vbo_out[i++] = glm::vec3(fx, fy, fz);
		}
	}
	for(unsigned int i = 0, y = 0; y < quads; ++y)
	{
		for(unsigned int x = 0; x < quads; ++x)
		{
			ibo_out[i++] = y * points + x;
			ibo_out[i++] = y * points + x + 1;
			ibo_out[i++] = (y + 1) * points + x + 1;
			ibo_out[i++] = (y + 1) * points + x + 1;
			ibo_out[i++] = (y + 1) * points + x;
			ibo_out[i++] = y * points + x;
		}
	}
}

// vbo must be have (divisions+2)^2 * 6 elements
// and ibo must have (divisions+1)^2 * 6 * 6 elements
static void create_grid_cube(unsigned int divisions, glm::vec3* vbo_out, GLushort* ibo_out)
{
	// note that this does not combine the vertexes of the edges due to laziness.
	// it's possible to just generate a ibo using just the vbo with a brute force algorithmn.
	unsigned int vbo_points = divisions + 2;
	unsigned int ibo_quads = divisions + 1;

	unsigned int side_vbo_points_size = vbo_points * vbo_points;
	unsigned int side_ibo_quads_size = ibo_quads * ibo_quads * 6;

	create_grid(divisions, vbo_out, ibo_out);

	// vbo, note the first side is already in vbo_out
	glm::mat4 orientations[6] = {
		glm::mat4(1.f),
		glm::mat4(1.f),
		glm::mat4(1.f),
		glm::mat4(1.f),
		glm::mat4(1.f),
		glm::mat4(1.f)};
	// the first face will stay in the center since all other faces reference it.
	size_t face_cur = 0;
	orientations[face_cur] = glm::translate(orientations[face_cur], glm::vec3(-0.5f, -0.5f, 0.f));
	// back
	++face_cur;
	orientations[face_cur] =
		glm::rotate(orientations[face_cur], glm::radians(180.f), glm::vec3(0.f, 1.f, 0.f));
	orientations[face_cur] = glm::translate(orientations[face_cur], glm::vec3(0.f, 0.f, 0.5f));
	// left
	++face_cur;
	orientations[face_cur] =
		glm::rotate(orientations[face_cur], glm::radians(-90.f), glm::vec3(0.f, 1.f, 0.f));
	orientations[face_cur] = glm::translate(orientations[face_cur], glm::vec3(0.f, 0.f, 0.5f));
	// right
	++face_cur;
	orientations[face_cur] =
		glm::rotate(orientations[face_cur], glm::radians(90.f), glm::vec3(0.f, 1.f, 0.f));
	orientations[face_cur] = glm::translate(orientations[face_cur], glm::vec3(0.f, 0.f, 0.5f));
	// top
	++face_cur;
	orientations[face_cur] =
		glm::rotate(orientations[face_cur], glm::radians(-90.f), glm::vec3(1.f, 0.f, 0.f));
	orientations[face_cur] = glm::translate(orientations[face_cur], glm::vec3(0.f, 0.f, 0.5f));
	// bottom
	++face_cur;
	orientations[face_cur] =
		glm::rotate(orientations[face_cur], glm::radians(90.f), glm::vec3(1.f, 0.f, 0.f));
	orientations[face_cur] = glm::translate(orientations[face_cur], glm::vec3(0.f, 0.f, 0.5f));

	// orientations[1] = glm::rotate(orientations[1], glm::radians(90.f), glm::vec3(0.f,1.f,0.f));
	// orientations[1] = glm::translate(orientations[1], glm::vec3(0.f,0.f,1.f));

	// orientations[4] = glm::translate(orientations[4], glm::vec3(0.5f,0.5f,0.5f));

	size_t vbo_cur = 0;
	for(auto& orientation : orientations)
	{
		for(size_t i = 0; i < side_vbo_points_size; ++i)
		{
			vbo_out[vbo_cur++] = orientation * glm::vec4(vbo_out[i], 1.f);
		}
	}

	// move the first face to not be in the center
	orientations[0] = glm::translate(glm::mat4(1.f), glm::vec3(0.f, 0.f, 0.5f));
	for(size_t i = 0; i < side_vbo_points_size; ++i)
	{
		vbo_out[i] = orientations[0] * glm::vec4(vbo_out[i], 1.f);
	}

	// ibo, note the first side is already in vbo_out
	for(size_t j = 1; j < 6; ++j)
	{
		for(size_t i = 0; i < side_ibo_quads_size; ++i)
		{
			ibo_out[j * side_ibo_quads_size + i] = ibo_out[i] + j * side_vbo_points_size;
		}
	}
}

bool demo_state::init()
{
	ctx.glEnable(GL_CULL_FACE);
	ctx.glCullFace(GL_BACK);
	ctx.glFrontFace(GL_CCW);

	ctx.glEnable(GL_DEPTH_TEST);
	ctx.glDepthMask(GL_TRUE);

	// I forgot that glDrawElements doesn't work well with GL_LINES
	// because every vertex is processed once, so lines are missing.
	// ctx.glLineWidth(4);

	// premultiplied
	ctx.glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	// not premultiplied
	// ctx.glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// all my shaders only use 1 texture.
	ctx.glActiveTexture(GL_TEXTURE0);

	if(cv_string_alpha_test.data == -1)
	{
		if(!mono_shader.create())
		{
			return false;
		}
	}
	else
	{
		if(!mono_shader.create_alpha_test())
		{
			return false;
		}
		// gotta set the alpha test value.
		ctx.glUseProgram(mono_shader.gl_program_id);
		ctx.glUniform1f(
			mono_shader.gl_uniforms.u_alpha_test, static_cast<float>(cv_string_alpha_test.data));
		ctx.glUseProgram(0);
		if(GL_CHECK(__func__) != GL_NO_ERROR)
		{
			return false;
		}
	}

	if(!point_shader.create())
	{
		return false;
	}

	if(!init_gl_point_sprite())
	{
		return false;
	}

	if(!init_gl_font())
	{
		return false;
	}

#ifdef __EMSCRIPTEN__
	em_global_demo = this;

#if 0    
EMSCRIPTEN_RESULT em_ret = emscripten_set_pointerlockchange_callback("#canvas", 0, 0, on_pointerlockchange);
    if (em_ret != EMSCRIPTEN_RESULT_SUCCESS)
    {
        slogf("%s returned %s.\n", "emscripten_set_pointerlockchange_callback", emscripten_result_to_string(em_ret));
    }
#endif

	EMSCRIPTEN_RESULT em_ret =
		emscripten_set_mouseup_callback("#canvas", this, 1, on_mouse_callback);
	if(em_ret != EMSCRIPTEN_RESULT_SUCCESS)
	{
		slogf(
			"%s returned %s.\n",
			"emscripten_set_mousedown_callback",
			emscripten_result_to_string(em_ret));
	}

#endif

	timer_last = timer_now();
	return true;
}

bool demo_state::init_gl_point_sprite()
{
	SDL_Surface* test_image = SDL_LoadBMP("garfed.bmp");
	ASSERT(test_image != NULL);

	point_buffer_size = test_image->w * test_image->h;
	std::unique_ptr<gl_point_vertex[]> point_buffer(new gl_point_vertex[point_buffer_size]);

	ASSERT(test_image->format->BytesPerPixel == 3);

	ASSERT(test_image->format->format == SDL_PIXELFORMAT_BGR24);
	ASSERT(test_image->w * 3 == test_image->pitch);
	GLubyte* cur = static_cast<GLubyte*>(test_image->pixels);
	for(int y = 0; y < test_image->h; ++y)
	{
		for(int x = 0; x < test_image->w; ++x)
		{
			gl_point_vertex& point = point_buffer[y * test_image->w + x];
			point.coords[0] = static_cast<GLfloat>(x);
			point.coords[1] = static_cast<GLfloat>(y);
			point.coords[2] = 0;
			// note bgr
			point.color[2] = *cur++;
			point.color[1] = *cur++;
			point.color[0] = *cur++;
			point.color[3] = 255;
			point.inst_id = 0;
		}
	}
	SDL_FreeSurface(test_image);

	int divisions = 1;

	int points = divisions + 2;
	int vbo_size = points * points * 6;

	if(vbo_size > std::numeric_limits<GLushort>::max())
	{
		serrf("%s has too many vertexies: %d", __func__, vbo_size);
		return false;
	}

	std::unique_ptr<glm::vec3[]> vbo_buffer(new glm::vec3[vbo_size]);

	int quads = divisions + 1;
	ibo_buffer_size = quads * quads * 6 * 6;
	ibo_buffer.reset(new GLushort[ibo_buffer_size]);

	create_grid_cube(divisions, vbo_buffer.get(), ibo_buffer.get());

	for(int i = 0; i < vbo_size; ++i)
	{
		// I really should use math to figure this out but oh well.
		vbo_buffer[i] *= 1.424f;
	}

	// create the buffer for the shader
	GLuint gl_buffers[3];
	ctx.glGenBuffers(std::size(gl_buffers), gl_buffers);
	if(gl_buffers[0] == 0)
	{
		serrf("%s error: glGenBuffers failed\n", __func__);
		return false;
	}
	gl_vert_vbo_id = gl_buffers[0];
	ctx.glBindBuffer(GL_ARRAY_BUFFER, gl_vert_vbo_id);
	ctx.glBufferData(
		GL_ARRAY_BUFFER,
		// NOLINTNEXTLINE(bugprone-narrowing-conversions)
		vbo_size * sizeof(decltype(vbo_buffer)::element_type),
		vbo_buffer.get(),
		GL_STATIC_DRAW);

	gl_vert_ibo_id = gl_buffers[1];
	ctx.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl_vert_ibo_id);
	ctx.glBufferData(
		GL_ELEMENT_ARRAY_BUFFER,
		// NOLINTNEXTLINE(bugprone-narrowing-conversions)
		ibo_buffer_size * sizeof(decltype(ibo_buffer)::element_type),
		ibo_buffer.get(),
		GL_STATIC_DRAW);

	gl_point_vbo_id = gl_buffers[2];
	ctx.glBindBuffer(GL_ARRAY_BUFFER, gl_point_vbo_id);
	ctx.glBufferData(
		GL_ARRAY_BUFFER,
		// NOLINTNEXTLINE(bugprone-narrowing-conversions)
		point_buffer_size * sizeof(gl_point_vertex),
		point_buffer.get(),
		GL_STATIC_DRAW);

	// basic VAO
	ctx.glGenVertexArrays(1, &gl_vao_id);
	if(gl_vao_id == 0)
	{
		serrf("%s error: glGenVertexArrays failed\n", __func__);
		return false;
	}
	ctx.glBindVertexArray(gl_vao_id);

	ctx.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl_vert_ibo_id);

	// position vertex setup
	ctx.glBindBuffer(GL_ARRAY_BUFFER, gl_vert_vbo_id);
	if(point_shader.gl_attributes.a_vert_pos != -1)
	{
		ctx.glEnableVertexAttribArray(point_shader.gl_attributes.a_vert_pos);
		ctx.glVertexAttribPointer(
			point_shader.gl_attributes.a_vert_pos, // attribute
			3, // size
			GL_FLOAT, // type
			GL_FALSE, // normalized?
			0, // stride
			0 // array buffer offset
		);
	}

	ctx.glBindBuffer(GL_ARRAY_BUFFER, gl_point_vbo_id);
	if(point_shader.gl_attributes.a_point_pos != -1)
	{
		ctx.glEnableVertexAttribArray(point_shader.gl_attributes.a_point_pos);
		ctx.glVertexAttribPointer(
			point_shader.gl_attributes.a_point_pos, // attribute
			3, // size
			GL_FLOAT, // type
			GL_FALSE, // normalized?
			sizeof(gl_point_vertex), // stride
			(void*)offsetof(gl_point_vertex, coords) // NOLINT
		);
		ctx.glVertexAttribDivisor(point_shader.gl_attributes.a_point_pos, 1);
	}
	if(point_shader.gl_attributes.a_point_color != -1)
	{
		ctx.glEnableVertexAttribArray(point_shader.gl_attributes.a_point_color);
		ctx.glVertexAttribPointer(
			point_shader.gl_attributes.a_point_color, // attribute
			4, // size
			GL_UNSIGNED_BYTE, // type
			GL_TRUE, // normalized?
			sizeof(gl_point_vertex), // stride
			(void*)offsetof(gl_point_vertex, color) // NOLINT
		);
		ctx.glVertexAttribDivisor(point_shader.gl_attributes.a_point_color, 1);
	}

	// finish
	ctx.glBindVertexArray(0);

	ctx.glBindBuffer(GL_ARRAY_BUFFER, 0);
	ctx.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	// create the texture for the instance id matrix
	ctx.glGenTextures(1, &gl_inst_table_tex_id);
	if(gl_inst_table_tex_id == 0)
	{
		serrf("%s error: glGenTextures failed\n", __func__);
		return false;
	}

	// ctx.glActiveTexture(GL_TEXTURE0);

	ctx.glBindTexture(GL_TEXTURE_2D, gl_inst_table_tex_id);
	ctx.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 0, 1, 0, GL_RGBA, GL_FLOAT, NULL);
	// Set texture parameters
	ctx.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	ctx.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	ctx.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	ctx.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	// set uniform globals that aren't set every frame
	ctx.glUseProgram(point_shader.gl_program_id);
	ctx.glUniform1i(point_shader.gl_uniforms.u_inst_table, 0);
	ctx.glBindTexture(GL_TEXTURE_2D, gl_inst_table_tex_id);
	ctx.glUseProgram(0);

	return GL_CHECK(__func__) == GL_NO_ERROR;
}

bool demo_state::destroy_gl_point_sprite()
{
	SAFE_GL_DELETE_VBO(gl_vert_vbo_id);
	SAFE_GL_DELETE_VBO(gl_vert_ibo_id);
	SAFE_GL_DELETE_VBO(gl_point_vbo_id);
	SAFE_GL_DELETE_VAO(gl_vao_id);
	SAFE_GL_DELETE_TEXTURE(gl_inst_table_tex_id);

	return GL_CHECK(__func__) == GL_NO_ERROR;
}

bool demo_state::init_gl_font()
{
	if(!font_manager.create())
	{
		return false;
	}

	{
#if 0
		TIMER_U start;
		TIMER_U end;
		start = timer_now();
#endif
		Unique_RWops hex_file = Unique_RWops_OpenFS(cv_hexfile_path.data, "rb");
		// Unique_RWops hex_file = Unique_RWops_OpenFS("unifont_upper-14.0.02.hex", "rb");
		if(!hex_file)
		{
			return false;
		}

		if(!font_manager.hex_font.init(std::move(hex_file), &font_manager.atlas))
		{
			return false;
		}

#if 0
		// pretty fast for initializing every glyph in unicode.
		// 140ms on asan 24ms on reldeb.
		end = timer_now();
		slogf("time: %f\n", timer_delta_ms(start, end));

        // I used to load all the hex glyphs into memory
        // but it uses megabytes of memory...
        // now I lazy load.
		//size_t hex_state_size =
		//	font_manager.hex_font.hex_block_chunks.size() *
		//	sizeof(decltype(font_manager.hex_font.hex_block_chunks)::value_type);
		//slogf("hex memory used: %zu kb\n", hex_state_size / 1024);

#endif
	}

#if 0
	TIMER_U start;
	TIMER_U end;
	start = timer_now();
#endif

	font_style_interface* current_font = NULL;

	// Unique_RWops test_font =
	// Unique_RWops_OpenFS("/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc", "rb");
	if(cv_string_font.data == "unifont")
	{
		unifont_style.init(&font_manager.hex_font, static_cast<float>(cv_string_pt.data));
		current_font = &unifont_style;
	}
	else
	{
		Unique_RWops test_font = Unique_RWops_OpenFS(cv_string_font.data, "rb");
		if(!test_font)
		{
			return false;
		}

		if(!font_rasterizer.create(font_manager.FTLibrary, std::move(test_font)))
		{
			return false;
		}

		font_settings.point_size = static_cast<float>(cv_string_pt.data);

		if(cv_string_mono.data == 1)
		{
			font_settings.render_mode = FT_RENDER_MODE_MONO;
			font_settings.load_flags = FT_LOAD_TARGET_MONO;
		}

		// FT_LOAD_RENDER can give the same bitmap outline as force_bitmap
		// but force_bitmap will choose the closest raster bitmap possible,
		// so if a font supports both vector and raster data, and you want
		// the text to be vectorized with a bitmap outline, use FT_LOAD_RENDER.
		// but this also breaks bold and italics style.
		// font_settings.load_flags = FT_LOAD_RENDER;
		// font_settings.load_flags = FT_LOAD_TARGET_MONO | FT_LOAD_RENDER;

		font_settings.bold_x = 1;
		font_settings.bold_y = 1;
		font_settings.italics_skew = 0.5;
		font_settings.outline_size = static_cast<float>(cv_string_outline.data);

		if(cv_string_force_bitmap.data == 1)
		{
			font_settings.force_bitmap = true;
		}

		if(!font_rasterizer.set_face_settings(&font_settings))
		{
			return false;
		}

		font_style.init(&font_manager, &font_rasterizer);
		current_font = &font_style;
	}

#if 0
	// it's pretty slow
	// 0.2ms on asan, 0.15 on reldeb
	// might need preloading + only load preloaded glyphs
	end = timer_now();
	slogf("time: %f\n", timer_delta_ms(start, end));
#endif

	// create the buffer for the shader
	ctx.glGenBuffers(1, &gl_font_interleave_vbo);
	if(gl_font_interleave_vbo == 0)
	{
		serrf("%s error: glGenBuffers failed\n", __func__);
		return false;
	}

	// basic VAO
	ctx.glGenVertexArrays(1, &gl_font_vao_id);
	if(gl_font_vao_id == 0)
	{
		serrf("%s error: glGenVertexArrays failed\n", __func__);
		return false;
	}

	// vertex setup
	ctx.glBindVertexArray(gl_font_vao_id);
	ctx.glBindBuffer(GL_ARRAY_BUFFER, gl_font_interleave_vbo);
	gl_create_interleaved_mono_vertex_vao(mono_shader);

	size_t max_quads = 10000;
	font_batcher_buffer =
		std::make_unique<gl_mono_vertex[]>(max_quads * mono_2d_batcher::QUAD_VERTS);
	font_batcher.init(font_batcher_buffer.get(), max_quads);

	font_painter.init(&font_batcher, current_font);
	// font_painter.set_scale(2);

	if(!console_menu.init(current_font, &font_batcher, mono_shader))
	{
		return false;
	}

	if(!option_menu.init(current_font, &font_batcher, mono_shader))
	{
		return false;
	}

	// set uniform globals that aren't set every frame
	ctx.glUseProgram(mono_shader.gl_program_id);
	ctx.glUniform1i(mono_shader.gl_uniforms.u_tex, 0);

	glm::mat4 mvp = glm::ortho<float>(
		0,
		cv_screen_width.data, // NOLINT(bugprone-narrowing-conversions)
		cv_screen_height.data, // NOLINT(bugprone-narrowing-conversions)
		0,
		-1000,
		1000);
	ctx.glUniformMatrix4fv(mono_shader.gl_uniforms.u_mvp, 1, GL_FALSE, glm::value_ptr(mvp));
	ctx.glUseProgram(0);

	return GL_CHECK(__func__) == GL_NO_ERROR;
}

bool demo_state::destroy_gl_font()
{
	bool success = true;

	success = font_style.destroy() && success;
	success = font_rasterizer.destroy() && success;
	success = font_manager.destroy() && success;

	SAFE_GL_DELETE_VBO(gl_font_interleave_vbo);
	SAFE_GL_DELETE_VAO(gl_font_vao_id);

	return GL_CHECK(__func__) == GL_NO_ERROR && success;
}

bool demo_state::destroy()
{
	bool success = true;

	success = option_menu.destroy() && success;
	success = console_menu.destroy() && success;

	success = destroy_gl_font() && success;
	success = destroy_gl_point_sprite() && success;

	success = point_shader.destroy() && success;
	success = mono_shader.destroy() && success;

#ifdef __EMSCRIPTEN__

	EMSCRIPTEN_RESULT em_ret = emscripten_set_mouseup_callback("#canvas", NULL, 0, NULL);
	if(em_ret != EMSCRIPTEN_RESULT_SUCCESS)
	{
		slogf(
			"%s returned %s.\n",
			"emscripten_set_mouseup_callback",
			emscripten_result_to_string(em_ret));
	}

#endif

	return success;
}

void demo_state::unfocus_demo()
{
	for(auto& val : keys_down)
	{
		val = false;
	}
#ifdef __EMSCRIPTEN__
	EMSCRIPTEN_RESULT em_ret = emscripten_exit_pointerlock();
	if(em_ret != EMSCRIPTEN_RESULT_SUCCESS)
	{
		slogf(
			"%s returned %s.\n",
			"emscripten_exit_pointerlock",
			emscripten_result_to_string(em_ret));
	}
#else
	if(SDL_SetRelativeMouseMode(SDL_FALSE) < 0)
	{
		slogf("info: SDL_SetRelativeMouseMode failed: %s\n", SDL_GetError());
	}
#endif
}
bool demo_state::unfocus_all()
{
	SDL_Event e;
	set_event_unfocus(e);
	return input(e);
}

bool demo_state::input(SDL_Event& e)
{
	switch(e.type)
	{
	case SDL_WINDOWEVENT:
		switch(e.window.event)
		{
			// this unfocuses the pointerlock/relative mouse, and held keys.
		case SDL_WINDOWEVENT_FOCUS_LOST:
		case SDL_WINDOWEVENT_HIDDEN:
			unfocus_demo();
			break;
			// there is no "hover focus"
			// case SDL_WINDOWEVENT_LEAVE:
		}
	}

	// TIMER_U t1 = timer_now();
	// bool input_eaten = false;
	// is the mouse currently locked?
#ifdef __EMSCRIPTEN__
	EmscriptenPointerlockChangeEvent plce;
	EMSCRIPTEN_RESULT em_ret = emscripten_get_pointerlock_status(&plce);
	if(em_ret != EMSCRIPTEN_RESULT_SUCCESS)
	{
		slogf(
			"%s returned %s.\n",
			"emscripten_get_pointerlock_status",
			emscripten_result_to_string(em_ret));
	}
	else if(plce.isActive)
#else
	if(SDL_GetRelativeMouseMode() == SDL_TRUE)
#endif
	{
		if(e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP)
		{
			set_event_unfocus(e);
		}
		if(e.type == SDL_MOUSEMOTION)
		{
			// int x;
			// int y;
			// SDL_GetRelativeMouseState(&x, &y);
			float inverse = (cv_mouse_invert.data == 1) ? -1 : 1;
			camera_yaw += static_cast<float>(e.motion.xrel * cv_mouse_sensitivity.data) * inverse;
			camera_pitch -= static_cast<float>(e.motion.yrel * cv_mouse_sensitivity.data) * inverse;
			camera_pitch = fmaxf(camera_pitch, -89.f);
			camera_pitch = fminf(camera_pitch, 89.f);

			glm::vec3 direction;
			direction.x = cos(glm::radians(camera_yaw)) * cos(glm::radians(camera_pitch));
			direction.y = sin(glm::radians(camera_pitch));
			direction.z = sin(glm::radians(camera_yaw)) * cos(glm::radians(camera_pitch));
			camera_direction = glm::normalize(direction);
			set_mouse_event_clipped(e);
		}
		if(e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)
		{
			unfocus_demo();
			set_event_unfocus(e);
		}
	}

	if(show_console)
	{
		switch(console_menu.input(e))
		{
			// TODO: if the console opened automatically from an error, I would want to display a
			// close button.
		case CONSOLE_RESULT::CONTINUE: break;
		case CONSOLE_RESULT::ERROR: return false;
		}
	}

	if(show_options)
	{
		switch(option_menu.input(e))
		{
		case OPTIONS_MENU_RESULT::CONTINUE: break;
		case OPTIONS_MENU_RESULT::CLOSE:
			show_options = false;
			// eat
			return true;
		case OPTIONS_MENU_RESULT::ERROR: return false;
		}
	}

	if(cv_bind_open_console.compare_sdl_event(e, KEYBIND_BUTTON_DOWN) != KEYBIND_NULL)
	{
		SDL_Event fake_event;
		// unfocus ALL
		set_event_unfocus(fake_event);
		// TODO: I could probably make this be triggered by adding a new input enum return?
		if(!input(fake_event))
		{
			return false;
		}
		show_console = !show_console;
		if(show_console)
		{
			// focus for the text input.
			// this will call set_event_unfocus(e);
			console_menu.prompt_cmd.focus(e);

			set_event_resize(fake_event);
		}
		else
		{
			set_event_unfocus(e);
			set_event_hidden(fake_event);
		}
		if(console_menu.input(fake_event) == CONSOLE_RESULT::ERROR)
		{
			return false;
		}
	}

	if(cv_bind_open_options.compare_sdl_event(e, KEYBIND_BUTTON_DOWN) != KEYBIND_NULL)
	{
		SDL_Event fake_event;
		// unfocus ALL
		set_event_unfocus(fake_event);
		if(!input(fake_event))
		{
			return false;
		}
		// this isn't a toggle. only open, press escape or click close.
		show_options = !show_options;
		// force resize.
		if(show_options)
		{
			if(!option_menu.refresh())
			{
				return false;
			}
			set_event_resize(fake_event);
		}
		else
		{
			set_event_hidden(fake_event);
		}
		if(option_menu.input(fake_event) == OPTIONS_MENU_RESULT::ERROR)
		{
			return false;
		}
		// eat
		set_event_unfocus(e);
	}

	// start demo input

	switch(e.type)
	{
	case SDL_KEYDOWN:
		// NOTE: probably should check this if I added in other keys
		// I wouldn't want to trigger if I was using a text prompt,
		// because key events will still be triggered even if textinput is active.
		// if(SDL_IsTextInputActive() == SDL_TRUE)

		switch(e.key.keysym.sym)
		{
		case SDLK_ESCAPE: unfocus_demo(); break;
		}
		break;
	case SDL_MOUSEBUTTONUP:
		// TODO: should check if I button down wasn't eaten before I button up.
		if(e.button.button == SDL_BUTTON_LEFT || e.button.button == SDL_BUTTON_RIGHT)
		{
			// eat
			set_event_unfocus(e);
			// unfocus ALL
			if(!input(e))
			{
				return false;
			}
#ifdef __EMSCRIPTEN__
			// this ONLY works when called inside of a mouse button event that is inside a html5
			// handler. the deferred option means if false (0), this will give an error and nothing
			// will happen if this was not called in the correct handler. if true(1), it will
			// attempt the request when the next time the handler is activated (the mouse button
			// handler), which is janky.
			em_ret = emscripten_request_pointerlock("#canvas", 1);
			if(em_ret != EMSCRIPTEN_RESULT_SUCCESS)
			{
				slogf(
					"%s returned %s.\n",
					"emscripten_request_pointerlock",
					emscripten_result_to_string(em_ret));
			}
#else
			if(SDL_SetRelativeMouseMode(SDL_TRUE) < 0)
			{
				slogf("info: SDL_SetRelativeMouseMode failed: %s\n", SDL_GetError());
			}
#endif
		}
		break;
	}

	// release motion if certain window events are triggerred
	if(e.type == SDL_WINDOWEVENT)
	{
		switch(e.window.event)
		{
		case SDL_WINDOWEVENT_FOCUS_LOST:
		case SDL_WINDOWEVENT_HIDDEN:
			for(bool& down : keys_down)
			{
				down = false;
			}
			break;
		}
	}

	// movement. note that you can't bind these to mouse buttons because they get eaten.
	keybind_compare_type ret;
	ret = cv_bind_move_forward.compare_sdl_event(e, KEYBIND_BUTTON_DOWN | KEYBIND_BUTTON_UP);
	if(ret != KEYBIND_NULL)
	{
		keys_down[MOVE_FORWARD] = (ret & KEYBIND_BUTTON_DOWN) != 0;
	}
	ret = cv_bind_move_backward.compare_sdl_event(e, KEYBIND_BUTTON_DOWN | KEYBIND_BUTTON_UP);
	if(ret != KEYBIND_NULL)
	{
		keys_down[MOVE_BACKWARD] = (ret & KEYBIND_BUTTON_DOWN) != 0;
	}
	ret = cv_bind_move_left.compare_sdl_event(e, KEYBIND_BUTTON_DOWN | KEYBIND_BUTTON_UP);
	if(ret != KEYBIND_NULL)
	{
		keys_down[MOVE_LEFT] = (ret & KEYBIND_BUTTON_DOWN) != 0;
	}
	ret = cv_bind_move_right.compare_sdl_event(e, KEYBIND_BUTTON_DOWN | KEYBIND_BUTTON_UP);
	if(ret != KEYBIND_NULL)
	{
		keys_down[MOVE_RIGHT] = (ret & KEYBIND_BUTTON_DOWN) != 0;
	}
	ret = cv_bind_move_jump.compare_sdl_event(e, KEYBIND_BUTTON_DOWN | KEYBIND_BUTTON_UP);
	if(ret != KEYBIND_NULL)
	{
		keys_down[MOVE_JUMP] = (ret & KEYBIND_BUTTON_DOWN) != 0;
	}
	ret = cv_bind_move_crouch.compare_sdl_event(e, KEYBIND_BUTTON_DOWN | KEYBIND_BUTTON_UP);
	if(ret != KEYBIND_NULL)
	{
		keys_down[MOVE_CROUCH] = (ret & KEYBIND_BUTTON_DOWN) != 0;
	}

	return true;
}
bool demo_state::update(double delta_sec)
{
	float color_delta = static_cast<float>(delta_sec);

	// this will not actually draw, this will just modify the atlas and buffer data.
	if(show_console)
	{
		if(!console_menu.update(delta_sec))
		{
			return false;
		}
	}

	if(show_options)
	{
		if(!option_menu.update(delta_sec))
		{
			return false;
		}
	}

	colors[0] = colors[0] + (0.5 * color_delta);
	colors[1] = colors[1] + (0.7 * color_delta);
	colors[2] = colors[2] + (0.11 * color_delta);

	const float cameraSpeed = static_cast<float>(cv_camera_speed.data * delta_sec);
	glm::vec3 up = {0, 1, 0};

	if(keys_down[MOVE_FORWARD])
	{
		camera_pos += cameraSpeed * camera_direction;
	}

	if(keys_down[MOVE_BACKWARD])
	{
		camera_pos -= cameraSpeed * camera_direction;
	}

	if(keys_down[MOVE_LEFT])
	{
		camera_pos -= glm::normalize(glm::cross(camera_direction, up)) * cameraSpeed;
	}

	if(keys_down[MOVE_RIGHT])
	{
		camera_pos += glm::normalize(glm::cross(camera_direction, up)) * cameraSpeed;
	}

	if(keys_down[MOVE_JUMP])
	{
		camera_pos += up * cameraSpeed;
	}

	if(keys_down[MOVE_CROUCH])
	{
		camera_pos -= up * cameraSpeed;
	}

	return true;
}

bool demo_state::render()
{
	glm::vec3 up = {0, 1, 0};

	TIMER_U tick1;
	TIMER_U tick2;
	tick1 = timer_now();

	// ctx.glClearColor(0, 1, 0, 1.f);

	ctx.glClearColor(
		static_cast<float>((sin(colors[0]) + 1.0) / 2.0),
		static_cast<float>((sin(colors[1]) + 1.0) / 2.0),
		static_cast<float>((sin(colors[2]) + 1.0) / 2.0),
		1.f);
	ctx.glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

#if 1

	// identity atm;
	// glm::mat4x4 proj = glm::ortho<float>(0,50,0, 50, -1000, 1000);
	glm::mat4 proj = glm::perspective(
		glm::radians(80.0f),
		static_cast<float>(cv_screen_width.data) / static_cast<float>(cv_screen_height.data),
		0.2f,
		100.0f);

	glm::mat4 orientation = glm::rotate(
		glm::mat4(1.f),
		15.f,
		glm::vec3(
			static_cast<float>((sin(colors[0]) + 1.0) / 2.0),
			static_cast<float>((sin(colors[1]) + 1.0) / 2.0),
			static_cast<float>((sin(colors[2]) + 1.0) / 2.0)));

	glm::mat4x4 view = glm::lookAt(camera_pos, camera_pos + camera_direction, up);

	ctx.glDisable(GL_BLEND);
	ctx.glEnable(GL_DEPTH_TEST);

	ctx.glUseProgram(point_shader.gl_program_id);

	// This really should be a uniform buffer object, but it isnt supported in gl es 3.0 (needs 3.1)
	// it is also possible to just have an array of uniforms, but tbo's are bigger.
	// it's also possible to round robin the TBO if uploading is a bottleneck,
	// but for now I will just hope that glTexImage2D will not block like glTexSubImage2D
	// ctx.glActiveTexture(GL_TEXTURE0);
	ctx.glBindTexture(GL_TEXTURE_2D, gl_inst_table_tex_id);
	ctx.glTexImage2D(
		GL_TEXTURE_2D, 0, GL_RGBA32F, 1 * 4, 1, 0, GL_RGBA, GL_FLOAT, glm::value_ptr(orientation));

	ctx.glUniformMatrix4fv(point_shader.gl_uniforms.u_proj, 1, GL_FALSE, glm::value_ptr(proj));
	ctx.glUniformMatrix4fv(point_shader.gl_uniforms.u_view, 1, GL_FALSE, glm::value_ptr(view));

	ctx.glBindVertexArray(gl_vao_id);
	ctx.glDrawElementsInstanced(
		GL_TRIANGLES, ibo_buffer_size, GL_UNSIGNED_SHORT, NULL, point_buffer_size);
	ctx.glBindVertexArray(0);
	ctx.glBindTexture(GL_TEXTURE_2D, 0);
#endif

	ctx.glEnable(GL_BLEND);
	ctx.glDisable(GL_DEPTH_TEST);
	ctx.glUseProgram(mono_shader.gl_program_id);

	if(update_screen_resize)
	{ 
		update_screen_resize = false;
		glm::mat4 mvp = glm::ortho<float>(
			0,
			cv_screen_width.data, // NOLINT(bugprone-narrowing-conversions)
			cv_screen_height.data, // NOLINT(bugprone-narrowing-conversions)
			0,
			-1000,
			1000);
		ctx.glUniformMatrix4fv(mono_shader.gl_uniforms.u_mvp, 1, GL_FALSE, glm::value_ptr(mvp));
	}

	// ctx.glActiveTexture(GL_TEXTURE0);
	ctx.glBindTexture(GL_TEXTURE_2D, font_manager.gl_atlas_tex_id);

	// since the text is stored in a GL_RED texture,
	// I would need to pad each row to align to 4, but I don't.
	ctx.glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	if(show_text && gl_font_vertex_count != 0)
	{
		ctx.glBindVertexArray(gl_font_vao_id);
		ctx.glDrawArrays(GL_TRIANGLES, 0, gl_font_vertex_count);
		ctx.glBindVertexArray(0);
	}

	if(show_options)
	{
		if(!option_menu.render())
		{
			return false;
		}
	}

	if(show_console)
	{
		// requires gl_atlas_tex_id
		if(!console_menu.render())
		{
			return false;
		}
	}
	ctx.glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	ctx.glBindTexture(GL_TEXTURE_2D, 0);

	ctx.glUseProgram(0);

	tick2 = timer_now();

	perf_render.test(timer_delta_ms(tick1, tick2));

#ifndef __EMSCRIPTEN__
	if(cv_vsync.data == 0)
	{
		// this isn't ideal, but it saves the CPU and GPU.
		SDL_Delay(1);
	}

	tick1 = timer_now();
	// tick1 = tick2;

	SDL_GL_SwapWindow(g_app.window);

	// this could help with vsync causing bad latency, in exchange for less gpu utilization.
	// but you could also use use CPU time on non-opengl stuff, and then sleep the remainder.
	// ctx.glFinish();
	tick2 = timer_now();
	perf_swap.test(timer_delta_ms(tick1, tick2));
#endif

	return GL_RUNTIME(__func__) == GL_NO_ERROR;
}
DEMO_RESULT demo_state::process()
{
	TIMER_U tick1;
	TIMER_U tick2;

	tick1 = timer_now();

	SDL_Event e;
	while(SDL_PollEvent(&e) != 0)
	{
		// important events that should go first and shouldn't be eaten by any elements.
		switch(e.type)
		{
		case SDL_QUIT: return DEMO_RESULT::EXIT;
		case SDL_WINDOWEVENT:
			switch(e.window.event)
			{
			case SDL_WINDOWEVENT_SIZE_CHANGED: {
				int w;
				int h;
				SDL_GL_GetDrawableSize(g_app.window, &w, &h);
				cv_screen_width.data = w;
				cv_screen_height.data = h;

				ctx.glViewport(0, 0, cv_screen_width.data, cv_screen_height.data);
				// cv_screen_width.data = e.window.data1;
				// cv_screen_height.data = e.window.data2;
				update_screen_resize = true;
			}
			break;
				/* TODO: pretty important window events.
			case SDL_WINDOWEVENT_LEAVE:
				slogf("leave\n");
				break;
			case SDL_WINDOWEVENT_ENTER:
				slogf("enter\n");
				break;
			case SDL_WINDOWEVENT_FOCUS_GAINED:
				slogf("key focus gain\n");
				break;
			case SDL_WINDOWEVENT_FOCUS_LOST:
				slogf("key focus lost\n");
				break;
			case SDL_WINDOWEVENT_SHOWN:
				slogf("shown\n");
				break;
			case SDL_WINDOWEVENT_HIDDEN:
				slogf("hidden\n");
				break;
			case SDL_WINDOWEVENT_EXPOSED:
				slogf("exposed\n");
				break;
				*/
			}
			break;
		case SDL_KEYDOWN:
			if(e.key.keysym.sym == SDLK_F10)
			{
				// std::string().at(0);
				std::string msg;
				msg += "StackTrace (f10):\n";
				debug_str_stacktrace(&msg, 0);
				msg += '\n';
				slog_raw(msg.data(), msg.length());
				continue;
			}
			break;
#ifdef __EMSCRIPTEN__
		// this should already be registered using a callback.
		case SDL_MOUSEBUTTONUP: continue;
#endif
		}
		if(cv_bind_fullscreen.compare_sdl_event(e, KEYBIND_BUTTON_DOWN) != KEYBIND_NULL)
		{
			cv_fullscreen.data = cv_fullscreen.data == 1 ? 0 : 1;
			if(!cv_fullscreen.cvar_read(cv_fullscreen.data == 1 ? "1" : "0"))
			{
				return DEMO_RESULT::ERROR;
			}
			// dont "unfocus", but make this event invisible.
			continue;
		}

		if(cv_bind_reset_window_size.compare_sdl_event(e, KEYBIND_BUTTON_DOWN) != KEYBIND_NULL)
		{
			SDL_SetWindowSize(
				g_app.window, cv_startup_screen_width.data, cv_startup_screen_height.data);
			// dont "unfocus", but make this event invisible.
			continue;
		}

		if(cv_bind_soft_reboot.compare_sdl_event(e, KEYBIND_BUTTON_DOWN) != KEYBIND_NULL)
		{
			return DEMO_RESULT::SOFT_REBOOT;
		}

		if(cv_bind_toggle_text.compare_sdl_event(e, KEYBIND_BUTTON_DOWN) != KEYBIND_NULL)
		{
			show_text = !show_text;
			// dont "unfocus", but make this event invisible.
			continue;
		}

		if(!input(e))
		{
			return DEMO_RESULT::ERROR;
		}
	}
	tick2 = timer_now();
	perf_input.test(timer_delta_ms(tick1, tick2));

	tick1 = tick2;

	TIMER_U current_time = timer_now();
	double delta = timer_delta<1>(timer_last, current_time);
	timer_last = current_time;
	if(!update(delta))
	{
		return DEMO_RESULT::ERROR;
	}

	tick2 = timer_now();

	perf_update.test(timer_delta_ms(tick1, tick2));

	if(!render())
	{
		return DEMO_RESULT::ERROR;
	}

	if(show_text)
	{
		if(!perf_time())
		{
			return DEMO_RESULT::ERROR;
		}
	}

	return DEMO_RESULT::CONTINUE;
}

bool demo_state::perf_time()
{
	TIMER_U tick_now = timer_now();

	// NOTE: "total" will include the time of perf_time,
	// which is NOT sampled in any of the other timers
	// so if you add up all the averages, it wont add up to total.
	static TIMER_U total_start = tick_now;
	// static bench_data total_data;
	perf_total.test(timer_delta_ms(total_start, tick_now));
	total_start = tick_now;

	static TIMER_U display_timer = tick_now;

	// TODO: I should also draw from SDL_WINDOWEVENT_SIZE_CHANGED!
	if(timer_delta_ms(display_timer, tick_now) > 100)
	{
		bool success = true;
		display_timer = tick_now;

		// load the atlas texture.
		ctx.glBindTexture(GL_TEXTURE_2D, font_manager.gl_atlas_tex_id);
		// since the text is stored in a GL_RED texture,
		// I would need to pad each row to align to 4, but I don't.
		ctx.glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

		font_batcher.clear();

		font_painter.begin();

#if 1

		// font_settings.render_mode = FT_RENDER_MODE_NORMAL;

		font_painter.set_style(FONT_STYLE_OUTLINE);
		font_painter.set_color(0, 0, 0, 255);
		success = success && display_perf_text();

#endif

		// font_settings.render_mode = FT_RENDER_MODE_MONO;

		font_painter.set_style(FONT_STYLE_NORMAL);
		font_painter.set_color(255, 255, 255, 255);
		success = success && display_perf_text();

		font_painter.end();

		// restore to the default 4 alignment.
		ctx.glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
		ctx.glBindTexture(GL_TEXTURE_2D, 0);

		gl_font_vertex_count = font_batcher.get_current_vertex_count();

		if(font_batcher.get_quad_count() != 0)
		{
			ctx.glBindBuffer(GL_ARRAY_BUFFER, gl_font_interleave_vbo);
			// orphaning
			ctx.glBufferData(
				GL_ARRAY_BUFFER, font_batcher.get_total_vertex_size(), NULL, GL_STREAM_DRAW);
			ctx.glBufferSubData(
				GL_ARRAY_BUFFER, 0, font_batcher.get_current_vertex_size(), font_batcher.buffer);
			ctx.glBindBuffer(GL_ARRAY_BUFFER, 0);
		}

		success = success && GL_RUNTIME(__func__) == GL_NO_ERROR;

		if(!success)
		{
			return false;
		}

		perf_total.reset();
		perf_input.reset();
		perf_update.reset();
		perf_render.reset();
#ifndef __EMSCRIPTEN__
		perf_swap.reset();
#endif

#if 0
        static bool first_sample = false;
        if(first_sample == false)
        {
            first_sample = true;
            TIMER_U first_sample_time = timer_now();
            slogf("first_text_pass: %f\n", timer_delta_ms(tick_now, first_sample_time));
        }
#endif
	}
	return true;
}

bool demo_state::display_perf_text()
{
	bool success = true;
	font_painter.set_flags(TEXT_FLAGS::NEWLINE);

	float x = 2;
	float y = 2;
#if 1
	// globals bad, oh well.
	static bool test_string_success = true;
	if(test_string_success)
	{
		font_painter.set_xy(x, y);
		font_painter.set_anchor(TEXT_ANCHOR::TOP_LEFT);
		if(!font_painter.draw_text(cv_string.data.c_str(), cv_string.data.size()))
		{
			console_menu.post_error(serr_get_error());
			test_string_success = false;
		}
	}
	// font_batcher.newline();
#endif
	x = static_cast<float>(cv_screen_width.data) - 2;
	y = 2;
	font_painter.set_xy(x, y);
	font_painter.set_anchor(TEXT_ANCHOR::TOP_RIGHT);

	success = success && font_painter.draw_text("average / low / high\n");
	success = success && perf_total.display("total", &font_painter);
	success = success && perf_input.display("input", &font_painter);
	success = success && perf_update.display("update", &font_painter);
	success = success && perf_render.display("render", &font_painter);
#ifndef __EMSCRIPTEN__
	success = success && perf_swap.display("swap", &font_painter);
#endif
	return success;
}

bool bench_data::display(const char* msg, font_sprite_painter* font_painter)
{
	return font_painter->draw_format(
		"%s: %.2f / %.2f / %.2f\n", msg, accum_ms(), low_ms(), high_ms());
}
