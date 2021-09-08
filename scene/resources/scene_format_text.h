#ifndef SCENE_FORMAT_TEXT_H
#define SCENE_FORMAT_TEXT_H

#include "io/resource_loader.h"
#include "io/resource_saver.h"
#include "os/file_access.h"
#include "scene/resources/packed_scene.h"
#include "variant_parser.h"



class ResourceInteractiveLoaderText : public ResourceInteractiveLoader {

	String local_path;
	String res_path;
	String error_text;

	FileAccess *f;

	VariantParser::StreamFile stream;

	struct ExtResource {
		String path;
		String type;
	};


	bool is_scene;
	String res_type;

	bool ignore_resource_parsing;

//	Map<String,String> remaps;

	Map<int,ExtResource> ext_resources;

	int resources_total;
	int resource_current;
	String resource_type;

	VariantParser::Tag next_tag;

	mutable int lines;

	Map<String,String> remaps;
	//void _printerr();

	static Error _parse_sub_resources(void* p_self, VariantParser::Stream* p_stream,Ref<Resource>& r_res,int &line,String &r_err_str) { return reinterpret_cast<ResourceInteractiveLoaderText*>(p_self)->_parse_sub_resource(p_stream,r_res,line,r_err_str); }
	static Error _parse_ext_resources(void* p_self, VariantParser::Stream* p_stream,Ref<Resource>& r_res,int &line,String &r_err_str) { return reinterpret_cast<ResourceInteractiveLoaderText*>(p_self)->_parse_ext_resource(p_stream,r_res,line,r_err_str); }

	Error _parse_sub_resource(VariantParser::Stream* p_stream,Ref<Resource>& r_res,int &line,String &r_err_str);
	Error _parse_ext_resource(VariantParser::Stream* p_stream,Ref<Resource>& r_res,int &line,String &r_err_str);

	VariantParser::ResourceParser rp;


	Ref<PackedScene> packed_scene;


friend class ResourceFormatLoaderText;

	List<RES> resource_cache;
	Error error;

	RES resource;

public:

	virtual void set_local_path(const String& p_local_path);
	virtual Ref<Resource> get_resource();
	virtual Error poll();
	virtual int get_stage() const;
	virtual int get_stage_count() const;

	void open(FileAccess *p_f, bool p_skip_first_tag=false);
	String recognize(FileAccess *p_f);
	void get_dependencies(FileAccess *p_f, List<String> *p_dependencies, bool p_add_types);
	Error rename_dependencies(FileAccess *p_f, const String &p_path,const Map<String,String>& p_map);


	~ResourceInteractiveLoaderText();

};



class ResourceFormatLoaderText : public ResourceFormatLoader {
public:

	virtual Ref<ResourceInteractiveLoader> load_interactive(const String &p_path,Error *r_error=NULL);
	virtual void get_recognized_extensions_for_type(const String& p_type,List<String> *p_extensions) const;
	virtual void get_recognized_extensions(List<String> *p_extensions) const;
	virtual bool handles_type(const String& p_type) const;
	virtual String get_resource_type(const String &p_path) const;
	virtual void get_dependencies(const String& p_path, List<String> *p_dependencies, bool p_add_types=false);
	virtual Error rename_dependencies(const String &p_path,const Map<String,String>& p_map);


};


class ResourceFormatSaverTextInstance  {

	String local_path;

	Ref<PackedScene> packed_scene;

	bool takeover_paths;
	bool relative_paths;
	bool bundle_resources;
	bool skip_editor;
	FileAccess *f;
	Set<RES> resource_set;
	List<RES> saved_resources;
	Map<RES,int> external_resources;
	Map<RES,int> internal_resources;

	void _find_resources(const Variant& p_variant,bool p_main=false);

	static String _write_resources(void *ud,const RES& p_resource);
	String _write_resource(const RES& res);


public:

	Error save(const String &p_path,const RES& p_resource,uint32_t p_flags=0);


};

class ResourceFormatSaverText : public ResourceFormatSaver {
public:
	static ResourceFormatSaverText* singleton;
	virtual Error save(const String &p_path,const RES& p_resource,uint32_t p_flags=0);
	virtual bool recognize(const RES& p_resource) const;
	virtual void get_recognized_extensions(const RES& p_resource,List<String> *p_extensions) const;

	ResourceFormatSaverText();
};


#endif // SCENE_FORMAT_TEXT_H
