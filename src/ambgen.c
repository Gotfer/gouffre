/* GOUFFRE - atelier sonore : nappes d'ambiance precalculees (dossier « donnees/ambiances »).
   Chaque niveau a deux nappes (l'exploration, puis l'arene du gardien), le campement une par saison.
   Tout est synthetise ici : vent, gouttes, feu, insectes, grenouilles, voix lointaines, cloches,
   reverberation de grotte... puis bouclé sans couture (fondu croise) et ecrit en WAV 16 bits stereo 44,1 kHz.
   Atelier seulement (-DGOUFFRE_ATELIER) : le jeu, lui, ne fait que lire les fichiers. */
#ifdef GOUFFRE_ATELIER
#include "game.h"
#include <stdio.h>

#define SR 44100
#define TF 6.2831853f
static int N;            /* echantillons rendus : boucle + fondu */
static int NL;           /* longueur de la boucle */
static int NX;           /* longueur du fondu croise */
static float DUR;        /* duree rendue (s) */

/* ---------------- hasard ---------------- */
static uint32_t rs=1u;
static inline uint32_t rr(void){ rs^=rs<<13; rs^=rs>>17; rs^=rs<<5; return rs; }
static inline float rnd(void){ return (float)(rr()>>8)*(1.0f/16777216.0f); }
static inline float rnd2(void){ return rnd()*2-1; }
static inline float rng(float a,float b){ return a+(b-a)*rnd(); }
static inline float expo(float rate){ return -logf(1.0f-rnd()*0.9999f)/rate; }
static inline int SMP(float t){ return (int)(t*SR); }
static inline float dbl(float db){ return powf(10.0f,db/20.0f); }

/* ---------------- tampons ---------------- */
typedef struct { float *l,*r; } St;
static St D, WS, T, TS;      /* mixage sec, envoi d'echo ; couche en cours (sec, envoi) */
static St st_alloc(void){ St s; s.l=(float*)calloc((size_t)N,4); s.r=(float*)calloc((size_t)N,4); return s; }
static void st_free(St *s){ free(s->l); free(s->r); s->l=s->r=NULL; }
static void st_zero(St *s){ memset(s->l,0,(size_t)N*4); memset(s->r,0,(size_t)N*4); }

/* ---------------- filtres ---------------- */
typedef struct { float b0,b1,b2,a1,a2,z1,z2; } BQ;
enum { F_LP, F_HP, F_BP, F_PK };
static void bq_coef(BQ *q,int type,float f,float Q,float gdb){
  float w=TF*clampf(f,10.0f,SR*0.45f)/SR, c=cosf(w), s=sinf(w), al=s/(2*Q), A=powf(10.0f,gdb/40.0f);
  float b0,b1,b2,a0,a1,a2;
  switch(type){
    case F_LP: b0=(1-c)/2; b1=1-c; b2=(1-c)/2; a0=1+al; a1=-2*c; a2=1-al; break;
    case F_HP: b0=(1+c)/2; b1=-(1+c); b2=(1+c)/2; a0=1+al; a1=-2*c; a2=1-al; break;
    case F_BP: b0=al; b1=0; b2=-al; a0=1+al; a1=-2*c; a2=1-al; break;
    default:   b0=1+al*A; b1=-2*c; b2=1-al*A; a0=1+al/A; a1=-2*c; a2=1-al/A; break;
  }
  q->b0=b0/a0; q->b1=b1/a0; q->b2=b2/a0; q->a1=a1/a0; q->a2=a2/a0;
}
static BQ bqn(int type,float f,float Q){ BQ q; memset(&q,0,sizeof q); bq_coef(&q,type,f,Q,0); return q; }
static inline void bq_copy(BQ *d,const BQ *s){ d->b0=s->b0; d->b1=s->b1; d->b2=s->b2; d->a1=s->a1; d->a2=s->a2; }
static inline float bq(BQ *q,float x){ float y=q->b0*x+q->z1; q->z1=q->b1*x-q->a1*y+q->z2; q->z2=q->b2*x-q->a2*y; return y; }

/* ---------------- bruits, oscillateurs ---------------- */
typedef struct { float b0,b1,b2,b3,b4,b5,b6; } Pink;
static inline float pink(Pink *p){ float w=rnd2();
  p->b0=0.99886f*p->b0+w*0.0555179f; p->b1=0.99332f*p->b1+w*0.0750759f; p->b2=0.96900f*p->b2+w*0.1538520f;
  p->b3=0.86650f*p->b3+w*0.3104856f; p->b4=0.55000f*p->b4+w*0.5329522f; p->b5=-0.7616f*p->b5-w*0.0168980f;
  float o=p->b0+p->b1+p->b2+p->b3+p->b4+p->b5+p->b6+w*0.5362f; p->b6=w*0.115926f; return o*0.11f; }
static inline float brown(float *s){ *s=(*s+0.02f*rnd2())/1.02f; return *s*3.5f; }
static inline float blep(float t,float dt){ if(t<dt){ t/=dt; return t+t-t*t-1; } if(t>1-dt){ t=(t-1)/dt; return t*t+t+t+1; } return 0; }
static inline float saw(float *ph,float f){ float dt=f/SR; *ph+=dt; if(*ph>=1) *ph-=1; return 2*(*ph)-1-blep(*ph,dt); }
static inline float osc(float *ph,float f){ *ph+=f/SR; if(*ph>=1) *ph-=1; return sinf(TF*(*ph)); }
/* hasard lisse (valeurs 0..1 reliees en douceur), periode moyenne T secondes */
typedef struct { float a,b,t,dt,base; } Sm;
static Sm sm(float T){ Sm s; s.a=rnd(); s.b=rnd(); s.t=rnd(); s.base=1.0f/(T*SR); s.dt=s.base; return s; }
static inline float smv(Sm *s){ s->t+=s->dt; if(s->t>=1){ s->t-=1; s->a=s->b; s->b=rnd(); s->dt=s->base*rng(0.7f,1.3f); } float u=s->t*s->t*(3-2*s->t); return s->a+(s->b-s->a)*u; }

/* ---------------- position d'un son (pan, distance -> volume, filtre, echo) ---------------- */
typedef struct { float gl,gr,sl,sr; BQ lp; int lp_on; } Pos;
static Pos pos(float pan,float dist){
  Pos p; memset(&p,0,sizeof p); float a=(clampf(pan,-1,1)+1)*0.25f*PI; float g=1-0.72f*dist, s=0.18f+0.82f*dist;
  p.gl=cosf(a)*g; p.gr=sinf(a)*g; p.sl=cosf(a)*s; p.sr=sinf(a)*s;
  p.lp_on= dist>0.04f; if(p.lp_on) bq_coef(&p.lp,F_LP,10000.0f*powf(0.17f,dist),0.7f,0);
  return p; }
static Pos posr(float d0,float d1){ return pos(rnd2()*0.9f,rng(d0,d1)); }
static inline void emit(Pos *p,int i,float x){
  if(p->lp_on) x=bq(&p->lp,x);
  if((unsigned)i>=(unsigned)N) return;
  T.l[i]+=x*p->gl; T.r[i]+=x*p->gr; TS.l[i]+=x*p->sl; TS.r[i]+=x*p->sr; }
static inline void emit_pan(int i,float x,float pan,float g,float send){   /* pan qui bouge (survols) */
  if((unsigned)i>=(unsigned)N) return; float a=(clampf(pan,-1,1)+1)*0.25f*PI, cl=cosf(a), cr=sinf(a);
  T.l[i]+=x*cl*g; T.r[i]+=x*cr*g; TS.l[i]+=x*cl*send; TS.r[i]+=x*cr*send; }

/* ---------------- couches : mise a niveau et ajout au mixage ---------------- */
static float T_rms(void){ double a=0; for(int i=0;i<N;i++) a+=(double)T.l[i]*T.l[i]+(double)T.r[i]*T.r[i]; return (float)sqrt(a/(2.0*N)); }
/* nappe continue : volume moyen impose (dB), part envoyee dans l'echo */
static void commit_bed(float db,float send){
  float k=dbl(db)/fmaxf(T_rms(),1e-9f);
  for(int i=0;i<N;i++){ float l=T.l[i]*k, r=T.r[i]*k; D.l[i]+=l; D.r[i]+=r; WS.l[i]+=l*send; WS.r[i]+=r*send; }
  st_zero(&T); st_zero(&TS); }
/* evenements : gain direct (les envois sont deja calcules) */
static void commit_ev(float db){
  float k=dbl(db);
  for(int i=0;i<N;i++){ D.l[i]+=T.l[i]*k; D.r[i]+=T.r[i]*k; WS.l[i]+=TS.l[i]*k; WS.r[i]+=TS.r[i]*k; }
  st_zero(&T); st_zero(&TS); }

/* =====================================================================================
   nappes continues
   ===================================================================================== */
/* vent : bruit rose filtre, centre qui suit les rafales ; Q eleve = sifflement */
static void bed_wind(float flo,float fhi,float Q,float gustT,float depth,float width,float db,float send){
  Pink a={0},b={0}; BQ fl=bqn(F_BP,flo,Q), fr=fl, gl=bqn(F_BP,flo*2,Q*0.6f), gr=gl;
  Sm c1=sm(rng(4,8)), c2=sm(rng(5,10)), g1=sm(gustT), g2=sm(gustT*0.43f), g3=sm(gustT*2.3f); float lr=logf(fhi/flo);
  for(int i=0;i<N;i++){
    float u1=smv(&c1), u2=smv(&c2), v1=smv(&g1), v2=smv(&g2), v3=smv(&g3);
    float gust=0.55f*v1+0.3f*v2+0.15f*v3;
    if((i&31)==0){ bq_coef(&fl,F_BP,flo*expf(lr*clampf(0.2f*u1+0.8f*gust,0,1)),Q,0); bq_copy(&fr,&fl);
                   bq_coef(&gl,F_BP,flo*expf(lr*(0.15f+0.85f*u2)),Q*0.6f,0); bq_copy(&gr,&gl); }
    float nl=pink(&a), nr=pink(&b)*width+nl*(1-width);
    float g=1-depth+depth*powf(gust,1.6f)*1.7f;
    T.l[i]+=(bq(&fl,nl)+0.5f*bq(&gl,nl))*g; T.r[i]+=(bq(&fr,nr)+0.5f*bq(&gr,nr))*g;
  }
  commit_bed(db,send);
}
/* bourdon : sinus graves qui battent lentement */
static void bed_drone(const float *f,int n,float detune,float db,float send){
  for(int k=0;k<n;k++){
    float p1=rnd(),p2=rnd(),p3=rnd(); Sm a=sm(rng(7,15)); float d=detune*rng(0.5f,1.5f), pan=rnd2()*0.5f;
    float cl=cosf((pan+1)*0.25f*PI), cr=sinf((pan+1)*0.25f*PI);
    for(int i=0;i<N;i++){ float e=0.3f+0.7f*smv(&a); float x=(osc(&p1,f[k])+0.8f*osc(&p2,f[k]+d)+0.18f*osc(&p3,f[k]*2.003f))*e; T.l[i]+=x*cl; T.r[i]+=x*cr; }
  }
  commit_bed(db,send);
}
/* grondement : bruit brun tres grave */
static void bed_rumble(float fc,float T_,float db,float send){
  float sa=0,sb=0; BQ l1=bqn(F_LP,fc,0.8f), r1=l1; Sm a=sm(T_), b=sm(T_*0.5f);
  for(int i=0;i<N;i++){ float e=0.35f+0.65f*(0.7f*smv(&a)+0.3f*smv(&b)); T.l[i]+=bq(&l1,brown(&sa))*e; T.r[i]+=bq(&r1,brown(&sb)*0.6f+sa*1.4f)*e; }
  commit_bed(db,send);
}
/* brasier : ronflement + souffle, qui vacille */
static void bed_roar(float fc,float flick,float db,float send){
  float sa=0,sb=0; Pink pa={0},pb={0}; BQ l1=bqn(F_LP,fc,0.7f), r1=l1, l2=bqn(F_BP,fc*2.5f,0.5f), r2=l2; Sm a=sm(flick), b=sm(flick*3.1f);
  for(int i=0;i<N;i++){ float e=0.45f+0.55f*(0.6f*smv(&a)+0.4f*smv(&b));
    T.l[i]+=(bq(&l1,brown(&sa))+0.35f*bq(&l2,pink(&pa)))*e; T.r[i]+=(bq(&r1,brown(&sb))+0.35f*bq(&r2,pink(&pb)))*e; }
  commit_bed(db,send);
}
/* courant sous l'eau : grave, lent, etouffe */
static void bed_current(float db,float send){
  float sa=0,sb=0; BQ l1=bqn(F_LP,220,0.9f), r1=l1, l2=bqn(F_BP,420,1.2f), r2=l2; Sm a=sm(9), b=sm(4), c=sm(13);
  for(int i=0;i<N;i++){ float e=0.4f+0.6f*smv(&a), f=smv(&b), g=smv(&c);
    if((i&63)==0){ bq_coef(&l2,F_BP,260+320*f,1.2f,0); bq_copy(&r2,&l2); }
    T.l[i]+=(bq(&l1,brown(&sa))+0.4f*bq(&l2,brown(&sb)))*e*(0.7f+0.3f*g); T.r[i]+=(bq(&r1,brown(&sb))+0.4f*bq(&r2,brown(&sa)))*e*(1.0f-0.3f*g); }
  commit_bed(db,send);
}
/* essaim : scies desaccordees qui ondulent */
static void bed_swarm(int n,float f0,float f1,float bpf,float db,float send){
  for(int k=0;k<n;k++){
    float ph=rnd(), f=rng(f0,f1); Sm w=sm(rng(0.6f,2.0f)), a=sm(rng(1.5f,5)); float pan=rnd2()*0.8f, cl=cosf((pan+1)*0.25f*PI), cr=sinf((pan+1)*0.25f*PI);
    BQ b=bqn(F_BP,bpf*rng(0.8f,1.25f),1.1f);
    for(int i=0;i<N;i++){ float ff=f*(1+0.04f*(smv(&w)-0.5f)); float x=bq(&b,saw(&ph,ff))*(0.2f+0.8f*smv(&a)); T.l[i]+=x*cl; T.r[i]+=x*cr; }
  }
  commit_bed(db,send);
}
/* grillons : trains d'impulsions aigues */
static void bed_crickets(int n,float db,float send){
  for(int k=0;k<n;k++){
    float fc=rng(3900,5200), pan=rnd2()*0.9f, dist=rng(0.1f,0.8f); Pos p=pos(pan,dist); float ph=rnd();
    float chirp=rng(0.35f,0.7f), pulses=(float)(int)rng(2,5), prate=rng(26,40), gain=rng(0.4f,1.0f); Sm act=sm(rng(8,20));
    float t=rnd()*chirp;
    for(int i=0;i<N;i++){
      float tt=(float)i/SR; (void)tt;
      float ac=smv(&act);
      float x=osc(&ph,fc)*(0.8f+0.2f*sinf(TF*fc*0.0123f*i/SR));
      /* position dans le cycle */
      float c=fmodf((float)i/SR+t,chirp), pp=c*prate; int pk=(int)pp; float fr=pp-pk;
      float e= (pk<(int)pulses) ? sinf(fr*PI)*sinf(fr*PI) : 0;
      x*=e*gain*(ac>0.25f?1.0f:ac*4);
      emit(&p,i,x);
    }
  }
  float k=dbl(db)/fmaxf(T_rms(),1e-9f); for(int i=0;i<N;i++){ D.l[i]+=T.l[i]*k; D.r[i]+=T.r[i]*k; WS.l[i]+=TS.l[i]*k*send; WS.r[i]+=TS.r[i]*k*send; }
  st_zero(&T); st_zero(&TS);
}
/* voyelles (formants) */
static const float VOW[5][3]={{700,1220,2600},{450,800,2830},{325,700,2530},{400,2000,2550},{270,2300,3000}};
/* choeur : accords tenus, voix « ah/oh » desaccordees, changements lents */
static void bed_choir(const float *ch,int nch,int per,float hold,float db,float send){
  int nv=per; float *ph=(float*)calloc((size_t)nv*3,4);
  for(int v=0;v<nv*3;v++) ph[v]=rnd();
  BQ F[3][3]; for(int d=0;d<3;d++) for(int k=0;k<3;k++) F[d][k]=bqn(F_BP,VOW[d==1?1:0][k],VOW[d==1?1:0][k]/90.0f);
  Sm vib[8]; for(int v=0;v<8;v++) vib[v]=sm(rng(0.2f,0.4f));
  int seg=SMP(hold);
  for(int i=0;i<N;i++){
    int s=i/seg; float u=(float)(i%seg)/seg; int c0=s%nch, c1=(s+1)%nch; float xf= u>0.8f ? (u-0.8f)/0.2f : 0; xf=xf*xf*(3-2*xf);
    float xs[3]={0,0,0};
    for(int v=0;v<nv;v++){
      float f=ch[c0*per+v]*(1-xf)+ch[c1*per+v]*xf; float vb=1+0.004f*sinf(TF*5.1f*i/SR+v)+0.003f*(smv(&vib[v&7])-0.5f);
      for(int d=0;d<3;d++) xs[d]+=saw(&ph[v*3+d],f*vb*(1+(d-1)*0.0035f)); }
    float y[3]; for(int d=0;d<3;d++){ y[d]=0; for(int k=0;k<3;k++) y[d]+=bq(&F[d][k],xs[d])*(k==0?1.0f:(k==1?0.55f:0.22f)); }
    float l=y[0]+0.6f*y[1], r=y[2]+0.6f*y[1];
    float e=0.8f+0.2f*sinf(TF*i/(SR*11.0f));
    T.l[i]+=l*e; T.r[i]+=r*e;
  }
  free(ph); commit_bed(db,send);
}
/* bulles en continu (ruisseau, eau qui bout) */
static void bed_bubbles(float rate,float f0,float f1,float db,float send,float d0,float d1){
  for(float t=rng(0,0.1f);t<DUR;t+=expo(rate)){
    Pos p=posr(d0,d1); float f=f0*powf(f1/f0,rnd()), tau=rng(0.006f,0.02f)*(800/f+0.4f), ph=0, a=powf(rnd(),2.5f); int i0=SMP(t), n=SMP(tau*6);
    for(int k=0;k<n;k++){ float tt=(float)k/SR; float ff=f*(1+0.9f*tt/(tau*6)); ph+=ff/SR; emit(&p,i0+k,sinf(TF*ph)*expf(-tt/tau)*a*(tt<0.001f?tt/0.001f:1)); }
  }
  float k=dbl(db)/fmaxf(T_rms(),1e-9f); for(int i=0;i<N;i++){ D.l[i]+=T.l[i]*k; D.r[i]+=T.r[i]*k; WS.l[i]+=TS.l[i]*k*send; WS.r[i]+=TS.r[i]*k*send; }
  st_zero(&T); st_zero(&TS);
}
/* clapotis : bruit filtre qui « babille » */
static void bed_babble(float flo,float fhi,float db,float send){
  Pink a={0},b={0}; BQ l1=bqn(F_BP,flo,2), r1=l1; Sm f=sm(0.12f), g=sm(0.09f), e=sm(3);
  for(int i=0;i<N;i++){ float u=smv(&f), v=smv(&g);
    if((i&15)==0){ bq_coef(&l1,F_BP,flo+(fhi-flo)*u,2.2f,0); bq_coef(&r1,F_BP,flo+(fhi-flo)*v,2.2f,0); }
    float ee=0.5f+0.5f*smv(&e); T.l[i]+=bq(&l1,pink(&a))*ee; T.r[i]+=bq(&r1,pink(&b))*ee; }
  commit_bed(db,send);
}
/* bruissement (feuilles, sable) : grains de bruit aigu qui suivent des rafales */
static void bed_rustle(float hp,float gustT,float density,float db,float send){
  BQ hl=bqn(F_HP,hp,0.7f), hr=hl, ll=bqn(F_LP,fminf(hp*2.6f,7000),0.7f), lr=ll; Sm g=sm(gustT), g2=sm(gustT*0.4f);
  float envl=0, envr=0; Pink pa={0},pb={0};
  for(int i=0;i<N;i++){ float gs=0.6f*smv(&g)+0.4f*smv(&g2); float pr=density*powf(gs,2.2f)/SR;
    if(rnd()<pr) envl=rng(0.3f,1.0f); if(rnd()<pr) envr=rng(0.3f,1.0f);
    envl*=0.9975f; envr*=0.9975f;
    T.l[i]+=bq(&ll,bq(&hl,pink(&pa)))*envl*envl; T.r[i]+=bq(&lr,bq(&hr,pink(&pb)))*envr*envr; }
  commit_bed(db,send);
}

/* =====================================================================================
   evenements (sons courts places dans le temps et l'espace)
   ===================================================================================== */
static void ev_drip(int i0,Pos *p,float f0,float a){   /* goutte : « plic » qui monte */
  float ph=0, tau=rng(0.02f,0.05f), rise=rng(0.4f,1.1f); int n=SMP(tau*7);
  for(int k=0;k<n;k++){ float t=(float)k/SR; float f=f0*(1+rise*(1-expf(-t/0.01f))); ph+=f/SR;
    float x=sinf(TF*ph)*expf(-t/tau)*(t<0.0012f?t/0.0012f:1); if(k<50) x+=rnd2()*0.25f*(1-k/50.0f); emit(p,i0+k,x*a); } }
static void ev_click(int i0,Pos *p,float a,float len_ms,float hp){   /* craquement de braise */
  int n=SMP(len_ms*0.001f)+2; float px=0; BQ lp=bqn(F_LP,fmaxf(hp*3,6000),0.7f);
  for(int k=0;k<n;k++){ float x=rnd2()*expf(-(float)k/(n*0.3f)); float h=x-px; px=x; emit(p,i0+k,bq(&lp,h)*a); } }
static void ev_crackle(int i0,Pos *p,float a){   /* grappe de craquements */
  int m=(int)rng(1,6); float t=0; for(int j=0;j<m;j++){ t+=rng(0.003f,0.04f); float aa=a*powf(rnd(),1.5f); ev_click(i0+SMP(t),p,aa,rng(0.4f,3.0f),2000); } }
static void ev_pop(int i0,Pos *p,float a){   /* claquement de bois */
  int n=SMP(0.03f); BQ b=bqn(F_BP,rng(600,1400),1.5f); for(int k=0;k<n;k++) emit(p,i0+k,bq(&b,rnd2())*a*expf(-(float)k/(n*0.15f))*3); }
static const float CLK[6]={1.0f,2.32f,4.25f,6.63f,9.38f,12.1f};
static void ev_clank(int i0,Pos *p,float f0,float dec,float a){   /* metal frappe */
  float ph[6]={0}; float amp[6]={1,0.7f,0.5f,0.35f,0.22f,0.12f}; int n=SMP(dec*5);
  for(int k=0;k<n;k++){ float t=(float)k/SR, x=0; for(int j=0;j<6;j++){ ph[j]+=f0*CLK[j]*(1+j*0.003f)/SR; x+=sinf(TF*ph[j])*amp[j]*expf(-t*(1+j*0.8f)/dec); }
    if(k<SMP(0.004f)) x+=rnd2()*0.6f*(1-(float)k/SMP(0.004f)); emit(p,i0+k,x*a*0.4f); } }
static void ev_chain(int i0,Pos *p,float a){   /* chaine qui bouge */
  int m=(int)rng(4,11); float t=0; for(int j=0;j<m;j++){ t+=rng(0.04f,0.16f); ev_clank(i0+SMP(t),p,rng(1400,2600),rng(0.06f,0.14f),a*rng(0.4f,1.0f)); } }
static void ev_chime(int i0,Pos *p,float f0,float dec,float a){   /* cristal, clochette */
  static const float R[5]={1.0f,2.01f,2.76f,3.93f,5.4f}; static const float A[5]={1,0.45f,0.5f,0.22f,0.12f}; float ph[5]={0}; int n=SMP(dec*5);
  for(int k=0;k<n;k++){ float t=(float)k/SR, x=0; for(int j=0;j<5;j++){ ph[j]+=f0*R[j]/SR; x+=sinf(TF*ph[j])*A[j]*expf(-t*(1+j*0.9f)/dec); }
    emit(p,i0+k,x*a*0.4f*(t<0.002f?t/0.002f:1)); } }
static void ev_bell(int i0,Pos *p,float f0,float dec,float a){   /* cloche lourde, gong */
  static const float R[7]={0.5f,1.0f,1.19f,1.5f,2.0f,2.51f,3.01f}; static const float A[7]={0.6f,1,0.55f,0.4f,0.35f,0.2f,0.12f}; float ph[7]={0}; int n=SMP(dec*4);
  for(int k=0;k<n;k++){ float t=(float)k/SR, x=0; for(int j=0;j<7;j++){ ph[j]+=f0*R[j]*(1+0.0007f*j)/SR; x+=sinf(TF*ph[j])*A[j]*expf(-t*(0.6f+j*0.5f)/dec)*(j>3? fminf(1,t*8):1); }
    if(k<SMP(0.01f)) x+=rnd2()*0.3f*(1-(float)k/SMP(0.01f)); emit(p,i0+k,x*a*0.3f); } }
static void ev_gong(int i0,Pos *p,float f0,float dec,float a){
  static const float R[6]={1.0f,1.47f,2.09f,2.56f,3.14f,3.98f}; float ph[6]={0}; int n=SMP(dec*4);
  for(int k=0;k<n;k++){ float t=(float)k/SR, x=0; for(int j=0;j<6;j++){ ph[j]+=f0*R[j]/SR; float bloom= j==0 ? 1 : fminf(1,t*(2.0f/(j+1)))*expf(-t*j*0.25f); x+=sinf(TF*ph[j])*bloom*expf(-t/dec)/(1+j*0.4f); }
    emit(p,i0+k,x*a*0.35f*(t<0.006f?t/0.006f:1)); } }
static void ev_crack(int i0,Pos *p,float a){   /* glace qui se fend */
  int n=SMP(rng(0.05f,0.16f)); BQ b=bqn(F_BP,rng(1500,4200),0.9f); float f1=rng(1800,5200), f2=f1*rng(1.3f,1.9f), p1=0,p2=0;
  for(int k=0;k<n;k++){ float t=(float)k/SR; float e=expf(-t/(n/(float)SR*0.25f)); p1+=f1/SR; p2+=f2/SR;
    float x=bq(&b,rnd2())*e*2.2f+(sinf(TF*p1)+0.6f*sinf(TF*p2))*expf(-t/0.05f)*0.35f; emit(p,i0+k,x*a); }
  ev_drip(i0,p,rng(60,110),a*0.6f); }
static void ev_creak(int i0,Pos *p,float f0,float dur,float a){   /* craquement (bois, glace, cire) : frottement par a-coups */
  int n=SMP(dur); BQ b1=bqn(F_BP,rng(350,800),3.5f), b2=bqn(F_BP,rng(900,1600),5.0f); float ph=0, g=rng(-0.35f,0.35f);
  for(int k=0;k<n;k++){ float u=(float)k/n; float f=f0*(1+g*u+0.06f*sinf(TF*u*rng(1.5f,1.6f))); ph+=f*(1+0.15f*rnd2())/SR;
    float x=0; if(ph>=1){ ph-=1; x=rng(0.5f,1.0f); }
    float e=sinf(PI*u); e*=e; float y=bq(&b1,x)*1.6f+bq(&b2,x)*0.9f; emit(p,i0+k,y*e*a*1.6f); } }
static void ev_moan(int i0,Pos *p,float f0,float dur,int vow,float a){   /* voix lointaine (fantome, plainte) */
  int n=SMP(dur); float ph=0; BQ F[3]; for(int k=0;k<3;k++) F[k]=bqn(F_BP,VOW[vow][k],VOW[vow][k]/80.0f); float gl=rng(-0.25f,0.15f);
  Pink pk={0};
  for(int k=0;k<n;k++){ float u=(float)k/n; float f=f0*(1+gl*u)*(1+0.012f*sinf(TF*5.3f*k/SR)); float x=saw(&ph,f)*0.6f+pink(&pk)*0.5f;
    float y=bq(&F[0],x)+0.5f*bq(&F[1],x)+0.2f*bq(&F[2],x); float e=sinf(PI*u); e=e*e*e; emit(p,i0+k,y*e*a*2.0f); } }
static void ev_whisper(int i0,Pos *p,float dur,float a){   /* chuchotement : souffle qui change de voyelle */
  int n=SMP(dur); BQ F[3]; for(int k=0;k<3;k++) F[k]=bqn(F_BP,1000,6); int v0=(int)rng(0,5), v1=(int)rng(0,5), last=0; float syl=rng(4,7);
  for(int k=0;k<n;k++){ float u=(float)k/n, s=(float)k/SR*syl; int si=(int)s; float sf=s-si; if(si!=last){ last=si; v0=v1; v1=(int)rng(0,5); }
    if((k&31)==0) for(int j=0;j<3;j++) bq_coef(&F[j],F_BP,VOW[v0][j]*(1-sf)+VOW[v1][j]*sf,6,0);
    float x=rnd2(); float y=bq(&F[0],x)*0.7f+bq(&F[1],x)+0.6f*bq(&F[2],x); float e=sinf(PI*u)*(0.4f+0.6f*sinf(PI*sf)); emit(p,i0+k,y*e*a*1.5f); } }
static void ev_whale(int i0,Pos *p,float f0,float dur,float a){   /* chant grave sous l'eau (Leviathan) */
  int n=SMP(dur); float p1=0,p2=0,p3=0; float r1=rng(1.4f,2.2f), r2=rng(0.7f,1.0f);
  for(int k=0;k<n;k++){ float u=(float)k/n; float f=f0*(u<0.4f ? 1+(r1-1)*sinf(u/0.4f*PI*0.5f) : r1+(r2-r1)*(u-0.4f)/0.6f)*(1+0.01f*sinf(TF*3.1f*k/SR));
    float x=osc(&p1,f)+0.35f*osc(&p2,f*2)+0.12f*osc(&p3,f*3.02f); float e=sinf(PI*u); emit(p,i0+k,x*e*e*a*0.6f); } }
static void ev_thump(int i0,Pos *p,float f0,float a){   /* coup sourd (pas de geant, battement) */
  int n=SMP(0.5f); float ph=0, sb=0; BQ lp=bqn(F_LP,140,0.7f);
  for(int k=0;k<n;k++){ float t=(float)k/SR; float f=f0*(1+0.8f*expf(-t/0.03f)); ph+=f/SR; float x=sinf(TF*ph)*expf(-t/0.13f)+bq(&lp,brown(&sb))*expf(-t/0.06f)*0.6f; emit(p,i0+k,x*a*(t<0.003f?t/0.003f:1)); } }
static void ev_heartbeat(int i0,Pos *p,float a){ ev_thump(i0,p,48,a); ev_thump(i0+SMP(0.27f),p,42,a*0.7f); }
static void ev_boom(int i0,Pos *p,float dur,float a){   /* grondement lointain */
  int n=SMP(dur); float sb=0; BQ lp=bqn(F_LP,rng(70,130),0.8f);
  for(int k=0;k<n;k++){ float u=(float)k/n; float e= u<0.12f ? u/0.12f : expf(-(u-0.12f)*4); emit(p,i0+k,bq(&lp,brown(&sb))*e*a*1.4f); } }
static void ev_howl(int i0,Pos *p,float f0,float a){   /* loup au loin */
  float dur=rng(2.2f,3.6f); int n=SMP(dur); float p1=0,p2=0,p3=0; float pk=rng(1.6f,1.95f), end=rng(1.15f,1.35f);
  for(int k=0;k<n;k++){ float u=(float)k/n; float c= u<0.18f ? 1+(pk-1)*sinf(u/0.18f*PI*0.5f) : (u<0.65f ? pk : pk+(end-pk)*(u-0.65f)/0.35f);
    float f=f0*c*(1+0.018f*sinf(TF*5.5f*k/SR)*fminf(1,u*3)); float x=osc(&p1,f)+0.3f*osc(&p2,f*2)+0.08f*osc(&p3,f*3);
    float e= u<0.1f ? u/0.1f : (u>0.75f ? (1-u)/0.25f : 1); emit(p,i0+k,x*e*a*0.5f); } }
static void ev_hoot(int i0,Pos *p,float f0,float len,float a){
  int n=SMP(len); float ph=0; for(int k=0;k<n;k++){ float u=(float)k/n; float f=f0*(1.04f-0.08f*u); float e=sinf(PI*powf(u,0.6f)); emit(p,i0+k,(sinf(TF*(ph+=f/SR))+0.15f*rnd2()*e)*e*a*0.5f); } }
static void ev_owl(int i0,Pos *p,float a){   /* hibou : hou... hou-hou-houuu */
  float f=rng(330,420); ev_hoot(i0,p,f,0.35f,a); float t=rng(0.9f,1.3f); ev_hoot(i0+SMP(t),p,f,0.18f,a*0.8f); t+=0.3f; ev_hoot(i0+SMP(t),p,f,0.16f,a*0.8f); t+=0.32f; ev_hoot(i0+SMP(t),p,f*0.97f,0.6f,a); }
static void ev_chirp(int i0,Pos *p,float f0,float f1,float len,float a){   /* syllabe d'oiseau */
  int n=SMP(len); float ph=0; for(int k=0;k<n;k++){ float u=(float)k/n; float f=f0+(f1-f0)*u; float e=sinf(PI*u); emit(p,i0+k,sinf(TF*(ph+=f/SR))*e*e*a*0.4f); } }
static void ev_bird(int i0,Pos *p,float a){   /* petite phrase : trille, sifflets, gazouillis */
  int kind=(int)rng(0,3); float base=rng(2200,4200), t=0;
  if(kind==0){ int m=(int)rng(6,16); float r=rng(14,24); for(int j=0;j<m;j++){ ev_chirp(i0+SMP(t),p,base,base*rng(1.15f,1.4f),0.045f,a); t+=1/r; } }
  else if(kind==1){ int m=(int)rng(2,4); for(int j=0;j<m;j++){ float f=base*rng(0.7f,1.0f); ev_chirp(i0+SMP(t),p,f,f*rng(0.95f,1.05f),rng(0.18f,0.35f),a*0.8f); t+=rng(0.3f,0.5f); } }
  else { int m=(int)rng(3,8); for(int j=0;j<m;j++){ float f=base*rng(0.8f,1.3f); ev_chirp(i0+SMP(t),p,f*1.3f,f*0.8f,rng(0.05f,0.09f),a); t+=rng(0.07f,0.14f); } } }
static void ev_nightingale(int i0,Pos *p,float a){   /* rossignol : sifflets qui enflent puis roulades */
  float t=0, f=rng(1500,2400); int m=(int)rng(3,6); for(int j=0;j<m;j++){ int n=SMP(0.08f+0.05f*j); float ph=0;
    for(int k=0;k<n;k++){ float u=(float)k/n; emit(p,i0+SMP(t)+k,sinf(TF*(ph+=f*(1+0.02f*u)/SR))*sinf(PI*u)*(0.4f+0.6f*j/m)*a*0.4f); } t+=0.12f+0.05f*j; }
  int r=(int)rng(8,20); float fr=rng(2800,3800); for(int j=0;j<r;j++){ ev_chirp(i0+SMP(t),p,fr,fr*0.7f,0.03f,a); t+=0.045f; } }
static void ev_croak(int i0,Pos *p,float f0,float a){   /* grenouille : salves rauques */
  int reps=(int)rng(1,4); float t=0; BQ F1=bqn(F_BP,rng(380,620),4), F2=bqn(F_BP,rng(900,1400),5); float pr=rng(18,34), dur=rng(0.12f,0.35f);
  for(int r=0;r<reps;r++){ int n=SMP(dur); float ph=0, pp=0;
    for(int k=0;k<n;k++){ float u=(float)k/n; pp+=pr/SR; float gate= (pp-floorf(pp))<0.55f ? 1 : 0.1f; float x=saw(&ph,f0*(1-0.1f*u))*gate; float y=bq(&F1,x)*1.5f+bq(&F2,x)*0.7f; emit(p,i0+SMP(t)+k,y*sinf(PI*u)*a*1.5f); }
    t+=dur+rng(0.12f,0.3f); } }
static void ev_peeper(int i0,Pos *p,float a){ float f=rng(2700,3200); ev_chirp(i0,p,f*0.92f,f*1.08f,rng(0.1f,0.15f),a); }
static void ev_flyby(int i0,float f0,float a,float dur){   /* insecte qui passe (Doppler, pan qui glisse) */
  int n=SMP(dur); float ph=0; float p0=rnd2()*1.2f, p1=-p0+rnd2()*0.3f; BQ b=bqn(F_BP,f0*2.5f,0.9f), lp=bqn(F_LP,3200,0.7f);
  for(int k=0;k<n;k++){ float u=(float)k/n; float x=(u-0.5f)*2; float dop=1-0.07f*x; float e=1/(1+x*x*9); float pan=p0+(p1-p0)*u;
    float y=bq(&lp,bq(&b,saw(&ph,f0*dop*(1+0.02f*sinf(TF*31*k/SR))))); emit_pan(i0+k,y,pan,e*a,e*a*0.4f); } }
static void ev_swell(int i0,Pos *p,float dur,float a){   /* souffle a l'envers (Neant) */
  int n=SMP(dur); Pink pk={0}; BQ b=bqn(F_BP,rng(300,900),1.2f);
  for(int k=0;k<n;k++){ float u=(float)k/n; float e=powf(u,3)*(u>0.97f?(1-u)/0.03f:1); emit(p,i0+k,bq(&b,pink(&pk))*e*a*3); } }
static void ev_glass(int i0,Pos *p,float f0,float dur,float a){   /* sons de verre tenus, qui battent */
  int n=SMP(dur); float p1=0,p2=0; float d=rng(0.8f,2.5f);
  for(int k=0;k<n;k++){ float u=(float)k/n; float e=sinf(PI*u); emit(p,i0+k,(osc(&p1,f0)+osc(&p2,f0+d))*e*e*a*0.3f); } }
static void ev_cart(int i0,float a){   /* wagonnet sur les rails, au loin */
  float dur=rng(4,7); int n=SMP(dur); float sb=0; BQ lp=bqn(F_LP,240,0.9f); float p0=rnd2(), p1=-p0; float clack=rng(1.6f,2.4f);
  Pos pc=pos(0,0.7f);
  for(int k=0;k<n;k++){ float u=(float)k/n; float e=sinf(PI*u); float pan=p0+(p1-p0)*u; emit_pan(i0+k,bq(&lp,brown(&sb))*e*a*1.2f,pan,0.5f,0.5f); }
  for(float t=0.2f;t<dur-0.3f;t+=1/clack){ float u=t/dur; pc=pos(p0+(p1-p0)*u,0.7f); ev_clank(i0+SMP(t),&pc,rng(500,700),0.05f,a*0.6f*sinf(PI*u)); ev_clank(i0+SMP(t+0.09f),&pc,rng(500,700),0.05f,a*0.45f*sinf(PI*u)); } }
static void ev_pick(int i0,Pos *p,float a){   /* pioche lointaine : serie de coups */
  int m=(int)rng(3,9); float t=0, f=rng(1100,1800), per=rng(0.7f,1.1f); for(int j=0;j<m;j++){ ev_clank(i0+SMP(t),p,f*rng(0.97f,1.03f),0.12f,a*rng(0.6f,1)); ev_pop(i0+SMP(t),p,a*0.3f); t+=per*rng(0.9f,1.1f); } }
static void ev_anvil(int i0,Pos *p,float a){   /* forge : marteau sur l'enclume */
  int m=(int)rng(2,6); float t=0, f=rng(520,780); for(int j=0;j<m;j++){ ev_clank(i0+SMP(t),p,f,rng(0.6f,1.1f),a*(j==m-1?1.0f:0.7f)); t+=rng(0.45f,0.7f); } }
static void ev_blop(int i0,Pos *p,float a){   /* bulle de lave ou de boue */
  float f=rng(55,140); int n=SMP(0.35f); float ph=0, sb=0; BQ lp=bqn(F_LP,400,0.7f);
  for(int k=0;k<n;k++){ float t=(float)k/SR; ph+=f*(1+1.6f*t/0.35f)/SR; float x=sinf(TF*ph)*expf(-t/0.07f)+bq(&lp,brown(&sb))*expf(-t/0.03f)*0.5f; emit(p,i0+k,x*a); } }
static void ev_bellows(int i0,Pos *p,float a){   /* soufflet de forge */
  float dur=rng(1.6f,2.4f); int n=SMP(dur); Pink pk={0}; BQ b=bqn(F_BP,rng(250,450),0.8f);
  for(int k=0;k<n;k++){ float u=(float)k/n; float e= u<0.45f ? sinf(u/0.45f*PI*0.5f) : cosf((u-0.45f)/0.55f*PI*0.5f); emit(p,i0+k,bq(&b,pink(&pk))*e*e*a*3); } }
static void ev_rattle(int i0,Pos *p,float a){   /* os qui s'entrechoquent */
  int m=(int)rng(3,9); float t=0; for(int j=0;j<m;j++){ t+=rng(0.03f,0.12f); BQ b=bqn(F_BP,rng(900,2200),6); int n=SMP(0.05f);
    for(int k=0;k<n;k++){ float x= k<40 ? rnd2()*(1-k/40.0f) : 0; emit(p,i0+SMP(t)+k,bq(&b,x)*a*rng(0.5f,1)*6); } } }
static void ev_sand(int i0,Pos *p,float dur,float a){   /* sable qui s'ecoule */
  int n=SMP(dur); BQ h=bqn(F_HP,3000,0.7f); for(int k=0;k<n;k++){ float u=(float)k/n; float g= rnd()<0.08f ? rnd() : 0; emit(p,i0+k,bq(&h,rnd2()*g)*sinf(PI*u)*a*3); } }
static void ev_spore(int i0,Pos *p,float a){   /* « pouf » de spores */
  int n=SMP(rng(0.25f,0.5f)); Pink pk={0}; BQ lp=bqn(F_LP,rng(500,1100),0.8f);
  for(int k=0;k<n;k++){ float u=(float)k/n; float e= u<0.06f ? u/0.06f : expf(-(u-0.06f)*6); emit(p,i0+k,bq(&lp,pink(&pk))*e*a*3); } }
static void ev_breath(int i0,Pos *p,float dur,float a){   /* respiration lente (Mere-Spore) */
  int n=SMP(dur); Pink pk={0}; BQ b=bqn(F_BP,350,1.0f);
  for(int k=0;k<n;k++){ float u=(float)k/n; float e=sinf(PI*u); if((k&63)==0) bq_coef(&b,F_BP,250+250*u,1.0f,0); emit(p,i0+k,bq(&b,pink(&pk))*e*e*a*3); } }

/* planificateur : evenements de Poisson, densite par minute */
#define EVERY(per_min) for(float t=rng(0,60.0f/(per_min)); t<DUR; t+=expo((per_min)/60.0f))

/* =====================================================================================
   les pistes
   ===================================================================================== */
typedef struct { float room, damp, wet, size, pre; float lp; float db; } Room;   /* echo, sourdine (lp>0), volume final */

static Room trk_level(int b,bool arena){
  Room R={0.86f,0.45f,0.5f,1.2f,0.03f,0,-24};
  switch(b){
  case 0: {   /* Catacombes : crypte seche, vent dans les galeries, os, chaines */
    float dr[3]={55,82.5f,110.2f}; bed_drone(dr,3,0.3f,arena?-30:-36,0.3f);
    bed_wind(110,420,1.1f,7,0.7f,0.8f,arena?-25:-26,0.35f);
    bed_rumble(90,12,arena?-29:-34,0.2f);
    EVERY(14){ Pos p=posr(0.3f,1); ev_drip(SMP(t),&p,rng(800,1700),rng(0.5f,1)); } commit_ev(-20);
    EVERY(arena?4.0f:2.2f){ Pos p=posr(0.4f,1); ev_rattle(SMP(t),&p,1); } commit_ev(-25);
    EVERY(1.3f){ Pos p=posr(0.6f,1); ev_chain(SMP(t),&p,1); } commit_ev(-27);
    EVERY(arena?1.6f:1.1f){ Pos p=posr(0.7f,1); ev_moan(SMP(t),&p,rng(110,190),rng(2.5f,4.5f),2,1); } commit_ev(arena?-26:-28);
    if(arena){ for(float t=rng(0,1);t<DUR;t+=60.0f/rng(46,52)){ Pos p=pos(0,0.5f); ev_heartbeat(SMP(t),&p,1); } commit_ev(-23);
               EVERY(2.5f){ Pos p=posr(0.5f,1); ev_boom(SMP(t),&p,rng(2,4),1); } commit_ev(-21); }
    R.room=0.88f; R.damp=0.5f; R.wet=0.55f; R.size=1.3f; break; }
  case 1: {   /* Foret fongique : grotte humide, gouttes, ruisselet, spores, scarabees */
    bed_wind(160,520,0.9f,9,0.5f,0.9f,-31,0.4f);
    bed_babble(400,1300,-37,0.3f); bed_bubbles(40,500,2200,-33,0.3f,0.3f,0.9f);
    EVERY(40){ Pos p=posr(0.15f,1); ev_drip(SMP(t),&p,rng(700,2000),rng(0.4f,1)); } commit_ev(-21);
    EVERY(6){ Pos p=posr(0.3f,0.9f); ev_spore(SMP(t),&p,1); } commit_ev(-27);
    EVERY(3){ Pos p=posr(0.4f,1); ev_rattle(SMP(t),&p,0.6f); } commit_ev(-30);   /* cliquetis de scarabees */
    EVERY(1.5f){ Pos p=posr(0.5f,1); ev_creak(SMP(t),&p,rng(40,90),rng(0.8f,1.8f),1); } commit_ev(-27);
    if(arena){ for(float t=rng(0,2);t<DUR;t+=rng(4.5f,6.5f)){ Pos p=pos(rnd2()*0.3f,0.4f); ev_breath(SMP(t),&p,rng(3,4.5f),1); } commit_ev(-21);
               float dr[2]={46,69.3f}; bed_drone(dr,2,0.5f,-30,0.3f); }
    R.room=0.85f; R.damp=0.6f; R.wet=0.5f; R.size=1.1f; break; }
  case 2: {   /* Forges englouties : brasier, enclumes, soufflets, lave */
    bed_roar(380,0.9f,arena?-22:-24,0.25f);
    EVERY(160){ Pos p=posr(0.1f,0.8f); ev_crackle(SMP(t),&p,1); } commit_ev(-18);
    EVERY(4){ Pos p=posr(0.5f,1); ev_anvil(SMP(t),&p,1); } commit_ev(-24);
    EVERY(5){ Pos p=posr(0.3f,0.9f); ev_bellows(SMP(t),&p,1); } commit_ev(-27);
    EVERY(arena?16:10){ Pos p=posr(0.2f,0.9f); ev_blop(SMP(t),&p,rng(0.5f,1)); } commit_ev(-22);
    EVERY(1.5f){ Pos p=posr(0.4f,1); ev_creak(SMP(t),&p,rng(60,140),rng(0.6f,1.4f),1); } commit_ev(-28);
    if(arena){ for(float t=rng(0,1);t<DUR;t+=rng(1.1f,1.6f)){ Pos p=pos(rnd2()*0.2f,0.45f); ev_thump(SMP(t),&p,rng(34,42),1); } commit_ev(-20);
               bed_rumble(110,6,-26,0.2f); }
    R.room=0.8f; R.damp=0.55f; R.wet=0.42f; R.size=1.0f; break; }
  case 3: {   /* Coeur du gouffre : battement, bourdon violet, chuchotements, verre */
    float dr[3]={41.2f,61.7f,98}; bed_drone(dr,3,0.6f,arena?-26:-29,0.4f);
    for(float t=rng(0,1);t<DUR;t+=60.0f/(arena?rng(70,76):rng(50,56))){ Pos p=pos(0,0.5f); ev_heartbeat(SMP(t),&p,1); } commit_ev(arena?-19:-23);
    EVERY(arena?5:3){ Pos p=posr(0.3f,1); ev_whisper(SMP(t),&p,rng(1.2f,2.6f),1); } commit_ev(-29);
    EVERY(2){ Pos p=posr(0.4f,1); ev_glass(SMP(t),&p,rng(900,2400),rng(4,8),1); } commit_ev(-31);
    bed_wind(80,300,1.4f,11,0.6f,0.8f,-31,0.4f);
    if(arena){ EVERY(3){ Pos p=posr(0.4f,1); ev_boom(SMP(t),&p,rng(2,4),1); } commit_ev(-21); }
    R.room=0.92f; R.damp=0.35f; R.wet=0.65f; R.size=1.6f; break; }
  case 4: {   /* Grottes de givre : vent qui siffle, glace qui craque, cristaux */
    bed_wind(500,1500,arena?6:8,6,0.8f,0.7f,arena?-24:-27,0.45f);
    bed_wind(150,600,1.0f,8,0.7f,0.9f,arena?-24:-28,0.35f);
    EVERY(arena?10:5){ Pos p=posr(0.2f,1); ev_crack(SMP(t),&p,rng(0.5f,1)); } commit_ev(-22);
    EVERY(9){ Pos p=posr(0.3f,1); ev_chime(SMP(t),&p,rng(1800,4200),rng(0.8f,2.2f),rng(0.4f,1)); } commit_ev(-27);
    EVERY(2){ Pos p=posr(0.4f,1); ev_creak(SMP(t),&p,rng(20,50),rng(1.5f,3),1); } commit_ev(-25);
    EVERY(5){ Pos p=posr(0.4f,1); ev_drip(SMP(t),&p,rng(1200,2400),0.7f); } commit_ev(-25);
    if(arena){ bed_rustle(3500,3,420,-31,0.3f); EVERY(2){ Pos p=posr(0.6f,1); ev_howl(SMP(t),&p,rng(300,380),1); } commit_ev(-28); }
    R.room=0.9f; R.damp=0.25f; R.wet=0.6f; R.size=1.5f; break; }
  case 5: {   /* Cite engloutie : sous l'eau, bulles, courant, chant du Leviathan, cloche noyee */
    bed_current(arena?-21:-23,0.3f);
    bed_bubbles(arena?30:16,250,1600,-30,0.4f,0.2f,1);
    EVERY(arena?3:1.5f){ Pos p=posr(0.6f,1); ev_whale(SMP(t),&p,rng(70,140),rng(4,7),1); } commit_ev(arena?-21:-25);
    EVERY(1.2f){ Pos p=posr(0.6f,1); ev_bell(SMP(t),&p,rng(110,170),rng(3,5),1); } commit_ev(-25);
    EVERY(3){ Pos p=posr(0.4f,1); ev_creak(SMP(t),&p,rng(30,70),rng(1.5f,3),1); } commit_ev(-29);
    if(arena){ bed_rumble(80,7,-25,0.2f); }
    R.room=0.9f; R.damp=0.7f; R.wet=0.6f; R.size=1.4f; R.lp=1300; break; }
  case 6: {   /* Ruche d'ambre : essaim, cire qui craque, miel qui goutte, survols */
    bed_swarm(arena?36:26,160,260,700,arena?-24:-27,0.3f);
    EVERY(arena?14:8){ ev_flyby(SMP(t),rng(200,320),1,rng(1.4f,2.6f)); } commit_ev(-24);
    EVERY(5){ Pos p=posr(0.3f,1); ev_creak(SMP(t),&p,rng(50,120),rng(0.5f,1.2f),1); } commit_ev(-27);
    EVERY(10){ Pos p=posr(0.2f,0.9f); ev_drip(SMP(t),&p,rng(300,600),rng(0.5f,1)); } commit_ev(-24);
    if(arena){ float dr[2]={55,110}; bed_drone(dr,2,1.0f,-28,0.3f); for(float t=rng(0,1);t<DUR;t+=60.0f/62){ Pos p=pos(0,0.5f); ev_thump(SMP(t),&p,40,1); } commit_ev(-24); }
    R.room=0.75f; R.damp=0.6f; R.wet=0.38f; R.size=0.9f; break; }
  case 7: {   /* Neant : presque rien, souffles a l'envers, voix, sons de verre */
    float dr[3]={36.7f,55,73.4f}; bed_drone(dr,3,0.25f,arena?-30:-38,0.5f);
    EVERY(arena?4:2){ Pos p=posr(0.3f,1); ev_swell(SMP(t),&p,rng(2,4),1); } commit_ev(-26);
    EVERY(1.5f){ Pos p=posr(0.6f,1); ev_moan(SMP(t),&p,rng(90,160),rng(3,6),(int)rng(0,3),1); } commit_ev(-27);
    EVERY(2.5f){ Pos p=posr(0.5f,1); ev_glass(SMP(t),&p,rng(600,1800),rng(5,9),1); } commit_ev(-31);
    EVERY(1.5f){ Pos p=posr(0.4f,1); ev_whisper(SMP(t),&p,rng(1.5f,3),1); } commit_ev(-28);
    if(arena){ EVERY(3){ Pos p=posr(0.4f,1); ev_boom(SMP(t),&p,rng(3,5),1); } commit_ev(-20); bed_rumble(60,9,-25,0.3f); }
    R.room=0.95f; R.damp=0.3f; R.wet=0.75f; R.size=1.9f; break; }
  case 8: {   /* Jardin des lucioles : oiseaux de nuit, brise, fontaine, insectes */
    bed_wind(250,900,0.8f,8,0.6f,0.9f,-31,0.3f); bed_rustle(2500,8,160,-36,0.2f);
    bed_babble(500,1600,-37,0.25f); bed_bubbles(60,700,2600,-35,0.2f,0.4f,1);
    EVERY(arena?4:7){ Pos p=posr(0.2f,1); ev_bird(SMP(t),&p,1); } commit_ev(-25);
    bed_crickets(arena?4:7,-36,0.25f);
    EVERY(3){ Pos p=posr(0.3f,1); ev_chime(SMP(t),&p,rng(1500,3000),rng(1,2),0.6f); } commit_ev(-30);
    if(arena){ EVERY(2){ Pos p=posr(0.4f,1); ev_creak(SMP(t),&p,rng(25,60),rng(2,3.5f),1); } commit_ev(-23); float dr[2]={49,73.4f}; bed_drone(dr,2,0.4f,-30,0.3f); }
    R.room=0.7f; R.damp=0.5f; R.wet=0.32f; R.size=1.0f; break; }
  case 9: {   /* Marais maudit : grenouilles, boue qui bulle, moustiques, roseaux */
    EVERY(arena?40:28){ Pos p=posr(0.3f,1); ev_croak(SMP(t),&p,rng(110,260),rng(0.5f,1.0f)); } commit_ev(-31);
    EVERY(14){ Pos p=posr(0.2f,1); ev_blop(SMP(t),&p,rng(0.5f,1)); } commit_ev(-25);
    EVERY(5){ ev_flyby(SMP(t),rng(520,680),0.6f,rng(1.2f,2.2f)); } commit_ev(-31);
    bed_rustle(1800,7,130,-34,0.2f); bed_wind(200,600,0.9f,9,0.5f,0.9f,-33,0.3f);
    EVERY(1.5f){ Pos p=posr(0.6f,1); ev_bird(SMP(t),&p,0.7f); } commit_ev(-31);
    if(arena){ EVERY(6){ Pos p=posr(0.3f,0.7f); ev_croak(SMP(t),&p,rng(60,90),1); } commit_ev(-27); bed_rumble(90,8,-28,0.2f); }
    R.room=0.6f; R.damp=0.6f; R.wet=0.25f; R.size=0.9f; break; }
  case 10: {   /* Mines de cristal : pioches, wagonnets, poutres, cristaux qui chantent */
    bed_wind(120,420,1.0f,9,0.5f,0.8f,-31,0.35f); bed_rumble(100,10,-34,0.2f);
    EVERY(2.2f){ Pos p=posr(0.5f,1); ev_pick(SMP(t),&p,1); } commit_ev(-25);
    EVERY(0.8f){ ev_cart(SMP(t),1); } commit_ev(-27);
    EVERY(2.5f){ Pos p=posr(0.3f,1); ev_creak(SMP(t),&p,rng(40,100),rng(1,2.2f),1); } commit_ev(-26);
    EVERY(arena?12:7){ Pos p=posr(0.2f,1); ev_chime(SMP(t),&p,rng(1200,3600),rng(1.5f,3.5f),rng(0.5f,1)); } commit_ev(-26);
    EVERY(12){ Pos p=posr(0.3f,1); ev_drip(SMP(t),&p,rng(900,1900),0.8f); } commit_ev(-25);
    if(arena){ EVERY(3){ Pos p=posr(0.3f,1); ev_glass(SMP(t),&p,rng(700,1500),rng(4,7),1); } commit_ev(-25); for(float t=rng(0,1);t<DUR;t+=rng(1.4f,2.0f)){ Pos p=pos(rnd2()*0.2f,0.45f); ev_thump(SMP(t),&p,rng(38,46),1); } commit_ev(-21); }
    R.room=0.87f; R.damp=0.35f; R.wet=0.5f; R.size=1.3f; break; }
  default: {   /* Sanctuaire du Soleil noir : choeur, gong, braseros, sable */
    static const float CH[4*4]={146.8f,220,293.7f,349.2f, 130.8f,196,261.6f,329.6f, 116.5f,174.6f,233.1f,293.7f, 130.8f,196,246.9f,311.1f};
    bed_choir(CH,4,4,arena?11:16,arena?-25:-28,0.5f);
    bed_roar(300,1.1f,-32,0.2f); EVERY(60){ Pos p=posr(0.2f,0.8f); ev_crackle(SMP(t),&p,1); } commit_ev(-25);
    EVERY(1.2f){ Pos p=posr(0.5f,1); ev_gong(SMP(t),&p,rng(70,110),rng(4,7),1); } commit_ev(-24);
    EVERY(2){ Pos p=posr(0.3f,1); ev_sand(SMP(t),&p,rng(2,5),1); } commit_ev(-31);
    bed_wind(180,640,0.9f,10,0.6f,0.9f,-31,0.4f);
    if(arena){ EVERY(3){ Pos p=posr(0.4f,1); ev_boom(SMP(t),&p,rng(2,4),1); } commit_ev(-21); bed_roar(200,0.7f,-27,0.2f); }
    R.room=0.9f; R.damp=0.45f; R.wet=0.6f; R.size=1.6f; break; }
  }
  return R;
}
static Room trk_camp(int season){
  Room R={0.6f,0.5f,0.22f,1.0f,0.02f,0,-25};
  bed_roar(320,0.7f,-30,0.15f);
  EVERY(90){ Pos p=pos(rnd2()*0.25f,rng(0.15f,0.35f)); ev_crackle(SMP(t),&p,1); } commit_ev(-20);
  EVERY(6){ Pos p=pos(rnd2()*0.25f,0.2f); ev_pop(SMP(t),&p,1); } commit_ev(-27);
  switch(season){
    case 0:   /* hiver : vent froid qui siffle, loups au loin, hibou */
      bed_wind(400,1300,5,7,0.8f,0.7f,-27,0.3f); bed_wind(140,500,1.0f,9,0.6f,0.9f,-29,0.3f);
      EVERY(0.8f){ Pos p=posr(0.75f,1); ev_howl(SMP(t),&p,rng(320,400),1); } commit_ev(-26);
      EVERY(0.6f){ Pos p=posr(0.5f,0.9f); ev_owl(SMP(t),&p,1); } commit_ev(-27);
      break;
    case 1:   /* printemps : rossignol, rainettes, brise douce, ruisseau */
      bed_wind(250,800,0.8f,9,0.5f,0.9f,-32,0.25f); bed_babble(500,1500,-35,0.2f); bed_bubbles(50,700,2400,-36,0.2f,0.6f,1);
      EVERY(2.5f){ Pos p=posr(0.3f,0.9f); ev_nightingale(SMP(t),&p,1); } commit_ev(-26);
      EVERY(30){ Pos p=posr(0.5f,1); ev_peeper(SMP(t),&p,1); } commit_ev(-29);
      break;
    case 2:   /* ete : grillons, grenouilles au loin, brise tiede, hibou */
      bed_crickets(10,-37,0.2f); bed_wind(250,700,0.8f,10,0.4f,0.9f,-34,0.25f);
      EVERY(5){ Pos p=posr(0.7f,1); ev_croak(SMP(t),&p,rng(120,220),1); } commit_ev(-29);
      EVERY(0.5f){ Pos p=posr(0.5f,0.9f); ev_owl(SMP(t),&p,1); } commit_ev(-28);
      break;
    default:   /* automne : rafales, feuilles qui volent, hiboux */
      bed_wind(180,700,0.9f,6,0.85f,0.85f,-27,0.3f); bed_rustle(2200,6,360,-30,0.2f);
      EVERY(1.2f){ Pos p=posr(0.4f,0.9f); ev_owl(SMP(t),&p,1); } commit_ev(-26);
      EVERY(2){ Pos p=posr(0.4f,1); ev_creak(SMP(t),&p,rng(60,120),rng(0.8f,1.6f),1); } commit_ev(-29);   /* branches */
      break;
  }
  return R;
}

/* =====================================================================================
   echo (Freeverb), finition, boucle, ecriture
   ===================================================================================== */
static void reverb(const Room *R){
  static const int CT[8]={1116,1188,1277,1356,1422,1491,1557,1617}, AT[4]={556,441,341,225};
  float *cb[2][8], cs[2][8]={{0}}; int ci[2][8]={{0}}, cn[2][8]; float *ab[2][4]; int ai[2][4]={{0}}, an[2][4];
  for(int c=0;c<2;c++){ for(int k=0;k<8;k++){ cn[c][k]=(int)(CT[k]*R->size)+c*23; cb[c][k]=(float*)calloc((size_t)cn[c][k],4); }
                        for(int k=0;k<4;k++){ an[c][k]=AT[k]+c*23; ab[c][k]=(float*)calloc((size_t)an[c][k],4); } }
  float fb=R->room*0.28f+0.7f, damp=R->damp*0.4f; int pre=SMP(R->pre); float *pd=(float*)calloc((size_t)pre+1,4); int pi=0;
  float wet1=R->wet*0.75f, wet2=R->wet*0.25f;
  /* deux passes : la premiere remplit l'echo avec la fin du morceau (boucle) */
  for(int i=0;i<N;i++){
    float in=(WS.l[i]+WS.r[i])*0.5f*0.03f; float x=pd[pi]; pd[pi]=in; if(++pi>pre) pi=0;
    float o[2];
    for(int c=0;c<2;c++){ float acc=0;
      for(int k=0;k<8;k++){ float y=cb[c][k][ci[c][k]]; cs[c][k]=y*(1-damp)+cs[c][k]*damp; cb[c][k][ci[c][k]]=x+cs[c][k]*fb; if(++ci[c][k]>=cn[c][k]) ci[c][k]=0; acc+=y; }
      for(int k=0;k<4;k++){ float b=ab[c][k][ai[c][k]]; float oo=b-acc; ab[c][k][ai[c][k]]=acc+b*0.5f; if(++ai[c][k]>=an[c][k]) ai[c][k]=0; acc=oo; }
      o[c]=acc; }
    D.l[i]+=(o[0]*wet1+o[1]*wet2)*3; D.r[i]+=(o[1]*wet1+o[0]*wet2)*3;
  }
  for(int c=0;c<2;c++){ for(int k=0;k<8;k++) free(cb[c][k]); for(int k=0;k<4;k++) free(ab[c][k]); }
  free(pd);
}
static void write_wav(const char *path,const int16_t *pcm,int frames){
  FILE *f=fopen(path,"wb"); if(!f){ printf("  ecriture impossible : %s\n",path); return; }
  uint32_t data=(uint32_t)frames*4, riff=36+data; uint16_t u16; uint32_t u32;
  fwrite("RIFF",1,4,f); fwrite(&riff,4,1,f); fwrite("WAVEfmt ",1,8,f); u32=16; fwrite(&u32,4,1,f); u16=1; fwrite(&u16,2,1,f); u16=2; fwrite(&u16,2,1,f);
  u32=SR; fwrite(&u32,4,1,f); u32=SR*4; fwrite(&u32,4,1,f); u16=4; fwrite(&u16,2,1,f); u16=16; fwrite(&u16,2,1,f);
  fwrite("data",1,4,f); fwrite(&data,4,1,f); fwrite(pcm,4,(size_t)frames,f); fclose(f);
}
static void finish_track(const char *path,const Room *R){
  reverb(R);
  /* sourdine (sous l'eau), coupe des infra-graves, deux passes pour que les filtres bouclent */
  { BQ hl=bqn(F_HP,28,0.7f), hr=hl, ll=bqn(F_LP,R->lp>0?R->lp:18000,0.7f), lr=ll;
    for(int pass=0;pass<2;pass++) for(int i=0;i<N;i++){ float l=bq(&hl,D.l[i]), r=bq(&hr,D.r[i]); if(R->lp>0){ l=bq(&ll,l); r=bq(&lr,r); } if(pass==1){ D.l[i]=l; D.r[i]=r; } } }
  /* boucle : la fin se fond dans le debut */
  for(int i=0;i<NX;i++){ float u=(float)i/NX; float a=sinf(u*PI*0.5f), b=cosf(u*PI*0.5f);
    D.l[i]=D.l[i]*a+D.l[NL+i]*b; D.r[i]=D.r[i]*a+D.r[NL+i]*b; }
  /* volume moyen commun, cretes adoucies */
  double s=0; { BQ h1=bqn(F_HP,60,0.5f), h2=h1, p1, p2; memset(&p1,0,sizeof p1); bq_coef(&p1,F_PK,3000,0.5f,4.0f); p2=p1;
    for(int i=0;i<NL;i++){ float l=bq(&p1,bq(&h1,D.l[i])), r=bq(&p2,bq(&h2,D.r[i])); s+=(double)l*l+(double)r*r; } }
  float rms=(float)sqrt(s/(2.0*NL)); float k=dbl(R->db)/fmaxf(rms,1e-9f);
  int16_t *pcm=(int16_t*)malloc((size_t)NL*4); float pk=0; int clip=0;
  for(int i=0;i<NL;i++) for(int c=0;c<2;c++){ float v=(c?D.r[i]:D.l[i])*k; if(fabsf(v)>pk) pk=fabsf(v);
    if(fabsf(v)>0.6f){ float e=fabsf(v)-0.6f; v=(v>0?1:-1)*(0.6f+0.35f*e/(e+0.35f)); clip++; }
    float dith=(rnd()-rnd())*(1.0f/32768.0f); pcm[i*2+c]=(int16_t)clampi((int)lrintf((v+dith)*32767.0f),-32767,32767); }
  write_wav(path,pcm,NL);
  printf("  %s : %.1f Mo, crete %.2f, adoucies %d\n",path,NL*4/1048576.0,pk,clip); fflush(stdout);
  free(pcm);
}
static void track_begin(float dur,uint32_t seed){
  DUR=dur+4.0f; NL=SMP(dur); NX=SMP(4.0f); N=NL+NX; rs=seed*2654435761u+12345u; if(!rs) rs=1;
  D=st_alloc(); WS=st_alloc(); T=st_alloc(); TS=st_alloc();
}
static void track_end(void){ st_free(&D); st_free(&WS); st_free(&T); st_free(&TS); }

/* pistes 0..11 : niveaux (exploration), 12..23 : arenes, 24..27 : campement (saisons) */
int amb_bake(const char *dir,int t0,int t1){
  char p[600];
  for(int t=t0;t<=t1 && t<28;t++){
    if(t<12){ printf("ambiance : %s\n",BIOMES[t].name); track_begin(300.0f,(uint32_t)t*977u+11u); Room R=trk_level(t,false); snprintf(p,sizeof p,"%s/niveau_%02d.wav",dir,t+1); finish_track(p,&R); }
    else if(t<24){ int b=t-12; printf("ambiance : %s (arene)\n",BIOMES[b].name); track_begin(240.0f,(uint32_t)b*1931u+29u); Room R=trk_level(b,true); snprintf(p,sizeof p,"%s/niveau_%02d_arene.wav",dir,b+1); finish_track(p,&R); }
    else { int s=t-24; printf("ambiance : campement (saison %d)\n",s); track_begin(240.0f,(uint32_t)s*7717u+3u); Room R=trk_camp(s); snprintf(p,sizeof p,"%s/campement_%d.wav",dir,s); finish_track(p,&R); }
    track_end();
  }
  return 0;
}
#endif
