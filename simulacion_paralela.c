/* simulacion_paralela.c
 * Simulación de ecosistema con OpenMP.
 * Autor: Carlos Valladares
 *
 * Idea general:
 *  - Mundo 2D toroidal con plantas, herbívoros y depredadores.
 *  - En cada paso: rebrote de plantas -> mueven/comen/reproducen herbívoros -> mueven/cazan depredadores.
 *  - Paralelicé: rebrote por celdas (collapse(2)), y animales por individuo (schedule(dynamic)).
 *  - Uso locks por celda para resolver conflictos (ocupar celda, comer, cazar) sin condiciones de carrera.
 *
 * Notas de diseño:
 *  - Mantengo doble buffer para animales (actual -> siguiente) para que cada paso use un estado coherente.
 *  - Evito builtins de GCC para portabilidad: uso #pragma omp atomic capture al reservar índices.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <omp.h>

/* Parámetros globales de la simulación */
typedef struct {
    int W, H;                /* ancho, alto del mundo */
    int steps;               /* pasos totales a simular */
    int nHerbInit, nPredInit;/* población inicial */
    unsigned int seed;       /* semilla RNG */

    /* “Reglas” del ecosistema */
    float p_regrow;          /* probabilidad rebrote de planta en celda vacía */
    int   E_move_cost;       /* energía que cuesta moverse */
    int   E_eat_plant;       /* energía ganada por comer planta */
    int   E_eat_herb;        /* energía ganada por depredar */
    int   E_repro;           /* umbral de energía para reproducirse */
} Parametros;

/* Representación básica de un animal */
typedef struct {
    int x, y;       /* posición en la grilla */
    int energia;    /* “vida” del agente */
    int vivo;       /* 1 = vivo, 0 = eliminado */
} Animal;

/* Desplazamiento a 4 vecinos (von Neumann) */
static const int DX[4] = { +1, -1,  0,  0 };
static const int DY[4] = {  0,  0, +1, -1 };

/* Utilidades “pequeñas” */
static inline int envolver(int a, int m) {
    return (a < 0) ? a + m : ((a >= m) ? a - m : a);
}
static inline int celda_id(int x, int y, int W) { return y * W + x; }

/* RNG LCG muy simple (suficiente para la práctica) */
static inline int irand(unsigned int *st) {
    *st = 1664525u * (*st) + 1013904223u;
    return (int)(*st >> 1);
}
static inline float frand01(unsigned int *st) {
    return (irand(st) & 0x7FFFFFFF) / 2147483648.0f; /* [0,1) */
}

/* Un paso aleatorio a uno de los 4 vecinos */
static inline void paso_aleatorio(const Parametros *P, int *x, int *y, unsigned int *st) {
    int d = irand(st) % 4;
    *x = envolver(*x + DX[d], P->W);
    *y = envolver(*y + DY[d], P->H);
}

/* Inicialización del mundo: plantas en 0 (vacías) y animales distribuidos */
static void inicializar(const Parametros *P,
                        unsigned char *plantas,
                        Animal *herb, int *nH,
                        Animal *pred, int *nP)
{
    memset(plantas, 0, P->W * P->H);

    unsigned int st = P->seed ^ 0xA5A5A5A5u;
    *nH = 0; *nP = 0;

    for (int i = 0; i < P->nHerbInit; ++i) {
        herb[*nH].x = irand(&st) % P->W;
        herb[*nH].y = irand(&st) % P->H;
        herb[*nH].energia = P->E_repro / 2;
        herb[*nH].vivo = 1;
        (*nH)++;
    }
    for (int i = 0; i < P->nPredInit; ++i) {
        pred[*nP].x = irand(&st) % P->W;
        pred[*nP].y = irand(&st) % P->H;
        pred[*nP].energia = P->E_repro / 2;
        pred[*nP].vivo = 1;
        (*nP)++;
    }
}

/* Intento de reproducción con locks por celda en el buffer “siguiente”.
   Nota: como estoy dentro de sección crítica de la celda madre, aquí aumento *nextN de forma simple. */
static int intentar_repro(const Parametros *P,
                          Animal *a,
                          Animal *siguiente, int *nextN,
                          int *ocup_siguiente, omp_lock_t *locks,
                          unsigned int *st)
{
    if (a->energia < P->E_repro) return 0;

    int dirs[4] = {0,1,2,3};
    /* pequeño shuffle */
    for (int i = 3; i > 0; --i) {
        int j = irand(st) % (i + 1);
        int tmp = dirs[i]; dirs[i] = dirs[j]; dirs[j] = tmp;
    }

    for (int k = 0; k < 4; ++k) {
        int nx = envolver(a->x + DX[dirs[k]], P->W);
        int ny = envolver(a->y + DY[dirs[k]], P->H);
        int id = celda_id(nx, ny, P->W);

        omp_set_lock(&locks[id]);
        if (!ocup_siguiente[id]) {
            int pos = (*nextN)++; /* seguro aquí porque estoy bajo lock de la celda de destino */
            siguiente[pos].x = nx;
            siguiente[pos].y = ny;
            siguiente[pos].energia = a->energia / 2;
            siguiente[pos].vivo = 1;
            ocup_siguiente[id] = 1;
            omp_unset_lock(&locks[id]);
            a->energia /= 2; /* la madre dona energía */
            return 1;
        }
        omp_unset_lock(&locks[id]);
    }
    return 0;
}

int main(int argc, char **argv) {
    /* Parámetros por defecto pensados para demo en aula */
    Parametros P;
    P.W = 200; P.H = 200; P.steps = 2000;
    P.nHerbInit = 2000; P.nPredInit = 800;
    P.seed = (unsigned)time(NULL);
    P.p_regrow = 0.02f;
    P.E_move_cost = 1;
    P.E_eat_plant = 5;
    P.E_eat_herb  = 20;
    P.E_repro     = 30;

    /* Permito overrides por CLI: W H steps nHerb nPred seed */
    if (argc >= 7) {
        P.W = atoi(argv[1]);
        P.H = atoi(argv[2]);
        P.steps = atoi(argv[3]);
        P.nHerbInit = atoi(argv[4]);
        P.nPredInit = atoi(argv[5]);
        P.seed = (unsigned)strtoul(argv[6], NULL, 10);
    }

    const int NCELDAS = P.W * P.H;

    /* Memorias principales */
    unsigned char *plantas = (unsigned char*)calloc(NCELDAS, 1);

    /* Arreglos “grandes”: dimensiono al máximo (#celdas) para evitar realojos */
    Animal *herb = (Animal*)malloc(sizeof(Animal) * NCELDAS);
    Animal *pred = (Animal*)malloc(sizeof(Animal) * NCELDAS);
    Animal *sigH = (Animal*)malloc(sizeof(Animal) * NCELDAS);
    Animal *sigP = (Animal*)malloc(sizeof(Animal) * NCELDAS);

    /* Ocupaciones del buffer siguiente, por especie */
    int *ocupH = (int*)calloc(NCELDAS, sizeof(int));
    int *ocupP = (int*)calloc(NCELDAS, sizeof(int));

    /* Un lock por celda para resolver conflictos locales */
    omp_lock_t *locks = (omp_lock_t*)malloc(sizeof(omp_lock_t) * NCELDAS);
    for (int i = 0; i < NCELDAS; ++i) omp_init_lock(&locks[i]);

    int nH = 0, nP = 0;
    inicializar(&P, plantas, herb, &nH, pred, &nP);

    /* Bucle de simulación */
    for (int t = 0; t < P.steps; ++t) {

        /* -------------------- Fase 1: Plantas (paralelo por celdas) -------------------- */
        #pragma omp parallel for collapse(2) schedule(static)
        for (int y = 0; y < P.H; ++y) {
            for (int x = 0; x < P.W; ++x) {
                /* RNG “por celda y paso” para no compartir estado entre hilos */
                unsigned int st = (unsigned)(1469598103u ^ (x*1315423911u + y*2654435761u + (unsigned)t));
                int id = celda_id(x, y, P.W);
                if (!plantas[id] && frand01(&st) < P.p_regrow) {
                    plantas[id] = 1;
                }
            }
        }

        /* -------------------- Fase 2: Herbívoros (paralelo por individuo) -------------- */
        memset(ocupH, 0, sizeof(int) * NCELDAS);
        int nH_siguiente = 0;

        #pragma omp parallel
        {
            /* Semilla por hilo para que cada uno tenga su propio RNG */
            unsigned int st = (unsigned)(P.seed ^ 0x9E3779B9u ^ (unsigned)omp_get_thread_num() ^ (unsigned)t);

            #pragma omp for schedule(dynamic, 128)
            for (int i = 0; i < nH; ++i) {
                Animal a = herb[i];
                if (!a.vivo) continue;

                a.energia -= P.E_move_cost;
                if (a.energia <= 0) continue;

                paso_aleatorio(&P, &a.x, &a.y, &st);
                int id = celda_id(a.x, a.y, P.W);

                /* Comer planta y reservar ocupación de la celda destino bajo su lock */
                omp_set_lock(&locks[id]);
                if (plantas[id]) { /* si hay, la consume */
                    a.energia += P.E_eat_plant;
                    plantas[id] = 0;
                }
                if (!ocupH[id]) {
                    ocupH[id] = 1;

                    int pos;
                    #pragma omp atomic capture
                    { pos = nH_siguiente; nH_siguiente++; }

                    sigH[pos] = a;

                    /* Intento de reproducción (crea 1 cría si hay vecina libre) */
                    intentar_repro(&P, &sigH[pos], sigH, &nH_siguiente, ocupH, locks, &st);

                } else {
                    /* choque simple: penalizo un poco si llegan dos al mismo sitio */
                    a.energia -= 1;
                    if (a.energia > 0) {
                        int pos;
                        #pragma omp atomic capture
                        { pos = nH_siguiente; nH_siguiente++; }
                        sigH[pos] = a;
                    }
                }
                omp_unset_lock(&locks[id]);
            }
        }
        /* swap buffers de herbívoros */
        Animal *tmpH = herb; herb = sigH; sigH = tmpH;
        nH = nH_siguiente;

        /* -------------------- Fase 3: Depredadores (paralelo por individuo) ------------ */
        memset(ocupP, 0, sizeof(int) * NCELDAS);
        int nP_siguiente = 0;

        /* Mapa rápido de herbívoros vivos por celda (solo lectura) */
        unsigned short *herbCount = (unsigned short*)calloc(NCELDAS, sizeof(unsigned short));
        for (int i = 0; i < nH; ++i) if (herb[i].vivo) herbCount[ celda_id(herb[i].x, herb[i].y, P.W) ]++;

        #pragma omp parallel
        {
            unsigned int st = (unsigned)(P.seed ^ 0xB5297A4Du ^ (unsigned)omp_get_thread_num() ^ (unsigned)t);

            #pragma omp for schedule(dynamic, 128)
            for (int i = 0; i < nP; ++i) {
                Animal p = pred[i];
                if (!p.vivo) continue;

                p.energia -= P.E_move_cost;
                if (p.energia <= 0) continue;

                paso_aleatorio(&P, &p.x, &p.y, &st);
                int id = celda_id(p.x, p.y, P.W);

                /* Caza: si hay herbívoros en la celda, consume 1 (protejo con lock de la celda) */
                if (herbCount[id] > 0) {
                    omp_set_lock(&locks[id]);
                    if (herbCount[id] > 0) {
                        herbCount[id]--;
                        p.energia += P.E_eat_herb;

                        /* “Eliminar” físicamente uno de los herbívoros de esa celda */
                        for (int j = 0; j < nH; ++j) {
                            if (herb[j].vivo && herb[j].x == p.x && herb[j].y == p.y) {
                                herb[j].vivo = 0;
                                break;
                            }
                        }
                    }
                    omp_unset_lock(&locks[id]);
                }

                /* Reservar ocupación de la celda destino para depredadores */
                omp_set_lock(&locks[id]);
                if (!ocupP[id]) {
                    ocupP[id] = 1;

                    int pos;
                    #pragma omp atomic capture
                    { pos = nP_siguiente; nP_siguiente++; }

                    sigP[pos] = p;

                    /* Intento de reproducción */
                    intentar_repro(&P, &sigP[pos], sigP, &nP_siguiente, ocupP, locks, &st);

                } else {
                    /* choque entre depredadores */
                    p.energia -= 1;
                    if (p.energia > 0) {
                        int pos;
                        #pragma omp atomic capture
                        { pos = nP_siguiente; nP_siguiente++; }
                        sigP[pos] = p;
                    }
                }
                omp_unset_lock(&locks[id]);
            }
        }
        free(herbCount);

        /* swap buffers de depredadores */
        Animal *tmpP = pred; pred = sigP; sigP = tmpP;
        nP = nP_siguiente;

        /* -------------------- Métricas (reducciones) ----------------------------------- */
        int celdas_con_planta = 0, vivosH = 0, vivosP = 0;

        #pragma omp parallel for reduction(+:celdas_con_planta)
        for (int i = 0; i < NCELDAS; ++i) celdas_con_planta += (plantas[i] != 0);

        #pragma omp parallel for reduction(+:vivosH)
        for (int i = 0; i < nH; ++i) vivosH += herb[i].vivo;

        #pragma omp parallel for reduction(+:vivosP)
        for (int i = 0; i < nP; ++i) vivosP += pred[i].vivo;

        if ((t % 100) == 0) {
            printf("t=%d  plantas=%d  herb=%d  pred=%d\n", t, celdas_con_planta, vivosH, vivosP);
        }
    }

    /* Conteo final */
    int celdas_con_planta = 0, vivosH = 0, vivosP = 0;

    #pragma omp parallel for reduction(+:celdas_con_planta)
    for (int i = 0; i < NCELDAS; ++i) celdas_con_planta += (plantas[i] != 0);

    #pragma omp parallel for reduction(+:vivosH)
    for (int i = 0; i < nH; ++i) vivosH += herb[i].vivo;

    #pragma omp parallel for reduction(+:vivosP)
    for (int i = 0; i < nP; ++i) vivosP += pred[i].vivo;

    printf("FINAL: plantas=%d  herb=%d  pred=%d\n", celdas_con_planta, vivosH, vivosP);

    for (int i = 0; i < NCELDAS; ++i) omp_destroy_lock(&locks[i]);
    free(locks);

    free(plantas);
    free(herb); free(pred);
    free(sigH); free(sigP);
    free(ocupH); free(ocupP);
    return 0;
}