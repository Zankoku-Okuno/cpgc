#include "stdint.h"
#include "stdlib.h"
#include "cpgc.h"

static const size_t block_size = 64;

typedef struct objblock objblock;
typedef struct objring objring;

//------------------------------------
// Data Structures

////// Object Info //////
typedef enum { UNMARKED = 0, MARKED = 1 } mark;

struct gcinfo {
    size_t num_unboxed_bytes : 32;
    size_t num_subobjs : 31;
    //FIXME make space in the word to give an contiguous-array length
    //TODO a flag to say "this is a pointer to the real info", which can then have a finalizer and larger bounds
    mark mark : 1;
};
gcinfo mk_gcinfo(size_t num_subobjs, size_t num_unboxed_bytes) {
    gcinfo out;
    out.num_subobjs = num_subobjs;
    out.num_unboxed_bytes = num_unboxed_bytes;
    out.mark = UNMARKED;
    return out;
}

// Compute the full size of a managed object.
static inline
size_t sizeofLayout(gcinfo info) {
    return info.num_subobjs * sizeof(gc*) + info.num_unboxed_bytes;
}


////// Objects //////
struct gc {
    void* data_p;
    gcinfo info;
};
static inline
void init_obj(gc* obj, gcinfo info) {
    obj->info = info;
}
static inline
void cleanup_obj(gc* obj) {
    // if (obj->info->cleanup) {
    //     obj->info->cleanup(obj->data_p);
    // }
    free(obj->data_p);
}

static inline
int isMarked(gc* obj) {
    return obj->info.mark == MARKED;
}
static inline
void setMarked(gc* obj) {
    obj->info.mark = MARKED;
}
static inline
void clrMarked(gc* obj) {
    obj->info.mark = UNMARKED;
}

static inline
gc** getsubobj(gc* obj, size_t ix) {
    //FIXME bounds-check ix, or at least assert it
    return (gc**)obj->data_p + ix;
}
static inline
void* getunboxed(gc* obj) {
    return (gc**)obj->data_p + obj->info.num_subobjs;
}

////// Blocks //////
static const uint64_t emptyRegistry = ~(uint64_t)0;
static const uint64_t fullRegistry = (uint64_t)0;

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

struct objblock {
    uint64_t registry;
    objblock* next_block;
    gc objs[block_size];
};
static inline
gc* objFromBlock(objblock* block, int ix) {
    return &block->objs[ix];
}
static
objblock* create_objblock() {
    objblock* new = malloc(sizeof(objblock));
    new->registry = emptyRegistry;
    return new;
}
void destroy_objblock(objblock* block) {
    uint64_t registry = block->registry;
    for (int i = 0, e = block_size; i < e; ++i) {
        if (isUsed(registry, i)) {
            cleanup_obj(objFromBlock(block, i));
        }
    }
    free(block);
}


////// Handle Rings //////
struct objring {
    objblock* current;
    objblock* last_collect;
};
objring create_objring() {
    objring res;
    res.current = res.last_collect = create_objblock();
    res.current->next_block = res.current;
    return res;
}
void destroy_objring(objring ring) {
    objblock* start = ring.current;
    objblock* current = start;
    do {
        objblock* next = current->next_block;
        destroy_objblock(current);
        current = next;
    } while (current != start);
}

////// Engines //////
struct gcengine {
    objring allobjs;
    gc* root; //TODO for simplicity, I'm just going with one root, presumably top of stack or some sort of overall interpreter control structure, but this probably needs multiple roots: how the rootset commonly changes determines my strategy
};
gcengine* create_gcengine() {
    gcengine* new = malloc(sizeof(gcengine));
    new->allobjs = create_objring();
    new->root = NULL;
    return new;
}
void destroy_gcengine(gcengine* engine) {
    engine->root = NULL;
    destroy_objring(engine->allobjs);
    free(engine);
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
gc* block_alloc(objblock* block) {
    int ix = bmalloc64(&block->registry);
    return 0 <= ix ? objFromBlock(block, ix) : NULL;
}

//Allocate a new gc pointer in the context of the passed engine.
//If the machine is out of memory, returns NULL.
static
gc* objring_alloc(objring* ring) {
    objblock* block = ring->current;
    for (;;) {
        gc* new = block_alloc(block);
        if (new) {
            return new;
        }
        else {
            if (block == ring->last_collect) {
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

//------------------------------------
// Tracing

// Trace a single object during major collection.
// C.f. `traceobj_minor`.
static
void traceobj_major(gc* obj) {
    if (!obj | isMarked(obj)) return;
    else {
        setMarked(obj);

        gc** subobj = (gc**)obj->data_p;
        gc** stop = subobj + obj->info.num_subobjs;
        for (; subobj < stop; ++subobj) {
            traceobj_major(*subobj);
        }
    }
}

// Perform tracing through the engine's entire heap.
// After calling this, all unmarked objects are known unreachable.
static
void traceengine_major(gcengine* engine) {
    traceobj_major(engine->root);
}

//TODO a babytrace, which also ignores objects outside of the nursery

//------------------------------------
// Cleanup

// Cleanup any unmarked objects held by this block.
// Set any marked objects held back to unmarked.
// Object slots that are not used are ignored.
static inline
void cleanup_block(objblock* block) {
    uint64_t registry = block->registry;
    for (int i = 0; i < block_size; ++i) {
        gc* obj = objFromBlock(block, i);
        if (isUsed(registry, i) && !isMarked(obj)) {
            cleanup_obj(obj);
            registry = clrUsed(registry, i);
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
    objblock* start_block = engine->allobjs.last_collect;
    objblock* block = start_block;
    objblock* last_block = NULL;
    do {
        cleanup_block(block);
        if (block->registry == emptyRegistry && last_block != NULL) {
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


// Obtain a fresh pointer in tenured space.
// If no such pointer can be created, return NULL.
static
gc* tenure_alloc(gcengine* engine, gcinfo info) {
    void* data_p = malloc(sizeofLayout(info));
    if (!data_p) {
        return NULL;
    }
    gc* obj = objring_alloc(&engine->allobjs);
    if (!obj) {
        free(data_p);
        return NULL;
    }
    init_obj(obj, info);
    obj->data_p = data_p;
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
    init_obj(obj, info);
    obj->data_p = raw_data;
    return obj;
}

//TODO void* gctake(gc* obj)
//the opposite of gcgive
//the engine forgets about the object, and its underlying data is returned

//------------------------------------
// User Data Manipulation


// Return the ith subobject from a gc-managed object.
gc** gcsubobj(gc* obj, size_t ix) {
    return getsubobj(obj, ix);
}

// Return a pointer to the unboxed data of a gc-managed object.
// The returned pointer becomes stale after any call to `gcalloc` with the same engine that manages this handle.
// Thus, it is best to run this without storing it for very long: either do your calculations or update, then get out of the way.
void* gcunboxed(gc* obj) {
    return getunboxed(obj);
}

//------------------------------------
// Root Management

//DEPRECATED
void setroot(gcengine* engine, gc* new_root) {
    engine->root = new_root;
}
