#include "stdio.h"
#include "cpgc.c"

int main(int argc, char **argv) {
    printf("Mesdames, messiurres, bon soir!\n");
    printf("gcinfo: %ld\ngc: %ld\n", sizeof(gcinfo), sizeof(gc));
    int testing_bmalloc64 = 1,
        testing_block_alloc = 1,
        testing_objring_alloc = 1,
        testing_basic_engine = 1;
    
    if (testing_bmalloc64) {
    printf("RUN: bitmask allocator...\n");
        uint64_t reg = emptyRegistry;
        int ix = -1;
        for (int i = 0; i < block_size*3; i++) {
            ix = bmalloc64(&reg);
            if (i < block_size && ix == -1) {
                printf("FAIL: Allocated too few: %d\n", i);
                exit(1);
            }
            if (i >= block_size && ix != -1) {
                printf("FAIL: Allocated too many: %d\n", i);
                exit(1); 
            }
        }
        for (int i = 0; i < block_size; ++i) {
            reg |= (uint64_t)0x1 << i;
            if (bmalloc64(&reg) != i) {
                printf("FAIL: couldn't allocate in the middle: from %p\n", (void*)reg);
                exit(1);
            }
            if ((ix = bmalloc64(&reg)) != -1) {
                printf("FAIL: Allocated too many after free: %d from %p\n", ix, (void*)reg);
                    exit(1);
            }
            if (reg != fullRegistry) {
                printf("FAIL: bitmask alloc into near-full registry didn't fill registry: %p\n", (void*)reg);
                exit(1);
            }
        }
    printf("PASS.\n");
    }

    if (testing_block_alloc) {
    printf("RUN: block allocator...\n");
        objblock* block = create_objblock();
        gc* last = NULL;
        for (int i = 1; i <= block_size+1; i++) {
            gc* p = block_alloc(block);
            if (last + 1 != p) {
                if (p == NULL && i <= block_size) {
                    printf("FAIL: Allocated too few gc: %d\n", i);
                    exit(1);
                }
                else if (last != NULL && p != NULL) {
                    printf("FAIL: Next gc ended up in a strange place:\n  %p after %p\n", p, last);
                    exit(1);
                }
            }
            last = p;
        }
    printf("PASS.\n");
    }

    if (testing_objring_alloc) {
    printf("RUN: objring allocator...\n");
        objring ring = create_objring();
        for (int i = 1; i <= 129; i++) {
            gc* p = objring_alloc(&ring);
            if (p == NULL) {
                printf("FAIL: couldn't allocate enough: %d\n", i);
                exit(1);
            }
        }
    printf("PASS.\n");
    }

    if (testing_basic_engine) {
    printf("RUN: basic engine...\n");
        gcinfo int_info = mk_gcinfo(0, sizeof(int));
        gcinfo intp_info = mk_gcinfo(1, 0);
        gcengine* e = create_gcengine();
        uint64_t* reg = &e->allobjs.current->registry;
        if (*reg != ~(uint64_t)0) {
            printf("FAIL: engine started with nozero objects: %p\n", (void*)*reg);
            exit(1);
        }
        int* raw_i = malloc(sizeof(int));
        *raw_i = 137;
        gc* i = gcgive(e, raw_i, int_info);
        if (*(int*)gcunboxed(i) != 137) {
            printf("FAIL: integer 137 data lost.\n");
            exit(1);
        }
        if (*reg != ~(uint64_t)1) {
            printf("FAIL: engine doesn't have one object after alloc i: %p\n", (void*)*reg);
            exit(1);
        }
        gc* i_p = gcalloc(e, intp_info);
        *gcsubobj(i_p, 0) = i;
        if (*gcsubobj(i_p, 0) != i) {
            printf("FAIL: pointer to i lost.\n");
            exit(1);
        }
        if (*(int*)gcunboxed(*gcsubobj(i_p, 0)) != 137) {
            printf("FAIL: couldn't trace back to integer 137.\n");
            exit(1);
        }
        if (*reg != ~(uint64_t)(1+2)) {
            printf("FAIL: engine doesn't have two objects after alloc i_p: %p\n", (void*)*reg);
            exit(1);
        }
        gc* nil = gcgive(e, NULL, int_info);
        if (*reg != ~(uint64_t)(1+2+4)) {
            printf("FAIL: engine doesn't have three objects after alloc nil: %p\n", (void*)*reg);
            exit(1);
        }

        setroot(e, i_p);
        traceengine_major(e);
        if (!isMarked(i) || !isMarked(i_p) || isMarked(nil)) {
            printf("FAIL: Improper marking after trace\n");
            printf("  nil %d\n", nil->info.mark);
            printf("  i %d\n", i->info.mark);
            printf("  i_p %d\n", i_p->info.mark);
            exit(1);
        }
        cleanupengine_major(e);
        if (isMarked(i) || isMarked(i_p)) {
            printf("FAIL: Improper marking after cleanup\n");
            printf("  i %d\n", i->info.mark);
            printf("  i_p %d\n", i_p->info.mark);
            exit(1);
        }
        if (*reg != ~(uint64_t)(1+2)) {
            printf("FAIL: nil not collected: %p\n", (void*)*reg);
            exit(1);
        }
        setroot(e, i);
        gccollect_major(e);
        if (*reg != ~(uint64_t)(1)) {
            printf("FAIL: i_p not collected: %p\n", (void*)*reg);
            exit(1);
        }

    printf("PASS.\n");
    }

    return 0;
}