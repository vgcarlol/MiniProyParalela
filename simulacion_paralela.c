// simulacion_paralela.c
// Mini-proyecto: simulación de tráfico con OpenMP (paralelo)
// Autor: Carlos Valladares
//
// Compilar:
//   gcc -O2 simulacion_paralela.c -fopenmp -o sim_omp
//
// Ejecutar:
//   ./sim_omp 50 4 20 5
//     -> 50 iteraciones, 4 semáforos, 20 vehículos, longitud de carril 5

#include <stdio.h>
#include <stdlib.h>
#include <omp.h>

typedef enum { ROJO = 0, AMARILLO = 1, VERDE = 2 } Estado;

// ---- Estructuras principales ----
// Un semáforo tiene una fase actual y duraciones por color.
// reloj_fase cuenta cuántas iteraciones lleva en la fase actual.
typedef struct {
    int id;
    Estado estado;
    int t_verde, t_amarillo, t_rojo;
    int reloj_fase;
} Semaforo;

// Un vehículo pertenece a un carril (indexa un semáforo)
// y avanza por posiciones discretas [0..long_carril].
typedef struct {
    int id;
    int carril;
    int posicion;
} Vehiculo;

// Cambia la fase de un semáforo y resetea su reloj.
static void rotar_fase(Semaforo* s) {
    s->reloj_fase = 0;
    if (s->estado == VERDE) s->estado = AMARILLO;
    else if (s->estado == AMARILLO) s->estado = ROJO;
    else s->estado = VERDE;
}

// Actualizo todos los semáforos en paralelo.
// Uso schedule(static) porque cada luz hace prácticamente el mismo trabajo.
static void actualizar_semaforos(Semaforo* ls, int n) {
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < n; ++i) {
        Semaforo* s = &ls[i];

        // Lógica de ciclo de semáforo
        s->reloj_fase++;
        if (s->estado == VERDE    && s->reloj_fase >= s->t_verde)    rotar_fase(s);
        if (s->estado == AMARILLO && s->reloj_fase >= s->t_amarillo) rotar_fase(s);
        if (s->estado == ROJO     && s->reloj_fase >= s->t_rojo)     rotar_fase(s);
        
    }
}

// Movimiento de vehículos en paralelo con schedule(dynamic).
// Uso un arreglo de "ocupación" (occ) para reservar posiciones destino
// por (carril, pos). Se incrementa de forma atómica y solo el primero
// en llegar al 1 gana el lugar, evitando que dos autos acaben en el
// mismo sitio (condición de carrera).
static void mover_vehiculos(Vehiculo* vs, int nv, const Semaforo* ls, int long_carril) {
    int maxpos = long_carril;

    // Descubro cuántos carriles hay (máximo carril + 1).
    int num_carriles = 0;
    for (int i = 0; i < nv; ++i)
        if (vs[i].carril + 1 > num_carriles) num_carriles = vs[i].carril + 1;

    // Matriz linealizada de ocupación [carril * (maxpos + 1) + pos]
    int total = (maxpos + 1) * num_carriles;
    int* occ = (int*)calloc(total, sizeof(int));
    if (!occ) { fprintf(stderr, "Memoria insuficiente en mover_vehiculos\n"); return; }

    // --- Paralelismo sobre vehículos ---
    // schedule(dynamic,16) reparte en bloques pequeños para balancear
    // cuando hay carriles con mucha cola (pos=0) y otros libres.
    #pragma omp parallel for schedule(dynamic, 16)
    for (int i = 0; i < nv; ++i) {
        Vehiculo* v = &vs[i];
        const Semaforo* s = &ls[v->carril];

        // Cálculo de la posición tentativa nueva
        int nueva = v->posicion;
        if (v->posicion == 0 && s->estado == VERDE) {
            nueva = 1;                       // Puede cruzar
        } else if (v->posicion < long_carril) {
            nueva = v->posicion + 1;         // Ya cruzó, avanza por el carril
        }

        // Intento reservar el destino (carril, nueva).
        // Si soy el primero (occ pasa de 0 a 1), me muevo;
        // si no, me quedo en donde estaba (evito colisión).
        int idx = v->carril * (maxpos + 1) + nueva;
        int reservado = 0;
        #pragma omp atomic capture
        reservado = ++occ[idx];
        if (reservado == 1) {
            v->posicion = nueva;
        }
    }

    free(occ);
}

// Inicializo con reparto parejo de carriles y fases alternadas
// para que el log se vea dinámico desde el inicio.
static void inicializar(int nv, int nl, Vehiculo* vs, Semaforo* ls) {
    // Semáforos en paralelo
    #pragma omp parallel for
    for (int i = 0; i < nl; ++i) {
        ls[i].id = i;
        ls[i].estado = (i % 2 == 0) ? VERDE : ROJO; // alterno para que no todos den vía a la vez
        ls[i].t_verde = 3; ls[i].t_amarillo = 1; ls[i].t_rojo = 3;
        ls[i].reloj_fase = 0;
    }
    // Vehículos en paralelo
    #pragma omp parallel for
    for (int i = 0; i < nv; ++i) {
        vs[i].id = i;
        vs[i].carril = i % nl;                 // balanceo por carril
        vs[i].posicion = (i % 3 == 0) ? 0 : 1; // algunos esperando, otros ya pasaron
    }
}

// Impresión del estado actual (nota: imprimir en paralelo no conviene,
// por eso aquí va secuencial; el I/O suele ser cuello de botella).
static void imprimir_estado(int iter, const Vehiculo* vs, int nv, const Semaforo* ls, int nl) {
    printf("Iteración %d (hilos max: %d)\n", iter, omp_get_max_threads());
    for (int i = 0; i < nv; ++i)
        printf("Vehículo %d - Carril %d - Posición: %d\n", vs[i].id, vs[i].carril, vs[i].posicion);
    for (int i = 0; i < nl; ++i)
        printf("Semáforo %d - Estado: %d\n", ls[i].id, ls[i].estado);
    puts("");
}

int main(int argc, char** argv) {
    if (argc < 5) {
        fprintf(stderr, "Uso: %s <iteraciones> <num_semaforos> <num_vehiculos> <long_carril>\n", argv[0]);
        return 1;
    }
    int iteraciones = atoi(argv[1]);
    int n_semaforos = atoi(argv[2]);
    int n_vehiculos = atoi(argv[3]);
    int long_carril = atoi(argv[4]);

    Semaforo* semaforos = (Semaforo*)malloc(sizeof(Semaforo) * n_semaforos);
    Vehiculo* vehiculos = (Vehiculo*)malloc(sizeof(Vehiculo) * n_vehiculos);
    if (!semaforos || !vehiculos) {
        fprintf(stderr, "Memoria insuficiente\n");
        free(semaforos); free(vehiculos);
        return 1;
    }

    // --- Ajustes globales de OpenMP ---
    // Permito que OpenMP ajuste el número de hilos si lo ve conveniente
    // según disponibilidad del sistema.
    omp_set_dynamic(1);

    // Habilito paralelismo anidado (para el ejemplo dentro de semáforos).
    omp_set_nested(1);
    omp_set_max_active_levels(2);

    // Heurística simple: si hay pocos vehículos, no tiene sentido disparar muchos hilos.
    int sugeridos = (n_vehiculos / 10) + 1;
    if (sugeridos < 2) sugeridos = 2;
    omp_set_num_threads(sugeridos);

    inicializar(n_vehiculos, n_semaforos, vehiculos, semaforos);

    for (int it = 1; it <= iteraciones; ++it) {
        // --- SECTIONS: luces y autos en paralelo en cada iteración ---
        #pragma omp parallel sections
        {
            #pragma omp section
            actualizar_semaforos(semaforos, n_semaforos);

            #pragma omp section
            mover_vehiculos(vehiculos, n_vehiculos, semaforos, long_carril);
        }

        // Log secuencial (evita mezclar prints de múltiples hilos)
        imprimir_estado(it, vehiculos, n_vehiculos, semaforos, n_semaforos);

        // --- Reajuste "dinámico" de hilos según congestión ---
        // Cuento cuántos autos siguen en pos=0. Si hay cola, subo hilos.
        int en_cola = 0;
        #pragma omp parallel for reduction(+:en_cola)
        for (int i = 0; i < n_vehiculos; ++i)
            if (vehiculos[i].posicion == 0) en_cola++;

        int nuevos_hilos = (en_cola / 8) + 2;  // heurística muy simple
        if (nuevos_hilos < 2) nuevos_hilos = 2;
        omp_set_num_threads(nuevos_hilos);
    }

    free(semaforos);
    free(vehiculos);
    return 0;
}
