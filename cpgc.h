// requires "stdint.h"

typedef struct gc gc;
typedef struct gcengine gcengine;

typedef struct {
	size_t num_subobjs;
	size_t unboxed_bytes;
	void (*cleanup)(void*); //FIXME I'm really not sure if I even want to allow a cleanup function. Could just slow things down while apparently offering bad promises. However, it really can be useful, e.g. if you're actually allocating from a pool, there may need to be an additional free-in-pool call.
} gcinfo;

gcengine* create_gcengine();
void destroy_gcengine(gcengine*);

gc* gcalloc(gcengine*, gcinfo*);
gc* gcgive(gcengine*, void* raw_data, gcinfo*);

gc** gcsubobj(gc* obj, size_t ix);
void* gcunboxed(gc* obj);

void gccollect_major(gcengine* engine);
void gccollect_minor(gcengine* engine);
void setroot(gcengine* engine, gc* new_root); //FIXME when we get multiple roots