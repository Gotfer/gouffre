/* GOUFFRE - decor des deux flancs du puits, facon « falaises peintes » :
   roche a facettes eclairees (touches de pinceau en diagonale), replat couvert (neige, mousse,
   cendre...), bordure du chemin, accessoires par biome. Texture raccordable verticalement,
   avec normales (eclairage GPU) et zones emissives (cristaux, lave, bougies...). */
#include "game.h"

#define SW SC_W
#define SH SC_H

static Sprite   sc_tex[2];
static Sprite   sc_face[2];                 /* paroi du puits, colonne u : 0 = pied, SC_FW-1 = rebord */
static uint32_t sc_fnrm[2][SC_FW*SH];
static uint8_t  sc_fem[2][SC_FW*SH];
static uint32_t sc_nrm[2][SW*SH];
static uint8_t  sc_em[2][SW*SH];
static float    sc_hg[SW*SH];
static uint8_t  sc_sp[SW*SH];
static int cur=0, sc_biome=-1;
#define SC_MAXG 48
static float sg_x[SC_MAXG], sg_y[SC_MAXG], sg_r[SC_MAXG]; static uint32_t sg_c[SC_MAXG]; static int sg_n;
const int SC_TORCH_Y[2][3]={{40,168,296},{104,232,360}};
/* hasard propre au decor : la generation ne consomme qu'un seul tirage du hasard de la partie */
static uint32_t srng=1u;
static inline uint32_t snext(void){ srng^=srng<<13; srng^=srng>>17; srng^=srng<<5; return srng; }
static inline float sfrand(void){ return (snext()>>8)*(1.0f/16777216.0f); }
static inline float sfrange(float a,float b){ return a+(b-a)*sfrand(); }
static inline int sirand(int n){ return n>0 ? (int)(snext()%(uint32_t)n) : 0; }
static inline int sirange(int a,int b){ return a+sirand(b-a+1); }
static inline bool schance(float p){ return sfrand()<p; }
#define frand sfrand
#define frange sfrange
#define irand sirand
#define irange sirange
#define chance schance

/* ---------- acces en coordonnees locales : u = distance au bord du chemin, y raccordable ---------- */
static inline int wy(int y){ return ((y%SH)+SH)%SH; }
static inline int tx(int u){ return cur==0 ? SW-1-u : u; }
static inline void P(int u,int y,uint32_t c){ if(u<0||u>=SW) return; sc_tex[cur].px[wy(y)*SW+tx(u)]=c|0xFF000000u; }
static inline uint32_t G(int u,int y){ u=clampi(u,0,SW-1); return sc_tex[cur].px[wy(y)*SW+tx(u)]; }
static inline void PB(int u,int y,uint32_t c,float a){ if(u<0||u>=SW) return; uint32_t *p=&sc_tex[cur].px[wy(y)*SW+tx(u)]; *p=col_lerp(*p,c,a)|0xFF000000u; }
static inline void E(int u,int y,int v){ if(u<0||u>=SW) return; sc_em[cur][wy(y)*SW+tx(u)]=(uint8_t)clampi(v,1,255); }
static float hg_off=0;   /* relief de base sous un accessoire pose sur la falaise */
static inline void HG(int u,int y,float h){ if(u<0||u>=SW) return; sc_hg[wy(y)*SW+tx(u)]=h+hg_off; }
static inline float GH(int u,int y){ u=clampi(u,0,SW-1); return sc_hg[wy(y)*SW+tx(u)]; }
static inline void SPc(int u,int y,int v){ if(u<0||u>=SW) return; sc_sp[wy(y)*SW+tx(u)]=(uint8_t)clampi(v,0,255); }
static void glow(float u,float y,uint32_t c,float r){
  if(sg_n>=SC_MAXG) return;
  sg_x[sg_n]=(float)(tx(clampi((int)u,0,SW-1))+(cur?PIT_X1:0)); sg_y[sg_n]=(float)wy((int)y); sg_c[sg_n]=c; sg_r[sg_n]=r; sg_n++;   /* x : pixels de l'ecran */
}
static const uint8_t BAY[4][4]={{0,8,2,10},{12,4,14,6},{3,11,1,9},{15,7,13,5}};
static inline float bay(int u,int y){ return (BAY[wy(y)&3][u&3]+0.5f)/16.0f; }
/* bruit 2D raccordable en y */
static float pn(float u,float y,float sc,uint32_t seed){ float t=(float)wy((int)y)/SH; float yy=(float)wy((int)y)+(y-floorf(y)); return lerpf(fbm(u*sc,yy*sc,seed),fbm(u*sc,(yy-SH)*sc,seed),t); }
/* courbe periodique en y (somme de sinus de frequences entieres) */
typedef struct { float a[5], p[5]; } Wave;
static Wave wave_new(float amp){ static const int F[5]={1,2,3,5,8}; Wave w; for(int k=0;k<5;k++){ w.a[k]=amp*frange(0.4f,1.0f)/(1+0.55f*F[k]); w.p[k]=frand()*TAU; } return w; }
static float wave(const Wave *w,float y){ static const int F[5]={1,2,3,5,8}; float s=0; for(int k=0;k<5;k++) s+=w->a[k]*sinf(TAU*F[k]*y/SH+w->p[k]); return s; }
static uint32_t rampd(const uint32_t *r,int n,float v,int u,int y,float dith){
  float f=clampf(v,0,0.999f)*n + (bay(u,y)-0.5f)*dith;
  return r[clampi((int)floorf(f),0,n-1)];
}
static uint32_t ramp(const uint32_t *r,int n,float v,int u,int y){ return rampd(r,n,v,u,y,0.9f); }

/* ---------- palettes par biome ---------- */
typedef struct {
  uint32_t rock[6];     /* falaise : ombre profonde -> lumiere */
  uint32_t cov[5];      /* couverture (neige, mousse, cendre...) : ombre -> lumiere */
  uint32_t line;        /* fissures, contours */
  uint32_t rim[4];      /* bordure du chemin : ombre, sombre, moyen, clair */
  float top;            /* couverture des sommets de la falaise */
  float shelf;          /* largeur moyenne du replat */
  float spec;           /* brillance de la roche */
} ScPal;
static const ScPal PAL[BIOME_COUNT]={
  /* catacombes : pierre grise et violette, poussiere d'os */
  {{HEX(0x1c1822),HEX(0x2a2532),HEX(0x3a3443),HEX(0x4c4555),HEX(0x625a6a),HEX(0x7c7484)},
   {HEX(0x5a5262),HEX(0x766c76),HEX(0x968a88),HEX(0xb4a898),HEX(0xd2c6b0)},HEX(0x110e16),
   {HEX(0x15121a),HEX(0x3a3442),HEX(0x5a5262),HEX(0x8a8292)},0.25f,40,0.18f},
  /* foret fongique : roche moussue */
  {{HEX(0x121a16),HEX(0x1c2820),HEX(0x26362a),HEX(0x334836),HEX(0x445c44),HEX(0x587456)},
   {HEX(0x24402a),HEX(0x325a32),HEX(0x44723c),HEX(0x5a8a48),HEX(0x76a25a)},HEX(0x0a120c),
   {HEX(0x0c120e),HEX(0x2a3a2e),HEX(0x40523f),HEX(0x627a5a)},0.55f,44,0.10f},
  /* forges : basalte, cendre et braises */
  {{HEX(0x140c0d),HEX(0x201315),HEX(0x2e1c1e),HEX(0x3e2828),HEX(0x523432),HEX(0x6a443c)},
   {HEX(0x2a1e1e),HEX(0x40302c),HEX(0x5a463c),HEX(0x76604e),HEX(0x947a62)},HEX(0x0a0607),
   {HEX(0x0e0809),HEX(0x342426),HEX(0x4d322a),HEX(0x7a5048)},0.18f,34,0.30f},
  /* coeur du gouffre : roche violette, chair et cristal */
  {{HEX(0x140b1e),HEX(0x1e112c),HEX(0x2a183c),HEX(0x38224e),HEX(0x4a3064),HEX(0x60427e)},
   {HEX(0x3a2050),HEX(0x55306e),HEX(0x744a90),HEX(0x9a6ab8),HEX(0xc49ae0)},HEX(0x0a0612),
   {HEX(0x0c0614),HEX(0x2e1c40),HEX(0x45305c),HEX(0x6a4e8a)},0.30f,38,0.30f},
  /* grottes de givre : roche violet-bleu, neige lavande (cf. reference) */
  {{HEX(0x221c32),HEX(0x302a48),HEX(0x403c60),HEX(0x524f78),HEX(0x686890),HEX(0x8484ae)},
   {HEX(0x8a86c4),HEX(0xa49ed6),HEX(0xbcb6e8),HEX(0xd4cef4),HEX(0xecebfe)},HEX(0x18142a),
   {HEX(0x1a1830),HEX(0x4e4c78),HEX(0x7a7aa8),HEX(0xb4b8e0)},0.45f,48,0.34f},
  /* cite engloutie : pierre verte, algues et sable */
  {{HEX(0x0c1c1e),HEX(0x13292a),HEX(0x1c3838),HEX(0x264a48),HEX(0x335e5a),HEX(0x447670)},
   {HEX(0x2a4e44),HEX(0x3a6a58),HEX(0x4e8468),HEX(0x7aa080),HEX(0xa8bc98)},HEX(0x061214),
   {HEX(0x081416),HEX(0x24443f),HEX(0x3a6058),HEX(0x6a9a8a)},0.35f,42,0.24f},
  /* ruche d'ambre : cire et miel */
  {{HEX(0x24140a),HEX(0x341e0c),HEX(0x4a2c10),HEX(0x643e16),HEX(0x82541e),HEX(0xa46e2a)},
   {HEX(0x8a5014),HEX(0xb06a18),HEX(0xd08a22),HEX(0xeaaa38),HEX(0xffd070)},HEX(0x140a04),
   {HEX(0x160c04),HEX(0x4a3014),HEX(0x6e4a1c),HEX(0xa07a3a)},0.40f,40,0.36f},
  /* neant : obsidienne, poussiere d'etoiles */
  {{HEX(0x0a0910),HEX(0x121019),HEX(0x1a1724),HEX(0x241f32),HEX(0x302a42),HEX(0x403856)},
   {HEX(0x2a2840),HEX(0x403e60),HEX(0x5c5a86),HEX(0x8a8ab8),HEX(0xc8ccf0)},HEX(0x050408),
   {HEX(0x06050a),HEX(0x1e1a2c),HEX(0x302a44),HEX(0x56507a)},0.20f,30,0.40f},
  /* v7 : jardin des lucioles : roche verte et herbe haute */
  {{HEX(0x10201c),HEX(0x182c26),HEX(0x223a32),HEX(0x2e4a40),HEX(0x3c5c50),HEX(0x4e7262)},
   {HEX(0x24502e),HEX(0x2e6a36),HEX(0x3e8240),HEX(0x5a9a4a),HEX(0x7ab45a)},HEX(0x08120e),
   {HEX(0x0a140f),HEX(0x24382e),HEX(0x38503f),HEX(0x5a7a62)},0.60f,44,0.12f},
  /* marais maudit : tourbe et mousse detrempee */
  {{HEX(0x14120c),HEX(0x1e1c12),HEX(0x2a2618),HEX(0x36321e),HEX(0x443e26),HEX(0x564e30)},
   {HEX(0x1e2410),HEX(0x283018),HEX(0x343c1e),HEX(0x424a24),HEX(0x52582c)},HEX(0x0a0906),
   {HEX(0x0c0b07),HEX(0x2a2718),HEX(0x3e3a24),HEX(0x5e5838)},0.45f,40,0.22f},
  /* mines de cristal : ardoise et poussiere */
  {{HEX(0x121018),HEX(0x1a1822),HEX(0x24222e),HEX(0x302c3c),HEX(0x3e3a4c),HEX(0x524c62)},
   {HEX(0x3a3446),HEX(0x4a4258),HEX(0x5c546c),HEX(0x726a84),HEX(0x8c84a0)},HEX(0x0a0810),
   {HEX(0x0a0810),HEX(0x2a2636),HEX(0x3e3a4c),HEX(0x625c74)},0.20f,36,0.32f},
  /* sanctuaire solaire : gres dore et sable */
  {{HEX(0x2a1a0c),HEX(0x3e2a14),HEX(0x56401e),HEX(0x6e5428),HEX(0x8a6c36),HEX(0xa88a4a)},
   {HEX(0x8a6a3a),HEX(0xa8844a),HEX(0xc4a05e),HEX(0xdcbc78),HEX(0xf0d898)},HEX(0x140a04),
   {HEX(0x160c06),HEX(0x4a3618),HEX(0x6e5428),HEX(0xa88a4a)},0.50f,42,0.20f},
};

/* ---------- bruits de la roche ---------- */
/* bruit etire le long des touches de pinceau (direction du,dy), raccordable en y */
static float an(float u,float y,float du,float dy,float sa,float st,uint32_t seed){
  float yy=(float)wy((int)y)+(y-floorf(y)); float t=yy/SH;
  float a0=u*du+yy*dy, c0=-u*dy+yy*du; float a1=u*du+(yy-SH)*dy, c1=-u*dy+(yy-SH)*du;
  return lerpf(fbm(a0*sa,c0*st,seed),fbm(a1*sa,c1*st,seed),t);
}

/* ---------- accessoires ---------- */
static void boulder(int u0,int y0,int rx,int ry,const ScPal *p,bool capped,float hbase){
  for(int dy=-ry-1;dy<=ry+1;dy++) for(int du=-rx-1;du<=rx+1;du++){
    float nx=du/(float)rx, ny=dy/(float)ry; float d=nx*nx+ny*ny; if(d>1.0f) continue;
    float nz=sqrtf(fmaxf(0,1-d));
    float l=clampf(-0.55f*ny+0.75f*nz,0,1);   /* lumiere venant du haut */
    float fac=floorf((nx*1.7f+ny*1.1f)*2.0f+3)/6.0f;                    /* facettes */
    float v=0.25f+0.6f*l+0.12f*(fac-0.5f);
    uint32_t c=ramp(p->rock,6,v,u0+du,y0+dy);
    if(d>0.82f && ny>0.2f) c=p->line;
    if(capped && ny<-0.15f+0.2f*sinf(du*1.3f) && nz>0.35f) c=ramp(p->cov,5,0.55f+0.45f*l,u0+du,y0+dy);
    P(u0+du,y0+dy,c); HG(u0+du,y0+dy,hbase+nz*0.35f);
  }
  /* ombre portee vers le bas */
  for(int du=-rx;du<=rx;du++) for(int k=1;k<=2;k++){ int y=y0+ry+k; PB(u0+du,y,p->line,0.35f/k); }
}
static void tuft(int u0,int y0,uint32_t c1,uint32_t c2,int n){
  for(int k=0;k<n;k++){ float a=-PI*0.5f+frange(-1.0f,1.0f); int len=irange(3,7); float u=(float)u0+frange(-2,2), y=(float)y0;
    for(int i=0;i<len;i++){ u+=cosf(a); y+=sinf(a)*0.8f; P((int)u,(int)y,i<len-2?c1:c2); } }
}
static void bush(int u0,int y0,const uint32_t *cols,int r){   /* buisson de givre / fougere : branches */
  for(int k=0;k<14;k++){ float a=frange(-PI,0)+(chance(0.25f)?PI*frange(0.1f,0.3f):0); float u=(float)u0, y=(float)y0; int len=irange(r/2,r);
    for(int i=0;i<len;i++){ u+=cosf(a)*0.9f; y+=sinf(a)*0.9f; a+=frange(-0.25f,0.25f);
      P((int)u,(int)y,cols[i<len/3?0:(i<len*2/3?1:2)]); if((i%3)==2){ P((int)u+1,(int)y,cols[2]); } }
    P((int)u,(int)y,cols[3]); }
}
static void crystal(int u0,int y0,uint32_t lit,uint32_t dark,int hmax,int emv){
  int n=irange(2,4);
  for(int k=0;k<n;k++){ int u=u0+irange(-4,4), h=irange(hmax/2,hmax); int w=maxi(1,h/4);
    for(int j=0;j<h;j++){ int ww=maxi(0,(int)(w*(1.0f-(float)j/h)+0.5f));
      for(int q=-ww;q<=ww;q++){ P(u+q,y0-j,q<0?lit:dark); E(u+q,y0-j,emv+irand(60)); SPc(u+q,y0-j,230); HG(u+q,y0-j,0.9f); } } }
}
static void candle_grp(int u0,int y0){
  int n=irange(2,4);
  for(int k=0;k<n;k++){ int u=u0+k*3-n, h=irange(3,7);
    for(int j=0;j<h;j++){ P(u,y0-j,HEX(0xe8dcc4)); P(u+1,y0-j,HEX(0xc0b49c)); }
    P(u,y0-h,HEX(0x2a1c14)); P(u,y0-h-1,HEX(0xffd36a)); P(u,y0-h-2,HEX(0xffb040)); E(u,y0-h-1,1+irand(200)); E(u,y0-h-2,1+irand(200)); }
  glow((float)u0,(float)y0-5,HEX(0xffb060),30);
}
static void skull(int u,int y,uint32_t c,uint32_t d){
  for(int k=1;k<4;k++) P(u+k,y,c); for(int k=0;k<5;k++){ P(u+k,y+1,c); P(u+k,y+2,c); }
  P(u+1,y+2,d); P(u+3,y+2,d); P(u+1,y+3,c); P(u+2,y+3,c); P(u+3,y+3,c); P(u+2,y+4,col_scale(c,0.7f));
}
static void bones(int u0,int y0){
  for(int k=0;k<4;k++){ int u=u0+irange(-6,6), y=y0+irange(-3,3); int l=irange(3,6); bool hz=chance(0.5f);
    for(int i=0;i<l;i++) P(hz?u+i:u,hz?y:y+i,HEX(0xc8bca4)); P(u,y,HEX(0xe8dcc4)); P(hz?u+l:u,hz?y:y+l,HEX(0xe8dcc4)); }
  if(chance(0.5f)) skull(u0-2,y0-3,HEX(0xd8ccb4),HEX(0x1a1420));
}
static void mushroom_big(int u0,int y0,bool teal){
  uint32_t cap=teal?HEX(0x45c2c8):HEX(0xf59ac0), capd=teal?HEX(0x1f6e78):HEX(0xa8507f), caph=teal?HEX(0xa8fff4):HEX(0xffd0e8);
  int h=irange(5,10), r=irange(4,7);
  for(int j=0;j<h;j++){ P(u0,y0-j,HEX(0xd8c8b0)); P(u0+1,y0-j,HEX(0xb8a890)); P(u0-1,y0-j,HEX(0xc8b8a0)); }
  for(int dy=-r;dy<=1;dy++) for(int du=-r-2;du<=r+2;du++){ if(du*du*0.55f+dy*dy*1.5f>r*r+1) continue; int u=u0+du, y=y0-h+dy;
    uint32_t c=dy>=0?capd:(dy<-r/2&&du<0?caph:cap); P(u,y,c); E(u,y,(teal?128:1)+irand(60)); HG(u,y,0.8f); }
  glow((float)u0,(float)y0-h,teal?HEX(0x45c2c8):HEX(0xf59ac0),34);
}
/* troncon de colonne : cl = {clair, milieu, sombre, chapiteau, chapiteau ombre, ombre portee} */
static void column_stump_c(int u0,int y0,const uint32_t *cl){
  int w=irange(9,13), h=irange(6,12);
  for(int j=0;j<h;j++) for(int q=0;q<w;q++){ uint32_t c=q<2?cl[0]:(q>w-3?cl[2]:cl[1]); if((q%3)==1 && j>1) c=col_scale(c,0.85f); P(u0+q,y0-j,c); HG(u0+q,y0-j,0.7f); }
  for(int q=-1;q<=w;q++){ P(u0+q,y0-h,cl[3]); P(u0+q,y0-h-1,cl[4]); }
  for(int q=0;q<w;q++) PB(u0+q,y0+1,cl[5],0.5f);
}
static void column_stump(int u0,int y0){
  static const uint32_t CL[6]={HEX(0x9ab0a8),HEX(0x6a8a80),HEX(0x3a5650),HEX(0xc8d8d0),HEX(0x8aa8a0),HEX(0x061214)};
  column_stump_c(u0,y0,CL);
}
static void coral(int u0,int y0,bool pink){
  uint32_t c=pink?HEX(0xf59ac0):HEX(0x5af0d0);
  for(int k=0;k<5;k++){ float a=-PI*0.5f+(k-2)*0.38f; for(int j=0;j<irange(4,8);j++){ int u=u0+(int)(cosf(a)*j), y=y0+(int)(sinf(a)*j); P(u,y,c); if(j>=3) E(u,y,pink?30+irand(60):140+irand(60)); } }
  glow((float)u0,(float)y0-4,c,24);
}
static void honey_drip(int u0,int y0,int len){
  for(int j=0;j<len;j++){ int w=j<len-2?1:2; for(int q=0;q<w;q++){ P(u0+q,y0+j,HEX(0xf0b040)); E(u0+q,y0+j,90+irand(80)); } }
  P(u0,y0+len,HEX(0xffe090)); E(u0,y0+len,200);
}
static void lava_vein(float u,float y,int len){
  float a=frange(PI*0.3f,PI*0.7f);
  for(int i=0;i<len;i++){ a+=frange(-0.4f,0.4f); a=clampf(a,PI*0.15f,PI*0.85f); u+=cosf(a)*0.6f; y+=sinf(a);
    P((int)u,(int)y,HEX(0xff7a22)); E((int)u,(int)y,1+((int)(y*1.3f)&127)); P((int)u+1,(int)y,HEX(0x5a1a10)); if(i%16==8) glow(u,y,HEX(0xff7a22),26); }
}
static void eye(int u,int y){
  for(int dy=-2;dy<=2;dy++) for(int du=-3;du<=3;du++){ if(du*du*0.45f+dy*dy>4.5f) continue; P(u+du,y+dy,HEX(0xf0e8f0)); }
  for(int dy=-1;dy<=1;dy++) for(int du=-1;du<=1;du++){ P(u+du,y+dy,HEX(0xc02848)); E(u+du,y+dy,140+irand(40)); }
  P(u,y,HEX(0x0a0408)); P(u-4,y,HEX(0x2a1030)); P(u+4,y,HEX(0x2a1030));
  glow((float)u,(float)y,HEX(0xff5a8a),18);
}
static void star(int u,int y){ P(u,y,chance(0.3f)?HEX(0x8a7aff):HEX(0xe0e8ff)); E(u,y,1+irand(120)); }

/* ---------- roche a facettes : maillage de triangles (sommets jitteres, raccordable en y) ---------- */
#define FG 24
#define FNU (SW/FG+3)
#define FNY (SH/FG)
static float fv_u[FNU*FNY], fv_y[FNU*FNY], fv_h[FNU*FNY];
static int   fc_id[SW*SH];
static float fc_sh[SW*SH], fc_h[SW*SH], fc_nz[SW*SH];
static void facets_build(uint32_t seed){
  for(int j=0;j<FNY;j++) for(int i=0;i<FNU;i++){
    float u0=(float)(i*FG)+(i>0?frange(-8.0f,8.0f):0), y0=(float)(j*FG)+frange(-8.0f,8.0f);
    fv_u[j*FNU+i]=u0; fv_y[j*FNU+i]=y0;
    fv_h[j*FNU+i]=30.0f + 0.30f*u0 + 34.0f*(pn(u0,y0,0.016f,seed+31)-0.5f) + 12.0f*(pn(u0,y0,0.05f,seed+32)-0.5f) + frange(-7.0f,7.0f);
  }
  const float LX=-0.30f, LY=-0.55f, LZ=0.78f;
  for(int y=0;y<SH;y++) for(int u=0;u<SW;u++){
    int ci=u/FG, cj=y/FG; bool found=false;
    for(int dj=-1;dj<=1 && !found;dj++) for(int di=-1;di<=1 && !found;di++){
      int i=ci+di, j=cj+dj; if(i<0||i>=FNU-1) continue;
      float yoff=0; int jj=j; if(jj<0){ jj+=FNY; yoff=-(float)SH; } else if(jj>=FNY){ jj-=FNY; yoff=(float)SH; }
      int j2=jj+1; float yoff2=yoff; if(j2>=FNY){ j2-=FNY; yoff2+= (float)SH; }
      int a=jj*FNU+i, b=jj*FNU+i+1, c=j2*FNU+i, d=j2*FNU+i+1;
      float pu[4]={fv_u[a],fv_u[b],fv_u[c],fv_u[d]}, py[4]={fv_y[a]+yoff,fv_y[b]+yoff,fv_y[c]+yoff2,fv_y[d]+yoff2}, ph[4]={fv_h[a],fv_h[b],fv_h[c],fv_h[d]};
      uint32_t hh=(uint32_t)(i*7919+jj*104729)^seed; hh^=hh>>13; hh*=0x5bd1e995u; hh^=hh>>15; bool diag=(hh&1)!=0;
      static const int T0[2][2][3]={{{0,1,3},{0,3,2}},{{0,1,2},{1,3,2}}};
      for(int t=0;t<2 && !found;t++){
        const int *k=T0[diag?1:0][t];
        float x0=pu[k[0]], y0=py[k[0]], x1=pu[k[1]], y1=py[k[1]], x2=pu[k[2]], y2=py[k[2]];
        float px=u+0.5f, pyy=y+0.5f;
        float den=(y1-y2)*(x0-x2)+(x2-x1)*(y0-y2); if(fabsf(den)<1e-4f) continue;
        float l0=((y1-y2)*(px-x2)+(x2-x1)*(pyy-y2))/den, l1=((y2-y0)*(px-x2)+(x0-x2)*(pyy-y2))/den, l2=1-l0-l1;
        if(l0<-0.001f||l1<-0.001f||l2<-0.001f) continue;
        bool merged=((hh>>5)%100)<38;     /* ~4 cases sur 10 : une seule face plane (quadrilatere) */
        const int *kp = merged ? T0[diag?1:0][0] : k;
        float qx0=pu[kp[0]], qy0=py[kp[0]], qx1=pu[kp[1]], qy1=py[kp[1]], qx2=pu[kp[2]], qy2=py[kp[2]];
        float h0=ph[kp[0]], h1=ph[kp[1]], h2=ph[kp[2]];
        /* plan z = A u + B y + C (u : distance au chemin) ; normale dans l'espace de l'ecran */
        float dd=(qx1-qx0)*(qy2-qy0)-(qx2-qx0)*(qy1-qy0); if(fabsf(dd)<1e-4f) dd=1e-4f;
        float A=((h1-h0)*(qy2-qy0)-(h2-h0)*(qy1-qy0))/dd;
        float B=((qx1-qx0)*(h2-h0)-(qx2-qx0)*(h1-h0))/dd;
        float As = cur==0 ? -A : A;
        float nx=-As, ny=-B, nz=1.0f; float nl=sqrtf(nx*nx+ny*ny+1.0f); nx/=nl; ny/=nl; nz/=nl;
        int o=y*SW+u;
        fc_id[o]=((jj*FNU+i)<<1)|(merged?0:t);
        fc_sh[o]=clampf(nx*LX+ny*LY+nz*LZ,0,1); fc_nz[o]=nz;
        fc_h[o]=h0+A*(px-qx0)+B*(pyy-qy0);
        found=true;
      }
    }
    if(!found){ int o=y*SW+u; int q= u>0 ? o-1 : (y>0 ? o-SW : o); if(q==o){ fc_id[o]=-1; fc_sh[o]=0.5f; fc_nz[o]=1; fc_h[o]=0; } else { fc_id[o]=fc_id[q]; fc_sh[o]=fc_sh[q]; fc_nz[o]=fc_nz[q]; fc_h[o]=fc_h[q]; } }
  }
}
static inline int FI(int u,int y){ u=clampi(u,0,SW-1); return fc_id[wy(y)*SW+u]; }
static inline float FS(int u,int y){ u=clampi(u,0,SW-1); return fc_sh[wy(y)*SW+u]; }

/* ---------- generation d'un flanc ---------- */
static void build_side(int biome,int side,uint32_t seed){
  cur=side;
  const ScPal *p=&PAL[biome];
  if(!sc_tex[side].px) sc_tex[side]=spr_alloc(SW,SH);
  memset(sc_em[side],0,sizeof sc_em[side]);
  Wave wcs=wave_new(16), wcs2=wave_new(6);
  facets_build(seed+55);
  float shelf=p->shelf;
  /* direction des touches de pinceau (steep, vers l'exterieur en descendant) */
  float du=0.36f, dy=0.93f;
  bool voidb = biome==7;
  for(int y=0;y<SH;y++){
    float cs = shelf + wave(&wcs,(float)y) + wave(&wcs2,(float)y*1.0f);
    for(int u=0;u<SW;u++){
      float n1=pn((float)u,(float)y,0.045f,seed+1), n2=pn((float)u,(float)y,0.13f,seed+2);
      float edge_u = cs + (n1-0.5f)*22.0f;
      float m = clampf((u-edge_u)/10.0f,0,1);            /* 0 : replat, 1 : falaise */
      if(voidb){ float isl=pn((float)u,(float)y,0.03f,seed+9)+ (u<edge_u+20?0.25f:0) - (u>110?0.15f:0); m = isl>0.62f && u>edge_u-6 ? 1.0f : (u<edge_u?0:-1.0f); if(m<0 && u<edge_u+4) m=0; }
      uint32_t c; float h;
      if(m<0){ /* neant : le vide et ses etoiles */
        float g=pn((float)u,(float)y,0.02f,seed+12);
        c=col_lerp(HEX(0x07060c),HEX(0x141026),clampf(g*1.4f-0.3f,0,1)); h=-0.2f;
      } else if(m>0.001f){
        /* roche a facettes : chaque triangle est un plan eclaire par le haut-gauche ;
           aretes vives (pli sombre cote ombre, liseré clair cote lumiere), touches de pinceau */
        int o=y*SW+u; float shade=fc_sh[o];
        float far=clampf((u-edge_u)/120.0f,0,1);
        float st=an((float)u,(float)y,du,dy,0.045f,0.30f,seed+6);
        float v = 0.50f + 1.25f*(shade-0.74f) + 0.08f*(st-0.5f) - 0.08f*far;
        int id=fc_id[o]; float dmax=0, dmin=0;
        { const int NU[4]={1,-1,0,0}, NY[4]={0,0,1,-1};
          for(int q=0;q<4;q++){ int u2=u+NU[q], y2=y+NY[q]; if(u2<0||u2>=SW) continue; if(FI(u2,y2)!=id){ float d=FS(u2,y2)-shade; if(d>dmax) dmax=d; if(d<dmin) dmin=d; } } }
        int edge = dmax>0.15f ? 1 : (dmin<-0.20f ? 2 : 0);
        if(edge==2) v+=0.13f;
        if(m<1.0f) v+=0.25f*(1-m);    /* rebord de la falaise eclaire */
        c=rampd(p->rock,6,v,u,y,0.22f);
        if(edge==1) c=col_lerp(c,p->line,0.6f);
        float cr=an((float)u,(float)y,du,dy,0.020f,0.11f,seed+7); float rid=1.0f-fabsf(2*cr-1);
        bool deep = rid>0.986f && pn((float)u,(float)y,0.03f,seed+14)>0.62f;
        if(deep) c=col_lerp(c,p->line,0.7f);
        /* couverture des sommets (neige, mousse...) : sur les facettes tournees vers le ciel, surtout pres du rebord */
        { uint32_t fh=(uint32_t)id*2654435761u^seed; fh^=fh>>15; float frn=(fh&1023)/1023.0f;
          float cov=0.6f*clampf((fc_nz[o]-0.80f)/0.2f,0,1)+0.25f*frn+0.30f*(1-far)+0.12f*(pn((float)u,(float)y,0.06f,seed+4)-0.5f)+0.05f*(st-0.5f)+0.15f*(shade-0.7f);
          float th=1.0f-0.55f*p->top;
          if(p->top>0 && cov>th && !deep && edge!=1){
            float k=clampf((cov-th)*9.0f,0,1);   /* bord de la plaque : ton plus sombre */
            c=ramp(p->cov,5,0.12f+0.42f*k+0.4f*shade+0.08f*(st-0.5f),u,y); } }
        h = 0.5f + fc_h[o]/4.4f;
        if(edge==1||deep) h-=0.08f;
        SPc(u,y,(int)(p->spec*255*(0.3f+v)));
      } else {
        /* replat : couverture du sol, ombres douces au pied de la falaise */
        float sv=0.58f+0.30f*(n1-0.5f)+0.30f*(n2-0.5f)+0.25f*(pn((float)u,(float)y,0.07f,seed+8)-0.5f);
        float foot=clampf((edge_u-u)/9.0f,0,1); sv-= (1-foot)*0.35f;
        float nearpath=clampf(u/6.0f,0,1); sv-=(1-nearpath)*0.15f;
        c=ramp(p->cov,5,sv,u,y); h=0.2f+0.05f*n2;
        SPc(u,y,(int)(p->spec*120));
      }
      P(u,y,c); HG(u,y,h);
    }
  }
  /* contour de la falaise sur le replat (ligne sombre + ombre portee vers le chemin) */
  for(int y=0;y<SH;y++) for(int u=1;u<SW-1;u++){
    bool a=GH(u,y)>0.4f, b=GH(u-1,y)>0.4f;
    if(a && !b){ P(u,y,p->line); PB(u-1,y,p->line,0.45f); PB(u-2,y,p->line,0.25f); PB(u-3,y,p->line,0.12f); }
  }
  /* bordure du chemin : muret de pierres arrondies (u 0..6), certaines coiffees (neige, mousse...) */
  { int yy=0; while(yy<SH){ int seg=irange(7,14); if(SH-yy<9) seg=SH-yy;
      bool capped = chance(p->top*0.9f); float tone=frange(-0.08f,0.08f);
      for(int y=yy;y<yy+seg;y++) for(int u=0;u<8;u++){
        int dy=y-yy, de=seg-1-dy;
        if(u==7){ PB(u,y,p->line,0.45f); continue; }                      /* ombre sur le replat */
        if((dy==0||de==0) && (u==0||u==6)){ PB(u,y,p->line,0.6f); continue; }   /* coins arrondis */
        uint32_t c;
        if(dy==0) c=p->line;                                              /* joint */
        else if(u==0) c=rampd(p->rock,6,0.22f+tone,u,y,0.2f);            /* face tournee vers le chemin */
        else if(u==6) c=rampd(p->rock,6,0.18f+tone,u,y,0.2f);
        else { float l=0.62f+tone-0.04f*u+(dy==1?0.14f:0)-(de<=1?0.18f:0); c=rampd(p->rock,6,l,u,y,0.35f); }
        if(capped && dy>=1 && dy<=2+(seg>10) && u>=1 && u<=5) c=rampd(p->cov,5,0.5f+0.25f*(dy==1),u,y,0.3f);
        P(u,y,c); HG(u,y,u==0?0.28f:(dy==0?0.36f:0.5f));
      }
      yy+=seg; } }
  /* accessoires */
  int nprops=0;
  for(int i=0;i<46;i++){
    int y=irand(SH); float cs=shelf+wave(&wcs,(float)y)+wave(&wcs2,(float)y);
    int u=irange(9,maxi(10,(int)cs-4));
    bool near=false; for(int k=0;k<3;k++){ int ty=SC_TORCH_Y[side][k]; int d=abs(y-ty); if(d>SH/2) d=SH-d; if(d<14 && u<16) near=true; } if(near) continue;
    switch(biome){
      case 0: if(i%3==0) bones(u,y); else if(i%3==1) boulder(u,y,irange(3,6),irange(2,4),p,false,0.4f); else if(i%5==2) candle_grp(u,y); else tuft(u,y,HEX(0x2e4a2e),HEX(0x4a6a3a),3); break;
      case 1: if(i%4==0) mushroom_big(u,y,chance(0.35f)); else if(i%4==1) boulder(u,y,irange(3,6),irange(2,4),p,true,0.4f); else tuft(u,y,HEX(0x3a8a3a),HEX(0x6ac04a),irange(4,7)); break;
      case 2: if(i%3==0) boulder(u,y,irange(3,6),irange(2,4),p,false,0.4f); else if(i%4==1){ for(int k=0;k<6;k++){ int uu=u+irange(-4,4), yy=y+irange(-3,3); P(uu,yy,HEX(0xff8a2a)); E(uu,yy,140+irand(100)); } glow((float)u,(float)y,HEX(0xff6a1a),20); } else tuft(u,y,HEX(0x3a2a28),HEX(0x5a4038),3); break;
      case 3: if(i%3==0){ crystal(u,y,HEX(0xd2b0f5),HEX(0x7a4ab8),10,128); glow((float)u,(float)y-4,HEX(0xa060ff),26); } else if(i%3==1) boulder(u,y,irange(3,6),irange(2,4),p,true,0.4f); else tuft(u,y,HEX(0x4a2266),HEX(0x7a4ab8),4); break;
      case 4: if(i%3==0){ static const uint32_t BC[4]={HEX(0x5a4a8a),HEX(0x7a6ab0),HEX(0x9a8ad0),HEX(0xd8ccff)}; bush(u,y,BC,irange(8,13)); }
              else if(i%3==1) boulder(u,y,irange(5,9),irange(3,6),p,true,0.4f);
              else if(i%5==2){ crystal(u,y,HEX(0xe8faff),HEX(0x7ac8f0),9,128); glow((float)u,(float)y-4,HEX(0x9ae8ff),24); }
              else tuft(u,y,HEX(0x8a7ac0),HEX(0xc8bcf0),4); break;
      case 5: if(i%4==0) column_stump(u-5,y); else if(i%4==1) coral(u,y,chance(0.5f)); else if(i%4==2) boulder(u,y,irange(3,6),irange(2,4),p,true,0.4f); else tuft(u,y,HEX(0x2a8a4a),HEX(0x3aa85a),5); break;
      case 6: if(i%3==0) boulder(u,y,irange(3,6),irange(3,5),p,true,0.4f); else if(i%3==1){ for(int dy2=-3;dy2<=3;dy2++) for(int du2=-5;du2<=5;du2++) if(du2*du2+dy2*dy2*2.5f<26){ P(u+du2,y+dy2,HEX(0xd08a20)); E(u+du2,y+dy2,1+irand(80)); } glow((float)u,(float)y,HEX(0xffa030),22); } else tuft(u,y,HEX(0xa07840),HEX(0xd8b070),3); break;
      case 8: if(i%4==0) mushroom_big(u,y,true); else if(i%4==1) boulder(u,y,irange(3,6),irange(2,4),p,true,0.4f);
              else if(i%4==2){ static const uint32_t FC[3]={HEX(0xf6f2ea),HEX(0xf59ac0),HEX(0xc8a8ff)}; for(int k=0;k<5;k++){ int uu=u+irange(-5,5), yy=y+irange(-3,3); P(uu,yy,FC[k%3]); P(uu+1,yy,HEX(0xf9dc5c)); } }
              else { tuft(u,y,HEX(0x2e7a3a),HEX(0x6ac04a),irange(4,7)); if(chance(0.3f)){ P(u,y-6,HEX(0xeaff6a)); E(u,y-6,1+irand(100)); glow((float)u,(float)y-6,HEX(0xd8ff6a),16); } } break;
      case 9: if(i%4==0) boulder(u,y,irange(3,6),irange(2,4),p,false,0.4f);
              else if(i%4==1){ tuft(u,y,HEX(0x5a5a24),HEX(0x9a8a3a),irange(5,8)); }
              else if(i%5==2){ for(int dy2=-2;dy2<=2;dy2++) for(int du2=-5;du2<=5;du2++) if(du2*du2+dy2*dy2*4<26) P(u+du2,y+dy2,HEX(0x1c2414)); P(u,y,HEX(0xb8e040)); E(u,y,1+irand(120)); glow((float)u,(float)y,HEX(0xb8e040),18); }
              else tuft(u,y,HEX(0x3a4020),HEX(0x6a7a2a),4); break;
      case 10: if(i%3==0){ bool cy=chance(0.4f); crystal(u,y,cy?HEX(0xc8f8ff):HEX(0xffc8f0),cy?HEX(0x3a8ab8):HEX(0x9a3a8a),irange(8,13),cy?128:1); glow((float)u,(float)y-4,cy?HEX(0x9af8ff):HEX(0xff8ad8),24); }
               else if(i%3==1) boulder(u,y,irange(3,6),irange(2,4),p,false,0.4f); else tuft(u,y,HEX(0x4a4258),HEX(0x6a6278),3); break;
      case 11: if(i%4==0){ static const uint32_t CL[6]={HEX(0xf0dcb0),HEX(0xc8a46e),HEX(0x7a5a30),HEX(0xfff0c8),HEX(0xd8b070),HEX(0x1e1206)}; column_stump_c(u-5,y,CL); }
               else if(i%4==1) boulder(u,y,irange(3,6),irange(2,4),p,true,0.4f);
               else if(i%4==2){ for(int k=0;k<4;k++){ int uu=u+irange(-4,4), yy=y+irange(-3,3); P(uu,yy,HEX(0xffd84a)); E(uu,yy,1+irand(100)); } glow((float)u,(float)y,HEX(0xffc040),18); }
               else tuft(u,y,HEX(0xa08850),HEX(0xd8c088),3); break;
      default: if(i%3==0) boulder(u,y,irange(3,6),irange(2,4),p,false,0.4f); else for(int k=0;k<3;k++) star(u+irange(-6,6),y+irange(-6,6)); break;
    }
    nprops++;
  }
  /* details dans la falaise */
  for(int i=0;i<40;i++){
    int y=irand(SH); float cs=shelf+wave(&wcs,(float)y)+wave(&wcs2,(float)y); int u=irange((int)cs+6,SW-8); if(u>=SW-8) continue;
    if(GH(u,y)<0.4f) continue;
    hg_off=GH(u,y)-0.45f;
    switch(biome){
      case 0: if(i%4==0){ for(int dy2=-1;dy2<9;dy2++) for(int du2=-1;du2<10;du2++){ bool in = du2>=0&&du2<9&&dy2>=0 && !(dy2<2 && (du2<2-dy2||du2>6+dy2)); bool rim = !in && du2>=-1&&du2<=9 && !(dy2<1 && (du2<1||du2>7));
                if(in) P(u+du2,y+dy2,col_lerp(p->line,p->rock[1],0.35f+0.06f*dy2)); else if(rim) PB(u+du2,y+dy2,p->rock[5],0.5f); }
                skull(u+2,y+3,HEX(0xc8bca4),HEX(0x1a1420)); } else if(i%7==3) candle_grp(u,y); break;
      case 1: if(i%3==0) tuft(u,y,HEX(0x3a8a3a),HEX(0x5aa846),5); else if(i%5==1) mushroom_big(u,y,chance(0.4f)); break;
      case 2: if(i%4==0) lava_vein((float)u,(float)y,irange(16,40)); break;
      case 3: if(i%5==0) eye(u,y); else if(i%3==0){ crystal(u,y,HEX(0xd2b0f5),HEX(0x7a4ab8),12,128); glow((float)u,(float)y-4,HEX(0xa060ff),22); } break;
      case 4: break;
      case 5: if(i%3==0) tuft(u,y,HEX(0x2a8a4a),HEX(0x3aa85a),6); else if(i%7==1) coral(u,y,chance(0.5f)); break;
      case 6: if(i%3==0) honey_drip(u,y,irange(5,14)); break;
      case 8: if(i%3==0) tuft(u,y,HEX(0x2e7a3a),HEX(0x6ac04a),5); else if(i%5==1) mushroom_big(u,y,true); break;
      case 9: if(i%3==0) tuft(u,y,HEX(0x3a4020),HEX(0x6a7a2a),5); break;
      case 10: if(i%3==0){ bool cy=chance(0.5f); crystal(u,y,cy?HEX(0xc8f8ff):HEX(0xffc8f0),cy?HEX(0x3a8ab8):HEX(0x9a3a8a),12,cy?128:1); glow((float)u,(float)y-4,cy?HEX(0x9af8ff):HEX(0xff8ad8),22); } break;
      case 11: if(i%4==0){ float a=frange(PI*0.3f,PI*0.7f), uu=(float)u, yy=(float)y; int len=irange(12,28);
                 for(int k=0;k<len;k++){ a+=frange(-0.4f,0.4f); a=clampf(a,PI*0.15f,PI*0.85f); uu+=cosf(a)*0.6f; yy+=sinf(a); P((int)uu,(int)yy,HEX(0xffd84a)); E((int)uu,(int)yy,1+((int)(yy*1.3f)&127)); P((int)uu+1,(int)yy,HEX(0x5a3a10)); if(k%14==7) glow(uu,yy,HEX(0xffc040),20); } } break;
      default: if(i%2==0) for(int k=0;k<4;k++) star(u+irange(-8,8),y+irange(-8,8)); break;
    }
  }
  hg_off=0;
  /* ruche : alveoles sur la falaise */
  if(biome==6){
    const float hs=10.0f/1.7320508f, cw=1.5f*hs;
    for(int y=0;y<SH;y++) for(int u=0;u<SW;u++){
      if(GH(u,y)<0.45f || sc_em[side][wy(y)*SW+tx(u)]) continue;
      if(pn((float)u,(float)y,0.05f,seed+21)<0.55f) continue;
      int ci=(int)floorf(u/cw); float best=1e9f, sec=1e9f;
      for(int i=ci-1;i<=ci+1;i++){ float cx=i*cw; float off=(i&1)?5.0f:0.0f; int cj=(int)floorf((y-off)/10.0f);
        for(int j=cj-1;j<=cj+1;j++){ float cy=j*10.0f+off; float d=sqrtf((u-cx)*(u-cx)+(y-cy)*(y-cy)); if(d<best){ sec=best; best=d; } else if(d<sec) sec=d; } }
      if(sec-best<1.2f) P(u,y,HEX(0x2a1606)); else if(best<3.5f && ((u*7+y*13)%11)==0){ P(u,y,HEX(0xd08a20)); E(u,y,1+irand(60)); }
    }
  }
  /* neant : etoiles dans le vide */
  if(voidb) for(int i=0;i<260;i++){ int u=irand(SW), y=irand(SH); if(GH(u,y)<0) star(u,y); }
  /* torches : socles de pierre sur la bordure */
  for(int k=0;k<3;k++){ int ty=SC_TORCH_Y[side][k];
    for(int j=-2;j<6;j++) for(int q=5;q<12;q++){ uint32_t c=q==5?p->rim[3]:(q>=10?p->rim[0]:p->rim[2]); if(j==-2) c=p->rim[3]; P(q,ty+j,c); HG(q,ty+j,0.6f); }
    for(int q=5;q<12;q++) PB(q,ty+6,p->line,0.5f); }
  /* normales et brillance */
  for(int i=0;i<SW*SH;i++) if(sc_em[side][i]) sc_sp[i]=0;
  nrm_from_height(sc_hg,SW,SH,sc_nrm[side],2.2f,sc_sp,true);
  if(side==0){ /* les normales sont calculees dans l'espace de la texture (deja dans le bon sens) */ }
  (void)nprops;
}

/* ---------- paroi du puits (vue 2.5D) : mur de pierres en assises, vu de biais ---------- */
static uint32_t fnrm(float nx,float ny,float nz,int sp){ float l=sqrtf(nx*nx+ny*ny+nz*nz); nx/=l; ny/=l; nz/=l;
  return ((uint32_t)clampi(sp,0,255)<<24)|((uint32_t)clampi((int)(nx*127.5f+128),0,255)<<16)|((uint32_t)clampi((int)(ny*127.5f+128),0,255)<<8)|(uint32_t)clampi((int)(nz*127.5f+128),0,255); }
static void build_face(int biome,int side,uint32_t seed){
  const ScPal *p=&PAL[biome];
  const int FW=SC_FW;
  if(!sc_face[side].px) sc_face[side]=spr_alloc(FW,SH);
  uint32_t *px=sc_face[side].px, *nm=sc_fnrm[side]; uint8_t *em=sc_fem[side];
  memset(em,0,(size_t)FW*SH);
  float sg = side==0 ? 1.0f : -1.0f;              /* la paroi regarde le puits */
  /* quatre assises de pierres de 6 px (u 0..23), rebord u 23..25 */
  enum { NC=4, CH=6, LIP=FW-3 };
  int len[NC][64]; int nst[NC];
  for(int c=0;c<NC;c++){ int y=sirand(12), k=0; while(y<SH+24 && k<64){ int l=sirange(10,22); len[c][k++]=l; y+=l; } nst[c]=k; }
  int start[NC]; for(int c=0;c<NC;c++) start[c]=sirand(14);
  bool cov = p->top>0.2f;
  for(int y=0;y<SH;y++) for(int u=0;u<FW;u++){
    int o=y*FW+u; uint32_t c; float v; int spv=(int)(p->spec*150);
    float nx=0.82f*sg, ny=0, nz=0.57f;
    if(u>=LIP){   /* rebord : la couverture du replat deborde un peu */
      float n=pn((float)u,(float)y,0.20f,seed+3);
      if(cov){ c=ramp(p->cov,5,0.30f+0.18f*(u-LIP)+0.25f*(n-0.5f),u,y); }
      else c=rampd(p->rock,6,0.62f+0.10f*(u-LIP)+0.15f*(n-0.5f),u,y,0.3f);
      if(u==LIP) c=col_lerp(c,p->line,0.35f);
      nx=0.45f*sg; nz=0.89f;
    } else {
      int cc=mini(u/CH,NC-1); int cu=u-cc*CH;
      /* pierre de l'assise : on retrouve son debut */
      int yy=((y-start[cc])%SH+SH)%SH, k=0, acc=0; while(k<nst[cc]-1 && acc+len[cc][k]<=yy){ acc+=len[cc][k]; k++; }
      int dy=yy-acc, dl=len[cc][k]-1-dy;
      uint32_t h=(uint32_t)(k*97+cc*31)*2654435761u^seed; h^=h>>13; float tone=((h&255)/255.0f-0.5f)*0.14f;
      float n=pn((float)u,(float)y,0.18f,seed+5);
      v=0.26f+0.30f*(u/(float)(LIP-1))+tone+0.10f*(n-0.5f);
      bool joint = cu==0 || dy==0;
      if(joint){ c=col_lerp(rampd(p->rock,6,v-0.12f,u,y,0.2f),p->line,0.65f); spv=0; }
      else {
        if(cu==CH-1) v+=0.10f;              /* arete du haut de la pierre, eclairee */
        if(cu==1) v-=0.06f;
        if(dy==1) v+=0.05f; if(dl==0) v-=0.06f;
        if(((u*7+y*13+(int)(h>>8))%23)==0) v-=0.10f;   /* grain */
        c=rampd(p->rock,6,v,u,y,0.30f);
        if(cu==CH-1) nx=0.55f*sg, nz=0.83f; else if(cu==1) nx=0.92f*sg, nz=0.38f;
        if(dy==1) ny=-0.25f; else if(dl==0) ny=0.25f;
      }
      if(u<6) c=col_lerp(c,p->line,0.45f*(1-u/6.0f));   /* pied de la paroi dans l'ombre */
    }
    px[o]=c|0xFF000000u; nm[o]=fnrm(nx,ny,nz,spv);
  }
  /* coulures depuis le rebord, selon le biome */
  int ndrip = biome==1||biome==4||biome==5||biome==6||biome==8||biome==9 ? 70 : (biome==2||biome==3||biome==7||biome==10||biome==11 ? 30 : 18);
  for(int i=0;i<ndrip;i++){
    int y=irand(SH), l=irange(2,biome==1||biome==5?13:8);
    for(int j=0;j<l;j++){ int u=LIP-1-j; if(u<1) break; int o=wy(y)*FW+u; float f=1.0f-(float)j/l;
      uint32_t c=px[o];
      switch(biome){
        case 1: c=col_lerp(c,p->cov[2+(j&1)],0.85f*f+0.15f); break;                       /* mousse et lianes */
        case 4: c=col_lerp(HEX(0xd8e6ff),HEX(0x8ab0e8),(float)j/l); nm[o]=fnrm(0.5f*sg,0,0.86f,200); break;   /* glacons */
        case 5: c=col_lerp(c,p->cov[2],0.8f*f); break;                                     /* algues */
        case 6: c=col_lerp(HEX(0xffc040),HEX(0xc07010),(float)j/l); em[o]=(uint8_t)(40+60*f); break;   /* miel */
        case 2: if(j<3){ c=HEX(0xff7a1a); em[o]=(uint8_t)(140+80*f); } break;               /* braises dans les joints */
        case 3: if(j<2){ c=HEX(0xc08aff); em[o]=110; } break;                              /* eclats de cristal */
        case 7: if(j==0){ c=HEX(0xd8e0ff); em[o]=(uint8_t)(60+irand(120)); } break;        /* poussiere d'etoiles */
        case 8: c=col_lerp(c,p->cov[2+(j&1)],0.85f*f+0.15f); break;                       /* lierre */
        case 9: c=col_lerp(c,HEX(0x2a3216),0.8f*f); break;                                 /* vase */
        case 10: if(j<2){ c=(y&1)?HEX(0xff8ad8):HEX(0x9af8ff); em[o]=(uint8_t)((y&1)?60:200); } break;   /* eclats de cristal */
        case 11: if(j==0){ c=HEX(0xffd84a); em[o]=(uint8_t)(1+irand(120)); } break;         /* dorures */
        default: c=col_lerp(c,p->cov[1],0.5f*f); break;
      }
      px[o]=c|0xFF000000u;
    }
  }
  /* supports des torches : equerre de fer forge */
  for(int k=0;k<3;k++){ int ty=SC_TORCH_Y[side][k];
    for(int j=-1;j<=2;j++) for(int u=7;u<=12;u++){ int o=wy(ty+j)*FW+u; bool edge= u==7||u==12||j==-1||j==2;
      px[o]=(edge?HEX(0x0e0c10):HEX(0x3a3440))|0xFF000000u; nm[o]=fnrm(0.6f*sg,0,0.8f,90); }
    for(int u=3;u<=7;u++){ int o=wy(ty)*FW+u; px[o]=HEX(0x22202a)|0xFF000000u; } }
}
Sprite *scenery_face(int side){ return &sc_face[side]; }
const uint32_t *scenery_face_nrm(int side){ return sc_fnrm[side]; }
const uint8_t *scenery_face_em(int side){ return sc_fem[side]; }

/* v8 : flancs et parois precalcules en HD (version basse definition pour le processeur) */
static bool sc_from_pack(int biome){
  if(!hd_available()) return false;
  for(int s=0;s<2;s++){ if(!sc_tex[s].px) sc_tex[s]=spr_alloc(SW,SH); if(!sc_face[s].px) sc_face[s]=spr_alloc(SC_FW,SH); }
  bool ok=true;
  for(int s=0;s<2 && ok;s++) ok= hd_region_low(biome,1+s,sc_tex[s].px,sc_em[s],sc_nrm[s],SW,SH) && hd_region_low(biome,3+s,sc_face[s].px,sc_fem[s],sc_fnrm[s],SC_FW,SH);
  if(!ok) return false;
  for(int s=0;s<2;s++){ for(int i=0;i<SW*SH;i++) sc_tex[s].px[i]|=0xFF000000u; for(int i=0;i<SC_FW*SH;i++) sc_face[s].px[i]|=0xFF000000u; }
  sg_n=hd_glows(biome,1,sg_x,sg_y,sg_r,sg_c,SC_MAXG);
  return true;
}
void scenery_build(int biome,uint32_t seed){
  sc_biome=biome; sg_n=0;
  if(sc_from_pack(biome)) return;
  srng=seed*2654435761u+0x9e3779b9u; if(!srng) srng=1;
  for(int s=0;s<2;s++){ memset(sc_hg,0,sizeof sc_hg); memset(sc_sp,0,sizeof sc_sp); build_side(biome,s,seed+s*977); }
  for(int s=0;s<2;s++) build_face(biome,s,seed+s*331+17);
}
Sprite *scenery_tex(int side){ return &sc_tex[side]; }
/* palettes (atelier HD) */
void scenery_pal(int b,uint32_t rock[6],uint32_t cov[5],uint32_t *line,uint32_t rim[4],float *top,float *shelf,float *spec){
  const ScPal *p=&PAL[clampi(b,0,BIOME_COUNT-1)];
  for(int i=0;i<6;i++) rock[i]=p->rock[i]&0xFFFFFFu; for(int i=0;i<5;i++) cov[i]=p->cov[i]&0xFFFFFFu; for(int i=0;i<4;i++) rim[i]=p->rim[i]&0xFFFFFFu;
  *line=p->line&0xFFFFFFu; *top=p->top; *shelf=p->shelf; *spec=p->spec;
}
const uint32_t *scenery_nrm(int side){ return sc_nrm[side]; }
const uint8_t *scenery_em(int side){ return sc_em[side]; }
int scenery_glows(const float **x,const float **y,const float **r,const uint32_t **c){ *x=sg_x; *y=sg_y; *r=sg_r; *c=sg_c; return sg_n; }
