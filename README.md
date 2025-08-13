# 🚦 Simulación de Tráfico — Versión Secuencial y Paralela (OpenMP)

Este proyecto implementa una simulación simplificada de tráfico con **vehículos** y **semáforos**, en dos variantes:
- **Secuencial** (`simulacion_secuencial.c`)
- **Paralela** con **OpenMP** (`simulacion_paralela.c`)

El objetivo es comparar el rendimiento y la lógica entre una ejecución en un solo hilo y una ejecución aprovechando el paralelismo (dinámico, anidado, `sections` y `parallel for`).

---

## 📦 Compilación

### Secuencial
```bash
gcc -O2 -std=c11 simulacion_secuencial.c -o sim_seq
```

### Paralela (OpenMP)
```bash
gcc -O2 -std=c11 -fopenmp simulacion_paralela.c -o sim_omp
```

**Banderas usadas:**
- `-O2` → optimización estándar.
- `-std=c11` → estándar de C 2011.
- `-fopenmp` → activa soporte para OpenMP (solo versión paralela).

---

## ▶ Ejecución

Formato general:
```bash
./ejecutable  iteraciones  num_semaforos  num_vehiculos  longitud_carril
```

| Parámetro        | Significado                               |
|------------------|-------------------------------------------|
| `iteraciones`    | Cantidad de ciclos de simulación          |
| `num_semaforos`  | Número total de semáforos                 |
| `num_vehiculos`  | Número total de vehículos en la simulación|
| `longitud_carril`| Posiciones desde el cruce hasta el final   |

**Ejemplo:**
```bash
./sim_seq 10 4 20 5
./sim_omp 10 4 20 5
```

---

## ⚙ Control de hilos en OpenMP

Antes de ejecutar la versión paralela:
```bash
export OMP_NUM_THREADS=8   # usa 8 hilos iniciales
```

En el código paralelo:
- **`omp_set_dynamic(1)`** → permite ajuste dinámico de hilos.
- **`omp_set_nested(1)`** → habilita paralelismo anidado.
- El número de hilos puede ajustarse en tiempo de ejecución según la congestión.

---

## 📊 Salida esperada

En cada iteración se imprime:
```
Iteración 1 (hilos max: 8)
Vehículo 0 - Carril 0 - Posición: 0
Vehículo 1 - Carril 1 - Posición: 1
...
Semáforo 0 - Estado: 2
Semáforo 1 - Estado: 0
...
```
- **Vehículo N** → su ID, carril y posición actual.
- **Semáforo N** → su ID y estado (`0=ROJO, 1=AMARILLO, 2=VERDE`).

---

## 🧪 Ejemplos de uso

### Ejecución rápida (demo)
```bash
./sim_seq 5 3 10 4
export OMP_NUM_THREADS=4
./sim_omp 5 3 10 4
```

### Ejecución media
```bash
./sim_seq 10 4 20 5
export OMP_NUM_THREADS=8
./sim_omp 10 4 20 5
```

### Ejecución más pesada
```bash
./sim_seq 20 6 50 7
export OMP_NUM_THREADS=12
./sim_omp 20 6 50 7
```

---

## ⏱ Medición de tiempo
```bash
time ./sim_seq 10 4 20 5
export OMP_NUM_THREADS=8
time ./sim_omp 10 4 20 5
```

---

## 📝 Notas
- La versión paralela resuelve **condiciones de carrera** con arreglos de ocupación y directivas `atomic`.
- El uso de `schedule(dynamic)` en vehículos mejora el balanceo de carga cuando algunos carriles están bloqueados.
- El paralelismo anidado se usa en la actualización de semáforos como ejemplo didáctico.
- Las constantes de tiempos de los semáforos (`t_verde`, `t_amarillo`, `t_rojo`) se pueden ajustar para cambiar el flujo de tráfico.
