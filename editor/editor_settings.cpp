/*************************************************************************/
/*  editor_settings.cpp                                                  */
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

#include "editor_settings.h"

#include "core/config/project_settings.h"
#include "core/input/input_map.h"
#include "core/io/certs_compressed.gen.h"
#include "core/io/compression.h"
#include "core/io/config_file.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/file_access_memory.h"
#include "core/io/ip.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/io/translation_loader_po.h"
#include "core/os/keyboard.h"
#include "core/os/os.h"
#include "core/version.h"
#include "editor/doc_translations.gen.h"
#include "editor/editor_node.h"
#include "editor/editor_translations.gen.h"
#include "scene/main/node.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"

// PRIVATE METHODS

Ref<EditorSettings> EditorSettings::singleton = nullptr;

// Properties

bool EditorSettings::_set(const StringName &p_name, const Variant &p_value) {
	_THREAD_SAFE_METHOD_

	bool changed = _set_only(p_name, p_value);
	if (changed) {
		emit_signal(SNAME("settings_changed"));
	}
	return true;
}

bool EditorSettings::_set_only(const StringName &p_name, const Variant &p_value) {
	_THREAD_SAFE_METHOD_

	if (p_name == "shortcuts") {
		Array arr = p_value;
		ERR_FAIL_COND_V(arr.size() && arr.size() & 1, true);
		for (int i = 0; i < arr.size(); i += 2) {
			String name = arr[i];
			Ref<InputEvent> shortcut = arr[i + 1];

			Ref<Shortcut> sc;
			sc.instantiate();
			sc->set_event(shortcut);
			add_shortcut(name, sc);
		}

		return false;
	} else if (p_name == "builtin_action_overrides") {
		Array actions_arr = p_value;
		for (int i = 0; i < actions_arr.size(); i++) {
			Dictionary action_dict = actions_arr[i];

			String name = action_dict["name"];
			Array events = action_dict["events"];

			InputMap *im = InputMap::get_singleton();
			im->action_erase_events(name);

			builtin_action_overrides[name].clear();
			for (int ev_idx = 0; ev_idx < events.size(); ev_idx++) {
				im->action_add_event(name, events[ev_idx]);
				builtin_action_overrides[name].push_back(events[ev_idx]);
			}
		}
		return false;
	}

	bool changed = false;

	if (p_value.get_type() == Variant::NIL) {
		if (props.has(p_name)) {
			props.erase(p_name);
			changed = true;
		}
	} else {
		if (props.has(p_name)) {
			if (p_value != props[p_name].variant) {
				props[p_name].variant = p_value;
				changed = true;
			}
		} else {
			props[p_name] = VariantContainer(p_value, last_order++);
			changed = true;
		}

		if (save_changed_setting) {
			if (!props[p_name].save) {
				props[p_name].save = true;
				changed = true;
			}
		}
	}

	return changed;
}

bool EditorSettings::_get(const StringName &p_name, Variant &r_ret) const {
	_THREAD_SAFE_METHOD_

	if (p_name == "shortcuts") {
		Array arr;
		for (const Map<String, Ref<Shortcut>>::Element *E = shortcuts.front(); E; E = E->next()) {
			Ref<Shortcut> sc = E->get();

			if (builtin_action_overrides.has(E->key())) {
				// This shortcut was auto-generated from built in actions: don't save.
				continue;
			}

			if (optimize_save) {
				if (!sc->has_meta("original")) {
					continue; //this came from settings but is not any longer used
				}

				Ref<InputEvent> original = sc->get_meta("original");
				if (sc->matches_event(original) || (original.is_null() && sc->get_event().is_null())) {
					continue; //not changed from default, don't save
				}
			}

			arr.push_back(E->key());
			arr.push_back(sc->get_event());
		}
		r_ret = arr;
		return true;
	} else if (p_name == "builtin_action_overrides") {
		Array actions_arr;
		for (Map<String, List<Ref<InputEvent>>>::Element *E = builtin_action_overrides.front(); E; E = E->next()) {
			List<Ref<InputEvent>> events = E->get();

			// TODO: skip actions which are the same as the builtin.
			Dictionary action_dict;
			action_dict["name"] = E->key();

			Array events_arr;
			for (List<Ref<InputEvent>>::Element *I = events.front(); I; I = I->next()) {
				events_arr.push_back(I->get());
			}

			action_dict["events"] = events_arr;

			actions_arr.push_back(action_dict);
		}

		r_ret = actions_arr;
		return true;
	}

	const VariantContainer *v = props.getptr(p_name);
	if (!v) {
		WARN_PRINT("EditorSettings::_get - Property not found: " + String(p_name));
		return false;
	}
	r_ret = v->variant;
	return true;
}

void EditorSettings::_initial_set(const StringName &p_name, const Variant &p_value) {
	set(p_name, p_value);
	props[p_name].initial = p_value;
	props[p_name].has_default_value = true;
}

struct _EVCSort {
	String name;
	Variant::Type type = Variant::Type::NIL;
	int order = 0;
	bool save = false;
	bool restart_if_changed = false;

	bool operator<(const _EVCSort &p_vcs) const { return order < p_vcs.order; }
};

void EditorSettings::_get_property_list(List<PropertyInfo> *p_list) const {
	_THREAD_SAFE_METHOD_

	const String *k = nullptr;
	Set<_EVCSort> vclist;

	while ((k = props.next(k))) {
		const VariantContainer *v = props.getptr(*k);

		if (v->hide_from_editor) {
			continue;
		}

		_EVCSort vc;
		vc.name = *k;
		vc.order = v->order;
		vc.type = v->variant.get_type();
		vc.save = v->save;
		/*if (vc.save) { this should be implemented, but lets do after 3.1 is out.
			if (v->initial.get_type() != Variant::NIL && v->initial == v->variant) {
				vc.save = false;
			}
		}*/
		vc.restart_if_changed = v->restart_if_changed;

		vclist.insert(vc);
	}

	for (Set<_EVCSort>::Element *E = vclist.front(); E; E = E->next()) {
		uint32_t pusage = PROPERTY_USAGE_NONE;
		if (E->get().save || !optimize_save) {
			pusage |= PROPERTY_USAGE_STORAGE;
		}

		if (!E->get().name.begins_with("_") && !E->get().name.begins_with("projects/")) {
			pusage |= PROPERTY_USAGE_EDITOR;
		} else {
			pusage |= PROPERTY_USAGE_STORAGE; //hiddens must always be saved
		}

		PropertyInfo pi(E->get().type, E->get().name);
		pi.usage = pusage;
		if (hints.has(E->get().name)) {
			pi = hints[E->get().name];
		}

		if (E->get().restart_if_changed) {
			pi.usage |= PROPERTY_USAGE_RESTART_IF_CHANGED;
		}

		p_list->push_back(pi);
	}

	p_list->push_back(PropertyInfo(Variant::ARRAY, "shortcuts", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL)); //do not edit
	p_list->push_back(PropertyInfo(Variant::ARRAY, "builtin_action_overrides", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL));
}

void EditorSettings::_add_property_info_bind(const Dictionary &p_info) {
	ERR_FAIL_COND(!p_info.has("name"));
	ERR_FAIL_COND(!p_info.has("type"));

	PropertyInfo pinfo;
	pinfo.name = p_info["name"];
	ERR_FAIL_COND(!props.has(pinfo.name));
	pinfo.type = Variant::Type(p_info["type"].operator int());
	ERR_FAIL_INDEX(pinfo.type, Variant::VARIANT_MAX);

	if (p_info.has("hint")) {
		pinfo.hint = PropertyHint(p_info["hint"].operator int());
	}
	if (p_info.has("hint_string")) {
		pinfo.hint_string = p_info["hint_string"];
	}

	add_property_hint(pinfo);
}

// Default configs
bool EditorSettings::has_default_value(const String &p_setting) const {
	_THREAD_SAFE_METHOD_

	if (!props.has(p_setting)) {
		return false;
	}
	return props[p_setting].has_default_value;
}

void EditorSettings::_load_defaults(Ref<ConfigFile> p_extra_config) {
	_THREAD_SAFE_METHOD_

	/* Languages */

	{
		String lang_hint = "en";
		String host_lang = OS::get_singleton()->get_locale();
		host_lang = TranslationServer::standardize_locale(host_lang);

		// Skip locales if Text server lack required features.
		Vector<String> locales_to_skip;
		if (!TS->has_feature(TextServer::FEATURE_BIDI_LAYOUT) || !TS->has_feature(TextServer::FEATURE_SHAPING)) {
			locales_to_skip.push_back("ar"); // Arabic
			locales_to_skip.push_back("fa"); // Persian
			locales_to_skip.push_back("ur"); // Urdu
		}
		if (!TS->has_feature(TextServer::FEATURE_BIDI_LAYOUT)) {
			locales_to_skip.push_back("he"); // Hebrew
		}
		if (!TS->has_feature(TextServer::FEATURE_SHAPING)) {
			locales_to_skip.push_back("bn"); // Bengali
			locales_to_skip.push_back("hi"); // Hindi
			locales_to_skip.push_back("ml"); // Malayalam
			locales_to_skip.push_back("si"); // Sinhala
			locales_to_skip.push_back("ta"); // Tamil
			locales_to_skip.push_back("te"); // Telugu
		}

		if (!locales_to_skip.is_empty()) {
			WARN_PRINT("Some locales are not properly supported by selected Text Server and are disabled.");
		}

		String best;
		EditorTranslationList *etl = _editor_translations;

		while (etl->data) {
			const String &locale = etl->lang;

			// Skip locales which we can't render properly (see above comment).
			// Test against language code without regional variants (e.g. ur_PK).
			String lang_code = locale.get_slice("_", 0);
			if (locales_to_skip.find(lang_code) != -1) {
				etl++;
				continue;
			}

			lang_hint += ",";
			lang_hint += locale;

			if (host_lang == locale) {
				best = locale;
			}

			if (best == String() && host_lang.begins_with(locale)) {
				best = locale;
			}

			etl++;
		}

		if (best == String()) {
			best = "en";
		}

		_initial_set("interface/editor/editor_language", best);
		set_restart_if_changed("interface/editor/editor_language", true);
		hints["interface/editor/editor_language"] = PropertyInfo(Variant::STRING, "interface/editor/editor_language", PROPERTY_HINT_ENUM, lang_hint, PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_RESTART_IF_CHANGED);
	}

	/* Interface */

	// Editor
	_initial_set("interface/editor/display_scale", 0);
	// Display what the Auto display scale setting effectively corresponds to.
	float scale = get_auto_display_scale();

	_initial_set("interface/editor/enable_debugging_pseudolocalization", false);
	set_restart_if_changed("interface/editor/enable_debugging_pseudolocalization", true);
	// Use pseudolocalization in editor.

	hints["interface/editor/display_scale"] = PropertyInfo(Variant::INT, "interface/editor/display_scale", PROPERTY_HINT_ENUM, vformat("Auto (%d%%),75%%,100%%,125%%,150%%,175%%,200%%,Custom", Math::round(scale * 100)), PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_RESTART_IF_CHANGED);
	_initial_set("interface/editor/custom_display_scale", 1.0f);
	hints["interface/editor/custom_display_scale"] = PropertyInfo(Variant::FLOAT, "interface/editor/custom_display_scale", PROPERTY_HINT_RANGE, "0.5,3,0.01", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_RESTART_IF_CHANGED);
	_initial_set("interface/editor/main_font_size", 14);
	hints["interface/editor/main_font_size"] = PropertyInfo(Variant::INT, "interface/editor/main_font_size", PROPERTY_HINT_RANGE, "8,48,1", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_RESTART_IF_CHANGED);
	_initial_set("interface/editor/code_font_size", 14);
	hints["interface/editor/code_font_size"] = PropertyInfo(Variant::INT, "interface/editor/code_font_size", PROPERTY_HINT_RANGE, "8,48,1", PROPERTY_USAGE_DEFAULT);
	_initial_set("interface/editor/code_font_contextual_ligatures", 0);
	hints["interface/editor/code_font_contextual_ligatures"] = PropertyInfo(Variant::INT, "interface/editor/code_font_contextual_ligatures", PROPERTY_HINT_ENUM, "Default,Disable Contextual Alternates (Coding Ligatures),Use Custom OpenType Feature Set", PROPERTY_USAGE_DEFAULT);
	_initial_set("interface/editor/code_font_custom_opentype_features", "");
	_initial_set("interface/editor/code_font_custom_variations", "");
	_initial_set("interface/editor/font_antialiased", true);
	_initial_set("interface/editor/font_hinting", 0);
#ifdef OSX_ENABLED
	hints["interface/editor/font_hinting"] = PropertyInfo(Variant::INT, "interface/editor/font_hinting", PROPERTY_HINT_ENUM, "Auto (None),None,Light,Normal", PROPERTY_USAGE_DEFAULT);
#else
	hints["interface/editor/font_hinting"] = PropertyInfo(Variant::INT, "interface/editor/font_hinting", PROPERTY_HINT_ENUM, "Auto (Light),None,Light,Normal", PROPERTY_USAGE_DEFAULT);
#endif
	_initial_set("interface/editor/main_font", "");
	hints["interface/editor/main_font"] = PropertyInfo(Variant::STRING, "interface/editor/main_font", PROPERTY_HINT_GLOBAL_FILE, "*.ttf,*.otf", PROPERTY_USAGE_DEFAULT);
	_initial_set("interface/editor/main_font_bold", "");
	hints["interface/editor/main_font_bold"] = PropertyInfo(Variant::STRING, "interface/editor/main_font_bold", PROPERTY_HINT_GLOBAL_FILE, "*.ttf,*.otf", PROPERTY_USAGE_DEFAULT);
	_initial_set("interface/editor/code_font", "");
	hints["interface/editor/code_font"] = PropertyInfo(Variant::STRING, "interface/editor/code_font", PROPERTY_HINT_GLOBAL_FILE, "*.ttf,*.otf", PROPERTY_USAGE_DEFAULT);
	_initial_set("interface/editor/low_processor_mode_sleep_usec", 6900); // ~144 FPS
	hints["interface/editor/low_processor_mode_sleep_usec"] = PropertyInfo(Variant::FLOAT, "interface/editor/low_processor_mode_sleep_usec", PROPERTY_HINT_RANGE, "1,100000,1", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_RESTART_IF_CHANGED);
	_initial_set("interface/editor/unfocused_low_processor_mode_sleep_usec", 100000); // 10 FPS
	// Allow an unfocused FPS limit as low as 1 FPS for those who really need low power usage
	// (but don't need to preview particles or shaders while the editor is unfocused).
	// With very low FPS limits, the editor can take a small while to become usable after being focused again,
	// so this should be used at the user's discretion.
	hints["interface/editor/unfocused_low_processor_mode_sleep_usec"] = PropertyInfo(Variant::FLOAT, "interface/editor/unfocused_low_processor_mode_sleep_usec", PROPERTY_HINT_RANGE, "1,1000000,1", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_RESTART_IF_CHANGED);
	_initial_set("interface/editor/separate_distraction_mode", false);
	_initial_set("interface/editor/automatically_open_screenshots", true);
	_initial_set("interface/editor/single_window_mode", false);
	hints["interface/editor/single_window_mode"] = PropertyInfo(Variant::BOOL, "interface/editor/single_window_mode", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_RESTART_IF_CHANGED);
	_initial_set("interface/editor/hide_console_window", false);
	_initial_set("interface/editor/save_each_scene_on_quit", true); // Regression

	// Inspector
	_initial_set("interface/inspector/max_array_dictionary_items_per_page", 20);
	hints["interface/inspector/max_array_dictionary_items_per_page"] = PropertyInfo(Variant::INT, "interface/inspector/max_array_dictionary_items_per_page", PROPERTY_HINT_RANGE, "10,100,1", PROPERTY_USAGE_DEFAULT);

	// Theme
	_initial_set("interface/theme/preset", "Default");
	hints["interface/theme/preset"] = PropertyInfo(Variant::STRING, "interface/theme/preset", PROPERTY_HINT_ENUM, "Default,Breeze Dark,Godot 2,Grey,Light,Solarized (Dark),Solarized (Light),Custom", PROPERTY_USAGE_DEFAULT);
	_initial_set("interface/theme/icon_and_font_color", 0);
	hints["interface/theme/icon_and_font_color"] = PropertyInfo(Variant::INT, "interface/theme/icon_and_font_color", PROPERTY_HINT_ENUM, "Auto,Dark,Light", PROPERTY_USAGE_DEFAULT);
	_initial_set("interface/theme/base_color", Color(0.2, 0.23, 0.31));
	hints["interface/theme/base_color"] = PropertyInfo(Variant::COLOR, "interface/theme/base_color", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT);
	_initial_set("interface/theme/accent_color", Color(0.41, 0.61, 0.91));
	hints["interface/theme/accent_color"] = PropertyInfo(Variant::COLOR, "interface/theme/accent_color", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT);
	_initial_set("interface/theme/contrast", 0.3);
	hints["interface/theme/contrast"] = PropertyInfo(Variant::FLOAT, "interface/theme/contrast", PROPERTY_HINT_RANGE, "-1, 1, 0.01");
	_initial_set("interface/theme/icon_saturation", 1.0);
	hints["interface/theme/icon_saturation"] = PropertyInfo(Variant::FLOAT, "interface/theme/icon_saturation", PROPERTY_HINT_RANGE, "0,2,0.01", PROPERTY_USAGE_DEFAULT);
	_initial_set("interface/theme/relationship_line_opacity", 0.1);
	hints["interface/theme/relationship_line_opacity"] = PropertyInfo(Variant::FLOAT, "interface/theme/relationship_line_opacity", PROPERTY_HINT_RANGE, "0.00, 1, 0.01");
	_initial_set("interface/theme/border_size", 0);
	hints["interface/theme/border_size"] = PropertyInfo(Variant::INT, "interface/theme/border_size", PROPERTY_HINT_RANGE, "0,2,1", PROPERTY_USAGE_DEFAULT);
	_initial_set("interface/theme/corner_radius", 3);
	hints["interface/theme/corner_radius"] = PropertyInfo(Variant::INT, "interface/theme/corner_radius", PROPERTY_HINT_RANGE, "0,6,1", PROPERTY_USAGE_DEFAULT);
	_initial_set("interface/theme/additional_spacing", 0);
	hints["interface/theme/additional_spacing"] = PropertyInfo(Variant::FLOAT, "interface/theme/additional_spacing", PROPERTY_HINT_RANGE, "0,5,0.1", PROPERTY_USAGE_DEFAULT);
	_initial_set("interface/theme/custom_theme", "");
	hints["interface/theme/custom_theme"] = PropertyInfo(Variant::STRING, "interface/theme/custom_theme", PROPERTY_HINT_GLOBAL_FILE, "*.res,*.tres,*.theme", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_RESTART_IF_CHANGED);

	// Scene tabs
	_initial_set("interface/scene_tabs/show_thumbnail_on_hover", true);
	_initial_set("interface/scene_tabs/resize_if_many_tabs", true);
	_initial_set("interface/scene_tabs/minimum_width", 50);
	hints["interface/scene_tabs/minimum_width"] = PropertyInfo(Variant::INT, "interface/scene_tabs/minimum_width", PROPERTY_HINT_RANGE, "50,500,1", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_RESTART_IF_CHANGED);
	_initial_set("interface/scene_tabs/show_script_button", false);

	/* Filesystem */

	// Directories
	_initial_set("filesystem/directories/autoscan_project_path", "");
	hints["filesystem/directories/autoscan_project_path"] = PropertyInfo(Variant::STRING, "filesystem/directories/autoscan_project_path", PROPERTY_HINT_GLOBAL_DIR);
	_initial_set("filesystem/directories/default_project_path", OS::get_singleton()->has_environment("HOME") ? OS::get_singleton()->get_environment("HOME") : OS::get_singleton()->get_system_dir(OS::SYSTEM_DIR_DOCUMENTS));
	hints["filesystem/directories/default_project_path"] = PropertyInfo(Variant::STRING, "filesystem/directories/default_project_path", PROPERTY_HINT_GLOBAL_DIR);

	// On save
	_initial_set("filesystem/on_save/compress_binary_resources", true);
	_initial_set("filesystem/on_save/safe_save_on_backup_then_rename", true);

	// File dialog
	_initial_set("filesystem/file_dialog/show_hidden_files", false);
	_initial_set("filesystem/file_dialog/display_mode", 0);
	hints["filesystem/file_dialog/display_mode"] = PropertyInfo(Variant::INT, "filesystem/file_dialog/display_mode", PROPERTY_HINT_ENUM, "Thumbnails,List");
	_initial_set("filesystem/file_dialog/thumbnail_size", 64);
	hints["filesystem/file_dialog/thumbnail_size"] = PropertyInfo(Variant::INT, "filesystem/file_dialog/thumbnail_size", PROPERTY_HINT_RANGE, "32,128,16");

	/* Docks */

	// SceneTree
	_initial_set("docks/scene_tree/start_create_dialog_fully_expanded", false);

	// FileSystem
	_initial_set("docks/filesystem/thumbnail_size", 64);
	hints["docks/filesystem/thumbnail_size"] = PropertyInfo(Variant::INT, "docks/filesystem/thumbnail_size", PROPERTY_HINT_RANGE, "32,128,16");
	_initial_set("docks/filesystem/always_show_folders", true);

	// Property editor
	_initial_set("docks/property_editor/auto_refresh_interval", 0.2); //update 5 times per second by default
	_initial_set("docks/property_editor/subresource_hue_tint", 0.75);
	hints["docks/property_editor/subresource_hue_tint"] = PropertyInfo(Variant::FLOAT, "docks/property_editor/subresource_hue_tint", PROPERTY_HINT_RANGE, "0,1,0.01", PROPERTY_USAGE_DEFAULT);

	/* Text editor */

	// Theme
	_initial_set("text_editor/theme/color_theme", "Default");
	hints["text_editor/theme/color_theme"] = PropertyInfo(Variant::STRING, "text_editor/theme/color_theme", PROPERTY_HINT_ENUM, "Default,Godot 2,Custom");

	// Theme: Highlighting
	_load_godot2_text_editor_theme();

	// Appearance
	// Appearance: Caret
	_initial_set("text_editor/appearance/caret/type", 0);
	hints["text_editor/appearance/caret/type"] = PropertyInfo(Variant::INT, "text_editor/appearance/caret/type", PROPERTY_HINT_ENUM, "Line,Block");
	_initial_set("text_editor/appearance/caret/caret_blink", true);
	_initial_set("text_editor/appearance/caret/caret_blink_speed", 0.5);
	hints["text_editor/appearance/caret/caret_blink_speed"] = PropertyInfo(Variant::FLOAT, "text_editor/appearance/caret/caret_blink_speed", PROPERTY_HINT_RANGE, "0.1, 10, 0.01");
	_initial_set("text_editor/appearance/caret/highlight_current_line", true);
	_initial_set("text_editor/appearance/caret/highlight_all_occurrences", true);

	// Appearance: Guidelines
	_initial_set("text_editor/appearance/guidelines/show_line_length_guidelines", true);
	_initial_set("text_editor/appearance/guidelines/line_length_guideline_soft_column", 80);
	hints["text_editor/appearance/guidelines/line_length_guideline_soft_column"] = PropertyInfo(Variant::INT, "text_editor/appearance/guidelines/line_length_guideline_soft_column", PROPERTY_HINT_RANGE, "20, 160, 1");
	_initial_set("text_editor/appearance/guidelines/line_length_guideline_hard_column", 100);
	hints["text_editor/appearance/guidelines/line_length_guideline_hard_column"] = PropertyInfo(Variant::INT, "text_editor/appearance/guidelines/line_length_guideline_hard_column", PROPERTY_HINT_RANGE, "20, 160, 1");

	// Appearance: Gutters
	_initial_set("text_editor/appearance/gutters/show_line_numbers", true);
	_initial_set("text_editor/appearance/gutters/line_numbers_zero_padded", false);
	_initial_set("text_editor/appearance/gutters/highlight_type_safe_lines", true);
	_initial_set("text_editor/appearance/gutters/show_bookmark_gutter", true);
	_initial_set("text_editor/appearance/gutters/show_info_gutter", true);

	// Appearance: Minimap
	_initial_set("text_editor/appearance/minimap/show_minimap", true);
	_initial_set("text_editor/appearance/minimap/minimap_width", 80);
	hints["text_editor/appearance/minimap/minimap_width"] = PropertyInfo(Variant::INT, "text_editor/appearance/minimap/minimap_width", PROPERTY_HINT_RANGE, "50,250,1");

	// Appearance: Lines
	_initial_set("text_editor/appearance/lines/code_folding", true);
	_initial_set("text_editor/appearance/lines/word_wrap", 0);
	hints["text_editor/appearance/lines/word_wrap"] = PropertyInfo(Variant::INT, "text_editor/appearance/lines/word_wrap", PROPERTY_HINT_ENUM, "None,Boundary");

	// Appearance: Whitespace
	_initial_set("text_editor/appearance/whitespace/draw_tabs", true);
	_initial_set("text_editor/appearance/whitespace/draw_spaces", false);
	_initial_set("text_editor/appearance/whitespace/line_spacing", 6);
	hints["text_editor/appearance/whitespace/line_spacing"] = PropertyInfo(Variant::INT, "text_editor/appearance/whitespace/line_spacing", PROPERTY_HINT_RANGE, "0,50,1");

	// Behavior
	// Behavior: Navigation
	_initial_set("text_editor/behavior/navigation/move_caret_on_right_click", true);
	_initial_set("text_editor/behavior/navigation/scroll_past_end_of_file", false);
	_initial_set("text_editor/behavior/navigation/smooth_scrolling", true);
	_initial_set("text_editor/behavior/navigation/v_scroll_speed", 80);

	// Behavior: Indent
	_initial_set("text_editor/behavior/indent/type", 0);
	hints["text_editor/behavior/indent/type"] = PropertyInfo(Variant::INT, "text_editor/behavior/indent/type", PROPERTY_HINT_ENUM, "Tabs,Spaces");
	_initial_set("text_editor/behavior/indent/size", 4);
	hints["text_editor/behavior/indent/size"] = PropertyInfo(Variant::INT, "text_editor/behavior/indent/size", PROPERTY_HINT_RANGE, "1, 64, 1"); // size of 0 crashes.
	_initial_set("text_editor/behavior/indent/auto_indent", true);

	// Behavior: Files
	_initial_set("text_editor/behavior/files/trim_trailing_whitespace_on_save", false);
	_initial_set("text_editor/behavior/files/autosave_interval_secs", 0);
	_initial_set("text_editor/behavior/files/restore_scripts_on_load", true);
	_initial_set("text_editor/behavior/files/convert_indent_on_save", true);

	// Script list
	_initial_set("text_editor/script_list/show_members_overview", true);
	_initial_set("text_editor/script_list/sort_members_outline_alphabetically", false);

	// Completion
	_initial_set("text_editor/completion/idle_parse_delay", 2.0);
	hints["text_editor/completion/idle_parse_delay"] = PropertyInfo(Variant::FLOAT, "text_editor/completion/idle_parse_delay", PROPERTY_HINT_RANGE, "0.1, 10, 0.01");
	_initial_set("text_editor/completion/auto_brace_complete", true);
	_initial_set("text_editor/completion/code_complete_delay", 0.3);
	hints["text_editor/completion/code_complete_delay"] = PropertyInfo(Variant::FLOAT, "text_editor/completion/code_complete_delay", PROPERTY_HINT_RANGE, "0.01, 5, 0.01");
	_initial_set("text_editor/completion/put_callhint_tooltip_below_current_line", true);
	_initial_set("text_editor/completion/complete_file_paths", true);
	_initial_set("text_editor/completion/add_type_hints", false);
	_initial_set("text_editor/completion/use_single_quotes", false);

	// Help
	_initial_set("text_editor/help/show_help_index", true);
	_initial_set("text_editor/help/help_font_size", 15);
	hints["text_editor/help/help_font_size"] = PropertyInfo(Variant::INT, "text_editor/help/help_font_size", PROPERTY_HINT_RANGE, "8,48,1");
	_initial_set("text_editor/help/help_source_font_size", 14);
	hints["text_editor/help/help_source_font_size"] = PropertyInfo(Variant::INT, "text_editor/help/help_source_font_size", PROPERTY_HINT_RANGE, "8,48,1");
	_initial_set("text_editor/help/help_title_font_size", 23);
	hints["text_editor/help/help_title_font_size"] = PropertyInfo(Variant::INT, "text_editor/help/help_title_font_size", PROPERTY_HINT_RANGE, "8,48,1");
	_initial_set("text_editor/help/class_reference_examples", 0);
	hints["text_editor/help/class_reference_examples"] = PropertyInfo(Variant::INT, "text_editor/help/class_reference_examples", PROPERTY_HINT_ENUM, "GDScript,C#,GDScript and C#");

	/* Editors */

	// GridMap
	_initial_set("editors/grid_map/pick_distance", 5000.0);

	// 3D
	_initial_set("editors/3d/primary_grid_color", Color(0.56, 0.56, 0.56, 0.5));
	hints["editors/3d/primary_grid_color"] = PropertyInfo(Variant::COLOR, "editors/3d/primary_grid_color", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT);

	_initial_set("editors/3d/secondary_grid_color", Color(0.38, 0.38, 0.38, 0.5));
	hints["editors/3d/secondary_grid_color"] = PropertyInfo(Variant::COLOR, "editors/3d/secondary_grid_color", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT);

	// Use a similar color to the 2D editor selection.
	_initial_set("editors/3d/selection_box_color", Color(1.0, 0.5, 0));
	hints["editors/3d/selection_box_color"] = PropertyInfo(Variant::COLOR, "editors/3d/selection_box_color", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_RESTART_IF_CHANGED);

	// If a line is a multiple of this, it uses the primary grid color.
	// Use a power of 2 value by default as it's more common to use powers of 2 in level design.
	_initial_set("editors/3d/primary_grid_steps", 8);
	hints["editors/3d/primary_grid_steps"] = PropertyInfo(Variant::INT, "editors/3d/primary_grid_steps", PROPERTY_HINT_RANGE, "1,100,1", PROPERTY_USAGE_DEFAULT);

	// At 1000, the grid mostly looks like it has no edge.
	_initial_set("editors/3d/grid_size", 200);
	hints["editors/3d/grid_size"] = PropertyInfo(Variant::INT, "editors/3d/grid_size", PROPERTY_HINT_RANGE, "1,2000,1", PROPERTY_USAGE_DEFAULT);

	// Default largest grid size is 100m, 10^2 (primary grid lines are 1km apart when primary_grid_steps is 10).
	_initial_set("editors/3d/grid_division_level_max", 2);
	// Higher values produce graphical artifacts when far away unless View Z-Far
	// is increased significantly more than it really should need to be.
	hints["editors/3d/grid_division_level_max"] = PropertyInfo(Variant::INT, "editors/3d/grid_division_level_max", PROPERTY_HINT_RANGE, "-1,3,1", PROPERTY_USAGE_DEFAULT);

	// Default smallest grid size is 1m, 10^0.
	_initial_set("editors/3d/grid_division_level_min", 0);
	// Lower values produce graphical artifacts regardless of view clipping planes, so limit to -2 as a lower bound.
	hints["editors/3d/grid_division_level_min"] = PropertyInfo(Variant::INT, "editors/3d/grid_division_level_min", PROPERTY_HINT_RANGE, "-2,2,1", PROPERTY_USAGE_DEFAULT);

	// -0.2 seems like a sensible default. -1.0 gives Blender-like behavior, 0.5 gives huge grids.
	_initial_set("editors/3d/grid_division_level_bias", -0.2);
	hints["editors/3d/grid_division_level_bias"] = PropertyInfo(Variant::FLOAT, "editors/3d/grid_division_level_bias", PROPERTY_HINT_RANGE, "-1.0,0.5,0.1", PROPERTY_USAGE_DEFAULT);

	_initial_set("editors/3d/grid_xz_plane", true);
	_initial_set("editors/3d/grid_xy_plane", false);
	_initial_set("editors/3d/grid_yz_plane", false);

	// Use a lower default FOV for the 3D camera compared to the
	// Camera3D node as the 3D viewport doesn't span the whole screen.
	// This means it's technically viewed from a further distance, which warrants a narrower FOV.
	_initial_set("editors/3d/default_fov", 70.0);
	hints["editors/3d/default_fov"] = PropertyInfo(Variant::FLOAT, "editors/3d/default_fov", PROPERTY_HINT_RANGE, "1,179,0.1");
	_initial_set("editors/3d/default_z_near", 0.05);
	hints["editors/3d/default_z_near"] = PropertyInfo(Variant::FLOAT, "editors/3d/default_z_near", PROPERTY_HINT_RANGE, "0.01,10,0.01,or_greater");
	_initial_set("editors/3d/default_z_far", 4000.0);
	hints["editors/3d/default_z_far"] = PropertyInfo(Variant::FLOAT, "editors/3d/default_z_far", PROPERTY_HINT_RANGE, "0.1,4000,0.1,or_greater");

	// 3D: Navigation
	_initial_set("editors/3d/navigation/navigation_scheme", 0);
	_initial_set("editors/3d/navigation/invert_y_axis", false);
	_initial_set("editors/3d/navigation/invert_x_axis", false);
	hints["editors/3d/navigation/navigation_scheme"] = PropertyInfo(Variant::INT, "editors/3d/navigation/navigation_scheme", PROPERTY_HINT_ENUM, "Godot,Maya,Modo");
	_initial_set("editors/3d/navigation/zoom_style", 0);
	hints["editors/3d/navigation/zoom_style"] = PropertyInfo(Variant::INT, "editors/3d/navigation/zoom_style", PROPERTY_HINT_ENUM, "Vertical, Horizontal");

	_initial_set("editors/3d/navigation/emulate_numpad", false);
	_initial_set("editors/3d/navigation/emulate_3_button_mouse", false);
	_initial_set("editors/3d/navigation/orbit_modifier", 0);
	hints["editors/3d/navigation/orbit_modifier"] = PropertyInfo(Variant::INT, "editors/3d/navigation/orbit_modifier", PROPERTY_HINT_ENUM, "None,Shift,Alt,Meta,Ctrl");
	_initial_set("editors/3d/navigation/pan_modifier", 1);
	hints["editors/3d/navigation/pan_modifier"] = PropertyInfo(Variant::INT, "editors/3d/navigation/pan_modifier", PROPERTY_HINT_ENUM, "None,Shift,Alt,Meta,Ctrl");
	_initial_set("editors/3d/navigation/zoom_modifier", 4);
	hints["editors/3d/navigation/zoom_modifier"] = PropertyInfo(Variant::INT, "editors/3d/navigation/zoom_modifier", PROPERTY_HINT_ENUM, "None,Shift,Alt,Meta,Ctrl");
	_initial_set("editors/3d/navigation/warped_mouse_panning", true);

	// 3D: Navigation feel
	_initial_set("editors/3d/navigation_feel/orbit_sensitivity", 0.4);
	hints["editors/3d/navigation_feel/orbit_sensitivity"] = PropertyInfo(Variant::FLOAT, "editors/3d/navigation_feel/orbit_sensitivity", PROPERTY_HINT_RANGE, "0.0, 2, 0.01");
	_initial_set("editors/3d/navigation_feel/orbit_inertia", 0.05);
	hints["editors/3d/navigation_feel/orbit_inertia"] = PropertyInfo(Variant::FLOAT, "editors/3d/navigation_feel/orbit_inertia", PROPERTY_HINT_RANGE, "0.0, 1, 0.01");
	_initial_set("editors/3d/navigation_feel/translation_inertia", 0.15);
	hints["editors/3d/navigation_feel/translation_inertia"] = PropertyInfo(Variant::FLOAT, "editors/3d/navigation_feel/translation_inertia", PROPERTY_HINT_RANGE, "0.0, 1, 0.01");
	_initial_set("editors/3d/navigation_feel/zoom_inertia", 0.075);
	hints["editors/3d/navigation_feel/zoom_inertia"] = PropertyInfo(Variant::FLOAT, "editors/3d/navigation_feel/zoom_inertia", PROPERTY_HINT_RANGE, "0.0, 1, 0.01");
	_initial_set("editors/3d/navigation_feel/manipulation_orbit_inertia", 0.075);
	hints["editors/3d/navigation_feel/manipulation_orbit_inertia"] = PropertyInfo(Variant::FLOAT, "editors/3d/navigation_feel/manipulation_orbit_inertia", PROPERTY_HINT_RANGE, "0.0, 1, 0.01");
	_initial_set("editors/3d/navigation_feel/manipulation_translation_inertia", 0.075);
	hints["editors/3d/navigation_feel/manipulation_translation_inertia"] = PropertyInfo(Variant::FLOAT, "editors/3d/navigation_feel/manipulation_translation_inertia", PROPERTY_HINT_RANGE, "0.0, 1, 0.01");

	// 3D: Freelook
	_initial_set("editors/3d/freelook/freelook_navigation_scheme", false);
	hints["editors/3d/freelook/freelook_navigation_scheme"] = PropertyInfo(Variant::INT, "editors/3d/freelook/freelook_navigation_scheme", PROPERTY_HINT_ENUM, "Default,Partially Axis-Locked (id Tech),Fully Axis-Locked (Minecraft)");
	_initial_set("editors/3d/freelook/freelook_sensitivity", 0.4);
	hints["editors/3d/freelook/freelook_sensitivity"] = PropertyInfo(Variant::FLOAT, "editors/3d/freelook/freelook_sensitivity", PROPERTY_HINT_RANGE, "0.0, 2, 0.01");
	_initial_set("editors/3d/freelook/freelook_inertia", 0.1);
	hints["editors/3d/freelook/freelook_inertia"] = PropertyInfo(Variant::FLOAT, "editors/3d/freelook/freelook_inertia", PROPERTY_HINT_RANGE, "0.0, 1, 0.01");
	_initial_set("editors/3d/freelook/freelook_base_speed", 5.0);
	hints["editors/3d/freelook/freelook_base_speed"] = PropertyInfo(Variant::FLOAT, "editors/3d/freelook/freelook_base_speed", PROPERTY_HINT_RANGE, "0.0, 10, 0.01");
	_initial_set("editors/3d/freelook/freelook_activation_modifier", 0);
	hints["editors/3d/freelook/freelook_activation_modifier"] = PropertyInfo(Variant::INT, "editors/3d/freelook/freelook_activation_modifier", PROPERTY_HINT_ENUM, "None,Shift,Alt,Meta,Ctrl");
	_initial_set("editors/3d/freelook/freelook_speed_zoom_link", false);

	// 2D
	_initial_set("editors/2d/grid_color", Color(1.0, 1.0, 1.0, 0.07));
	_initial_set("editors/2d/guides_color", Color(0.6, 0.0, 0.8));
	_initial_set("editors/2d/smart_snapping_line_color", Color(0.9, 0.1, 0.1));
	_initial_set("editors/2d/bone_width", 5);
	_initial_set("editors/2d/bone_color1", Color(1.0, 1.0, 1.0, 0.7));
	_initial_set("editors/2d/bone_color2", Color(0.6, 0.6, 0.6, 0.7));
	_initial_set("editors/2d/bone_selected_color", Color(0.9, 0.45, 0.45, 0.7));
	_initial_set("editors/2d/bone_ik_color", Color(0.9, 0.9, 0.45, 0.7));
	_initial_set("editors/2d/bone_outline_color", Color(0.35, 0.35, 0.35, 0.5));
	_initial_set("editors/2d/bone_outline_size", 2);
	_initial_set("editors/2d/viewport_border_color", Color(0.4, 0.4, 1.0, 0.4));
	_initial_set("editors/2d/constrain_editor_view", true);
	_initial_set("editors/2d/warped_mouse_panning", true);
	_initial_set("editors/2d/simple_panning", false);
	_initial_set("editors/2d/scroll_to_pan", false);
	_initial_set("editors/2d/pan_speed", 20);

	// Tiles editor
	_initial_set("editors/tiles_editor/display_grid", true);
	_initial_set("editors/tiles_editor/grid_color", Color(1.0, 0.5, 0.2, 0.5));

	// Polygon editor
	_initial_set("editors/polygon_editor/point_grab_radius", 8);
	_initial_set("editors/polygon_editor/show_previous_outline", true);

	// Animation
	_initial_set("editors/animation/autorename_animation_tracks", true);
	_initial_set("editors/animation/confirm_insert_track", true);
	_initial_set("editors/animation/default_create_bezier_tracks", false);
	_initial_set("editors/animation/default_create_reset_tracks", true);
	_initial_set("editors/animation/onion_layers_past_color", Color(1, 0, 0));
	_initial_set("editors/animation/onion_layers_future_color", Color(0, 1, 0));

	// Visual editors
	_initial_set("editors/visual_editors/minimap_opacity", 0.85);
	hints["editors/visual_editors/minimap_opacity"] = PropertyInfo(Variant::FLOAT, "editors/visual_editors/minimap_opacity", PROPERTY_HINT_RANGE, "0.0,1.0,0.01", PROPERTY_USAGE_DEFAULT);

	/* Run */

	// Window placement
	_initial_set("run/window_placement/rect", 1);
	hints["run/window_placement/rect"] = PropertyInfo(Variant::INT, "run/window_placement/rect", PROPERTY_HINT_ENUM, "Top Left,Centered,Custom Position,Force Maximized,Force Fullscreen");
	String screen_hints = "Same as Editor,Previous Monitor,Next Monitor";
	for (int i = 0; i < DisplayServer::get_singleton()->get_screen_count(); i++) {
		screen_hints += ",Monitor " + itos(i + 1);
	}
	_initial_set("run/window_placement/rect_custom_position", Vector2());
	_initial_set("run/window_placement/screen", 0);
	hints["run/window_placement/screen"] = PropertyInfo(Variant::INT, "run/window_placement/screen", PROPERTY_HINT_ENUM, screen_hints);

	// Auto save
	_initial_set("run/auto_save/save_before_running", true);

	// Output
	_initial_set("run/output/font_size", 13);
	hints["run/output/font_size"] = PropertyInfo(Variant::INT, "run/output/font_size", PROPERTY_HINT_RANGE, "8,48,1");
	_initial_set("run/output/always_clear_output_on_play", true);
	_initial_set("run/output/always_open_output_on_play", true);
	_initial_set("run/output/always_close_output_on_stop", false);

	/* Network */

	// Debug
	_initial_set("network/debug/remote_host", "127.0.0.1"); // Hints provided in setup_network

	_initial_set("network/debug/remote_port", 6007);
	hints["network/debug/remote_port"] = PropertyInfo(Variant::INT, "network/debug/remote_port", PROPERTY_HINT_RANGE, "1,65535,1");

	// SSL
	_initial_set("network/ssl/editor_ssl_certificates", _SYSTEM_CERTS_PATH);
	hints["network/ssl/editor_ssl_certificates"] = PropertyInfo(Variant::STRING, "network/ssl/editor_ssl_certificates", PROPERTY_HINT_GLOBAL_FILE, "*.crt,*.pem");

	/* Extra config */

	_initial_set("project_manager/sorting_order", 0);
	hints["project_manager/sorting_order"] = PropertyInfo(Variant::INT, "project_manager/sorting_order", PROPERTY_HINT_ENUM, "Name,Path,Last Edited");

	if (p_extra_config.is_valid()) {
		if (p_extra_config->has_section("init_projects") && p_extra_config->has_section_key("init_projects", "list")) {
			Vector<String> list = p_extra_config->get_value("init_projects", "list");
			for (int i = 0; i < list.size(); i++) {
				String name = list[i].replace("/", "::");
				set("projects/" + name, list[i]);
			}
		}

		if (p_extra_config->has_section("presets")) {
			List<String> keys;
			p_extra_config->get_section_keys("presets", &keys);

			for (const String &key : keys) {
				Variant val = p_extra_config->get_value("presets", key);
				set(key, val);
			}
		}
	}
}

void EditorSettings::_load_godot2_text_editor_theme() {
	// Godot 2 is only a dark theme; it doesn't have a light theme counterpart.
	_initial_set("text_editor/theme/highlighting/symbol_color", Color(0.73, 0.87, 1.0));
	_initial_set("text_editor/theme/highlighting/keyword_color", Color(1.0, 1.0, 0.7));
	_initial_set("text_editor/theme/highlighting/control_flow_keyword_color", Color(1.0, 0.85, 0.7));
	_initial_set("text_editor/theme/highlighting/base_type_color", Color(0.64, 1.0, 0.83));
	_initial_set("text_editor/theme/highlighting/engine_type_color", Color(0.51, 0.83, 1.0));
	_initial_set("text_editor/theme/highlighting/user_type_color", Color(0.42, 0.67, 0.93));
	_initial_set("text_editor/theme/highlighting/comment_color", Color(0.4, 0.4, 0.4));
	_initial_set("text_editor/theme/highlighting/string_color", Color(0.94, 0.43, 0.75));
	_initial_set("text_editor/theme/highlighting/background_color", Color(0.13, 0.12, 0.15));
	_initial_set("text_editor/theme/highlighting/completion_background_color", Color(0.17, 0.16, 0.2));
	_initial_set("text_editor/theme/highlighting/completion_selected_color", Color(0.26, 0.26, 0.27));
	_initial_set("text_editor/theme/highlighting/completion_existing_color", Color(0.13, 0.87, 0.87, 0.87));
	_initial_set("text_editor/theme/highlighting/completion_scroll_color", Color(1, 1, 1));
	_initial_set("text_editor/theme/highlighting/completion_font_color", Color(0.67, 0.67, 0.67));
	_initial_set("text_editor/theme/highlighting/text_color", Color(0.67, 0.67, 0.67));
	_initial_set("text_editor/theme/highlighting/line_number_color", Color(0.67, 0.67, 0.67, 0.4));
	_initial_set("text_editor/theme/highlighting/safe_line_number_color", Color(0.67, 0.78, 0.67, 0.6));
	_initial_set("text_editor/theme/highlighting/caret_color", Color(0.67, 0.67, 0.67));
	_initial_set("text_editor/theme/highlighting/caret_background_color", Color(0, 0, 0));
	_initial_set("text_editor/theme/highlighting/text_selected_color", Color(0, 0, 0));
	_initial_set("text_editor/theme/highlighting/selection_color", Color(0.41, 0.61, 0.91, 0.35));
	_initial_set("text_editor/theme/highlighting/brace_mismatch_color", Color(1, 0.2, 0.2));
	_initial_set("text_editor/theme/highlighting/current_line_color", Color(0.3, 0.5, 0.8, 0.15));
	_initial_set("text_editor/theme/highlighting/line_length_guideline_color", Color(0.3, 0.5, 0.8, 0.1));
	_initial_set("text_editor/theme/highlighting/word_highlighted_color", Color(0.8, 0.9, 0.9, 0.15));
	_initial_set("text_editor/theme/highlighting/number_color", Color(0.92, 0.58, 0.2));
	_initial_set("text_editor/theme/highlighting/function_color", Color(0.4, 0.64, 0.81));
	_initial_set("text_editor/theme/highlighting/member_variable_color", Color(0.9, 0.31, 0.35));
	_initial_set("text_editor/theme/highlighting/mark_color", Color(1.0, 0.4, 0.4, 0.4));
	_initial_set("text_editor/theme/highlighting/bookmark_color", Color(0.08, 0.49, 0.98));
	_initial_set("text_editor/theme/highlighting/breakpoint_color", Color(0.9, 0.29, 0.3));
	_initial_set("text_editor/theme/highlighting/executing_line_color", Color(0.98, 0.89, 0.27));
	_initial_set("text_editor/theme/highlighting/code_folding_color", Color(0.8, 0.8, 0.8, 0.8));
	_initial_set("text_editor/theme/highlighting/search_result_color", Color(0.05, 0.25, 0.05, 1));
	_initial_set("text_editor/theme/highlighting/search_result_border_color", Color(0.41, 0.61, 0.91, 0.38));
}

bool EditorSettings::_save_text_editor_theme(String p_file) {
	String theme_section = "color_theme";
	Ref<ConfigFile> cf = memnew(ConfigFile); // hex is better?

	List<String> keys;
	props.get_key_list(&keys);
	keys.sort();

	for (const String &key : keys) {
		if (key.begins_with("text_editor/theme/highlighting/") && key.find("color") >= 0) {
			cf->set_value(theme_section, key.replace("text_editor/theme/highlighting/", ""), ((Color)props[key].variant).to_html());
		}
	}

	Error err = cf->save(p_file);

	return err == OK;
}

bool EditorSettings::_is_default_text_editor_theme(String p_theme_name) {
	return p_theme_name == "default" || p_theme_name == "godot 2" || p_theme_name == "custom";
}

static Dictionary _get_builtin_script_templates() {
	Dictionary templates;

	// No Comments
	templates["no_comments.gd"] =
			"extends %BASE%\n"
			"\n"
			"\n"
			"func _ready()%VOID_RETURN%:\n"
			"%TS%pass\n";

	// Empty
	templates["empty.gd"] =
			"extends %BASE%"
			"\n"
			"\n";

	return templates;
}

static void _create_script_templates(const String &p_path) {
	Dictionary templates = _get_builtin_script_templates();
	List<Variant> keys;
	templates.get_key_list(&keys);
	FileAccessRef file = FileAccess::create(FileAccess::ACCESS_FILESYSTEM);
	DirAccessRef dir = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	dir->change_dir(p_path);
	for (int i = 0; i < keys.size(); i++) {
		if (!dir->file_exists(keys[i])) {
			Error err = file->reopen(p_path.plus_file((String)keys[i]), FileAccess::WRITE);
			ERR_FAIL_COND(err != OK);
			file->store_string(templates[keys[i]]);
			file->close();
		}
	}
}

// PUBLIC METHODS

EditorSettings *EditorSettings::get_singleton() {
	return singleton.ptr();
}

void EditorSettings::create() {
	// IMPORTANT: create() *must* create a valid EditorSettings singleton,
	// as the rest of the engine code will assume it. As such, it should never
	// return (incl. via ERR_FAIL) without initializing the singleton member.

	if (singleton.ptr()) {
		ERR_PRINT("Can't recreate EditorSettings as it already exists.");
		return;
	}

	GDREGISTER_CLASS(EditorSettings); // Otherwise it can't be unserialized.

	String config_file_path;
	Ref<ConfigFile> extra_config = memnew(ConfigFile);

	if (!EditorPaths::get_singleton()) {
		ERR_PRINT("Bug (please report): EditorPaths haven't been initialized, EditorSettings cannot be created properly.");
		goto fail;
	}

	if (EditorPaths::get_singleton()->is_self_contained()) {
		Error err = extra_config->load(EditorPaths::get_singleton()->get_self_contained_file());
		if (err != OK) {
			ERR_PRINT("Can't load extra config from path: " + EditorPaths::get_singleton()->get_self_contained_file());
		}
	}

	if (EditorPaths::get_singleton()->are_paths_valid()) {
		_create_script_templates(EditorPaths::get_singleton()->get_config_dir().plus_file("script_templates"));

		// Validate editor config file.

		DirAccessRef dir = DirAccess::open(EditorPaths::get_singleton()->get_config_dir());
		String config_file_name = "editor_settings-" + itos(VERSION_MAJOR) + ".tres";
		config_file_path = EditorPaths::get_singleton()->get_config_dir().plus_file(config_file_name);
		if (!dir->file_exists(config_file_name)) {
			goto fail;
		}

		singleton = ResourceLoader::load(config_file_path, "EditorSettings");

		if (singleton.is_null()) {
			ERR_PRINT("Could not load editor settings from path: " + config_file_path);
			goto fail;
		}

		singleton->save_changed_setting = true;
		singleton->config_file_path = config_file_path;

		print_verbose("EditorSettings: Load OK!");

		singleton->setup_language();
		singleton->setup_network();
		singleton->load_favorites();
		singleton->list_text_editor_themes();

		return;
	}

fail:
	// patch init projects
	String exe_path = OS::get_singleton()->get_executable_path().get_base_dir();

	if (extra_config->has_section("init_projects")) {
		Vector<String> list = extra_config->get_value("init_projects", "list");
		for (int i = 0; i < list.size(); i++) {
			list.write[i] = exe_path.plus_file(list[i]);
		}
		extra_config->set_value("init_projects", "list", list);
	}

	singleton = Ref<EditorSettings>(memnew(EditorSettings));
	singleton->save_changed_setting = true;
	singleton->config_file_path = config_file_path;
	singleton->_load_defaults(extra_config);
	singleton->setup_language();
	singleton->setup_network();
	singleton->list_text_editor_themes();
}

void EditorSettings::setup_language() {
	TranslationServer::get_singleton()->set_editor_pseudolocalization(get("interface/editor/enable_debugging_pseudolocalization"));
	String lang = get("interface/editor/editor_language");
	if (lang == "en") {
		return; // Default, nothing to do.
	}
	// Load editor translation for configured/detected locale.
	EditorTranslationList *etl = _editor_translations;
	while (etl->data) {
		if (etl->lang == lang) {
			Vector<uint8_t> data;
			data.resize(etl->uncomp_size);
			Compression::decompress(data.ptrw(), etl->uncomp_size, etl->data, etl->comp_size, Compression::MODE_DEFLATE);

			FileAccessMemory *fa = memnew(FileAccessMemory);
			fa->open_custom(data.ptr(), data.size());

			Ref<Translation> tr = TranslationLoaderPO::load_translation(fa);

			if (tr.is_valid()) {
				tr->set_locale(etl->lang);
				TranslationServer::get_singleton()->set_tool_translation(tr);
				break;
			}
		}

		etl++;
	}

	// Load class reference translation.
	DocTranslationList *dtl = _doc_translations;
	while (dtl->data) {
		if (dtl->lang == lang) {
			Vector<uint8_t> data;
			data.resize(dtl->uncomp_size);
			Compression::decompress(data.ptrw(), dtl->uncomp_size, dtl->data, dtl->comp_size, Compression::MODE_DEFLATE);

			FileAccessMemory *fa = memnew(FileAccessMemory);
			fa->open_custom(data.ptr(), data.size());

			Ref<Translation> tr = TranslationLoaderPO::load_translation(fa);

			if (tr.is_valid()) {
				tr->set_locale(dtl->lang);
				TranslationServer::get_singleton()->set_doc_translation(tr);
				break;
			}
		}

		dtl++;
	}
}

void EditorSettings::setup_network() {
	List<IPAddress> local_ip;
	IP::get_singleton()->get_local_addresses(&local_ip);
	String hint;
	String current = has_setting("network/debug/remote_host") ? get("network/debug/remote_host") : "";
	String selected = "127.0.0.1";

	// Check that current remote_host is a valid interface address and populate hints.
	for (const IPAddress &ip : local_ip) {
		// link-local IPv6 addresses don't work, skipping them
		if (String(ip).begins_with("fe80:0:0:0:")) { // fe80::/64
			continue;
		}
		// Same goes for IPv4 link-local (APIPA) addresses.
		if (String(ip).begins_with("169.254.")) { // 169.254.0.0/16
			continue;
		}
		// Select current IP (found)
		if (ip == current) {
			selected = ip;
		}
		if (hint != "") {
			hint += ",";
		}
		hint += ip;
	}

	// Add hints with valid IP addresses to remote_host property.
	add_property_hint(PropertyInfo(Variant::STRING, "network/debug/remote_host", PROPERTY_HINT_ENUM, hint));
	// Fix potentially invalid remote_host due to network change.
	set("network/debug/remote_host", selected);
}

void EditorSettings::save() {
	//_THREAD_SAFE_METHOD_

	if (!singleton.ptr()) {
		return;
	}

	if (singleton->config_file_path == "") {
		ERR_PRINT("Cannot save EditorSettings config, no valid path");
		return;
	}

	Error err = ResourceSaver::save(singleton->config_file_path, singleton);

	if (err != OK) {
		ERR_PRINT("Error saving editor settings to " + singleton->config_file_path);
	} else {
		print_verbose("EditorSettings: Save OK!");
	}
}

void EditorSettings::destroy() {
	if (!singleton.ptr()) {
		return;
	}
	save();
	singleton = Ref<EditorSettings>();
}

void EditorSettings::set_optimize_save(bool p_optimize) {
	optimize_save = p_optimize;
}

// Properties

void EditorSettings::set_setting(const String &p_setting, const Variant &p_value) {
	_THREAD_SAFE_METHOD_
	set(p_setting, p_value);
}

Variant EditorSettings::get_setting(const String &p_setting) const {
	_THREAD_SAFE_METHOD_
	return get(p_setting);
}

bool EditorSettings::has_setting(const String &p_setting) const {
	_THREAD_SAFE_METHOD_

	return props.has(p_setting);
}

void EditorSettings::erase(const String &p_setting) {
	_THREAD_SAFE_METHOD_

	props.erase(p_setting);
}

void EditorSettings::raise_order(const String &p_setting) {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND(!props.has(p_setting));
	props[p_setting].order = ++last_order;
}

void EditorSettings::set_restart_if_changed(const StringName &p_setting, bool p_restart) {
	_THREAD_SAFE_METHOD_

	if (!props.has(p_setting)) {
		return;
	}
	props[p_setting].restart_if_changed = p_restart;
}

void EditorSettings::set_initial_value(const StringName &p_setting, const Variant &p_value, bool p_update_current) {
	_THREAD_SAFE_METHOD_

	if (!props.has(p_setting)) {
		return;
	}
	props[p_setting].initial = p_value;
	props[p_setting].has_default_value = true;
	if (p_update_current) {
		set(p_setting, p_value);
	}
}

Variant _EDITOR_DEF(const String &p_setting, const Variant &p_default, bool p_restart_if_changed) {
	Variant ret = p_default;
	if (EditorSettings::get_singleton()->has_setting(p_setting)) {
		ret = EditorSettings::get_singleton()->get(p_setting);
	} else {
		EditorSettings::get_singleton()->set_manually(p_setting, p_default);
		EditorSettings::get_singleton()->set_restart_if_changed(p_setting, p_restart_if_changed);
	}

	if (!EditorSettings::get_singleton()->has_default_value(p_setting)) {
		EditorSettings::get_singleton()->set_initial_value(p_setting, p_default);
	}

	return ret;
}

Variant _EDITOR_GET(const String &p_setting) {
	ERR_FAIL_COND_V(!EditorSettings::get_singleton()->has_setting(p_setting), Variant());
	return EditorSettings::get_singleton()->get(p_setting);
}

bool EditorSettings::property_can_revert(const String &p_setting) {
	if (!props.has(p_setting)) {
		return false;
	}

	if (!props[p_setting].has_default_value) {
		return false;
	}

	return props[p_setting].initial != props[p_setting].variant;
}

Variant EditorSettings::property_get_revert(const String &p_setting) {
	if (!props.has(p_setting) || !props[p_setting].has_default_value) {
		return Variant();
	}

	return props[p_setting].initial;
}

void EditorSettings::add_property_hint(const PropertyInfo &p_hint) {
	_THREAD_SAFE_METHOD_

	hints[p_hint.name] = p_hint;
}

// Editor data and config directories
// EditorPaths::create() is responsible for the creation of these directories.

String EditorSettings::get_templates_dir() const {
	return EditorPaths::get_singleton()->get_data_dir().plus_file("templates");
}

String EditorSettings::get_project_settings_dir() const {
	return EditorPaths::get_singleton()->get_project_data_dir().plus_file("editor");
}

String EditorSettings::get_text_editor_themes_dir() const {
	return EditorPaths::get_singleton()->get_config_dir().plus_file("text_editor_themes");
}

String EditorSettings::get_script_templates_dir() const {
	return EditorPaths::get_singleton()->get_config_dir().plus_file("script_templates");
}

String EditorSettings::get_project_script_templates_dir() const {
	return ProjectSettings::get_singleton()->get("editor/script/templates_search_path");
}

String EditorSettings::get_feature_profiles_dir() const {
	return EditorPaths::get_singleton()->get_config_dir().plus_file("feature_profiles");
}

// Metadata

void EditorSettings::set_project_metadata(const String &p_section, const String &p_key, Variant p_data) {
	Ref<ConfigFile> cf = memnew(ConfigFile);
	String path = get_project_settings_dir().plus_file("project_metadata.cfg");
	Error err;
	err = cf->load(path);
	ERR_FAIL_COND_MSG(err != OK && err != ERR_FILE_NOT_FOUND, "Cannot load editor settings from file '" + path + "'.");
	cf->set_value(p_section, p_key, p_data);
	err = cf->save(path);
	ERR_FAIL_COND_MSG(err != OK, "Cannot save editor settings to file '" + path + "'.");
}

Variant EditorSettings::get_project_metadata(const String &p_section, const String &p_key, Variant p_default) const {
	Ref<ConfigFile> cf = memnew(ConfigFile);
	String path = get_project_settings_dir().plus_file("project_metadata.cfg");
	Error err = cf->load(path);
	if (err != OK) {
		return p_default;
	}
	return cf->get_value(p_section, p_key, p_default);
}

void EditorSettings::set_favorites(const Vector<String> &p_favorites) {
	favorites = p_favorites;
	FileAccess *f = FileAccess::open(get_project_settings_dir().plus_file("favorites"), FileAccess::WRITE);
	if (f) {
		for (int i = 0; i < favorites.size(); i++) {
			f->store_line(favorites[i]);
		}
		memdelete(f);
	}
}

Vector<String> EditorSettings::get_favorites() const {
	return favorites;
}

void EditorSettings::set_recent_dirs(const Vector<String> &p_recent_dirs) {
	recent_dirs = p_recent_dirs;
	FileAccess *f = FileAccess::open(get_project_settings_dir().plus_file("recent_dirs"), FileAccess::WRITE);
	if (f) {
		for (int i = 0; i < recent_dirs.size(); i++) {
			f->store_line(recent_dirs[i]);
		}
		memdelete(f);
	}
}

Vector<String> EditorSettings::get_recent_dirs() const {
	return recent_dirs;
}

void EditorSettings::load_favorites() {
	FileAccess *f = FileAccess::open(get_project_settings_dir().plus_file("favorites"), FileAccess::READ);
	if (f) {
		String line = f->get_line().strip_edges();
		while (line != "") {
			favorites.push_back(line);
			line = f->get_line().strip_edges();
		}
		memdelete(f);
	}

	f = FileAccess::open(get_project_settings_dir().plus_file("recent_dirs"), FileAccess::READ);
	if (f) {
		String line = f->get_line().strip_edges();
		while (line != "") {
			recent_dirs.push_back(line);
			line = f->get_line().strip_edges();
		}
		memdelete(f);
	}
}

bool EditorSettings::is_dark_theme() {
	int AUTO_COLOR = 0;
	int LIGHT_COLOR = 2;
	Color base_color = get("interface/theme/base_color");
	int icon_font_color_setting = get("interface/theme/icon_and_font_color");
	return (icon_font_color_setting == AUTO_COLOR && ((base_color.r + base_color.g + base_color.b) / 3.0) < 0.5) || icon_font_color_setting == LIGHT_COLOR;
}

void EditorSettings::list_text_editor_themes() {
	String themes = "Default,Godot 2,Custom";

	DirAccess *d = DirAccess::open(get_text_editor_themes_dir());
	if (d) {
		List<String> custom_themes;
		d->list_dir_begin();
		String file = d->get_next();
		while (file != String()) {
			if (file.get_extension() == "tet" && !_is_default_text_editor_theme(file.get_basename().to_lower())) {
				custom_themes.push_back(file.get_basename());
			}
			file = d->get_next();
		}
		d->list_dir_end();
		memdelete(d);

		custom_themes.sort();
		for (const String &E : custom_themes) {
			themes += "," + E;
		}
	}
	add_property_hint(PropertyInfo(Variant::STRING, "text_editor/theme/color_theme", PROPERTY_HINT_ENUM, themes));
}

void EditorSettings::load_text_editor_theme() {
	String p_file = get("text_editor/theme/color_theme");

	if (_is_default_text_editor_theme(p_file.get_file().to_lower())) {
		if (p_file == "Godot 2") {
			_load_godot2_text_editor_theme();
		}
		return; // sorry for "Settings changed" console spam
	}

	String theme_path = get_text_editor_themes_dir().plus_file(p_file + ".tet");

	Ref<ConfigFile> cf = memnew(ConfigFile);
	Error err = cf->load(theme_path);

	if (err != OK) {
		return;
	}

	List<String> keys;
	cf->get_section_keys("color_theme", &keys);

	for (const String &key : keys) {
		String val = cf->get_value("color_theme", key);

		// don't load if it's not already there!
		if (has_setting("text_editor/theme/highlighting/" + key)) {
			// make sure it is actually a color
			if (val.is_valid_html_color() && key.find("color") >= 0) {
				props["text_editor/theme/highlighting/" + key].variant = Color::html(val); // change manually to prevent "Settings changed" console spam
			}
		}
	}
	emit_signal(SNAME("settings_changed"));
	// if it doesn't load just use what is currently loaded
}

bool EditorSettings::import_text_editor_theme(String p_file) {
	if (!p_file.ends_with(".tet")) {
		return false;
	} else {
		if (p_file.get_file().to_lower() == "default.tet") {
			return false;
		}

		DirAccess *d = DirAccess::open(get_text_editor_themes_dir());
		if (d) {
			d->copy(p_file, get_text_editor_themes_dir().plus_file(p_file.get_file()));
			memdelete(d);
			return true;
		}
	}
	return false;
}

bool EditorSettings::save_text_editor_theme() {
	String p_file = get("text_editor/theme/color_theme");

	if (_is_default_text_editor_theme(p_file.get_file().to_lower())) {
		return false;
	}
	String theme_path = get_text_editor_themes_dir().plus_file(p_file + ".tet");
	return _save_text_editor_theme(theme_path);
}

bool EditorSettings::save_text_editor_theme_as(String p_file) {
	if (!p_file.ends_with(".tet")) {
		p_file += ".tet";
	}

	if (_is_default_text_editor_theme(p_file.get_file().to_lower().trim_suffix(".tet"))) {
		return false;
	}
	if (_save_text_editor_theme(p_file)) {
		// switch to theme is saved in the theme directory
		list_text_editor_themes();
		String theme_name = p_file.substr(0, p_file.length() - 4).get_file();

		if (p_file.get_base_dir() == get_text_editor_themes_dir()) {
			_initial_set("text_editor/theme/color_theme", theme_name);
			load_text_editor_theme();
		}
		return true;
	}
	return false;
}

bool EditorSettings::is_default_text_editor_theme() {
	String p_file = get("text_editor/theme/color_theme");
	return _is_default_text_editor_theme(p_file.get_file().to_lower());
}

Vector<String> EditorSettings::get_script_templates(const String &p_extension, const String &p_custom_path) {
	Vector<String> templates;
	String template_dir = get_script_templates_dir();
	if (!p_custom_path.is_empty()) {
		template_dir = p_custom_path;
	}
	DirAccess *d = DirAccess::open(template_dir);
	if (d) {
		d->list_dir_begin();
		String file = d->get_next();
		while (file != String()) {
			if (file.get_extension() == p_extension) {
				templates.push_back(file.get_basename());
			}
			file = d->get_next();
		}
		d->list_dir_end();
		memdelete(d);
	}
	return templates;
}

String EditorSettings::get_editor_layouts_config() const {
	return EditorPaths::get_singleton()->get_config_dir().plus_file("editor_layouts.cfg");
}

float EditorSettings::get_auto_display_scale() const {
#ifdef OSX_ENABLED
	return DisplayServer::get_singleton()->screen_get_max_scale();
#else
	const int screen = DisplayServer::get_singleton()->window_get_current_screen();
	// Use the smallest dimension to use a correct display scale on portrait displays.
	const int smallest_dimension = MIN(DisplayServer::get_singleton()->screen_get_size(screen).x, DisplayServer::get_singleton()->screen_get_size(screen).y);
	if (DisplayServer::get_singleton()->screen_get_dpi(screen) >= 192 && smallest_dimension >= 1400) {
		// hiDPI display.
		return 2.0;
	} else if (smallest_dimension >= 1700) {
		// Likely a hiDPI display, but we aren't certain due to the returned DPI.
		// Use an intermediate scale to handle this situation.
		return 1.5;
	} else if (smallest_dimension <= 800) {
		// Small loDPI display. Use a smaller display scale so that editor elements fit more easily.
		// Icons won't look great, but this is better than having editor elements overflow from its window.
		return 0.75;
	}
	return 1.0;
#endif
}

// Shortcuts

void EditorSettings::add_shortcut(const String &p_name, Ref<Shortcut> &p_shortcut) {
	shortcuts[p_name] = p_shortcut;
}

bool EditorSettings::is_shortcut(const String &p_name, const Ref<InputEvent> &p_event) const {
	const Map<String, Ref<Shortcut>>::Element *E = shortcuts.find(p_name);
	ERR_FAIL_COND_V_MSG(!E, false, "Unknown Shortcut: " + p_name + ".");

	return E->get()->matches_event(p_event);
}

Ref<Shortcut> EditorSettings::get_shortcut(const String &p_name) const {
	const Map<String, Ref<Shortcut>>::Element *SC = shortcuts.find(p_name);
	if (SC) {
		return SC->get();
	}

	// If no shortcut with the provided name is found in the list, check the built-in shortcuts.
	// Use the first item in the action list for the shortcut event, since a shortcut can only have 1 linked event.

	Ref<Shortcut> sc;
	const Map<String, List<Ref<InputEvent>>>::Element *builtin_override = builtin_action_overrides.find(p_name);
	if (builtin_override) {
		sc.instantiate();
		sc->set_event(builtin_override->get().front()->get());
		sc->set_name(InputMap::get_singleton()->get_builtin_display_name(p_name));
	}

	// If there was no override, check the default builtins to see if it has an InputEvent for the provided name.
	if (sc.is_null()) {
		const OrderedHashMap<String, List<Ref<InputEvent>>>::ConstElement builtin_default = InputMap::get_singleton()->get_builtins().find(p_name);
		if (builtin_default) {
			sc.instantiate();
			sc->set_event(builtin_default.get().front()->get());
			sc->set_name(InputMap::get_singleton()->get_builtin_display_name(p_name));
		}
	}

	if (sc.is_valid()) {
		// Add the shortcut to the list.
		shortcuts[p_name] = sc;
		return sc;
	}

	return Ref<Shortcut>();
}

void EditorSettings::get_shortcut_list(List<String> *r_shortcuts) {
	for (const Map<String, Ref<Shortcut>>::Element *E = shortcuts.front(); E; E = E->next()) {
		r_shortcuts->push_back(E->key());
	}
}

Ref<Shortcut> ED_GET_SHORTCUT(const String &p_path) {
	if (!EditorSettings::get_singleton()) {
		return nullptr;
	}

	Ref<Shortcut> sc = EditorSettings::get_singleton()->get_shortcut(p_path);

	ERR_FAIL_COND_V_MSG(!sc.is_valid(), sc, "Used ED_GET_SHORTCUT with invalid shortcut: " + p_path + ".");

	return sc;
}

Ref<Shortcut> ED_SHORTCUT(const String &p_path, const String &p_name, Key p_keycode) {
#ifdef OSX_ENABLED
	// Use Cmd+Backspace as a general replacement for Delete shortcuts on macOS
	if (p_keycode == KEY_DELETE) {
		p_keycode = KEY_MASK_CMD | KEY_BACKSPACE;
	}
#endif

	Ref<InputEventKey> ie;
	if (p_keycode) {
		ie.instantiate();

		ie->set_unicode(p_keycode & KEY_CODE_MASK);
		ie->set_keycode(p_keycode & KEY_CODE_MASK);
		ie->set_shift_pressed(bool(p_keycode & KEY_MASK_SHIFT));
		ie->set_alt_pressed(bool(p_keycode & KEY_MASK_ALT));
		ie->set_ctrl_pressed(bool(p_keycode & KEY_MASK_CTRL));
		ie->set_meta_pressed(bool(p_keycode & KEY_MASK_META));
	}

	if (!EditorSettings::get_singleton()) {
		Ref<Shortcut> sc;
		sc.instantiate();
		sc->set_name(p_name);
		sc->set_event(ie);
		sc->set_meta("original", ie);
		return sc;
	}

	Ref<Shortcut> sc = EditorSettings::get_singleton()->get_shortcut(p_path);
	if (sc.is_valid()) {
		sc->set_name(p_name); //keep name (the ones that come from disk have no name)
		sc->set_meta("original", ie); //to compare against changes
		return sc;
	}

	sc.instantiate();
	sc->set_name(p_name);
	sc->set_event(ie);
	sc->set_meta("original", ie); //to compare against changes
	EditorSettings::get_singleton()->add_shortcut(p_path, sc);

	return sc;
}

void EditorSettings::set_builtin_action_override(const String &p_name, const Array &p_events) {
	List<Ref<InputEvent>> event_list;

	// Override the whole list, since events may have their order changed or be added, removed or edited.
	InputMap::get_singleton()->action_erase_events(p_name);
	for (int i = 0; i < p_events.size(); i++) {
		event_list.push_back(p_events[i]);
		InputMap::get_singleton()->action_add_event(p_name, p_events[i]);
	}

	// Check if the provided event array is same as built-in. If it is, it does not need to be added to the overrides.
	// Note that event order must also be the same.
	bool same_as_builtin = true;
	OrderedHashMap<String, List<Ref<InputEvent>>>::ConstElement builtin_default = InputMap::get_singleton()->get_builtins().find(p_name);
	if (builtin_default) {
		List<Ref<InputEvent>> builtin_events = builtin_default.get();

		if (p_events.size() == builtin_events.size()) {
			int event_idx = 0;

			// Check equality of each event.
			for (const Ref<InputEvent> &E : builtin_events) {
				if (!E->is_match(p_events[event_idx])) {
					same_as_builtin = false;
					break;
				}
				event_idx++;
			}
		} else {
			same_as_builtin = false;
		}
	}

	if (same_as_builtin && builtin_action_overrides.has(p_name)) {
		builtin_action_overrides.erase(p_name);
	} else {
		builtin_action_overrides[p_name] = event_list;
	}

	// Update the shortcut (if it is used somewhere in the editor) to be the first event of the new list.
	if (shortcuts.has(p_name)) {
		shortcuts[p_name]->set_event(event_list.front()->get());
	}
}

const Array EditorSettings::get_builtin_action_overrides(const String &p_name) const {
	const Map<String, List<Ref<InputEvent>>>::Element *AO = builtin_action_overrides.find(p_name);
	if (AO) {
		Array event_array;

		List<Ref<InputEvent>> events_list = AO->get();
		for (const Ref<InputEvent> &E : events_list) {
			event_array.push_back(E);
		}
		return event_array;
	}

	return Array();
}

void EditorSettings::notify_changes() {
	_THREAD_SAFE_METHOD_

	SceneTree *sml = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());

	if (!sml) {
		return;
	}

	Node *root = sml->get_root()->get_child(0);

	if (!root) {
		return;
	}
	root->propagate_notification(NOTIFICATION_EDITOR_SETTINGS_CHANGED);
}

void EditorSettings::_bind_methods() {
	ClassDB::bind_method(D_METHOD("has_setting", "name"), &EditorSettings::has_setting);
	ClassDB::bind_method(D_METHOD("set_setting", "name", "value"), &EditorSettings::set_setting);
	ClassDB::bind_method(D_METHOD("get_setting", "name"), &EditorSettings::get_setting);
	ClassDB::bind_method(D_METHOD("erase", "property"), &EditorSettings::erase);
	ClassDB::bind_method(D_METHOD("set_initial_value", "name", "value", "update_current"), &EditorSettings::set_initial_value);
	ClassDB::bind_method(D_METHOD("property_can_revert", "name"), &EditorSettings::property_can_revert);
	ClassDB::bind_method(D_METHOD("property_get_revert", "name"), &EditorSettings::property_get_revert);
	ClassDB::bind_method(D_METHOD("add_property_info", "info"), &EditorSettings::_add_property_info_bind);

	ClassDB::bind_method(D_METHOD("get_project_settings_dir"), &EditorSettings::get_project_settings_dir);

	ClassDB::bind_method(D_METHOD("set_project_metadata", "section", "key", "data"), &EditorSettings::set_project_metadata);
	ClassDB::bind_method(D_METHOD("get_project_metadata", "section", "key", "default"), &EditorSettings::get_project_metadata, DEFVAL(Variant()));

	ClassDB::bind_method(D_METHOD("set_favorites", "dirs"), &EditorSettings::set_favorites);
	ClassDB::bind_method(D_METHOD("get_favorites"), &EditorSettings::get_favorites);
	ClassDB::bind_method(D_METHOD("set_recent_dirs", "dirs"), &EditorSettings::set_recent_dirs);
	ClassDB::bind_method(D_METHOD("get_recent_dirs"), &EditorSettings::get_recent_dirs);

	ClassDB::bind_method(D_METHOD("set_builtin_action_override", "name", "actions_list"), &EditorSettings::set_builtin_action_override);

	ADD_SIGNAL(MethodInfo("settings_changed"));

	BIND_CONSTANT(NOTIFICATION_EDITOR_SETTINGS_CHANGED);
}

EditorSettings::EditorSettings() {
	last_order = 0;
	optimize_save = true;
	save_changed_setting = true;

	_load_defaults();
}

EditorSettings::~EditorSettings() {
}
