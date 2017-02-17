/*
 *	Loader Library by Parra Studios
 *	Copyright (C) 2016 - 2017 Vicente Eduardo Ferrer Garcia <vic798@gmail.com>
 *
 *	A library for loading executable code at run-time into a process.
 *
 */

 /* -- Headers -- */

#include <metacall/metacall-version.h>

#include <loader/loader.h>
#include <loader/loader_impl.h>

#include <reflect/reflect_scope.h>
#include <reflect/reflect_context.h>

#include <adt/adt_hash_map.h>

#include <environment/environment_variable_path.h>

#include <log/log.h>

#include <stdlib.h>
#include <string.h>

/* -- Definitions -- */

#define LOADER_LIBRARY_PATH			"LOADER_LIBRARY_PATH"
#define LOADER_DEFAULT_LIBRARY_PATH	"."

#define LOADER_SCRIPT_PATH			"LOADER_SCRIPT_PATH"
#define LOADER_DEFAULT_SCRIPT_PATH	"scripts"

/* -- Forward Declarations -- */

struct loader_type;

struct loader_get_iterator_args_type;

/* -- Type Definitions -- */

typedef struct loader_type * loader;

typedef struct loader_get_iterator_args_type * loader_get_iterator_args;

/* -- Member Data -- */

struct loader_type
{
	hash_map impl_map;
	char * library_path;
	char * script_path;
};

struct loader_get_iterator_args_type
{
	const char * name;
	scope_object obj;

};

/* -- Private Methods -- */

static loader loader_singleton(void);

static loader_impl loader_create_impl(const loader_naming_tag extension);

static int loader_get_cb_iterate(hash_map map, hash_map_key key, hash_map_value val, hash_map_cb_iterate_args args);

static int loader_unload_impl_map_cb_iterate(hash_map map, hash_map_key key, hash_map_value val, hash_map_cb_iterate_args args);

static function_interface get_interface_proxy();

static value loader_register_invoke_proxy(function func, function_impl func_impl, function_args args);

static void loader_register_destroy_proxy(function func, function_impl func_impl);

/* -- Methods -- */

static loader loader_singleton()
{
	static struct loader_type loader_instance =
	{
		NULL, NULL, NULL
	};

	return &loader_instance;
}

void loader_initialize()
{
	loader l = loader_singleton();

	if (l->impl_map == NULL)
	{
		l->impl_map = hash_map_create(&hash_callback_str, &comparable_callback_str);
	}

	if (l->library_path == NULL)
	{
		static const char loader_library_default_path[] = LOADER_DEFAULT_LIBRARY_PATH;

		l->library_path = environment_variable_path_create(LOADER_LIBRARY_PATH, loader_library_default_path);

		log_write("metacall", LOG_LEVEL_DEBUG, "Loader library path: %s", l->library_path);
	}

	if (l->script_path == NULL)
	{
		static const char loader_script_default_path[] = LOADER_DEFAULT_SCRIPT_PATH;

		l->script_path = environment_variable_path_create(LOADER_SCRIPT_PATH, loader_script_default_path);

		log_write("metacall", LOG_LEVEL_DEBUG, "Loader script path: %s", l->script_path);
	}

	if (hash_map_get(l->impl_map, (hash_map_key)LOADER_HOST_PROXY_NAME) == NULL)
	{
		loader_impl proxy = loader_impl_create_proxy();

		if (proxy == NULL)
		{
			log_write("metacall", LOG_LEVEL_ERROR, "Loader invalid proxy initialization");
		}

		if (hash_map_insert(l->impl_map, (hash_map_key)LOADER_HOST_PROXY_NAME, proxy) != 0)
		{
			log_write("metacall", LOG_LEVEL_ERROR, "Loader invalid proxy insertion <%p>", (void *) proxy);

			loader_impl_destroy(proxy);
		}
	}
}

static loader_impl loader_create_impl(const loader_naming_tag tag)
{
	loader l = loader_singleton();

	loader_impl impl = loader_impl_create(l->library_path, (const hash_map_key)tag);

	if (impl != NULL)
	{
		if (hash_map_insert(l->impl_map, (hash_map_key)tag, impl) == 0)
		{
			if (loader_impl_execution_path(impl, ".") == 0)
			{
				if (l->script_path != NULL)
				{
					if (loader_impl_execution_path(impl, l->script_path) == 0)
					{
						return impl;
					}
				}
				else
				{
					return impl;
				}
			}

			hash_map_remove(l->impl_map, (hash_map_key)tag);
		}

		loader_impl_destroy(impl);
	}

	return NULL;
}

loader_impl loader_get_impl(const loader_naming_tag tag)
{
	loader l = loader_singleton();

	loader_impl impl = (loader_impl)hash_map_get(l->impl_map, (const hash_map_key)tag);

	if (impl == NULL)
	{
		impl = loader_create_impl(tag);

		log_write("metacall", LOG_LEVEL_DEBUG, "Created loader (%s) implementation <%p>", tag, (void *)impl);
	}

	return impl;
}

int loader_load_path(const loader_naming_path path)
{
	loader l = loader_singleton();

#ifdef LOADER_LAZY
	log_write("metacall", LOG_LEVEL_DEBUG, "Loader lazy initialization");

	loader_initialize();
#endif

	if (l->impl_map != NULL)
	{
		(void)path;

		/* ... */
	}

	return 1;
}

int loader_load_from_file(const loader_naming_tag tag, const loader_naming_path paths[], size_t size)
{
	loader l = loader_singleton();

#ifdef LOADER_LAZY
	log_write("metacall", LOG_LEVEL_DEBUG, "Loader lazy initialization");

	loader_initialize();
#endif

	if (l->impl_map != NULL && size > 0)
	{
		if (tag != NULL)
		{
			loader_impl impl = loader_get_impl(tag);

			log_write("metacall", LOG_LEVEL_DEBUG, "Loader (%s) implementation <%p>", tag, (void *)impl);

			if (impl != NULL)
			{
				if (l->script_path != NULL)
				{
					loader_naming_path absolute_path[LOADER_LOAD_FROM_FILES_SIZE];

					size_t iterator;

					for (iterator = 0; iterator < size; ++iterator)
					{
						memcpy(absolute_path[iterator], l->script_path, strlen(l->script_path) + 1);

						strncat(absolute_path[iterator], paths[iterator], LOADER_NAMING_PATH_SIZE - 1);
					}

					return loader_impl_load_from_file(impl, (const loader_naming_path *)absolute_path, size);
				}
				else
				{
					return loader_impl_load_from_file(impl, paths, size);
				}
			}
		}
	}

	return 1;
}

int loader_load_from_memory(const loader_naming_tag tag, const char * buffer, size_t size)
{
	loader l = loader_singleton();

#ifdef LOADER_LAZY
	log_write("metacall", LOG_LEVEL_DEBUG, "Loader lazy initialization");

	loader_initialize();
#endif

	if (l->impl_map != NULL)
	{
		loader_impl impl = loader_get_impl(tag);

		log_write("metacall", LOG_LEVEL_DEBUG, "Loader (%s) implementation <%p>", tag, (void *)impl);

		if (impl != NULL)
		{
			return loader_impl_load_from_memory(impl, buffer, size);
		}
	}

	return 1;
}

int loader_load_from_package(const loader_naming_tag extension, const loader_naming_path path)
{
	loader l = loader_singleton();

#ifdef LOADER_LAZY
	log_write("metacall", LOG_LEVEL_DEBUG, "Loader lazy initialization");

	loader_initialize();
#endif

	if (l->impl_map != NULL)
	{
		loader_impl impl = loader_get_impl(extension);

		log_write("metacall", LOG_LEVEL_DEBUG, "Loader (%s) implementation <%p>", extension, (void *)impl);

		if (impl != NULL)
		{
			return loader_impl_load_from_package(impl, path);
		}
	}

	return 1;
}

static int loader_get_cb_iterate(hash_map map, hash_map_key key, hash_map_value val, hash_map_cb_iterate_args args)
{
	if (map != NULL && key != NULL && val != NULL && args != NULL)
	{
		loader_impl impl = val;

		loader_get_iterator_args get_args = args;

		context ctx = loader_impl_context(impl);

		scope sp = context_scope(ctx);

		get_args->obj = scope_get(sp, get_args->name);

		if (get_args->obj != NULL)
		{
			log_write("metacall", LOG_LEVEL_DEBUG, "Loader get callback: impl %p, name %s", (void *)get_args->obj, get_args->name);

			return 1;
		}
	}

	return 0;
}

loader_data loader_get(const char * name)
{
	loader l = loader_singleton();

	if (l->impl_map != NULL)
	{
		hash_map_cb_iterate_args args;

		struct loader_get_iterator_args_type get_args;

		get_args.name = name;
		get_args.obj = NULL;

		args = (loader_get_iterator_args)&get_args;

		hash_map_iterate(l->impl_map, &loader_get_cb_iterate, args);

		if (get_args.obj != NULL)
		{
			return (loader_data)get_args.obj;
		}
	}

	return NULL;
}

static int loader_unload_impl_map_cb_iterate(hash_map map, hash_map_key key, hash_map_value val, hash_map_cb_iterate_args args)
{
	if (map != NULL && key != NULL && val != NULL && args == NULL)
	{
		loader_impl impl = val;

		loader_impl_destroy(impl);

		return 0;
	}

	return 1;
}

int loader_unload()
{
	loader l = loader_singleton();

	if (l->impl_map != NULL)
	{
		hash_map_iterate(l->impl_map, &loader_unload_impl_map_cb_iterate, NULL);

		if (hash_map_clear(l->impl_map) != 0)
		{
#ifdef LOADER_LAZY
			log_write("metacall", LOG_LEVEL_DEBUG, "Loader lazy destruction");

			loader_destroy();
#endif

			return 1;
		}
	}

#ifdef LOADER_LAZY
	log_write("metacall", LOG_LEVEL_DEBUG, "Loader lazy destruction");

	loader_destroy();
#endif

	return 0;
}

void loader_destroy()
{
	loader l = loader_singleton();

	if (l->impl_map != NULL)
	{
		hash_map_destroy(l->impl_map);

		l->impl_map = NULL;
	}

	if (l->library_path != NULL)
	{
		environment_variable_path_destroy(l->library_path);

		l->library_path = NULL;
	}

	if (l->script_path != NULL)
	{
		environment_variable_path_destroy(l->script_path);

		l->script_path = NULL;
	}
}

static value loader_register_invoke_proxy(function func, function_impl func_impl, function_args args) {
	
	(void)func;
	
	host_invoke * hinvoke = (host_invoke*)func_impl;

	return hinvoke->invoke(args);
}

static void loader_register_destroy_proxy(function func, function_impl func_impl) {
	(void)func;

	if (func_impl != NULL) {
		free(func_impl);
	}
}


static function_interface get_interface_proxy(void)
{
	static struct function_interface_type interface =
	{
		NULL,
		&loader_register_invoke_proxy,
		&loader_register_destroy_proxy
	};

	return &interface;
}

int loader_register(const char * name, loader_register_invoke invoke, type_id return_type, size_t arg_size, type_id args_type_id[]) {
	function f = NULL;

	loader_impl loader = loader_get_impl(LOADER_HOST_PROXY_NAME);

	context ctx = loader_impl_context(loader);

	scope sp = context_scope(ctx);
	
	host_invoke * hinvoke = malloc(sizeof(host_invoke));
	
	hinvoke->invoke = invoke;
	
	f = function_create(name, arg_size, hinvoke, &get_interface_proxy);

	if (f != NULL) {

		signature s = function_signature(f);

		if (arg_size > 0) {

			size_t iterator;

			for (iterator = 0; iterator < arg_size; iterator++)
			{
				signature_set(s, iterator, "holder", type_create(args_type_id[iterator], "holder", NULL, NULL));
			}
		}

		signature_set_return(s, type_create(return_type, "holder", NULL, NULL));

		scope_define(sp, name, f);

		return 0;
	}

	return 1;
}

const char * loader_print_info()
{
	static const char loader_info[] =
		"Loader Library " METACALL_VERSION "\n"
		"Copyright (C) 2016 - 2017 Vicente Eduardo Ferrer Garcia <vic798@gmail.com>\n"

#ifdef LOADER_STATIC_DEFINE
		"Compiled as static library type\n"
#else
		"Compiled as shared library type\n"
#endif

#ifdef LOADER_LAZY
		"Compiled with lazy initialization and destruction"
#else
		"Compiled with explicit initialization and destruction"
#endif

		"\n";

	return loader_info;
}
