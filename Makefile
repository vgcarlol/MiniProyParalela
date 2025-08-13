# Compilador y banderas
CC       = gcc
CFLAGS   = -O2 -std=c11
OMPFLAGS = -fopenmp

# Archivos fuente
SEQ_SRC  = simulacion_secuencial.c
OMP_SRC  = simulacion_paralela.c

# Ejecutables
SEQ_BIN  = sim_seq
OMP_BIN  = sim_omp

# Parámetros de ejecución por defecto
ITER     = 10
NSEM     = 4
NVEH     = 20
LCARRIL  = 5

# Archivos de salida
SEQ_OUT  = salida_secuencial.txt
OMP_OUT  = salida_paralela.txt

# Regla principal: compilar y ejecutar ambos
all: run_all

# Compilación versión secuencial
$(SEQ_BIN): $(SEQ_SRC)
	$(CC) $(CFLAGS) $< -o $@

# Compilación versión OpenMP
$(OMP_BIN): $(OMP_SRC)
	$(CC) $(CFLAGS) $(OMPFLAGS) $< -o $@

# Ejecutar solo la secuencial y guardar salida
run_seq: $(SEQ_BIN)
	./$(SEQ_BIN) $(ITER) $(NSEM) $(NVEH) $(LCARRIL) > $(SEQ_OUT)

# Ejecutar solo la paralela y guardar salida
run_omp: $(OMP_BIN)
	./$(OMP_BIN) $(ITER) $(NSEM) $(NVEH) $(LCARRIL) > $(OMP_OUT)

# Ejecutar ambas versiones y guardar salidas
run_all: $(SEQ_BIN) $(OMP_BIN)
	./$(SEQ_BIN) $(ITER) $(NSEM) $(NVEH) $(LCARRIL) > $(SEQ_OUT)
	./$(OMP_BIN) $(ITER) $(NSEM) $(NVEH) $(LCARRIL) > $(OMP_OUT)
	@echo "Salidas guardadas en $(SEQ_OUT) y $(OMP_OUT)"

# Limpieza
clean:
	rm -f $(SEQ_BIN) $(OMP_BIN) $(SEQ_OUT) $(OMP_OUT)

.PHONY: all clean run_seq run_omp run_all
