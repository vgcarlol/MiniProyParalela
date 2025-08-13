CC=gcc
CFLAGS=-O2 -std=c11
OMPFLAGS=-fopenmp
all: sim_seq sim_omp
sim_seq: simulacion_secuencial.c
	$(CC) $(CFLAGS) $< -o sim_seq
sim_omp: simulacion_paralela.c
	$(CC) $(CFLAGS) $(OMPFLAGS) $< -o sim_omp
clean:
	rm -f sim_seq sim_omp
