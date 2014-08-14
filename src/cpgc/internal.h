#ifndef CPGC_INTERNAL_H
#define CPGC_INTERNAL_H

#include "cpgc/config.h"
#include "cpgc.h"


struct gcGateway {
    gcMeta* meta; //NULL when a heterogenous array 
    size_t arrLen; //zero when not an array
    uint8_t mark_age;
    uint8_t* dataptr;
};

typedef struct gcGatewayBlock {
    uintmax_t num_used;
    gcGateway gateways[8*sizeof(uintmax_t)]; //here are the actual gateways, which do not move as long as they are valid
    gcRef queue[8*sizeof(uintmax_t)]; //this memory is used during tracing to store the visit queue
        //even if every handle is used, the queue space allocated will be sufficient to trace each reference once
} gcGatewayBlock;


typedef struct gcGatewayRegistry {
    gcGatewayBlock block0;
    gcGatewayBlock* sinceLastAlloc;
    gcGatewayBlock* sinceLastMinor;
} gcGatewayRegistry;

typedef struct gcNursery {
    size_t top; //bytes allocated in the nursery
    uint8_t base[BYTES_PER_NURSERY];
} gcNursery;

struct gcEngine {
    gcGatewayRegistry registry;
    gcNursery* activeNursery;
    gcNursery* agingNursery;
    //TODO root list
    //TODO cell list
};


//allocate uninitialized memory and an initialized gateway to that memory
gcRef gc_alloc(gcEngine* engine, gcMeta* meta, size_t arrLen);
    //calculate from meta and n how much memory needs to be allocated
    //allocate memory from either nursery or tenured space (may trigger a collection)
    //find an unused handle in the registry (or exapand registry)
    //initialize metadata and pointer to alloc'd mem
    //mark it used, then return it by reference
    
    //return a pointer to a fresh block of data allocated from the nursery, or NULL on failure
    void* gc_nalloc(gcEngine* engine, size_t bytes);

    //return a pointer to a fresh block of data allocated from tenured space, or NULL on failure
    void* gc_talloc(gcEngine* engine, size_t bytes);


//TODO a trace function

void gc_sweep(gcEngine engine, uint8_t age);

    void gc_sweepBlock(gcGatewayBlock* reg, uint8_t age);
        //search a block for gateways not marked since age
        //any such gateway is cleaned up and marked unused


#endif