// simulacion_secuencial.c
// Mini-proyecto: simulación simple de tráfico con semáforos (versión base SIN paralelismo)
// Autor: Carlos Valladares
//
// Compilar:
//   gcc -O2 -std=c11 simulacion_secuencial.c -o sim_seq
//
// Ejecutar (ejemplo):
//   ./sim_seq 50 4 20 5
//     -> 50 iteraciones, 4 semáforos, 20 vehículos, longitud de carril 5
//
// Idea general: modelo discreto y minimalista para tener un baseline
// contra el cual comparar la versión OpenMP.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum { ROJO = 0, AMARILLO = 1, VERDE = 2 } Estado;

// Semáforo con duraciones de cada fase y un reloj interno.
typedef struct {
    int id;
    Estado estado;
    int t_verde, t_amarillo, t_rojo; // duraciones
    int reloj_fase;                  // progreso dentro de la fase actual   
} Semaforo;

// Vehículo: pertenece a un carril (indexa un semáforo) y tiene una posición.
typedef struct {
    int id;
    int carril;    // semáforo/carril al que pertenece
    int posicion;  // 0 antes del cruce; >0 ya cruzó y avanza
} Vehiculo;

// Cambia de fase y resetea el reloj interno.
static void rotar_fase(Semaforo* s) {
    s->reloj_fase = 0;
    if (s->estado == VERDE) s->estado = AMARILLO;
    else if (s->estado == AMARILLO) s->estado = ROJO;
    else s->estado = VERDE;
}

// Recorre todos los semáforos y avanza sus relojes.
// Cuando una fase se agota, rota al siguiente color.
static void actualizar_semaforos(Semaforo* ls, int n) {
    for (int i = 0; i < n; ++i) {
        Semaforo* s = &ls[i];
        s->reloj_fase++;
        if (s->estado == VERDE    && s->reloj_fase >= s->t_verde)    rotar_fase(s);
        if (s->estado == AMARILLO && s->reloj_fase >= s->t_amarillo) rotar_fase(s);
        if (s->estado == ROJO     && s->reloj_fase >= s->t_rojo)     rotar_fase(s);
    }
}

// Reglas de movimiento (muy simples y deterministas):
// - Si un auto está en pos 0 y su semáforo está en VERDE, cruza (va a 1).
// - Si ya pasó el cruce (pos > 0) y no llegó al final, avanza una celda.
// No hay choques porque el baseline no modela ocupación; en paralelo sí la modelo.
static void mover_vehiculos(Vehiculo* vs, int nv, const Semaforo* ls, int long_carril) {
    for (int i = 0; i < nv; ++i) {
        Vehiculo* v = &vs[i];
        const Semaforo* s = &ls[v->carril];

        if (v->posicion == 0) {
            if (s->estado == VERDE) v->posicion = 1;
        } else if (v->posicion < long_carril) {
            v->posicion++;
        }
    }
}

// Inicializo alternando estados de semáforos y distribuyendo autos por carriles.
static void inicializar(int nv, int nl, Vehiculo* vs, Semaforo* ls) {
    for (int i = 0; i < nl; ++i) {
        ls[i].id = i;
        ls[i].estado = (i % 2 == 0) ? VERDE : ROJO;
        ls[i].t_verde = 3; ls[i].t_amarillo = 1; ls[i].t_rojo = 3;
        ls[i].reloj_fase = 0;
    }
    for (int i = 0; i < nv; ++i) {
        vs[i].id = i;
        vs[i].carril = i % nl;
        vs[i].posicion = (i % 3 == 0) ? 0 : 1;
    }
}

// Log de estado por iteración. Mantengo texto similar al de la versión paralela
// para facilitar la comparación de salidas.
static void imprimir_estado(int iter, const Vehiculo* vs, int nv, const Semaforo* ls, int nl) {
    printf("Iteración %d\n", iter);
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

    inicializar(n_vehiculos, n_semaforos, vehiculos, semaforos);

    for (int it = 1; it <= iteraciones; ++it) {
        actualizar_semaforos(semaforos, n_semaforos);
        mover_vehiculos(vehiculos, n_vehiculos, semaforos, long_carril);
        imprimir_estado(it, vehiculos, n_vehiculos, semaforos, n_semaforos);
    }

    free(semaforos);
    free(vehiculos);
    return 0;
}
