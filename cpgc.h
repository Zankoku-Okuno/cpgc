// requires "stdint.h"

typedef struct gc gc;
typedef struct gcinfo gcinfo;
typedef struct gcengine gcengine;


gcengine* create_gcengine();
void destroy_gcengine(gcengine*);
void setgcFinalizers(gcengine* engine, void (**finalizers)(void*));
void setgcTracers(gcengine* engine, void (**tracers)(gcengine*, void*, void (*)(gcengine*, gc*)));

void setroot(gcengine* engine, gc* new_root); //FIXME when we get multiple roots
void gccollect_major(gcengine* engine);
void gccollect_minor(gcengine* engine);


gcinfo mk_gcinfo_obj(size_t num_subobjs, size_t num_unboxed_bytes, void (*finalize)(void*));
gcinfo mk_gcinfo_arr(size_t arrlen, size_t num_subobjs, size_t num_unboxed_bytes, void (*finalize)(void*));

gc* gcalloc(gcengine*, gcinfo);
gc* gcgive(gcengine*, void* raw_data, gcinfo);


gc** gcsubobj(gc* obj, size_t subobjix);
void* gcraw(gc* obj);

size_t arrlen(gc* obj);
gc** gcarrsubobj(gc* obj, size_t arrix, size_t subobjix);
void* gcarrraw(gc* obj, size_t arrix);
