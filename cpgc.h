// requires "stdint.h"

typedef struct gc gc;
typedef struct gcinfo gcinfo;
typedef struct gcengine gcengine;


gcengine* create_gcengine();
void destroy_gcengine(gcengine*);

gcinfo mk_gcinfo(size_t num_subobjs, size_t num_unboxed_bytes, size_t arrlen);

gc* gcalloc(gcengine*, gcinfo);
gc* gcgive(gcengine*, void* raw_data, gcinfo);

gc** gcsubobj(gc* obj, size_t subobjix);
void* gcunboxed(gc* obj);

size_t arrlen(gc* obj);
gc** gcarrsubobj(gc* obj, size_t arrix, size_t subobjix);
void* gcarrunboxed(gc* obj, size_t arrix);

void gccollect_major(gcengine* engine);
void gccollect_minor(gcengine* engine);
void setroot(gcengine* engine, gc* new_root); //FIXME when we get multiple roots