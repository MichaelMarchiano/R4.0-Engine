/*************************************************************************/
/*  display_server_javascript.cpp                                        */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2021 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2021 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#include "platform/javascript/display_server_javascript.h"

#include "platform/javascript/os_javascript.h"
#include "servers/rendering/rasterizer_dummy.h"

#include <emscripten.h>
#include <png.h>

#include "dom_keys.inc"
#include "godot_js.h"

#define DOM_BUTTON_LEFT 0
#define DOM_BUTTON_MIDDLE 1
#define DOM_BUTTON_RIGHT 2
#define DOM_BUTTON_XBUTTON1 3
#define DOM_BUTTON_XBUTTON2 4

DisplayServerJavaScript *DisplayServerJavaScript::get_singleton() {
	return static_cast<DisplayServerJavaScript *>(DisplayServer::get_singleton());
}

// Window (canvas)
void DisplayServerJavaScript::focus_canvas() {
	godot_js_display_canvas_focus();
}

bool DisplayServerJavaScript::is_canvas_focused() {
	return godot_js_display_canvas_is_focused() != 0;
}

bool DisplayServerJavaScript::check_size_force_redraw() {
	return godot_js_display_size_update() != 0;
}

Point2 DisplayServerJavaScript::compute_position_in_canvas(int p_x, int p_y) {
	int point[2];
	godot_js_display_compute_position(p_x, p_y, point, point + 1);
	return Point2(point[0], point[1]);
}

EM_BOOL DisplayServerJavaScript::fullscreen_change_callback(int p_event_type, const EmscriptenFullscreenChangeEvent *p_event, void *p_user_data) {
	DisplayServerJavaScript *display = get_singleton();
	// Empty ID is canvas.
	String target_id = String::utf8(p_event->id);
	if (target_id.is_empty() || target_id == String::utf8(&(display->canvas_id[1]))) {
		// This event property is the only reliable data on
		// browser fullscreen state.
		if (p_event->isFullscreen) {
			display->window_mode = WINDOW_MODE_FULLSCREEN;
		} else {
			display->window_mode = WINDOW_MODE_WINDOWED;
		}
	}
	return false;
}

// Drag and drop callback.
void DisplayServerJavaScript::drop_files_js_callback(char **p_filev, int p_filec) {
	DisplayServerJavaScript *ds = get_singleton();
	if (!ds) {
		ERR_FAIL_MSG("Unable to drop files because the DisplayServer is not active");
	}
	if (ds->drop_files_callback.is_null()) {
		return;
	}
	Vector<String> files;
	for (int i = 0; i < p_filec; i++) {
		files.push_back(String::utf8(p_filev[i]));
	}
	Variant v = files;
	Variant *vp = &v;
	Variant ret;
	Callable::CallError ce;
	ds->drop_files_callback.call((const Variant **)&vp, 1, ret, ce);
}

// JavaScript quit request callback.
void DisplayServerJavaScript::request_quit_callback() {
	DisplayServerJavaScript *ds = get_singleton();
	if (ds && !ds->window_event_callback.is_null()) {
		Variant event = int(DisplayServer::WINDOW_EVENT_CLOSE_REQUEST);
		Variant *eventp = &event;
		Variant ret;
		Callable::CallError ce;
		ds->window_event_callback.call((const Variant **)&eventp, 1, ret, ce);
	}
}

// Keys

template <typename T>
void DisplayServerJavaScript::dom2godot_mod(T *emscripten_event_ptr, Ref<InputEventWithModifiers> godot_event) {
	godot_event->set_shift_pressed(emscripten_event_ptr->shiftKey);
	godot_event->set_alt_pressed(emscripten_event_ptr->altKey);
	godot_event->set_ctrl_pressed(emscripten_event_ptr->ctrlKey);
	godot_event->set_meta_pressed(emscripten_event_ptr->metaKey);
}

Ref<InputEventKey> DisplayServerJavaScript::setup_key_event(const EmscriptenKeyboardEvent *emscripten_event) {
	Ref<InputEventKey> ev;
	ev.instantiate();
	ev->set_echo(emscripten_event->repeat);
	dom2godot_mod(emscripten_event, ev);
	ev->set_keycode(dom_code2godot_scancode(emscripten_event->code, emscripten_event->key, false));
	ev->set_physical_keycode(dom_code2godot_scancode(emscripten_event->code, emscripten_event->key, true));

	String unicode = String::utf8(emscripten_event->key);
	// Check if empty or multi-character (e.g. `CapsLock`).
	if (unicode.length() != 1) {
		// Might be empty as well, but better than nonsense.
		unicode = String::utf8(emscripten_event->charValue);
	}
	if (unicode.length() == 1) {
		ev->set_unicode(unicode[0]);
	}

	return ev;
}

EM_BOOL DisplayServerJavaScript::keydown_callback(int p_event_type, const EmscriptenKeyboardEvent *p_event, void *p_user_data) {
	DisplayServerJavaScript *display = get_singleton();
	Ref<InputEventKey> ev = setup_key_event(p_event);
	ev->set_pressed(true);
	if (ev->get_unicode() == 0 && keycode_has_unicode(ev->get_keycode())) {
		// Defer to keypress event for legacy unicode retrieval.
		display->deferred_key_event = ev;
		// Do not suppress keypress event.
		return false;
	}
	Input::get_singleton()->parse_input_event(ev);
	return true;
}

EM_BOOL DisplayServerJavaScript::keypress_callback(int p_event_type, const EmscriptenKeyboardEvent *p_event, void *p_user_data) {
	DisplayServerJavaScript *display = get_singleton();
	display->deferred_key_event->set_unicode(p_event->charCode);
	Input::get_singleton()->parse_input_event(display->deferred_key_event);
	return true;
}

EM_BOOL DisplayServerJavaScript::keyup_callback(int p_event_type, const EmscriptenKeyboardEvent *p_event, void *p_user_data) {
	Ref<InputEventKey> ev = setup_key_event(p_event);
	ev->set_pressed(false);
	Input::get_singleton()->parse_input_event(ev);
	return ev->get_keycode() != KEY_UNKNOWN && ev->get_keycode() != (Key)0;
}

// Mouse

EM_BOOL DisplayServerJavaScript::mouse_button_callback(int p_event_type, const EmscriptenMouseEvent *p_event, void *p_user_data) {
	DisplayServerJavaScript *display = get_singleton();

	Ref<InputEventMouseButton> ev;
	ev.instantiate();
	ev->set_pressed(p_event_type == EMSCRIPTEN_EVENT_MOUSEDOWN);
	ev->set_position(compute_position_in_canvas(p_event->clientX, p_event->clientY));
	ev->set_global_position(ev->get_position());
	dom2godot_mod(p_event, ev);

	switch (p_event->button) {
		case DOM_BUTTON_LEFT:
			ev->set_button_index(MOUSE_BUTTON_LEFT);
			break;
		case DOM_BUTTON_MIDDLE:
			ev->set_button_index(MOUSE_BUTTON_MIDDLE);
			break;
		case DOM_BUTTON_RIGHT:
			ev->set_button_index(MOUSE_BUTTON_RIGHT);
			break;
		case DOM_BUTTON_XBUTTON1:
			ev->set_button_index(MOUSE_BUTTON_XBUTTON1);
			break;
		case DOM_BUTTON_XBUTTON2:
			ev->set_button_index(MOUSE_BUTTON_XBUTTON2);
			break;
		default:
			return false;
	}

	if (ev->is_pressed()) {
		double diff = emscripten_get_now() - display->last_click_ms;

		if (ev->get_button_index() == display->last_click_button_index) {
			if (diff < 400 && Point2(display->last_click_pos).distance_to(ev->get_position()) < 5) {
				display->last_click_ms = 0;
				display->last_click_pos = Point2(-100, -100);
				display->last_click_button_index = -1;
				ev->set_double_click(true);
			}

		} else {
			display->last_click_button_index = ev->get_button_index();
		}

		if (!ev->is_double_click()) {
			display->last_click_ms += diff;
			display->last_click_pos = ev->get_position();
		}
	}

	Input *input = Input::get_singleton();
	int mask = input->get_mouse_button_mask();
	MouseButton button_flag = MouseButton(1 << (ev->get_button_index() - 1));
	if (ev->is_pressed()) {
		// Since the event is consumed, focus manually. The containing iframe,
		// if exists, may not have focus yet, so focus even if already focused.
		focus_canvas();
		mask |= button_flag;
	} else if (mask & button_flag) {
		mask &= ~button_flag;
	} else {
		// Received release event, but press was outside the canvas, so ignore.
		return false;
	}
	ev->set_button_mask(mask);

	input->parse_input_event(ev);
	// Prevent multi-click text selection and wheel-click scrolling anchor.
	// Context menu is prevented through contextmenu event.
	return true;
}

EM_BOOL DisplayServerJavaScript::mousemove_callback(int p_event_type, const EmscriptenMouseEvent *p_event, void *p_user_data) {
	DisplayServerJavaScript *ds = get_singleton();
	Input *input = Input::get_singleton();
	int input_mask = input->get_mouse_button_mask();
	Point2 pos = compute_position_in_canvas(p_event->clientX, p_event->clientY);
	// For motion outside the canvas, only read mouse movement if dragging
	// started inside the canvas; imitating desktop app behaviour.
	if (!ds->cursor_inside_canvas && !input_mask)
		return false;

	Ref<InputEventMouseMotion> ev;
	ev.instantiate();
	dom2godot_mod(p_event, ev);
	ev->set_button_mask(input_mask);

	ev->set_position(pos);
	ev->set_global_position(ev->get_position());

	ev->set_relative(Vector2(p_event->movementX, p_event->movementY));
	input->set_mouse_position(ev->get_position());
	ev->set_speed(input->get_last_mouse_speed());

	input->parse_input_event(ev);
	// Don't suppress mouseover/-leave events.
	return false;
}

// Cursor
const char *DisplayServerJavaScript::godot2dom_cursor(DisplayServer::CursorShape p_shape) {
	switch (p_shape) {
		case DisplayServer::CURSOR_ARROW:
			return "auto";
		case DisplayServer::CURSOR_IBEAM:
			return "text";
		case DisplayServer::CURSOR_POINTING_HAND:
			return "pointer";
		case DisplayServer::CURSOR_CROSS:
			return "crosshair";
		case DisplayServer::CURSOR_WAIT:
			return "progress";
		case DisplayServer::CURSOR_BUSY:
			return "wait";
		case DisplayServer::CURSOR_DRAG:
			return "grab";
		case DisplayServer::CURSOR_CAN_DROP:
			return "grabbing";
		case DisplayServer::CURSOR_FORBIDDEN:
			return "no-drop";
		case DisplayServer::CURSOR_VSIZE:
			return "ns-resize";
		case DisplayServer::CURSOR_HSIZE:
			return "ew-resize";
		case DisplayServer::CURSOR_BDIAGSIZE:
			return "nesw-resize";
		case DisplayServer::CURSOR_FDIAGSIZE:
			return "nwse-resize";
		case DisplayServer::CURSOR_MOVE:
			return "move";
		case DisplayServer::CURSOR_VSPLIT:
			return "row-resize";
		case DisplayServer::CURSOR_HSPLIT:
			return "col-resize";
		case DisplayServer::CURSOR_HELP:
			return "help";
		default:
			return "auto";
	}
}

void DisplayServerJavaScript::cursor_set_shape(CursorShape p_shape) {
	ERR_FAIL_INDEX(p_shape, CURSOR_MAX);
	if (cursor_shape == p_shape) {
		return;
	}
	cursor_shape = p_shape;
	godot_js_display_cursor_set_shape(godot2dom_cursor(cursor_shape));
}

DisplayServer::CursorShape DisplayServerJavaScript::cursor_get_shape() const {
	return cursor_shape;
}

void DisplayServerJavaScript::cursor_set_custom_image(const RES &p_cursor, CursorShape p_shape, const Vector2 &p_hotspot) {
	if (p_cursor.is_valid()) {
		Ref<Texture2D> texture = p_cursor;
		Ref<AtlasTexture> atlas_texture = p_cursor;
		Ref<Image> image;
		Size2 texture_size;
		Rect2 atlas_rect;

		if (texture.is_valid()) {
			image = texture->get_image();
		}

		if (!image.is_valid() && atlas_texture.is_valid()) {
			texture = atlas_texture->get_atlas();

			atlas_rect.size.width = texture->get_width();
			atlas_rect.size.height = texture->get_height();
			atlas_rect.position.x = atlas_texture->get_region().position.x;
			atlas_rect.position.y = atlas_texture->get_region().position.y;

			texture_size.width = atlas_texture->get_region().size.x;
			texture_size.height = atlas_texture->get_region().size.y;
		} else if (image.is_valid()) {
			texture_size.width = texture->get_width();
			texture_size.height = texture->get_height();
		}

		ERR_FAIL_COND(!texture.is_valid());
		ERR_FAIL_COND(p_hotspot.x < 0 || p_hotspot.y < 0);
		ERR_FAIL_COND(texture_size.width > 256 || texture_size.height > 256);
		ERR_FAIL_COND(p_hotspot.x > texture_size.width || p_hotspot.y > texture_size.height);

		image = texture->get_image();

		ERR_FAIL_COND(!image.is_valid());

		image = image->duplicate();

		if (atlas_texture.is_valid())
			image->crop_from_point(
					atlas_rect.position.x,
					atlas_rect.position.y,
					texture_size.width,
					texture_size.height);

		if (image->get_format() != Image::FORMAT_RGBA8) {
			image->convert(Image::FORMAT_RGBA8);
		}

		png_image png_meta;
		memset(&png_meta, 0, sizeof png_meta);
		png_meta.version = PNG_IMAGE_VERSION;
		png_meta.width = texture_size.width;
		png_meta.height = texture_size.height;
		png_meta.format = PNG_FORMAT_RGBA;

		PackedByteArray png;
		size_t len;
		PackedByteArray data = image->get_data();
		ERR_FAIL_COND(!png_image_write_get_memory_size(png_meta, len, 0, data.ptr(), 0, nullptr));

		png.resize(len);
		ERR_FAIL_COND(!png_image_write_to_memory(&png_meta, png.ptrw(), &len, 0, data.ptr(), 0, nullptr));

		godot_js_display_cursor_set_custom_shape(godot2dom_cursor(p_shape), png.ptr(), len, p_hotspot.x, p_hotspot.y);

	} else {
		godot_js_display_cursor_set_custom_shape(godot2dom_cursor(p_shape), nullptr, 0, 0, 0);
	}

	cursor_set_shape(cursor_shape);
}

// Mouse mode
void DisplayServerJavaScript::mouse_set_mode(MouseMode p_mode) {
	ERR_FAIL_COND_MSG(p_mode == MOUSE_MODE_CONFINED || p_mode == MOUSE_MODE_CONFINED_HIDDEN, "MOUSE_MODE_CONFINED is not supported for the HTML5 platform.");
	if (p_mode == mouse_get_mode()) {
		return;
	}

	if (p_mode == MOUSE_MODE_VISIBLE) {
		godot_js_display_cursor_set_visible(1);
		emscripten_exit_pointerlock();

	} else if (p_mode == MOUSE_MODE_HIDDEN) {
		godot_js_display_cursor_set_visible(0);
		emscripten_exit_pointerlock();

	} else if (p_mode == MOUSE_MODE_CAPTURED) {
		godot_js_display_cursor_set_visible(1);
		EMSCRIPTEN_RESULT result = emscripten_request_pointerlock(canvas_id, false);
		ERR_FAIL_COND_MSG(result == EMSCRIPTEN_RESULT_FAILED_NOT_DEFERRED, "MOUSE_MODE_CAPTURED can only be entered from within an appropriate input callback.");
		ERR_FAIL_COND_MSG(result != EMSCRIPTEN_RESULT_SUCCESS, "MOUSE_MODE_CAPTURED can only be entered from within an appropriate input callback.");
	}
}

DisplayServer::MouseMode DisplayServerJavaScript::mouse_get_mode() const {
	if (godot_js_display_cursor_is_hidden()) {
		return MOUSE_MODE_HIDDEN;
	}

	EmscriptenPointerlockChangeEvent ev;
	emscripten_get_pointerlock_status(&ev);
	return (ev.isActive && String::utf8(ev.id) == String::utf8(&canvas_id[1])) ? MOUSE_MODE_CAPTURED : MOUSE_MODE_VISIBLE;
}

// Wheel
EM_BOOL DisplayServerJavaScript::wheel_callback(int p_event_type, const EmscriptenWheelEvent *p_event, void *p_user_data) {
	ERR_FAIL_COND_V(p_event_type != EMSCRIPTEN_EVENT_WHEEL, false);
	DisplayServerJavaScript *ds = get_singleton();
	if (!is_canvas_focused()) {
		if (ds->cursor_inside_canvas) {
			focus_canvas();
		} else {
			return false;
		}
	}

	Input *input = Input::get_singleton();
	Ref<InputEventMouseButton> ev;
	ev.instantiate();
	ev->set_position(input->get_mouse_position());
	ev->set_global_position(ev->get_position());

	ev->set_shift_pressed(input->is_key_pressed(KEY_SHIFT));
	ev->set_alt_pressed(input->is_key_pressed(KEY_ALT));
	ev->set_ctrl_pressed(input->is_key_pressed(KEY_CTRL));
	ev->set_meta_pressed(input->is_key_pressed(KEY_META));

	if (p_event->deltaY < 0)
		ev->set_button_index(MOUSE_BUTTON_WHEEL_UP);
	else if (p_event->deltaY > 0)
		ev->set_button_index(MOUSE_BUTTON_WHEEL_DOWN);
	else if (p_event->deltaX > 0)
		ev->set_button_index(MOUSE_BUTTON_WHEEL_LEFT);
	else if (p_event->deltaX < 0)
		ev->set_button_index(MOUSE_BUTTON_WHEEL_RIGHT);
	else
		return false;

	// Different browsers give wildly different delta values, and we can't
	// interpret deltaMode, so use default value for wheel events' factor.

	int button_flag = 1 << (ev->get_button_index() - 1);

	ev->set_pressed(true);
	ev->set_button_mask(MouseButton(input->get_mouse_button_mask() | button_flag));
	input->parse_input_event(ev);

	ev->set_pressed(false);
	ev->set_button_mask(MouseButton(input->get_mouse_button_mask() & ~button_flag));
	input->parse_input_event(ev);

	return true;
}

// Touch
EM_BOOL DisplayServerJavaScript::touch_press_callback(int p_event_type, const EmscriptenTouchEvent *p_event, void *p_user_data) {
	DisplayServerJavaScript *display = get_singleton();
	Ref<InputEventScreenTouch> ev;
	ev.instantiate();
	int lowest_id_index = -1;
	for (int i = 0; i < p_event->numTouches; ++i) {
		const EmscriptenTouchPoint &touch = p_event->touches[i];
		if (lowest_id_index == -1 || touch.identifier < p_event->touches[lowest_id_index].identifier)
			lowest_id_index = i;
		if (!touch.isChanged)
			continue;
		ev->set_index(touch.identifier);
		ev->set_position(compute_position_in_canvas(touch.clientX, touch.clientY));
		display->touches[i] = ev->get_position();
		ev->set_pressed(p_event_type == EMSCRIPTEN_EVENT_TOUCHSTART);

		Input::get_singleton()->parse_input_event(ev);
	}
	// Resume audio context after input in case autoplay was denied.
	return true;
}

EM_BOOL DisplayServerJavaScript::touchmove_callback(int p_event_type, const EmscriptenTouchEvent *p_event, void *p_user_data) {
	DisplayServerJavaScript *display = get_singleton();
	Ref<InputEventScreenDrag> ev;
	ev.instantiate();
	int lowest_id_index = -1;
	for (int i = 0; i < p_event->numTouches; ++i) {
		const EmscriptenTouchPoint &touch = p_event->touches[i];
		if (lowest_id_index == -1 || touch.identifier < p_event->touches[lowest_id_index].identifier)
			lowest_id_index = i;
		if (!touch.isChanged)
			continue;
		ev->set_index(touch.identifier);
		ev->set_position(compute_position_in_canvas(touch.clientX, touch.clientY));
		Point2 &prev = display->touches[i];
		ev->set_relative(ev->get_position() - prev);
		prev = ev->get_position();

		Input::get_singleton()->parse_input_event(ev);
	}
	return true;
}

bool DisplayServerJavaScript::screen_is_touchscreen(int p_screen) const {
	return godot_js_display_touchscreen_is_available();
}

// Virtual Keyboard
void DisplayServerJavaScript::vk_input_text_callback(const char *p_text, int p_cursor) {
	DisplayServerJavaScript *ds = DisplayServerJavaScript::get_singleton();
	if (!ds || ds->input_text_callback.is_null()) {
		return;
	}
	// Call input_text
	Variant event = String(p_text);
	Variant *eventp = &event;
	Variant ret;
	Callable::CallError ce;
	ds->input_text_callback.call((const Variant **)&eventp, 1, ret, ce);
	// Insert key right to reach position.
	Input *input = Input::get_singleton();
	Ref<InputEventKey> k;
	for (int i = 0; i < p_cursor; i++) {
		k.instantiate();
		k->set_pressed(true);
		k->set_echo(false);
		k->set_keycode(KEY_RIGHT);
		input->parse_input_event(k);
		k.instantiate();
		k->set_pressed(false);
		k->set_echo(false);
		k->set_keycode(KEY_RIGHT);
		input->parse_input_event(k);
	}
}

void DisplayServerJavaScript::virtual_keyboard_show(const String &p_existing_text, const Rect2 &p_screen_rect, bool p_multiline, int p_max_input_length, int p_cursor_start, int p_cursor_end) {
	godot_js_display_vk_show(p_existing_text.utf8().get_data(), p_multiline, p_cursor_start, p_cursor_end);
}

void DisplayServerJavaScript::virtual_keyboard_hide() {
	godot_js_display_vk_hide();
}

// Gamepad
void DisplayServerJavaScript::gamepad_callback(int p_index, int p_connected, const char *p_id, const char *p_guid) {
	Input *input = Input::get_singleton();
	if (p_connected) {
		input->joy_connection_changed(p_index, true, String::utf8(p_id), String::utf8(p_guid));
	} else {
		input->joy_connection_changed(p_index, false, "");
	}
}

void DisplayServerJavaScript::process_joypads() {
	Input *input = Input::get_singleton();
	int32_t pads = godot_js_display_gamepad_sample_count();
	int32_t s_btns_num = 0;
	int32_t s_axes_num = 0;
	int32_t s_standard = 0;
	float s_btns[16];
	float s_axes[10];
	for (int idx = 0; idx < pads; idx++) {
		int err = godot_js_display_gamepad_sample_get(idx, s_btns, &s_btns_num, s_axes, &s_axes_num, &s_standard);
		if (err) {
			continue;
		}
		for (int b = 0; b < s_btns_num; b++) {
			float value = s_btns[b];
			// Buttons 6 and 7 in the standard mapping need to be
			// axis to be handled as JOY_AXIS_TRIGGER by Godot.
			if (s_standard && (b == 6 || b == 7)) {
				Input::JoyAxisValue joy_axis;
				joy_axis.min = 0;
				joy_axis.value = value;
				JoyAxis a = b == 6 ? JOY_AXIS_TRIGGER_LEFT : JOY_AXIS_TRIGGER_RIGHT;
				input->joy_axis(idx, a, joy_axis);
			} else {
				input->joy_button(idx, (JoyButton)b, value);
			}
		}
		for (int a = 0; a < s_axes_num; a++) {
			Input::JoyAxisValue joy_axis;
			joy_axis.min = -1;
			joy_axis.value = s_axes[a];
			input->joy_axis(idx, (JoyAxis)a, joy_axis);
		}
	}
}

Vector<String> DisplayServerJavaScript::get_rendering_drivers_func() {
	Vector<String> drivers;
	drivers.push_back("dummy");
	return drivers;
}

// Clipboard
void DisplayServerJavaScript::update_clipboard_callback(const char *p_text) {
	get_singleton()->clipboard = p_text;
}

void DisplayServerJavaScript::clipboard_set(const String &p_text) {
	clipboard = p_text;
	int err = godot_js_display_clipboard_set(p_text.utf8().get_data());
	ERR_FAIL_COND_MSG(err, "Clipboard API is not supported.");
}

String DisplayServerJavaScript::clipboard_get() const {
	godot_js_display_clipboard_get(update_clipboard_callback);
	return clipboard;
}

void DisplayServerJavaScript::send_window_event_callback(int p_notification) {
	DisplayServerJavaScript *ds = get_singleton();
	if (!ds) {
		return;
	}
	if (p_notification == DisplayServer::WINDOW_EVENT_MOUSE_ENTER || p_notification == DisplayServer::WINDOW_EVENT_MOUSE_EXIT) {
		ds->cursor_inside_canvas = p_notification == DisplayServer::WINDOW_EVENT_MOUSE_ENTER;
	}
	if (!ds->window_event_callback.is_null()) {
		Variant event = int(p_notification);
		Variant *eventp = &event;
		Variant ret;
		Callable::CallError ce;
		ds->window_event_callback.call((const Variant **)&eventp, 1, ret, ce);
	}
}

void DisplayServerJavaScript::set_icon(const Ref<Image> &p_icon) {
	ERR_FAIL_COND(p_icon.is_null());
	Ref<Image> icon = p_icon;
	if (icon->is_compressed()) {
		icon = icon->duplicate();
		ERR_FAIL_COND(icon->decompress() != OK);
	}
	if (icon->get_format() != Image::FORMAT_RGBA8) {
		if (icon == p_icon)
			icon = icon->duplicate();
		icon->convert(Image::FORMAT_RGBA8);
	}

	png_image png_meta;
	memset(&png_meta, 0, sizeof png_meta);
	png_meta.version = PNG_IMAGE_VERSION;
	png_meta.width = icon->get_width();
	png_meta.height = icon->get_height();
	png_meta.format = PNG_FORMAT_RGBA;

	PackedByteArray png;
	size_t len;
	PackedByteArray data = icon->get_data();
	ERR_FAIL_COND(!png_image_write_get_memory_size(png_meta, len, 0, data.ptr(), 0, nullptr));

	png.resize(len);
	ERR_FAIL_COND(!png_image_write_to_memory(&png_meta, png.ptrw(), &len, 0, data.ptr(), 0, nullptr));

	godot_js_display_window_icon_set(png.ptr(), len);
}

void DisplayServerJavaScript::_dispatch_input_event(const Ref<InputEvent> &p_event) {
	OS_JavaScript *os = OS_JavaScript::get_singleton();

	// Resume audio context after input in case autoplay was denied.
	os->resume_audio();

	Callable cb = get_singleton()->input_event_callback;
	if (!cb.is_null()) {
		Variant ev = p_event;
		Variant *evp = &ev;
		Variant ret;
		Callable::CallError ce;
		cb.call((const Variant **)&evp, 1, ret, ce);
	}
}

DisplayServer *DisplayServerJavaScript::create_func(const String &p_rendering_driver, WindowMode p_window_mode, VSyncMode p_vsync_mode, uint32_t p_flags, const Size2i &p_resolution, Error &r_error) {
	return memnew(DisplayServerJavaScript(p_rendering_driver, p_window_mode, p_vsync_mode, p_flags, p_resolution, r_error));
}

DisplayServerJavaScript::DisplayServerJavaScript(const String &p_rendering_driver, WindowMode p_window_mode, VSyncMode p_vsync_mode, uint32_t p_flags, const Size2i &p_resolution, Error &r_error) {
	r_error = OK; // Always succeeds for now.

	// Ensure the canvas ID.
	godot_js_config_canvas_id_get(canvas_id, 256);

	// Handle contextmenu, webglcontextlost
	godot_js_display_setup_canvas(p_resolution.x, p_resolution.y, p_window_mode == WINDOW_MODE_FULLSCREEN, OS::get_singleton()->is_hidpi_allowed() ? 1 : 0);

	// Check if it's windows.
	swap_cancel_ok = godot_js_display_is_swap_ok_cancel() == 1;

	// Expose method for requesting quit.
	godot_js_os_request_quit_cb(request_quit_callback);

	RasterizerDummy::make_current(); // TODO GLES2 in Godot 4.0... or webgpu?
#if 0
	EmscriptenWebGLContextAttributes attributes;
	emscripten_webgl_init_context_attributes(&attributes);
	attributes.alpha = GLOBAL_GET("display/window/per_pixel_transparency/allowed");
	attributes.antialias = false;
	ERR_FAIL_INDEX_V(p_video_driver, VIDEO_DRIVER_MAX, ERR_INVALID_PARAMETER);

	if (p_desired.layered) {
		set_window_per_pixel_transparency_enabled(true);
	}

	bool gl_initialization_error = false;

	if (RasterizerGLES2::is_viable() == OK) {
		attributes.majorVersion = 1;
		RasterizerGLES2::register_config();
		RasterizerGLES2::make_current();
	} else {
		gl_initialization_error = true;
	}

	EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx = emscripten_webgl_create_context(canvas_id, &attributes);
	if (emscripten_webgl_make_context_current(ctx) != EMSCRIPTEN_RESULT_SUCCESS) {
		gl_initialization_error = true;
	}

	if (gl_initialization_error) {
		OS::get_singleton()->alert("Your browser does not seem to support WebGL. Please update your browser version.",
				"Unable to initialize video driver");
		return ERR_UNAVAILABLE;
	}

	video_driver_index = p_video_driver;
#endif

	EMSCRIPTEN_RESULT result;
#define EM_CHECK(ev)                         \
	if (result != EMSCRIPTEN_RESULT_SUCCESS) \
		ERR_PRINT("Error while setting " #ev " callback: Code " + itos(result));
#define SET_EM_CALLBACK(target, ev, cb)                                  \
	result = emscripten_set_##ev##_callback(target, nullptr, true, &cb); \
	EM_CHECK(ev)
#define SET_EM_WINDOW_CALLBACK(ev, cb)                                                            \
	result = emscripten_set_##ev##_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, false, &cb); \
	EM_CHECK(ev)
	// These callbacks from Emscripten's html5.h suffice to access most
	// JavaScript APIs.
	SET_EM_CALLBACK(canvas_id, mousedown, mouse_button_callback)
	SET_EM_WINDOW_CALLBACK(mousemove, mousemove_callback)
	SET_EM_WINDOW_CALLBACK(mouseup, mouse_button_callback)
	SET_EM_CALLBACK(canvas_id, wheel, wheel_callback)
	SET_EM_CALLBACK(canvas_id, touchstart, touch_press_callback)
	SET_EM_CALLBACK(canvas_id, touchmove, touchmove_callback)
	SET_EM_CALLBACK(canvas_id, touchend, touch_press_callback)
	SET_EM_CALLBACK(canvas_id, touchcancel, touch_press_callback)
	SET_EM_CALLBACK(canvas_id, keydown, keydown_callback)
	SET_EM_CALLBACK(canvas_id, keypress, keypress_callback)
	SET_EM_CALLBACK(canvas_id, keyup, keyup_callback)
	SET_EM_CALLBACK(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, fullscreenchange, fullscreen_change_callback)
#undef SET_EM_CALLBACK
#undef EM_CHECK

	// For APIs that are not (sufficiently) exposed, a
	// library is used below (implemented in library_godot_display.js).
	godot_js_display_notification_cb(&send_window_event_callback,
			WINDOW_EVENT_MOUSE_ENTER,
			WINDOW_EVENT_MOUSE_EXIT,
			WINDOW_EVENT_FOCUS_IN,
			WINDOW_EVENT_FOCUS_OUT);
	godot_js_display_paste_cb(update_clipboard_callback);
	godot_js_display_drop_files_cb(drop_files_js_callback);
	godot_js_display_gamepad_cb(&DisplayServerJavaScript::gamepad_callback);
	godot_js_display_vk_cb(&vk_input_text_callback);

	Input::get_singleton()->set_event_dispatch_function(_dispatch_input_event);
}

DisplayServerJavaScript::~DisplayServerJavaScript() {
	//emscripten_webgl_commit_frame();
	//emscripten_webgl_destroy_context(webgl_ctx);
}

bool DisplayServerJavaScript::has_feature(Feature p_feature) const {
	switch (p_feature) {
		//case FEATURE_CONSOLE_WINDOW:
		//case FEATURE_GLOBAL_MENU:
		//case FEATURE_HIDPI:
		//case FEATURE_IME:
		case FEATURE_ICON:
		case FEATURE_CLIPBOARD:
		case FEATURE_CURSOR_SHAPE:
		case FEATURE_CUSTOM_CURSOR_SHAPE:
		case FEATURE_MOUSE:
		case FEATURE_TOUCHSCREEN:
			return true;
		//case FEATURE_MOUSE_WARP:
		//case FEATURE_NATIVE_DIALOG:
		//case FEATURE_NATIVE_ICON:
		//case FEATURE_WINDOW_TRANSPARENCY:
		//case FEATURE_KEEP_SCREEN_ON:
		//case FEATURE_ORIENTATION:
		case FEATURE_VIRTUAL_KEYBOARD:
			return godot_js_display_vk_available() != 0;
		default:
			return false;
	}
}

void DisplayServerJavaScript::register_javascript_driver() {
	register_create_function("javascript", create_func, get_rendering_drivers_func);
}

String DisplayServerJavaScript::get_name() const {
	return "javascript";
}

int DisplayServerJavaScript::get_screen_count() const {
	return 1;
}

Point2i DisplayServerJavaScript::screen_get_position(int p_screen) const {
	return Point2i(); // TODO offsetX/Y?
}

Size2i DisplayServerJavaScript::screen_get_size(int p_screen) const {
	int size[2];
	godot_js_display_screen_size_get(size, size + 1);
	return Size2(size[0], size[1]);
}

Rect2i DisplayServerJavaScript::screen_get_usable_rect(int p_screen) const {
	int size[2];
	godot_js_display_window_size_get(size, size + 1);
	return Rect2i(0, 0, size[0], size[1]);
}

int DisplayServerJavaScript::screen_get_dpi(int p_screen) const {
	return godot_js_display_screen_dpi_get();
}

float DisplayServerJavaScript::screen_get_scale(int p_screen) const {
	return godot_js_display_pixel_ratio_get();
}

Vector<DisplayServer::WindowID> DisplayServerJavaScript::get_window_list() const {
	Vector<WindowID> ret;
	ret.push_back(MAIN_WINDOW_ID);
	return ret;
}

DisplayServerJavaScript::WindowID DisplayServerJavaScript::get_window_at_screen_position(const Point2i &p_position) const {
	return MAIN_WINDOW_ID;
}

void DisplayServerJavaScript::window_attach_instance_id(ObjectID p_instance, WindowID p_window) {
	window_attached_instance_id = p_instance;
}

ObjectID DisplayServerJavaScript::window_get_attached_instance_id(WindowID p_window) const {
	return window_attached_instance_id;
}

void DisplayServerJavaScript::window_set_rect_changed_callback(const Callable &p_callable, WindowID p_window) {
	// Not supported.
}

void DisplayServerJavaScript::window_set_window_event_callback(const Callable &p_callable, WindowID p_window) {
	window_event_callback = p_callable;
}

void DisplayServerJavaScript::window_set_input_event_callback(const Callable &p_callable, WindowID p_window) {
	input_event_callback = p_callable;
}

void DisplayServerJavaScript::window_set_input_text_callback(const Callable &p_callable, WindowID p_window) {
	input_text_callback = p_callable;
}

void DisplayServerJavaScript::window_set_drop_files_callback(const Callable &p_callable, WindowID p_window) {
	drop_files_callback = p_callable;
}

void DisplayServerJavaScript::window_set_title(const String &p_title, WindowID p_window) {
	godot_js_display_window_title_set(p_title.utf8().get_data());
}

int DisplayServerJavaScript::window_get_current_screen(WindowID p_window) const {
	return 1;
}

void DisplayServerJavaScript::window_set_current_screen(int p_screen, WindowID p_window) {
	// Not implemented.
}

Point2i DisplayServerJavaScript::window_get_position(WindowID p_window) const {
	return Point2i(); // TODO Does this need implementation?
}

void DisplayServerJavaScript::window_set_position(const Point2i &p_position, WindowID p_window) {
	// Not supported.
}

void DisplayServerJavaScript::window_set_transient(WindowID p_window, WindowID p_parent) {
	// Not supported.
}

void DisplayServerJavaScript::window_set_max_size(const Size2i p_size, WindowID p_window) {
	// Not supported.
}

Size2i DisplayServerJavaScript::window_get_max_size(WindowID p_window) const {
	return Size2i();
}

void DisplayServerJavaScript::window_set_min_size(const Size2i p_size, WindowID p_window) {
	// Not supported.
}

Size2i DisplayServerJavaScript::window_get_min_size(WindowID p_window) const {
	return Size2i();
}

void DisplayServerJavaScript::window_set_size(const Size2i p_size, WindowID p_window) {
	godot_js_display_desired_size_set(p_size.x, p_size.y);
}

Size2i DisplayServerJavaScript::window_get_size(WindowID p_window) const {
	int size[2];
	godot_js_display_window_size_get(size, size + 1);
	return Size2i(size[0], size[1]);
}

Size2i DisplayServerJavaScript::window_get_real_size(WindowID p_window) const {
	return window_get_size(p_window);
}

void DisplayServerJavaScript::window_set_mode(WindowMode p_mode, WindowID p_window) {
	if (window_mode == p_mode)
		return;

	switch (p_mode) {
		case WINDOW_MODE_WINDOWED: {
			if (window_mode == WINDOW_MODE_FULLSCREEN) {
				godot_js_display_fullscreen_exit();
			}
			window_mode = WINDOW_MODE_WINDOWED;
		} break;
		case WINDOW_MODE_FULLSCREEN: {
			int result = godot_js_display_fullscreen_request();
			ERR_FAIL_COND_MSG(result, "The request was denied. Remember that enabling fullscreen is only possible from an input callback for the HTML5 platform.");
		} break;
		case WINDOW_MODE_MAXIMIZED:
		case WINDOW_MODE_MINIMIZED:
			WARN_PRINT("WindowMode MAXIMIZED and MINIMIZED are not supported in HTML5 platform.");
			break;
		default:
			break;
	}
}

DisplayServerJavaScript::WindowMode DisplayServerJavaScript::window_get_mode(WindowID p_window) const {
	return window_mode;
}

bool DisplayServerJavaScript::window_is_maximize_allowed(WindowID p_window) const {
	return false;
}

void DisplayServerJavaScript::window_set_flag(WindowFlags p_flag, bool p_enabled, WindowID p_window) {
	// Not supported.
}

bool DisplayServerJavaScript::window_get_flag(WindowFlags p_flag, WindowID p_window) const {
	return false;
}

void DisplayServerJavaScript::window_request_attention(WindowID p_window) {
	// Not supported.
}

void DisplayServerJavaScript::window_move_to_foreground(WindowID p_window) {
	// Not supported.
}

bool DisplayServerJavaScript::window_can_draw(WindowID p_window) const {
	return true;
}

bool DisplayServerJavaScript::can_any_window_draw() const {
	return true;
}

void DisplayServerJavaScript::process_events() {
	if (godot_js_display_gamepad_sample() == OK) {
		process_joypads();
	}
}

int DisplayServerJavaScript::get_current_video_driver() const {
	return 1;
}

bool DisplayServerJavaScript::get_swap_cancel_ok() {
	return swap_cancel_ok;
}

void DisplayServerJavaScript::swap_buffers() {
	//emscripten_webgl_commit_frame();
}
