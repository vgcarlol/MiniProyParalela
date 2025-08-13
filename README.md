# Simulación de Ecosistema — Versión Secuencial y Paralela (OpenMP)

Este proyecto implementa una simulación simplificada de un ecosistema con **plantas**, **herbívoros** y **depredadores**, en dos variantes:
- **Secuencial** (`simulacion_secuencial.c`)
- **Paralela** con **OpenMP** (`simulacion_paralela.c`)

Permite comparar rendimiento entre ejecución en un solo hilo y en múltiples hilos.

---

## 📦 Compilación

### Secuencial
```bash
gcc -O3 -march=native -std=c11 simulacion_secuencial.c -o sim_seq.exe
```

### Paralela (OpenMP)
```bash
gcc -O3 -march=native -std=c11 -fopenmp simulacion_paralela.c -o sim_omp.exe
```

**Banderas usadas:**
- `-O3` → optimización alta.
- `-march=native` → usa instrucciones específicas de tu CPU.
- `-std=c11` → estándar de C 2011.
- `-fopenmp` → activa soporte para OpenMP (solo versión paralela).

---

## ▶ Ejecución

Formato general:
```bash
./ejecutable  W  H  steps  nHerb  nPred  seed
```

| Parámetro | Significado | Efecto |
|-----------|-------------|--------|
| `W`       | Ancho del mundo (número de celdas) | Más grande → más trabajo de cómputo |
| `H`       | Alto del mundo | Igual que `W` |
| `steps`   | Pasos de simulación | Más pasos → más tiempo y evolución |
| `nHerb`   | Herbívoros iniciales | Afecta rapidez de consumo de plantas |
| `nPred`   | Depredadores iniciales | Afecta presión sobre herbívoros |
| `seed`    | Semilla aleatoria | Igual semilla = misma simulación |

---

## 🌱 Parámetros del modelo (en el código)

En ambos `.c` hay valores por defecto si no se pasan argumentos:
```c
P.p_regrow = 0.02f;  // Probabilidad de rebrote por celda y paso
P.E_move_cost = 1;   // Energía que pierde un animal al moverse
P.E_eat_plant = 5;   // Energía obtenida al comer planta
P.E_eat_herb  = 20;  // Energía obtenida al comer herbívoro
P.E_repro     = 30;  // Energía necesaria para reproducirse
```

**Ajustes comunes:**
- Subir `p_regrow` → más plantas, herbívoros sobreviven más.
- Subir `E_move_cost` → animales mueren antes si no comen.
- Bajar `E_repro` → más reproducción.

---

## 📊 Salida

Cada 100 pasos:
```
t=300  plantas=39281  herb=15  pred=0
```
- `t` → paso actual.
- `plantas` → número de celdas con planta.
- `herb` / `pred` → número de animales vivos.

---

## ⚙ Control de hilos en OpenMP

Antes de ejecutar la versión paralela:
```bash
export OMP_NUM_THREADS=12   # usa 12 hilos
```

Parámetros opcionales:
```bash
export OMP_SCHEDULE="dynamic,128"
export OMP_PROC_BIND=spread
export OMP_PLACES=cores
```

---

## 🧪 Presets recomendados

### 💨 Rápido (demo 10–15s)
```bash
./sim_seq.exe 120 120 300 800 300 42
export OMP_NUM_THREADS=8
./sim_omp.exe 120 120 300 800 300 42
```

### ⚖ Medio (buena visualización)
```bash
./sim_seq.exe 300 300 800 7000 2800 1234
export OMP_NUM_THREADS=12
./sim_omp.exe 300 300 800 7000 2800 1234
```

### 🏋 Grande (estrés CPU)
```bash
./sim_seq.exe 600 600 1000 15000 6000 1234
export OMP_NUM_THREADS=12
./sim_omp.exe 600 600 1000 15000 6000 1234
```

---

## ⏱ Medición de tiempo

```bash
time ./sim_seq.exe 300 300 800 7000 2800 1234
export OMP_NUM_THREADS=12
time ./sim_omp.exe 300 300 800 7000 2800 1234
```

---

## 📝 Notas
- Usa la **misma semilla** (`seed`) para comparar resultados.
- Si `nPred` es muy alto, los herbívoros pueden extinguirse rápido.
- Si `p_regrow` es muy alto, la población puede crecer indefinidamente.
- Los parámetros pueden ajustarse para ver dinámicas de colapso y recuperación.
