/* GOUFFRE - icones realistes des billes et des objets : rendu 3D procedural.
   Chaque icone est une petite scene : une forme decrite par un champ de distance,
   des materiaux (metal poli, verre, lave, glace, cuir, bois...) eclaires comme en studio
   (lumiere principale, contre-jour, environnement reflechi), et des effets dessines
   autour (flammes, givre, eclairs, fumee, etincelles, halo).
   Le rendu se fait en 128x128 sur-echantillonne, ligne par ligne, pour etre etale
   sur plusieurs images au demarrage du jeu. */
#include "game.h"
#include <math.h>

#define N ICON_N
#define FN ((float)ICON_N)

/* ======================================================================= */
/*                              petits outils                              */
/* ======================================================================= */
typedef struct { float x,y,z; } F3;
#define V(a,b,c) ((F3){(float)(a),(float)(b),(float)(c)})
static inline F3 add(F3 a,F3 b){ return V(a.x+b.x,a.y+b.y,a.z+b.z); }
static inline F3 sub(F3 a,F3 b){ return V(a.x-b.x,a.y-b.y,a.z-b.z); }
static inline F3 mul(F3 a,float s){ return V(a.x*s,a.y*s,a.z*s); }
static inline F3 mulv(F3 a,F3 b){ return V(a.x*b.x,a.y*b.y,a.z*b.z); }
static inline float dot(F3 a,F3 b){ return a.x*b.x+a.y*b.y+a.z*b.z; }
static inline float len(F3 a){ return sqrtf(dot(a,a)); }
static inline F3 nrm(F3 a){ float l=len(a); return l>1e-12f?mul(a,1.0f/l):V(0,0,1); }
static inline F3 cross(F3 a,F3 b){ return V(a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x); }
static inline F3 mix3(F3 a,F3 b,float t){ return V(a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t); }
static inline float mixf(float a,float b,float t){ return a+(b-a)*t; }
static inline float sat(float x){ return x<0?0:(x>1?1:x); }
static inline float sst(float a,float b,float x){ float t=sat((x-a)/(b-a)); return t*t*(3-2*t); }
static inline float maxc(F3 a){ return fmaxf(a.x,fmaxf(a.y,a.z)); }
static inline F3 refl(F3 d,F3 n){ return sub(d,mul(n,2*dot(d,n))); }
static inline F3 hexc(uint32_t h){ /* couleur sRGB -> lineaire */
  return V(powf(((h>>16)&255)/255.0f,2.2f),powf(((h>>8)&255)/255.0f,2.2f),powf((h&255)/255.0f,2.2f)); }
static inline F3 rotx(F3 p,float a){ float c=cosf(a),s=sinf(a); return V(p.x,c*p.y-s*p.z,s*p.y+c*p.z); }
static inline F3 roty(F3 p,float a){ float c=cosf(a),s=sinf(a); return V(c*p.x+s*p.z,p.y,-s*p.x+c*p.z); }
static inline F3 rotz(F3 p,float a){ float c=cosf(a),s=sinf(a); return V(c*p.x-s*p.y,s*p.x+c*p.y,p.z); }
static inline int ffl(float x){ int i=(int)x; return i-(x<(float)i); }
static inline bool refr(F3 d,F3 n,float eta,F3 *t){
  float c=-dot(n,d), k=1-eta*eta*(1-c*c); if(k<0) return false;
  *t=add(mul(d,eta),mul(n,eta*c-sqrtf(k))); return true; }

/* ---------------- bruit ---------------- */
static uint8_t PRM[512]; static bool prm_ok=false;
static void prm_init(void){
  if(prm_ok) return;
  prm_ok=true; uint32_t s=0x6d2b79f5u;
  for(int i=0;i<256;i++) PRM[i]=(uint8_t)i;
  for(int i=255;i>0;i--){ s=s*1664525u+1013904223u; int j=(int)((s>>8)%(uint32_t)(i+1)); uint8_t t=PRM[i]; PRM[i]=PRM[j]; PRM[j]=t; }
  for(int i=0;i<256;i++) PRM[256+i]=PRM[i];
}
static inline float fade(float t){ return t*t*t*(t*(t*6-15)+10); }
static inline float grd(int h,float x,float y,float z){
  h&=15; float u=h<8?x:y, v=h<4?y:((h==12||h==14)?x:z); return ((h&1)?-u:u)+((h&2)?-v:v); }
static float pn(F3 p){   /* bruit de gradient, environ -1..1 */
  int X=ffl(p.x),Y=ffl(p.y),Z=ffl(p.z); float x=p.x-X,y=p.y-Y,z=p.z-Z; X&=255; Y&=255; Z&=255;
  float u=fade(x),v=fade(y),w=fade(z);
  int A=PRM[X]+Y,AA=PRM[A]+Z,AB=PRM[A+1]+Z,B=PRM[X+1]+Y,BA=PRM[B]+Z,BB=PRM[B+1]+Z;
  float a0=mixf(grd(PRM[AA],x,y,z),grd(PRM[BA],x-1,y,z),u), a1=mixf(grd(PRM[AB],x,y-1,z),grd(PRM[BB],x-1,y-1,z),u);
  float b0=mixf(grd(PRM[AA+1],x,y,z-1),grd(PRM[BA+1],x-1,y,z-1),u), b1=mixf(grd(PRM[AB+1],x,y-1,z-1),grd(PRM[BB+1],x-1,y-1,z-1),u);
  return mixf(mixf(a0,a1,v),mixf(b0,b1,v),w);
}
static float fbm3(F3 p,int oct){ float s=0,a=0.5f; for(int i=0;i<oct;i++){ s+=a*pn(p); p=add(mul(p,2.03f),V(17.1f,-9.3f,4.7f)); a*=0.5f; } return s; }
static float ridged(F3 p,int oct){ float s=0,a=0.5f,w=1; for(int i=0;i<oct;i++){ float r=1-fabsf(pn(p)); r*=r; s+=a*r*w; w=sat(r*1.6f); p=add(mul(p,2.11f),V(-3.3f,7.7f,1.9f)); a*=0.5f; } return s; }
static inline float pn2(float x,float y){ return pn(V(x,y,0.37f)); }
static float fbm2(float x,float y,int oct){ return fbm3(V(x,y,0.37f),oct); }
static inline uint32_t h3(int x,int y,int z){ uint32_t h=(uint32_t)x*374761393u+(uint32_t)y*668265263u+(uint32_t)z*2246822519u; h=(h^(h>>13))*1274126177u; return h^(h>>16); }
typedef struct { float f1,f2; F3 c1; uint32_t id; } Wor;
static Wor worley(F3 p,float jit){   /* cellules : f1, f2, centre et identifiant de la plus proche */
  int X=ffl(p.x),Y=ffl(p.y),Z=ffl(p.z); Wor w; w.f1=w.f2=9; w.c1=p; w.id=0;
  for(int dz=-1;dz<=1;dz++) for(int dy=-1;dy<=1;dy++) for(int dx=-1;dx<=1;dx++){
    uint32_t h=h3(X+dx,Y+dy,Z+dz);
    F3 c=V(X+dx+0.5f+jit*((h&1023)/1023.0f-0.5f),Y+dy+0.5f+jit*(((h>>10)&1023)/1023.0f-0.5f),Z+dz+0.5f+jit*(((h>>20)&1023)/1023.0f-0.5f));
    F3 d=sub(p,c); float q=dot(d,d);
    if(q<w.f1){ w.f2=w.f1; w.f1=q; w.c1=c; w.id=h; } else if(q<w.f2) w.f2=q;
  }
  w.f1=sqrtf(w.f1); w.f2=sqrtf(w.f2); return w;
}
static inline float rnd(uint32_t *s){ *s=*s*1664525u+1013904223u; return ((*s>>8)&0xFFFFFF)/16777216.0f; }
static inline float rr(uint32_t *s,float a,float b){ return a+(b-a)*rnd(s); }

/* champs de distance */
static inline float sd_sph(F3 p,F3 c,float r){ return len(sub(p,c))-r; }
static inline float sd_box(F3 p,F3 b,float r){ F3 q=V(fabsf(p.x)-b.x+r,fabsf(p.y)-b.y+r,fabsf(p.z)-b.z+r);
  return len(V(fmaxf(q.x,0),fmaxf(q.y,0),fmaxf(q.z,0)))+fminf(fmaxf(q.x,fmaxf(q.y,q.z)),0)-r; }
static inline float sd_cap(F3 p,F3 a,F3 b,float r){ F3 pa=sub(p,a), ba=sub(b,a); float h=sat(dot(pa,ba)/dot(ba,ba)); return len(sub(pa,mul(ba,h)))-r; }
static inline float sd_cap2(F3 p,F3 a,F3 b,float ra,float rb){ /* capsule conique */
  F3 pa=sub(p,a), ba=sub(b,a); float h=sat(dot(pa,ba)/dot(ba,ba)); return len(sub(pa,mul(ba,h)))-mixf(ra,rb,h); }
static inline float sd_tor(F3 p,float R,float r){ float q=sqrtf(p.x*p.x+p.z*p.z)-R; return sqrtf(q*q+p.y*p.y)-r; }   /* tore autour de y */
static inline float sd_cyl(F3 p,float r,float h,float rr_){ float dx=sqrtf(p.x*p.x+p.z*p.z)-r+rr_, dy=fabsf(p.y)-h+rr_;
  return fminf(fmaxf(dx,dy),0)+len(V(fmaxf(dx,0),fmaxf(dy,0),0))-rr_; }
static inline float smin(float a,float b,float k){ float h=sat(0.5f+0.5f*(b-a)/k); return mixf(b,a,h)-k*h*(1-h); }
static inline float smax(float a,float b,float k){ return -smin(-a,-b,k); }
static inline float sd_ell(F3 p,F3 r){ float k0=len(V(p.x/r.x,p.y/r.y,p.z/r.z)), k1=len(V(p.x/(r.x*r.x),p.y/(r.y*r.y),p.z/(r.z*r.z))); return k1>1e-6f?k0*(k0-1)/k1:-fminf(r.x,fminf(r.y,r.z)); }
/* cone : pointe en a, base en b de rayon r */
static inline float sd_cone(F3 p,F3 a,F3 b,float r){ return sd_cap2(p,b,a,r,0.002f); }

/* ======================================================================= */
/*                    scene : formes, materiaux, lumiere                    */
/* ======================================================================= */
typedef struct {
  F3 alb; float rough, metal, spec;
  F3 emit;
  float sss; F3 sssc;      /* lumiere qui traverse la matiere (cire, peau, glace) */
  float clear;             /* vernis mouille */
  float aoc;               /* creux peints (multiplie l'occlusion) */
  int glass; float ior; F3 absorb;   /* verre : refraction a travers une sphere */
  float disp;              /* dispersion (arc-en-ciel) */
  int tglass; F3 tint;     /* verre mince : on voit au travers (sablier, lanterne, fiole) */
} Mat;
static void mat_def(Mat *m){ memset(m,0,sizeof *m); m->alb=V(0.5f,0.5f,0.5f); m->rough=0.5f; m->spec=0.04f; m->aoc=1; m->ior=1.5f; m->tint=V(1,1,1); }

typedef struct IconDef IconDef;
struct IconDef {
  float (*sdf)(F3 p,int *m);
  void (*mat)(F3 p,F3 *n,int m,Mat *o);
  F3 (*vol)(F3 q,float *dens);              /* interieur d'une sphere de verre (q : unite) */
  void (*fx)(int layer,float x,float y,F3 *c,float *a);   /* effets 2D : 0 derriere, 1 devant */
  void (*setup)(void);
  float bound;    /* sphere englobante */
  float glow;     /* halo de l'emission */
  float shadow;   /* ombre portee */
  int shad;       /* ombres propres (formes en relief) */
};

/* lumiere du studio (monde : x a droite, y en haut, z vers l'oeil) */
static F3 LK, LF, LR, CK, CF, CR, SKY, GND, SB1, SB2, SB1C, SB2C;
static void lights_init(void){
  LK=nrm(V(-0.55f,0.72f,0.55f)); CK=mul(V(1.0f,0.93f,0.84f),2.2f);
  LF=nrm(V(0.85f,-0.10f,0.52f)); CF=mul(V(0.48f,0.58f,0.80f),0.45f);
  LR=nrm(V(0.45f,0.50f,-0.74f)); CR=mul(V(1.0f,0.90f,0.80f),1.5f);
  SKY=V(0.22f,0.23f,0.27f); GND=V(0.07f,0.06f,0.05f);
  SB1=nrm(V(-0.52f,0.70f,0.50f)); SB1C=V(6.0f,5.6f,5.1f);
  SB2=nrm(V(0.85f,0.25f,0.45f)); SB2C=V(1.0f,1.15f,1.4f);
}
static F3 env(F3 r,float rough){
  F3 top=V(0.40f,0.39f,0.42f), hor=V(0.12f,0.115f,0.13f), gnd=V(0.075f,0.066f,0.060f), c;
  if(r.y>=0) c=mix3(hor,top,powf(r.y,0.65f)); else { c=mix3(V(0.035f,0.032f,0.032f),gnd,sst(0.05f,0.6f,-r.y)); c=mix3(hor,c,sat(-r.y*6.0f)); }
  float w=0.006f+rough*rough*0.55f, k=1.0f/(1+rough*7);
  c=add(c,mul(SB1C,expf(-(1-dot(r,SB1))/w)*k));
  c=add(c,mul(SB2C,expf(-(1-dot(r,SB2))/(w*1.6f))*k));
  return c;
}

static const IconDef *D;          /* icone en cours */
static int CUR;
static uint32_t SEED;
static bool g_noglass=false;     /* vrai : les formes ignorent leurs vitres */
static inline float SDF(F3 p,int *m){ return D->sdf(p,m); }

static F3 calc_n(F3 p){
  const float e=0.0012f; int m;
  F3 a=V(1,-1,-1), b=V(-1,-1,1), c=V(-1,1,-1), d=V(1,1,1);
  return nrm(add(add(mul(a,SDF(add(p,mul(a,e)),&m)),mul(b,SDF(add(p,mul(b,e)),&m))),add(mul(c,SDF(add(p,mul(c,e)),&m)),mul(d,SDF(add(p,mul(d,e)),&m)))));
}
static float calc_ao(F3 p,F3 n){
  float o=0,w=1; int m;
  for(int i=1;i<=5;i++){ float h=0.012f+0.045f*i; o+=(h-SDF(add(p,mul(n,h)),&m))*w; w*=0.72f; }
  return sat(1-1.6f*o);
}
static float shadow(F3 p,F3 l){
  float r=1,t=0.01f; int m;
  for(int i=0;i<40 && t<2.2f;i++){ float h=SDF(add(p,mul(l,t)),&m); if(h<0.0004f) return 0; r=fminf(r,10*h/t); t+=fminf(fmaxf(h,0.006f),0.12f); }
  return sat(r);
}
/* relief : perturbe la normale selon un champ de hauteur */
static F3 bump(F3 p,F3 n,float (*h)(F3),float amp,float e){
  float h0=h(p);
  F3 g=V((h(add(p,V(e,0,0)))-h0)/e,(h(add(p,V(0,e,0)))-h0)/e,(h(add(p,V(0,0,e)))-h0)/e);
  g=sub(g,mul(n,dot(g,n)));
  return nrm(sub(n,mul(g,amp)));
}

static inline F3 fresnel(F3 f0,float c){ float k=powf(1-sat(c),5); return add(f0,mul(sub(V(1,1,1),f0),k)); }
static F3 light1(F3 n,F3 v,F3 l,F3 lc,const Mat *m,F3 dif,F3 f0,float sh){
  float nl=dot(n,l); if(nl<=0||sh<=0) return V(0,0,0);
  float nv=fmaxf(dot(n,v),1e-3f);
  F3 h=nrm(add(l,v)); float nh=fmaxf(dot(n,h),0), vh=fmaxf(dot(v,h),0);
  float a=fmaxf(m->rough*m->rough,0.002f), a2=a*a, dd=nh*nh*(a2-1)+1, Dg=a2/(PI*dd*dd);
  float k=(m->rough+1)*(m->rough+1)/8, G=(nl/(nl*(1-k)+k))*(nv/(nv*(1-k)+k));
  F3 F=fresnel(f0,vh);
  F3 s=mul(F,Dg*G/(4*nl*nv+1e-4f));
  F3 c=add(mul(dif,1.0f/PI),s);
  if(m->clear>0){ float a3=0.0036f, d3=nh*nh*(a3-1)+1; float D3=a3/(PI*d3*d3); float F3c=0.04f+0.96f*powf(1-vh,5); c=add(c,V(D3*F3c*m->clear*0.25f,D3*F3c*m->clear*0.25f,D3*F3c*m->clear*0.25f)); }
  return mul(mulv(c,lc),nl*sh*PI);
}
static F3 shade(F3 p,F3 n,F3 rd,const Mat *m,float ao,float sh){
  F3 v=mul(rd,-1); float nv=fmaxf(dot(n,v),1e-3f);
  F3 f0=mix3(V(m->spec,m->spec,m->spec),m->alb,m->metal);
  F3 dif=mul(m->alb,1-m->metal);
  F3 c=light1(n,v,LK,CK,m,dif,f0,sh);
  c=add(c,light1(n,v,LF,CF,m,dif,f0,1));
  c=add(c,light1(n,v,LR,CR,m,dif,f0,1));
  F3 amb=mix3(GND,SKY,0.5f+0.5f*n.y);
  c=add(c,mulv(dif,mul(amb,ao)));
  /* reflets de l'environnement */
  F3 r=refl(rd,n);
  F3 fr=add(f0,mul(sub(V(fmaxf(1-m->rough,f0.x),fmaxf(1-m->rough,f0.y),fmaxf(1-m->rough,f0.z)),f0),powf(1-nv,5)));
  c=add(c,mul(mulv(fr,env(r,m->rough)),ao*(0.35f+0.65f*sh*0.5f+0.325f)));
  if(m->clear>0){ float fc=(0.04f+0.96f*powf(1-nv,5))*m->clear; c=add(c,mul(env(r,0.04f),fc*ao)); }
  if(m->sss>0){
    float wrap=fmaxf(0,(dot(n,LK)+0.55f)/1.55f);
    float back=powf(fmaxf(0,dot(v,mul(LR,-1))),2.0f)+0.35f*powf(1-nv,2);
    c=add(c,mul(mulv(m->sssc,add(mul(CK,wrap*0.22f),mul(CR,back*0.35f))),m->sss));
  }
  return add(c,m->emit);
}

/* verre : la sphere refracte l'environnement (image renversee), interieur eventuel */
static F3 glass_sphere(F3 p,F3 n,F3 rd,F3 C,float R,const Mat *m,float ao){
  F3 v=mul(rd,-1); float nv=fmaxf(dot(n,v),1e-3f);
  float f0=powf((m->ior-1)/(m->ior+1),2), fr=f0+(1-f0)*powf(1-nv,5);
  F3 refc=env(refl(rd,n),m->rough*0.5f);
  F3 t1; if(!refr(rd,n,1.0f/m->ior,&t1)) t1=refl(rd,n);
  float s=-2*dot(sub(p,C),t1); if(s<0) s=0;
  F3 p2=add(p,mul(t1,s)), n2=nrm(sub(p2,C));
  F3 t2; if(!refr(t1,mul(n2,-1),m->ior,&t2)) t2=refl(t1,mul(n2,-1));
  F3 bg=env(t2,0.25f+m->rough);
  if(m->disp>0){   /* chaque couleur se refracte un peu differemment */
    float ch[3];
    for(int k=0;k<3;k++){ float io=m->ior+(k-1)*m->disp; F3 a1,a2; if(!refr(rd,n,1.0f/io,&a1)) a1=refl(rd,n);
      float s1=-2*dot(sub(p,C),a1); if(s1<0) s1=0; F3 q2=add(p,mul(a1,s1)), m2=nrm(sub(q2,C));
      if(!refr(a1,mul(m2,-1),io,&a2)){ a2=refl(a1,mul(m2,-1)); }
      F3 e=env(a2,0.15f); ch[k]=k==0?e.x:(k==1?e.y:e.z); }
    bg=V(ch[0],ch[1],ch[2]);
  }
  F3 tr=V(expf(-m->absorb.x*s),expf(-m->absorb.y*s),expf(-m->absorb.z*s));
  F3 inside=mulv(bg,tr);
  if(D->vol){   /* interieur : on avance dans la sphere en accumulant lumiere et brume */
    const int NS=20; float dt=s/NS, T=1; F3 acc=V(0,0,0);
    for(int i=0;i<NS;i++){
      F3 q=mul(sub(add(p,mul(t1,(i+0.5f)*dt)),C),1.0f/R); float dens=0;
      F3 e=D->vol(q,&dens);
      acc=add(acc,mul(e,T*dt/R));
      T*=expf(-dens*dt/R);
      if(T<0.004f) break;
    }
    inside=add(acc,mul(inside,T));
  }
  F3 c=add(mul(refc,fr),mul(inside,1-fr));
  /* reflet net de la lumiere principale */
  F3 h=nrm(add(LK,v)); float sp=powf(fmaxf(dot(n,h),0),900.0f)*30.0f;
  c=add(c,mul(CK,sp*fmaxf(dot(n,LK),0)));
  F3 h2=nrm(add(LR,v)); c=add(c,mul(CR,powf(fmaxf(dot(n,h2),0),200.0f)*6.0f));
  c=add(c,m->emit);
  return mul(c,0.75f+0.25f*ao);
}

/* ======================================================================= */
/*                               le rendu                                  */
/* ======================================================================= */
static float OBJ[N*N*4];    /* objet : couleur lineaire premultipliee + couverture */
static float EMI[N*N*3];    /* emission (pour le halo) */
static float TMP[N*N*4], TMP2[N*N*4];
static float BALL_C=-0.04f, BALL_R=0.70f;   /* bille standard */
static F3 BC;

static inline void px_to_world(float px,float py,float *x,float *y){ *x=px*(2.0f/FN)-1; *y=1-py*(2.0f/FN); }

static bool trace_from(F3 ro,F3 rd,float t,float t1,float *tt,int *mm){
  for(int i=0;i<160 && t<t1;i++){
    F3 p=add(ro,mul(rd,t)); int m=0; float d=SDF(p,&m);
    if(d<0.0007f){ *tt=t; *mm=m; return true; }
    t+=d*0.85f;
  }
  return false;
}
static bool trace(float x,float y,float *tt,int *mm){
  F3 ro=V(x,y,3.0f), rd=V(0,0,-1);
  F3 oc=sub(ro,BC); float b=dot(oc,rd), c=dot(oc,oc)-D->bound*D->bound, h=b*b-c;
  if(h<0) return false;
  h=sqrtf(h); float t=fmaxf(-b-h,0), t1=-b+h;
  for(int i=0;i<160 && t<t1;i++){
    F3 p=add(ro,mul(rd,t)); int m=0; float d=SDF(p,&m);
    if(d<0.0007f){ *tt=t; *mm=m; return true; }
    t+=d*0.85f;
  }
  return false;
}
static F3 shade_hit(F3 ro,F3 rd,float t,int m,F3 *em,Mat *mo){
  F3 p=add(ro,mul(rd,t)), n=calc_n(p);
  Mat mt; mat_def(&mt); F3 nn=n; D->mat(p,&nn,m,&mt);
  float ao=sat(calc_ao(p,n)*mt.aoc);
  float sh=1; if(D->shad && dot(nn,LK)>0) sh=shadow(add(p,mul(n,0.004f)),LK);
  F3 c;
  if(mt.glass) c=glass_sphere(p,nn,rd,BC,BALL_R,&mt,ao);
  else c=shade(p,nn,rd,&mt,ao,sh);
  *em=mt.emit; if(mo) *mo=mt;
  return c;
}
/* couleur premultipliee et opacite du point touche (le verre mince laisse voir derriere) */
static void shade_at(float x,float y,float t,int m,F3 *col,F3 *em,float *al){
  F3 ro=V(x,y,3.0f), rd=V(0,0,-1);
  Mat mt; F3 c=shade_hit(ro,rd,t,m,em,&mt);
  if(!mt.tglass){ *col=c; *al=1; return; }
  F3 p=add(ro,mul(rd,t)), n=calc_n(p);
  float nv=fmaxf(fabsf(n.z),1e-3f); float fr=0.05f+0.95f*powf(1-nv,5);
  F3 rc=mul(env(refl(rd,n),0.03f),fr);
  F3 h=nrm(add(LK,V(0,0,1))); rc=add(rc,mul(CK,powf(fmaxf(dot(n,h),0),600.0f)*14.0f));
  /* on continue a travers la vitre */
  g_noglass=true; float t2; int m2; F3 e2=V(0,0,0);
  bool hit=trace_from(ro,rd,t+0.004f,t+4.0f,&t2,&m2);
  F3 inner=V(0,0,0); float ia=0;
  if(hit){ inner=shade_hit(ro,rd,t2,m2,&e2,NULL); ia=1; }
  g_noglass=false;
  float haze=0.10f;
  *col=add(rc,mulv(mul(add(mul(inner,ia),mul(V(0.6f,0.7f,0.75f),haze*(1-ia))),1-fr),mt.tint));
  *al=sat(fr+(1-fr)*(ia+haze*(1-ia)));
  *em=add(*em,mul(e2,0.6f));
}

/* rendu de quelques lignes : 4 rayons par pixel pour les bords, un eclairage par materiau touche */
static const float SS[4][2]={{0.375f,0.125f},{0.875f,0.375f},{0.625f,0.875f},{0.125f,0.625f}};
static void render_rows(int y0,int y1){
  for(int py=y0;py<y1;py++) for(int px=0;px<N;px++){
    float ts[4]; int ms[4]; bool hs[4]; int nh=0;
    for(int s=0;s<4;s++){ float x,y; px_to_world(px+SS[s][0],py+SS[s][1],&x,&y); hs[s]=trace(x,y,&ts[s],&ms[s]); nh+=hs[s]; }
    float *o=&OBJ[(py*N+px)*4], *e=&EMI[(py*N+px)*3];
    if(!nh){ o[0]=o[1]=o[2]=o[3]=0; e[0]=e[1]=e[2]=0; continue; }
    F3 acc=V(0,0,0), ea=V(0,0,0); float aa=0; int done_m[4]; F3 dc[4], de[4]; float da[4]; int nd=0;
    for(int s=0;s<4;s++){ if(!hs[s]) continue;
      int k=-1; for(int j=0;j<nd;j++) if(done_m[j]==ms[s]) k=j;
      if(k<0){ float x,y; px_to_world(px+SS[s][0],py+SS[s][1],&x,&y); shade_at(x,y,ts[s],ms[s],&dc[nd],&de[nd],&da[nd]); done_m[nd]=ms[s]; k=nd++; }
      acc=add(acc,dc[k]); ea=add(ea,de[k]); aa+=da[k]; }
    o[0]=acc.x*0.25f; o[1]=acc.y*0.25f; o[2]=acc.z*0.25f; o[3]=aa*0.25f;
    e[0]=ea.x*0.25f; e[1]=ea.y*0.25f; e[2]=ea.z*0.25f;
  }
}

/* flou gaussien separable (canaux nc) */
static void blur(float *buf,int nc,float sigma){
  int r=(int)ceilf(sigma*2.5f); if(r<1) return; if(r>40) r=40;
  float w[41]; float ws=0; for(int i=0;i<=r;i++){ w[i]=expf(-(i*i)/(2*sigma*sigma)); ws+=i?2*w[i]:w[i]; }
  for(int i=0;i<=r;i++) w[i]/=ws;
  for(int y=0;y<N;y++) for(int x=0;x<N;x++) for(int c=0;c<nc;c++){
    float s=0; for(int i=-r;i<=r;i++){ int xx=x+i; if(xx<0||xx>=N) continue; s+=buf[(y*N+xx)*nc+c]*w[i<0?-i:i]; } TMP2[(y*N+x)*nc+c]=s; }
  for(int y=0;y<N;y++) for(int x=0;x<N;x++) for(int c=0;c<nc;c++){
    float s=0; for(int i=-r;i<=r;i++){ int yy=y+i; if(yy<0||yy>=N) continue; s+=TMP2[(yy*N+x)*nc+c]*w[i<0?-i:i]; } buf[(y*N+x)*nc+c]=s; }
}

static inline F3 aces(F3 x){
  #define AC(v) sat(((v)*(2.51f*(v)+0.03f))/((v)*(2.43f*(v)+0.59f)+0.14f))
  return V(AC(x.x),AC(x.y),AC(x.z));
  #undef AC
}

/* compose objet, halo, effets et ombre ; ecrit l'icone RGBA (sRGB premultiplie) */
static void finish(uint8_t *out,int stride){
  /* halo : emission floutee */
  float *G=TMP; memcpy(G,EMI,sizeof EMI);
  if(D->glow>0){ blur(G,3,5.0f); }
  /* ombre portee : couverture floutee et decalee */
  static float SHD[N*N];
  for(int i=0;i<N*N;i++) SHD[i]=OBJ[i*4+3];
  { float *tmpb=TMP2; (void)tmpb; }
  static float SB[N*N*4];
  for(int i=0;i<N*N;i++){ SB[i*4]=SHD[i]; SB[i*4+1]=SB[i*4+2]=SB[i*4+3]=0; }
  blur(SB,4,3.2f);
  for(int py=0;py<N;py++) for(int px=0;px<N;px++){
    int i=py*N+px; float x,y; px_to_world(px+0.5f,py+0.5f,&x,&y);
    F3 bc=V(0,0,0), fc=V(0,0,0); float ba=0, fa=0;
    if(D->fx){ D->fx(0,x,y,&bc,&ba); D->fx(1,x,y,&fc,&fa); }
    F3 oc=V(OBJ[i*4],OBJ[i*4+1],OBJ[i*4+2]); float oa=OBJ[i*4+3];
    /* halo additif autour de l'emission */
    if(D->glow>0){ F3 g=V(G[i*3],G[i*3+1],G[i*3+2]); oc=add(oc,mul(g,D->glow)); }
    /* ombre portee (decalee de 2 px vers le bas) */
    int sy=py-3, sx=px-1; float sa=0; if(sy>=0&&sx>=0) sa=SB[(sy*N+sx)*4]*D->shadow;
    /* empilement : ombre < derriere < objet < devant */
    F3 c=V(0,0,0); float a=sa;
    c=add(bc,mul(c,1-ba)); a=ba+a*(1-ba);
    c=add(oc,mul(c,1-oa)); a=oa+a*(1-oa);
    c=add(fc,mul(c,1-fa)); a=fa+a*(1-fa);
    /* courbe filmique puis sRGB (on garde rgb <= alpha) */
    c=aces(mul(c,1.05f));
    float m=maxc(c); if(m>a) a=fminf(1,m);
    uint8_t *o=&out[py*stride+px*4];
    if(a<=0.0005f){ o[0]=o[1]=o[2]=o[3]=0; continue; }
    F3 s=mul(c,1.0f/a);
    s=V(powf(sat(s.x),1/2.2f),powf(sat(s.y),1/2.2f),powf(sat(s.z),1/2.2f));
    o[0]=(uint8_t)(s.x*a*255+0.5f); o[1]=(uint8_t)(s.y*a*255+0.5f); o[2]=(uint8_t)(s.z*a*255+0.5f); o[3]=(uint8_t)(a*255+0.5f);
  }
}

/* ======================================================================= */
/*                           aides des billes                              */
/* ======================================================================= */
static float sdf_ball(F3 p,int *m){ *m=0; return len(sub(p,BC))-BALL_R; }
static inline F3 bq(F3 p){ return mul(sub(p,BC),1.0f/BALL_R); }   /* point sur la sphere unite */
/* effets : composer une couche premultipliee */
static inline void over(F3 *c,float *a,F3 col,float al){ al=sat(al); *c=add(mul(col,al),mul(*c,1-al)); *a=al+*a*(1-al); }
static inline void addl(F3 *c,F3 col){ *c=add(*c,col); }
static inline float dball(float x,float y){ return sqrtf(x*x+(y-BALL_C)*(y-BALL_C))-BALL_R; }

/* etincelles / particules precalculees */
typedef struct { float x,y,r,a; F3 c; } Spk;
static Spk SPK[160]; static int NSPK=0;
static void spk_add(float x,float y,float r,F3 c,float a){ if(NSPK<160){ Spk *s=&SPK[NSPK++]; s->x=x; s->y=y; s->r=r; s->c=c; s->a=a; } }
static void spk_eval(float x,float y,F3 *c,float *a){
  for(int i=0;i<NSPK;i++){ Spk *s=&SPK[i]; float dx=x-s->x, dy=y-s->y, d2=dx*dx+dy*dy; if(d2>s->r*s->r*9) continue;
    float g=expf(-d2/(s->r*s->r)); addl(c,mul(s->c,g*s->a)); *a=fmaxf(*a,sat(g*s->a*0.6f)); }
}
/* etoile scintillante (croix lumineuse) */
static float star4(float x,float y,float cx,float cy,float s){
  float dx=fabsf(x-cx), dy=fabsf(y-cy);
  return expf(-(dx*dx+dy*dy)/(s*s*0.08f))+0.6f*expf(-dx/(s*0.05f))*expf(-dy/(s*0.9f))+0.6f*expf(-dy/(s*0.05f))*expf(-dx/(s*0.9f));
}
/* arcs electriques : polylignes brisees */
typedef struct { float x0,y0,x1,y1,w; } Seg;
static Seg ARC[400]; static int NARC=0;
static void arc_build(float x0,float y0,float x1,float y1,float disp,int depth,float w,uint32_t *s){
  if(depth==0||NARC>=399){ if(NARC<400){ Seg *g=&ARC[NARC++]; g->x0=x0; g->y0=y0; g->x1=x1; g->y1=y1; g->w=w; } return; }
  float mx=(x0+x1)*0.5f, my=(y0+y1)*0.5f, dx=x1-x0, dy=y1-y0;
  float o=(rnd(s)-0.5f)*disp; mx+=-dy*o; my+=dx*o;
  arc_build(x0,y0,mx,my,disp*0.62f,depth-1,w,s); arc_build(mx,my,x1,y1,disp*0.62f,depth-1,w,s);
  if(depth>=3 && rnd(s)<0.28f){ float bx=mx+(mx-x0)*0.8f+(rnd(s)-0.5f)*0.25f, by=my+(my-y0)*0.8f+(rnd(s)-0.5f)*0.25f; arc_build(mx,my,bx,by,disp*0.5f,depth-2,w*0.6f,s); }
}
static float arc_dist(float x,float y,float *w){
  float best=9; *w=1;
  for(int i=0;i<NARC;i++){ Seg *g=&ARC[i]; float px=x-g->x0, py=y-g->y0, bx=g->x1-g->x0, by=g->y1-g->y0;
    float h=sat((px*bx+py*by)/(bx*bx+by*by+1e-9f)); float dx=px-bx*h, dy=py-by*h; float d=sqrtf(dx*dx+dy*dy)/g->w; if(d<best){ best=d; *w=g->w; } }
  return best;
}
/* ======================================================================= */
/*                                LES BILLES                               */
/* ======================================================================= */
/* --- fer : acier poli, a peine martele --- */
static float h_hammer(F3 p){ Wor w=worley(mul(bq(p),3.0f),0.9f); return w.f1*w.f1; }
static void mat_fer(F3 p,F3 *n,int m,Mat *o){ (void)m;
  F3 q=bq(p);
  *n=bump(p,*n,h_hammer,0.006f,0.004f);
  float sc=fbm3(V(q.x*6,q.y*45,q.z*6),3);
  o->metal=1; o->alb=V(0.58f,0.59f,0.62f); o->rough=0.10f+0.16f*sat(sc*0.6f+0.5f);
  float pat=fbm3(mul(q,2.0f),4); o->alb=mix3(o->alb,V(0.40f,0.38f,0.36f),sat(pat*0.9f+0.1f)*0.7f);
}
/* --- plomb : gris sombre, mat, quelques bosses --- */
static float h_dent(F3 p){ Wor w=worley(mul(bq(p),2.0f),0.9f); return -expf(-w.f1*w.f1*30)*0.9f+fbm3(mul(bq(p),12),3)*0.05f; }
static void mat_plomb(F3 p,F3 *n,int m,Mat *o){ (void)m;
  F3 q=bq(p); *n=bump(p,*n,h_dent,0.07f,0.004f);
  float g=fbm3(mul(q,3),4);
  o->metal=0.65f; o->alb=mix3(V(0.25f,0.26f,0.29f),V(0.15f,0.16f,0.18f),sat(g+0.5f)); o->rough=0.60f+0.12f*g;
}
/* --- feu : lave en fusion sous une croute qui se fend, entouree de flammes --- */
static F3 fire_ramp(float t){   /* 0 : rouge sombre, 1 : jaune presque blanc */
  t=sat(t);
  if(t<0.35f) return mix3(V(0.18f,0.012f,0.004f),V(0.95f,0.16f,0.02f),t/0.35f);
  if(t<0.72f) return mix3(V(0.95f,0.16f,0.02f),V(1.55f,0.62f,0.08f),(t-0.35f)/0.37f);
  return mix3(V(1.55f,0.62f,0.08f),V(1.9f,1.45f,0.55f),(t-0.72f)/0.28f);
}
/* flammes : langues de feu qui montent depuis la surface (intensite 0..1) */
static float flames(float x,float y,float cx,float cy,float R,float H,float seed){
  float dx=x-cx, dy=y-cy, d=sqrtf(dx*dx+dy*dy), e=d-R;
  float up=dy/(d+1e-4f);
  float hgt=0.05f+H*powf(sat(0.5f+0.5f*up),1.5f);
  float n=fbm2(x*3.2f+seed,y*1.5f-seed*0.37f,5)*0.5f+0.5f;
  float n2=fbm2(x*7.5f-seed,y*3.2f+seed,3)*0.5f+0.5f;
  float f=1-e/hgt;
  f=f*1.3f-(n*0.8f+n2*0.2f)*1.05f+0.12f;
  return sat(f*1.7f);
}
static float h_lava(F3 p){ Wor w=worley(mul(bq(p),3.0f),0.85f); return sst(0.0f,0.18f,w.f2-w.f1)*0.5f+fbm3(mul(bq(p),9),2)*0.04f; }
static float fire_turb(F3 q){
  F3 w=V(fbm3(mul(q,1.8f),3),fbm3(add(mul(q,1.8f),V(5,2,7)),3),fbm3(add(mul(q,1.8f),V(1,8,3)),3));
  return fbm3(add(mul(q,2.8f),mul(w,1.7f)),5)*0.5f+0.5f;
}
static void mat_feu(F3 p,F3 *n,int m,Mat *o){ (void)m;
  F3 q=bq(p);
  float t=fire_turb(q);
  float rim=1-sat(n->z);
  float heat=sat(t*1.45f-0.18f-rim*0.45f);
  o->alb=V(0.08f,0.02f,0.01f); o->rough=0.6f;
  o->emit=mul(fire_ramp(0.12f+0.88f*heat),0.75f+0.55f*heat);
}
static void fx_feu(int L,float x,float y,F3 *c,float *a){
  float f;
  if(L==0) f=flames(x,y,0,BALL_C,BALL_R,0.50f,0.0f);
  else {   /* devant : langues de feu sur le bord superieur */
    float d=dball(x,y); if(d<-0.16f||d>0.02f||y<BALL_C){ spk_eval(x,y,c,a); return; }
    f=flames(x,y+0.05f,0,BALL_C,BALL_R-0.10f,0.30f,5.0f)*sat((d+0.16f)/0.10f)*0.8f;
  }
  if(f>0.002f){
    F3 col=fire_ramp(0.15f+0.85f*f);
    float al=sat(f*1.6f)*0.95f;
    over(c,a,mul(col,1.0f/fmaxf(maxc(col),1.0f)),al);
    addl(c,mul(col,f*f*0.45f));
  }
  if(L==1) spk_eval(x,y,c,a);
}
static void setup_feu(void){
  BALL_C=-0.13f;
  uint32_t s=77; NSPK=0;
  for(int i=0;i<12;i++){ float ang=rr(&s,-1.1f,1.1f), r=rr(&s,0.80f,1.05f); spk_add(sinf(ang)*r*0.95f,BALL_C+cosf(ang)*r*1.08f,rr(&s,0.008f,0.016f),V(1.9f,0.9f,0.2f),rr(&s,0.5f,1.0f)); }
}

/* --- givre : boule de glace translucide herissee de pics --- */
static F3 SPIKE[12]; static float SPIKEL[12]; static int NSPIKE=0;
static void setup_glace(void){
  uint32_t s=4242; NSPIKE=0; NSPK=0;
  const float A[9]={-2.3f,-1.55f,-0.75f,0.05f,0.85f,1.6f,2.35f,3.0f,-3.0f};
  for(int i=0;i<9;i++){ float a=A[i]+rr(&s,-0.15f,0.15f); float zz=rr(&s,-0.15f,0.55f);
    F3 d=nrm(V(sinf(a),cosf(a),zz)); SPIKE[NSPIKE]=d; SPIKEL[NSPIKE]=rr(&s,0.20f,0.33f)*(fabsf(a)>2.4f?0.7f:1.0f); NSPIKE++; }
  for(int i=0;i<8;i++){ float ang=rr(&s,0,TAU), r=rr(&s,0.55f,0.95f); spk_add(cosf(ang)*r,BALL_C+sinf(ang)*r,rr(&s,0.007f,0.013f),V(1.2f,1.6f,2.0f),rr(&s,0.3f,0.8f)); }
}
static float sdf_glace(F3 p,int *m){
  float d=len(sub(p,BC))-BALL_R; *m=0;
  F3 dp=nrm(sub(p,BC));
  for(int i=0;i<NSPIKE;i++){ if(dot(dp,SPIKE[i])<0.80f) continue;
    F3 a=add(BC,mul(SPIKE[i],BALL_R*0.80f)), b=add(BC,mul(SPIKE[i],BALL_R+SPIKEL[i]));
    float ds=sd_cap2(p,a,b,0.10f,0.006f); if(ds<d){ d=smin(d,ds,0.03f); if(ds<d+0.02f) *m=1; } }
  return d;
}
static F3 vol_glace(F3 q,float *dens){
  Wor w=worley(add(mul(q,1.7f),V(1.3f,0.2f,2.1f)),0.9f);
  float g=w.f2-w.f1, crack=expf(-g*g*2200.0f);
  float mist=sat(fbm3(mul(q,3.0f),3)+0.2f);
  *dens=0.35f+crack*5.0f+mist*0.8f;
  return add(mul(V(0.70f,0.90f,1.0f),crack*1.3f+mist*0.10f),V(0.010f,0.035f,0.06f));
}
static void mat_glace(F3 p,F3 *n,int m,Mat *o){
  F3 q=bq(p);
  if(m==1){ o->alb=V(0.50f,0.78f,0.95f); o->rough=0.10f; o->sss=0.9f; o->sssc=V(0.40f,0.75f,1.0f); o->spec=0.03f; o->emit=V(0.015f,0.045f,0.08f); return; }
  float frost=sst(0.25f,0.65f,fbm3(mul(q,3.2f),5)+0.35f*q.y);
  o->glass=1; o->ior=1.31f; o->absorb=V(2.6f,0.75f,0.32f); o->rough=0.04f+frost*0.4f;
  o->emit=mul(V(0.42f,0.55f,0.62f),frost*0.45f);
}
static void fx_glace(int L,float x,float y,F3 *c,float *a){
  if(L==0){   /* brume froide derriere */
    float d=dball(x,y); float mm=sat(1-d/0.20f)*sat(d/0.02f+1);
    float n=0.5f+0.5f*fbm2(x*4,y*4+3,4); float al=mm*n*0.28f; if(al>0) over(c,a,V(0.55f,0.80f,1.0f),al);
    return;
  }
  spk_eval(x,y,c,a);
  float st=star4(x,y,-0.30f,BALL_C+0.36f,0.14f)*0.8f; if(st>0.01f){ addl(c,mul(V(1.2f,1.5f,1.8f),st)); *a=fmaxf(*a,sat(st)); }
}

/* --- foudre : orbe de plasma dore dans du verre sombre, eclairs autour --- */
static void setup_foudre(void){
  uint32_t s=999; NARC=0; NSPK=0;
  const float A[6]={-2.2f,-0.9f,0.25f,1.25f,2.25f,2.95f};
  for(int i=0;i<6;i++){ float a=A[i]+rr(&s,-0.2f,0.2f); float r0=BALL_R*0.97f, r1=BALL_R+rr(&s,0.16f,0.27f);
    float b=a+rr(&s,-0.5f,0.5f);
    arc_build(sinf(a)*r0,BALL_C+cosf(a)*r0,sinf(b)*r1,BALL_C+cosf(b)*r1,0.55f,5,rr(&s,0.012f,0.017f),&s); }
}
static F3 vol_foudre(F3 q,float *dens){
  float r=len(q);
  F3 w=V(fbm3(mul(q,1.8f),3),fbm3(add(mul(q,1.8f),V(5,1,3)),3),fbm3(add(mul(q,1.8f),V(1,7,2)),3));
  F3 qq=add(mul(q,1.6f),mul(w,0.9f));
  float fil=1-fabsf(pn(qq)); fil=powf(fil,22.0f);
  float fil2=1-fabsf(pn(add(mul(qq,2.1f),V(3,3,3)))); fil2=powf(fil2,26.0f)*0.6f;
  float core=expf(-r*r*16.0f);
  *dens=1.2f;
  return add(add(mul(V(1.9f,1.25f,0.30f),(fil+fil2)*3.2f*(0.35f+0.65f*(1-r))),mul(V(2.2f,1.9f,1.2f),core*2.2f)),V(0.06f,0.035f,0.008f));
}
static void mat_foudre(F3 p,F3 *n,int m,Mat *o){ (void)p; (void)n; (void)m;
  o->glass=1; o->ior=1.45f; o->absorb=V(1.6f,1.8f,2.6f); o->rough=0.03f;
}
static void fx_foudre(int L,float x,float y,F3 *c,float *a){
  if(L==0){ float d=dball(x,y); float g=expf(-fmaxf(d,0)/0.06f)*0.30f; addl(c,mul(V(1.2f,0.8f,0.15f),g)); *a=fmaxf(*a,g*0.4f); return; }
  float w; float d=arc_dist(x,y,&w);
  float core=expf(-d*d*1.2f), halo=expf(-d*0.75f)*0.35f;
  if(core+halo>0.003f){ addl(c,add(mul(V(1.9f,1.75f,1.3f),core),mul(V(1.1f,0.7f,0.12f),halo))); *a=fmaxf(*a,sat(core+halo*0.5f)); }
}

/* --- venin : liquide toxique luisant, bulles et gouttes --- */
static float sdf_venin(F3 p,int *m){
  *m=0; float d=len(sub(p,BC))-BALL_R;
  float n=fbm3(mul(bq(p),3.0f),3)*0.030f; d+=n;
  static const float B[5][4]={{-0.42f,0.42f,0.32f,0.10f},{0.30f,0.55f,0.25f,0.075f},{0.55f,0.05f,0.38f,0.085f},{-0.15f,-0.18f,0.68f,0.06f},{0.10f,0.30f,0.64f,0.05f}};
  for(int i=0;i<5;i++){ float db=sd_sph(p,V(B[i][0],B[i][1]+BALL_C,B[i][2]),B[i][3]); d=smin(d,db,0.05f); }
  float g1=sd_cap2(p,V(0.10f,BALL_C-BALL_R+0.06f,0.25f),V(0.12f,BALL_C-BALL_R-0.17f,0.30f),0.09f,0.045f);
  float g2=sd_cap2(p,V(-0.32f,BALL_C-BALL_R+0.16f,0.20f),V(-0.34f,BALL_C-BALL_R-0.04f,0.24f),0.07f,0.035f);
  d=smin(d,g1,0.10f); d=smin(d,g2,0.08f);
  float g3=sd_sph(p,V(0.125f,BALL_C-BALL_R-0.27f,0.30f),0.055f); if(g3<d){ d=g3; *m=1; }
  return d;
}
static void mat_venin(F3 p,F3 *n,int m,Mat *o){ (void)n; (void)m;
  F3 q=bq(p); float sw=fbm3(add(mul(q,2.3f),V(2,2,2)),5);
  o->alb=mix3(V(0.002f,0.016f,0.002f),V(0.014f,0.075f,0.005f),sat(0.5f+sw*1.2f)); o->rough=0.10f; o->clear=1.0f; o->spec=0.05f;
  o->sss=0.22f; o->sssc=V(0.15f,0.80f,0.04f);
  o->emit=mul(V(0.012f,0.060f,0.003f),sat(0.45f+sw));
}
static void fx_venin(int L,float x,float y,F3 *c,float *a){
  if(L==1) return;
  float d=dball(x,y); if(d<-0.05f) return;
  float n=fbm2(x*3.2f,y*3.2f-1.3f,5);
  float up=sat((y-BALL_C)/BALL_R*0.5f+0.6f);
  float al=sat(1-d/0.26f)*sat(0.40f+n)*0.45f*up;
  if(al>0) over(c,a,V(0.20f,0.62f,0.08f),al);
}

static F3 fib(int i,int n){ float y=1-2*(i+0.5f)/n, r=sqrtf(fmaxf(0,1-y*y)), a=i*2.39996323f; return V(cosf(a)*r,y,sinf(a)*r); }
static inline float sd_octa(F3 p,float s){ return (fabsf(p.x)+fabsf(p.y)+fabsf(p.z)-s)*0.57735f; }
static F3 palette(float t,F3 a,F3 b,F3 c,F3 d){ return V(a.x+b.x*cosf(TAU*(c.x*t+d.x)),a.y+b.y*cosf(TAU*(c.y*t+d.y)),a.z+b.z*cosf(TAU*(c.z*t+d.z))); }
static inline F3 lin(F3 c){ return V(powf(sat(c.x),2.2f),powf(sat(c.y),2.2f),powf(sat(c.z),2.2f)); }

/* --- petite bille : bille de verre a oeil de chat --- */
static void setup_baby(void){ BALL_C=-0.06f; BALL_R=0.50f; }
static F3 vol_baby(F3 q,float *dens){
  float r=len(q); float a=q.y*2.2f+0.4f;
  float best=9; int kb=0; for(int k=0;k<3;k++){ float ang=a+k*(PI/3); float d=fabsf(q.x*cosf(ang)-q.z*sinf(ang)); if(d<best){ best=d; kb=k; } }
  float vane=expf(-best*best*900.0f)*sst(0.90f,0.55f,r);
  *dens=0.04f+vane*10.0f;
  static const float VC[3][3]={{0.55f,0.30f,1.0f},{0.30f,0.65f,1.0f},{1.0f,0.85f,1.0f}};
  F3 c=V(VC[kb][0],VC[kb][1],VC[kb][2]);
  return mul(c,vane*3.0f);
}
static void mat_baby(F3 p,F3 *n,int m,Mat *o){ (void)p; (void)n; (void)m; o->glass=1; o->ior=1.5f; o->absorb=V(0.10f,0.09f,0.05f); o->rough=0.02f; }

/* --- epines : boule de ronces herissee d'aiguilles --- */
static F3 TH[32]; static float THL[32]; static int NTH=0;
static void setup_epine(void){ uint32_t s=31; NTH=26;
  for(int i=0;i<NTH;i++){ TH[i]=nrm(rotz(rotx(fib(i,NTH),0.5f),0.3f)); THL[i]=rr(&s,0.20f,0.30f); } }
static float sdf_epine(F3 p,int *m){
  F3 v=sub(p,BC); float d0=len(v)-BALL_R*0.92f; float d=d0; *m=0;
  F3 dp=nrm(v);
  for(int i=0;i<NTH;i++){ if(dot(dp,TH[i])<0.86f) continue;
    F3 a=add(BC,mul(TH[i],BALL_R*0.80f)), b=add(BC,mul(TH[i],BALL_R*0.92f+THL[i]));
    float ds=sd_cap2(p,a,b,0.085f,0.003f); if(ds<d0) *m=1; d=smin(d,ds,0.035f); }
  return d;
}
static float h_bark(F3 p){ return ridged(mul(bq(p),4.0f),3)*0.5f; }
static void mat_epine(F3 p,F3 *n,int m,Mat *o){
  F3 q=bq(p);
  if(m==0){ *n=bump(p,*n,h_bark,0.25f,0.004f);
    float v=ridged(mul(q,4.0f),3);
    o->alb=mix3(V(0.10f,0.008f,0.015f),V(0.42f,0.03f,0.05f),sat(v*1.2f)); o->rough=0.45f; o->sss=0.35f; o->sssc=V(0.8f,0.05f,0.08f); o->clear=0.3f; }
  else { float t=sat((len(sub(p,BC))-BALL_R*0.85f)/0.28f);
    o->alb=mix3(V(0.36f,0.03f,0.05f),V(0.03f,0.008f,0.008f),t); o->rough=0.28f; o->clear=0.6f; o->sss=0.25f*(1-t); o->sssc=V(0.8f,0.08f,0.08f); }
}

/* --- bombe : fonte noire, collerette de laiton, meche allumee --- */
static const float FUSE[4][3]={{0.0f,0.0f,0.0f},{0.07f,0.10f,0.03f},{0.17f,0.16f,0.05f},{0.27f,0.15f,0.05f}};
static float fuse_y0;
static void setup_bombe(void){ BALL_C=-0.10f; BALL_R=0.60f; fuse_y0=BALL_C+BALL_R+0.04f;
  uint32_t s=55; NSPK=0; F3 t=V(FUSE[3][0]+0.02f,fuse_y0+FUSE[3][1]+0.01f,0);
  for(int i=0;i<10;i++){ float a=rr(&s,0,TAU), r=rr(&s,0.03f,0.14f); spk_add(t.x+cosf(a)*r,t.y+sinf(a)*r*0.9f+0.03f,rr(&s,0.006f,0.012f),V(2.0f,1.2f,0.35f),rr(&s,0.5f,1.0f)); } }
static float sdf_bombe(F3 p,int *m){
  float d=len(sub(p,BC))-BALL_R; *m=0;
  float nk=sd_cyl(sub(p,V(0,BALL_C+BALL_R*0.94f,0)),0.17f,0.075f,0.02f); if(nk<d){ d=smin(d,nk,0.02f); *m=1; }
  float rim=sd_tor(sub(p,V(0,BALL_C+BALL_R*0.94f+0.07f,0)),0.155f,0.022f); if(rim<d){ d=rim; *m=1; }
  float fu=9; for(int i=0;i<3;i++){ F3 a=V(FUSE[i][0],fuse_y0+FUSE[i][1],FUSE[i][2]), b=V(FUSE[i+1][0],fuse_y0+FUSE[i+1][1],FUSE[i+1][2]); fu=fminf(fu,sd_cap(p,a,b,0.030f)); }
  if(fu<d){ d=fu; *m=2; }
  return d;
}
static void mat_bombe(F3 p,F3 *n,int m,Mat *o){ (void)n;
  if(m==0){ float g=fbm3(mul(bq(p),5),4); o->metal=0.55f; o->alb=V(0.045f+0.02f*g,0.045f+0.02f*g,0.05f+0.02f*g); o->rough=0.32f+0.12f*g; }
  else if(m==1){ o->metal=1; o->alb=V(0.78f,0.56f,0.26f); o->rough=0.28f; }
  else { float tw=0.5f+0.5f*sinf((p.x+p.y)*90.0f); o->alb=mix3(V(0.30f,0.21f,0.12f),V(0.55f,0.42f,0.25f),tw); o->rough=0.85f;
    float tip=sat((p.x-0.18f)/0.09f); o->emit=mul(V(1.6f,0.5f,0.08f),tip*tip*1.2f); }
}
static void fx_bombe(int L,float x,float y,F3 *c,float *a){
  if(L==0) return;
  float tx=FUSE[3][0]+0.03f, ty=fuse_y0+FUSE[3][1]+0.01f;
  float st=star4(x,y,tx,ty,0.20f); float g=expf(-((x-tx)*(x-tx)+(y-ty)*(y-ty))/0.004f);
  if(st+g>0.005f){ addl(c,add(mul(V(2.2f,1.5f,0.6f),st*0.9f),mul(V(1.8f,0.7f,0.1f),g*0.8f))); *a=fmaxf(*a,sat(st*0.8f+g*0.5f)); }
  spk_eval(x,y,c,a);
}

/* --- spectre : orbe translucide ou flotte un visage de brume --- */
static void setup_spectre(void){ }
static F3 vol_spectre(F3 q,float *dens){
  F3 w=V(fbm3(mul(q,1.6f),3),fbm3(add(mul(q,1.6f),V(4,2,1)),3),0);
  float mist=sat(fbm3(add(mul(q,2.0f),mul(w,1.2f)),4)*1.3f+0.35f)*sst(1.0f,0.55f,len(q));
  float ex=(fabsf(q.x)-0.27f)/0.15f, ey=(q.y-0.10f)/0.21f; float eye=sqrtf(ex*ex+ey*ey);
  float mx=q.x/0.17f, my=(q.y+0.33f)/0.13f; float mo=sqrtf(mx*mx+my*my);
  float face=sst(0.75f,1.15f,fminf(eye,mo));
  float front=sat(q.z*1.5f+0.6f);
  float k=mist*mixf(1,face,front);
  *dens=0.25f+k*2.2f;
  return mul(mix3(V(0.35f,0.18f,0.85f),V(0.85f,0.75f,1.0f),k*k),k*1.15f);
}
static void mat_spectre(F3 p,F3 *n,int m,Mat *o){ (void)p; (void)m;
  o->glass=1; o->ior=1.12f; o->absorb=V(0.9f,1.4f,0.35f); o->rough=0.06f;
  float rim=powf(1-sat(n->z),3); o->emit=mul(V(0.45f,0.30f,1.0f),rim*0.9f);
}
static void fx_spectre(int L,float x,float y,F3 *c,float *a){
  if(L==1){ spk_eval(x,y,c,a); return; }
  float d=dball(x,y); if(d<-0.03f) return;
  /* volutes qui s'echappent vers le bas a gauche */
  float n=fbm2(x*3.0f+1.7f,y*3.0f+0.4f,5);
  float tail=sat(1-d/(0.10f+0.25f*sat(-(y-BALL_C)/BALL_R+0.2f)));
  float al=sat(tail*(0.55f+n*1.2f))*0.6f;
  if(al>0.003f) over(c,a,V(0.45f,0.30f,0.95f),al);
  float g=expf(-fmaxf(d,0)/0.05f)*0.25f; addl(c,mul(V(0.5f,0.35f,1.0f),g));
}

/* --- vampire : sang epais et luisant qui goutte --- */
static float sdf_sang(F3 p,int *m){
  *m=0; float d=len(sub(p,BC))-BALL_R;
  d+=fbm3(mul(bq(p),2.5f),3)*0.012f;
  float g1=sd_cap2(p,V(-0.05f,BALL_C-BALL_R+0.08f,0.28f),V(-0.06f,BALL_C-BALL_R-0.20f,0.32f),0.10f,0.045f);
  float g2=sd_cap2(p,V(0.36f,BALL_C-BALL_R+0.22f,0.22f),V(0.38f,BALL_C-BALL_R+0.02f,0.26f),0.07f,0.035f);
  float g3=sd_cap2(p,V(-0.42f,BALL_C-BALL_R+0.25f,0.18f),V(-0.44f,BALL_C-BALL_R+0.10f,0.22f),0.06f,0.03f);
  d=smin(d,g1,0.11f); d=smin(d,g2,0.08f); d=smin(d,g3,0.07f);
  float dr=sd_sph(p,V(-0.065f,BALL_C-BALL_R-0.31f,0.32f),0.05f); if(dr<d){ d=dr; *m=1; }
  return d;
}
static void mat_sang(F3 p,F3 *n,int m,Mat *o){ (void)n; (void)m;
  F3 q=bq(p); float v=ridged(add(mul(q,3.0f),V(2,9,1)),4);
  o->alb=mix3(V(0.075f,0.002f,0.006f),V(0.018f,0.0005f,0.002f),sat(v*1.1f-0.2f)); o->rough=0.06f; o->clear=1.0f; o->spec=0.05f;
  o->sss=0.28f; o->sssc=V(1.0f,0.03f,0.05f);
}

/* --- eclats : cristal rose taille en facettes, eclats qui s'en detachent --- */
static F3 FAC[40]; static int NFAC=0;
static F3 SHP[6]; static float SHS[6]; static float SHR[6];
static void setup_eclat(void){ uint32_t s=91; NFAC=34; for(int i=0;i<NFAC;i++) FAC[i]=nrm(rotx(fib(i,NFAC),0.4f));
  const float P[5][3]={{-0.70f,0.52f,0.1f},{0.74f,0.40f,0.0f},{0.68f,-0.58f,0.2f},{-0.72f,-0.42f,0.1f},{0.05f,0.84f,0.0f}};
  for(int i=0;i<5;i++){ SHP[i]=V(P[i][0],P[i][1]+BALL_C*0.5f,P[i][2]); SHS[i]=rr(&s,0.06f,0.10f); SHR[i]=rr(&s,0,TAU); }
  NSPK=0; for(int i=0;i<8;i++){ float a=rr(&s,0,TAU), r=rr(&s,0.80f,0.98f); spk_add(cosf(a)*r,BALL_C+sinf(a)*r,rr(&s,0.006f,0.011f),V(1.8f,1.2f,1.7f),rr(&s,0.4f,0.9f)); } }
static float sdf_eclat(F3 p,int *m){
  F3 v=sub(p,BC); float d=-9; for(int i=0;i<NFAC;i++){ float k=dot(v,FAC[i])-BALL_R*0.97f; if(k>d) d=k; }
  *m=0;
  for(int i=0;i<5;i++){ F3 q=rotz(roty(sub(p,SHP[i]),SHR[i]),SHR[i]*0.7f); q.y*=0.6f; float ds=sd_octa(q,SHS[i]); if(ds<d){ d=ds; *m=1; } }
  return d;
}
static F3 vol_eclat(F3 q,float *dens){
  Wor w=worley(mul(q,1.3f),0.9f); float g=w.f2-w.f1; float fr=expf(-g*g*2600.0f)*0.6f;
  *dens=0.3f+fr*4.0f; return add(mul(V(1.0f,0.80f,0.95f),fr*1.4f),V(0.06f,0.02f,0.05f));
}
static void mat_eclat(F3 p,F3 *n,int m,Mat *o){ (void)p; (void)n;
  if(m==0){ o->glass=1; o->ior=1.6f; o->absorb=V(0.20f,1.25f,0.45f); o->rough=0.02f; }
  else { o->alb=V(0.95f,0.50f,0.82f); o->rough=0.06f; o->spec=0.08f; o->sss=0.7f; o->sssc=V(1.0f,0.55f,0.9f); o->emit=V(0.10f,0.04f,0.09f); }
}
static void fx_eclat(int L,float x,float y,F3 *c,float *a){ if(L==1) spk_eval(x,y,c,a); }

/* --- aimantee : moitie bleue moitie rouge, lignes de champ --- */
static void mat_aimant(F3 p,F3 *n,int m,Mat *o){ (void)n; (void)m;
  F3 q=bq(p); float g=fbm3(mul(q,6),3)*0.04f;
  if(fabsf(q.y)<0.07f){ o->metal=1; o->alb=V(0.80f,0.80f,0.82f); o->rough=0.12f; return; }
  if(q.y>0){ o->metal=0.75f; o->alb=V(0.05f,0.14f+g,0.62f); o->rough=0.22f; o->clear=0.5f; }
  else { o->metal=0.6f; o->alb=V(0.62f,0.04f,0.035f); o->rough=0.25f; o->clear=0.5f; }
}
static void fx_aimant(int L,float x,float y,F3 *c,float *a){
  if(L==0) return;
  float dx=x, dy=y-BALL_C, r=sqrtf(dx*dx+dy*dy); if(r<BALL_R*1.0f) return;
  float th=atan2f(fabsf(dx),dy);   /* 0 en haut */
  float s2=sinf(th)*sinf(th);
  float best=0;
  const float LL[3]={0.98f,1.22f,1.50f};
  for(int k=0;k<3;k++){ float rl=LL[k]*s2; if(rl<BALL_R*0.95f) continue; float dd=fabsf(r-rl);
    float w=0.011f+0.004f*k; float v=expf(-dd*dd/(w*w))*(0.9f-0.2f*k);
    float dash=0.65f+0.35f*sinf(th*22.0f-k*1.3f); best=fmaxf(best,v*dash); }
  if(best>0.004f){ addl(c,mul(V(0.45f,0.85f,1.6f),best)); *a=fmaxf(*a,sat(best*0.8f)); }
}

/* --- lumiere : orbe radieux, rayons divins --- */
static void setup_lumiere(void){ BALL_R=0.56f; uint32_t s=13; NSPK=0;
  for(int i=0;i<9;i++){ float a=rr(&s,0,TAU), r=rr(&s,0.70f,0.95f); spk_add(cosf(a)*r,BALL_C+sinf(a)*r,rr(&s,0.007f,0.013f),V(2.0f,1.8f,1.1f),rr(&s,0.4f,0.9f)); } }
static void mat_lumiere(F3 p,F3 *n,int m,Mat *o){ (void)m;
  F3 q=bq(p); float f=fbm3(mul(q,3.0f),4);
  float c=sat(n->z); o->alb=V(0.9f,0.85f,0.7f); o->rough=0.3f;
  o->emit=add(mul(V(1.8f,1.50f,0.85f),powf(c,1.5f)*(0.85f+0.25f*f)),mul(V(1.4f,0.78f,0.20f),(1-c)*1.1f));
}
static void fx_lumiere(int L,float x,float y,F3 *c,float *a){
  float dx=x, dy=y-BALL_C, r=sqrtf(dx*dx+dy*dy); float e=r-BALL_R;
  if(L==0){
    float ang=atan2f(dy,dx);
    float ray=powf(0.5f+0.5f*pn2(ang*3.0f,0.5f),3.0f)+0.6f*powf(0.5f+0.5f*pn2(ang*7.0f+4,1.3f),5.0f);
    float fall=expf(-fmaxf(e,0)/0.20f);
    float v=ray*fall*1.1f+expf(-fmaxf(e,0)/0.07f)*0.55f;
    if(v>0.003f){ addl(c,mul(V(1.25f,0.95f,0.40f),v)); *a=fmaxf(*a,sat(v*0.55f)); }
    return;
  }
  float st=star4(x,y,-0.12f,BALL_C+0.14f,0.42f)*0.35f; if(st>0.01f){ addl(c,mul(V(1.6f,1.5f,1.2f),st)); *a=fmaxf(*a,sat(st)); }
  spk_eval(x,y,c,a);
}

/* --- folle : bille de verre aux tourbillons de couleurs vives --- */
static void mat_folle(F3 p,F3 *n,int m,Mat *o){ (void)n; (void)m;
  F3 q=bq(p);
  F3 w=V(fbm3(mul(q,1.5f),3),fbm3(add(mul(q,1.5f),V(3,7,1)),3),0);
  float ang=atan2f(q.z,q.x)+q.y*2.2f+w.x*2.6f;
  float t=ang/TAU*3.0f+w.y*0.8f;
  static const float PC[4][3]={{1.0f,0.22f,0.72f},{0.50f,0.16f,0.88f},{1.0f,0.82f,0.25f},{0.95f,0.40f,0.85f}};
  float u=t-floorf(t); u*=4; int i0=(int)u; float fr_=u-i0; fr_=sst(0.30f,0.70f,fr_); int i1=(i0+1)&3; i0&=3;
  F3 col=mix3(V(PC[i0][0],PC[i0][1],PC[i0][2]),V(PC[i1][0],PC[i1][1],PC[i1][2]),fr_);
  o->alb=lin(col); o->rough=0.12f; o->clear=1.0f; o->sss=0.3f; o->sssc=o->alb;
}
static void setup_folle(void){ uint32_t s=4; NSPK=0; for(int i=0;i<6;i++){ float a=rr(&s,0,TAU), r=rr(&s,0.80f,0.98f); spk_add(cosf(a)*r,BALL_C+sinf(a)*r,rr(&s,0.008f,0.013f),V(1.8f,0.9f,1.6f),rr(&s,0.5f,0.9f)); } }
static void fx_folle(int L,float x,float y,F3 *c,float *a){ if(L==1){ spk_eval(x,y,c,a); float st=star4(x,y,0.52f,BALL_C+0.50f,0.18f)*0.7f; if(st>0.01f){ addl(c,mul(V(1.8f,1.2f,1.7f),st)); *a=fmaxf(*a,sat(st)); } } }

/* --- cristal : boule de cristal limpide, eclats de lumiere a l'interieur --- */
static F3 vol_cristal(F3 q,float *dens){
  Wor w=worley(add(mul(q,3.5f),V(0.3f,0.7f,0.1f)),0.8f);
  float sp=expf(-w.f1*w.f1*260.0f)*((w.id&3)==0?1.0f:0.0f);
  float caus=expf(-((q.y+0.62f)*(q.y+0.62f))*28.0f-(q.x*q.x+q.z*q.z)*6.0f);
  *dens=0.15f;
  return add(mul(V(1.4f,2.0f,2.2f),sp*2.5f),mul(V(0.6f,1.1f,1.2f),caus*1.2f));
}
static void mat_cristal(F3 p,F3 *n,int m,Mat *o){ (void)p; (void)n; (void)m; o->glass=1; o->ior=1.5f; o->absorb=V(0.75f,0.10f,0.06f); o->rough=0.02f; }
static void fx_cristal(int L,float x,float y,F3 *c,float *a){ if(L==1){ float st=star4(x,y,-0.34f,BALL_C+0.34f,0.20f)*0.8f; if(st>0.01f){ addl(c,mul(V(1.6f,1.9f,2.0f),st)); *a=fmaxf(*a,sat(st)); } } }

/* --- vent : sphere de brume verte prise dans des bourrasques --- */
static F3 vol_rafale(F3 q,float *dens){
  float sw=0.5f+0.5f*sinf(atan2f(q.z,q.x)*3.0f+q.y*5.0f+fbm3(mul(q,2.0f),3)*3.0f);
  float k=powf(sw,4.0f)*sst(1.0f,0.4f,len(q));
  *dens=0.3f+k*2.0f; return add(mul(V(0.75f,1.0f,0.85f),k*1.0f),V(0.03f,0.07f,0.05f));
}
static void mat_rafale(F3 p,F3 *n,int m,Mat *o){ (void)p; (void)n; (void)m; o->glass=1; o->ior=1.25f; o->absorb=V(1.1f,0.25f,0.55f); o->rough=0.18f; }
static float streak(float x,float y,float cy,float ra,float rb,float rot,float t0,float t1,float w,int front){
  float dx=x, dy=y-cy; float c=cosf(rot), s=sinf(rot); float u=c*dx+s*dy, v=-s*dx+c*dy;
  float t=atan2f(v/rb,u/ra); if(t<0) t+=TAU;
  bool isfront=t>PI; if((front!=0)!=isfront) return 0;
  float span=t1-t0; float tt=t-t0; if(tt<0) tt+=TAU; if(tt>span) return 0;
  float k=tt/span; float taper=sinf(k*PI);
  float rho=sqrtf((u/ra)*(u/ra)+(v/rb)*(v/rb)); float d=fabsf(rho-1)*fminf(ra,rb);
  float ww=w*(0.3f+0.7f*taper);
  return expf(-d*d/(ww*ww))*taper;
}
static void fx_rafale(int L,float x,float y,F3 *c,float *a){
  float v=0;
  v+=streak(x,y,BALL_C,0.92f,0.34f,0.35f,0.6f,5.6f,0.030f,L);
  v+=streak(x,y,BALL_C+0.05f,0.85f,0.28f,-0.25f,2.4f,7.2f,0.024f,L)*0.85f;
  v+=streak(x,y,BALL_C-0.05f,0.98f,0.40f,0.10f,4.0f,8.6f,0.020f,L)*0.7f;
  if(v>0.003f){ over(c,a,V(0.85f,1.0f,0.92f),sat(v*0.85f)); }
}

/* ======================================================================= */
/*                              LES EVOLUTIONS                             */
/* ======================================================================= */
static float cloudn(float x,float y,float sc,float seed){ return fbm2(x*sc+seed,y*sc-seed*0.3f,5)*0.5f+0.5f; }
/* nuage eclaire : densite et lumiere (plus clair en haut a gauche) */
static void puff(F3 *c,float *a,float x,float y,float region,float sc,float seed,F3 tint,float op){
  if(region<=0.003f) return;
  float n=cloudn(x,y,sc,seed);
  float dens=sat((n-0.42f)*3.0f)*region; if(dens<=0.003f) return;
  float n2=cloudn(x-0.035f,y+0.035f,sc,seed);
  float lgt=sat(0.62f+(n2-n)*9.0f+0.25f*(1-n));
  over(c,a,mul(tint,0.55f+0.6f*lgt),dens*op);
}

/* --- vapeur : sphere ruisselante (froid en haut, chaud en bas) dans des volutes de vapeur --- */
static float h_drops(F3 p){ Wor w=worley(mul(bq(p),9.0f),0.85f); return expf(-w.f1*w.f1*90.0f)*((w.id&3)==0?1.0f:0.0f); }
static F3 vol_vapeur(F3 q,float *dens){
  float ang=atan2f(q.z,q.x)+q.y*2.0f; F3 sw=V(cosf(ang)*len(V(q.x,0,q.z)),q.y,sinf(ang)*len(V(q.x,0,q.z)));
  float m=sat(fbm3(mul(sw,2.6f),5)*1.5f+0.15f+0.25f*q.y)*sst(1.0f,0.5f,len(q));
  float hot=sat(-q.y*0.8f-0.1f);
  *dens=0.3f+m*2.6f;
  return add(mul(mix3(V(0.62f,0.70f,0.80f),V(1.1f,0.55f,0.25f),hot*0.6f),m*0.95f),V(0.02f,0.03f,0.05f));
}
static void mat_vapeur(F3 p,F3 *n,int m,Mat *o){ (void)m;
  *n=bump(p,*n,h_drops,0.12f,0.003f);
  o->glass=1; o->ior=1.4f; o->absorb=V(0.7f,0.35f,0.15f); o->rough=0.04f;
}
static void fx_vapeur(int L,float x,float y,F3 *c,float *a){
  float dx=x, dy=y-BALL_C, d=sqrtf(dx*dx+dy*dy)-BALL_R;
  if(L==0){ float up=sat(dy/BALL_R*0.6f+0.5f); float reg=sat(1-d/(0.07f+0.36f*up))*sat(d/0.05f+1.5f);
    puff(c,a,x,y-0.0f,reg,2.4f,0.0f,V(0.88f,0.91f,0.95f),0.95f); return; }
  if(d>0.02f||d<-0.30f||y>BALL_C+0.25f) return;
  float reg=sat(1+d/0.30f)*sat((BALL_C+0.25f-y)/0.25f);
  puff(c,a,x,y,reg*0.9f,2.9f,7.3f,V(0.90f,0.93f,0.97f),0.85f);
}

/* --- meteore : roc en fusion qui file, trainee de feu --- */
static float sdf_meteore(F3 p,int *m){ *m=0; F3 q=sub(p,BC); float r=len(q);
  float disp=fbm3(mul(q,2.0f),4)*0.09f+fbm3(mul(q,6.0f),2)*0.02f; return (r-BALL_R+disp)*0.8f; }
static void mat_meteore(F3 p,F3 *n,int m,Mat *o){ (void)m;
  F3 q=bq(p); Wor w=worley(mul(q,2.2f),0.85f); float edge=w.f2-w.f1;
  float crack=1-sst(0.0f,0.05f,edge); float heat=sat(0.5f+0.8f*fbm3(add(mul(q,2.0f),V(3,1,2)),4));
  float front=sst(0.30f,0.95f,-q.x*0.7f-q.y*0.7f);   /* face avant (bas gauche) chauffee par l'air */
  *n=bump(p,*n,h_lava,0.45f,0.004f);
  o->alb=mix3(V(0.030f,0.022f,0.018f),V(0.075f,0.050f,0.040f),heat); o->rough=0.85f;
  o->emit=add(mul(fire_ramp(0.35f+0.65f*crack),crack*(0.7f+0.5f*heat)*(0.4f+0.6f*front)),mul(fire_ramp(0.25f+0.6f*front),front*front*1.2f));
}
static void setup_meteore(void){ BALL_C=-0.16f; BALL_R=0.52f; uint32_t s=8; NSPK=0;
  for(int i=0;i<16;i++){ float t=rr(&s,0.2f,1.0f); float sp=rr(&s,-0.25f,0.25f); float ux=0.72f,uy=0.69f;
    spk_add(-0.10f+ux*t*1.0f-uy*sp,BALL_C+uy*t*1.0f+ux*sp,rr(&s,0.006f,0.014f),V(1.9f,0.8f,0.15f),rr(&s,0.4f,1.0f)); } }
static void fx_meteore(int L,float x,float y,F3 *c,float *a){
  if(L==1){ spk_eval(x,y,c,a); return; }
  const float ux=0.72f, uy=0.69f;
  float dx=x, dy=y-BALL_C; float s=dx*ux+dy*uy, t=-dx*uy+dy*ux;
  float Lt=1.30f; if(s<-0.05f) return;
  float k=sat(s/Lt);
  float w=BALL_R*(1.0f-0.62f*k)+0.02f;
  float n=fbm2(s*2.6f-1.0f,t*6.0f+2.0f,5)*0.5f+0.5f;
  float f=sat(1-fabsf(t)/w); f=f*f*(3-2*f)*powf(1-k,0.8f);
  f*=0.55f+0.75f*n; f=sat(f*1.3f);
  if(f<=0.002f) return;
  float hot=sat(f*(1.25f-k));
  F3 col=fire_ramp(0.10f+0.90f*hot);
  F3 smoke=V(0.07f,0.05f,0.045f); col=mix3(col,smoke,sst(0.50f,0.95f,k)*0.85f);
  over(c,a,mul(col,1.0f/fmaxf(maxc(col),1.0f)),sat(f*1.4f)*(0.95f-0.35f*k)); addl(c,mul(col,hot*hot*0.35f));
}

/* --- tempete : nuages d'orage tourbillonnants et eclairs bleus dans le verre --- */
static void setup_tempete(void){
  uint32_t s=313; NARC=0; NSPK=0;
  const float A[5]={-2.5f,-1.2f,0.4f,1.5f,2.6f};
  for(int i=0;i<5;i++){ float a=A[i]+rr(&s,-0.2f,0.2f); float r0=BALL_R*0.97f, r1=BALL_R+rr(&s,0.14f,0.25f); float b=a+rr(&s,-0.5f,0.5f);
    arc_build(sinf(a)*r0,BALL_C+cosf(a)*r0,sinf(b)*r1,BALL_C+cosf(b)*r1,0.55f,5,rr(&s,0.011f,0.015f),&s); }
}
static F3 vol_tempete(F3 q,float *dens){
  float ang=atan2f(q.z,q.x)+len(V(q.x,0,q.z))*2.5f;
  F3 sw=V(cosf(ang)*len(V(q.x,0,q.z)),q.y,sinf(ang)*len(V(q.x,0,q.z)));
  float cl=sat(fbm3(mul(sw,2.4f),5)*1.3f+0.25f);
  F3 w=V(fbm3(mul(q,1.6f),3),fbm3(add(mul(q,1.6f),V(5,1,3)),3),0);
  float bolt=powf(1-fabsf(pn(add(mul(q,1.5f),mul(w,1.0f)))),26.0f);
  *dens=0.5f+cl*2.5f;
  F3 cc=mix3(V(0.02f,0.04f,0.12f),V(0.20f,0.30f,0.55f),cl);
  return add(mul(cc,cl*0.6f),mul(V(0.9f,1.6f,2.4f),bolt*3.0f));
}
static void mat_tempete(F3 p,F3 *n,int m,Mat *o){ (void)p; (void)n; (void)m; o->glass=1; o->ior=1.45f; o->absorb=V(2.0f,1.6f,0.9f); o->rough=0.03f; }
static void fx_tempete(int L,float x,float y,F3 *c,float *a){
  if(L==0){ float d=dball(x,y); float g=expf(-fmaxf(d,0)/0.07f)*0.30f; addl(c,mul(V(0.2f,0.6f,1.3f),g)); *a=fmaxf(*a,g*0.4f); return; }
  float w; float d=arc_dist(x,y,&w);
  float core=expf(-d*d*1.2f), halo=expf(-d*0.75f)*0.35f;
  if(core+halo>0.003f){ addl(c,add(mul(V(1.4f,1.8f,2.0f),core),mul(V(0.2f,0.6f,1.4f),halo))); *a=fmaxf(*a,sat(core+halo*0.5f)); }
}

/* --- fleau : ronce violette suintant un poison vert --- */
static void setup_fleau(void){ uint32_t s=71; NTH=14; for(int i=0;i<NTH;i++){ TH[i]=nrm(roty(rotx(fib(i,NTH),0.7f),0.4f)); THL[i]=rr(&s,0.18f,0.26f); } }
static float sdf_fleau(F3 p,int *m){
  F3 v=sub(p,BC); float d0=len(v)-BALL_R*0.94f; float d=d0; *m=0;
  F3 dp=nrm(v);
  for(int i=0;i<NTH;i++){ if(dot(dp,TH[i])<0.84f) continue;
    F3 a=add(BC,mul(TH[i],BALL_R*0.80f)), b=add(BC,mul(TH[i],BALL_R*0.94f+THL[i]));
    float ds=sd_cap2(p,a,b,0.10f,0.004f); if(ds<d0) *m=1; d=smin(d,ds,0.04f); }
  float g1=sd_cap2(p,V(0.05f,BALL_C-BALL_R*0.9f,0.30f),V(0.06f,BALL_C-BALL_R-0.20f,0.34f),0.08f,0.04f);
  if(g1<d){ *m=2; } d=smin(d,g1,0.08f);
  return d;
}
static void mat_fleau(F3 p,F3 *n,int m,Mat *o){
  F3 q=bq(p);
  if(m==2){ o->alb=V(0.04f,0.20f,0.01f); o->rough=0.08f; o->clear=1; o->sss=0.6f; o->sssc=V(0.3f,1.0f,0.1f); o->emit=V(0.04f,0.20f,0.01f); return; }
  float v=ridged(mul(q,3.5f),4);
  *n=bump(p,*n,h_bark,0.18f,0.004f);
  if(m==0){ o->alb=V(0.07f,0.015f,0.09f); o->rough=0.38f; o->clear=0.4f;
    float vein=sst(0.55f,0.85f,v); o->emit=mul(V(0.35f,1.0f,0.10f),vein*0.55f); }
  else { float t=sat((len(sub(p,BC))-BALL_R*0.85f)/0.24f); o->alb=mix3(V(0.10f,0.02f,0.12f),V(0.02f,0.01f,0.02f),t); o->rough=0.3f; o->clear=0.5f;
    o->emit=mul(V(0.30f,0.95f,0.08f),sst(0.6f,1.0f,t)*0.8f); }
}
static void fx_fleau(int L,float x,float y,F3 *c,float *a){
  if(L==1) return;
  float d=dball(x,y); if(d<-0.05f) return;
  float n=fbm2(x*3.0f+4,y*3.0f-1,5); float al=sat(1-d/0.30f)*sat(0.38f+n)*0.5f;
  if(al>0) over(c,a,V(0.30f,0.75f,0.10f),al);
}

/* --- faucheuse : crane d'os aux yeux ardents --- */
static float sdf_faucheuse(F3 p,int *m){
  F3 q=sub(p,BC); *m=0;
  float d=sd_ell(q,V(BALL_R*0.96f,BALL_R,BALL_R*0.98f));
  /* machoire plus etroite */
  float jaw=sd_ell(sub(q,V(0,-0.30f,0.10f)),V(0.42f,0.32f,0.48f)); d=smin(d,jaw,0.10f);
  float cut=q.y+0.62f; d=smax(d,-cut,0.05f);
  for(int s2=-1;s2<=1;s2+=2){ float e=sd_ell(sub(q,V(s2*0.25f,0.06f,0.55f)),V(0.17f,0.15f,0.20f)); d=smax(d,-e,0.04f); }
  float no=sd_ell(sub(q,V(0,-0.15f,0.64f)),V(0.06f,0.09f,0.12f)); d=smax(d,-no,0.02f);
  /* braises dans les orbites */
  for(int s2=-1;s2<=1;s2+=2){ float e=sd_sph(q,V(s2*0.24f,0.05f,0.47f),0.07f); if(e<d){ d=e; *m=1; } }
  return d;
}
static void mat_faucheuse(F3 p,F3 *n,int m,Mat *o){
  F3 q=sub(p,BC);
  if(m==1){ o->alb=V(0.3f,0.05f,0.1f); o->emit=V(2.2f,0.55f,0.95f); return; }
  float dirt=fbm3(mul(q,4.0f),4); Wor w=worley(mul(q,3.0f),0.9f); float crk=1-sst(0.0f,0.025f,w.f2-w.f1);
  *n=bump(p,*n,h_dent,0.03f,0.004f);
  o->alb=mix3(V(0.62f,0.55f,0.42f),V(0.30f,0.24f,0.18f),sat(dirt*0.9f+0.35f)); o->rough=0.6f; o->sss=0.2f; o->sssc=V(0.9f,0.6f,0.4f);
  o->alb=mix3(o->alb,V(0.08f,0.05f,0.04f),crk*0.6f*sat(q.y+0.4f));
  /* dents : rainures verticales sous le nez */
  if(q.y<-0.30f && q.y>-0.52f && q.z>0.25f){ float g=fabsf(sinf(q.x*26.0f)); float gap=1-sst(0.0f,0.25f,g); float line=1-sst(0.0f,0.03f,fabsf(q.y+0.41f));
    o->alb=mix3(o->alb,V(0.05f,0.03f,0.03f),fmaxf(gap*0.85f,line*0.9f)); o->aoc=1-0.6f*fmaxf(gap,line); }
}
static void fx_faucheuse(int L,float x,float y,F3 *c,float *a){
  if(L==1){ for(int s2=-1;s2<=1;s2+=2){ float g=expf(-((x-s2*0.24f)*(x-s2*0.24f)+(y-BALL_C-0.05f)*(y-BALL_C-0.05f))/0.010f)*0.6f; addl(c,mul(V(1.6f,0.4f,0.8f),g)); *a=fmaxf(*a,g*0.4f); } return; }
  float d=dball(x,y); if(d<-0.03f) return;
  float n=fbm2(x*3.0f+2,y*2.6f-3,5); float up=sat((y-BALL_C)/BALL_R*0.5f+0.7f);
  float al=sat(1-d/0.28f)*sat(0.40f+n*1.2f)*0.65f*up;
  if(al>0.003f) over(c,a,V(0.40f,0.10f,0.32f),al);
}

/* --- prisme : cristal blanc a facettes qui decompose la lumiere --- */
static void setup_prisme(void){ uint32_t s=6; NFAC=20; for(int i=0;i<NFAC;i++) FAC[i]=nrm(roty(rotx(fib(i,NFAC),0.3f),0.5f)); NSPK=0;
  for(int i=0;i<9;i++){ float a=rr(&s,0,TAU), r=rr(&s,0.78f,0.98f); F3 col=lin(palette(rnd(&s),V(0.5f,0.5f,0.5f),V(0.5f,0.5f,0.5f),V(1,1,1),V(0.0f,0.33f,0.67f)));
    spk_add(cosf(a)*r,BALL_C+sinf(a)*r,rr(&s,0.008f,0.014f),mul(col,2.2f),rr(&s,0.5f,1.0f)); } }
static float sdf_facets(F3 p,int *m){ F3 v=sub(p,BC); float d=-9; int k=0; for(int i=0;i<NFAC;i++){ float q=dot(v,FAC[i])-BALL_R*0.97f; if(q>d){ d=q; k=i; } } *m=10+k; return d; }
static F3 vol_prisme(F3 q,float *dens){
  float ang=atan2f(q.y,q.x)+q.z*1.5f; F3 col=lin(palette(ang/TAU*2.0f,V(0.5f,0.5f,0.5f),V(0.5f,0.5f,0.5f),V(1,1,1),V(0.0f,0.33f,0.67f)));
  Wor w=worley(mul(q,1.2f),0.9f); float g=w.f2-w.f1; float pl=expf(-g*g*4000.0f);
  *dens=0.1f+pl*1.0f; return add(mul(col,pl*0.55f),mul(col,0.04f));
}
static void mat_prisme(F3 p,F3 *n,int m,Mat *o){ (void)p; (void)n; (void)m; o->glass=1; o->ior=1.52f; o->disp=0.09f; o->absorb=V(0.05f,0.05f,0.04f); o->rough=0.01f; }
static void fx_prisme(int L,float x,float y,F3 *c,float *a){ if(L==1){ spk_eval(x,y,c,a); float st=star4(x,y,-0.32f,BALL_C+0.36f,0.22f)*0.8f; if(st>0.01f){ addl(c,mul(V(1.8f,1.8f,1.9f),st)); *a=fmaxf(*a,sat(st)); } } }

/* --- singularite : trou noir et son disque de matiere en feu violet --- */
static void setup_sing(void){ BALL_C=0.0f; BALL_R=0.36f; }
static void mat_sing(F3 p,F3 *n,int m,Mat *o){ (void)p; (void)n; (void)m; o->alb=V(0,0,0); o->spec=0.0f; o->rough=1; }
static float disk_v(float x,float y,int front,float *col_t){
  float rot=-0.22f; float c=cosf(rot), s=sinf(rot); float u=c*x+s*y, v=-s*x+c*y;
  float ra=0.95f, rb=0.22f; float rho=sqrtf((u/ra)*(u/ra)+(v/rb)*(v/rb));
  bool isfront=v<0; if((front!=0)!=isfront) return 0;
  if(rho<0.44f||rho>1.0f) return 0;
  float ang=atan2f(v/rb,u/ra);
  float k=(rho-0.44f)/0.56f;
  float str=0.55f+0.45f*fbm2(ang*3.0f,rho*9.0f,4);
  float dop=0.75f+0.45f*(-u/ra);   /* un cote plus brillant */
  float edge=sst(0.0f,0.12f,k)*sst(1.0f,0.6f,k);
  *col_t=k;
  return edge*str*dop;
}
static void fx_sing(int L,float x,float y,F3 *c,float *a){
  float r=sqrtf(x*x+y*y);
  float ct=0; float v=disk_v(x,y,L,&ct);
  if(L==0){
    /* image deviee du disque autour de l'horizon, et anneau de photons */
    float ring=expf(-((r-BALL_R*1.10f)*(r-BALL_R*1.10f))/0.0009f)*1.4f;
    float lens=expf(-((r-BALL_R*1.35f)*(r-BALL_R*1.35f))/0.006f)*0.7f*(0.5f+0.5f*fbm2(atan2f(y,x)*4.0f,r*6.0f,3));
    float glow=expf(-fmaxf(r-BALL_R,0)/0.22f)*0.35f;
    F3 cc=add(add(mul(V(1.6f,1.2f,2.4f),ring),mul(V(1.2f,0.5f,2.0f),lens)),mul(V(0.30f,0.12f,0.60f),glow));
    addl(c,cc); *a=fmaxf(*a,sat(ring+lens*0.6f+glow*0.5f));
  }
  if(v>0.003f){
    F3 hot=V(2.2f,1.8f,2.6f), mid=V(1.3f,0.45f,2.2f), cold=V(0.35f,0.08f,0.70f);
    F3 cc=ct<0.4f?mix3(hot,mid,ct/0.4f):mix3(mid,cold,(ct-0.4f)/0.6f);
    over(c,a,mul(cc,1.0f/fmaxf(maxc(cc),1)),sat(v*1.2f)*0.95f); addl(c,mul(cc,v*0.5f));
  }
}

/* --- enclume : boule d'acier chrome cerclee et rivetee --- */
static float sdf_enclume(F3 p,int *m){
  F3 q=sub(p,BC); float d=len(q)-BALL_R; *m=0;
  float band=smax(len(q)-(BALL_R+0.035f),fabsf(q.y)-0.085f,0.012f); if(band<d){ d=band; *m=1; }
  float ang=atan2f(q.z,q.x); float st=TAU/14; float a0=floorf(ang/st+0.5f)*st;
  F3 rc=V(cosf(a0)*(BALL_R+0.045f),0,sinf(a0)*(BALL_R+0.045f)); float rv=sd_sph(q,rc,0.032f); if(rv<d){ d=rv; *m=2; }
  return d;
}
static void mat_enclume(F3 p,F3 *n,int m,Mat *o){ (void)n;
  F3 q=bq(p);
  if(m==0){ float sc=fbm3(V(q.x*8,q.y*50,q.z*8),3); o->metal=1; o->alb=V(0.86f,0.86f,0.90f); o->rough=0.05f+0.08f*sat(sc*0.6f+0.5f); }
  else if(m==1){ o->metal=1; o->alb=V(0.30f,0.30f,0.34f); o->rough=0.30f; }
  else { o->metal=1; o->alb=V(0.85f,0.62f,0.32f); o->rough=0.22f; }
}

/* --- rayon divin : or poli, aureole et colonne de lumiere --- */
static void setup_rayon(void){ BALL_C=-0.06f; BALL_R=0.56f; uint32_t s=21; NSPK=0;
  for(int i=0;i<8;i++){ float a=rr(&s,0,TAU), r=rr(&s,0.70f,0.95f); spk_add(cosf(a)*r,BALL_C+sinf(a)*r,rr(&s,0.007f,0.012f),V(2.0f,1.7f,0.9f),rr(&s,0.4f,0.9f)); } }
static float sdf_rayon(F3 p,int *m){
  F3 q=sub(p,BC); float d=len(q)-BALL_R; *m=0;
  F3 h=rotz(rotx(q,0.30f),0.14f); float t=sd_tor(h,0.80f,0.030f); if(t<d){ d=t; *m=1; }
  return d;
}
static void mat_rayon(F3 p,F3 *n,int m,Mat *o){
  if(m==1){ o->metal=1; o->alb=V(1.0f,0.78f,0.35f); o->rough=0.15f; o->emit=V(1.5f,1.05f,0.35f); return; }
  F3 q=bq(p); float g=fbm3(mul(q,3),3);
  /* motif grave : soleil rayonnant */
  float ang=atan2f(q.y,q.x), rad=sqrtf(q.x*q.x+q.y*q.y);
  float eng=(q.z>0.3f)? (sst(0.02f,0.0f,fabsf(rad-0.42f))+sst(0.05f,0.0f,fabsf(sinf(ang*8))*rad)*sst(0.40f,0.20f,rad)*sst(0.05f,0.12f,rad)) : 0;
  *n=bump(p,*n,h_hammer,0.004f,0.004f);
  o->metal=1; o->alb=V(1.0f,0.76f,0.32f); o->rough=0.24f+0.06f*g+eng*0.2f;
  o->emit=add(mul(V(1.4f,1.0f,0.35f),eng*0.6f),mul(V(0.30f,0.20f,0.06f),0.6f+0.4f*sat(n->z)));
}
static void fx_rayon(int L,float x,float y,F3 *c,float *a){
  if(L==1){ spk_eval(x,y,c,a); return; }
  float beam=expf(-x*x/0.012f)*sat(1-fabsf(y-BALL_C)/1.0f)*0.9f;
  float d=dball(x,y); float glow=expf(-fmaxf(d,0)/0.10f)*0.45f;
  float v=beam+glow; if(v>0.003f){ addl(c,mul(V(1.5f,1.15f,0.50f),v)); *a=fmaxf(*a,sat(v*0.6f)); }
}

/* --- kaleidoscope : gemme a facettes de toutes les couleurs --- */
static void setup_kaleido(void){ NFAC=34; for(int i=0;i<NFAC;i++) FAC[i]=nrm(rotz(rotx(fib(i,NFAC),0.6f),0.2f)); uint32_t s=17; NSPK=0;
  for(int i=0;i<7;i++){ float a=rr(&s,0,TAU), r=rr(&s,0.80f,0.98f); spk_add(cosf(a)*r,BALL_C+sinf(a)*r,rr(&s,0.008f,0.013f),V(2.0f,1.8f,2.0f),rr(&s,0.4f,0.9f)); } }
static void mat_kaleido(F3 p,F3 *n,int m,Mat *o){ (void)p; (void)n;
  int k=m-10; float t=k*0.6180339f; t-=floorf(t);
  F3 col=lin(palette(t,V(0.55f,0.5f,0.55f),V(0.45f,0.45f,0.45f),V(1,1,1),V(0.0f,0.33f,0.67f)));
  o->alb=col; o->metal=0.75f; o->rough=0.06f; o->emit=mul(col,0.12f);
}

/* --- geode toxique : roc ouvert sur des cristaux d'amethyste --- */
static F3 GEO_O; 
static void setup_geode(void){ GEO_O=V(0.12f,0.10f,0.62f); }
static float geo_rin(F3 dir){ Wor w=worley(mul(dir,5.0f),0.9f); return 0.60f-0.20f*sst(0.55f,0.0f,w.f1); }
static float sdf_geode(F3 p,int *m){
  F3 q=sub(p,BC); float r=len(q);
  float rock=r-BALL_R+fbm3(mul(q,3.0f),3)*0.03f;
  float rin=geo_rin(mul(q,1.0f/fmaxf(r,1e-4f)));
  float d=fmaxf(rock,-(r-rin));
  float op=sd_sph(p,add(BC,GEO_O),0.50f);
  d=smax(d,-op,0.03f);
  *m= r>BALL_R-0.03f ? 0 : (r>0.58f ? 1 : 2);
  return d*0.75f;
}
static void mat_geode(F3 p,F3 *n,int m,Mat *o){
  F3 q=bq(p);
  if(m==0){ float g=fbm3(mul(q,4),5); *n=bump(p,*n,h_dent,0.05f,0.004f); o->alb=mix3(V(0.10f,0.09f,0.085f),V(0.25f,0.23f,0.21f),sat(g+0.5f)); o->rough=0.85f; }
  else if(m==1){ float r=len(sub(p,BC)); float b=0.5f+0.5f*sinf(r*120.0f); o->alb=mix3(V(0.55f,0.52f,0.58f),V(0.85f,0.82f,0.88f),b); o->rough=0.5f; }
  else { float r=len(sub(p,BC)); float tip=sst(0.56f,0.42f,r);
    o->alb=mix3(V(0.20f,0.04f,0.35f),V(0.70f,0.45f,0.95f),tip); o->rough=0.08f; o->spec=0.07f; o->sss=0.8f; o->sssc=V(0.7f,0.3f,1.0f);
    o->emit=add(mul(V(0.25f,0.05f,0.45f),0.25f+tip*0.5f),mul(V(0.25f,0.85f,0.10f),sst(0.5f,0.38f,r)*0.6f)); }
}
static void fx_geode(int L,float x,float y,F3 *c,float *a){
  if(L==1) return;
  float d=dball(x,y); if(d<-0.03f) return;
  float n=fbm2(x*3.0f+9,y*3.0f,5); float up=sat((y-BALL_C)/BALL_R*0.5f+0.6f);
  float al=sat(1-d/0.24f)*sat(0.36f+n)*0.45f*up; if(al>0) over(c,a,V(0.40f,0.85f,0.20f),al);
}

/* --- blizzard : boule de neige tassee dans un tourbillon de flocons --- */
static float h_snow(F3 p){ return fbm3(mul(bq(p),9.0f),3)*0.6f+fbm3(mul(bq(p),26.0f),2)*0.3f; }
static void mat_blizzard(F3 p,F3 *n,int m,Mat *o){ (void)m;
  F3 q=bq(p); *n=bump(p,*n,h_snow,0.20f,0.004f);
  o->alb=V(0.80f,0.86f,0.93f); o->rough=0.75f; o->sss=0.55f; o->sssc=V(0.45f,0.70f,1.0f);
  float gl=pn(mul(q,40.0f)); o->emit=mul(V(1.4f,1.6f,1.8f),sst(0.62f,0.70f,gl)*0.8f);
}
static void setup_blizzard(void){ uint32_t s=101; NSPK=0;
  for(int i=0;i<40;i++){ float a=rr(&s,0,TAU), r=rr(&s,0.70f,1.0f); spk_add(cosf(a)*r,BALL_C+sinf(a)*r*0.85f,rr(&s,0.008f,0.016f),V(1.3f,1.45f,1.6f),rr(&s,0.4f,1.0f)); } }
static void fx_blizzard(int L,float x,float y,F3 *c,float *a){
  float v=0;
  v+=streak(x,y,BALL_C,0.94f,0.36f,0.30f,0.4f,5.4f,0.030f,L);
  v+=streak(x,y,BALL_C+0.04f,0.86f,0.30f,-0.30f,2.2f,7.0f,0.024f,L)*0.8f;
  if(v>0.003f){ float n=0.6f+0.4f*fbm2(x*12,y*12,2); over(c,a,V(0.92f,0.96f,1.0f),sat(v*0.8f*n)); }
  if(L==1) spk_eval(x,y,c,a);
  else { float d=dball(x,y); float al=sat(1-d/0.18f)*sat(d/0.02f+1)*0.25f*(0.5f+0.5f*fbm2(x*4,y*4,4)); if(al>0) over(c,a,V(0.7f,0.85f,1.0f),al); }
}

/* --- tempete de feu : tornade de flammes autour d'un coeur ardent --- */
static void fx_tempfeu(int L,float x,float y,F3 *c,float *a){
  float f=0;
  f=fmaxf(f,streak(x,y,BALL_C,0.95f,0.34f,0.32f,0.3f,5.5f,0.055f,L));
  f=fmaxf(f,streak(x,y,BALL_C+0.10f,0.86f,0.26f,-0.28f,2.0f,7.0f,0.045f,L)*0.85f);
  f=fmaxf(f,streak(x,y,BALL_C-0.12f,0.92f,0.30f,0.05f,3.6f,8.4f,0.040f,L)*0.75f);
  if(L==0) f=fmaxf(f,flames(x,y,0,BALL_C,BALL_R,0.34f,2.0f)*0.9f);
  if(f>0.002f){ float n=0.5f+0.5f*fbm2(x*6,y*6+L*3,3); f=sat(f*(0.7f+0.6f*n));
    F3 col=fire_ramp(0.15f+0.85f*f); over(c,a,mul(col,1.0f/fmaxf(maxc(col),1.0f)),sat(f*1.5f)*0.95f); addl(c,mul(col,f*f*0.4f)); }
  if(L==1) spk_eval(x,y,c,a);
}

/* --- aurore : ciel de nuit dans le verre, voiles d'aurore et etoiles --- */
static F3 vol_aurore(F3 q,float *dens){
  F3 acc=V(0,0,0); float dd=0.2f;
  for(int k=0;k<3;k++){
    float zc=0.30f*sinf(q.x*2.3f+k*2.1f)+(k-1)*0.30f;
    float dz=q.z-zc; float sheet=expf(-dz*dz*90.0f);
    float h=q.y+0.30f+0.12f*sinf(q.x*4.0f+k);
    float prof= h<0 ? expf(h*25.0f) : expf(-h*2.2f);
    float stri=0.55f+0.45f*pn(V(q.x*16.0f,k*3.0f,0.5f));
    float v=sheet*prof*stri;
    F3 col=mix3(V(0.15f,1.0f,0.65f),V(1.0f,0.30f,0.80f),sat(h*1.6f));
    acc=add(acc,mul(col,v*2.2f)); dd+=v*1.5f;
  }
  Wor w=worley(mul(q,5.0f),0.9f); float st=expf(-w.f1*w.f1*900.0f)*((w.id&3)==0);
  acc=add(acc,mul(V(1.5f,1.5f,1.6f),st*2.0f));
  *dens=dd; return add(acc,V(0.005f,0.008f,0.025f));
}
static void mat_aurore(F3 p,F3 *n,int m,Mat *o){ (void)p; (void)n; (void)m; o->glass=1; o->ior=1.45f; o->absorb=V(2.4f,2.2f,1.6f); o->rough=0.02f; }

/* --- eclat : esquille de cristal rose --- */
static float sdf_shard(F3 p,int *m){ *m=0; F3 q=rotz(roty(p,0.5f),-0.65f); q.y*=0.42f; q.x*=1.0f; return sd_octa(q,0.30f)*0.42f; }
static void mat_shard(F3 p,F3 *n,int m,Mat *o){ (void)p; (void)n; (void)m; o->alb=V(0.95f,0.52f,0.85f); o->rough=0.05f; o->spec=0.08f; o->sss=0.9f; o->sssc=V(1.0f,0.55f,0.9f); o->emit=V(0.12f,0.05f,0.10f); }
static void setup_shard(void){ uint32_t s=3; NSPK=0; for(int i=0;i<6;i++){ float a=rr(&s,0,TAU), r=rr(&s,0.45f,0.80f); spk_add(cosf(a)*r,sinf(a)*r,rr(&s,0.008f,0.014f),V(1.9f,1.3f,1.8f),rr(&s,0.4f,0.9f)); } }
static void fx_shard(int L,float x,float y,F3 *c,float *a){ if(L==1){ spk_eval(x,y,c,a); float st=star4(x,y,0.10f,0.30f,0.30f)*0.7f; if(st>0.01f){ addl(c,mul(V(1.8f,1.4f,1.8f),st)); *a=fmaxf(*a,sat(st)); } } }

/* --- ame : feu follet vert au regard vide --- */
static float sdf_ghost(F3 p,int *m){
  *m=0; float d=sd_sph(p,V(0,0.12f,0),0.42f);
  float t1=sd_cap2(p,V(0.0f,-0.05f,0),V(0.16f,-0.42f,-0.05f),0.30f,0.17f);
  float t2=sd_cap2(p,V(0.16f,-0.42f,-0.05f),V(-0.04f,-0.70f,-0.08f),0.17f,0.09f);
  float t3=sd_cap2(p,V(-0.04f,-0.70f,-0.08f),V(-0.30f,-0.80f,-0.10f),0.09f,0.02f);
  d=smin(d,t1,0.12f); d=smin(d,t2,0.08f); d=smin(d,t3,0.06f);
  return d;
}
static void mat_ghost(F3 p,F3 *n,int m,Mat *o){ (void)m;
  float c=sat(n->z); o->alb=V(0.10f,0.45f,0.25f); o->rough=0.3f; o->sss=1.0f; o->sssc=V(0.3f,1.0f,0.6f);
  F3 e=add(mul(V(0.30f,1.30f,0.75f),0.35f+0.65f*c),mul(V(0.6f,1.6f,1.0f),powf(1-c,2.5f)*0.8f));
  float ex=(fabsf(p.x)-0.15f)/0.075f, ey=(p.y-0.16f)/0.11f; float eye=sqrtf(ex*ex+ey*ey);
  float mx=p.x/0.07f, my=(p.y+0.02f)/0.05f; float mo=sqrtf(mx*mx+my*my);
  float hole=sst(1.0f,0.8f,fminf(eye,mo))*sat(n->z*2);
  o->emit=mul(e,1-hole*0.95f); o->alb=mix3(o->alb,V(0,0,0),hole); o->sss*=1-hole;
}
static void fx_ghost(int L,float x,float y,F3 *c,float *a){
  if(L==1) return;
  float d=sqrtf(x*x+(y-0.12f)*(y-0.12f))-0.42f; float g=expf(-fmaxf(d,0)/0.10f)*0.45f;
  addl(c,mul(V(0.25f,1.0f,0.6f),g)); *a=fmaxf(*a,g*0.35f);
}

/* ======================================================================= */
/*                                LES OBJETS                               */
/* ======================================================================= */
static inline float sd_rbox(F3 p,F3 c,F3 b,float r){ return sd_box(sub(p,c),b,r); }
static inline float dot2(float x,float y){ return x*x+y*y; }
static float sd_heart2(float x,float y){   /* coeur 2D : pointe en (0,0), lobes vers y=1 */
  x=fabsf(x);
  if(y+x>1.0f) return sqrtf(dot2(x-0.25f,y-0.75f))-0.35355339f;
  float m=fmaxf(x+y,0)*0.5f;
  return sqrtf(fminf(dot2(x,y-1.0f),dot2(x-m,y-m)))*((x-y)>0?1.0f:-1.0f);
}
/* coeur bombe en 3D */
static float sd_heart3(F3 p,float sc,float th){
  float d2=sd_heart2(p.x/sc,(p.y+0.62f*sc)/sc)*sc;
  float h=th*sqrtf(sat(-d2/(0.38f*sc)))+0.04f;
  return smax(d2,fabsf(p.z)-h,0.10f*sc)*0.8f;
}
/* rotation de l'objet entier (vue de trois quarts) */
static float OBJ_RY=0, OBJ_RX=0;
static inline F3 ob(F3 p){ return rotx(roty(p,OBJ_RY),OBJ_RX); }

/* --- gantelets : poing d'armure en acier, revers de laiton --- */
static float sdf_gant(F3 p,int *m){
  F3 q=mul(ob(p),0.92f); *m=0;
  float d=sd_rbox(q,V(0,0.02f,0),V(0.30f,0.26f,0.19f),0.11f);
  for(int i=0;i<4;i++){ float x=-0.235f+i*0.157f; float f=sd_rbox(q,V(x,0.33f-fabsf(x)*0.12f,0.05f),V(0.068f,0.13f,0.16f),0.06f); if(f<d){ d=smin(d,f,0.012f); if(f<d+0.01f) *m=1; } }
  float th=sd_cap2(q,V(-0.34f,-0.10f,0.10f),V(-0.20f,0.16f,0.22f),0.095f,0.075f); if(th<d){ d=smin(d,th,0.02f); *m=1; }
  float wr=sd_cyl(sub(q,V(0,-0.36f,0)),0.24f,0.12f,0.03f); d=smin(d,wr,0.03f);
  float cu=sd_cap2(q,V(0,-0.48f,0),V(0,-0.80f,0),0.29f,0.37f); cu=smax(cu,-(q.y+0.82f),0.01f); if(cu<d){ d=cu; *m=2; }
  float rim=sd_tor(sub(q,V(0,-0.80f,0)),0.36f,0.035f); if(rim<d){ d=rim; *m=3; }
  float rim2=sd_tor(sub(q,V(0,-0.49f,0)),0.27f,0.03f); if(rim2<d){ d=rim2; *m=3; }
  return d/0.92f;
}
static void mat_gant(F3 p,F3 *n,int m,Mat *o){
  F3 q=mul(ob(p),0.92f); float g=fbm3(mul(q,6.0f),3);
  *n=bump(p,*n,h_dent,0.02f,0.004f);
  if(m==3){ o->metal=1; o->alb=V(0.85f,0.62f,0.30f); o->rough=0.25f; return; }
  o->metal=1; o->alb=V(0.74f,0.75f,0.78f); o->rough=0.34f+0.10f*g;
  if(m==2){ o->alb=V(0.62f,0.63f,0.67f); o->rough=0.36f+0.1f*g; }
  /* rainures entre les plaques */
  if(m==0){ float band=fabsf(sinf(q.y*28.0f)); o->aoc=0.75f+0.25f*sst(0.0f,0.3f,band); }
}

/* --- sablier : bois tourne, verre et sable dore --- */
static float sdf_sablier(F3 p,int *m){
  F3 q=ob(p); *m=0; float d=9;
  float cap1=sd_cyl(sub(q,V(0,0.70f,0)),0.42f,0.055f,0.03f), cap2=sd_cyl(sub(q,V(0,-0.70f,0)),0.42f,0.055f,0.03f);
  d=fminf(cap1,cap2);
  for(int i=0;i<3;i++){ float a=i*TAU/3+0.5f; F3 c=V(cosf(a)*0.33f,0,sinf(a)*0.33f);
    float r=0.032f+0.012f*sinf(q.y*22.0f)*sinf(q.y*22.0f);
    float pl=sd_cap(q,add(c,V(0,-0.66f,0)),add(c,V(0,0.66f,0)),r); d=fminf(d,pl); }
  /* sable : tas en bas, reste en haut, filet au centre */
  float sb=smax(sd_cap2(q,V(0,-0.64f,0),V(0,-0.32f,0),0.25f,0.02f),-(q.y+0.66f),0.01f);
  float st=smax(sd_ell(sub(q,V(0,0.28f,0)),V(0.235f,0.30f,0.235f)),(q.y-0.17f)+0.06f*(1-sqrtf(q.x*q.x+q.z*q.z)/0.24f),0.02f);
  st=smax(st,-(q.y-0.02f),0.01f);
  float fl=sd_cap(q,V(0,0.05f,0),V(0,-0.40f,0),0.010f);
  float sand=fminf(fminf(sb,st),fl); if(sand<d){ d=sand; *m=2; }
  if(d>0.0f && !g_noglass){
    float bt=sd_ell(sub(q,V(0,0.30f,0)),V(0.27f,0.36f,0.27f)), bb=sd_ell(sub(q,V(0,-0.30f,0)),V(0.27f,0.36f,0.27f));
    float gl=smin(bt,bb,0.06f); gl=smax(gl,-(q.y-0.66f),0.0f); gl=smax(gl,q.y-0.66f,0.0f);
    float shell=fabsf(gl)-0.006f; shell=smax(shell,fabsf(q.y)-0.65f,0.01f);
    if(shell<d){ d=shell; *m=1; }
  }
  if(d>=9) d=1;
  return d;
}
static float h_wood(F3 p){ F3 q=ob(p); return fbm3(V(q.x*30,q.y*3,q.z*30),3)*0.3f; }
static void mat_sablier(F3 p,F3 *n,int m,Mat *o){
  F3 q=ob(p);
  if(m==1){ o->tglass=1; o->tint=V(0.92f,0.95f,0.98f); return; }
  if(m==2){ float g=pn(mul(q,90.0f)); o->alb=V(0.78f+0.08f*g,0.56f+0.06f*g,0.26f); o->rough=0.9f; o->emit=V(0.04f,0.025f,0.0f); return; }
  *n=bump(p,*n,h_wood,0.025f,0.004f);
  float ring=0.5f+0.5f*sinf(len(V(q.x,0,q.z))*90.0f+fbm3(mul(q,6),3)*4.0f);
  o->alb=mix3(V(0.26f,0.12f,0.05f),V(0.42f,0.22f,0.09f),ring); o->rough=0.35f; o->clear=0.5f;
}

/* --- bottes ailees : botte de cuir fauve, grande aile blanche a la cheville --- */
static float sd_poly(const float (*v)[2],int n,float px,float py){
  float d=dot2(px-v[0][0],py-v[0][1]); float sg=1;
  for(int i=0,j=n-1;i<n;j=i,i++){
    float ex=v[j][0]-v[i][0], ey=v[j][1]-v[i][1], wx=px-v[i][0], wy=py-v[i][1];
    float h=sat((wx*ex+wy*ey)/(ex*ex+ey*ey)); float bx=wx-ex*h, by=wy-ey*h; d=fminf(d,bx*bx+by*by);
    bool c1=py>=v[i][1], c2=py<v[j][1], c3=ex*wy>ey*wx;
    if((c1&&c2&&c3)||(!c1&&!c2&&!c3)) sg=-sg;
  }
  return sg*sqrtf(d);
}
static const float BOOT[][2]={{-0.30f,0.60f},{0.10f,0.60f},{0.07f,0.20f},{0.06f,-0.06f},{0.14f,-0.22f},{0.36f,-0.30f},{0.56f,-0.36f},{0.64f,-0.44f},{0.62f,-0.56f},
  {-0.04f,-0.57f},{-0.06f,-0.68f},{-0.32f,-0.68f},{-0.34f,-0.46f},{-0.37f,-0.10f},{-0.38f,0.25f}};
static float sdf_botte(F3 p,int *m){
  F3 q=ob(p); *m=0;
  float d2=sd_poly(BOOT,(int)(sizeof BOOT/sizeof BOOT[0]),q.x,q.y)+0.07f;
  float th=0.12f+0.04f*sat(q.y+0.4f);
  float d=smax(d2,fabsf(q.z)-th,0.13f)-0.01f;
  float cuff=smax(sd_poly(BOOT,(int)(sizeof BOOT/sizeof BOOT[0]),q.x*0.93f,q.y)+0.04f,fabsf(q.z)-0.15f,0.10f); cuff=smax(cuff,-(q.y-0.44f),0.02f);
  if(cuff<d){ d=smin(d,cuff,0.01f); *m=2; }
  if(q.y<-0.545f && *m==0) *m=1;
  float bk=sd_rbox(q,V(-0.10f,0.50f,0.19f),V(0.07f,0.055f,0.015f),0.012f); if(bk<d){ d=bk; *m=3; }
  for(int i=0;i<5;i++){ float a=0.30f+i*0.33f; F3 c=V(-0.20f-cosf(a)*0.30f,0.08f+sinf(a)*0.30f,0.21f+0.025f*i);
    F3 r=rotz(sub(q,c),-a); float f=sd_ell(r,V(0.36f-i*0.03f,0.080f,0.020f)); if(f<d){ d=f; *m=4+i*0; } }
  return d*0.9f;
}
static float h_leather(F3 p){ F3 q=ob(p); return fbm3(mul(q,7.0f),3)*0.3f; }
static void mat_botte(F3 p,F3 *n,int m,Mat *o){
  F3 q=ob(p);
  if(m==4){ float ang=atan2f(q.y-0.08f,-(q.x+0.20f)); float k=(ang-0.30f)/0.33f; float fk=k-floorf(k+0.5f); float quill=sst(0.06f,0.0f,fabsf(fk));
    float vane=fabsf(sinf((q.x-q.y)*70.0f)); float tip=sst(0.38f,0.72f,len(sub(q,V(-0.16f,0.06f,0.21f))));
    o->aoc=0.75f+0.25f*sst(0.5f,0.3f,fabsf(fk));
    o->alb=mix3(V(0.80f,0.78f,0.74f),V(0.97f,0.96f,0.93f),vane); o->alb=mix3(o->alb,V(0.95f,0.72f,0.30f),tip*0.6f); o->alb=mix3(o->alb,V(0.55f,0.50f,0.45f),quill*0.6f);
    o->rough=0.5f; o->sss=0.6f; o->sssc=V(1.0f,0.95f,0.85f); o->emit=mul(V(0.10f,0.09f,0.07f),1-tip); return; }
  if(m==3){ o->metal=1; o->alb=V(0.95f,0.72f,0.32f); o->rough=0.18f; return; }
  if(m==1){ o->alb=V(0.06f,0.04f,0.035f); o->rough=0.6f; return; }
  *n=bump(p,*n,h_leather,0.02f,0.004f);
  float g=fbm3(mul(q,2.5f),4);
  o->alb=mix3(V(0.30f,0.12f,0.04f),V(0.48f,0.24f,0.09f),sat(0.55f+g)); o->rough=0.42f; o->clear=0.35f;
  /* couture le long du pied */
  float st=fabsf(q.y+0.46f); float dash=0.5f+0.5f*sinf(q.x*90.0f);
  o->alb=mix3(o->alb,V(0.85f,0.70f,0.45f),sst(0.012f,0.004f,st)*sst(0.3f,0.7f,dash));
  if(m==2) o->alb=mul(o->alb,0.78f);
}

/* --- coeur de pierre : granit fendu, une braise dans les fissures --- */
static float sdf_cpierre(F3 p,int *m){ *m=0; F3 q=ob(p); float d=sd_heart3(q,0.98f,0.30f); d+=fbm3(mul(q,4.0f),3)*0.02f; return d; }
static float h_granit(F3 p){ F3 q=ob(p); Wor w=worley(mul(q,3.5f),0.9f); return sst(0.0f,0.06f,w.f2-w.f1)*0.4f+fbm3(mul(q,20.0f),2)*0.08f; }
static void mat_cpierre(F3 p,F3 *n,int m,Mat *o){ (void)m;
  F3 q=ob(p); Wor w=worley(mul(q,3.5f),0.9f); float crk=1-sst(0.0f,0.045f,w.f2-w.f1);
  *n=bump(p,*n,h_granit,0.35f,0.004f);
  float sp=pn(mul(q,45.0f)), g=fbm3(mul(q,3.0f),4);
  o->alb=mix3(V(0.20f,0.19f,0.18f),V(0.40f,0.38f,0.36f),sat(0.5f+g)); if(sp>0.45f) o->alb=V(0.08f,0.075f,0.07f); if(sp<-0.55f) o->alb=V(0.62f,0.60f,0.58f);
  o->rough=0.75f; o->alb=mix3(o->alb,V(0.05f,0.03f,0.03f),crk*0.7f);
  o->emit=mul(V(1.4f,0.20f,0.06f),crk*0.55f*sat(0.4f+fbm3(mul(q,2.0f),3)));
}

/* --- ressort : spirale d'acier entre deux plaques --- */
static float sdf_ressort(F3 p,int *m){
  F3 q=ob(p); *m=0;
  const float R=0.34f, P=0.20f, rw=0.052f;
  float r=sqrtf(q.x*q.x+q.z*q.z); float th=atan2f(q.z,q.x)/TAU;
  float k=roundf(q.y/P-th); float yh=(k+th)*P; float yy=q.y-yh;
  if(yh>0.52f) yy=q.y-0.52f;
  if(yh<-0.52f) yy=q.y+0.52f;
  float dc=sqrtf((r-R)*(r-R)+yy*yy*0.97f)-rw;
  dc=smax(dc,fabsf(q.y)-0.56f,0.02f);
  float pl1=sd_cyl(sub(q,V(0,0.62f,0)),0.44f,0.045f,0.02f), pl2=sd_cyl(sub(q,V(0,-0.62f,0)),0.44f,0.045f,0.02f);
  float d=dc; float pl=fminf(pl1,pl2); if(pl<d){ d=pl; *m=1; }
  return d*0.8f;
}
static void mat_ressort(F3 p,F3 *n,int m,Mat *o){ (void)n;
  F3 q=ob(p);
  if(m==1){ float g=fbm3(mul(q,5),3); o->metal=1; o->alb=V(0.34f,0.30f,0.27f); o->rough=0.35f+0.1f*g; return; }
  o->metal=1; o->alb=V(0.78f,0.80f,0.86f); o->rough=0.12f;
}

/* --- sac de billes : toile nouee, billes de verre qui s'echappent --- */
static float sdf_sac(F3 p,int *m){
  F3 q=ob(p); *m=0;
  float body=sd_ell(sub(q,V(0,-0.22f,0)),V(0.50f,0.46f,0.44f));
  float neck=sd_cap2(q,V(0,0.10f,0),V(0,0.32f,0),0.20f,0.12f); float d=smin(body,neck,0.12f);
  float ang=atan2f(q.z,q.x); float ruf=sd_cap2(q,V(0,0.30f,0),V(0,0.52f,0),0.11f,0.24f+0.03f*sinf(ang*7)); ruf=smax(ruf,-sd_cap2(q,V(0,0.36f,0),V(0,0.60f,0),0.06f,0.19f),0.01f);
  d=smin(d,ruf,0.04f);
  d+=fbm3(mul(q,5.0f),3)*0.015f+0.008f*sinf(ang*9+q.y*6)*sat(q.y+0.2f);
  float rope=sd_tor(sub(q,V(0,0.29f,0)),0.135f,0.032f); if(rope<d){ d=rope; *m=1; }
  static const float MB[3][4]={{0.42f,-0.62f,0.30f,0.13f},{0.12f,-0.72f,0.45f,0.11f},{-0.08f,0.50f,0.02f,0.10f}};
  for(int i=0;i<3;i++){ float b=sd_sph(q,V(MB[i][0],MB[i][1],MB[i][2]),MB[i][3]); if(b<d){ d=b; *m=2+i; } }
  return d;
}
static float h_weave(F3 p){ F3 q=ob(p); return (sinf(q.x*55.0f+q.z*55.0f)*sinf(q.y*55.0f))*0.3f+fbm3(mul(q,6.0f),2)*0.8f; }
static void mat_sac(F3 p,F3 *n,int m,Mat *o){
  F3 q=ob(p);
  if(m>=2){ static const float MC[3][3]={{0.10f,0.35f,0.85f},{0.85f,0.20f,0.10f},{0.15f,0.65f,0.25f}}; F3 c=V(MC[m-2][0],MC[m-2][1],MC[m-2][2]);
    F3 lq=sub(q,V(0,0,0)); float sw=0.5f+0.5f*sinf(lq.x*40+lq.y*25);
    o->alb=mix3(mul(c,0.5f),c,sw); o->rough=0.04f; o->clear=1; o->sss=0.8f; o->sssc=c; o->spec=0.06f; return; }
  if(m==1){ o->alb=V(0.55f,0.42f,0.24f); o->rough=0.8f; float tw=0.5f+0.5f*sinf(atan2f(q.z,q.x)*30+q.y*60); o->alb=mul(o->alb,0.75f+0.35f*tw); return; }
  *n=bump(p,*n,h_weave,0.012f,0.003f);
  float g=fbm3(mul(q,3.0f),4);
  o->alb=mix3(V(0.30f,0.18f,0.08f),V(0.48f,0.32f,0.15f),sat(0.5f+g)); o->rough=0.85f;
}

/* --- oeil de lynx : iris d'ambre, pupille fendue, fourrure autour --- */
static void mat_lynx(F3 p,F3 *n,int m,Mat *o){ (void)m;
  F3 q=bq(p);
  float ex=q.x/0.95f, ey=q.y/0.58f; float lid=ex*ex+ey*ey*(1+0.35f*ex*ex);
  if(lid>1.0f){   /* fourrure fauve rayee */
    float st=fbm3(V(q.x*3,q.y*14,q.z*3),4); float fur=fbm3(mul(q,30.0f),2);
    *n=nrm(add(*n,mul(V(fur,fbm3(mul(q,31.0f),2),0),0.5f)));
    o->alb=mix3(V(0.42f,0.24f,0.10f),V(0.08f,0.05f,0.03f),sst(0.05f,0.35f,st)); o->rough=0.8f;
    float liner=sst(1.18f,1.0f,lid); o->alb=mix3(o->alb,V(0.02f,0.015f,0.01f),liner); return;
  }
  float r=sqrtf(q.x*q.x+q.y*q.y); float ang=atan2f(q.y,q.x);
  float fib_=0.5f+0.5f*pn(V(cosf(ang)*9,sinf(ang)*9,r*5));
  float pupil=fabsf(q.x)<0.075f*sqrtf(sat(1-(q.y/0.52f)*(q.y/0.52f)))+0.005f ? 1.0f:0.0f;
  F3 iris=mix3(V(0.85f,0.55f,0.06f),V(0.45f,0.60f,0.10f),sst(0.45f,0.15f,r));
  iris=mul(iris,0.55f+0.6f*fib_);
  iris=mix3(iris,V(0.12f,0.05f,0.01f),sst(0.40f,0.55f,r));
  o->alb=iris; o->rough=0.04f; o->clear=1.0f; o->spec=0.05f;
  if(pupil>0.5f){ o->alb=V(0.005f,0.004f,0.004f); }
  o->emit=mul(V(0.30f,0.18f,0.02f),(1-pupil)*sst(0.5f,0.1f,r)*0.5f);
  float ldk=sst(0.80f,1.0f,lid); o->alb=mul(o->alb,1-0.6f*ldk);
}

/* --- lanterne : fer forge, vitres, flamme --- */
static float sdf_lant(F3 p,int *m){
  F3 q=ob(p); *m=0; float d=9;
  d=sd_rbox(q,V(0,-0.58f,0),V(0.33f,0.06f,0.33f),0.02f);
  for(int i=0;i<4;i++){ float sx=(i&1)?0.27f:-0.27f, sz=(i&2)?0.27f:-0.27f; d=fminf(d,sd_rbox(q,V(sx,-0.08f,sz),V(0.035f,0.46f,0.035f),0.01f)); }
  d=fminf(d,sd_rbox(q,V(0,0.40f,0),V(0.33f,0.05f,0.33f),0.02f));
  float roof=sd_cap2(q,V(0,0.44f,0),V(0,0.72f,0),0.40f,0.05f); roof=smax(roof,-(q.y-0.44f),0.01f); d=fminf(d,roof);
  float ring=sd_tor(rotx(sub(q,V(0,0.84f,0)),PI*0.5f),0.10f,0.025f); d=fminf(d,ring);
  float cand=sd_cyl(sub(q,V(0,-0.40f,0)),0.075f,0.12f,0.02f); if(cand<d){ d=cand; *m=2; }
  float fl=sd_ell(sub(q,V(0,-0.12f,0)),V(0.065f,0.14f,0.065f)); if(fl<d){ d=fl; *m=3; }
  if(!g_noglass){ float gl=fabsf(sd_rbox(q,V(0,-0.08f,0),V(0.25f,0.44f,0.25f),0.0f))-0.004f; if(gl<d){ d=gl; *m=1; } }
  return d;
}
static void mat_lant(F3 p,F3 *n,int m,Mat *o){
  F3 q=ob(p);
  if(m==1){ o->tglass=1; o->tint=V(1.0f,0.92f,0.78f); return; }
  if(m==2){ o->alb=V(0.85f,0.80f,0.68f); o->rough=0.5f; o->sss=1.0f; o->sssc=V(1.0f,0.6f,0.3f); o->emit=V(0.25f,0.12f,0.04f); return; }
  if(m==3){ float t=sat((q.y+0.26f)/0.28f); o->alb=V(1,1,1); o->emit=mix3(V(2.2f,1.5f,0.6f),V(1.6f,0.55f,0.08f),t); return; }
  float g=fbm3(mul(q,7),3); *n=bump(p,*n,h_dent,0.03f,0.004f);
  o->metal=1; o->alb=mix3(V(0.22f,0.17f,0.12f),V(0.38f,0.28f,0.16f),sat(0.5f+g)); o->rough=0.40f+0.15f*g;
  /* reflet chaud de la flamme sur le metal */
  float df=len(sub(q,V(0,-0.12f,0))); o->emit=mul(V(0.6f,0.3f,0.08f),expf(-df*4.0f)*0.4f);
}
static void fx_lant(int L,float x,float y,F3 *c,float *a){
  if(L==1) return;
  float d=sqrtf(x*x+(y+0.10f)*(y+0.10f)); float g=expf(-d/0.22f)*0.5f;
  addl(c,mul(V(1.3f,0.75f,0.25f),g)); *a=fmaxf(*a,g*0.35f);
}

/* --- mousse : pierre couverte d'un coussin de mousse, jeune pousse et rosee --- */
static float sdf_mousse(F3 p,int *m){
  F3 q=ob(p); *m=0;
  float rock=sd_ell(sub(q,V(0,-0.38f,0)),V(0.66f,0.36f,0.52f))+fbm3(mul(q,3.0f),3)*0.05f;
  float moss=sd_ell(sub(q,V(0,-0.22f,0)),V(0.60f,0.30f,0.48f))+fbm3(mul(q,9.0f),3)*0.035f-0.02f; moss=smax(moss,-(q.y+0.30f),0.08f);
  float d=rock; if(moss<d){ d=smin(d,moss,0.02f); *m=1; } else d=smin(d,moss,0.02f);
  float st=sd_cap2(q,V(0.05f,0.02f,0.05f),V(0.02f,0.42f,0.08f),0.025f,0.018f); if(st<d){ d=st; *m=2; }
  F3 l1=rotz(sub(q,V(-0.13f,0.44f,0.08f)),0.5f); float lf1=sd_ell(l1,V(0.16f,0.05f,0.10f));
  F3 l2=rotz(sub(q,V(0.17f,0.47f,0.08f)),-0.4f); float lf2=sd_ell(l2,V(0.15f,0.05f,0.09f));
  float lf=fminf(lf1,lf2); if(lf<d){ d=lf; *m=3; }
  float dew=sd_sph(q,V(-0.30f,0.04f,0.36f),0.06f); if(dew<d){ d=dew; *m=4; }
  return d*0.85f;
}
static void mat_mousse(F3 p,F3 *n,int m,Mat *o){
  F3 q=ob(p);
  if(m==4){ o->alb=V(0.6f,0.8f,0.7f); o->rough=0.02f; o->clear=1; o->spec=0.03f; o->sss=1.0f; o->sssc=V(0.5f,0.9f,0.7f); o->emit=V(0.03f,0.06f,0.05f); return; }
  if(m==2){ o->alb=V(0.20f,0.40f,0.08f); o->rough=0.5f; o->sss=0.6f; o->sssc=V(0.4f,0.9f,0.2f); return; }
  if(m==3){ o->alb=V(0.10f,0.45f,0.06f); o->rough=0.35f; o->sss=1.0f; o->sssc=V(0.45f,1.0f,0.15f); o->clear=0.4f; return; }
  if(m==1){ float f=fbm3(mul(q,25.0f),3); *n=nrm(add(*n,mul(V(f,fbm3(mul(q,26.0f),2),f*0.5f),0.6f)));
    o->alb=mix3(V(0.05f,0.18f,0.02f),V(0.22f,0.42f,0.06f),sat(0.5f+f)); o->rough=0.9f; o->sss=0.5f; o->sssc=V(0.3f,0.8f,0.1f); return; }
  float g=fbm3(mul(q,5),4); o->alb=mix3(V(0.16f,0.15f,0.14f),V(0.32f,0.30f,0.28f),sat(0.5f+g)); o->rough=0.85f;
}

/* --- filet : flotteur de verre vert pris dans un filet de corde --- */
static float sdf_filet(F3 p,int *m){
  F3 q=ob(p); F3 c=V(0,-0.08f,0); F3 v=sub(q,c); float r=len(v); *m=0;
  float d=r-0.60f;
  /* mailles en losange sur la sphere */
  float th=atan2f(v.z,v.x), ph=acosf(clampf(v.y/fmaxf(r,1e-4f),-1,1));
  const float K=6.0f; float u=(th*K/TAU)+ph*2.2f, w=(th*K/TAU)-ph*2.2f;
  float du=fabsf(u-roundf(u)), dw=fabsf(w-roundf(w));
  float ang=fminf(du,dw)*TAU/K*0.62f*sinf(fmaxf(ph,0.2f));
  float net=sqrtf(ang*ang*r*r+(r-0.635f)*(r-0.635f))-0.022f;
  net=smax(net,-(v.y+0.50f),0.01f);
  if(net<d){ d=net; *m=1; }
  float knot=sd_sph(q,add(c,V(0,0.64f,0)),0.07f); if(knot<d){ d=knot; *m=1; }
  float loop=sd_tor(rotx(sub(q,add(c,V(0,0.80f,0))),PI*0.5f),0.10f,0.024f); if(loop<d){ d=loop; *m=1; }
  return d*0.8f;
}
static void mat_filet(F3 p,F3 *n,int m,Mat *o){ (void)n;
  F3 q=ob(p);
  if(m==1){ float tw=0.5f+0.5f*sinf((q.x+q.y+q.z)*120.0f); o->alb=mix3(V(0.40f,0.30f,0.16f),V(0.62f,0.50f,0.30f),tw); o->rough=0.85f; return; }
  float g=fbm3(mul(q,3),3);
  o->alb=V(0.04f,0.30f,0.20f); o->rough=0.05f; o->clear=1; o->spec=0.06f; o->sss=1.0f; o->sssc=V(0.15f,0.75f,0.45f);
  o->emit=mul(V(0.02f,0.12f,0.08f),0.5f+0.5f*g);
}

/* --- trefle : quatre feuilles en coeur, tige, goutte de rosee --- */
static float sdf_trefle(F3 p,int *m){
  F3 q=ob(p); *m=0; float d=9;
  for(int i=0;i<4;i++){ float a=i*PI*0.5f+PI*0.25f; F3 l=rotz(q,-a+PI*0.5f);   /* feuille vers +y */
    float d2=sd_heart2(l.x/0.42f,(l.y-0.03f)/0.42f)*0.42f;   /* pointe vers le centre, lobes vers l'exterieur */
    float rr2=l.x*l.x+l.y*l.y; float z=l.z+rr2*0.35f-0.02f;
    float lf=smax(d2,fabsf(z)-0.022f,0.02f); if(lf<d){ d=lf; *m=10+i; } }
  float st=sd_cap2(q,V(0,0,-0.02f),V(0.20f,-0.62f,-0.04f),0.035f,0.025f); if(st<d){ d=smin(d,st,0.02f); *m=1; }
  float dew=sd_sph(q,V(0.18f,0.30f,0.07f),0.055f); if(dew<d){ d=dew; *m=2; }
  return d*0.85f;
}
static void mat_trefle(F3 p,F3 *n,int m,Mat *o){ (void)n;
  F3 q=ob(p);
  if(m==2){ o->alb=V(0.6f,0.8f,0.65f); o->rough=0.02f; o->clear=1; o->sss=1.0f; o->sssc=V(0.5f,1.0f,0.6f); o->emit=V(0.04f,0.08f,0.05f); return; }
  if(m==1){ o->alb=V(0.12f,0.30f,0.05f); o->rough=0.5f; o->sss=0.5f; o->sssc=V(0.4f,0.9f,0.2f); return; }
  int i=m-10; float a=i*PI*0.5f+PI*0.25f; F3 l=rotz(q,-a+PI*0.5f);
  float r=sqrtf(l.x*l.x+l.y*l.y);
  float chev=fabsf(l.y-0.20f-fabsf(l.x)*0.55f); float mark=sst(0.035f,0.008f,chev)*sst(0.10f,0.20f,r);
  float vein=sst(0.012f,0.0f,fabsf(l.x))*sst(0.05f,0.12f,r);
  o->alb=mix3(V(0.04f,0.20f,0.03f),V(0.10f,0.36f,0.05f),sat(r*2.2f)); o->alb=mix3(o->alb,V(0.32f,0.55f,0.22f),mark*0.8f); o->alb=mix3(o->alb,V(0.18f,0.42f,0.10f),vein*0.7f);
  o->rough=0.30f; o->clear=0.5f; o->sss=0.7f; o->sssc=V(0.35f,0.95f,0.15f);
}
static void setup_trefle(void){ uint32_t s=2; NSPK=0; for(int i=0;i<5;i++){ float a=rr(&s,0,TAU), r=rr(&s,0.62f,0.85f); spk_add(cosf(a)*r,sinf(a)*r,rr(&s,0.008f,0.013f),V(1.9f,1.6f,0.6f),rr(&s,0.5f,0.9f)); } }
static void fx_trefle(int L,float x,float y,F3 *c,float *a){ if(L==1) spk_eval(x,y,c,a); }

/* --- bouclier runique : acier sombre, bordure de laiton, rune ardente --- */
static float shield2(float x,float y){   /* ecu : haut plat, pointe en bas */
  float top=y-0.62f;
  float side=sqrtf((fabsf(x)+0.55f)*(fabsf(x)+0.55f)+(y-0.30f)*(y-0.30f))-1.18f;
  return fmaxf(top,side);
}
static float sdf_bouc(F3 p,int *m){
  F3 q=ob(p); *m=0;
  float d2=shield2(q.x,q.y);
  float bulge=0.10f*sqrtf(sat(1-(q.x*q.x+(q.y-0.0f)*(q.y-0.0f))/0.9f));
  float body=smax(d2,fabsf(q.z-bulge*0.5f)-0.06f-bulge*0.5f,0.03f);
  float rim=smax(fabsf(d2+0.04f)-0.045f,fabsf(q.z-bulge*0.2f)-0.10f,0.02f);
  float d=body; if(rim<d){ d=smin(d,rim,0.01f); *m=1; }
  return d*0.9f;
}
static float rune(float x,float y){   /* rune en forme de fleche a deux branches (generique) */
  float a=sd_cap(V(x,y,0),V(0,-0.36f,0),V(0,0.34f,0),0.035f);
  float b=sd_cap(V(x,y,0),V(0,0.02f,0),V(-0.20f,0.24f,0),0.035f);
  float c=sd_cap(V(x,y,0),V(0,0.02f,0),V(0.20f,0.24f,0),0.035f);
  float e=sd_cap(V(x,y,0),V(0,-0.22f,0),V(-0.14f,-0.36f,0),0.035f);
  float f=sd_cap(V(x,y,0),V(0,-0.22f,0),V(0.14f,-0.36f,0),0.035f);
  return fminf(fminf(a,fminf(b,c)),fminf(e,f));
}
static void mat_bouc(F3 p,F3 *n,int m,Mat *o){ (void)n;
  F3 q=ob(p);
  if(m==1){ o->metal=1; o->alb=V(0.85f,0.62f,0.30f); o->rough=0.25f; float rv=fabsf(sinf(atan2f(q.y,q.x)*14)); o->aoc=0.7f+0.3f*sst(0.1f,0.3f,rv); return; }
  float g=fbm3(V(q.x*2,q.y*30,q.z*2),3);
  o->metal=1; o->alb=V(0.20f,0.24f,0.30f); o->rough=0.30f+0.12f*g;
  float r=rune(q.x,q.y-0.02f);
  float core=sst(0.006f,-0.01f,r), halo=expf(-fmaxf(r,0)/0.05f);
  o->alb=mix3(o->alb,V(0.02f,0.03f,0.04f),core); o->rough=mixf(o->rough,0.6f,core);
  o->emit=add(mul(V(0.30f,1.30f,2.0f),core*1.1f),mul(V(0.10f,0.40f,0.80f),halo*0.35f));
}

/* --- coeur enrage : coeur rouge sombre, veines de braise, flammes --- */
static float sdf_crage(F3 p,int *m){ *m=0; F3 q=ob(p); float d=sd_heart3(q,0.92f,0.36f); d+=fbm3(mul(q,3.0f),3)*0.015f; return d; }
static void mat_crage(F3 p,F3 *n,int m,Mat *o){ (void)n; (void)m;
  F3 q=ob(p); float v=ridged(add(mul(q,3.2f),V(4,1,7)),4);
  float vein=sst(0.62f,0.92f,v);
  o->alb=mix3(V(0.16f,0.008f,0.012f),V(0.30f,0.02f,0.03f),sat(fbm3(mul(q,3.0f),3)+0.5f)); o->rough=0.15f; o->clear=0.8f; o->sss=0.5f; o->sssc=V(1.0f,0.08f,0.05f);
  o->alb=mix3(o->alb,V(0.03f,0.005f,0.005f),vein*0.8f);
  o->emit=mul(fire_ramp(0.35f+0.5f*vein),vein*0.75f);
}
static void fx_crage(int L,float x,float y,F3 *c,float *a){
  if(L==1) return;
  float f=0;
  for(int s2=-1;s2<=1;s2+=2){ float cx=s2*0.24f, cy=0.18f; f=fmaxf(f,flames(x,y,cx,cy,0.30f,0.42f,s2*3.0f)); }
  float d=sd_heart2(x/0.92f,(y+0.62f*0.92f)/0.92f)*0.92f; f*=sat(d/0.02f+1.0f);
  if(f>0.002f){ F3 col=fire_ramp(0.1f+0.9f*f); over(c,a,mul(col,1.0f/fmaxf(maxc(col),1.0f)),sat(f*1.6f)*0.95f); addl(c,mul(col,f*f*0.4f)); }
}

/* --- fiole d'alchimiste : ballon de verre, potion violette lumineuse, bouchon --- */
static float sdf_fiole(F3 p,int *m){
  F3 q=ob(p); *m=0;
  float liq=smax(sd_sph(q,V(0,-0.26f,0),0.385f),q.y+0.10f+0.02f*sinf(q.x*14),0.01f);
  float d=liq; *m=2;
  for(int i=0;i<4;i++){ float b=sd_sph(q,V(-0.12f+i*0.09f,-0.30f+0.09f*(i&1)-0.05f*i,0.20f),0.025f+0.008f*(i&1)); if(b<d){ d=b; *m=3; } }
  float cork=sd_cyl(sub(q,V(0,0.66f,0)),0.105f,0.10f,0.02f); if(cork<d){ d=cork; *m=4; }
  float lip=sd_tor(sub(q,V(0,0.54f,0)),0.13f,0.028f); if(lip<d){ d=lip; *m=1; }
  if(!g_noglass){
    float bul=sd_sph(q,V(0,-0.26f,0),0.42f), nk=sd_cyl(sub(q,V(0,0.28f,0)),0.12f,0.28f,0.0f);
    float gl=fabsf(smin(bul,nk,0.08f))-0.007f; gl=smax(gl,q.y-0.56f,0.01f);
    if(gl<d){ d=gl; *m=1; }
  }
  return d;
}
static void mat_fiole(F3 p,F3 *n,int m,Mat *o){ (void)n;
  F3 q=ob(p);
  if(m==1){ o->tglass=1; o->tint=V(0.95f,0.97f,1.0f); return; }
  if(m==4){ float g=pn(mul(q,60)); o->alb=V(0.45f+0.05f*g,0.30f,0.16f); o->rough=0.9f; return; }
  float g=fbm3(mul(q,4),3);
  o->alb=V(0.30f,0.05f,0.45f); o->rough=0.06f; o->sss=1.2f; o->sssc=V(0.8f,0.25f,1.0f);
  o->emit=mul(V(0.55f,0.12f,0.85f),0.55f+0.45f*g);
  if(m==3){ o->emit=V(1.2f,0.6f,1.6f); }
}
static void fx_fiole(int L,float x,float y,F3 *c,float *a){
  if(L==1) return;
  float d=sqrtf(x*x+(y+0.26f)*(y+0.26f))-0.40f; float g=expf(-fmaxf(d,0)/0.10f)*0.35f;
  addl(c,mul(V(0.6f,0.15f,1.0f),g)); *a=fmaxf(*a,g*0.3f);
}

/* --- miroir : cadre dore cisele, glace argentee, manche --- */
static float sdf_miroir(F3 p,int *m){
  F3 q=ob(p); *m=0;
  F3 c=sub(q,V(0,0.18f,0));
  float e2=sqrtf((c.x/0.46f)*(c.x/0.46f)+(c.y/0.54f)*(c.y/0.54f));
  float rim2=(e2-1)*0.50f;
  float frame=smax(fabsf(rim2+0.035f)-0.045f,fabsf(c.z)-0.045f-0.012f*sinf(atan2f(c.y,c.x)*12),0.02f);
  float glass=smax(rim2+0.06f,fabsf(c.z+0.005f)-0.015f,0.01f);
  float back=smax(rim2,fabsf(c.z+0.03f)-0.02f,0.01f);
  float d=frame; *m=1;
  if(glass<d){ d=glass; *m=2; }
  if(back<d){ d=back; *m=1; }
  float hd=sd_cap2(q,V(0,-0.38f,0),V(0,-0.86f,0),0.065f,0.05f); if(hd<d){ d=smin(d,hd,0.04f); *m=1; }
  for(int i=0;i<2;i++){ float rg=sd_tor(sub(q,V(0,-0.50f-i*0.22f,0)),0.07f,0.022f); if(rg<d){ d=rg; *m=1; } }
  float gem=sd_sph(q,V(0,0.18f+0.54f+0.02f,0.03f),0.05f); if(gem<d){ d=gem; *m=3; }
  return d*0.9f;
}
static void mat_miroir(F3 p,F3 *n,int m,Mat *o){ (void)n;
  F3 q=ob(p);
  if(m==3){ o->alb=V(0.6f,0.05f,0.15f); o->rough=0.03f; o->clear=1; o->sss=1; o->sssc=V(1.0f,0.1f,0.3f); o->emit=V(0.25f,0.02f,0.06f); return; }
  if(m==1){ float g=fbm3(mul(q,9),3); o->metal=1; o->alb=V(0.95f,0.70f,0.32f); o->rough=0.20f+0.12f*g; o->aoc=0.8f+0.2f*sat(g+0.5f); return; }
  o->metal=1; o->alb=V(0.90f,0.92f,0.96f); o->rough=0.02f;
  float sh=sat(0.5f+0.6f*(q.x*0.5f+q.y)); float streak_=expf(-powf((q.x+q.y*0.6f-0.05f)/0.07f,2))+0.6f*expf(-powf((q.x+q.y*0.6f+0.16f)/0.03f,2));
  o->emit=add(mul(V(0.22f,0.30f,0.42f),0.25f+0.45f*sh),mul(V(0.9f,0.95f,1.0f),streak_*0.55f));
}


/* ======================================================================= */
/*                      v7 : billes et evolutions nouvelles                */
/* ======================================================================= */
/* ondes sonores : arcs concentriques de part et d'autre de la bille */
static float sound_arcs(float x,float y,float r0,float step,int n,float spread){
  float dx=x, dy=y-BALL_C, d=sqrtf(dx*dx+dy*dy); if(d<1e-4f) return 0;
  float ang=fabsf(atan2f(dy,fabsf(dx)));     /* 0 : horizontale */
  float aw=sat(1-ang/spread); aw*=aw;
  float s=0;
  for(int k=0;k<n;k++){ float rk=r0+step*k; float e=(d-rk)/0.014f; s+=expf(-e*e)*(1.0f-0.22f*k); }
  return s*aw;
}
/* --- echo : sphere de bronze violet gravee de sillons, ondes autour --- */
static float h_echo(F3 p){ F3 q=bq(p); float r=sqrtf(q.x*q.x+q.y*q.y); return 0.5f+0.5f*cosf(r*30.0f)*sat(q.z*3); }
static void mat_echo(F3 p,F3 *n,int m,Mat *o){ (void)m;
  F3 q=bq(p); *n=bump(p,*n,h_echo,0.05f,0.004f);
  float r=sqrtf(q.x*q.x+q.y*q.y), g=0.5f+0.5f*cosf(r*30.0f);
  float pat=fbm3(mul(q,3.0f),3);
  o->metal=0.9f; o->alb=mix3(V(0.22f,0.18f,0.62f),V(0.46f,0.40f,0.95f),sat(0.5f+pat)); o->rough=0.16f+0.12f*g;
  o->emit=mul(V(0.10f,0.07f,0.38f),(1-g)*sat(q.z*2)*0.8f);
}
static void fx_echo(int L,float x,float y,F3 *c,float *a){
  if(L==1){ spk_eval(x,y,c,a); return; }
  float s=sound_arcs(x,y,BALL_R+0.10f,0.11f,3,1.0f);
  if(s>0.003f){ addl(c,mul(V(0.55f,0.48f,1.25f),s)); *a=fmaxf(*a,sat(s*0.9f)); }
  float d=dball(x,y); float g=expf(-fmaxf(d,0)/0.06f)*0.22f; addl(c,mul(V(0.35f,0.25f,1.0f),g)); *a=fmaxf(*a,g*0.4f);
}
/* --- doree : or poli cercle d'une bande crenelee, etincelles --- */
static void setup_doree(void){ uint32_t s=88; NSPK=0;
  for(int i=0;i<9;i++){ float ang=rr(&s,0,TAU), r=rr(&s,0.78f,1.0f); spk_add(cosf(ang)*r,BALL_C+sinf(ang)*r,rr(&s,0.006f,0.012f),V(2.0f,1.6f,0.7f),rr(&s,0.4f,0.9f)); } }
static float sdf_doree(F3 p,int *m){
  F3 v=sub(p,BC); float d=len(v)-BALL_R; *m=0;
  F3 w=rotx(rotz(v,0.35f),0.55f);
  float band=sd_tor(w,BALL_R*0.985f,0.045f); if(band<d+0.01f) *m=1; d=smin(d,band,0.02f);
  return d;
}
static void mat_doree(F3 p,F3 *n,int m,Mat *o){ (void)n;
  F3 q=bq(p); float g=fbm3(mul(q,7),3);
  o->metal=1; o->alb=V(1.0f,0.76f,0.30f); o->rough=0.08f+0.06f*g; o->emit=V(0.09f,0.055f,0.008f);
  if(m==1){ F3 w=rotx(rotz(q,0.35f),0.55f); float notch=0.5f+0.5f*cosf(atan2f(w.z,w.x)*22.0f); o->alb=V(0.95f,0.66f,0.24f); o->rough=0.22f+0.15f*notch; o->aoc=0.75f+0.25f*notch; }
}
static void fx_doree(int L,float x,float y,F3 *c,float *a){
  if(L==0){ float d=dball(x,y); float g=expf(-fmaxf(d,0)/0.07f)*0.25f; addl(c,mul(V(1.2f,0.8f,0.2f),g)); *a=fmaxf(*a,g*0.4f); return; }
  spk_eval(x,y,c,a);
  float st=star4(x,y,-0.33f,BALL_C+0.38f,0.20f)*0.9f+star4(x,y,0.48f,BALL_C-0.18f,0.12f)*0.6f;
  if(st>0.01f){ addl(c,mul(V(2.0f,1.7f,1.0f),st)); *a=fmaxf(*a,sat(st)); }
}
/* --- racines : bois veine enserre de racines, quelques feuilles --- */
static F3 RAX[4]; static float RAN[4];
static void setup_racine(void){ RAX[0]=nrm(V(0.3f,1,0.2f)); RAX[1]=nrm(V(1,0.2f,-0.4f)); RAX[2]=nrm(V(-0.6f,0.4f,1)); RAX[3]=nrm(V(0.2f,-0.9f,0.6f)); for(int i=0;i<4;i++) RAN[i]=0.4f+i*1.3f; NTH=0; }
static float vine_d(F3 v,int i,float thick){   /* racine : grand cercle de la sphere, legerement ondule */
  F3 ax=RAX[i]; float h=dot(v,ax); F3 pr=sub(v,mul(ax,h)); float rr_=len(pr);
  F3 u=nrm(cross(ax,V(0.3f,0.5f,0.8f))), w=cross(ax,u); float ang=atan2f(dot(pr,w),dot(pr,u));
  float wob=0.035f*sinf(ang*5.0f+RAN[i])+0.02f*sinf(ang*11.0f+i);
  return sqrtf(sqrf(rr_-BALL_R*1.0f)+sqrf(h-wob))-thick*(0.75f+0.25f*sinf(ang*3+i));
}
static float sdf_racine(F3 p,int *m){
  F3 v=sub(p,BC); float d=len(v)-BALL_R*0.97f; *m=0;
  for(int i=0;i<3;i++){ float r=vine_d(v,i,0.042f); if(r<d){ *m=1; } d=smin(d,r,0.025f); }
  static const float LF[3][4]={{-0.30f,0.62f,0.30f,0.6f},{0.42f,0.50f,0.25f,-0.5f},{0.05f,0.70f,-0.05f,0.2f}};
  for(int i=0;i<3;i++){ F3 c=V(LF[i][0],BALL_C+LF[i][1],LF[i][2]); F3 q=rotz(sub(p,c),LF[i][3]); float lf=sd_ell(q,V(0.17f,0.045f,0.11f)); if(lf<d){ d=lf; *m=2; } }
  return d;
}
static float h_wood7(F3 p){ F3 q=bq(p); return 0.5f+0.5f*sinf(q.y*26.0f+fbm3(mul(q,3),3)*5.0f); }
static void mat_racine(F3 p,F3 *n,int m,Mat *o){
  F3 q=bq(p);
  if(m==2){ o->alb=V(0.10f,0.32f,0.05f); o->rough=0.4f; o->sss=0.6f; o->sssc=V(0.3f,0.9f,0.1f); o->clear=0.3f; return; }
  if(m==1){ *n=bump(p,*n,h_bark,0.22f,0.004f); float v=ridged(mul(q,6.0f),3); o->alb=mix3(V(0.10f,0.06f,0.03f),V(0.28f,0.17f,0.08f),sat(v)); o->rough=0.7f; return; }
  *n=bump(p,*n,h_wood7,0.05f,0.004f);
  float rings=h_wood7(p); o->alb=mix3(V(0.30f,0.17f,0.07f),V(0.55f,0.36f,0.17f),rings); o->rough=0.45f+0.2f*rings; o->clear=0.25f;
}
/* --- acide : globe de verre plein d'un liquide jaune-vert qui bouillonne --- */
static float sdf_acide(F3 p,int *m){
  *m=0; float d=len(sub(p,BC))-BALL_R;
  float g1=sd_cap2(p,V(0.08f,BALL_C-BALL_R+0.04f,0.22f),V(0.09f,BALL_C-BALL_R-0.14f,0.26f),0.06f,0.03f);
  float dr=sd_sph(p,V(0.095f,BALL_C-BALL_R-0.26f,0.27f),0.045f);
  if(g1<d){ *m=1; } d=smin(d,g1,0.05f); if(dr<d){ d=dr; *m=1; }
  return d;
}
static F3 vol_acide(F3 q,float *dens){
  float surf=0.22f+0.05f*sinf(q.x*7.0f+q.z*5.0f);
  if(q.y>surf){ float v=sat(fbm3(mul(q,3.0f),3)+0.3f); *dens=0.15f+v*0.4f; return mul(V(0.30f,0.45f,0.05f),v*0.25f); }
  Wor w=worley(add(mul(q,4.2f),V(0.3f,1.7f,0.9f)),0.9f);
  float bub=expf(-sqrf(w.f1-0.20f)*600.0f)*((w.id&3)!=0?1.0f:0.0f);
  float deep=sat((surf-q.y)*1.2f);
  float swirl=0.5f+0.5f*fbm3(mul(q,2.5f),4);
  *dens=1.4f+deep*1.2f;
  return add(mul(V(0.55f,0.95f,0.06f),(0.35f+0.65f*swirl)*(1.15f-0.5f*deep)),mul(V(1.6f,2.0f,0.8f),bub*1.8f));
}
static void mat_acide(F3 p,F3 *n,int m,Mat *o){ (void)p; (void)n;
  if(m==1){ o->alb=V(0.20f,0.40f,0.02f); o->rough=0.06f; o->clear=1; o->sss=0.6f; o->sssc=V(0.6f,1.0f,0.1f); o->emit=V(0.10f,0.20f,0.01f); return; }
  o->glass=1; o->ior=1.40f; o->absorb=V(0.6f,0.2f,1.6f); o->rough=0.03f;
}
static void fx_acide(int L,float x,float y,F3 *c,float *a){
  if(L==1){ spk_eval(x,y,c,a); return; }
  float d=dball(x,y); float g=expf(-fmaxf(d,0)/0.08f)*0.30f; addl(c,mul(V(0.6f,1.1f,0.1f),g)); *a=fmaxf(*a,g*0.45f);
  /* vapeur qui s'eleve */
  if(y>BALL_C+0.3f && d>-0.02f){ float n=fbm2(x*4.0f,y*2.5f-2.0f,4); float al=sat(1-d/0.30f)*sat(0.3f+n)*0.30f; if(al>0) over(c,a,V(0.55f,0.75f,0.25f),al); }
}
static void setup_acide(void){ uint32_t s=97; NSPK=0; for(int i=0;i<6;i++){ float ang=rr(&s,0.6f,2.6f), r=rr(&s,0.80f,1.0f); spk_add(cosf(ang)*r,BALL_C+sinf(ang)*r,rr(&s,0.007f,0.013f),V(1.2f,2.0f,0.4f),rr(&s,0.4f,0.8f)); } }
/* --- tonnerre : orage violet dans le verre, eclairs et ondes --- */
static void setup_tonnerre(void){
  uint32_t s=4711; NARC=0; NSPK=0;
  const float A[5]={-2.4f,-1.0f,0.6f,1.7f,2.7f};
  for(int i=0;i<5;i++){ float a=A[i]+rr(&s,-0.2f,0.2f); float r0=BALL_R*0.97f, r1=BALL_R+rr(&s,0.15f,0.26f); float b=a+rr(&s,-0.5f,0.5f);
    arc_build(sinf(a)*r0,BALL_C+cosf(a)*r0,sinf(b)*r1,BALL_C+cosf(b)*r1,0.55f,5,rr(&s,0.011f,0.016f),&s); }
}
static F3 vol_tonnerre(F3 q,float *dens){
  float ang=atan2f(q.z,q.x)+len(V(q.x,0,q.z))*2.2f;
  F3 sw=V(cosf(ang)*len(V(q.x,0,q.z)),q.y,sinf(ang)*len(V(q.x,0,q.z)));
  float cl=sat(fbm3(mul(sw,2.4f),5)*1.3f+0.25f);
  F3 w=V(fbm3(mul(q,1.6f),3),fbm3(add(mul(q,1.6f),V(5,1,3)),3),0);
  float bolt=powf(1-fabsf(pn(add(mul(q,1.5f),mul(w,1.0f)))),24.0f);
  *dens=0.5f+cl*2.5f;
  F3 cc=mix3(V(0.05f,0.02f,0.12f),V(0.34f,0.24f,0.62f),cl);
  return add(mul(cc,cl*0.6f),mul(V(1.6f,1.4f,2.6f),bolt*3.2f));
}
static void mat_tonnerre(F3 p,F3 *n,int m,Mat *o){ (void)p; (void)n; (void)m; o->glass=1; o->ior=1.45f; o->absorb=V(1.4f,1.8f,0.7f); o->rough=0.03f; }
static void fx_tonnerre(int L,float x,float y,F3 *c,float *a){
  if(L==0){ float s=sound_arcs(x,y,BALL_R+0.12f,0.10f,3,1.15f); if(s>0.003f){ addl(c,mul(V(0.75f,0.65f,1.4f),s)); *a=fmaxf(*a,sat(s*0.9f)); }
    float d=dball(x,y); float g=expf(-fmaxf(d,0)/0.07f)*0.30f; addl(c,mul(V(0.5f,0.4f,1.3f),g)); *a=fmaxf(*a,g*0.4f); return; }
  float w; float d=arc_dist(x,y,&w);
  float core=expf(-d*d*1.2f), halo=expf(-d*0.75f)*0.35f;
  if(core+halo>0.003f){ addl(c,add(mul(V(1.8f,1.7f,2.1f),core),mul(V(0.7f,0.4f,1.4f),halo))); *a=fmaxf(*a,sat(core+halo*0.5f)); }
}
/* --- midas : le fer se change en or ; une couture incandescente les separe --- */
static float midas_edge(F3 q){ return q.y+0.10f-0.22f*fbm3(mul(q,2.2f),3)-0.08f*sinf(q.x*6+q.z*4); }
static void mat_midas(F3 p,F3 *n,int m,Mat *o){ (void)m;
  F3 q=bq(p); float e=midas_edge(q);
  if(e>0){ float g=fbm3(mul(q,7),3); o->metal=1; o->alb=V(1.0f,0.76f,0.32f); o->rough=0.08f+0.06f*g; o->emit=V(0.08f,0.05f,0.008f); }
  else { *n=bump(p,*n,h_hammer,0.006f,0.004f); float sc=fbm3(V(q.x*6,q.y*45,q.z*6),3); o->metal=1; o->alb=V(0.55f,0.56f,0.60f); o->rough=0.14f+0.16f*sat(sc*0.6f+0.5f); }
  float seam=expf(-sqrf(e/0.035f)); o->emit=add(o->emit,mul(V(1.6f,1.0f,0.25f),seam*0.9f));
}
static void setup_midas(void){ uint32_t s=1234; NSPK=0;
  for(int i=0;i<12;i++){ float ang=rr(&s,0.2f,2.9f), r=rr(&s,0.80f,1.05f); spk_add(cosf(ang)*r,BALL_C+sinf(ang)*r,rr(&s,0.006f,0.013f),V(2.0f,1.6f,0.6f),rr(&s,0.4f,1.0f)); } }
static void fx_midas(int L,float x,float y,F3 *c,float *a){
  if(L==0){ float d=dball(x,y); float g=expf(-fmaxf(d,0)/0.08f)*0.32f; addl(c,mul(V(1.3f,0.85f,0.2f),g)); *a=fmaxf(*a,g*0.45f); return; }
  spk_eval(x,y,c,a);
  float st=star4(x,y,-0.30f,BALL_C+0.40f,0.22f);
  if(st>0.01f){ addl(c,mul(V(2.0f,1.7f,1.0f),st)); *a=fmaxf(*a,sat(st)); }
}
/* --- ronces : boule de bois prise dans des ronces herissees --- */
static F3 RTH[24]; static int NRTH=0;
static void setup_ronces(void){ setup_racine(); uint32_t s=271; NRTH=0;
  for(int i=0;i<3;i++){ F3 ax=RAX[i]; F3 u=nrm(cross(ax,V(0.3f,0.5f,0.8f))), w=cross(ax,u);
    for(int k=0;k<8 && NRTH<24;k++){ float ang=k*TAU/8+rr(&s,-0.2f,0.2f); RTH[NRTH++]=nrm(add(mul(u,cosf(ang)),mul(w,sinf(ang)))); } } }
static float sdf_ronces(F3 p,int *m){
  F3 v=sub(p,BC); float d=len(v)-BALL_R*0.95f; *m=0;
  for(int i=0;i<3;i++){ float r=vine_d(v,i,0.05f); if(r<d){ *m=1; } d=smin(d,r,0.025f); }
  F3 dp=nrm(v);
  for(int i=0;i<NRTH;i++){ if(dot(dp,RTH[i])<0.90f) continue;
    F3 a=add(BC,mul(RTH[i],BALL_R*0.98f)), b=add(BC,mul(RTH[i],BALL_R*1.26f));
    float ds=sd_cap2(p,a,b,0.035f,0.002f); if(ds<d){ d=ds; *m=3; } }
  float berry=sd_sph(p,V(0.36f,BALL_C+0.42f,0.42f),0.07f); if(berry<d){ d=berry; *m=4; }
  return d;
}
static void mat_ronces(F3 p,F3 *n,int m,Mat *o){
  if(m==3){ float t=sat((len(sub(p,BC))-BALL_R)/0.25f); o->alb=mix3(V(0.30f,0.26f,0.12f),V(0.85f,0.80f,0.62f),t); o->rough=0.35f; o->clear=0.4f; return; }
  if(m==4){ o->alb=V(0.35f,0.02f,0.08f); o->rough=0.08f; o->clear=1; o->sss=0.5f; o->sssc=V(1.0f,0.1f,0.2f); return; }
  if(m==1){ F3 q=bq(p); *n=bump(p,*n,h_bark,0.2f,0.004f); float v=ridged(mul(q,6.0f),3); o->alb=mix3(V(0.06f,0.10f,0.03f),V(0.20f,0.30f,0.08f),sat(v)); o->rough=0.6f; o->sss=0.2f; o->sssc=V(0.3f,0.6f,0.1f); return; }
  mat_racine(p,n,0,o);
}
/* --- pluie acide : une bombe de verre pleine d'acide --- */
static void mat_pluie(F3 p,F3 *n,int m,Mat *o){
  if(m==0){ o->glass=1; o->ior=1.40f; o->absorb=V(0.6f,0.2f,1.6f); o->rough=0.03f; return; }
  mat_bombe(p,n,m,o);
  if(m==2){ float tip=sat((p.x-0.18f)/0.09f); o->emit=mul(V(0.7f,1.6f,0.15f),tip*tip*1.2f); }
}
static void setup_pluie(void){ setup_bombe(); uint32_t s=55; NSPK=0; F3 t=V(FUSE[3][0]+0.02f,fuse_y0+FUSE[3][1]+0.01f,0);
  for(int i=0;i<10;i++){ float a=rr(&s,0,TAU), r=rr(&s,0.03f,0.14f); spk_add(t.x+cosf(a)*r,t.y+sinf(a)*r*0.9f+0.03f,rr(&s,0.006f,0.012f),V(1.0f,2.0f,0.35f),rr(&s,0.5f,1.0f)); }
  for(int i=0;i<5;i++){ float xx=rr(&s,-0.35f,0.35f); spk_add(xx,BALL_C-BALL_R-rr(&s,0.08f,0.28f),rr(&s,0.012f,0.02f),V(0.9f,1.6f,0.2f),rr(&s,0.5f,0.9f)); } }
static void fx_pluie(int L,float x,float y,F3 *c,float *a){
  if(L==0){ float d=dball(x,y); float g=expf(-fmaxf(d,0)/0.08f)*0.30f; addl(c,mul(V(0.6f,1.1f,0.1f),g)); *a=fmaxf(*a,g*0.45f); return; }
  float tx=FUSE[3][0]+0.03f, ty=fuse_y0+FUSE[3][1]+0.01f;
  float st=star4(x,y,tx,ty,0.20f); float g=expf(-((x-tx)*(x-tx)+(y-ty)*(y-ty))/0.004f);
  if(st+g>0.005f){ addl(c,add(mul(V(1.4f,2.2f,0.6f),st*0.9f),mul(V(0.7f,1.8f,0.1f),g*0.8f))); *a=fmaxf(*a,sat(st*0.8f+g*0.5f)); }
  spk_eval(x,y,c,a);
}


/* ======================================================================= */
/*                          v7 : objets nouveaux                           */
/* ======================================================================= */
/* --- cape d'ombre : capuche vide ou luisent deux yeux, drape violet --- */
static float sdf_cape(F3 p,int *m){
  F3 q=ob(p); *m=0;
  float ang=atan2f(q.z,q.x);
  float fold=0.038f*sinf(ang*7.0f+q.y*2.0f)*sat((0.30f-q.y)*1.6f);
  float body=sd_cap2(q,V(0,0.24f,-0.02f),V(0,-0.72f,-0.02f),0.20f,0.50f)+fold;
  body=smax(body,-(q.y+0.76f-0.04f*sinf(ang*9.0f)),0.02f);
  float hood=sd_ell(sub(q,V(0,0.40f,0)),V(0.27f,0.30f,0.26f));
  float d=smin(body,hood,0.10f);
  float inner=sd_ell(sub(q,V(0,0.37f,0.13f)),V(0.19f,0.21f,0.22f));
  if(-inner>d-0.0f){ *m=1; } d=smax(d,-inner,0.015f);
  for(int i=-1;i<=1;i+=2){ float e=sd_sph(q,V(i*0.075f,0.40f,0.17f),0.026f); if(e<d){ d=e; *m=2; } }
  float cl=sd_cyl(rotx(sub(q,V(0,0.13f,0.215f)),PI*0.5f),0.045f,0.015f,0.008f); if(cl<d){ d=cl; *m=3; }
  return d*0.9f;
}
static float h_velvet(F3 p){ F3 q=ob(p); return fbm3(mul(q,18.0f),2)*0.5f; }
static void mat_cape(F3 p,F3 *n,int m,Mat *o){
  F3 q=ob(p);
  if(m==2){ o->alb=V(1,1,1); o->emit=V(1.2f,0.55f,2.2f); return; }
  if(m==3){ o->metal=1; o->alb=V(0.95f,0.70f,0.30f); o->rough=0.2f; return; }
  if(m==1){ o->alb=V(0.004f,0.002f,0.008f); o->rough=0.9f; o->aoc=0.3f; return; }
  *n=bump(p,*n,h_velvet,0.04f,0.004f);
  float ang=atan2f(q.z,q.x); float f=0.5f+0.5f*sinf(ang*7.0f+q.y*2.0f);
  o->alb=mix3(V(0.035f,0.012f,0.07f),V(0.13f,0.05f,0.24f),f); o->rough=0.55f; o->sss=0.15f; o->sssc=V(0.4f,0.1f,0.8f);
  o->clear=0.15f;
}
static void fx_cape(int L,float x,float y,F3 *c,float *a){
  if(L==1) return;
  /* fumee d'ombre qui s'echappe du bas */
  float n=fbm2(x*3.0f,y*3.0f+1.1f,4);
  float d=sqrtf(x*x*0.8f+(y+0.62f)*(y+0.62f)*1.6f);
  float al=sat(1-d/0.55f)*sat(0.35f+n)*0.5f*sat(-(y+0.35f)*3);
  if(al>0.003f) over(c,a,V(0.10f,0.04f,0.18f),al);
  float g=expf(-sqrtf(x*x+(y-0.40f)*(y-0.40f))/0.12f)*0.25f; addl(c,mul(V(0.6f,0.25f,1.1f),g)); *a=fmaxf(*a,g*0.3f);
}
/* --- loupe : cercle de laiton, verre bombe, manche de bois --- */
static float sdf_loupe(F3 p,int *m){
  F3 q=ob(p); *m=0; F3 c=V(-0.12f,0.16f,0);
  F3 r=rotx(sub(q,c),PI*0.5f);
  float d=sd_tor(r,0.42f,0.055f);
  F3 h0=V(c.x+0.30f,c.y-0.30f,0), h1=V(c.x+0.70f,c.y-0.70f,0);
  float fer=sd_cap(q,h0,add(h0,V(0.10f,-0.10f,0)),0.07f); if(fer<d){ d=smin(d,fer,0.02f); }
  float hd=sd_cap2(q,add(h0,V(0.10f,-0.10f,0)),h1,0.065f,0.075f); if(hd<d){ d=smin(d,hd,0.015f); *m=2; }
  if(!g_noglass){ float gl=smax(len(V(r.x,0,r.z))-0.40f,fabsf(r.y)-0.012f-0.03f*sat(1-(r.x*r.x+r.z*r.z)/0.16f),0.005f); if(gl<d){ d=gl; *m=1; } }
  return d;
}
static void mat_loupe(F3 p,F3 *n,int m,Mat *o){
  F3 q=ob(p);
  if(m==1){ o->tglass=1; o->tint=V(0.85f,0.97f,1.0f); return; }
  if(m==2){ float g=fbm3(V(q.x*4,q.y*4,q.z*40),3); o->alb=mix3(V(0.20f,0.08f,0.03f),V(0.42f,0.20f,0.08f),sat(0.5f+g)); o->rough=0.35f; o->clear=0.6f; return; }
  float g=fbm3(mul(q,8),3); o->metal=1; o->alb=V(0.92f,0.68f,0.30f); o->rough=0.18f+0.1f*g;
}
static void fx_loupe(int L,float x,float y,F3 *c,float *a){
  if(L==0) return;
  float st=star4(x,y,-0.30f,0.36f,0.18f)*0.8f;
  if(st>0.01f){ addl(c,mul(V(1.6f,1.8f,2.0f),st)); *a=fmaxf(*a,sat(st)); }
}
/* --- ancre : fer forge rouille, jas de bois --- */
static float sdf_ancre(F3 p,int *m){
  F3 q=ob(p); *m=0;
  float ring=sd_tor(rotx(sub(q,V(0,0.66f,0)),PI*0.5f),0.11f,0.035f);
  float shank=sd_cap(q,V(0,0.54f,0),V(0,-0.58f,0),0.062f);
  float d=fminf(ring,shank);
  F3 ar=rotx(sub(q,V(0,-0.14f,0)),PI*0.5f);
  float arm=sd_tor(ar,0.46f,0.058f); arm=smax(arm,q.y+0.14f-0.02f,0.02f);
  d=smin(d,arm,0.04f);
  for(int i=-1;i<=1;i+=2){ float fl=sd_cap2(q,V(i*0.46f,-0.20f,0),V(i*0.40f,0.06f,0),0.11f,0.012f); fl=smax(fl,fabsf(q.z)-0.035f,0.01f); d=smin(d,fl,0.03f); }
  float stock=sd_cap(q,V(-0.34f,0.42f,0),V(0.34f,0.42f,0),0.05f); if(stock<d){ d=stock; *m=1; }
  return d*0.95f;
}
static void mat_ancre(F3 p,F3 *n,int m,Mat *o){
  F3 q=ob(p);
  if(m==1){ float g=fbm3(V(q.x*40,q.y*4,q.z*4),3); o->alb=mix3(V(0.22f,0.12f,0.05f),V(0.40f,0.24f,0.11f),sat(0.5f+g)); o->rough=0.6f; return; }
  *n=bump(p,*n,h_dent,0.05f,0.004f);
  float rust=sst(0.05f,0.45f,fbm3(mul(q,5.0f),4)+0.15f);
  o->metal=1-0.7f*rust; o->alb=mix3(V(0.30f,0.31f,0.34f),V(0.38f,0.16f,0.06f),rust); o->rough=0.35f+0.45f*rust;
}
/* --- totem d'epines : poteau de bois sculpte, yeux braises, epines --- */
static F3 TOT[14]; static float TOTL[14]; static int NTOT=0;
static void setup_totem(void){ uint32_t s=91; NTOT=0;
  for(int i=0;i<12;i++){ float ang=rr(&s,0,TAU), y=rr(&s,-0.55f,0.18f); if(ang>1.0f && ang<2.1f && y>0.1f) ang+=1.4f;
    TOT[NTOT]=V(cosf(ang),y,sinf(ang)); TOTL[NTOT]=rr(&s,0.16f,0.24f); NTOT++; }
  for(int i=0;i<2;i++){ float ang=-0.6f+i*1.2f+PI*0.5f; TOT[NTOT]=V(cosf(ang),0.62f,sinf(ang)*0.6f); TOTL[NTOT]=0.20f; NTOT++; } }
static float sdf_totem(F3 p,int *m){
  F3 q=mul(ob(p),0.84f); *m=0; const float RT=0.25f;
  float d=sd_cyl(sub(q,V(0,-0.06f,0)),RT,0.66f,0.05f);
  for(int k=0;k<2;k++){ float y=k?-0.28f:0.16f; d=smax(d,-sd_tor(sub(q,V(0,y,0)),RT+0.015f,0.03f),0.01f); }
  for(int i=-1;i<=1;i+=2){ float so=sd_sph(q,V(i*0.10f,0.40f,RT-0.01f),0.062f); d=smax(d,-so,0.012f); }
  d=smax(d,-sd_box(sub(q,V(0,0.28f,RT)),V(0.11f,0.02f,0.06f),0.01f),0.01f);
  for(int i=-1;i<=1;i+=2){ float e=sd_sph(q,V(i*0.10f,0.40f,RT-0.035f),0.036f); if(e<d){ d=e; *m=2; } }
  for(int i=0;i<NTOT;i++){ F3 dir=nrm(V(TOT[i].x,0,TOT[i].z)); F3 b=V(dir.x*(RT-0.02f),TOT[i].y,dir.z*(RT-0.02f)); F3 t=add(b,mul(nrm(add(dir,V(0,0.35f,0))),TOTL[i]));
    if(i>=12){ b=V(TOT[i].x*0.14f,0.56f,TOT[i].z*0.14f); t=add(b,V(TOT[i].x*0.10f,TOTL[i],TOT[i].z*0.10f)); }
    float th=sd_cap2(q,b,t,0.05f,0.004f); if(th<d){ d=th; *m=1; } }
  return d/0.84f;
}
static void mat_totem(F3 p,F3 *n,int m,Mat *o){
  F3 q=ob(p);
  if(m==2){ o->alb=V(1,1,1); o->emit=V(2.0f,0.45f,0.12f); return; }
  if(m==1){ float t=sat((len(V(q.x,0,q.z))*0.84f-0.23f)/0.2f); o->alb=mix3(V(0.30f,0.20f,0.10f),V(0.80f,0.74f,0.58f),t); o->rough=0.35f; o->clear=0.3f; return; }
  *n=bump(p,*n,h_bark,0.12f,0.004f);
  float g=fbm3(V(q.x*6,q.y*30,q.z*6),3);
  o->alb=mix3(V(0.16f,0.07f,0.03f),V(0.36f,0.18f,0.08f),sat(0.5f+g)); o->rough=0.55f;
  F3 qs=mul(q,0.84f); float de=fminf(len(sub(qs,V(-0.10f,0.40f,0.215f))),len(sub(qs,V(0.10f,0.40f,0.215f))));
  o->emit=mul(V(0.8f,0.15f,0.03f),expf(-de*14.0f)*0.6f);
}
static void fx_totem(int L,float x,float y,F3 *c,float *a){
  if(L==1) return;
  float g=expf(-sqrtf(x*x+(y-0.46f)*(y-0.46f))/0.16f)*0.30f; addl(c,mul(V(1.4f,0.35f,0.08f),g)); *a=fmaxf(*a,g*0.35f);
}

/* ======================================================================= */
/*                                 la table                                */
/* ======================================================================= */

/* ======================================================================= */
/*                       v9 : nouvelles billes et fusions                  */
/* ======================================================================= */
static void spk_ring(uint32_t seed,int n,float r0,float r1,F3 col){ uint32_t s=seed; NSPK=0;
  for(int i=0;i<n;i++){ float an=rr(&s,0,TAU), r=rr(&s,r0,r1); spk_add(cosf(an)*r,BALL_C+sinf(an)*r,rr(&s,0.007f,0.014f),col,rr(&s,0.4f,1.0f)); } }
static void fx_aura(float x,float y,F3 *c,float *a,F3 col,float w,float k){ float d=dball(x,y); float g=expf(-fmaxf(d,0)/w)*k; addl(c,mul(col,g)); *a=fmaxf(*a,g*0.45f); }
/* --- ombre : obsidienne aux veines violettes, fumee noire --- */
static void setup_ombre(void){ spk_ring(91,8,0.85f,1.1f,V(0.9f,0.5f,1.8f)); }
static void mat_ombre(F3 p,F3 *n,int m,Mat *o){ (void)n; (void)m;
  F3 q=bq(p); Wor w=worley(mul(q,1.9f),0.9f); float vein=expf(-sqrf((w.f2-w.f1)/0.028f)); float f=fbm3(mul(q,3.0f),4);
  o->alb=V(0.035f,0.025f,0.05f); o->rough=0.12f; o->spec=0.06f; o->clear=0.6f;
  o->emit=mul(V(0.50f,0.22f,1.0f),vein*sat(f*1.2f+0.3f)*0.9f);
}
static void fx_ombre(int L,float x,float y,F3 *c,float *a){
  if(L==1){ spk_eval(x,y,c,a); return; }
  float d=dball(x,y); if(d<-0.02f) return;
  float nn=fbm2(x*3.2f+2.1f,y*3.2f-0.7f,5); float tail=sat(1-d/(0.14f+0.12f*sat(nn+0.5f)));
  float al=sat(tail*(0.45f+nn))*0.6f; if(al>0.003f) over(c,a,V(0.10f,0.05f,0.18f),al);
  fx_aura(x,y,c,a,V(0.45f,0.2f,0.9f),0.06f,0.3f);
}
/* --- ricochet : cuivre martele, cercles graves, vert-de-gris --- */
static void mat_ricochet(F3 p,F3 *n,int m,Mat *o){ (void)m;
  F3 q=bq(p); *n=bump(p,*n,h_hammer,0.006f,0.004f);
  float band=fabsf(sinf(q.y*9.0f+fbm3(mul(q,2),2)*0.6f)); float g=fbm3(mul(q,4),3);
  o->metal=1; o->alb=mix3(V(0.95f,0.55f,0.32f),V(0.62f,0.30f,0.16f),sat(g*0.8f+0.4f)); o->rough=0.16f+0.12f*sat(g+0.5f);
  if(band<0.10f){ o->alb=mul(o->alb,0.45f); o->rough=0.5f; }
  float pat=sat(fbm3(mul(q,5),3)*1.5f-0.1f); o->alb=mix3(o->alb,V(0.25f,0.45f,0.38f),pat*0.25f);
}
static void fx_ricochet(int L,float x,float y,F3 *c,float *a){ if(L!=1) return;
  float st=star4(x,y,-0.30f,BALL_C+0.36f,0.22f)*0.8f; if(st>0.01f){ addl(c,mul(V(2.0f,1.6f,1.1f),st)); *a=fmaxf(*a,sat(st)); } }
/* --- lave : croute noire fendue de lave --- */
static void setup_lave(void){ spk_ring(57,12,0.82f,1.08f,V(1.9f,0.8f,0.2f)); }
static void mat_lave(F3 p,F3 *n,int m,Mat *o){ (void)m;
  F3 q=bq(p); Wor w=worley(mul(q,2.6f),0.85f); float crack=expf(-sqrf((w.f2-w.f1)/0.035f));
  *n=bump(p,*n,h_lava,0.05f,0.004f); float t=fire_turb(q);
  o->alb=V(0.07f,0.05f,0.045f); o->rough=0.8f;
  o->emit=mul(fire_ramp(0.2f+0.65f*sat(crack*0.9f+t*0.25f)),crack*1.6f);
}
static void fx_lave(int L,float x,float y,F3 *c,float *a){ if(L==1){ spk_eval(x,y,c,a); return; } fx_aura(x,y,c,a,V(1.3f,0.45f,0.08f),0.07f,0.35f); }
/* --- etoilee : verre bleu nuit plein d'etoiles --- */
static void setup_etoile(void){ spk_ring(313,10,0.70f,1.05f,V(1.4f,1.6f,2.2f)); }
static void mat_etoile(F3 p,F3 *n,int m,Mat *o){ (void)p; (void)n; (void)m; o->glass=1; o->ior=1.4f; o->absorb=V(1.8f,1.3f,0.35f); o->rough=0.02f; }
static F3 vol_etoile(F3 q,float *dens){
  Wor w=worley(mul(q,5.5f),1.0f); float st=((w.id&7)<3) ? expf(-w.f1*w.f1*180.0f) : 0;
  float neb=sat(fbm3(mul(q,2.2f),4)+0.3f);
  *dens=0.3f+neb*0.9f+st*6.0f;
  return add(mul(V(0.25f,0.28f,0.95f),neb*0.35f),mul(V(1.5f,1.5f,1.9f),st*1.5f));
}
static void fx_etoile(int L,float x,float y,F3 *c,float *a){
  if(L==0){ fx_aura(x,y,c,a,V(0.4f,0.5f,1.4f),0.06f,0.3f); return; }
  spk_eval(x,y,c,a); float st=star4(x,y,0.30f,BALL_C+0.30f,0.26f)*0.9f; if(st>0.01f){ addl(c,mul(V(1.7f,1.8f,2.2f),st)); *a=fmaxf(*a,sat(st)); } }
/* --- eclipse : disque noir et couronne de lumiere --- */
static void mat_eclipse(F3 p,F3 *n,int m,Mat *o){ (void)p; (void)m; o->alb=V(0.012f,0.010f,0.015f); o->rough=0.35f;
  float rim=powf(1-sat(n->z),4); o->emit=mul(V(1.8f,1.4f,0.8f),rim*1.2f); }
static void fx_eclipse(int L,float x,float y,F3 *c,float *a){
  float dx=x, dy=y-BALL_C, r=sqrtf(dx*dx+dy*dy), e=r-BALL_R;
  if(L==0){ float ang=atan2f(dy,dx); float ray=powf(0.5f+0.5f*pn2(ang*4.0f,2.2f),3.0f);
    float v=expf(-fabsf(e-0.01f)/0.035f)*0.9f+ray*expf(-fmaxf(e,0)/0.16f)*0.8f; if(v>0.003f){ addl(c,mul(V(1.5f,1.15f,0.6f),v)); *a=fmaxf(*a,sat(v*0.6f)); } return; }
  float st=star4(x,y,0.42f,BALL_C+0.30f,0.30f)*0.9f; if(st>0.01f){ addl(c,mul(V(2.0f,1.8f,1.3f),st)); *a=fmaxf(*a,sat(st)); } }
/* --- cauchemar : verre violet fume, deux yeux rouges --- */
static void mat_cauchemar(F3 p,F3 *n,int m,Mat *o){ (void)m;
  F3 q=bq(p); o->glass=1; o->ior=1.15f; o->absorb=V(1.0f,2.2f,0.6f); o->rough=0.05f;
  float rim=powf(1-sat(n->z),3); o->emit=mul(V(0.5f,0.2f,0.9f),rim*0.8f);
  for(int s2=-1;s2<=1;s2+=2){ float ex=q.x-s2*0.30f, ey=q.y-0.12f; float de=sqrtf(ex*ex*1.0f+ey*ey*6.0f); if(q.z>0.5f && de<0.16f) o->emit=add(o->emit,mul(V(2.4f,0.25f,0.15f),sat((0.16f-de)/0.06f))); }
}
static F3 vol_cauchemar(F3 q,float *dens){ float m=sat(fbm3(mul(q,2.6f),4)+0.35f); *dens=0.4f+m*2.0f; return mul(V(0.30f,0.10f,0.45f),m*0.6f); }
/* --- carambole : bille de billard rouge, rond blanc numerote --- */
static void mat_carambole(F3 p,F3 *n,int m,Mat *o){ (void)n; (void)m;
  F3 q=bq(p); o->alb=V(0.72f,0.06f,0.04f); o->rough=0.06f; o->clear=1.0f; o->spec=0.05f;
  float dc=len(sub(q,nrm(V(-0.15f,0.25f,0.95f))));
  if(dc<0.38f){ o->alb=V(0.92f,0.90f,0.86f); if(dc<0.22f){ float ux=q.x+0.15f, uy=q.y-0.25f; float r6=sqrtf(ux*ux+(uy+0.04f)*(uy+0.04f)); if(fabsf(r6-0.08f)<0.03f || (ux<-0.05f && ux>-0.11f && uy>-0.04f && uy<0.12f)) o->alb=V(0.04f,0.03f,0.03f); } }
}
/* --- volcan : roche, rivieres de lave, cratere fumant --- */
static void setup_volcan(void){ spk_ring(808,14,0.80f,1.12f,V(2.0f,0.9f,0.25f)); }
static void mat_volcan(F3 p,F3 *n,int m,Mat *o){ (void)m;
  F3 q=bq(p); *n=bump(p,*n,h_lava,0.08f,0.004f); float riv=expf(-sqrf((fbm3(mul(q,2.0f),4)+0.05f)/0.025f))*sat(1.2f-q.y);
  float crater=sst(0.80f,0.95f,q.y);
  o->alb=V(0.12f,0.09f,0.08f); o->rough=0.9f;
  o->emit=mul(fire_ramp(0.3f+0.6f*sat(riv+crater)),riv*1.4f+crater*2.2f);
}
static void fx_volcan(int L,float x,float y,F3 *c,float *a){
  if(L==1){ spk_eval(x,y,c,a); return; }
  float d=dball(x,y); if(d>-0.02f && y>BALL_C){ float nn=fbm2(x*3.0f+0.4f,y*2.2f-1.1f,5); float plume=sat(1-fabsf(x+0.05f*(y-BALL_C))/(0.12f+0.25f*(y-BALL_C-BALL_R*0.8f)))*sat((y-BALL_C-BALL_R*0.6f)/0.2f)*sat(1.4f-(y-BALL_C));
    float al=sat(plume*(0.4f+nn))*0.55f; if(al>0.003f) over(c,a,V(0.22f,0.18f,0.18f),al); }
  fx_aura(x,y,c,a,V(1.3f,0.4f,0.08f),0.06f,0.3f);
}
/* --- constellation : bleu nuit, etoiles reliees --- */
static float CSX[8], CSY[8];
static void setup_constellation(void){ uint32_t s=4401; NSPK=0; for(int i=0;i<8;i++){ float an=rr(&s,0,TAU), r=rr(&s,0.1f,0.62f); CSX[i]=cosf(an)*r; CSY[i]=BALL_C+sinf(an)*r; } }
static void mat_constellation(F3 p,F3 *n,int m,Mat *o){ (void)n; (void)m; F3 q=bq(p); float neb=sat(fbm3(mul(q,2.4f),4)+0.4f);
  o->alb=V(0.03f,0.04f,0.12f); o->rough=0.08f; o->clear=0.8f; o->emit=mul(V(0.15f,0.18f,0.55f),neb*0.6f); }
static void fx_constellation(int L,float x,float y,F3 *c,float *a){
  if(L==0){ fx_aura(x,y,c,a,V(0.45f,0.55f,1.4f),0.06f,0.3f); return; }
  for(int i=0;i<7;i++){ float x0=CSX[i],y0=CSY[i],x1=CSX[i+1],y1=CSY[i+1]; float bx=x1-x0, by=y1-y0, px=x-x0, py=y-y0; float h=sat((px*bx+py*by)/(bx*bx+by*by+1e-6f)); float dl=sqrtf(sqrf(px-bx*h)+sqrf(py-by*h));
    float l=expf(-dl*dl/0.00004f)*0.6f; if(l>0.005f){ addl(c,mul(V(0.8f,0.9f,1.6f),l)); *a=fmaxf(*a,sat(l*0.8f)); } }
  for(int i=0;i<8;i++){ float st=star4(x,y,CSX[i],CSY[i],0.10f+0.04f*(i&1)); if(st>0.01f){ addl(c,mul(V(1.8f,1.9f,2.2f),st)); *a=fmaxf(*a,sat(st)); } }
}
/* --- supernova : coeur blanc, souffle d'or --- */
static void setup_supernova(void){ BALL_R=0.58f; spk_ring(2024,14,0.75f,1.15f,V(2.2f,1.4f,0.5f)); }
static void mat_supernova(F3 p,F3 *n,int m,Mat *o){ (void)m; F3 q=bq(p); float f=fbm3(mul(q,3.5f),4); float c=sat(n->z);
  o->alb=V(0.9f,0.8f,0.6f); o->rough=0.4f; o->emit=add(mul(V(2.2f,2.0f,1.6f),powf(c,1.2f)*(0.8f+0.3f*f)),mul(fire_ramp(0.7f),(1-c)*1.2f)); }
static void fx_supernova(int L,float x,float y,F3 *c,float *a){
  float dx=x, dy=y-BALL_C, r=sqrtf(dx*dx+dy*dy), e=r-BALL_R;
  if(L==0){ float ang=atan2f(dy,dx); float ray=powf(0.5f+0.5f*pn2(ang*5.0f,7.7f),4.0f)+0.5f*powf(0.5f+0.5f*pn2(ang*11.0f,1.9f),6.0f);
    float v=ray*expf(-fmaxf(e,0)/0.24f)*1.3f+expf(-fmaxf(e,0)/0.08f)*0.7f; if(v>0.003f){ addl(c,mul(V(1.6f,0.9f,0.3f),v)); *a=fmaxf(*a,sat(v*0.55f)); } return; }
  spk_eval(x,y,c,a);
}
/* --- big bang : coeur noir et disque d'accretion irise --- */
static void setup_bigbang(void){ BALL_R=0.42f; spk_ring(777,16,0.6f,1.15f,V(1.6f,1.3f,2.2f)); }
static void mat_bigbang(F3 p,F3 *n,int m,Mat *o){ (void)p; (void)m; o->alb=V(0,0,0); o->rough=1; float rim=powf(1-sat(n->z),5); o->emit=mul(V(0.9f,0.6f,1.6f),rim*1.5f); }
static void fx_bigbang(int L,float x,float y,F3 *c,float *a){
  float dx=x, dy=(y-BALL_C)/0.38f, r=sqrtf(dx*dx+dy*dy);
  float ang=atan2f(dy,dx); float band=expf(-sqrf((r-0.78f)/0.16f))*(0.6f+0.4f*sinf(ang*3+r*14));
  bool front = (y-BALL_C)<0; if((L==1)!=front){ if(L==1) spk_eval(x,y,c,a); return; }
  if(band>0.01f){ F3 col=V(0.9f+0.6f*sinf(ang*2),0.5f+0.5f*sinf(ang*2+2.1f),1.2f+0.5f*sinf(ang*2+4.2f)); addl(c,mul(col,band*1.4f)); *a=fmaxf(*a,sat(band)); }
  if(L==1) spk_eval(x,y,c,a);
}
/* --- jugement dernier : or poli, aureole et rayons --- */
static void setup_jugement(void){ setup_tonnerre(); BALL_C=-0.04f; spk_ring(1212,10,0.8f,1.1f,V(2.2f,1.9f,1.0f)); }
static void mat_jugement(F3 p,F3 *n,int m,Mat *o){ (void)m; F3 q=bq(p); float g=fbm3(mul(q,6),3); o->metal=1; o->alb=V(1.0f,0.80f,0.40f); o->rough=0.08f+0.05f*g;
  float cross=fminf(fabsf(q.x),fabsf(q.y-0.05f)); if(q.z>0.6f && cross<0.06f) o->emit=V(2.0f,1.8f,1.2f); (void)n; }
static void fx_jugement(int L,float x,float y,F3 *c,float *a){
  if(L==0){ fx_lumiere(0,x,y,c,a); return; }
  float w; float d=arc_dist(x,y,&w); float core=expf(-d*d*1.2f), halo=expf(-d*0.75f)*0.30f;
  if(core+halo>0.003f){ addl(c,add(mul(V(1.9f,1.8f,2.2f),core),mul(V(0.8f,0.6f,1.6f),halo))); *a=fmaxf(*a,sat(core+halo*0.5f)); }
  spk_eval(x,y,c,a);
}
/* --- zero absolu : cristal de glace a facettes, coeur bleu --- */
static void setup_zero(void){ uint32_t s=9; NFAC=24; for(int i=0;i<NFAC;i++) FAC[i]=nrm(roty(rotx(fib(i,NFAC),0.5f),0.2f)); spk_ring(s+51,12,0.78f,1.1f,V(1.6f,2.0f,2.4f)); }
static void mat_zero(F3 p,F3 *n,int m,Mat *o){ (void)p; (void)n; (void)m; o->glass=1; o->ior=1.31f; o->absorb=V(0.9f,0.35f,0.10f); o->rough=0.02f; o->disp=0.03f; }
static F3 vol_zero(F3 q,float *dens){ Wor w=worley(mul(q,2.2f),0.9f); float cr=expf(-sqrf((w.f2-w.f1)/0.03f)); float core=expf(-len(q)*len(q)*5.0f);
  *dens=0.3f+cr*4.0f+core*2.5f; return add(mul(V(0.8f,0.95f,1.0f),cr*1.2f),mul(V(0.3f,0.75f,1.6f),core*1.2f)); }
static void fx_zero(int L,float x,float y,F3 *c,float *a){
  if(L==0){ fx_aura(x,y,c,a,V(0.6f,0.9f,1.5f),0.08f,0.35f); return; }
  float st=star4(x,y,-0.32f,BALL_C+0.30f,0.34f)*0.9f; if(st>0.01f){ addl(c,mul(V(1.8f,2.1f,2.4f),st)); *a=fmaxf(*a,sat(st)); } spk_eval(x,y,c,a); }
/* --- ouroboros : ecailles noires et vertes, oeil rouge --- */
static void setup_ouro(void){ spk_ring(4242,8,0.85f,1.05f,V(0.8f,2.0f,0.6f)); }
static float h_scale(F3 p){ Wor w=worley(mulv(bq(p),V(5.0f,7.0f,5.0f)),0.6f); return sst(0.0f,0.25f,w.f1)*0.6f; }
static void mat_ouro(F3 p,F3 *n,int m,Mat *o){ (void)m; F3 q=bq(p); Wor w=worley(mulv(q,V(5.0f,7.0f,5.0f)),0.6f); *n=bump(p,*n,h_scale,0.03f,0.004f);
  float sc=sat(w.f1*3.0f); o->alb=mix3(V(0.02f,0.12f,0.05f),V(0.25f,0.55f,0.18f),sc); o->rough=0.25f; o->clear=0.5f;
  float band=fabsf(q.y+0.1f*sinf(q.x*6)); o->emit=mul(V(0.3f,1.0f,0.25f),expf(-band*band/0.004f)*0.6f);
  float ex=q.x-0.42f, ey=q.y-0.18f; if(q.z>0.5f && ex*ex+ey*ey*1.6f<0.012f) o->emit=V(2.4f,0.3f,0.1f);
}
static void fx_ouro(int L,float x,float y,F3 *c,float *a){ if(L==1){ spk_eval(x,y,c,a); return; } fx_aura(x,y,c,a,V(0.4f,1.2f,0.35f),0.07f,0.32f); }
static void mat_plain(F3 p,F3 *n,int m,Mat *o){ (void)p; (void)n; (void)m; o->alb=V(0.6f,0.6f,0.6f); o->rough=0.3f; }

/* ===================== v10 : billes et objets ===================== */
/* --- boomerang : bois tourne, bandes peintes --- */
static void mat_boomerang(F3 p,F3 *n,int m,Mat *o){ (void)m;
  F3 q=bq(p); float ring=sinf((q.x*0.3f+q.y)*26.0f+fbm3(mul(q,3),3)*3.0f); float g=fbm3(V(q.x*2,q.y*14,q.z*2),3);
  o->alb=mix3(V(0.55f,0.30f,0.13f),V(0.78f,0.50f,0.24f),sat(0.5f+0.35f*ring+0.3f*g)); o->rough=0.45f; o->clear=0.5f;
  float band=fabsf(q.y-0.18f*sinf(q.x*3.0f)); if(band<0.10f){ o->alb=V(0.86f,0.82f,0.70f); o->rough=0.6f; o->clear=0.2f; }
  if(fabsf(band-0.18f)<0.03f) o->alb=V(0.55f,0.12f,0.08f);
  *n=bump(p,*n,h_hammer,0.004f,0.004f);
}
static void fx_boomerang(int L,float x,float y,F3 *c,float *a){ if(L!=1) return;
  float st=star4(x,y,-0.28f,BALL_C+0.34f,0.18f)*0.7f; if(st>0.01f){ addl(c,mul(V(1.8f,1.5f,1.0f),st)); *a=fmaxf(*a,sat(st)); } }
/* --- sismique : pierre brune, failles ambrees --- */
static void mat_seisme(F3 p,F3 *n,int m,Mat *o){ (void)m;
  F3 q=bq(p); Wor w=worley(mul(q,2.2f),0.8f); float crack=expf(-sqrf((w.f2-w.f1)/0.045f));
  *n=bump(p,*n,h_hammer,0.06f,0.004f); float g=fbm3(mul(q,5),4);
  o->alb=mix3(V(0.30f,0.22f,0.15f),V(0.52f,0.40f,0.28f),sat(0.5f+g)); o->rough=0.85f;
  o->emit=mul(V(1.6f,0.75f,0.2f),crack*0.9f);
}
static void fx_seisme(int L,float x,float y,F3 *c,float *a){ if(L==1) return; fx_aura(x,y,c,a,V(0.9f,0.45f,0.12f),0.05f,0.18f); }
/* --- chaine : acier grave de maillons --- */
static void mat_chaine(F3 p,F3 *n,int m,Mat *o){ (void)m;
  F3 q=bq(p); float u=atan2f(q.z,q.x)*6.0f/PI, v=q.y*5.0f; float cu=u-floorf(u)-0.5f, cv=v-floorf(v)-0.5f;
  float sh=((int)floorf(v))&1 ? 0.5f : 0; float cu2=u+sh; cu2=cu2-floorf(cu2)-0.5f; (void)cu;
  float link=fabsf(sqrtf(cu2*cu2*2.2f+cv*cv*0.9f)-0.30f);
  o->metal=1; o->alb=V(0.62f,0.64f,0.70f); o->rough=0.22f;
  if(link<0.06f){ o->alb=V(0.30f,0.31f,0.36f); o->rough=0.45f; }
}
/* --- chakram : or et lame rouge --- */
static void mat_chakram(F3 p,F3 *n,int m,Mat *o){ (void)m;
  F3 q=bq(p); float edge=fabsf(q.y); o->metal=1; o->alb=V(0.95f,0.72f,0.30f); o->rough=0.14f;
  if(edge<0.12f){ o->alb=V(0.85f,0.20f,0.12f); o->metal=0.6f; o->rough=0.25f; }
  float sp=fabsf(sinf(atan2f(q.z,q.x)*4.0f)); if(edge>0.12f && edge<0.2f && sp>0.8f) o->alb=V(1.0f,0.92f,0.6f);
  *n=bump(p,*n,h_hammer,0.003f,0.004f);
}
static void fx_chakram(int L,float x,float y,F3 *c,float *a){ if(L!=1) return;
  float st=star4(x,y,-0.30f,BALL_C+0.36f,0.24f)*0.9f; if(st>0.01f){ addl(c,mul(V(2.0f,1.6f,1.0f),st)); *a=fmaxf(*a,sat(st)); } }
/* --- faille : roche noire fendue de lumiere jaune --- */
static void mat_faille(F3 p,F3 *n,int m,Mat *o){ (void)m;
  F3 q=bq(p); Wor w=worley(mul(q,1.6f),0.9f); float crack=expf(-sqrf((w.f2-w.f1)/0.06f));
  *n=bump(p,*n,h_hammer,0.09f,0.004f);
  o->alb=V(0.10f,0.08f,0.07f); o->rough=0.9f; o->emit=mul(V(2.0f,1.3f,0.35f),crack*1.5f);
}
static void fx_faille(int L,float x,float y,F3 *c,float *a){ if(L==1) return; fx_aura(x,y,c,a,V(1.2f,0.8f,0.2f),0.06f,0.3f); }
/* --- entrave : acier sombre, maillons bleus lumineux --- */
static void mat_entrave(F3 p,F3 *n,int m,Mat *o){
  mat_chaine(p,n,m,o); F3 q=bq(p);
  o->alb=mul(o->alb,0.55f); float g=sat(0.5f+fbm3(mul(q,4),3));
  if(o->rough>0.4f) o->emit=mul(V(0.4f,0.7f,1.8f),0.9f+0.6f*g);
}
static void fx_entrave(int L,float x,float y,F3 *c,float *a){ if(L==1) return; fx_aura(x,y,c,a,V(0.35f,0.6f,1.4f),0.06f,0.25f); }
/* --- apocalypse : obsidienne, failles de feu --- */
static void setup_apoca(void){ spk_ring(777,14,0.85f,1.12f,V(2.0f,0.8f,0.2f)); }
static void mat_apoca(F3 p,F3 *n,int m,Mat *o){ (void)m;
  F3 q=bq(p); Wor w=worley(mul(q,2.0f),0.9f); float crack=expf(-sqrf((w.f2-w.f1)/0.05f)); float t=fire_turb(q);
  *n=bump(p,*n,h_lava,0.07f,0.004f);
  o->alb=V(0.05f,0.03f,0.03f); o->rough=0.35f; o->clear=0.4f;
  o->emit=mul(fire_ramp(0.35f+0.6f*sat(crack+t*0.3f)),crack*2.2f+0.15f);
}
static void fx_apoca(int L,float x,float y,F3 *c,float *a){ if(L==1){ spk_eval(x,y,c,a); return; } fx_aura(x,y,c,a,V(1.6f,0.5f,0.1f),0.09f,0.5f); }
/* --- pendule : balancier de laiton --- */
static float sdf_pendule(F3 p,int *m){
  F3 q=ob(p); *m=0;
  float pivot=sd_tor(rotx(sub(q,V(0,0.78f,0)),PI*0.5f),0.10f,0.03f);
  float rod=sd_cap(q,V(0,0.70f,0),V(0,-0.30f,0),0.035f);
  F3 b=rotx(sub(q,V(0,-0.48f,0)),PI*0.5f); float bob=sd_cyl(b,0.36f,0.06f,0.03f);
  float d=fminf(pivot,rod); if(bob<d){ d=bob; *m=1; }
  return d;
}
static void mat_pendule(F3 p,F3 *n,int m,Mat *o){
  F3 q=ob(p); o->metal=1;
  if(m==1){ float r=sqrtf(q.x*q.x+sqrf(q.y+0.48f)); o->alb=V(0.95f,0.74f,0.36f); o->rough=0.12f+0.2f*sat(fabsf(sinf(r*60.0f)));
    if(fabsf(r-0.26f)<0.015f) o->alb=V(0.55f,0.36f,0.12f); return; }
  o->alb=V(0.80f,0.62f,0.30f); o->rough=0.25f; *n=bump(p,*n,h_hammer,0.002f,0.004f);
}
/* --- boussole : boitier de laiton, cadran, aiguille rouge --- */
static float sdf_boussole(F3 p,int *m){
  F3 q=ob(p); *m=0; F3 r=rotx(q,PI*0.5f);
  float d=sd_cyl(r,0.62f,0.12f,0.04f);
  float dial=sd_cyl(sub(r,V(0,0.10f,0)),0.52f,0.03f,0.01f); if(dial<d){ d=smax(d,-dial,0.01f); }
  float face=sd_cyl(sub(r,V(0,0.06f,0)),0.53f,0.012f,0.005f); if(face<d){ d=face; *m=1; }
  F3 nq=rotz(q,0.5f); float nd=sd_box(sub(nq,V(0,0,0.11f)),V(0.06f,0.44f,0.015f),0.01f); nd=fmaxf(nd,(fabsf(nq.x)-0.06f*(1-fabsf(nq.y)/0.46f)));
  if(nd<d){ d=nd; *m= nq.y>0 ? 2 : 3; }
  float ring=sd_tor(sub(q,V(0,0.70f,0)),0.09f,0.03f); if(ring<d){ d=ring; *m=0; }
  return d;
}
static void mat_boussole(F3 p,F3 *n,int m,Mat *o){
  F3 q=ob(p);
  if(m==1){ float a=atan2f(q.y,q.x); float r=sqrtf(q.x*q.x+q.y*q.y); o->alb=V(0.90f,0.86f,0.74f); o->rough=0.6f;
    if(r>0.40f && r<0.48f && fabsf(sinf(a*8))>0.92f) o->alb=V(0.15f,0.12f,0.10f);
    if(r>0.30f && r<0.48f && fabsf(sinf(a*2))>0.995f) o->alb=V(0.6f,0.1f,0.08f); return; }
  if(m==2){ o->alb=V(0.85f,0.10f,0.08f); o->rough=0.3f; o->clear=0.6f; return; }
  if(m==3){ o->alb=V(0.92f,0.92f,0.90f); o->rough=0.3f; return; }
  o->metal=1; o->alb=V(0.90f,0.66f,0.30f); o->rough=0.2f; *n=bump(p,*n,h_hammer,0.003f,0.004f);
}
/* --- grimoire : cuir violet, coins d'or, rune lumineuse --- */
static float sdf_grimoire(F3 p,int *m){
  F3 q=rotz(ob(p),0.15f); *m=0;
  float cover=sd_box(q,V(0.52f,0.66f,0.17f),0.04f);
  float pages=sd_box(sub(q,V(0.04f,0,0)),V(0.50f,0.62f,0.13f),0.01f);
  float d=smax(cover,-sd_box(sub(q,V(0.56f,0,0)),V(0.10f,0.60f,0.13f),0.0f),0.005f);
  if(pages<d){ d=pages; *m=1; }
  for(int i=0;i<4;i++){ float cx=(i&1)?0.46f:-0.46f, cy=(i&2)?0.60f:-0.60f; float cn=sd_box(sub(q,V(cx,cy,0)),V(0.09f,0.09f,0.19f),0.02f); if(cn<d){ d=cn; *m=2; } }
  return d;
}
static void mat_grimoire(F3 p,F3 *n,int m,Mat *o){
  F3 q=rotz(ob(p),0.15f);
  if(m==1){ o->alb=V(0.86f,0.80f,0.64f); o->rough=0.8f; if(fabsf(sinf(q.z*120))>0.7f) o->alb=mul(o->alb,0.85f); return; }
  if(m==2){ o->metal=1; o->alb=V(0.95f,0.74f,0.32f); o->rough=0.2f; return; }
  float g=fbm3(mul(q,9),4); o->alb=mix3(V(0.22f,0.08f,0.30f),V(0.36f,0.15f,0.46f),sat(0.5f+g)); o->rough=0.55f;
  *n=bump(p,*n,h_hammer,0.01f,0.004f);
  if(q.z>0.12f){ float r=sqrtf(q.x*q.x+q.y*q.y); float rune=fabsf(r-0.26f)<0.025f || (fabsf(q.x)<0.02f && fabsf(q.y)<0.26f) || (fabsf(q.y-0.08f)<0.02f && fabsf(q.x)<0.18f);
    if(rune){ o->alb=V(0.9f,0.8f,1.0f); o->emit=V(0.8f,0.5f,1.6f); } }
}
static void fx_grimoire(int L,float x,float y,F3 *c,float *a){ if(L==0) return;
  float st=star4(x,y,0.32f,0.48f,0.16f)*0.8f; if(st>0.01f){ addl(c,mul(V(1.6f,1.3f,2.0f),st)); *a=fmaxf(*a,sat(st)); } }

static const IconDef DEFS[ICON_COUNT]={
  [BK_FER]   ={sdf_ball,mat_fer,NULL,NULL,NULL,0.75f,0,0.55f},
  [BK_PLOMB] ={sdf_ball,mat_plomb,NULL,NULL,NULL,0.75f,0,0.55f},
  [BK_FEU]   ={sdf_ball,mat_feu,NULL,fx_feu,setup_feu,0.75f,0.8f,0.35f},
  [BK_GLACE] ={sdf_glace,mat_glace,vol_glace,fx_glace,setup_glace,1.0f,0.3f,0.45f,1},
  [BK_FOUDRE]={sdf_ball,mat_foudre,vol_foudre,fx_foudre,setup_foudre,0.75f,0.6f,0.3f},
  [BK_VENIN] ={sdf_venin,mat_venin,NULL,fx_venin,NULL,1.0f,0.4f,0.5f,1},
  [BK_BABY]  ={sdf_ball,mat_baby,vol_baby,NULL,setup_baby,0.55f,0,0.5f},
  [BK_EPINE] ={sdf_epine,mat_epine,NULL,NULL,setup_epine,1.0f,0,0.5f,1},
  [BK_BOMBE] ={sdf_bombe,mat_bombe,NULL,fx_bombe,setup_bombe,0.95f,0.5f,0.55f,1},
  [BK_SPECTRE]={sdf_ball,mat_spectre,vol_spectre,fx_spectre,setup_spectre,0.75f,0.4f,0.3f},
  [BK_SANG]  ={sdf_sang,mat_sang,NULL,NULL,NULL,1.0f,0,0.5f,1},
  [BK_ECLAT] ={sdf_eclat,mat_eclat,vol_eclat,fx_eclat,setup_eclat,1.05f,0.2f,0.45f,1},
  [BK_AIMANT]={sdf_ball,mat_aimant,NULL,fx_aimant,NULL,0.75f,0,0.5f},
  [BK_LUMIERE]={sdf_ball,mat_lumiere,NULL,fx_lumiere,setup_lumiere,0.62f,0.9f,0.2f},
  [BK_FOLLE] ={sdf_ball,mat_folle,NULL,fx_folle,setup_folle,0.75f,0,0.5f},
  [BK_CRISTAL]={sdf_ball,mat_cristal,vol_cristal,fx_cristal,NULL,0.75f,0.2f,0.45f},
  [BK_RAFALE]={sdf_ball,mat_rafale,vol_rafale,fx_rafale,NULL,0.75f,0,0.35f},
  [BK_VAPEUR]={sdf_ball,mat_vapeur,vol_vapeur,fx_vapeur,NULL,0.75f,0.3f,0.45f},
  [BK_METEORE]={sdf_meteore,mat_meteore,NULL,fx_meteore,setup_meteore,0.70f,0.7f,0.4f,1},
  [BK_TEMPETE]={sdf_ball,mat_tempete,vol_tempete,fx_tempete,setup_tempete,0.75f,0.5f,0.35f},
  [BK_FLEAU] ={sdf_fleau,mat_fleau,NULL,fx_fleau,setup_fleau,1.0f,0.5f,0.5f,1},
  [BK_FAUCHEUSE]={sdf_faucheuse,mat_faucheuse,NULL,fx_faucheuse,NULL,0.80f,0.6f,0.5f,1},
  [BK_PRISME]={sdf_facets,mat_prisme,vol_prisme,fx_prisme,setup_prisme,0.75f,0.2f,0.45f},
  [BK_SINGULARITE]={sdf_ball,mat_sing,NULL,fx_sing,setup_sing,0.40f,0,0.3f},
  [BK_ENCLUME]={sdf_enclume,mat_enclume,NULL,NULL,NULL,0.82f,0,0.55f,1},
  [BK_RAYON] ={sdf_rayon,mat_rayon,NULL,fx_rayon,setup_rayon,0.90f,0.6f,0.4f,1},
  [BK_KALEIDO]={sdf_facets,mat_kaleido,NULL,fx_eclat,setup_kaleido,0.75f,0.1f,0.5f},
  [BK_GEODE] ={sdf_geode,mat_geode,NULL,fx_geode,setup_geode,0.80f,0.4f,0.5f,1},
  [BK_BLIZZARD]={sdf_ball,mat_blizzard,NULL,fx_blizzard,setup_blizzard,0.75f,0.1f,0.45f},
  [BK_TEMPFEU]={sdf_ball,mat_feu,NULL,fx_tempfeu,setup_feu,0.75f,0.8f,0.35f},
  [BK_AURORE]={sdf_ball,mat_aurore,vol_aurore,NULL,NULL,0.75f,0.3f,0.5f},
  [BK_ECHO]  ={sdf_ball,mat_echo,NULL,fx_echo,NULL,1.05f,0.3f,0.5f},
  [BK_DOREE] ={sdf_doree,mat_doree,NULL,fx_doree,setup_doree,0.80f,0.2f,0.55f},
  [BK_RACINE]={sdf_racine,mat_racine,NULL,NULL,setup_racine,0.85f,0,0.55f,1},
  [BK_ACIDE] ={sdf_acide,mat_acide,vol_acide,fx_acide,setup_acide,1.0f,0.4f,0.45f},
  [BK_TONNERRE]={sdf_ball,mat_tonnerre,vol_tonnerre,fx_tonnerre,setup_tonnerre,1.05f,0.5f,0.35f},
  [BK_MIDAS] ={sdf_ball,mat_midas,NULL,fx_midas,setup_midas,0.85f,0.4f,0.55f},
  [BK_RONCES]={sdf_ronces,mat_ronces,NULL,NULL,setup_ronces,1.0f,0,0.55f,1},
  [BK_PLUIE] ={sdf_bombe,mat_pluie,vol_acide,fx_pluie,setup_pluie,0.95f,0.5f,0.55f,1},
  [BK_OMBRE] ={sdf_ball,mat_ombre,NULL,fx_ombre,setup_ombre,0.85f,0.4f,0.4f},
  [BK_RICOCHET]={sdf_ball,mat_ricochet,NULL,fx_ricochet,NULL,0.75f,0,0.55f},
  [BK_LAVE]  ={sdf_ball,mat_lave,NULL,fx_lave,setup_lave,0.80f,0.7f,0.45f},
  [BK_ETOILE]={sdf_ball,mat_etoile,vol_etoile,fx_etoile,setup_etoile,0.80f,0.4f,0.4f},
  [BK_BOOMERANG]={sdf_ball,mat_boomerang,NULL,fx_boomerang,NULL,0.75f,0,0.55f},
  [BK_SEISME]={sdf_ball,mat_seisme,NULL,fx_seisme,NULL,0.80f,0.3f,0.55f},
  [BK_CHAINE]={sdf_ball,mat_chaine,NULL,NULL,NULL,0.75f,0,0.55f},
  [BK_ECLIPSE]={sdf_ball,mat_eclipse,NULL,fx_eclipse,NULL,0.95f,0.8f,0.35f},
  [BK_CAUCHEMAR]={sdf_ball,mat_cauchemar,vol_cauchemar,fx_ombre,setup_ombre,0.85f,0.4f,0.4f},
  [BK_CARAMBOLE]={sdf_ball,mat_carambole,NULL,fx_ricochet,NULL,0.75f,0,0.55f},
  [BK_VOLCAN]={sdf_ball,mat_volcan,NULL,fx_volcan,setup_volcan,1.05f,0.7f,0.45f},
  [BK_CONSTELLATION]={sdf_ball,mat_constellation,NULL,fx_constellation,setup_constellation,0.80f,0.4f,0.45f},
  [BK_SUPERNOVA]={sdf_ball,mat_supernova,NULL,fx_supernova,setup_supernova,0.95f,0.9f,0.3f},
  [BK_CHAKRAM]={sdf_ball,mat_chakram,NULL,fx_chakram,NULL,0.80f,0,0.55f},
  [BK_FAILLE]={sdf_ball,mat_faille,NULL,fx_faille,NULL,0.85f,0.6f,0.5f},
  [BK_ENTRAVE]={sdf_ball,mat_entrave,NULL,fx_entrave,NULL,0.85f,0.5f,0.5f},
  [BK_BIGBANG]={sdf_ball,mat_bigbang,NULL,fx_bigbang,setup_bigbang,1.05f,0.6f,0.3f},
  [BK_JUGEMENT]={sdf_ball,mat_jugement,NULL,fx_jugement,setup_jugement,1.05f,0.8f,0.45f},
  [BK_ZERO]  ={sdf_facets,mat_zero,vol_zero,fx_zero,setup_zero,0.80f,0.4f,0.45f},
  [BK_OUROBOROS]={sdf_ball,mat_ouro,NULL,fx_ouro,setup_ouro,0.80f,0.3f,0.5f},
  [BK_APOCALYPSE]={sdf_ball,mat_apoca,NULL,fx_apoca,setup_apoca,0.95f,0.9f,0.3f},
  [BK_SHARD] ={sdf_shard,mat_shard,NULL,fx_shard,setup_shard,0.85f,0.1f,0.5f},
  [BK_GHOST] ={sdf_ghost,mat_ghost,NULL,fx_ghost,NULL,1.0f,0.4f,0.3f},
  [ICON_PAS(PS_FORCE)]   ={sdf_gant,mat_gant,NULL,NULL,NULL,1.05f,0,0.55f,1},
  [ICON_PAS(PS_CADENCE)] ={sdf_sablier,mat_sablier,NULL,NULL,NULL,1.05f,0,0.5f,1},
  [ICON_PAS(PS_BOTTES)]  ={sdf_botte,mat_botte,NULL,NULL,NULL,1.05f,0,0.55f,1},
  [ICON_PAS(PS_COEUR)]   ={sdf_cpierre,mat_cpierre,NULL,NULL,NULL,1.0f,0.3f,0.55f,1},
  [ICON_PAS(PS_RESSORT)] ={sdf_ressort,mat_ressort,NULL,NULL,NULL,1.05f,0,0.55f,1},
  [ICON_PAS(PS_SAC)]     ={sdf_sac,mat_sac,NULL,NULL,NULL,1.05f,0,0.55f,1},
  [ICON_PAS(PS_LYNX)]    ={sdf_ball,mat_lynx,NULL,NULL,NULL,0.75f,0.1f,0.5f},
  [ICON_PAS(PS_LANTERNE)]={sdf_lant,mat_lant,NULL,fx_lant,NULL,1.05f,0.7f,0.5f,1},
  [ICON_PAS(PS_MOUSSE)]  ={sdf_mousse,mat_mousse,NULL,NULL,NULL,1.05f,0,0.5f,1},
  [ICON_PAS(PS_FILET)]   ={sdf_filet,mat_filet,NULL,NULL,NULL,1.05f,0,0.5f,1},
  [ICON_PAS(PS_TREFLE)]  ={sdf_trefle,mat_trefle,NULL,fx_trefle,setup_trefle,1.05f,0,0.5f,1},
  [ICON_PAS(PS_BOUCLIER)]={sdf_bouc,mat_bouc,NULL,NULL,NULL,1.05f,0.5f,0.55f,1},
  [ICON_PAS(PS_RAGE)]    ={sdf_crage,mat_crage,NULL,fx_crage,NULL,1.0f,0.6f,0.45f,1},
  [ICON_PAS(PS_FIOLE)]   ={sdf_fiole,mat_fiole,NULL,fx_fiole,NULL,1.05f,0.6f,0.5f,1},
  [ICON_PAS(PS_MIROIR)]  ={sdf_miroir,mat_miroir,NULL,NULL,NULL,1.05f,0,0.5f,1},
  [ICON_PAS(PS_CAPE)]    ={sdf_cape,mat_cape,NULL,fx_cape,NULL,1.05f,0.4f,0.5f,1},
  [ICON_PAS(PS_LOUPE)]   ={sdf_loupe,mat_loupe,NULL,fx_loupe,NULL,1.05f,0,0.5f,1},
  [ICON_PAS(PS_ANCRE)]   ={sdf_ancre,mat_ancre,NULL,NULL,NULL,1.05f,0,0.55f,1},
  [ICON_PAS(PS_TOTEM)]   ={sdf_totem,mat_totem,NULL,fx_totem,setup_totem,1.05f,0.4f,0.55f,1},
  [ICON_PAS(PS_PENDULE)] ={sdf_pendule,mat_pendule,NULL,NULL,NULL,1.05f,0,0.55f,1},
  [ICON_PAS(PS_BOUSSOLE)]={sdf_boussole,mat_boussole,NULL,NULL,NULL,1.05f,0.2f,0.55f,1},
  [ICON_PAS(PS_GRIMOIRE)]={sdf_grimoire,mat_grimoire,NULL,fx_grimoire,NULL,1.05f,0.4f,0.55f,1},
};

/* ======================================================================= */
/*                                 API                                     */
/* ======================================================================= */
static int g_row=0, g_id=-1;
void icg_begin(int id){
  prm_init(); lights_init();
  CUR=id; g_id=id; g_row=0; SEED=0x9e3779b9u*(uint32_t)(id+1);
  static IconDef fallback; fallback.sdf=sdf_ball; fallback.mat=mat_plain; fallback.bound=0.75f; fallback.shadow=0.5f;
  D=(id>=0&&id<ICON_COUNT&&DEFS[id].sdf)?&DEFS[id]:&fallback;
  BALL_C=-0.04f; BALL_R=0.70f; BC=V(0,BALL_C,0); NSPK=0; NARC=0; NSPIKE=0; OBJ_RY=0; OBJ_RX=0;
  if(id>=BK_COUNT){ static const float RY[PS_COUNT]={0.45f,0.5f,-0.35f,0.25f,0.3f,0.5f,0,0.62f,0.4f,0.3f,0.0f,0.22f,0.2f,0.4f,0.35f, 0.25f,0.30f,0.35f,0.40f};
    static const float RX[PS_COUNT]={-0.15f,-0.18f,-0.08f,-0.10f,-0.38f,-0.22f,0,-0.22f,-0.35f,-0.25f,-0.55f,-0.10f,-0.08f,-0.18f,-0.12f, -0.10f,-0.12f,-0.10f,-0.14f};
    OBJ_RY=RY[id-BK_COUNT]; OBJ_RX=RX[id-BK_COUNT]; BALL_C=0; }
  if(D->setup) D->setup();
  BC=V(0,BALL_C,0);
  memset(OBJ,0,sizeof OBJ); memset(EMI,0,sizeof EMI);
}
bool icg_rows(int nrows){
  if(g_id<0) return true;
  int y1=g_row+nrows; if(y1>N) y1=N;
  render_rows(g_row,y1); g_row=y1;
  return g_row>=N;
}
void icg_finish(uint8_t *out,int stride){ finish(out,stride); g_id=-1; }
void icg_render(int id,uint8_t *out,int stride){ icg_begin(id); while(!icg_rows(16)){} icg_finish(out,stride); }
