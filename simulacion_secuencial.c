/* simulacion_secuencial.c
 * Simulación de ecosistema SECUNCIAL (sin OpenMP).
 * Autor: Carlos Valladares
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

typedef struct { int W,H,steps,nHerbInit,nPredInit; unsigned int seed;
    float p_regrow; int E_move_cost,E_eat_plant,E_eat_herb,E_repro; } Params;
typedef struct { int x,y,energy,alive; } Animal;
static const int DX[4]={+1,-1,0,0}, DY[4]={0,0,+1,-1};
static inline int wrap(int a,int m){ return (a<0)?a+m:((a>=m)?a-m:a); }
static inline int idx(int x,int y,int W){ return y*W+x; }
static inline int irand(unsigned int*st){ *st=1664525u*(*st)+1013904223u; return (int)(*st>>1); }
static inline float frand(unsigned int*st){ return (irand(st)&0x7FFFFFFF)/2147483648.0f; }
static void random_step(const Params*P,int*x,int*y,unsigned int*st){ int d=irand(st)%4; *x=wrap(*x+DX[d],P->W); *y=wrap(*y+DY[d],P->H); }
static int try_repro(const Params*P,Animal*a,Animal*arr,int*na,unsigned char*occ,unsigned int*st){
    if(a->energy<P->E_repro) return 0; int dirs[4]={0,1,2,3};
    for(int i=3;i>0;--i){int j=irand(st)%(i+1); int t=dirs[i]; dirs[i]=dirs[j]; dirs[j]=t;}
    for(int k=0;k<4;++k){int nx=wrap(a->x+DX[dirs[k]],P->W), ny=wrap(a->y+DY[dirs[k]],P->H);
        int id=idx(nx,ny,P->W); if(!occ[id]){ if(*na>=P->W*P->H) return 0;
            arr[*na]=(Animal){nx,ny,a->energy/2,1}; *na+=1; a->energy/=2; occ[id]=1; return 1; } }
    return 0;
}
static void init_world(const Params*P,unsigned char*plants,Animal*herb,int*nh,Animal*pred,int*np){
    memset(plants,0,P->W*P->H); unsigned int st=P->seed^0xA5A5A5A5u; *nh=0; *np=0;
    for(int i=0;i<P->nHerbInit;++i){ herb[*nh]=(Animal){irand(&st)%P->W,irand(&st)%P->H,P->E_repro/2,1}; (*nh)++; }
    for(int i=0;i<P->nPredInit;++i){ pred[*np]=(Animal){irand(&st)%P->W,irand(&st)%P->H,P->E_repro/2,1}; (*np)++; }
}
int main(int argc,char**argv){
    Params P={200,200,2000,2000,800,(unsigned)time(NULL),0.02f,1,5,20,30};
    if(argc>=7){ P.W=atoi(argv[1]); P.H=atoi(argv[2]); P.steps=atoi(argv[3]);
        P.nHerbInit=atoi(argv[4]); P.nPredInit=atoi(argv[5]); P.seed=(unsigned)strtoul(argv[6],NULL,10); }
    int N=P.W*P.H; unsigned char*plants=calloc(N,1); Animal*herb=malloc(sizeof(Animal)*N),*pred=malloc(sizeof(Animal)*N);
    int nH,nP; init_world(&P,plants,herb,&nH,pred,&nP);
    unsigned char *occH=calloc(N,1), *occP=calloc(N,1); unsigned int st=P.seed^0x5F3759DFu;
    for(int t=0;t<P.steps;++t){
        for(int y=0;y<P.H;++y) for(int x=0;x<P.W;++x){ int id=idx(x,y,P.W); if(!plants[id] && frand(&st)<P.p_regrow) plants[id]=1; }
        memset(occH,0,N); int nH2=0;
        for(int i=0;i<nH;++i){ Animal a=herb[i]; if(!a.alive) continue; a.energy-=P.E_move_cost; if(a.energy<=0) continue;
            random_step(&P,&a.x,&a.y,&st); int id=idx(a.x,a.y,P.W); if(plants[id]){ a.energy+=P.E_eat_plant; plants[id]=0; }
            if(!occH[id]) occH[id]=1; else a.energy-=1; if(a.energy>0){ try_repro(&P,&a,herb,&nH2,occH,&st); herb[nH2++]=a; } }
        nH=nH2;
        memset(occP,0,N); int nP2=0; unsigned short*Hc=calloc(N,sizeof(unsigned short));
        for(int i=0;i<nH;++i) if(herb[i].alive) Hc[idx(herb[i].x,herb[i].y,P.W)]++;
        for(int i=0;i<nP;++i){ Animal p=pred[i]; if(!p.alive) continue; p.energy-=P.E_move_cost; if(p.energy<=0) continue;
            random_step(&P,&p.x,&p.y,&st); int id=idx(p.x,p.y,P.W);
            if(Hc[id]>0){ Hc[id]--; p.energy+=P.E_eat_herb; for(int j=0;j<nH;++j){ if(herb[j].alive && herb[j].x==p.x && herb[j].y==p.y){ herb[j].alive=0; break; } } }
            if(!occP[id]) occP[id]=1; else p.energy-=1; if(p.energy>0){ try_repro(&P,&p,pred,&nP2,occP,&st); pred[nP2++]=p; } }
        nP=nP2; free(Hc);
        if((t%100)==0){ int ph=0, hh=0, pp=0; for(int i=0;i<N;++i) ph+=plants[i]!=0; for(int i=0;i<nH;++i) hh+=herb[i].alive; for(int i=0;i<nP;++i) pp+=pred[i].alive;
            printf("t=%d  plantas=%d  herb=%d  pred=%d\n",t,ph,hh,pp); }
    }
    int ph=0,hh=0,pp=0; for(int i=0;i<N;++i) ph+=plants[i]!=0; for(int i=0;i<nH;++i) hh+=herb[i].alive; for(int i=0;i<nP;++i) pp+=pred[i].alive;
    printf("FINAL: plantas=%d  herb=%d  pred=%d\n",ph,hh,pp);
    free(plants); free(herb); free(pred); free(occH); free(occP); return 0;
}
