CC:=gcc -std=c11 -pedantic -Wall -Iinclude -Isrc


bin/toy: include/cpgc.h src/
	$(CC) src/toy.c -o bin/toy

bin/cpgc.o: include/cpgc.h src/cpgc/config.h src/cpgc/internal.h src/cpgc/internal.c
	$(CC) -c src/cpgc/internal.c -o bin/cpgc.o