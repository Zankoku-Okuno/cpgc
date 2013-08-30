#include "heap.h"

/*
 * Gateways are maintained in a list of blocks. Unused slots for gateways are simply nulls.
 * The free list should notice when a block conatins no live gateways and free that block.
 * This should give good creation performance, at the expense of some complexity during collection.
 *
 * When an object is first allocated, and is small enough, it is placed in the nursery.
 * The nursery is a pre-allocated block of memory using stack allocation.
 * When the nursery is filled, we perform a minor collection.
 * 
 * During minor collection, nothing need be free'd. However, surviving objects need to be moved into their 
 * own piece of memory, and surviving gateways will need to be updated.
 * During a major collection, we don't need to update gateways, but dead gateways will need to be collected.
 */

// ============ Tunable Parameters ============ //

static int MAX_HEAP_SIZE; //STUB

static int REG_BLOCK_SIZE = 32;

static int NURSERY_SIZE = 512*1024;
static int SKIP_NURSERY_THRESHOLD = 1024;

static int SUGGESTED_QUEUE_SIZE = 128;


// ============ Gateways ============ //

typedef struct gc_gate {
    void* data;
    size_t bytes;
    int marked;
    void (*trace)(tracer* engine, void* obj);
    void (*destroy)(void* obj);
} gc_gate;


typedef struct reg_node {
    size_t filled;
    gc_gate* data;
    reg_node* next;
    reg_node* prev;
} reg_node;


static thread_local struct {
    reg_node* root;
    //these help speed up the search for a place to put a new gateway
    reg_node* start;
    int       start_in_block;
    //this helps narrow the search for sweeping during minor collections
    reg_node* minor_root;
} registry;


static inline reg_node* new_registry_node(reg_node* from) {
    reg_node* new = malloc(sizeof reg_node);
    if (!new) abort(); //FIXME provide message
      new->filled = 0;
      new->data = malloc(REG_BLOCK_SIZE*sizeof(gc_gate));
      if (!new->data) abort(); //FIXME provide message
      for(int i = 0; i < REG_BLOCK_SIZE; ++i) new->data[i].data = NULL;
      new->next = NULL;
      new->prev = from;
      if (from) from->next = new;
    return new;
}


gc_gate* new_gate(size_t bytes) {
    gc_gate* gateway;
    {
        //make sure we're in a block that has somewhere to track a gateway
        while (registry.start->filled >= REG_BLOCK_SIZE) {
            reg_node* new = registry.start->next ? registry.start->next : new_registry_node(registry.start);
            registry.start = new;
        }
        //look for an empty slot in the block (it's there, because we checked above)
        while(registry.start->data[registry.start_in_block].data != NULL) {
            registry.start_in_block += 1;
            registry.start_in_block %= REG_BLOCK_SIZE;
        }
        gateway = &registry.start->data[registry.start_in_block];
        //update the start vars
        registry.start_in_block++;
        registry.start->filled++;
    }
    //clear the gateway data
    gateway->bytes = bytes;
    gateway->marked = 0;
    return gateway;
}

// ============ Heap Memory Area ============ //

//region for new objects
static thread_local struct {
    byte* data; //holds the data
    byte* top;  //first unoccupied byte
    byte* end;  //byte after last data byte
} nursery;

static inline int in_nursery(void* data) {
    return (nursery.data <= data) & ( data < nursery.end);
}

// ============ Tracing ============ //

typedef struct {
    void* ptr;
    void (*trace)(void*);
} root_entry;

static thread_local struct {
    int in_major;
    struct {
        root_entry* at;
        size_t len;
        size_t cap;
    } root;
    struct {
        int read_a;
        struct {
            gc_gate** buf;
            int len;
            int cap;
        } a;
        struct {
            gc_gate** buf;
            int len;
            int cap;
        } b;
    } queue;
} tracer;


void gc_mark(gc_gate* x) {
    //Don't bother re-queuing already marked objects
    //If we're in minor collection, don't bother with anythong outside the nursery.
    if (x->marked || !tracer.in_major && !in_nursery(x->data)) return;
    else {
        //Mark the gateway.
        x->marked = 1;
        //Add the gateway to the tracing queue.
        if (tracer.queue.read_a) {
            if (tracer.queue.b.len >= tracer.queue.b.cap) {
                tracer.queue.b.cap += SUGGESTED_QUEUE_SIZE;
                tracer.queue.b.buf = realloc(tracer.queue.b.buf, tracer.queue.b.cap);
                if (!tracer.queue.b.buf) abort(); //FIXME provide message
            }
            tracer.queue.b.buf[tracer.queue.b.len++] = x;
        }
        else {
            if (tracer.queue.a.len >= tracer.queue.a.cap) {
                tracer.queue.a.cap += SUGGESTED_QUEUE_SIZE;
                tracer.queue.a.buf = realloc(tracer.queue.a.buf, tracer.queue.a.cap);
                if (!tracer.queue.a.buf) abort(); //FIXME provide message
            }
            tracer.queue.a.buf[tracer.queue.a.len++] = x;
        }
    }
}


void trace(int stage) {
    tracer.in_major = stage;
    //Trace each root.
    for(size_t i = 0; i < tracer.roots.len; ++i)
        tracer.roots.at[i].trace();
    //While there has been a write to the queue (see `break`s below).
    for(;;) {
        tracer.queue.read_a = !tracer.queue.read_a;
        //Pop and trace from a.
        if (tracer.queue.read_a) {
            if (!tracer.queue.a.len) break;
            for(int i = 0; i < tracer.queue.a.len; ++i)
                tracer.queue.a.buf[i]->trace();
        }
        //Pop and trace from b.
        else {
            if (!tracer.queue.b.len) break;
            for(int i = 0; i < tracer.queue.b.len; ++i)
                tracer.queue.b.buf[i]->trace();
        }
    }
}

reg_node* cleanup_reg_node(reg_node* node) {
    //Get rid of empty registry nodes.
    if ((node->filled == 0) & (node->prev)) {
        node->prev->next = node->next;
        free(node->data);
        free(node);
        if (node == registry.start) return node->prev; //HAX around if start is the only non-full reg_node (which may yet be the case)
    }
    //Setup new_gateway to start search at first non-full node.
    else {
        if (node->filled < REG_BLOCK_SIZE || node == registry.start) return node;
    }
    return NULL;
}


void minor_gc() {
    //Mark reachable objects.
    trace(0);
    //Finalize dead gateways, move data of live gateways into tenure.
    reg_node* reset_start = NULL;
    for(reg_node* node = registry.minor_root, *next; node; node = next) {
        for (int i = 0; i < REG_BLOCK_SIZE; ++i) {
            if (node->data[i].marked) {
                node->data[i].marked = 0;
                void* new = malloc(node->data[i].bytes);
                if (!new) abort //FIXME provide message
                memcpy(new, node->data[i].data, node->data[i].bytes);
                node->data[i].data = new;
            }
            else if (in_nursery(node->data[i].data)) {
                if (node->data[i].destroy) node->data[i].destroy(node->data[i].data);
            }
        }
        next = node->next;
        if (reset_start) 
            reset_start = cleanup_reg_node(node);
        else
            cleanup_reg_node(node);
        if (node == registry.start) break;
    }
    //Reset registry gate finding.
    if (reset_start) registry.minor_root = registry.start = reset_start;
}

void major_gc() {
    //Mark reachable objects.
    trace(1);
    //Finalize dead gateways.
    reg_node* reset_start = NULL;
    for(reg_node* node = registry.root, *next; node; node = next) {
        for (int i = 0; i < REG_BLOCK_SIZE; ++i) {
            if (node->data[i].marked) {
                node->data[i].marked = 0;
            }
            else if (node->data[i].data) {
                if (node->data[i].destroy) node->data[i].destroy(node->data[i].data);
                free(node->data[i].data);
                node->filled--;
            }
        }
        next = node->next;
        if (reset_start) 
            reset_start = cleanup_reg_node(node);
        else
            cleanup_reg_node(node);
    }
    //Reset registry gate finding.
    registry.minor_root = registry.start = reset_start;
}

/*during collection

minor collections are a little different:
    just run the finalizer on the dead gateways
    copy live memory out of the nursery & update gateway data pointers
also, a major collection may first need to perform a minor collection, unless I add complexity

*/


// ============ Objects ============ //

gcobj new_gcobj( size_t bytes, const void* x
               , void (*trace)(tracer* engine, void* obj)
               , void (*destroy)(void* obj)
               )
{
    //Grab a fresh gateway.
    gc_gate* gateway = new_gate(size_t bytes);
      gateway->trace   = trace;
      gateway->destroy = destroy;
    //Big objects bypass the nursery.
    if (bytes >= SKIP_NURSERY_THRESHOLD) {
        //Make a copy of the original.
        void* copy = malloc(bytes);
        if (!copy) abort(); //FIXME provide message
        memcpy(copy, x, bytes);
        //Update the gateway data pointer.
        gateway->data = copy;
    }
    else {
        //Check if the nursery is full, and garbage collect if so.
        if (nursery.top + bytes >= nursery.end) minor_gc();
        //Move data into the managed heap
        memcpy(nursery.top, x, bytes);
        nursery.top += bytes;
        //Update the gateway data pointer.
        gateway->data = nursery.top;
    }
    //Hand over only the gateway.
    return gateway;
}

// ============ Initialize ============ //

void init_gc() {
    //Set up gateway registry.
    {
        registry.root = new_registry_node(NULL);
        registry.start = registry.root;
        registry.start_in_block = 0;
        registry.minor_root = registry.root;
    }
    //Set up nursery.
    {
        nursery.data = malloc(NURSERY_SIZE);
        if(!nursery.data) abort(); //FIXME provide message
        nursery.top = nursery.data;
        nursery.end = nursery.data + NURSERY_SIZE;
    }
}
