
all : cache.o test.c
	gcc -o test test.c cache.o 

cache.o : cache.c cache.h
	gcc -c -g cache.c

clean :
	rm *.o test
