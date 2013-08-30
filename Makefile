
CC:=$(CC) -Iinclude -Isrc -Wall





IMPL_SRC=src/impl.h src/impl.c \
         src/alloc.h   src/gateway.h   src/trace.h   src/init.h  \
         src/alloc.inc src/gateway.inc src/trace.inc src/init.inc
bin/impl.o: $(IMPL_SRC)
	$(CC) -c $(CFLAGS) src/impl.c
