/* simulacion_paralela.c
 * Simulación de ecosistema con OpenMP.
 * Autor: Carlos Valladares
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <omp.h>

typedef struct { int W,H,steps,nHerbInit,nPredInit; unsigned int seed;
    float p_regrow; int E_move_cost,E_eat_plant,E_eat_herb,E_repro; } Params;
typedef struct { int x,y,energy,alive; } Animal;
static const int DX[4]={+1,-1,0,0}, DY[4]={0,0,+1,-1};
static inline int wrap(int a,int m){ return (a<0)?a+m:((a>=m)?a-m:a); }
static inline int idx(int x,int y,int W){ return y*W+x; }
static inline int irand(unsigned int*st){ *st=1664525u*(*st)+1013904223u; return (int)(*st>>1); }
static inline float frand(unsigned int*st){ return (irand(st)&0x7FFFFFFF)/2147483648.0f; }
static void random_step(const Params*P,int*x,int*y,unsigned int*st){ int d=irand(st)%4; *x=wrap(*x+DX[d],P->W); *y=wrap(*y+DY[d],P->H); }

static void init_world(const Params*P,unsigned char*plants,Animal*herb,int*nh,Animal*pred,int*np){
    memset(plants,0,P->W*P->H); unsigned int st=P->seed^0xA5A5A5A5u; *nh=0; *np=0;
    for(int i=0;i<P->nHerbInit;++i){ herb[*nh]=(Animal){irand(&st)%P->W,irand(&st)%P->H,P->E_repro/2,1}; (*nh)++; }
    for(int i=0;i<P->nPredInit;++i){ pred[*np]=(Animal){irand(&st)%P->W,irand(&st)%P->H,P->E_repro/2,1}; (*np)++; }
}
static int try_repro_locked(const Params*P,Animal*a,Animal*next,int*nextN,int*nextOcc,omp_lock_t*locks,unsigned int*st){
    if(a->energy<P->E_repro) return 0; int dirs[4]={0,1,2,3};
    for(int i=3;i>0;--i){int j=irand(st)%(i+1); int t=dirs[i]; dirs[i]=dirs[j]; dirs[j]=t;}
    for(int k=0;k<4;++k){ int nx=wrap(a->x+DX[dirs[k]],P->W), ny=wrap(a->y+DY[dirs[k]],P->H), id=idx(nx,ny,P->W);
        omp_set_lock(&locks[id]);
        if(!nextOcc[id]){ int pos=__sync_fetch_and_add(nextN,1); next[pos]=(Animal){nx,ny,a->energy/2,1}; nextOcc[id]=1; omp_unset_lock(&locks[id]); a->energy/=2; return 1; }
        omp_unset_lock(&locks[id]);
    } return 0;
}

int main(int argc,char**argv){
    Params P={200,200,2000,2000,800,(unsigned)time(NULL),0.02f,1,5,20,30};
    if(argc>=7){ P.W=atoi(argv[1]); P.H=atoi(argv[2]); P.steps=atoi(argv[3]);
        P.nHerbInit=atoi(argv[4]); P.nPredInit=atoi(argv[5]); P.seed=(unsigned)strtoul(argv[6],NULL,10); }
    int N=P.W*P.H; unsigned char*plants=calloc(N,1);
    Animal *herb=malloc(sizeof(Animal)*N), *pred=malloc(sizeof(Animal)*N);
    Animal *nextH=malloc(sizeof(Animal)*N), *nextP=malloc(sizeof(Animal)*N);
    int *occH=calloc(N,sizeof(int)), *occP=calloc(N,sizeof(int));
    omp_lock_t*locks=malloc(sizeof(omp_lock_t)*N); for(int i=0;i<N;++i) omp_init_lock(&locks[i]);
    int nH,nP; init_world(&P,plants,herb,&nH,pred,&nP);
    for(int t=0;t<P.steps;++t){
        #pragma omp parallel for collapse(2) schedule(static)
        for(int y=0;y<P.H;++y) for(int x=0;x<P.W;++x){ unsigned int st=(unsigned)(1469598103u ^ (x*1315423911u + y*2654435761u + (unsigned)t));
            int id=idx(x,y,P.W); if(!plants[id] && frand(&st)<P.p_regrow) plants[id]=1; }
        memset(occH,0,sizeof(int)*N); int nH2=0;
        #pragma omp parallel
        {
            unsigned int st=(unsigned)(P.seed^0x9E3779B9u^(unsigned)omp_get_thread_num()^(unsigned)t);
            #pragma omp for schedule(dynamic,128)
            for(int i=0;i<nH;++i){
                Animal a=herb[i]; if(!a.alive) continue; a.energy-=P.E_move_cost; if(a.energy<=0) continue;
                random_step(&P,&a.x,&a.y,&st); int id=idx(a.x,a.y,P.W);
                omp_set_lock(&locks[id]);
                if(plants[id]){ a.energy+=P.E_eat_plant; plants[id]=0; }
                if(!occH[id]){ occH[id]=1; int pos=__sync_fetch_and_add(&nH2,1); nextH[pos]=a; try_repro_locked(&P,&nextH[pos],nextH,&nH2,occH,locks,&st); }
                else { a.energy-=1; if(a.energy>0){ int pos=__sync_fetch_and_add(&nH2,1); nextH[pos]=a; } }
                omp_unset_lock(&locks[id]);
            }
        }
        Animal*tmpH=herb; herb=nextH; nextH=tmpH; nH=nH2;
        memset(occP,0,sizeof(int)*N); int nP2=0;
        unsigned short*Hc=calloc(N,sizeof(unsigned short));
        for(int i=0;i<nH;++i) if(herb[i].alive) Hc[idx(herb[i].x,herb[i].y,P.W)]++;
        #pragma omp parallel
        {
            unsigned int st=(unsigned)(P.seed^0xB5297A4Du^(unsigned)omp_get_thread_num()^(unsigned)t);
            #pragma omp for schedule(dynamic,128)
            for(int i=0;i<nP;++i){
                Animal p=pred[i]; if(!p.alive) continue; p.energy-=P.E_move_cost; if(p.energy<=0) continue;
                random_step(&P,&p.x,&p.y,&st); int id=idx(p.x,p.y,P.W);
                if(Hc[id]>0){ omp_set_lock(&locks[id]); if(Hc[id]>0){ Hc[id]--; p.energy+=P.E_eat_herb;
                        for(int j=0;j<nH;++j){ if(herb[j].alive && herb[j].x==p.x && herb[j].y==p.y){ herb[j].alive=0; break; } } }
                    omp_unset_lock(&locks[id]); }
                omp_set_lock(&locks[id]);
                if(!occP[id]){ occP[id]=1; int pos=__sync_fetch_and_add(&nP2,1); nextP[pos]=p; try_repro_locked(&P,&nextP[pos],nextP,&nP2,occP,locks,&st); }
                else { p.energy-=1; if(p.energy>0){ int pos=__sync_fetch_and_add(&nP2,1); nextP[pos]=p; } }
                omp_unset_lock(&locks[id]);
            }
        }
        free(Hc);
        Animal*tmpP=pred; pred=nextP; nextP=tmpP; nP=nP2;
        int ph=0,hh=0,pp=0;
        #pragma omp parallel for reduction(+:ph)
        for(int i=0;i<N;++i) ph+=plants[i]!=0;
        #pragma omp parallel for reduction(+:hh)
        for(int i=0;i<nH;++i) hh+=herb[i].alive;
        #pragma omp parallel for reduction(+:pp)
        for(int i=0;i<nP;++i) pp+=pred[i].alive;
        if((t%100)==0) printf("t=%d  plantas=%d  herb=%d  pred=%d\n",t,ph,hh,pp);
    }
    int ph=0,hh=0,pp=0;
    #pragma omp parallel for reduction(+:ph)
    for(int i=0;i<N;++i) ph+=plants[i]!=0;
    #pragma omp parallel for reduction(+:hh)
    for(int i=0;i<nH;++i) hh+=herb[i].alive;
    #pragma omp parallel for reduction(+:pp)
    for(int i=0;i<nP;++i) pp+=pred[i].alive;
    printf("FINAL: plantas=%d  herb=%d  pred=%d\n",ph,hh,pp);
    for(int i=0;i<N;++i) omp_destroy_lock(&locks[i]); free(locks);
    free(plants); free(herb); free(pred); free(nextH); free(nextP); free(occH); free(occP);
    return 0;
}
