/* GOUFFRE - atelier des textures HD (outil hors jeu) : definitions communes.
   Une « toile » couvre une zone du jeu (sol du puits, flanc, paroi, fond du campement) a HS texels
   par pixel du jeu. Chaque texel porte une couleur, une hauteur (en pixels du jeu), une brillance,
   une lumiere propre (masque 0..1 + code d'animation, comme le sol du jeu). Les reliefs donnent les
   normales ; l'occlusion ambiante est cuite dans la couleur. */
#ifndef HDGEN_H
#define HDGEN_H
#include "game.h"
#include <math.h>

extern int HS;                   /* texels par pixel du jeu (3 : 1080p, 6 : 4K) */

typedef struct { float r,g,b; } C3;
static inline C3 hx(uint32_t h){ C3 c={((h>>16)&255)/255.0f,((h>>8)&255)/255.0f,(h&255)/255.0f}; return c; }
static inline C3 c3(float r,float g,float b){ C3 c={r,g,b}; return c; }
static inline C3 cmix(C3 a,C3 b,float t){ return c3(a.r+(b.r-a.r)*t,a.g+(b.g-a.g)*t,a.b+(b.b-a.b)*t); }
static inline C3 csc(C3 a,float s){ return c3(a.r*s,a.g*s,a.b*s); }
static inline C3 cadd(C3 a,C3 b){ return c3(a.r+b.r,a.g+b.g,a.b+b.b); }
static inline float sat1(float x){ return x<0?0:(x>1?1:x); }
static inline float sst(float a,float b,float x){ float t=sat1((x-a)/(b-a)); return t*t*(3-2*t); }

typedef struct {
  int gw,gh,w,h; bool wrap;
  float *c, *ht, *sp, *em; uint8_t *cd;
} Cv;

/* hasard de l'atelier (independant du jeu) */
void hg_seed(uint32_t s);
float hg_rand(void);
static inline float hg_range(float a,float b){ return a+(b-a)*hg_rand(); }
static inline int hg_irange(int a,int b){ return a+(int)(hg_rand()*(b-a+1)*0.99999f); }
static inline bool hg_chance(float p){ return hg_rand()<p; }

/* toile */
Cv   cv_new(int gw,int gh,bool wrap);
void cv_free(Cv *v);
static inline int cv_wy(const Cv *v,int y){ return v->wrap ? ((y%v->h)+v->h)%v->h : (y<0?0:(y>=v->h?v->h-1:y)); }
static inline int cv_i(const Cv *v,int x,int y){ x=x<0?0:(x>=v->w?v->w-1:x); return cv_wy(v,y)*v->w+x; }
static inline C3 cv_col(const Cv *v,int i){ return c3(v->c[i*3],v->c[i*3+1],v->c[i*3+2]); }
static inline void cv_set(Cv *v,int i,C3 c){ v->c[i*3]=c.r; v->c[i*3+1]=c.g; v->c[i*3+2]=c.b; }
float cv_ht_at(const Cv *v,float gx,float gy);       /* hauteur sous un point (pixels du jeu) */

/* bruits raccordables : periode verticale P (pixels du jeu, 0 = aucune) ; valeurs -1..1 */
float gnoise(float x,float y,int py,uint32_t s);
float fbm2(float x,float y,float cell,int oct,uint32_t s,int P);
float ridge2(float x,float y,float cell,int oct,uint32_t s,int P);   /* 0..1, aretes */
typedef struct { float f1,f2; uint32_t id; float px,py; } Vo;
Vo voro(float x,float y,float cell,int P,uint32_t s,float jit);
static inline uint32_t hh3(int x,int y,uint32_t s){ uint32_t h=(uint32_t)x*0x8da6b343u ^ (uint32_t)y*0xd8163841u ^ s*0xcb1ab31fu; h^=h>>13; h*=0x5bd1e995u; h^=h>>15; return h; }
static inline float hfl(uint32_t h){ return (h&0xFFFFFF)/16777216.0f; }

/* fragments : les accessoires sont des fonctions (x,y locaux en pixels du jeu) -> fragment */
enum { HM_ON, HM_ADD, HM_DECAL };
typedef struct { C3 c; float a, h, sp, em; int code; int hm; } Fr;
typedef void (*Shf)(float lx,float ly,void *u,Fr *f);
void stamp(Cv *v,float cx,float cy,float rx,float ry,Shf fn,void *u);
/* remplissage complet : fn appelee avec les coordonnees absolues */
void fill(Cv *v,Shf fn,void *u);

/* lumieres d'un decor (pour le jeu) */
typedef struct { int n; float x[96],y[96],r[96]; uint32_t c[96]; } Glows;
static inline void glow_add(Glows *g,float x,float y,uint32_t c,float r){ if(g->n<96){ g->x[g->n]=x; g->y[g->n]=y; g->c[g->n]=c; g->r[g->n]=r; g->n++; } }

/* finitions */
void cv_ao(Cv *v,float strength,float radius);
void cv_grain(Cv *v,float amt,uint32_t seed);

/* accessoires communs (hdgen_acc.c) */
void acc_rock(Cv *v,float x,float y,float r,C3 c0,C3 c1,float h,uint32_t s);
void acc_pebbles(Cv *v,float x,float y,float r,int n,C3 c,uint32_t s);
void acc_skull(Cv *v,float x,float y,float sc,float ang,C3 c);
void acc_bone(Cv *v,float x0,float y0,float x1,float y1,float r,C3 c);
void acc_candle(Cv *v,float x,float y,float r,float h,int code,Glows *g,float gx,float gy);
void acc_mushroom(Cv *v,float x,float y,float r,bool teal,int code);
void acc_crystal(Cv *v,float x,float y,float r,float h,float ang,C3 lit,C3 dark,float em,int code);
void acc_crystals(Cv *v,float x,float y,float r,int n,C3 lit,C3 dark,float em,int code,uint32_t s);
void acc_grass(Cv *v,float x,float y,float r,int n,C3 c0,C3 c1,float len,uint32_t s);
void acc_flower(Cv *v,float x,float y,float r,C3 petal,C3 heart,int petals,float ang);
void acc_leaf(Cv *v,float x,float y,float len,float ang,C3 c);
void acc_root(Cv *v,float x,float y,float len,float w,float ang,C3 c,uint32_t s);
void acc_fern(Cv *v,float x,float y,float len,float ang,C3 c0,C3 c1,uint32_t s);
void acc_twig(Cv *v,float x,float y,float len,float ang,C3 c,uint32_t s);
void acc_crack(Cv *v,float x,float y,float len,float w,float depth,C3 c,float em,int code0,int codestep,uint32_t s);
void acc_coral(Cv *v,float x,float y,float r,C3 c,float em,int code);
void acc_lily(Cv *v,float x,float y,float r,float ang,C3 c,bool flower);
void acc_reed(Cv *v,float x,float y,float len,float ang,C3 c,bool cattail);
void acc_coin(Cv *v,float x,float y,float r,C3 c);
void acc_moss(Cv *v,float x,float y,float r,C3 c0,C3 c1,uint32_t s);
void acc_blob(Cv *v,float x,float y,float rx,float ry,float ang,C3 c,float h,float sp,float em,int code);
void acc_line(Cv *v,float x0,float y0,float x1,float y1,float w,C3 c,float h,float sp,int hm);
void acc_box(Cv *v,float x0,float y0,float x1,float y1,C3 c,float h,float bevel,float sp);

/* sols, flancs, parois, campement */
void hd_floor(Cv *v,int biome,uint32_t seed,Glows *g);
void hd_side(Cv *v,int biome,int side,uint32_t seed,Glows *g);
void hd_face(Cv *v,int biome,int side,uint32_t seed);
void hd_camp(Cv *v,uint32_t seed,Glows *g,int season);
void hd_floor_variant(Cv *v,int biome,int var,uint32_t seed,Glows *g);   /* v9 : parties du niveau et arene */
void cv_tint(Cv *v,C3 mul,C3 toward,float k);

/* palettes des flancs (scenery.c) */
void scenery_pal(int b,uint32_t rock[6],uint32_t cov[5],uint32_t *line,uint32_t rim[4],float *top,float *shelf,float *spec);
#endif
