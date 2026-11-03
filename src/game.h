/* GOUFFRE - declarations communes */
#ifndef GAME_H
#define GAME_H
#include "platform.h"
#include "gpu.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#define PI  3.14159265358979f
#define TAU 6.28318530717958f
#define COUNTOF(a) ((int)(sizeof(a)/sizeof((a)[0])))
#define HEX(x) (0xFF000000u|(uint32_t)(x))

static inline float clampf(float v,float a,float b){return v<a?a:(v>b?b:v);}
static inline int   clampi(int v,int a,int b){return v<a?a:(v>b?b:v);}
static inline float lerpf(float a,float b,float t){return a+(b-a)*t;}
static inline float approachf(float v,float t,float d){if(v<t){v+=d;if(v>t)v=t;}else{v-=d;if(v<t)v=t;}return v;}
static inline float sqrf(float x){return x*x;}
static inline float signf(float x){return x<0?-1.f:1.f;}
static inline float maxf(float a,float b){return a>b?a:b;}
static inline float minf(float a,float b){return a<b?a:b;}
static inline int mini(int a,int b){return a<b?a:b;}
static inline int maxi(int a,int b){return a>b?a:b;}

/* ---------------- aleatoire ---------------- */
void     rng_seed(uint32_t s);
uint32_t rng_next(void);
float    frand(void);
float    frange(float a,float b);
int      irand(int n);
int      irange(int a,int b);
bool     chance(float p);
/* aleatoire reserve a l'affichage (n'influence pas la partie) */
float    rfrand(void);
float    rfrange(float a,float b);
int      rirand(int n);

/* ---------------- couleurs ---------------- */
static inline int cr(uint32_t c){return (int)((c>>16)&255);}
static inline int cg(uint32_t c){return (int)((c>>8)&255);}
static inline int cb(uint32_t c){return (int)(c&255);}
static inline uint32_t rgb(int r,int g,int b){
  r=clampi(r,0,255); g=clampi(g,0,255); b=clampi(b,0,255);
  return 0xFF000000u|((uint32_t)r<<16)|((uint32_t)g<<8)|(uint32_t)b; }
uint32_t col_lerp(uint32_t a,uint32_t b,float t);
uint32_t col_scale(uint32_t c,float f);

/* ---------------- rendu ---------------- */
typedef struct { uint32_t *px; int w,h; int cx0,cy0,cx1,cy1; int alpha; } Target;
typedef struct { int w,h; uint32_t *px; uint32_t *nm; uint32_t *em; } Sprite;   /* nm : normales, em : lumiere propre */

extern uint32_t g_screen[SCREEN_W*SCREEN_H];
extern Target g_scr;
extern Target *gT;

void gfx_init(void);
void gfx_target(Target *t);
void gfx_clip(int x0,int y0,int x1,int y1);
void gfx_noclip(void);
void gfx_clear(uint32_t c);
void gfx_pset(int x,int y,uint32_t c);
void gfx_pblend(int x,int y,uint32_t c,int a);
void gfx_padd(int x,int y,uint32_t c,int a);
void gfx_rect(int x,int y,int w,int h,uint32_t c);
void gfx_rect_a(int x,int y,int w,int h,uint32_t c,int a);
void gfx_rect_add(int x,int y,int w,int h,uint32_t c,int a);
void gfx_frame(int x,int y,int w,int h,uint32_t c);
void gfx_hline(int x0,int x1,int y,uint32_t c);
void gfx_vline(int x,int y0,int y1,uint32_t c);
void gfx_line(int x0,int y0,int x1,int y1,uint32_t c);
void gfx_line_a(int x0,int y0,int x1,int y1,uint32_t c,int a,bool additive);
void gfx_thick_line(float x0,float y0,float x1,float y1,float r,uint32_t c);
void gfx_disc(int cx,int cy,int r,uint32_t c);
void gfx_disc_a(int cx,int cy,int r,uint32_t c,int a);
void gfx_disc_add(int cx,int cy,int r,uint32_t c,int a);
void gfx_ring(int cx,int cy,int r,uint32_t c);
void gfx_ring_a(int cx,int cy,int r,uint32_t c,int a,bool additive);
void gfx_ellipse(int cx,int cy,int rx,int ry,uint32_t c);
void gfx_ellipse_a(int cx,int cy,int rx,int ry,uint32_t c,int a);
void gfx_glow(float cx,float cy,float r,uint32_t c,float inten);
void gfx_darken(int x,int y,int w,int h,int a);
void gfx_gradient_v(int x,int y,int w,int h,uint32_t top,uint32_t bot);
void gfx_dither_rect(int x,int y,int w,int h,uint32_t c,int level);
void gfx_tri(float x0,float y0,float x1,float y1,float x2,float y2,uint32_t c);

Sprite spr_alloc(int w,int h);
Target spr_target(Sprite *s);
void   spr_clear(Sprite *s);
void   spr_outline(Sprite *s,uint32_t col);
Sprite spr_from_art(const char *const *rows,int nrows,bool mirror,const char *swap);
Sprite spr_art_raw(const char *const *rows,int nrows,bool mirror,const char *swap);
Sprite spr_copy(const Sprite *a);
void   spr_shade(Sprite *s,float amount);
void   spr_outline_tinted(Sprite *s);
void   spr_lift_foot(Sprite *s,int side);
void   spr_make_normals(Sprite *s,float bump);
void   spr_emis_colors(Sprite *s,const uint32_t *cols,int n,float k);
void   nrm_from_height(const float *h,int w,int hgt,uint32_t *out,float k,const uint8_t *spec,bool wrapy);
void gfx_capsule_add(float x0,float y0,float x1,float y1,float r,uint32_t c,int a);
void gfx_tri_add(float x0,float y0,float x1,float y1,float x2,float y2,uint32_t c,int a);
float vnoise(float x,float y,uint32_t seed);
float fbm(float x,float y,uint32_t seed);
/* post-traitement */
void post_bloom(int x0,int y0,int x1,int y1,int thresh,float strength,float wide);
void post_grade(int x0,int y0,int x1,int y1,float sat,float contrast,uint32_t shadow,float shadow_amt,float vig);
void post_shockwave(float cx,float cy,float rad,float thick,float str,int cx0,int cy0,int cx1,int cy1);
void post_haze(int x0,int y0,int x1,int y1,float t,float amp);

enum { DS_FLIPX=1, DS_FLIPY=2, DS_ADD=4, DS_NOOCC=8 };
typedef struct { int flags; float sx,sy; uint32_t tint; int alpha; uint32_t flash; int flash_a; float lift; } DrawOpt;   /* lift : hauteur au-dessus du sol (ombres) */
DrawOpt dopt(void);
void draw_spr(const Sprite *s,int x,int y,int flags);
void draw_spr_c(const Sprite *s,float cx,float cy,const DrawOpt *o);
void draw_spr_scaled(const Sprite *s,int x,int y,int scale,uint32_t tint);

/* texte (police pixel maison, UTF-8, accents) */
enum { AL_LEFT=0, AL_CENTER=1, AL_RIGHT=2 };
enum { TX_SHADOW=1, TX_OUTLINE=2 };
bool text_is_number(const char *s);                 /* chiffres seuls (avec ! + -) */
int  text_big_digits(int x,int y,const char *s,uint32_t col,int scale,int align);   /* chiffres gras 7x10 */
#define TXT_H 11
void font_init(void);
int  text_w(const char *s,int scale);
void text_vext(const char *s,int *top,int *bot);   /* lignes occupees (relatives au y de text_draw) */
int  text_draw(int x,int y,const char *s,uint32_t col,int scale,int align,int fx);
int  text_wrap(int x,int y,int w,const char *s,uint32_t col,int scale,int fx,int lh);
int  text_wrap_count(const char *s,int w,int scale);
/* icones dans la police */
#define IC_HEART "\xEE\x80\x80"
#define IC_COIN  "\xEE\x80\x81"
#define IC_GEM   "\xEE\x80\x82"
#define IC_SKULL "\xEE\x80\x83"
#define IC_STAR  "\xEE\x80\x84"
#define IC_ARROW "\xEE\x80\x85"
#define IC_CLOCK "\xEE\x80\x86"
#define IC_BALL  "\xEE\x80\x87"
#define IC_PS_CROSS  "\xEE\x80\x88"
#define IC_PS_CIRCLE "\xEE\x80\x89"
#define IC_PS_TRI    "\xEE\x80\x8A"
#define IC_PS_SQUARE "\xEE\x80\x8B"
#define IC_PRISM "\xEE\x80\x8C"      /* v6.6 : prisme (dessine en arc-en-ciel) */

/* police gothique des titres (gothic.c, d'apres Jacquard 24 / 12, licence OFL) */
int gothic_draw(int x,int y,const char *s,uint32_t ctop,uint32_t cbot,int size,int align,int fx,uint32_t oc);
int gothic_title(int cx,int y,const char *s,int size);
int gothic_w(const char *s,int size);
int gothic_h(int size);
int gothic_asc(int size);

/* eclairage */
void light_begin(int x0,int y0,int x1,int y1,int amb_r,int amb_g,int amb_b);
void light_add(float x,float y,float r,uint32_t c,float inten);
void light_apply(void);
void light_gain(float g);
void light_floor(int x,int y,int v);
uint16_t *light_chan(int ch);

/* ---------------- graphismes du jeu ---------------- */
enum {
  SP_SENTINELLE, SP_ALCHIMISTE, SP_JONGLEUR, SP_FORGERONNE, SP_MAGNETISEUR, SP_NECRO, SP_HORLOGERE, SP_PYROMANE,
  SP_DRUIDE, SP_MARCHAND,      /* v7 : les sprites des heros gardent les numeros des heros */
  SP_ASTRONOME, SP_VOLEUSE,    /* v9 */
  SP_CHASSERESSE, SP_GEOMANCIEN,   /* v10 */
  SP_SQUELETTE, SP_CHAUVE1, SP_CHAUVE2, SP_GELEE, SP_GELETTE, SP_CRANE, SP_OSSUAIRE,
  SP_CHAMPIGNON, SP_ARAIGNEE, SP_SCARABEE, SP_GELEE_TOX, SP_GELETTE_TOX, SP_SPORANGE,
  SP_DIABLOTIN, SP_MAGMITE, SP_ARMURE, SP_CHAUVEF1, SP_CHAUVEF2, SP_GOLEM,
  SP_OEIL, SP_SPECTRE, SP_CHEVALIER, SP_OBELISQUE, SP_HORREUR, SP_TENTACULE,
  SP_FOLLET1, SP_FOLLET2, SP_MANTE, SP_CRACHEUR, SP_SENTVIDE,
  /* v4 : les Profondeurs */
  SP_LOUP, SP_ESPRIT, SP_TORTUE, SP_GELEE_GIVRE, SP_GELETTE_GIVRE, SP_HARFANG1, SP_HARFANG2, SP_YETI,
  SP_NOYE, SP_MURENE, SP_CRABE, SP_MEDUSE, SP_SIRENE, SP_KRAKEN,
  SP_OUVRIERE1, SP_OUVRIERE2, SP_SOLDAT, SP_LARVE, SP_LARVULE, SP_FRELON, SP_GARDIENNE, SP_COCON, SP_BOURDON,
  SP_OMBRE, SP_ECHO, SP_ECHO_MINEUR, SP_OEIL_NEANT, SP_DEVOREUR, SP_HERAUT, SP_COLOSSE_NEANT,
  /* v7 : le Royaume oublie */
  SP_LUCIOLE, SP_FLEUR, SP_ESCARGOT, SP_PAPILLON1, SP_PAPILLON2, SP_SYLVAIN,
  SP_CRAPAUD, SP_SANGSUE, SP_SORCIERE, SP_GELEE_MARAIS, SP_GELETTE_MARAIS, SP_MOUSTIQUE1, SP_MOUSTIQUE2, SP_HYDRE,
  SP_GOLEM_CRISTAL, SP_MINEUR, SP_VER, SP_ARAIGNEE_CRISTAL, SP_CHAUVEC1, SP_CHAUVEC2, SP_QUARTZ,
  SP_PRETRE, SP_SCARABEE_OR, SP_MOMIE, SP_PHENIX1, SP_PHENIX2, SP_GARDIEN_SOL, SP_SPHINX,
  SP_ICON_FORCE, SP_ICON_CADENCE, SP_ICON_BOTTES, SP_ICON_COEUR, SP_ICON_RESSORT,
  SP_ICON_SAC, SP_ICON_LYNX, SP_ICON_LANTERNE, SP_ICON_MOUSSE, SP_ICON_FILET, SP_ICON_TREFLE,
  SP_ICON_BOUCLIER, SP_ICON_RAGE, SP_ICON_FIOLE, SP_ICON_MIROIR,
  SP_ICON_CAPE, SP_ICON_LOUPE, SP_ICON_ANCRE, SP_ICON_TOTEM,
  SP_COUNT
};
extern Sprite SPR[SP_COUNT];
extern Sprite SPR_RAW[SP_COUNT];      /* sans contour (eclats a la mort) */
extern Sprite SPR_WALK[SP_COUNT][2];  /* images de marche (px==NULL si aucune) */
extern Sprite SPR_LO[SP_COUNT];       /* personnages a l'ancienne taille (vignettes) */
extern Sprite SPR_LO_WALK[SP_COUNT][2];
#define EN_SCALE 2                    /* les personnages sont dessines (et touches) deux fois plus grands */
#define EN_HP 2.0f                    /* ... et deux fois moins nombreux par ligne : plus resistants */
#define EN_XP 2                       /* experience et or par ennemi doubles (meme progression) */
#define EN_COIN 2.0f
extern Sprite SPR_BALL[64];      /* icones de billes 16x16 */
extern Sprite SPR_BLD[16];       /* batiments du campement */
extern Sprite SPR_HUT[16];       /* maisons des heros */
void art_init(void);
void portraits_init(void);                       /* portraits peints des heros (64x64) */
const Sprite *portrait(int ch,bool locked);
void art_draw_event(int ev,int cx,int cy,float t);
void art_draw_boss(int type,float cx,float cy,float t,float flash,float anim,float look_x,float look_y,int phase);
void art_draw_tentacle(float bx,float by,float t,float len,uint32_t c1,uint32_t c2,float ang);
void art_draw_ball(float x,float y,float r,int kind,float t,bool glow);
void art_ball_icon(Sprite *s,int kind);

/* ---------------- audio ---------------- */
enum {
  SFX_SHOOT, SFX_BOUNCE, SFX_HIT, SFX_KILL, SFX_GEM, SFX_COIN, SFX_LEVELUP, SFX_HURT,
  SFX_EXPLODE, SFX_ZAP, SFX_FREEZE, SFX_BOSS_ROAR, SFX_BOSS_DIE, SFX_CLICK, SFX_HOVER,
  SFX_SELECT, SFX_BREACH, SFX_ENEMY_SHOT, SFX_LASER_CHARGE, SFX_LASER, SFX_SPLIT, SFX_HEAL,
  SFX_UPGRADE, SFX_DENY, SFX_EVOLVE, SFX_WARN, SFX_SLAM, SFX_SHIELD, SFX_RETURN, SFX_REVIVE,
  SFX_ACHIEVE, SFX_EVENT, SFX_BEAM, SFX_WIND, SFX_CRYSTAL, SFX_TIME, SFX_BLOCK, SFX_GHOST, SFX_DASH, SFX_BLINK,
  SFX_ECHO, SFX_THUNDER, SFX_ROOT, SFX_ACID, SFX_GOLD,   /* v7 */
  SFX_COUNT
};
/* INTRO / FIN / FIN2 : cinematiques (ne bouclent pas) ; LVL0.. : un theme par niveau (v6.6) */
enum { MUS_TITLE, MUS_BATTLE, MUS_BOSS, MUS_CAMP, MUS_INTRO, MUS_FIN, MUS_FIN2, MUS_LVL0, MUS_COUNT=MUS_LVL0+12 };   /* un theme par niveau (12) */
void audio_init(void);
void audio_frame(void);
void sfx_play(int id,float vol,float pan,float pitch);
void sfx(int id);
void sfx_at(int id,float x,float vol);
void music_play(int id,float rate);
void music_stop(void);
void music_prepare(int id);          /* pistes des cinematiques : construites a la premiere ecoute */
void music_levels_start(int first);  /* themes des niveaux : calcules en tache de fond (celui-ci d'abord) */
int  music_level(int biome);         /* identifiant du theme d'un niveau (calcule tout de suite s'il n'est pas commence) */
bool music_level_ready(int biome);
int  music_debug_get(int id,const int16_t **d);   /* tests : acces au tampon d'une piste */
extern volatile float g_vol_music, g_vol_sfx, g_vol_amb;
void amb_play(const char *name);          /* v9 : nappe d'ambiance (donnees/ambiances/NOM.wav), NULL = silence */
bool amb_playing(char *name,int n);

/* ---------------- donnees ---------------- */
enum {
  BK_BABY, BK_FER, BK_FEU, BK_GLACE, BK_FOUDRE, BK_VENIN, BK_EPINE, BK_BOMBE,
  BK_SPECTRE, BK_SANG, BK_ECLAT, BK_PLOMB, BK_AIMANT, BK_LUMIERE, BK_FOLLE, BK_CRISTAL, BK_RAFALE,
  BK_ECHO, BK_DOREE, BK_RACINE, BK_ACIDE,                       /* v7 */
  BK_OMBRE, BK_RICOCHET, BK_LAVE, BK_ETOILE,                    /* v9 */
  BK_BOOMERANG, BK_SEISME, BK_CHAINE,                           /* v10 */
  BK_VAPEUR, BK_METEORE, BK_TEMPETE, BK_FLEAU, BK_FAUCHEUSE, BK_PRISME, BK_SINGULARITE, BK_ENCLUME,
  BK_RAYON, BK_KALEIDO, BK_GEODE, BK_BLIZZARD, BK_TEMPFEU, BK_AURORE,
  BK_TONNERRE, BK_MIDAS, BK_RONCES, BK_PLUIE,                   /* v7 */
  BK_ECLIPSE, BK_CAUCHEMAR, BK_CARAMBOLE, BK_VOLCAN, BK_CONSTELLATION, BK_SUPERNOVA,   /* v9 */
  BK_CHAKRAM, BK_FAILLE, BK_ENTRAVE,                            /* v10 */
  BK_BIGBANG, BK_JUGEMENT, BK_ZERO, BK_OUROBOROS,               /* v9 : fusions legendaires */
  BK_APOCALYPSE,                                                /* v10 */
  BK_SHARD, BK_GHOST,
  BK_COUNT
};
/* v7 puis v9 : les numeros des billes ont change (sauvegardes converties au chargement) */
int bk_old_to_new(int k);
int bk_v9_to_v10(int k);
int bk_v10_to_v11(int k);
uint32_t evo_seen_v10_to_v11(uint32_t m);
#define BK_FIRST_BASE BK_FER
#define BK_LAST_BASE  BK_CHAINE
#define BK_FIRST_EVO  BK_VAPEUR
#define BK_LAST_EVO   BK_ENTRAVE
#define BK_FIRST_LEG  BK_BIGBANG
#define BK_LAST_LEG   BK_APOCALYPSE
#define LEG_COUNT (BK_LAST_LEG-BK_FIRST_LEG+1)
#define BASE_BALL_COUNT (BK_LAST_BASE-BK_FIRST_BASE+1)
#define STARS_MAX (3*(BK_LAST_EVO-BK_FIRST_BASE+1))   /* etoiles de maitrise : 3 par bille et par evolution */
#define EVO_COUNT (BK_LAST_EVO-BK_FIRST_EVO+1)
#define IS_EVO(k) ((k)>=BK_FIRST_EVO && (k)<=BK_LAST_LEG)   /* evolutions et fusions legendaires (pas de niveaux) */
#define IS_LEG(k) ((k)>=BK_FIRST_LEG && (k)<=BK_LAST_LEG)
typedef struct {
  const char *name;
  uint32_t col, col2;
  float r, dmg, dmg_lvl, speed;
  int evo_a, evo_b;
} BallDef;
extern const BallDef BALLS[BK_COUNT];
void ball_desc(int kind,int lvl,char *buf,int n);

enum { PS_FORCE, PS_CADENCE, PS_BOTTES, PS_COEUR, PS_RESSORT, PS_SAC, PS_LYNX, PS_LANTERNE, PS_MOUSSE, PS_FILET, PS_TREFLE,
       PS_BOUCLIER, PS_RAGE, PS_FIOLE, PS_MIROIR,
       PS_CAPE, PS_LOUPE, PS_ANCRE, PS_TOTEM,      /* v7 */
       PS_PENDULE, PS_BOUSSOLE, PS_GRIMOIRE,      /* v10 */
       PS_COUNT };
typedef struct { const char *name; int sprite; } PassiveDef;
extern const PassiveDef PASSIVES[PS_COUNT];
/* icones realistes (icongen.c : rendu 3D procedural, icons.c : atlas et affichage) */
#define ICON_N 128
#define ICON_COUNT (BK_COUNT+PS_COUNT)
#define ICON_PAS(p) (BK_COUNT+(p))
void icg_begin(int id);
bool icg_rows(int nrows);                       /* rend quelques lignes ; vrai quand l'icone est finie */
void icg_finish(uint8_t *out,int stride);       /* RGBA sRGB premultiplie, ICON_N x ICON_N */
void icg_render(int id,uint8_t *out,int stride);
void icons_init(void);                          /* lance la generation (tache de fond si possible) */
void icons_frame(void);                         /* a chaque image : suit la generation */
bool icons_ready(void);
void icon_draw(int id,float cx,float cy,float size,const DrawOpt *o);   /* icone centree, taille en pixels du jeu */
int  icons_frame_id(void);
int  icons_atlas_version(void);                 /* 0 tant que l'atlas n'est pas pret */
const uint8_t *icons_atlas_level(int l,int *w,int *h);
int  icons_atlas_levels(void);
int  icons_gpu_prepare(uint8_t *mask,uint8_t *idata);
bool gfx_target_is_ui(void);
void passive_desc(int kind,int lvl,char *buf,int n);

enum EnemyFlags { EF_SHOOT=1, EF_ZIGZAG=2, EF_SPLIT=4, EF_ARMOR=8, EF_SHIELD=16, EF_PHASE=32,
  EF_REGEN=64, EF_HEAL=128, EF_ELITE=256, EF_FLY=512, EF_EMBERS=1024, EF_PLATE=2048, EF_DRIFT=4096,
  EF_BLINK=8192, EF_DASH=16384, EF_LOB=32768, EF_ORBS=65536,
  EF_FROST=1<<17, EF_DIVE=1<<18, EF_SUMMON=1<<19 };
/* l'ordre ne doit jamais changer (indices sauvegardes) : on ajoute a la fin */
enum {
  EK_SQUELETTE, EK_CHAUVE, EK_GELEE, EK_GELETTE, EK_CRANE, EK_OSSUAIRE,
  EK_CHAMPIGNON, EK_ARAIGNEE, EK_SCARABEE, EK_GELEE_TOX, EK_GELETTE_TOX, EK_SPORANGE,
  EK_DIABLOTIN, EK_MAGMITE, EK_ARMURE, EK_CHAUVE_FEU, EK_GOLEM,
  EK_OEIL, EK_TENTACULE, EK_SPECTRE, EK_CHEVALIER, EK_OBELISQUE, EK_HORREUR,
  EK_FOLLET, EK_MANTE, EK_CRACHEUR, EK_SENTVIDE,
  /* v4 : Grottes de Givre, Cite Engloutie, Ruche d'Ambre, Neant */
  EK_LOUP, EK_ESPRIT, EK_TORTUE, EK_GELEE_GIVRE, EK_GELETTE_GIVRE, EK_HARFANG, EK_YETI,
  EK_NOYE, EK_MURENE, EK_CRABE, EK_MEDUSE, EK_SIRENE, EK_KRAKEN,
  EK_OUVRIERE, EK_SOLDAT, EK_LARVE, EK_LARVULE, EK_FRELON, EK_GARDIENNE, EK_COCON, EK_BOURDON,
  EK_OMBRE, EK_ECHO, EK_ECHO_MINEUR, EK_OEIL_NEANT, EK_DEVOREUR, EK_HERAUT, EK_COLOSSE_NEANT,
  /* v7 : le Royaume oublie (Jardin des Lucioles, Marais maudit, Mines de cristal, Sanctuaire solaire) */
  EK_LUCIOLE, EK_FLEUR, EK_ESCARGOT, EK_PAPILLON, EK_SYLVAIN,
  EK_CRAPAUD, EK_SANGSUE, EK_SORCIERE, EK_GELEE_MARAIS, EK_GELETTE_MARAIS, EK_MOUSTIQUE, EK_HYDRE,
  EK_GOLEM_CRISTAL, EK_MINEUR, EK_VER, EK_ARAIGNEE_CRISTAL, EK_CHAUVE_CRISTAL, EK_QUARTZ,
  EK_PRETRE, EK_SCARABEE_OR, EK_MOMIE, EK_PHENIX, EK_GARDIEN_SOL, EK_SPHINX,
  EK_COUNT
};
#define EK_FIRST_V4 EK_LOUP
#define EK_FIRST_V7 EK_LUCIOLE
typedef struct {
  const char *name; float hp, speed; int w,h; int sprite, sprite2; int flags;
  float shoot_cd; int xp; float coin; float breach, contact; uint32_t blood; int child;
  const char *lore;
} EnemyDef;
extern const EnemyDef ENEMIES[EK_COUNT];
void enemy_traits(int kind,char *buf,int n);

enum { BOSS_ROI, BOSS_MERE, BOSS_COLOSSE, BOSS_OEIL, BOSS_GIVRE, BOSS_LEVIATHAN, BOSS_RUCHE, BOSS_NEANT,
       BOSS_ARBRE, BOSS_CRAPAUD, BOSS_PRISME, BOSS_SOLEIL,      /* v7 : le Royaume oublie */
       BOSS_COUNT };
typedef struct { const char *name; float hp; float w,h; float armor; const char *lore; } BossDef;
extern const BossDef BOSSES[BOSS_COUNT];

/* trois destinations de 4 niveaux : le Gouffre (biomes 0-3), les Profondeurs (biomes 4-7)
   et, depuis la v7, le Royaume oublie (biomes 8-11, ouvert en meme temps que les Profondeurs) */
#define BIOME_COUNT 12
#define LEVELS_PER_DEST 4
#define DEST_COUNT 3
#define DEPTH_MAX 10
typedef struct {
  const char *name;
  uint32_t floor_a, floor_b, floor_c, wall_a, wall_b, accent, amb;
  int pool[6]; int pool_n; int elite; int boss;
  float duration, hp_mult, speed, music_rate;
} BiomeDef;
extern const BiomeDef BIOMES[BIOME_COUNT];

enum { CH_SENTINELLE, CH_ALCHIMISTE, CH_JONGLEUR, CH_FORGERONNE, CH_MAGNETISEUR, CH_NECRO, CH_HORLOGERE, CH_PYROMANE,
       CH_DRUIDE, CH_MARCHAND,      /* v7 */
       CH_ASTRONOME, CH_VOLEUSE,    /* v9 */
       CH_CHASSERESSE, CH_GEOMANCIEN,   /* v10 */
       CH_COUNT };
typedef struct {
  const char *name, *title, *trait, *desc;
  int sprite, start_ball; float hp, speed, dmg, fire; int babies; int cost; uint32_t col;
  int unlock_ach;   /* -1 : achat en cristaux ; sinon : defi a accomplir */
} CharDef;
extern const CharDef CHARS[CH_COUNT];

enum { BLD_FORGE, BLD_INFIRMERIE, BLD_ATELIER, BLD_BIBLIO, BLD_TRESOR, BLD_PUITS, BLD_SANCTUAIRE, BLD_TOUR, BLD_LABO,
       BLD_AUTEL, BLD_ARMURERIE, /* v7 */ BLD_SERRE, BLD_OBSERV, /* v9 */ BLD_CREUSET, BLD_CABINET, BLD_COUNT };
typedef struct { const char *name, *desc; int max; int base_cost; int x, y; } BuildingDef;
extern const BuildingDef BUILDINGS[BLD_COUNT];
int  building_cost(int b,int lvl);
int  building_crystals(int b,int lvl);   /* niveaux superieurs : aussi des cristaux */
void building_effect(int b,int lvl,char *buf,int n);

/* ---------------- maquettes voxel des batiments (voxel.c, vmodels.c, batiments.c) ---------------- */
enum { VM_NONE, VM_SOLID, VM_GLOW, VM_METAL, VM_FIRE, VM_WATER, VM_FLAT, VM_LEAF };
#define VC(m,c) ((((uint32_t)(m))<<24)|((uint32_t)(c)&0xFFFFFFu))
enum { EMI_SMOKE, EMI_SPARK, EMI_GREEN, EMI_MAGIC, EMI_STEAM };
#define VOX_MAX_EMIT 12
typedef struct { float x,y,z; int kind; } VoxEmit;
typedef struct {
  int x0,y0,z0,nx,ny,nz,cap;     /* boite englobante dans le repere de travail */
  uint32_t *c, *n;               /* couleur + matiere (octet haut, 0 = vide) ; normale lissee + ouverture */
  uint8_t *blk; int bnx,bny,bnz,bcap;   /* blocs de 4x4x4 non vides (saut de l'espace vide) */
  float px,py,pz,fz;             /* pivot (centre du batiment, au sol) ; facade avant */
  int nem; VoxEmit em[VOX_MAX_EMIT];
} VoxModel;
typedef struct { float yaw,pitch,scale,cx,cy; } VoxCam;
typedef struct { int x0,y0,z0,x1,y1,z1; } VBox;
enum { F_FRONT, F_BACK, F_LEFT, F_RIGHT };
enum { VR_PREVIEW=1, VR_SMOOTH=2 };   /* SMOOTH : normales lissees et occlusion douce (gros plans) */
void vx_begin(void);
void vx_set(int x,int y,int z,uint32_t c);
uint32_t vx_get(int x,int y,int z);
void vx_paint(int x,int y,int z,uint32_t c);
void vx_emit(float x,float y,float z,int kind);
void vx_box(int x0,int y0,int z0,int x1,int y1,int z1,uint32_t c);
uint32_t vhash(int x,int y,int z);
void vx_bricks(int x0,int y0,int z0,int x1,int y1,int z1,uint32_t c,uint32_t cm);
void vx_planks(int x0,int y0,int z0,int x1,int y1,int z1,uint32_t c,uint32_t cd);
void vx_stones(int x0,int y0,int z0,int x1,int y1,int z1,uint32_t c,uint32_t cd);
void vx_roof(int x0,int x1,int y0,int z0,int z1,int h,int axe,uint32_t c,uint32_t cd,int over);
void vx_roof_hip(int x0,int x1,int y0,int z0,int z1,int h,uint32_t c,uint32_t cd,int over);
void vx_cyl(float cx,float cz,float r,int y0,int y1,uint32_t c);
void vx_cyl_stones(float cx,float cz,float r,int y0,int y1,uint32_t c,uint32_t cd);
void vx_cone(float cx,float cz,float r,int y0,int h,uint32_t c,uint32_t cd);
void vx_sphere(float cx,float cy,float cz,float r,uint32_t c);
void vx_blob(float cx,float cy,float cz,float rx,float ry,float rz,uint32_t c,uint32_t c2);
void vx_line(float x0,float y0,float z0,float x1,float y1,float z1,uint32_t c);
void vx_face(const VBox *b,int face,int u,int y,int d,uint32_t c);
int  vx_face_w(const VBox *b,int face);
void vx_face_rect(const VBox *b,int face,int u0,int y0,int u1,int y1,int d,uint32_t c);
void vx_window(const VBox *b,int face,int u,int y,int w,int h,uint32_t glass,uint32_t frame);
void vx_door(const VBox *b,int face,int u,int y,int w,int h,uint32_t c,uint32_t cd);
bool vx_bounds(int *x0,int *y0,int *z0,int *x1,int *y1,int *z1);
void vx_end(VoxModel *m,float pivx,float pivz);
void vox_free(VoxModel *m);
void vox_mark_new(VoxModel *m,const VoxModel *ref);
void vox_compose(const VoxModel *old,const VoxModel *nw,float sweep,VoxModel *out);
void vox_basis(const VoxCam *cam,float f[3],float r[3],float u[3]);
float vox_project(const VoxModel *m,const VoxCam *cam,float X,float Y,float Z,float *sx,float *sy);
void vox_fit(const VoxModel *m,float pitch,int w,int h,float maxscale,float *scale,float *cyoff);
int  vox_hull(const VoxModel *m,float **pts);
void vox_fit_pts(const VoxModel *m,const float *p,int n,float pitch,int w,int h,float maxscale,float *scale,float *cyoff);
void vox_render(const VoxModel *m,float time,int flags,const VoxCam *cam,Sprite *out);
void vox_set_light(float x,float y,float z);   /* (0,0,0) : lumiere par defaut */
void vox_blit(const Sprite *img,int x0,int y0,float glow);
void vox_camp_sprite(const VoxModel *m,Sprite *s,int *ax,int *ay);
void vox_camp_point(const VoxModel *m,float X,float Y,float Z,float *sx,float *sy);
/* modeles (vmodels.c) : niveau 0 = chantier ; socle = maquette posee sur un carre d'herbe */
void bld_model(int b,int lvl,bool socle,VoxModel *m);
const char *bld_visual(int b,int lvl);       /* ce que le niveau ajoute au batiment */
/* batiments.c : sprites du campement tires des maquettes, ecran « Amélioration » */
void bld_sprites_refresh(void);
const Sprite *bld_sprite(int b,int *ax,int *ay);
void bld_camp_rect(int b,int x,int y,int *rx,int *ry,int *rw,int *rh);
void bld_camp_draw(int b,int x,int y,const DrawOpt *o);
void bld_camp_fx(int b,int x,int y);
void upgrade_open(int b,int ret);
void upgrade_select(int b);
bool upgrade_busy(void);
void upgrade_enter(void);
void upgrade_frame(float dt);
/* vitesse de jeu (run.c) */
float game_speed_mul(int k);
void speed_buttons_rect(int i,int *x,int *y,int *w,int *h);
void vox_debug_sheet(const char *path);      /* planche de controle (tests) */

/* ---------------- modes de jeu ---------------- */
enum { MODE_NORMAL, MODE_ENDLESS, MODE_DAILY, MODE_RUSH, MODE_COUNT };
enum { MOD_VERRE, MOD_RUEE, MOD_OR, MOD_BLINDES, MOD_JONGLE, MOD_RAGE, MOD_BROUILLARD, MOD_CHAMPIONS,
       MOD_HORDE, MOD_SANS_SOIN, MOD_FRAGILE, MOD_ELITES,   /* v10 : mutateurs */
       MOD_COUNT };
#define MOD_DAILY_N 8          /* le defi du jour tire parmi les 8 premiers */
#define MUT_UNLOCK MODE_COUNT  /* recompense de defi : les mutateurs (pas un mode) */
bool mut_unlocked(void);
int  ach_for_reward(int rw,int val);
int  mut_count(uint32_t m);
int  mut_bonus(uint32_t m);    /* bonus d'or et d'experience (%) */
typedef struct { const char *name, *desc; } ModDef;
/* niveaux de difficulte (reglage du joueur, modifiable a tout moment) */
enum { DIFF_FACILE, DIFF_NORMALE, DIFF_DIFFICILE, DIFF_COUNT };
typedef struct { const char *name, *desc, *brief; float en_hp, en_dmg, boss_hp, speed, reward, boss_heal; uint32_t col; } DiffDef;
extern const DiffDef DIFFS[DIFF_COUNT];
const DiffDef *diff_cur(void);
extern const ModDef MODS[MOD_COUNT];

/* ---------------- defis ---------------- */
#define ACH_MAX 256
enum { RW_GOLD, RW_CRYSTAL, RW_BALL, RW_PAS, RW_HERO, RW_MODE };
typedef struct { const char *name, *desc; int stat; int param; int target; int rw; int rw_val; } AchDef;
extern const AchDef ACHS[];
extern const int ACH_COUNT;

/* ---------------- entrainement des heros (v6.6, entrainement.c) ----------------
   Les prismes, gagnes sur les gardiens, font monter six attributs propres a chaque heros.
   Chaque attribut a son propre cout, qui grandit a chaque niveau ; il n'y a pas de niveau maximum. */
enum { TR_VITA, TR_PUIS, TR_CADE, TR_VITE, TR_PREC, TR_SAVO, TR_COUNT };
#define TRAIN_CAP 9999       /* simple garde-fou de la sauvegarde */

/* ---------------- sauvegarde ---------------- */
typedef struct {
  int gold, crystals;
  int bld[BLD_COUNT];
  bool unlocked[CH_COUNT];
  int depth_max, depth_sel, last_char;
  int runs, wins, best_level, total_kills;
  int vol_music, vol_sfx, vol_amb;
  bool fullscreen, pixel_perfect, shake, dmg_numbers, autoaim;
  bool pad_autoaim, rumble;
  int game_speed;                 /* vitesse de jeu : 0 = x1, 1 = x1.5, 2 = x2 */
  uint32_t evo_seen;
  /* v2 */
  bool ach[ACH_MAX];
  uint64_t balls_unlocked, balls_seen; uint32_t pas_unlocked;
  int kills_en[EK_COUNT+8]; int boss_kills[BOSS_COUNT];
  int elites, evolutions, best_combo, best_plevel, gold_total, bosses_total;
  uint32_t hero_wins; int events_done, endless_best, endless_unlocked;
  int daily_date, daily_best, daily_done, daily_wins;
  int mode_sel, bloom, quality;
  int champions;
  uint32_t feats;
  /* v4 */
  int depth_max2, depth_sel2, dest_sel;      /* profondeurs des Profondeurs (depth_max/sel : le Gouffre) */
  int depth_max3, depth_sel3;                /* v7 : profondeurs du Royaume oublie */
  int depth_won[DEST_COUNT];                 /* plus grande profondeur gagnee par destination */
  int prof_runs, prof_wins;
  int hero_xp[CH_COUNT];                     /* experience permanente des heros (rangs 1-10) */
  int ball_kills[BK_COUNT];                  /* maitrise des billes */
  int crystals_total;
  int play_time;                             /* secondes de jeu en descente */
  /* v4.1 */
  int difficulty;                            /* DIFF_FACILE / DIFF_NORMALE / DIFF_DIFFICILE */
  int diff_wins[DIFF_COUNT];                 /* victoires par difficulte */
  /* v5 */
  bool crt;                                  /* lignes de balayage (ecran cathodique) */
  bool view25;                               /* v6.5 : puits vu en 2.5D (camera inclinee) */
  bool intro_seen;                           /* cinematique d'introduction deja vue */
  int ends_seen;                             /* fins vues : bit 0 le Gouffre, bit 1 les Profondeurs */
  /* v6 : les huit niveaux se jouent un par un */
  uint16_t lvl_done[BIOME_COUNT][DEPTH_MAX];  /* heros ayant termine le niveau a cette profondeur (un bit par heros), toutes difficultes */
  uint16_t lvl_done_d[DIFF_COUNT][BIOME_COUNT][DEPTH_MAX];   /* v6.4 : les memes coches, difficulte par difficulte */
  int lvl_depth[BIOME_COUNT];                /* profondeur choisie pour chaque niveau */
  int lvl_sel, lvl_page;                     /* niveau et page selectionnes dans la liste */
  int duo_last;                              /* dernier compagnon emmene (-1 : aucun) */
  int lvl_best[BIOME_COUNT];                 /* meilleur temps (s) par niveau */
  int levels_won, mb_kills;                  /* niveaux termines, gardiens mineurs vaincus */
  int roy_wins, roy_runs;                    /* v7 : niveaux termines et descentes dans le Royaume oublie */
  int curios, curio_kills;                   /* v8 : curiosites brisees, monstres vaincus par les pieges du decor */
  int legendaries;                           /* v9 : fusions legendaires realisees */
  uint8_t obj_done[BIOME_COUNT];             /* v9 : objectifs de niveau remplis (3 bits par niveau) */
  int rush_wins, rush_dest, rush_best[DEST_COUNT], rush_sel;   /* v9 : rush des gardiens (destinations terminees en bits, records en s) */
  int mark_kills;                            /* v9 : monstres marques vaincus */
  uint32_t mut_sel;                          /* v10 : mutateurs choisis (bits) */
  int mut_wins, mut_best;                    /* v10 : niveaux gagnes avec mutateurs, record de mutateurs a la fois */
  int champ_kinds[16];                       /* v10 : champions vaincus par pouvoir */
  int quake_kills, link_kills;               /* v10 : monstres vaincus par les secousses et les chaines */
  /* v6.6 : prismes et entrainement */
  int prisms, prisms_total;                  /* prismes en poche, prismes gagnes en tout */
  uint16_t train[CH_COUNT][TR_COUNT];        /* niveau de chaque attribut, heros par heros (sans limite) */
} Save;
extern Save S;
void save_defaults(void);
void save_load(void);
void save_write(void);

/* ---------------- actions (clavier / souris / manette) ---------------- */
typedef struct {
  bool up, down, left, right;          /* navigation (avec repetition) */
  bool confirm, back, pause, reroll, toggle_aim;
  bool n[4];
  bool click, rclick, mouse_moved, mouse_down;
  float mx, my;
  float movex, movey;                  /* deplacement analogique */
  float aimx, aimy; bool aim_stick;    /* stick droit */
  bool pad; int pad_type;
  int wheel; bool tab_l, tab_r;
  bool trig_l, trig_r;                 /* gachettes L2 / R2 (appui) */
} Actions;
extern Actions A;
extern Input *g_in;
/* noms des boutons de la manette en cours (glyphes PlayStation ou lettres Xbox) */
enum { B_CONFIRM, B_BACK, B_ALT, B_X, B_START, B_SELECT };
const char *btn(int b);
void rumble(float strength,float seconds);
void toast(const char *msg);

/* ---------------- interface ---------------- */
void ui_begin(void);
void ui_end(void);
bool ui_button(int x,int y,int w,int h,const char *label,bool enabled);
bool ui_hot(void);              /* le dernier bouton est-il selectionne */
int  ui_focus_index(void);
void ui_set_focus(int i);
void ui_panel(int x,int y,int w,int h,uint32_t fill,uint32_t border);
void ui_fancy_panel(int x,int y,int w,int h);
void ui_bar(int x,int y,int w,int h,float t,uint32_t fill,uint32_t back);
void ui_cursor(void);
bool mouse_in(int x,int y,int w,int h);
void draw_title_logo(int cx,int y,float t);
/* ornements de bronze (menus.c) */
void orn_frame(int x,int y,int w,int h,int sel);
void orn_panel(int x,int y,int w,int h);
bool orn_button(int x,int y,int w,int h,const char *label,bool hot,bool enabled);
void orn_padlock(int cx,int cy);
void orn_emblem(int cx,int cy);
void title_text(int cx,int y,const char *T,int sc);
int  pad_badge(int x,int y,int kind);      /* pastille de bouton (L1, R1, L2, R2, croix...) ; renvoie la largeur */
int  text_badge(int x,int y,const char *t);
enum { BG_L1, BG_R1, BG_L2, BG_R2, BG_CONFIRM, BG_BACK, BG_ALT, BG_X };
void draw_check(int x,int y,uint32_t c);   /* coche verte 9x8 */
void duo_portrait(int x,int y,int w,int h,int ch,int ch2,int scale);   /* portraits superposes dans un cadre */
const char *hero_pair_name(int ch,int ch2,char *b,int n);

/* ---------------- etats du jeu ---------------- */
enum { GS_TITLE, GS_CHARSEL, GS_CAMP, GS_RUN, GS_RESULTS, GS_OPTIONS, GS_META, GS_CINE, GS_LEVELS, GS_UPGRADE, GS_TRAINING, GS_HELP };
void help_open(int ret);                 /* v9.1 : menu Aide */
void help_frame(float dt);
void menu_backdrop(float dt,int amb);
/* entrainement des heros (entrainement.c) */
int   train_level(int ch,int k);
int   train_cost(int lvl);                 /* prismes pour passer du niveau lvl au suivant */
float train_value(int k,int lvl);          /* bonus d'un attribut a ce niveau (PV, ou fraction) */
float train_bonus(int ch,int k);           /* bonus actuel du heros */
void  train_effect(int k,int lvl,char *b,int n);
int   train_total(int ch);                 /* somme des niveaux du heros */
bool  train_can(int ch,int k);             /* heros debloque, attribut pas au maximum, assez de prismes */
bool  train_any_affordable(void);
int   train_boss_prisms(void);             /* prismes donnes par le gardien de la partie en cours */
extern const char *const ATTR_N[6];        /* noms des six attributs (runui.c) */
void  attr_icon(int k,int x,int y,int sc); /* petites icones en pixels des attributs */
void title_heroes_draw(float t);           /* ecran titre : les heros en buste autour du titre */
void training_open(int ch,int ret);
void training_enter(void);
void training_frame(float dt);
bool training_busy(void);
const char *const *hero_art(int ch);       /* croquis ASCII du heros (12 x 15, art.c) */
uint32_t art_pal(char c);                  /* couleur d'une lettre des croquis (gfx.c) */
void levels_frame(float dt);           /* liste des niveaux (screens.c) */
void levels_enter(void);
void levels_build_banners(void);
/* choix fait dans la liste des niveaux, repris par l'ecran des heros */
typedef struct { int level, depth, mode; } Pick;
extern Pick PICK;
void start_run_request(int ch,int ch2,int depth,int mode,int level);
extern int g_state, g_prev_state;
extern float g_time;
extern bool g_quit;
extern int g_fs_request;
void goto_state(int s);
void options_open(int return_state);
void title_frame(float dt);
void charsel_frame(float dt);
void camp_frame(float dt);
void results_frame(float dt);
void options_frame(float dt);
void camp_init_scene(void);
void meta_frame(float dt);             /* ecrans Defis / Codex / Journal */
/* cinematiques (cine.c) : 0 introduction, 1 fin du Gouffre, 2 fin des Profondeurs */
enum { CINE_INTRO, CINE_FIN, CINE_FIN2, CINE_FIN3 };   /* FIN3 (v7) : le Royaume oublie */
void cine_request(int id,int ret);     /* fondu puis lecture, retour a l'etat « ret » */
void cine_start(int id,int ret);       /* lecture immediate */
void cine_enter(void);
void cine_frame(float dt);
bool cine_unlocked(int id);
extern bool g_skip_intro;              /* tests : pas d'introduction au demarrage */
void meta_open(int screen,int ret);
enum { META_ACH, META_CODEX, META_JOURNAL };
/* progression longue (meta.c) */
#define RANK_MAX 10
int   hero_rank(int ch);
int   hero_rank_xp(int ch,int *need);      /* xp dans le rang actuel, xp necessaire pour le suivant (0 au max) */
float hero_dmg_bonus(int ch);
int   hero_hp_bonus(int ch);
int   ball_stars(int k);
int   ball_star_need(int k,int star);      /* victimes necessaires pour l'etoile 1..3 */
bool  ball_masterable(int k);
int   total_stars(void);
int   bestiary_tier(int ek);               /* 0 inconnu, 1 vu, 2, 3 */
int   bestiary_need(int ek,int tier);
int   boss_tier(int b);
float completion_pct(void);
float completion_part(int cat,float *weight,const char **name);
#define COMP_CATS 8
bool  dest_unlocked(int d);
int   depth_max_of(int d);
int   *depth_sel_of(int d);
/* niveaux joues un par un (meta.c) */
int   lvl_maxwon(int L);                   /* plus grande profondeur gagnee sur ce niveau (0 : jamais termine) */
bool  lvl_unlocked(int L);
int   lvl_prereq(int L);                   /* v7 : niveau a terminer pour ouvrir L (-1 : aucun) */
int   lvl_depth_max(int L);                /* profondeur la plus forte que l'on peut choisir */
unsigned lvl_heroes(int L,int depth);      /* heros ayant termine (depth 0 : a n'importe quelle profondeur) */
unsigned lvl_heroes_diff(int L,int depth,int diff);   /* idem, dans une difficulte (Facile / Normale / Difficile) */
int   lvl_count_unlocked(void);
int   lvl_checks(void);                    /* coches (heros x niveaux, toutes profondeurs) */
void  lvl_derive(void);                    /* recalcule profondeurs debloquees, niveau atteint... */
int   lvl_mb_kind(int L,int i);            /* gardiens mineurs : espece et nom */
const char *lvl_mb_name(int L,int i);
const char *lvl_short(int L);              /* nom en capitales pour la liste */
const char *curse_name(int depth);         /* malediction ajoutee a cette profondeur (6-10) */
const char *curse_desc(int depth);
/* progression (meta.c) */
void meta_check(void);                 /* evalue les defis, accorde les recompenses */
int  ach_progress(int i,int *target);
bool ball_unlocked(int k);
bool pas_unlocked(int k);
bool hero_available(int ch);
int  daily_seed_today(void);
void daily_setup(int seed,int *hero,int *mods2);
int  daily_level_today(void);
void meta_note_kill(int kind);
void draw_meta_toast(float dt);
int  meta_new_count(void);

/* ---------------- partie (run) ---------------- */
#define PIT_X0 176
#define PIT_X1 464
#define PIT_Y0 0
#define PIT_Y1 360
#define PIT_W (PIT_X1-PIT_X0)
/* vue 2.5D du puits : une camera inclinee voit le haut du puits plus loin (plus petit) et le bas plus pres.
   Chaque ligne du jeu est agrandie autour de l'axe du puits (echelle 1-K en haut, 1+K en bas) ;
   les decors des cotes s'etirent jusqu'au bord de l'ecran. */
#define P25_K 0.13f
#define P25_FWA 0.55f   /* largeur apparente de la paroi : (A + B*y/H) fois sa largeur, plus large pres de la camera */
#define P25_FWB 1.50f
extern float g_p25;                                  /* force de la vue 2.5D pour l'image en cours (0 : de dessus) */
float p25_scale(float y);                            /* agrandissement a la profondeur y */
void  p25_fwd(float x,float y,float *X,float *Y);    /* jeu -> ecran */
void  p25_inv(float X,float Y,float *x,float *y);    /* ecran -> jeu */
void  p25_warp_cpu(void);                            /* rendu logiciel : applique la vue a g_screen */
/* decor des flancs (falaises) : deux bandes de PIT_X0 de large, raccordables sur SC_H lignes */
#define SC_W PIT_X0
#define SC_H 384
#define SC_FW 26      /* vue 2.5D : paroi verticale du puits (largeur dans la texture, 0 = pied, SC_FW-1 = rebord) */
void scenery_build(int biome,uint32_t seed);
/* textures HD precalculees (hdpack.c, dossier « donnees ») */
bool hd_available(void);
bool hd_floor_low(int biome,uint32_t *alb,uint8_t *em,uint32_t *nrm,int w,int h,void (*glow)(float,float,uint32_t,float));
bool hd_region_low(int key,int id,uint32_t *alb,uint8_t *em,uint32_t *nrm,int w,int h);
int  hd_glows(int key,int reg,float *x,float *y,float *rr,uint32_t *c,int max);
void hd_gpu_select(int key);            /* niveau 0..11 (+16 x partie), HD_CAMP + saison, -1 */
void hd_gpu_prefetch(int key);          /* v9 : prepare le jeu suivant en tache de fond */
void hd_set_variant(int v);             /* v9 : partie du niveau (0..2) ou arene du gardien (3) */
int  hd_get_variant(void);
int  hd_level_key(int biome);
int  hd_season(void);                   /* 0 hiver, 1 printemps, 2 ete, 3 automne */
void hd_gpu_prefetch_level(int biome,int var);
const char *hd_data_dir(void);          /* dossier « donnees » */
bool hd_gpu_active(int key);
int  hd_gpu_key(void);
int  hd_gpu_version(void);
int  hd_gpu_levels(void);
const uint8_t *hd_gpu_level(int which,int l,int *w,int *h);
const uint8_t *hd_gpu_low(int *w,int *h);
void hd_gpu_release_cpu(void);
void hd_frame(int scroll,int sh);
void hd_gpu_params(float M[8][4],float P[8][4],float G[4],float *scroll);
#define HD_CAMP 100
Sprite *scenery_tex(int side);
const uint32_t *scenery_nrm(int side);
const uint8_t *scenery_em(int side);
int scenery_glows(const float **x,const float **y,const float **r,const uint32_t **c);
extern const int SC_TORCH_Y[2][3];
Sprite *scenery_face(int side);                 /* paroi du puits (vue 2.5D) */
const uint32_t *scenery_face_nrm(int side);
const uint8_t *scenery_face_em(int side);
#define FLOOR_Y 352
#define ZONE_TOP 236
#define ZONE_BOT 344

#define MAX_EN 300
#define MAX_BALLS 220
#define MAX_BUL 420
#define MAX_PICK 500
#define MAX_PART 2600
#define MAX_TXT 140
#define MAX_ZONES 40
#define MAX_BOLTS 80
#define MAX_FLASH 48
#define MAX_SLOTS 5

typedef struct {
  bool on; int kind; int id;
  float x,y,w,h; float hp,hpmax; float t; float flash;
  float shoot; float vx; float push;
  int burn; float burn_t; int poison; float poison_t; float slow_t, freeze_t;
  int bleed; float bleed_t; float dot_acc, dot_show;
  float phase; float spawn; float ang; bool minion; bool guard; float hpbar_t;
  int last_ball;
  float blind_t; int champ; float special; int sstate; float orb; float shield;
  float scale; float kick;
  float summon_t; int summoned;   /* v4 : invocateurs */
  float calm_t;                   /* v4.1 : temps depuis le dernier degat (la regeneration attend) */
  bool flee;                      /* v4.1 : s'enfuit devant le gardien */
  int mboss;                      /* v6 : gardien mineur (1 ou 2), 0 sinon */
  float mb_atk; int mb_step; float mb_cx;
  float root_t;                   /* v7 : enracine (immobile, +25 % de degats subis) */
  float stun_t;                   /* v7 : etourdi (ni deplacement ni tir) */
  int acid; float acid_t;         /* v7 : corrosion (+5 % de degats subis par charge ; 3+ : armures percees) */
  float mark_t, mark_k;           /* v9 : marque de l'ombre (degats subis +mark_k) */
  int link_id; float link_t, link_k;   /* v10 : chaine (partage des degats avec link_id) */
  float champ_t;                  /* v10 : rythme des pouvoirs de champion */
} Enemy;
enum { CHAMP_NONE, CHAMP_RAPIDE, CHAMP_COLOSSE, CHAMP_REGEN, CHAMP_BOUCLIER,
       CHAMP_EXPLOSIF, CHAMP_INVOCATEUR, CHAMP_SOIGNEUR, CHAMP_SPECTRAL,   /* v10 */
       CHAMP_COUNT };
extern const char *const CHAMP_NAMES[CHAMP_COUNT];

enum { BS_READY, BS_FLY, BS_RETURN };
#define TRAIL_N 10
typedef struct {
  bool on; int kind; int lvl; int state; bool temp; int slot;
  float x,y,vx,vy,r,speed; float life;
  int hit_id[4]; float hit_t[4];
  float tx[TRAIL_N], ty[TRAIL_N]; float trail_t;
  float cd; int relaunch;
  int pierce, bounces; float beam_t; int hits;
  int wall; float cd2;            /* v9 : rebonds sur les murs depuis le lancer, second delai */
  float fly_t; int boom;          /* v10 : temps de vol, demi-tours du boomerang */
} Ball;

typedef struct { bool on; int kind; float x,y,vx,vy,r,dmg,life,t; uint32_t col; float tx,ty,tz,vz; } Bullet;
typedef struct { bool on; int kind; float x,y,vx,vy; int val; float t; bool magnet; } Pickup;
enum { PK_GEM, PK_GEM2, PK_GEM3, PK_COIN, PK_HEART };
typedef struct { float x,y,vx,vy,life,max,grav,drag; uint32_t c; uint8_t size,kind; } Part;
enum { PT_PIX, PT_SQ, PT_GLOW, PT_SMOKE, PT_SPARK, PT_RING, PT_EMBER, PT_MOTE, PT_SHARD, PT_STREAK };
typedef struct { bool on; float x,y,vy,life,age; char s[16]; uint32_t c; int scale; } FText;
typedef struct { bool on; int kind; float x,y,r,life,max,tick; float dmg; int aux; } Zone;
enum { ZN_STEAM, ZN_POISON, ZN_FIRE, ZN_WARN, ZN_LAVA, ZN_FROST, ZN_AMBER, ZN_ECHO, ZN_RONCES, ZN_ACIDE, ZN_STAR, ZN_DARK };
/* projectiles ennemis : 0 normal, 1 normal (braises, eclats), 2 lave lobee, 3 givre (ralentit), 4 ambre lobee (sol collant) */
#define BUL_LOB(u) ((u)->kind==2||(u)->kind==4||(u)->kind==5)   /* 5 : vase des marais (sol collant) */
typedef struct { bool on; float x0,y0,x1,y1,life; uint32_t c; } Bolt;
typedef struct { bool on; float x,y,r,life,max,inten; uint32_t c; } Flash;
typedef struct { bool on; float x,y,t,max,rmax,str; } Shock;
/* v8 : curiosites du decor (curios.c) : tresors a briser, pieges a declencher avec les billes */
#define MAX_CURIO 6
enum { CU_TRESOR, CU_PIEGE };
typedef struct { bool on; int kind; float x,y,r; int hits,hmax; float t,flash,wob,lab; } Curio;
const char *curio_name(int biome,int kind);
const Sprite *curio_sprite(int biome,int kind,int *ax,int *ay);
#define MAX_SHOCK 8
typedef struct { bool on; float x,y0,y1,life,max; uint32_t c; float w; } Beam;
#define MAX_BEAM 16

typedef struct {
  bool on; int type; float x,y,w,h; float hp,hpmax; float t; float flash;
  float enter; int phase; float atk1, atk2, atk3; float vx; float anim;
  float laser_x, laser_t; int laser_st; bool laser_hit;
  float dying; int guards; float guard_cd; float roar;
  float slam_x[6]; int slam_n; float slam_t;
  float dive_t; int dive; float tx; float spin; int slam_kind;   /* v4 : plongee, teleportation, colonnes */
  int acid; float acid_t;                                         /* v7 : corrosion */
  float mark_t, mark_k;                                           /* v9 : marque de l'ombre */
} Boss;

enum { RS_INTRO, RS_PLAY, RS_LEVELUP, RS_PAUSE, RS_CLEAR, RS_DEAD, RS_WIN, RS_EVOLIST, RS_EVENT };
enum { CARD_NEWBALL, CARD_UPBALL, CARD_NEWPAS, CARD_UPPAS, CARD_EVO, CARD_HEAL, CARD_GOLD, CARD_LEG };
/* exploits ponctuels (S.feats) */
enum { FEAT_NOHIT_LEVEL, FEAT_FAST_BOSS, FEAT_BOSS_NOHIT, FEAT_3EVOS, FEAT_FULL_BALLS, FEAT_KILLS_1000,
       FEAT_LOW_HP_BOSS, FEAT_RICH, FEAT_SPEED, FEAT_SOLO, FEAT_PACT,
       FEAT_4EVOS, FEAT_KILLS_2000, FEAT_GOLD_3000, FEAT_NOHIT_PROF, FEAT_SPEED_PROF, FEAT_NO_EVO_WIN, FEAT_FEW_BALLS,
       FEAT_FAST_VOID, FEAT_REVIVE_WIN, FEAT_DUO, /* v7 */ FEAT_NOHIT_ROY, FEAT_SPEED_ROY, /* v9 */ FEAT_RUSH_FAST, FEAT_COUNT };
void meta_feat(int f);
#define EV_COUNT 15   /* 12-14 : evenements des Profondeurs */
typedef struct { int type; int kind; int a,b; } Card;

typedef struct {
  int ch; int depth;
  int level; float ltime, lduration, len_k, dmg_k; float spawn_t; int rows; bool boss_spawned; float boss_warn;
  int sub; float sub_t; float total_time;
  /* joueur */
  float px,py, hp,hpmax; float iframes; float anim; int facing; float hurt_t; float walk;
  float fire_cd; int fire_idx; bool autoaim; float aimx, aimy; float aim_idle;
  int plevel; float xp, xpnext;
  int gold, kills, crystals, prisms; float gold_frac;
  int balls[MAX_SLOTS], ball_lvl[MAX_SLOTS]; int nballs;
  int pas[MAX_SLOTS], pas_lvl[MAX_SLOTS]; int npas;
  int rerolls, revives; int pending_levels; bool boss_reward;
  /* stats calculees */
  float s_dmg, s_fire, s_speed, s_bspeed, s_crit, s_critm, s_pick, s_xp, s_gold, s_regen, s_net, s_status, s_breach;
  int s_babies; float regen_acc;
  float s_mirror, s_rage; int s_statdur;
  float s_dodge, s_giant, s_anchor; int s_thorns; float thorn_cd;   /* v7 : cape, loupe, ancre, totem */
  /* choix */
  Card cards[4]; int ncards; float card_t; int card_sel;
  /* monde */
  Enemy en[MAX_EN]; Ball balls_e[MAX_BALLS]; Bullet bul[MAX_BUL]; Pickup pick[MAX_PICK];
  Part part[MAX_PART]; int part_next; FText txt[MAX_TXT]; Zone zones[MAX_ZONES]; Bolt bolts[MAX_BOLTS]; Flash flashes[MAX_FLASH];
  Boss boss; int next_id;
  float shake, scroll; float danger; float banner_t; char banner[64]; char banner2[64];
  float fade; float clear_t; float dead_t; int pause_sel; float levelup_flash;
  float combo_t; int combo; float hit_sfx_t; float bounce_sfx_t;
  bool won; int max_level_reached;
  int evolutions_done;
  float tutorial_t;
  int prev_sub;
  bool ended;
  float aim_ang, aim_hold;
  /* v2 */
  int mode, stage, cycle; int mods[MOD_COUNT]; int nmods;
  float hitstop, hitstop_cd;
  Shock shocks[MAX_SHOCK]; Beam beams[MAX_BEAM];
  float hp_trail, boss_trail, xp_show;
  float ev_dmg, ev_hp, ev_speed; int ev_babies; bool pact_next, pact;
  float timeslow_t, timeslow_cd; float shield_cd; bool shield_up; float shield_flash;
  float ghost_cd;
  int ev_id; int ev_sel; float ev_t; int ev_result; char ev_msg[200]; int ev_opts; bool ev_pending; uint32_t ev_seen;
  /* suivi des exploits */
  int lvl_hits, boss_hits; float boss_t; bool used_reroll; int st_elites, st_bosses;
  int new_achs;
  float walk_t; float low_hp_t;
  float champ_rate;
  /* v4 */
  int dest, biome;              /* destination (0 Gouffre, 1 Profondeurs) et biome du niveau (0-7) */
  float chill_t;                /* givre : le heros est ralenti */
  float amber_t;                /* ambre : le heros colle au sol */
  bool revived;
  int curses;                   /* nombre de maledictions actives (profondeur 6+ : 1..5) */
  int dest0, rank;              /* destination de depart, rang du heros */
  int picked_balls;             /* nouvelles billes choisies pendant la descente */
  int diff_low;                 /* difficulte la plus facile utilisee pendant la descente (recompenses) */
  /* v6 */
  int ch2, rank2;               /* compagnon (-1 : aucun) et son rang */
  int lvl;                      /* niveau joue (0-7) */
  bool single;                  /* un seul niveau, long, avec deux gardiens mineurs */
  int seg;                      /* partie du niveau (0, 1, 2) : gardiens mineurs vaincus */
  bool mb_on, mb_warned, mb_reward; float mb_warn, mb_t; int mb_id; float mb_hpmax;   /* gardien mineur en cours */
  int mb_kills;                 /* gardiens mineurs vaincus pendant la descente */
  float boss_time;              /* temps passe face aux gardiens (mineurs compris) */
  float comp_x, comp_y; int comp_face; float comp_walk;   /* compagnon (dessin) */
  int lvl_hits_seg;             /* degats subis depuis le dernier gardien */
  int lu_from;                  /* etat a retrouver apres un choix d'amelioration */
  /* v8 */
  Curio cur[MAX_CURIO]; float curio_t; int curio_last; int curio_n;   /* curiosites du decor */
  int legendaries;              /* v9 : fusions legendaires de la descente */
  float astro_t;                /* v9 : etoile de l'Astronome */
  int curios_run, breaches, champ_kills, combo_max;   /* v9 : suivi des objectifs de niveau */
  float s_slow, s_home, s_grim;       /* v10 : pendule, boussole, grimoire */
  int hunt; float hunt_t;             /* v10 : Chasseresse (billes rattrapees) */
  float geo_t;                        /* v10 : secousse du Geomancien */
  uint32_t muts;                      /* v10 : mutateurs de la descente */
} Run;
static inline bool has_ch(int c){ extern Run R; return R.ch==c || R.ch2==c; }
static inline int  ch_rank(int c){ extern Run R; return R.ch==c ? R.rank : (R.ch2==c ? R.rank2 : 0); }
static inline bool curse_on(int depth_needed){ extern Run R; return R.depth>=depth_needed && R.mode!=MODE_DAILY; }
static inline float mag_radius(void){ return ch_rank(CH_MAGNETISEUR)>=5?48.0f:36.0f; }
extern Run R;
#define REROLL_Y 284
void bg_build(int biome);
void card_rect(int i,int n,int *x,int *y,int *w,int *h);
void run_card_text(Card *c,char *title,int tn,char *sub,int sn,char *desc,int dn,char *hint,int hn);
void run_pause(void);
void run_resume(void);
void levelup_input(float dt);          /* runui.c : choix d'amelioration et pause */
void levelup_draw(void);
void pause_ui(void);
void pause_reset(void);

void run_start(int ch,int ch2,int depth,int mode,int level);
int  run_event_options(char lab[4][96],bool en[4]);
void run_event_choose(int i);
void run_event_text(const char **title,const char **desc);
bool run_has_mod(int m);
float run_diff(void);
float run_hp_scale(void);
void run_skip_card(void);              /* « Passer » au choix d'amelioration */
void run_reroll_cards(void);
void run_choose_card(int i);
void run_quick_restart(void);
void run_abandon(void);
void run_card_detail(Card *c,char *head,int hn,char *name,int nn,char *dmg,int dn,char *desc,int ddn,int *recipes,int *nrec);
void run_frame(float dt);
void run_draw(void);
void run_draw_hud(void);
void run_recalc_stats(void);
int  run_ball_level(int kind);
int  run_pas_level(int kind);
void run_end_to_results(bool won);
void spawn_particles(float x,float y,int n,uint32_t c,float spd,int kind,float life);
void add_ftext(float x,float y,const char *s,uint32_t c,int scale);

/* resultats de la derniere partie */
typedef struct { bool won; int level; int kills; int gold; int crystals; float time; int plevel; int ch; int balls[MAX_SLOTS], ball_lvl[MAX_SLOTS], nballs; bool new_depth;
  int mode, stage, score; bool record; int achs[16]; int nachs; int depth;
  int dest, biome; int hxp, rank0, rank1; int stars_k[6], stars_n[6], nstars; int diff;
  int ch2, hxp2, rank0b, rank1b;       /* v6 : compagnon */
  int lvl; unsigned new_checks; bool next_unlocked, new_dest; int mb_kills; int check_gold; bool record_time;
  int prisms; int obj_met, obj_new; } RunResult;
/* v9 : objectifs de niveau (data.c) */
enum { OB_CURIO, OB_BOSS, OB_BREACH, OB_CHAMP, OB_COMBO, OB_LEVEL };
int  obj_kind(int L,int k);
void obj_text(int L,int k,char *b,int n);
int  obj_value(int L,int k,int *target);     /* progression dans la descente en cours */
bool obj_met(int L,int k);
int  obj_total(void);
extern RunResult LAST;

#endif
