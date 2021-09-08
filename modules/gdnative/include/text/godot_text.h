/*************************************************************************/
/*  godot_text.h                                                         */
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

#ifndef GODOT_NATIVETEXT_H
#define GODOT_NATIVETEXT_H

#include <gdnative/gdnative.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GODOT_TEXT_API_MAJOR 1
#define GODOT_TEXT_API_MINOR 0

#define GODOT_GLYPH_SIZE 40

#ifndef GODOT_TEXT_API_GODOT_GLYPH_TYPE_DEFINED
#define GODOT_TEXT_API_GODOT_GLYPH_TYPE_DEFINED
typedef struct {
	uint8_t _dont_touch_that[GODOT_GLYPH_SIZE];
} godot_glyph;
#endif

#define GODOT_PACKED_GLYPH_ARRAY_SIZE (2 * sizeof(void *))

#ifndef GODOT_TEXT_API_GODOT_PACKED_GLYPH_ARRAY_TYPE_DEFINED
#define GODOT_TEXT_API_GODOT_PACKED_GLYPH_ARRAY_TYPE_DEFINED
typedef struct {
	uint8_t _dont_touch_that[GODOT_PACKED_GLYPH_ARRAY_SIZE];
} godot_packed_glyph_array;
#endif

typedef struct {
	godot_gdnative_api_version version;

	void *(*constructor)(godot_object *);
	void (*destructor)(void *);

	godot_string (*get_name)(const void *);
	godot_bool (*has_feature)(const void *, godot_int);

	void (*free)(void *, godot_rid *);
	bool (*has)(void *, godot_rid *);

	bool (*load_support_data)(void *, const godot_string *);
	godot_string (*get_support_data_filename)(const void *);
	godot_string (*get_support_data_info)(const void *);
	bool (*save_support_data)(void *, const godot_string *);

	bool (*is_locale_right_to_left)(void *, const godot_string *);
	int32_t (*name_to_tag)(const void *, const godot_string *);
	godot_string (*tag_to_name)(const void *, int32_t);

	godot_rid (*create_font)(void *);
	void (*font_set_data)(void *, godot_rid *, const godot_packed_byte_array *);
	void (*font_set_data_ptr)(void *, godot_rid *, const uint8_t *, size_t);
	void (*font_set_antialiased)(void *, godot_rid *, bool);
	bool (*font_is_antialiased)(const void *, godot_rid *);
	void (*font_set_multichannel_signed_distance_field)(void *, godot_rid *, bool);
	bool (*font_is_multichannel_signed_distance_field)(const void *, godot_rid *);
	void (*font_set_msdf_pixel_range)(void *, godot_rid *, godot_int);
	godot_int (*font_get_msdf_pixel_range)(const void *, godot_rid *);
	void (*font_set_msdf_size)(void *, godot_rid *, godot_int);
	godot_int (*font_get_msdf_size)(const void *, godot_rid *);
	void (*font_set_fixed_size)(void *, godot_rid *, godot_int);
	godot_int (*font_get_fixed_size)(const void *, godot_rid *);
	void (*font_set_force_autohinter)(void *, godot_rid *, bool);
	bool (*font_is_force_autohinter)(const void *, godot_rid *);
	void (*font_set_hinting)(void *, godot_rid *, godot_int);
	godot_int (*font_get_hinting)(const void *, godot_rid *);
	void (*font_set_variation_coordinates)(void *, godot_rid *, const godot_dictionary *);
	godot_dictionary (*font_get_variation_coordinates)(const void *, godot_rid *);
	void (*font_set_oversampling)(void *, godot_rid *, godot_real_t);
	godot_real_t (*font_get_oversampling)(const void *, godot_rid *);
	godot_array (*font_get_size_cache_list)(const void *, godot_rid *);
	void (*font_clear_size_cache)(void *, godot_rid *);
	void (*font_remove_size_cache)(void *, godot_rid *, const godot_vector2i *);
	void (*font_set_ascent)(void *, godot_rid *, godot_int, godot_real_t);
	godot_real_t (*font_get_ascent)(const void *, godot_rid *, godot_int);
	void (*font_set_descent)(void *, godot_rid *, godot_int, godot_real_t);
	godot_real_t (*font_get_descent)(const void *, godot_rid *, godot_int);
	void (*font_set_underline_position)(void *, godot_rid *, godot_int, godot_real_t);
	godot_real_t (*font_get_underline_position)(const void *, godot_rid *, godot_int);
	void (*font_set_underline_thickness)(void *, godot_rid *, godot_int, godot_real_t);
	godot_real_t (*font_get_underline_thickness)(const void *, godot_rid *, godot_int);
	void (*font_set_scale)(void *, godot_rid *, godot_int, godot_real_t);
	godot_real_t (*font_get_scale)(const void *, godot_rid *, godot_int);
	void (*font_set_spacing)(void *, godot_rid *, godot_int, godot_int, godot_int);
	godot_int (*font_get_spacing)(const void *, godot_rid *, godot_int, godot_int);
	godot_int (*font_get_texture_count)(const void *, godot_rid *, const godot_vector2i *);
	void (*font_clear_textures)(void *, godot_rid *, const godot_vector2i *);
	void (*font_remove_texture)(void *, godot_rid *, const godot_vector2i *, godot_int);
	void (*font_set_texture_image)(void *, godot_rid *, const godot_vector2i *, godot_int, const godot_object *);
	godot_object *(*font_get_texture_image)(const void *, godot_rid *, const godot_vector2i *, godot_int);
	void (*font_set_texture_offsets)(void *, godot_rid *, const godot_vector2i *, godot_int, const godot_packed_int32_array *);
	godot_packed_int32_array (*font_get_texture_offsets)(const void *, godot_rid *, const godot_vector2i *, godot_int);
	godot_array (*font_get_glyph_list)(const void *, godot_rid *, const godot_vector2i *);
	void (*font_clear_glyphs)(void *, godot_rid *, const godot_vector2i *);
	void (*font_remove_glyph)(void *, godot_rid *, const godot_vector2i *, int32_t);
	godot_vector2 (*font_get_glyph_advance)(const void *, godot_rid *, godot_int, int32_t);
	void (*font_set_glyph_advance)(void *, godot_rid *, godot_int, int32_t, const godot_vector2 *);
	godot_vector2 (*font_get_glyph_offset)(const void *, godot_rid *, const godot_vector2i *, int32_t);
	void (*font_set_glyph_offset)(void *, godot_rid *, const godot_vector2i *, int32_t, const godot_vector2 *);
	godot_vector2 (*font_get_glyph_size)(const void *, godot_rid *, const godot_vector2i *, int32_t);
	void (*font_set_glyph_size)(void *, godot_rid *, const godot_vector2i *, int32_t, const godot_vector2 *);
	godot_rect2 (*font_get_glyph_uv_rect)(const void *, godot_rid *, const godot_vector2i *, int32_t);
	void (*font_set_glyph_uv_rect)(void *, godot_rid *, const godot_vector2i *, int32_t, const godot_rect2 *);
	godot_int (*font_get_glyph_texture_idx)(const void *, godot_rid *, const godot_vector2i *, int32_t);
	void (*font_set_glyph_texture_idx)(void *, godot_rid *, const godot_vector2i *, int32_t, godot_int);
	bool (*font_get_glyph_contours)(const void *, godot_rid *, godot_int, int32_t, godot_packed_vector3_array *, godot_packed_int32_array *, bool *);
	godot_array (*font_get_kerning_list)(const void *, godot_rid *, godot_int);
	void (*font_clear_kerning_map)(void *, godot_rid *, godot_int);
	void (*font_remove_kerning)(void *, godot_rid *, godot_int, const godot_vector2i *);
	void (*font_set_kerning)(void *, godot_rid *, godot_int, const godot_vector2i *, const godot_vector2 *);
	godot_vector2 (*font_get_kerning)(const void *, godot_rid *, godot_int, const godot_vector2i *);
	int32_t (*font_get_glyph_index)(const void *, godot_rid *, godot_int, char32_t, char32_t);
	bool (*font_has_char)(const void *, godot_rid *, char32_t);
	godot_string (*font_get_supported_chars)(const void *, godot_rid *);
	void (*font_render_range)(void *, godot_rid *, const godot_vector2i *, char32_t, char32_t);
	void (*font_render_glyph)(void *, godot_rid *, const godot_vector2i *, int32_t);
	void (*font_draw_glyph)(const void *, godot_rid *, godot_rid *, godot_int, const godot_vector2 *, int32_t, const godot_color *);
	void (*font_draw_glyph_outline)(const void *, godot_rid *, godot_rid *, godot_int, godot_int, const godot_vector2 *, int32_t, const godot_color *);
	bool (*font_is_language_supported)(const void *, godot_rid *, const godot_string *);
	void (*font_set_language_support_override)(void *, godot_rid *, const godot_string *, bool);
	bool (*font_get_language_support_override)(const void *, godot_rid *, const godot_string *);
	void (*font_remove_language_support_override)(void *, godot_rid *, const godot_string *);
	godot_packed_string_array (*font_get_language_support_overrides)(const void *, godot_rid *);
	bool (*font_is_script_supported)(const void *, godot_rid *, const godot_string *);
	void (*font_set_script_support_override)(void *, godot_rid *, const godot_string *, bool);
	bool (*font_get_script_support_override)(const void *, godot_rid *, const godot_string *);
	void (*font_remove_script_support_override)(void *, godot_rid *, const godot_string *);
	godot_packed_string_array (*font_get_script_support_overrides)(const void *, godot_rid *);
	godot_dictionary (*font_supported_feature_list)(const void *, godot_rid *);
	godot_dictionary (*font_supported_variation_list)(const void *, godot_rid *);
	godot_real_t (*font_get_global_oversampling)(const void *);
	void (*font_set_global_oversampling)(void *, godot_real_t);

	godot_rid (*create_shaped_text)(void *, godot_int, godot_int);
	void (*shaped_text_clear)(void *, godot_rid *);
	void (*shaped_text_set_direction)(void *, godot_rid *, godot_int);
	godot_int (*shaped_text_get_direction)(void *, godot_rid *);
	void (*shaped_text_set_bidi_override)(void *, godot_rid *, const godot_packed_vector2i_array *);
	void (*shaped_text_set_orientation)(void *, godot_rid *, godot_int);
	godot_int (*shaped_text_get_orientation)(void *, godot_rid *);
	void (*shaped_text_set_preserve_invalid)(void *, godot_rid *, bool);
	bool (*shaped_text_get_preserve_invalid)(void *, godot_rid *);
	void (*shaped_text_set_preserve_control)(void *, godot_rid *, bool);
	bool (*shaped_text_get_preserve_control)(void *, godot_rid *);
	bool (*shaped_text_add_string)(void *, godot_rid *, const godot_string *, const godot_rid **, int, const godot_dictionary *, const godot_string *);
	bool (*shaped_text_add_object)(void *, godot_rid *, const godot_variant *, const godot_vector2 *, godot_int, godot_int);
	bool (*shaped_text_resize_object)(void *, godot_rid *, const godot_variant *, const godot_vector2 *, godot_int);
	godot_rid (*shaped_text_substr)(void *, godot_rid *, godot_int, godot_int);
	godot_rid (*shaped_text_get_parent)(void *, godot_rid *);
	godot_real_t (*shaped_text_fit_to_width)(void *, godot_rid *, godot_real_t, uint8_t);
	godot_real_t (*shaped_text_tab_align)(void *, godot_rid *, godot_packed_float32_array *);
	bool (*shaped_text_shape)(void *, godot_rid *);
	bool (*shaped_text_update_breaks)(void *, godot_rid *);
	bool (*shaped_text_update_justification_ops)(void *, godot_rid *);
	void (*shaped_text_overrun_trim_to_width)(void *, godot_rid *, godot_real_t, uint8_t);
	bool (*shaped_text_is_ready)(void *, godot_rid *);
	godot_packed_glyph_array (*shaped_text_get_glyphs)(void *, godot_rid *);
	godot_vector2i (*shaped_text_get_range)(void *, godot_rid *);
	godot_packed_glyph_array (*shaped_text_sort_logical)(void *, godot_rid *);
	godot_packed_vector2i_array (*shaped_text_get_line_breaks_adv)(void *, godot_rid *, godot_packed_float32_array *, int, bool, uint8_t);
	godot_packed_vector2i_array (*shaped_text_get_line_breaks)(void *, godot_rid *, godot_real_t, int, uint8_t);
	godot_packed_vector2i_array (*shaped_text_get_word_breaks)(void *, godot_rid *, int);
	godot_array (*shaped_text_get_objects)(void *, godot_rid *);
	godot_rect2 (*shaped_text_get_object_rect)(void *, godot_rid *, const godot_variant *);
	godot_vector2 (*shaped_text_get_size)(void *, godot_rid *);
	godot_real_t (*shaped_text_get_ascent)(void *, godot_rid *);
	godot_real_t (*shaped_text_get_descent)(void *, godot_rid *);
	godot_real_t (*shaped_text_get_width)(void *, godot_rid *);
	godot_real_t (*shaped_text_get_underline_position)(void *, godot_rid *);
	godot_real_t (*shaped_text_get_underline_thickness)(void *, godot_rid *);

	godot_string (*format_number)(void *, const godot_string *, const godot_string *);
	godot_string (*parse_number)(void *, const godot_string *, const godot_string *);
	godot_string (*percent_sign)(void *, const godot_string *);
} godot_text_interface_gdnative;

void GDAPI godot_text_register_interface(const godot_text_interface_gdnative *p_interface, const godot_string *p_name, uint32_t p_features);

// Glyph

void GDAPI godot_glyph_new(godot_glyph *r_dest);

godot_vector2i GDAPI godot_glyph_get_range(const godot_glyph *p_self);
void GDAPI godot_glyph_set_range(godot_glyph *p_self, const godot_vector2i *p_range);

godot_int GDAPI godot_glyph_get_count(const godot_glyph *p_self);
void GDAPI godot_glyph_set_count(godot_glyph *p_self, godot_int p_count);

godot_int GDAPI godot_glyph_get_repeat(const godot_glyph *p_self);
void GDAPI godot_glyph_set_repeat(godot_glyph *p_self, godot_int p_repeat);

godot_int GDAPI godot_glyph_get_flags(const godot_glyph *p_self);
void GDAPI godot_glyph_set_flags(godot_glyph *p_self, godot_int p_flags);

godot_vector2 GDAPI godot_glyph_get_offset(const godot_glyph *p_self);
void GDAPI godot_glyph_set_offset(godot_glyph *p_self, const godot_vector2 *p_offset);

godot_real_t GDAPI godot_glyph_get_advance(const godot_glyph *p_self);
void GDAPI godot_glyph_set_advance(godot_glyph *p_self, godot_real_t p_advance);

godot_rid GDAPI godot_glyph_get_font(const godot_glyph *p_self);
void GDAPI godot_glyph_set_font(godot_glyph *p_self, godot_rid *p_font);

godot_int GDAPI godot_glyph_get_font_size(const godot_glyph *p_self);
void GDAPI godot_glyph_set_font_size(godot_glyph *p_self, godot_int p_size);

godot_int GDAPI godot_glyph_get_index(const godot_glyph *p_self);
void GDAPI godot_glyph_set_index(godot_glyph *p_self, godot_int p_index);

// GlyphArray

void GDAPI godot_packed_glyph_array_new(godot_packed_glyph_array *r_dest);
void GDAPI godot_packed_glyph_array_new_copy(godot_packed_glyph_array *r_dest, const godot_packed_glyph_array *p_src);

const godot_glyph GDAPI *godot_packed_glyph_array_ptr(const godot_packed_glyph_array *p_self);
godot_glyph GDAPI *godot_packed_glyph_array_ptrw(godot_packed_glyph_array *p_self);

void GDAPI godot_packed_glyph_array_append(godot_packed_glyph_array *p_self, const godot_glyph *p_data);

void GDAPI godot_packed_glyph_array_append_array(godot_packed_glyph_array *p_self, const godot_packed_glyph_array *p_array);

godot_error GDAPI godot_packed_glyph_array_insert(godot_packed_glyph_array *p_self, const godot_int p_idx, const godot_glyph *p_data);

godot_bool GDAPI godot_packed_glyph_array_has(godot_packed_glyph_array *p_self, const godot_glyph *p_value);

void GDAPI godot_packed_glyph_array_sort(godot_packed_glyph_array *p_self);

void GDAPI godot_packed_glyph_array_reverse(godot_packed_glyph_array *p_self);

void GDAPI godot_packed_glyph_array_push_back(godot_packed_glyph_array *p_self, const godot_glyph *p_data);

void GDAPI godot_packed_glyph_array_remove(godot_packed_glyph_array *p_self, godot_int p_idx);

void GDAPI godot_packed_glyph_array_resize(godot_packed_glyph_array *p_self, godot_int p_size);

void GDAPI godot_packed_glyph_array_set(godot_packed_glyph_array *p_self, godot_int p_idx, const godot_glyph *p_data);
godot_glyph GDAPI godot_packed_glyph_array_get(const godot_packed_glyph_array *p_self, godot_int p_idx);

godot_int GDAPI godot_packed_glyph_array_size(const godot_packed_glyph_array *p_self);

godot_bool GDAPI godot_packed_glyph_array_is_empty(const godot_packed_glyph_array *p_self);

void GDAPI godot_packed_glyph_array_destroy(godot_packed_glyph_array *p_self);

// Grapheme

#ifdef __cplusplus
}
#endif

#endif /* !GODOT_NATIVETEXT_H */
