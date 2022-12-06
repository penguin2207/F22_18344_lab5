all: swaps gups has-tsx

swaps: swaps.c
	gcc -g -std=gnu99 -O3 -pthread -o swaps swaps.c
gups: gups.c
	gcc -g -std=gnu99 -O3 -pthread -o gups gups.c

has-tsx: has-tsx.c tsx-cpuid.h
	gcc -g -std=gnu99 -O3 -pthread -o has-tsx has-tsx.c

clean:
	-rm gups swaps has-tsx *.o
