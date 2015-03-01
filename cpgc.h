// requires "stdint.h"

typedef struct gc gc;
typedef struct gcinfo gcinfo;
typedef struct gcroot gcroot;
typedef struct gcengine gcengine;


gcengine* create_gcengine();
void destroy_gcengine(gcengine*);


gc* gcalloc(gcengine*, gcinfo);
gc* gcgive(gcengine*, void* raw_data, gcinfo);

gcroot* new_gcroot(gcengine*, gc* new_root);
void set_gcroot(gcroot*, gc* new_root);
void free_gcroot(gcroot*);

void gccollect_major(gcengine* engine);
// void gccollect_minor(gcengine* engine);


gc** gcsubobj(gc* obj, size_t subobjix);
void* gcraw(gc* obj);

size_t arrlen(gc* obj);
gc** gcarrsubobj(gc* obj, size_t arrix, size_t subobjix);
void* gcarrraw(gc* obj, size_t arrix);


gcinfo mk_gcinfo_obj(size_t num_subobjs, size_t num_unboxed_bytes, size_t finalizer_offset);
gcinfo mk_gcinfo_arr(size_t arrlen, size_t num_subobjs, size_t num_unboxed_bytes, size_t finalizer_offset);
size_t gcsizeof(gcinfo);

void setgcFinalizers(gcengine*, size_t num_finalizers, void (**finalizers)(void*));
static inline size_t nogcFinalizer(void) {
	extern size_t nogcFinalizer_val;
	return nogcFinalizer_val;
}
size_t findgcFinalizer(void (*finalizer)(void*), size_t num_finalizers, void (**finalizers)(void*));

void setgcTracers(gcengine* engine, void (**tracers)(gcengine*, void*, void (*)(gcengine*, gc*)));
static inline size_t fixedgcTracer(void) {
	extern size_t fixedgcTracer_val;
	return fixedgcTracer_val;
}
size_t findgcTracer(void (*tracer)(gcengine*, void*, void (*)(gcengine*, gc*)),
				    size_t num_finalizers,
				    void (**tracers)(gcengine*, void*, void (*)(gcengine*, gc*)));

