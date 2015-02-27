
CC:=$(CC) -Iinclude -Isrc -Wall




IMPL_H=cpgc.h
IMPL_C=cpgc.c
IMPL_SRC=$(IMPL_H) $(IMPL_C)


test: main.c
	$(CC) main.c
	./a.out

build: bin/cpgc.o

bin/cpgc.o: $(IMPL_SRC)
	$(CC) -O3 -c $(CFLAGS) $(IMPL_C) -o bin/cpgc.o


clean:
	rm -f bin/cpgc.o