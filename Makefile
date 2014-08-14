CC:=gcc -std=c11 -pedantic -Wall -Iinclude -Isrc


bin/toy: include/cpgc.h src/
	$(CC) src/toy.c -o bin/toy
