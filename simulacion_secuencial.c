/* simulacion_secuencial.c
 * Simulación de ecosistema SECUNCIAL (sin OpenMP).
 * Autor: Carlos Valladares
 *
 * Idea general:
 *  - Mundo 2D toroidal con plantas, herbívoros y depredadores.
 *  - En cada paso: rebrote de plantas -> mueven/comen/reproducen herbívoros -> mueven/cazan depredadores.
 *  - Sin paralelismo: todo corre en un único hilo para poder comparar luego con la versión OpenMP.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

/* Parámetros globales del modelo */
typedef struct {
    int W, H;                /* tamaño del mundo */
    int steps;               /* cantidad de pasos a simular */
    int nHerbInit, nPredInit;/* poblaciones iniciales */
    unsigned int seed;       /* semilla aleatoria */

    /* Reglas de “energía” y rebrote */
    float p_regrow;          /* prob. de rebrote de planta en celda vacía */
    int   E_move_cost;       /* energía gastada al moverse */
    int   E_eat_plant;       /* energía ganada por comer planta */
    int   E_eat_herb;        /* energía ganada por depredar */
    int   E_repro;           /* umbral de energía para reproducirse */
} Parametros;

/* Agente: herbívoro o depredador (se distinguen por el arreglo en el que viven) */
typedef struct {
    int x, y;        /* posición */
    int energia;     /* nivel de energía */
    int vivo;        /* 1: vivo, 0: eliminado */
} Animal;

/* Vecindad von Neumann (4 direcciones) */
static const int DX[4] = { +1, -1,  0,  0 };
static const int DY[4] = {  0,  0, +1, -1 };

/* Utilidades pequeñas */
static inline int envolver(int a, int m) {
    return (a < 0) ? a + m : ((a >= m) ? a - m : a);
}
static inline int celda_id(int x, int y, int W) { return y * W + x; }

/* RNG LCG simple (suficiente para esta práctica) */
static inline int irand(unsigned int *st) {
    *st = 1664525u * (*st) + 1013904223u;
    return (int)(*st >> 1);
}
static inline float frand01(unsigned int *st) {
    return (irand(st) & 0x7FFFFFFF) / 2147483648.0f; /* [0,1) */
}

/* Un paso aleatorio a un vecino */
static inline void paso_aleatorio(const Parametros *P, int *x, int *y, unsigned int *st) {
    int d = irand(st) % 4;
    *x = envolver(*x + DX[d], P->W);
    *y = envolver(*y + DY[d], P->H);
}

/* Intentar reproducción: crea una cría en celda vecina libre (si supera umbral) */
static int intentar_repro(const Parametros *P,
                          Animal *madre,
                          Animal *arrDestino, int *nDestino,
                          unsigned char *ocupacionDestino,
                          unsigned int *st)
{
    if (madre->energia < P->E_repro) return 0;

    int dirs[4] = {0,1,2,3};
    /* pequeñísimo shuffle para probar vecinos en orden aleatorio */
    for (int i = 3; i > 0; --i) {
        int j = irand(st) % (i + 1);
        int tmp = dirs[i]; dirs[i] = dirs[j]; dirs[j] = tmp;
    }

    for (int k = 0; k < 4; ++k) {
        int nx = envolver(madre->x + DX[dirs[k]], P->W);
        int ny = envolver(madre->y + DY[dirs[k]], P->H);
        int id = celda_id(nx, ny, P->W);

        if (!ocupacionDestino[id]) {
            /* límite de seguridad por si se llena el arreglo */
            if (*nDestino >= P->W * P->H) return 0;

            arrDestino[*nDestino].x = nx;
            arrDestino[*nDestino].y = ny;
            arrDestino[*nDestino].energia = madre->energia / 2;
            arrDestino[*nDestino].vivo = 1;
            (*nDestino)++;

            madre->energia /= 2;   /* compartir energía con la cría */
            ocupacionDestino[id] = 1;
            return 1;
        }
    }
    return 0;
}

/* Inicialización del mundo: plantas arrancan vacías y animales se esparcen */
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

int main(int argc, char **argv) {
    /* Parámetros por defecto (pensados para correr en laptop) */
    Parametros P;
    P.W = 200; P.H = 200; P.steps = 2000;
    P.nHerbInit = 2000; P.nPredInit = 800;
    P.seed = (unsigned)time(NULL);
    P.p_regrow = 0.02f;
    P.E_move_cost = 1;
    P.E_eat_plant = 5;
    P.E_eat_herb  = 20;
    P.E_repro     = 30;

    /* Permito override por CLI: W H steps nHerb nPred seed */
    if (argc >= 7) {
        P.W = atoi(argv[1]);
        P.H = atoi(argv[2]);
        P.steps = atoi(argv[3]);
        P.nHerbInit = atoi(argv[4]);
        P.nPredInit = atoi(argv[5]);
        P.seed = (unsigned)strtoul(argv[6], NULL, 10);
    }

    const int NCELDAS = P.W * P.H;

    /* Memoria principal */
    unsigned char *plantas = (unsigned char*)calloc(NCELDAS, 1);

    /* Arreglos de animales (dimensiono a tope para evitar realojos) */
    Animal *herb = (Animal*)malloc(sizeof(Animal) * NCELDAS);
    Animal *pred = (Animal*)malloc(sizeof(Animal) * NCELDAS);
    int nH = 0, nP = 0;

    inicializar(&P, plantas, herb, &nH, pred, &nP);

    /* Máscaras de ocupación para “siguiente estado” de cada especie */
    unsigned char *ocupH = (unsigned char*)calloc(NCELDAS, 1);
    unsigned char *ocupP = (unsigned char*)calloc(NCELDAS, 1);

    /* Estado aleatorio global de la simulación (secuencial) */
    unsigned int st = P.seed ^ 0x5F3759DFu;

    /* Bucle principal de pasos */
    for (int t = 0; t < P.steps; ++t) {
        /* Rebrote de plantas por celda */
        for (int y = 0; y < P.H; ++y) {
            for (int x = 0; x < P.W; ++x) {
                int id = celda_id(x, y, P.W);
                if (!plantas[id] && frand01(&st) < P.p_regrow) plantas[id] = 1;
            }
        }

        /* Herbívoros: mover, comer, colisiones simples, reproducir y pasar a siguiente */
        memset(ocupH, 0, NCELDAS);
        int nH_sig = 0;
        for (int i = 0; i < nH; ++i) {
            Animal a = herb[i];
            if (!a.vivo) continue;

            a.energia -= P.E_move_cost;
            if (a.energia <= 0) continue;

            paso_aleatorio(&P, &a.x, &a.y, &st);
            int id = celda_id(a.x, a.y, P.W);

            if (plantas[id]) {           /* comer planta si hay */
                a.energia += P.E_eat_plant;
                plantas[id] = 0;
            }

            if (!ocupH[id]) {            /* reservar la celda en el “siguiente” */
                ocupH[id] = 1;
            } else {
                a.energia -= 1;          /* penalizo choque simple */
            }

            if (a.energia > 0) {
                /* reproducir (si alcanza el umbral, agrega una cría al arreglo “herb”) */
                intentar_repro(&P, &a, herb, &nH_sig, ocupH, &st);
                herb[nH_sig++] = a;      /* pasar individuo al siguiente estado */
            }
        }
        nH = nH_sig;

        /* Depredadores: similar, pero cazan herbívoros */
        memset(ocupP, 0, NCELDAS);
        int nP_sig = 0;

        /* mapa rápido de herbívoros por celda (solo lectura en este paso) */
        unsigned short *Hc = (unsigned short*)calloc(NCELDAS, sizeof(unsigned short));
        for (int i = 0; i < nH; ++i) if (herb[i].vivo) Hc[ celda_id(herb[i].x, herb[i].y, P.W) ]++;

        for (int i = 0; i < nP; ++i) {
            Animal p = pred[i];
            if (!p.vivo) continue;

            p.energia -= P.E_move_cost;
            if (p.energia <= 0) continue;

            paso_aleatorio(&P, &p.x, &p.y, &st);
            int id = celda_id(p.x, p.y, P.W);

            if (Hc[id] > 0) {            /* cazar: consume 1 herbívoro “virtual” */
                Hc[id]--;
                p.energia += P.E_eat_herb;

                /* “Matar” físicamente a uno de los herbívoros en esa celda */
                for (int j = 0; j < nH; ++j) {
                    if (herb[j].vivo && herb[j].x == p.x && herb[j].y == p.y) {
                        herb[j].vivo = 0;
                        break;
                    }
                }
            }

            if (!ocupP[id]) ocupP[id] = 1; else p.energia -= 1;

            if (p.energia > 0) {
                intentar_repro(&P, &p, pred, &nP_sig, ocupP, &st);
                pred[nP_sig++] = p;
            }
        }
        nP = nP_sig;
        free(Hc);

        /* Métricas cada 100 pasos (plantas, herbívoros vivos, depredadores vivos) */
        if ((t % 100) == 0) {
            int plantas_vivas = 0, vivosH = 0, vivosP = 0;
            for (int i = 0; i < NCELDAS; ++i) plantas_vivas += (plantas[i] != 0);
            for (int i = 0; i < nH; ++i) vivosH += herb[i].vivo;
            for (int i = 0; i < nP; ++i) vivosP += pred[i].vivo;
            printf("t=%d  plantas=%d  herb=%d  pred=%d\n", t, plantas_vivas, vivosH, vivosP);
        }
    }

    /* Conteo final */
    int plantas_vivas = 0, vivosH = 0, vivosP = 0;
    for (int i = 0; i < NCELDAS; ++i) plantas_vivas += (plantas[i] != 0);
    for (int i = 0; i < nH; ++i) vivosH += herb[i].vivo;
    for (int i = 0; i < nP; ++i) vivosP += pred[i].vivo;
    printf("FINAL: plantas=%d  herb=%d  pred=%d\n", plantas_vivas, vivosH, vivosP);

    /* Liberación de memoria */
    free(plantas);
    free(herb); free(pred);
    free(ocupH); free(ocupP);
    return 0;
}
