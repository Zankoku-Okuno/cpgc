#include "cpgc.h"
#include <inttypes.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

static const size_t handleBlockSize = 8 * sizeof(uintmax_t);


struct gcEngine {
    gcGateway* objs;
    size_t len, cap;
};

struct gcGateway {
    gcMeta* meta; //NULL when a heterogenous array 
    size_t arrLen; //zero when not an array
    bool root;
    bool marked;
    uint8_t* dataptr;
};


static inline gcRef* subObj(gcRef superObj, size_t n) {
    return (gcRef*)( superObj->dataptr + n * sizeof(gcRef) );
}
static inline void* subData(gcRef obj, size_t offset_bytes) {
    return (void*)( obj->dataptr + obj->meta->refs * sizeof(gcRef) + offset_bytes);
}


//have the engine supply fresh handles (no backing data or metadata)
static gcRef gc_createRef(gcEngine* engine);

//allocate space from the engine, which may require looking at metadata to decide policy
static uint8_t* gc_alloc(gcEngine* engine, gcMeta* meta, size_t n);

static void gc_free(gcRef obj);

static void gc_collect(gcEngine* engine);


void gc_createEngine(gcEngine* out) {
    printf("Creating engine at <%p>...\n", (void*)out);
    out->objs = malloc(handleBlockSize * sizeof(gcGateway));
    if (out->objs == NULL) {
        out = NULL;
        return;
    }
    out->len = 0;
    out->cap = handleBlockSize;
    printf("Engine created.\n");
}

void gc_destroyEngine(gcEngine* engine) {
    printf("Tearing down engine at <%p>...\n", (void*)engine);
    for (size_t i = 0, e = engine->len; i < e; ++i) {
        gc_free(&engine->objs[i]);
    }
    free(engine->objs);
    engine->len = 0;
    engine->cap = 0;
    printf("Engine torn down.\n");
}

void gc_addRoot(gcEngine* engine, gcRef ref) {
    ref->root = true;
}
void gc_remRoot(gcEngine* engine, gcRef ref) {
    ref->root = false;
}


gcRef gc_allocValue(gcEngine* engine, gcMeta* meta) {
    gcRef out = gc_createRef(engine);
    out->meta = meta;
    out->arrLen = 0;
    out->dataptr = gc_alloc(engine, meta, 1);
    return out->dataptr == NULL ? NULL : out;
}

gcRef gc_allocUnboxedArr(gcEngine* engine, gcMeta* meta, size_t arrLen) {
    gcRef out = gc_createRef(engine);
    out->meta = meta;
    out->arrLen = arrLen;
    out->dataptr = gc_alloc(engine, meta, arrLen);
    return out->dataptr == NULL ? NULL : out;
}

gcRef gc_allocBoxedArr(gcEngine* engine, size_t arrLen) {
    gcRef out = gc_createRef(engine);
    out->meta = NULL;
    out->arrLen = arrLen;
    out->dataptr = gc_alloc(engine, NULL, arrLen);
    return out->dataptr == NULL ? NULL : out;
}


gcRef gc_createRef(gcEngine* engine) {
    gcRef out = NULL;
    {
        //search for unused handle
        for (size_t i = 0, e = engine->len; i < e; ++i) {
            if (engine->objs[i].dataptr == NULL) {
                out = &engine->objs[i];
                goto end;
            }
        }
        if (engine->len != engine->cap) {
            out = &engine->objs[engine->len++];
            goto end;
        }
        else {
            //run a collection and re-search
            gc_collect(engine);
            for (size_t i = 0, e = engine->len; i < e; ++i) {
                if (engine->objs[i].dataptr == NULL) {
                    out = &engine->objs[i];
                    goto end;
                }
            }
            //as a last resort, expand the capacity of our handle set if needed,
            //TODO we need to alloc another block as part of a linked list
            printf("Exceeded object capacity.\n");
            exit(1);
        }
    }
    end:
    if (out == NULL) {
        return NULL;
    }
    printf("Alloc'd object <%p>.\n", (void*) out);
    out->root = out->marked = false;
    return out;
}

uint8_t* gc_alloc(gcEngine* engine, gcMeta* meta, size_t n) {
    size_t objSize;
    if (meta == NULL) {
        objSize = sizeof(gcRef);
    }
    else {
        objSize = meta->refs * sizeof(gcRef) + meta->raw;
    }
    uint8_t* ptr = malloc(objSize == 0 ? 1 : objSize * n);
    return ptr;
}

//NOTE will fail when cleanup relies on gc'd data, so don't do it
void gc_free(gcRef obj) {
    if (obj->dataptr != NULL) {
        if (obj->meta->cleanup != NULL) {
            obj->meta->cleanup(obj->dataptr);
        }
        free(obj->dataptr);
        obj->dataptr = NULL;
        printf("Freed object <%p>.\n", (void*) obj);
    }
}

static void gc_recursiveMark(gcRef obj) {
    if (obj == NULL) {
        return;
    }
    if ((obj->dataptr == NULL) | obj->marked) {
        printf("    already traced.\n");
        return;
    }
    obj->marked = true;
    printf("    tracing <%p>...\n", (void*)(obj));
    for (size_t i = 0, e = obj->meta->refs; i < e; ++i) {
        gcRef* next = (gcRef*)(obj->dataptr + i * sizeof(gcRef));
        printf("    following to <%p>\n", (void*)(*next));
        gc_recursiveMark(*next);
    }
    printf("    traced <%p>.\n", (void*)(obj));
}

static void gc_collect(gcEngine* engine) {
    size_t e = engine->len;
    //run through and unmark
    printf("Collecting: unmarking...\n");
    for (size_t i = 0; i < e; ++i) {
        engine->objs[i].marked = false;
    }
    //run through looking for roots, start a depth-first search from each
    printf("Collecting: marking...\n");
    for (size_t i = 0; i < e; ++i) {
        if (engine->objs[i].root == true) {
            gc_recursiveMark(&engine->objs[i]);
        }
    }
    //run through and free unmarked stuff
    printf("Collecting: sweeping...\n");
    for (size_t i = 0; i < e; ++i) {
        if (engine->objs[i].marked == false) {
            gc_free(&engine->objs[i]);
        }
    }
    printf("Collecting: done.\n");
}



void showEngine(gcEngine* engine) {
    printf("Engine (%zu of %zu): ", engine->len, engine->cap);
    for (int i = 0, e = engine->len; i < e; ++i) {
        gcGateway obj = engine->objs[i];
        if (obj.dataptr == NULL) {
            printf("0");
        }
        else if (obj.root) {
            printf("R");
        }
        else if (obj.marked) {
            printf("1");
        }
        else {
            printf("u");
        }
    }
    printf("\n");
}



static gcMeta intMeta = { 0, sizeof(int32_t), NULL };
static gcMeta nilMeta = { 0, 0, NULL };
static gcMeta consMeta = { 2, 0, NULL };

int main(int argc, char** argv) {
    gcEngine engine;
    gcEngine* ep = &engine;
    gc_createEngine(ep);
    {
        gcRef nil = gc_allocValue(ep, &nilMeta);
        gcRef i0 = gc_allocValue(ep, &intMeta);
        {
            *(int32_t*)subData(i0, 0) = 137;
            // *(int32_t*)(i0->dataptr) = 137;
        }
        gcRef i1 = gc_allocValue(ep, &intMeta);
        {
            *(int32_t*)subData(i1, 0) = 42;
        }
        gcRef cons0 = gc_allocValue(ep, &consMeta);
        {
            *subObj(cons0, 0) = i0;
            *subObj(cons0, 1) = nil;
        }
        gcRef cons1 = gc_allocValue(ep, &consMeta);
        {
            *subObj(cons1, 0) = i1;
            *subObj(cons1, 1) = cons0;
        }

        gc_addRoot(ep, cons1);
        gc_collect(ep);
        showEngine(ep);
        gc_remRoot(ep, cons1);
        gc_addRoot(ep, cons0);
        gc_collect(ep);
        showEngine(ep);
    }
    gc_destroyEngine(ep);
}