#ifndef CPGC_H
#define CPGC_H


#include <stddef.h>
#include <inttypes.h>


//supply and track all overhead data for a single garbage collection thread
typedef struct gcEngine gcEngine;

//smart pointer to garbage-collected data
typedef struct gcGateway gcGateway;
//a gcRef is not moved as long as the reference is still valid (accessible and not free'd)
typedef gcGateway* gcRef;

//describes a garbage-collected datum: everything needed to trace and collect
typedef struct gcMeta {
    size_t refs; //number of gc pointers to managed data
    size_t raw; //bytes of unmanaged data
    void (*cleanup)(void* obj); //function called before collection. NULL is no cleanup. cleanup must not rely on the existence or non-existance of other managed data
} gcMeta;


//each engine is entirely isolated
void gc_createEngine(gcEngine* out);

//clean up every managed value and then cleanup the engine itself
void gc_destroyEngine(gcEngine* engine);

void gc_addRoot(gcEngine* engine, gcRef obj);

void gc_remRoot(gcEngine* engine, gcRef obj);
//TODO convenience&efficiency: swap root -- del one and add the other

//mark a gateway as being a reference cell (and therefore may point to newer values)
void gc_addCell(gcEngine* engine, gcRef obj);

//prepare a handle to store a single datum
gcRef gc_allocValue(gcEngine* engine, gcMeta* meta);

//prepare a handle to store an array of homogenous (same metadata) data
gcRef gc_allocUnboxedArr(gcEngine* engine, gcMeta* meta, size_t arrLen);

//prepare a handle to store an array of heterogenous data (each datum is a handle)
gcRef gc_allocBoxedArr(gcEngine* engine, size_t arrLen);

//TODO obtain ptr to data: the client must use it right, or else
//TODO set ptr to data: the client must ensure the gc can be the owner of that data
//TODO convenience: load data, copy handle & data, copy data out (extract)
//TODO copy handle&data to another engine
//TODO deep copy (move to an engin-external assignable, such as a ref or a channel)


#endif