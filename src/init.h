#ifndef INIT_H
#define INIT_H


//REFAC when we make these configable, move to their own location
static int MAX_HEAP_SIZE;
static int REG_BLOCK_SIZE;
static int NURSERY_SIZE;
static int SKIP_NURSERY_THRESHOLD;
static int SUGGESTED_QUEUE_SIZE;


/**
 * Ready this thread for using the garbage collection system.
 * Any use of these gc functions results in undefined behavior if `gc_init` hasn't yet been called.
 */
void gc_init();

/**
 * Teardown the garbage collection system in this thread, including running finalizers on all living objects.
 * Any use of these gc functions results in undefined behavior after `gc_finish` is called.
 */
void gc_finish();


/**
 * Data structure to manage the nursery.
 */
typedef struct nursery_t nursery_t;

/**
 * Get a handle to this thread's nursery.
 */
static inline nursery_t* getNursery();


/**
 * Abstract type to allocate and track all gateways.
 */
typedef struct registry_t registry_t;

/**
 * Get a handle to this thread's registry.
 */
static inline registry_t* getRegistry();

/**
 * During gc, the gateway registry should be cleaned up as well.
 * This is because the gcobj handles must be left in place until object death, but many objects are expected to die.
 */
static void clean_registry(int stage);


/**
 * Data structures needed by the tracer.
 */
typedef struct trace_engine_t trace_engine_t;

/**
 * Get a handle to this thread's tracer.
 */
static inline trace_engine_t* getTracer();


#endif