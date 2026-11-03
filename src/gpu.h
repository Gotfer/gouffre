/* GOUFFRE - interface du rendu GPU : couches, lumieres, effets, parametres d'image */
#ifndef GPU_H
#define GPU_H
#include <stdint.h>
#include <stdbool.h>
#include "platform.h"

/* ---- couches dessinees par le CPU (640x360) ----
   scene  : couleurs de base (eclairees par le GPU)           -> g_screen
   emis   : lumiere propre, ajoutee apres l'eclairage (HDR)    -> g_emis
   ui     : interface, alpha premultiplie, jamais eclairee     -> g_ui
   nrm    : normales (rvb) + brillance speculaire (alpha) de la scene */
enum { LY_SCENE, LY_EMIS, LY_UI, LY_COUNT };
extern bool g_gpu;        /* rendu GPU actif pour cette image */
extern bool g_gpu_ok;     /* carte graphique utilisable (fixe par la plateforme) */
extern uint32_t g_emis[SCREEN_W*SCREEN_H];
extern uint32_t g_ui[SCREEN_W*SCREEN_H];
extern uint32_t g_nrm[SCREEN_W*SCREEN_H];
extern uint8_t  g_occ[SCREEN_W*SCREEN_H];   /* hauteur (+1) des sprites debout au-dessus du sol : ombres portees */
#define NRM_FLAT 0x108080FFu
void layer(int ly);
int  layer_current(void);

/* ---- lumieres dynamiques ---- */
typedef struct { float x,y,r,h; float cr,cg,cb; float namt; float sh; } GLightRec;
#define GPU_MAX_LIGHTS 2048
extern GLightRec g_lights[GPU_MAX_LIGHTS];
extern int g_nlights;

/* ---- effets lisses dessines par le GPU a pleine resolution ---- */
enum { FX_GLOW, FX_DISC, FX_RING, FX_CAPS, FX_BALL, FX_SMOKE, FX_SHAFT, FX_STAR, FX_BEAM };
typedef struct { uint8_t type; float x0,y0,x1,y1,r,p,q; uint32_t c,c2; float a; int16_t clip[4]; } FxItem;
#define GPU_MAX_FX 12000
extern FxItem g_fx[GPU_MAX_FX];
extern int g_nfx;

typedef struct {
  /* eclairage */
  int lit_on; float lit[4]; float amb[3]; float normal_amt; float spec; float shadows; float light_gain, light_range;
  /* camera : decalage sous-pixel, zoom autour de (cam_cx,cam_cy), zone echantillonnable */
  float cam_x, cam_y, cam_zoom, cam_cx, cam_cy; float view[4];
  /* deformations : ondes de choc (x,y,rayon,force), air brulant */
  float shock[8][4]; float haze;
  /* emission et brouillard volumetrique (couleur + densite), voile sombre du haut */
  float emis_gain; float fog[4]; float fog_dark, fog_speed, scroll;
  /* image */
  float bloom, bloom_thr, exposure, sat, con, vig;
  float sh_tint[3], hi_tint[3], tint_amt;
  float flash[4]; float chroma, grain;
  float dof, dof_y0, dof_y1, blur_all;
  float lowhp, desat, time;
  float scan;            /* lignes de balayage facon ecran cathodique (0 = aucune) */
  float p25;             /* vue 2.5D du puits : force de l'inclinaison (0 = vue de dessus) */
} GpuParams;
extern GpuParams GP;
void gpu_frame_begin(void);
/* reglage de qualite : 0 = performance, 1 = elevee, 2 = ultra (resolution native) */
extern int g_gpu_quality;

/* effets (GPU, ou primitives CPU en repli) - coordonnees en pixels du jeu, a = 0..1 (plus = HDR) */
void fx_glow(float x,float y,float r,uint32_t c,float a);
void fx_dust(float x,float y,float r,uint32_t c,float a);    /* poussiere : ne se voit que dans la lumiere */
void fx_disc(float x,float y,float r,uint32_t c,float a);
void fx_ring(float x,float y,float r,float w,uint32_t c,float a);
void fx_caps(float x0,float y0,float x1,float y1,float r,uint32_t c,float a);
void fx_ball(float x,float y,float r,int kind,float a);
void fx_smoke(float x,float y,float r,uint32_t c,float a);
void fx_shaft(float x0,float y0,float x1,float y1,float w,uint32_t c,float a);
void fx_star(float x,float y,float r,uint32_t c,float a);
void fx_beam(float x,float y0,float y1,float w,uint32_t c,float a);
void fx_orb(float x,float y,float r,uint32_t c,float a);      /* projectile ennemi : corps opaque, bord sombre, coeur blanc */

/* ---- cote plateforme ---- */
typedef void *(*GpuGetProc)(const char *name);
bool gpu_init(GpuGetProc gp,char *log,int logn);
void gpu_render(unsigned fbo,int win_w,int win_h,int vx,int vy,int vw,int vh);
bool gpu_icons_active(void);        /* icones en pleine definition composees par le GPU */
bool gpu_failed(void);              /* erreur OpenGL detectee pendant le rendu */
void gpu_reset_state(void);         /* remet l'etat OpenGL fixe (pour l'affichage logiciel de repli) */
void gpu_shutdown(void);
const char *gpu_renderer_name(void);
/* banc de test : cible hors ecran + relecture */
unsigned gpu_test_target(int w,int h);
void gpu_test_read(int w,int h,uint8_t *rgb);
#endif
