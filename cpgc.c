#include "stdint.h"
#include "stdlib.h"
#include "cpgc.h"

static const size_t objblock_size = 64;
static const size_t rootblock_size = 64;

typedef struct objblock objblock;
typedef struct objring objring;

typedef struct rootblock rootblock;
typedef struct rootring rootring;

typedef struct engine_info engine_info;


//------------------------------------
// Metadata

typedef enum { UNMARKED = 0, MARKED = 1 } mark;
typedef enum { FIXED = 0, CUSTOM = 1 } info_format;

struct gcinfo {
    union {
        struct {
            size_t finalizer_offset : 16;
            size_t num_raw_bytes : 16;
            size_t arrlen : 16;
            size_t num_subobjs : 14;
            info_format format : 1;
            mark mark : 1;
        } fixed;
        struct {
            size_t finalizer_offset : 16;
            size_t tracer_offset : 16;
            size_t objsize : 30;
            info_format format : 1;
            mark mark : 1;
        } custom;
    } as;
};

gcinfo mk_gcinfo_arr(size_t arrlen, size_t num_subobjs, size_t num_unboxed_bytes, size_t finalizer_offset) {
    gcinfo out;
    out.as.fixed.finalizer_offset = finalizer_offset;
    out.as.fixed.num_raw_bytes = num_unboxed_bytes;
    out.as.fixed.arrlen = arrlen;
    out.as.fixed.num_subobjs = num_subobjs;
    out.as.fixed.format = FIXED;
    out.as.fixed.mark = UNMARKED;
    return out;
}
gcinfo mk_gcinfo_obj(size_t num_subobjs, size_t num_unboxed_bytes, size_t finalizer_offset) {
    return mk_gcinfo_arr(1, num_subobjs, num_unboxed_bytes, finalizer_offset);
}
gcinfo mk_gcinfo_custom(size_t objsize, size_t tracer_offset, size_t finalizer_offset) {
    gcinfo out;
    out.as.custom.finalizer_offset = finalizer_offset;
    out.as.custom.tracer_offset = tracer_offset;
    out.as.custom.objsize = objsize;
    out.as.fixed.format = FIXED;
    out.as.fixed.mark = UNMARKED;
    return out;

}

// Compute the size of one element of a fixed-format managed array.
static inline
int isCustomFormat(gcinfo info) {
    return info.as.fixed.format == CUSTOM;
}
static inline
size_t sizeofObjElem(gcinfo info) {
    return info.as.fixed.num_subobjs * sizeof(gc*) + info.as.fixed.num_raw_bytes;
}
// Compute the full size of a managed object.
static inline
size_t sizeofObj(gcinfo info) {
    return isCustomFormat(info) ? info.as.custom.objsize
                                : info.as.fixed.arrlen * sizeofObjElem(info);
}

//------------------------------------
// Data Structures

struct gc {
    void* data_p;
    gcinfo info;
};

struct objblock {
    uint64_t registry;
    objblock* next_block;
    gc objs[objblock_size];
};
struct objring {
    objblock* current;
    objblock* last_collect;
};

struct gcroot {
    gc* rootslot;
};
struct rootblock {
    rootblock* next_block;
    gcroot roots[rootblock_size];
};
struct rootring {
    rootblock* current;
};

struct engine_info {
    size_t num_objects_in_tenure;
    size_t size_objects_in_tenure;
};
struct gcengine {
    objring allobjs;
    rootring allroots;

    void (**finalizers)(void*);
    void (**tracers)(gcengine*, void*, void (*)(gcengine*, gc*));

    engine_info tracking;
};

////// Objects //////
static inline
int isMarked(gc* obj) {
    return obj->info.as.fixed.mark == MARKED;
}
static inline
void setMarked(gc* obj) {
    obj->info.as.fixed.mark = MARKED;
}
static inline
void clrMarked(gc* obj) {
    obj->info.as.fixed.mark = UNMARKED;
}

static inline
void cleanup_tenureobj(void (**finalizers)(void*), gc* obj) {
    size_t offset = obj->info.as.fixed.finalizer_offset;
    if (offset) {
        finalizers[offset](obj->data_p);
    }
    free(obj->data_p);
}
static inline
void cleanup_minor(gc* obj) {
    free(obj->data_p);
}

////// Registries //////
static const uint64_t emptyObjRegistry = ~(uint64_t)0;
static const uint64_t fullObjRegistry = (uint64_t)0;

static inline
int isUsed(uint64_t registry, int i) {
    return !(registry & ((uint64_t)1 << i));
}
static inline
uint64_t setUsed(uint64_t word, int i) {
    return word & (~(uint64_t)1 << i);
}
static inline
uint64_t clrUsed(uint64_t word, int i) {
    return word | ((uint64_t)1 << i);
}

////// Object Blocks //////
static inline
gc* objFromBlock(objblock* block, int ix) {
    return &block->objs[ix];
}

static
objblock* create_objblock() {
    objblock* new = malloc(sizeof(objblock));
    new->registry = emptyObjRegistry;
    return new;
}
static
void destroy_objblock(gcengine* engine, objblock* block) {
    uint64_t registry = block->registry;
    for (int i = 0, e = objblock_size; i < e; ++i) {
        if (isUsed(registry, i)) {
            //FIXME I need to use the appropriate cleanup
            cleanup_tenureobj(engine->finalizers, objFromBlock(block, i));
        }
    }
    free(block);
}

////// Object Rings //////
static
objring create_objring() {
    objring res;
    res.current = res.last_collect = create_objblock();
    res.current->next_block = res.current;
    return res;
}
static
void destroy_objring(gcengine* engine, objring ring) {
    objblock* start = ring.current;
    objblock* current = start;
    do {
        objblock* next = current->next_block;
        destroy_objblock(engine, current);
        current = next;
    } while (current != start);
}

////// Root Blocks //////
static
rootblock* create_rootblock() {
    rootblock* new = malloc(sizeof(rootblock));
    for (int i = 0; i < rootblock_size; ++i) {
        new->roots[i].rootslot = NULL;
    }
    return new;
}
static
void destroy_rootblock(rootblock* block) {
    free(block);
}

////// Root Rings //////
static
rootring create_rootring() {
    rootring res;
    res.current = create_rootblock();
    res.current->next_block = res.current;
    return res;
}
static
void destroy_rootring(rootring ring) {
    rootblock* start = ring.current;
    rootblock* current = start;
    do {
        rootblock* next = current->next_block;
        destroy_rootblock(current);
        current = next;
    } while (current != start);
}

////// Engines //////
gcengine* create_gcengine() {
    gcengine* new = malloc(sizeof(gcengine));
    new->allobjs = create_objring();
    new->allroots = create_rootring();
    new->tracking.num_objects_in_tenure = new->tracking.size_objects_in_tenure = 0;
    return new;
}
void destroy_gcengine(gcengine* engine) {
    destroy_rootring(engine->allroots);
    destroy_objring(engine, engine->allobjs);
    free(engine);
}

void setgcFinalizers(gcengine* engine, size_t num_finalizers, void (**finalizers)(void*)) {
    engine->finalizers = finalizers;
}
size_t nogcFinalizer_val = 0;
size_t findgcFinalizer(void (*finalizer)(void*), size_t num_finalizers, void (**finalizers)(void*)) {
    if (!finalizer | !finalizers) {
        return nogcFinalizer_val;
    }
    for (int i = 1; i < num_finalizers; ++i) {
        if (finalizer == finalizers[i]) {
            return i;
        }
    }
    return nogcFinalizer_val;
}
void setgcTracers(gcengine* engine, void (**tracers)(gcengine*, void*, void (*)(gcengine*, gc*))) {
    engine->tracers = tracers;
}
size_t fixedgcTracer_val = 0;
size_t findgcTracer(void (*tracer)(gcengine*, void*, void (*)(gcengine*, gc*)),
                    size_t num_tracers,
                    void (**tracers)(gcengine*, void*, void (*)(gcengine*, gc*))) {
    if (!tracer | !tracers) {
        return fixedgcTracer_val;
    }
    for (int i = 1; i < num_tracers; ++i) {
        if (tracer == tracers[i]) {
            return i;
        }
    }
    return fixedgcTracer_val;
}

static inline
void tracking_addTenure(gcengine* engine, gcinfo info) {
    engine->tracking.num_objects_in_tenure += 1;
    engine->tracking.size_objects_in_tenure += sizeofObj(info);
}
static inline
void tracking_subTenure(gcengine* engine, gcinfo info) {
    engine->tracking.num_objects_in_tenure -= 1;
    engine->tracking.size_objects_in_tenure -= sizeofObj(info);
}

//------------------------------------
// Management Structures

//Input a pointer to a registry: a 64-bit word.
//In that word, the nth bit represents either that the nth slot is used (on zero), or free (on one).
//Thus, an empty registry is all ones, full is all zeros.
//This finds a free slot and updates the registry to mark it filled.
//It then returns the index of the slot found.
//If no slot can be found, the registry is left alone and `-1` is returned.
static
int bmalloc64(uint64_t* registry_p) {
    int ix = __builtin_ffsll(*registry_p);
    if (ix--) {
        *registry_p = setUsed(*registry_p, ix);
    }
    return ix;
}

//Input a block of gc smart pointers and get back a `gc*` from it.
//If the block cannot allocate any more, then NULL is returned.
//The block is updated accordingly, but the data held by the pointer is not initialized.
static
gc* objblock_alloc(objblock* block) {
    int ix = bmalloc64(&block->registry);
    return 0 <= ix ? objFromBlock(block, ix) : NULL;
}

static inline
gcroot* rootblock_alloc(rootblock* block) {
    for (int i = 0; i < rootblock_size; ++i) {
        if (block->roots[i].rootslot == NULL) {
            return &block->roots[i];
        }
    }
    return NULL;
}

//Allocate a new gc pointer in the context of the passed engine.
//If the machine is out of memory, returns NULL.
static
gc* objring_alloc(objring* ring) {
    objblock* block = ring->current;
    for (;;) {
        gc* new = objblock_alloc(block);
        if (new) {
            return new;
        }
        else {
            if (block->next_block == ring->last_collect) {
                objblock* new_block = create_objblock();
                if (!new_block) {
                    return NULL;
                }
                block->next_block = new_block;
                new_block->next_block = ring->last_collect;
            }
            block = ring->current = block->next_block;
        }
    }
}

static
gcroot* rootring_alloc(rootring* ring) {
    rootblock* start = ring->current;
    rootblock* block = start;
    for (;;) {
        gcroot* new = rootblock_alloc(block);
        if (new) {
            return new;
        }
        else {
            if (block->next_block == start) {
                rootblock* new_block = create_rootblock();
                if (!new_block) {
                    return NULL;
                }
                block->next_block = new_block;
                new_block->next_block = start;
            }
            block = ring->current = block->next_block;
        }
    }
}

//------------------------------------
// Tracing

// Trace a single object during major collection.
// C.f. `traceobj_minor`.
static
void traceobj_major(gcengine* engine, gc* obj) {
    if (!obj || isMarked(obj)) {
        return;
    }
    else {
        setMarked(obj);
        if (isCustomFormat(obj->info)) {
            size_t offset = obj->info.as.custom.tracer_offset;
            void (*trace)(gcengine*, void*, void (*)(gcengine*, gc*)) = engine->tracers[offset];
            trace(engine, obj->data_p, traceobj_major);
        }
        else {
            size_t elemsize = sizeofObjElem(obj->info);
            void* elemStart = obj->data_p;
            void* subobjStop = obj->data_p + sizeof(gc*) * obj->info.as.fixed.num_subobjs;
            void* arrStop = obj->data_p + sizeofObjElem(obj->info) * obj->info.as.fixed.arrlen;
            while (elemStart < arrStop) {
                for (gc** subobj = (gc**)elemStart; subobj < (gc**)subobjStop; ++subobj) {
                    traceobj_major(engine, *subobj);
                }
                elemStart += elemsize;
                subobjStop += elemsize;
            }
        }
    }
}

// Perform tracing through the engine's entire heap.
// After calling this, all unmarked objects are known unreachable.
static
void traceengine_major(gcengine* engine) {
    rootblock* start = engine->allroots.current;
    rootblock* block = start;
    do {
        for (int i = 0; i < rootblock_size; ++i) {
            if (block->roots[i].rootslot) {
                traceobj_major(engine, block->roots[i].rootslot);
            }
        }

        block = block->next_block;
    } while (block != start);
}

//TODO a babytrace, which also ignores objects outside of the nursery

//------------------------------------
// Cleanup

// Cleanup any unmarked objects held by this block.
// Set any marked objects held back to unmarked.
// Object slots that are not used are ignored.
static inline
void cleanup_block(gcengine* engine, objblock* block) {
    uint64_t registry = block->registry;
    for (int i = 0; i < objblock_size; ++i) {
        gc* obj = objFromBlock(block, i);
        //FIXME the following assumes the object is in tenure space
        if (isUsed(registry, i) && !isMarked(obj)) {
            cleanup_tenureobj(engine->finalizers, obj);
            registry = clrUsed(registry, i);
            tracking_subTenure(engine, obj->info);
        }
        else {
            clrMarked(obj);
        }
    }
    block->registry = registry;
}

// Cleanup any unmarked objects held by this engine.
// Set any marked objects back to unmarked.
// If any blocks are left without any object handles, those blocks are removed,
// but we make sure there at least one block is left in the ring.
static
void cleanupengine_major(gcengine* engine) {
    objblock* start_block = engine->allobjs.current;
    objblock* block = start_block;
    objblock* last_block = NULL;
    do {
        cleanup_block(engine, block);
        if (block->registry == emptyObjRegistry && last_block != NULL) {
            last_block->next_block = block->next_block;
            free(block);
            block = last_block->next_block;
        }
        else {
            last_block = block;
            block = block->next_block;
        }
    } while (block != start_block);
}

//TODO baby cleanup

//------------------------------------
// Collection

// Perform a major trace and collect cycle.
// That is, scan the entire engine and cleanup any unused data.
void gccollect_major(gcengine* engine) {
    traceengine_major(engine);
    cleanupengine_major(engine);
}

//TODO void gccollect_minor(gcengine* engine)

//------------------------------------
// User Data Allocation

size_t gcsizeof(gcinfo info) {
    return sizeofObj(info);
}

// Obtain a fresh pointer in tenured space.
// If no such pointer can be created, return NULL.
static
gc* tenure_alloc(gcengine* engine, gcinfo info) {
    void* data_p = malloc(sizeofObj(info));
    if (!data_p) {
        return NULL;
    }
    gc* obj = objring_alloc(&engine->allobjs);
    if (!obj) {
        free(data_p);
        return NULL;
    }
    obj->data_p = data_p;
    obj->info = info;
    tracking_addTenure(engine, info);
    return obj;
}

// The engine allocates a fresh region of uninitialized data which it manages.
// If memory cannot initially be found, this triggers a collection cycle.
// If the memory still cannot be allocated (incl. if the internal bookkeeping cannot be alloc'd), returns NULL.
// The contents of `info` must not change for the lifetime of the engine.
gc* gcalloc(gcengine* engine, gcinfo info) {
    gc* obj = tenure_alloc(engine, info);
    if (!obj) {
        gccollect_major(engine);
        return tenure_alloc(engine, info);
    }
    else {
        return obj;
    }
}

// Give the engine control of existing data.
// Thus, the passed pointer and all its aliases become obsolete, and must not be used in subsequent code.
// If internal bookkeeping cannot be allocated, returns NULL.
// Also, if this function fails, the pointer does not become stale, but is still owned by its previous owner.
// The contents of `info` must not change for the lifetime of the engine.
gc* gcgive(gcengine* engine, void* raw_data, gcinfo info) {
    gc* obj = objring_alloc(&engine->allobjs);
    if (!obj) {
        return NULL;
    }
    obj->data_p = raw_data;
    obj->info = info;
    tracking_addTenure(engine, info);
    return obj;
}

// The opposite of gcgive.
// The engine forgets about the object, and its underlying data is returned.
// BUT! What if you take the object while it is reachable?
// void* gctake(gcengine* engine, gc* obj)

//------------------------------------
// User Data Manipulation

// Return the `subobjix`th subobject from a fixed-format gc-managed object.
gc** gcsubobj(gc* obj, size_t subobjix) {
    return (gc**)obj->data_p + subobjix;
}

// Return a pointer to the unboxed data of a fixed-format gc-managed object,
// or all the data of a custom-format object.
// The returned pointer becomes stale after any call to `gcalloc` with the same engine that manages this handle.
// Thus, it is best to run this without storing it for very long: either do your calculations or update, then get out of the way.
void* gcraw(gc* obj) {
    if (isCustomFormat(obj->info)) {
        return obj->data_p;
    }
    else {
        return (gc**)obj->data_p + obj->info.as.fixed.num_subobjs;
    }
}

// Return the length of a fixed-format managed array.
// If the pointer is only to an object, return 1.
size_t arrlen(gc* obj) {
    return obj->info.as.fixed.arrlen;
}

// Return a subobject (as `gcsubobj`) of the `arrix`th object in a fixed-format managed array.
gc** gcarrsubobj(gc* obj, size_t arrix, size_t subobjix) {
    return (gc**)(obj->data_p + arrix * sizeofObjElem(obj->info)) + subobjix;
}

// Return the unboxed data (as `gcunbox`) of the `arrix`th object in a fixed-format managed array.
void* gcarrraw(gc* obj, size_t arrix) {
    return (gc**)(obj->data_p + arrix * sizeofObjElem(obj->info)) + obj->info.as.fixed.num_subobjs;
}

//------------------------------------
// Root Management

// Allocate a root slot from the passed engine and initialize it.
// The tracer won't follow NULL objects, so passing NULL is acceptable.
gcroot* new_gcroot(gcengine* engine, gc* new_root) {
    gcroot* new = rootring_alloc(&engine->allroots);
    new->rootslot = new_root;
    return new;
}

void set_gcroot(gcroot* rootslot, gc* new_root) {
    rootslot->rootslot = new_root;
}

void free_gcroot(gcroot* rootslot) {
    rootslot->rootslot = NULL;
}
