/* GOUFFRE - cinematiques : la legende du Gouffre (introduction) et les deux fins.
   Tout est peint par le programme, plan par plan : decors en couches, personnages et gardiens du jeu,
   particules, lumieres (GPU ou logiciel), sous-titres a la machine a ecrire, musique calee sur les plans. */
#include "game.h"

extern float g_boss_draw_scale;
void menu_tunnel(float t,float speed,int bright);   /* menus.c : le puits vu d'en haut */
void camp_world(float dt,float amb);                /* menus.c : le campement, sans interface */

#define LB 32                 /* bandes noires (format cinema) */
#define VY0 LB
#define VY1 (SCREEN_H-LB)
enum { FD_NONE, FD_BLACK, FD_WHITE };
typedef void (*ShotFn)(float t,float d);
typedef struct { float dur; ShotFn fn; const char *c1; float t1; const char *c2; float t2; float cend; uint8_t fin, fout; } Shot;

static int cn_id=0, cn_ret=GS_TITLE; static float cn_t=0, cn_hold=0, cn_dt=0, cn_prev=-1, cn_hint=0;
static int cn_shot=-1; static bool cn_done=false;
static const Shot *cn_list=NULL; static int cn_n=0;
static float cn_shake=0, cn_flash=0;     /* tremblement et eclair blanc demandes par le plan */
static int cn_hero=CH_SENTINELLE;        /* heros mis en avant (le vainqueur pour les fins) */

/* ---------------- petits outils ---------------- */
static uint32_t cs_r=1;
static void cs_seed(uint32_t s){ cs_r=s*2654435761u+0x6b43a9b5u; if(!cs_r) cs_r=1; }
static inline uint32_t cs_next(void){ cs_r^=cs_r<<13; cs_r^=cs_r>>17; cs_r^=cs_r<<5; return cs_r; }
static inline float cs_f(void){ return (cs_next()>>8)*(1.0f/16777216.0f); }
static inline float cs_rg(float a,float b){ return a+(b-a)*cs_f(); }
static inline float h01(uint32_t i){ i^=i>>16; i*=0x7feb352du; i^=i>>15; i*=0x846ca68bu; i^=i>>16; return (i&0xFFFFFF)/16777216.0f; }
static inline float smooth01(float x){ x=clampf(x,0,1); return x*x*(3-2*x); }
static inline float ease_out(float x){ x=clampf(x,0,1); return 1-(1-x)*(1-x); }
static inline float seg(float t,float a,float b){ return clampf((t-a)/(b-a),0,1); }
static const uint8_t BY[4][4]={{0,8,2,10},{12,4,14,6},{3,11,1,9},{15,7,13,5}};
static inline void pxs(int x,int y,uint32_t c){ if(x<gT->cx0||y<gT->cy0||x>=gT->cx1||y>=gT->cy1) return; gT->px[y*gT->w+x]=c; }
static inline bool cue(float t,float at){ return cn_prev<at && t>=at; }   /* vrai une seule fois quand le temps du plan franchit « at » */
static void emis_on(int *ly){ *ly=layer_current(); if(g_gpu) layer(LY_EMIS); }
static void emis_off(int ly){ layer(ly); }

/* degrade a trois couleurs, tramé pour eviter les bandes */
static void grad3(int y0,int y1,uint32_t a,uint32_t b,uint32_t c,float mid){
  for(int y=y0;y<y1;y++){
    if(y<gT->cy0||y>=gT->cy1) continue;
    float t=(float)(y-y0)/(float)maxi(1,y1-y0-1), r,g,bb;
    if(t<mid){ float k=t/mid; r=lerpf((float)cr(a),(float)cr(b),k); g=lerpf((float)cg(a),(float)cg(b),k); bb=lerpf((float)cb(a),(float)cb(b),k); }
    else { float k=(t-mid)/(1-mid); r=lerpf((float)cr(b),(float)cr(c),k); g=lerpf((float)cg(b),(float)cg(c),k); bb=lerpf((float)cb(b),(float)cb(c),k); }
    uint32_t *row=&gT->px[y*gT->w];
    for(int x=gT->cx0;x<gT->cx1;x++){ float d=((BY[y&3][x&3]+0.5f)/16.0f-0.5f)*2.2f; row[x]=rgb((int)(r+d),(int)(g+d),(int)(bb+d)); }
  }
}
static void stars(uint32_t seed,int n,int y0,int y1,float t,float a){
  if(a<=0.01f) return;
  int ly; emis_on(&ly);
  for(int i=0;i<n;i++){
    float hx=h01(seed+i*3u), hy=h01(seed+i*3u+1), hb=h01(seed+i*3u+2);
    int x=(int)(hx*SCREEN_W), y=y0+(int)(hy*(float)(y1-y0));
    float tw=0.6f+0.4f*sinf(t*(1.0f+hb*2.5f)+(float)i);
    float al=a*tw*(0.3f+0.7f*hb);
    uint32_t c= hb>0.85f ? HEX(0xffe6c8) : HEX(0xd4dcff);
    if(g_gpu){ gfx_pset(x,y,col_scale(c,al)); if(hb>0.94f) fx_star(x+0.5f,y+0.5f,2.2f,c,al*0.55f); }
    else gfx_padd(x,y,c,(int)(al*255));
  }
  emis_off(ly);
}
static void moon(float x,float y,float r,float a){
  fx_glow(x,y,r*5,HEX(0x8a90ff),0.16f*a); fx_glow(x,y,r*2.2f,HEX(0xd8d4ff),0.22f*a);
  gfx_disc((int)x,(int)y,(int)r,HEX(0xc8c4b4)); gfx_disc((int)x-1,(int)y-1,(int)r-1,HEX(0xeeeadc));
  gfx_disc((int)(x+r*0.3f),(int)(y-r*0.2f),(int)(r*0.22f),HEX(0xd2cebe)); gfx_disc((int)(x-r*0.35f),(int)(y+r*0.3f),(int)(r*0.16f),HEX(0xd6d2c2));
  gfx_disc((int)(x+r*0.1f),(int)(y+r*0.45f),(int)(r*0.12f),HEX(0xd8d4c4));
  int ly; emis_on(&ly); if(g_gpu) gfx_disc((int)x,(int)y,(int)r,col_scale(HEX(0xeeeadc),0.3f*a)); emis_off(ly);
}
/* chaine de montagnes : faces eclairees du cote de la lumiere (lx>0 : lumiere a droite), neige sur les sommets */
static float ridge(float x,uint32_t seed,float sc){ float n=fbm(x*sc,0.37f,seed); float r=1.0f-fabsf(2*n-1); return r*r*0.75f+n*0.25f; }
static void mountains(uint32_t seed,float base,float amp,float sc,float off,uint32_t col,uint32_t lit,uint32_t snow,int ybot,float lx){
  for(int x=0;x<SCREEN_W;x++){
    float h=base-amp*ridge((float)x+off,seed,sc);
    float hl=base-amp*ridge((float)x+off-6,seed,sc), hr=base-amp*ridge((float)x+off+6,seed,sc);
    float sl=(hr-hl)*lx;                    /* >0 : la face regarde la lumiere */
    float k=smooth01(sl*0.12f);
    int y0=(int)h; float top=base-amp;
    for(int y=y0;y<ybot;y++){
      if(y<gT->cy0||y>=gT->cy1) continue;
      float dep=(float)(y-y0)/(amp*0.9f+1);
      uint32_t c=col;
      if(k>0) c=col_lerp(col,lit,k*clampf(1.0f-dep,0,1));
      float st=h01((uint32_t)(x*7)+seed); if(((y+(int)(st*8))%7)==0 && dep>0.05f) c=col_scale(c,0.92f);
      if(snow && h<top+amp*0.3f && y-y0<(int)((top+amp*0.3f-h)*0.5f)+1) c=col_lerp(snow,c,0.25f+0.5f*(1-k));
      gT->px[y*gT->w+x]=c|0xFF000000u;
    }
  }
}
/* village sur sa colline (la colline elle-meme est dessinee par l'appelant) */
static float hill_y(float x,float cx,float base){ float d=(x-cx)/115.0f; return base-30*expf(-d*d); }
static void village(float cx,float base,float lights,float t,float day){
  cs_seed(4242);
  uint32_t wall=col_lerp(HEX(0x22203a),HEX(0x8a6a6a),day), wall2=col_lerp(HEX(0x1a1830),HEX(0x6a5058),day), roof=col_lerp(HEX(0x141228),HEX(0x5a3a3a),day);
  int ly;
  for(int i=0;i<12;i++){
    float x=cx-112+i*20.0f+cs_rg(-4,4); float w=cs_rg(10,15), h=cs_rg(8,12); bool tower=(i==6);
    if(tower){ w=11; h=24; }
    float gy=hill_y(x+w*0.5f,cx,base)+2;
    int x0=(int)x, y0=(int)(gy-h), xw=(int)w, yh=(int)h;
    gfx_rect(x0,y0,xw,yh+4,wall); gfx_rect(x0+xw-3,y0,3,yh+4,wall2);
    gfx_tri((float)x0-2,(float)y0,(float)(x0+xw+2),(float)y0,(float)x0+xw*0.5f,(float)y0-(tower?14.0f:w*0.55f),roof);
    if(!tower){ gfx_rect(x0+xw-5,y0-6,2,5,wall2); }
    /* fenetres */
    int nw=tower?1:(xw>12?2:1);
    for(int k=0;k<nw;k++){
      float th=h01(77u+(uint32_t)i*13u+(uint32_t)k); bool on=th<lights;
      int wx=x0+3+k*5, wy=y0+(tower?6:3);
      uint32_t wc= on ? HEX(0xffc65a) : col_lerp(HEX(0x0c0a16),HEX(0x3a2a2a),day);
      gfx_rect(wx,wy,2,tower?3:2,wc);
      if(on && day<0.6f){ emis_on(&ly); if(g_gpu) gfx_rect(wx,wy,2,tower?3:2,HEX(0xffb84a)); emis_off(ly); fx_glow(wx+1.0f,wy+1.0f,7,HEX(0xffa040),0.18f*(1-day)); light_add(wx+1.0f,wy+1.0f,18,HEX(0xffb060),0.25f*(1-day)); }
    }
    /* fumee de cheminee */
    if(!tower) for(int k=0;k<3;k++){ float ph=fmodf(t*0.22f+k*0.33f+i*0.17f,1.0f); float sx=x0+xw-4+ph*9+sinf(ph*5+i)*1.5f, sy=y0-7-ph*22;
      fx_smoke(sx,sy,1.2f+ph*3.5f,col_lerp(HEX(0x5a5a78),HEX(0xd8c8c0),day),0.32f*(1-ph)); }
  }
}
/* champ : degrade + touffes d'herbe en perspective */
static void field(float off,int y0,uint32_t top,uint32_t bot,uint32_t tuft,float day){
  for(int x=0;x<SCREEN_W;x++){ int ys=(int)(y0+3*sinf((x+off)*0.012f)+2*sinf((x+off)*0.031f)); for(int y=ys;y<VY1+2 && y<SCREEN_H;y++){ if(y<gT->cy0) continue;
      float k=(float)(y-y0)/(float)(VY1-y0); float d=((BY[y&3][x&3]+0.5f)/16.0f-0.5f)*0.06f; gT->px[y*gT->w+x]=col_lerp(top,bot,clampf(k+d,0,1)); } }
  for(int i=0;i<520;i++){ float hx=h01(9000u+(uint32_t)i), hy=h01(19000u+(uint32_t)i); float y=y0+6+hy*hy*(VY1-y0-4); float x=fmodf(hx*900.0f-off*(0.6f+0.4f*hy)+900.0f,900.0f)-130.0f;
    int len=1+(int)(hy*3.5f); uint32_t c=col_lerp(tuft,col_scale(tuft,0.6f),h01(500u+(uint32_t)i)); for(int k=0;k<len;k++) pxs((int)x,(int)y-k,c); if(len>2){ pxs((int)x+1,(int)y-len+1,c); } }
  (void)day;
}
/* le trou du Gouffre vu de trois-quarts */
static void pit_hole(float cx,float cy,float rx,float ry,float open,uint32_t rim,uint32_t glowc,float glow){
  if(open<=0.01f) return;
  float RX=rx*open, RY=ry*open;
  for(int y=(int)(cy-RY)-1;y<=(int)(cy+RY)+1;y++){
    float dy=(y+0.5f-cy)/RY; if(fabsf(dy)>1) continue;
    float w=RX*sqrtf(1-dy*dy)*(1.0f+0.07f*(vnoise(y*0.35f,1.7f,77)-0.5f)*2);
    int x0=(int)(cx-w), x1=(int)(cx+w);
    float v=(dy+1)*0.5f;
    for(int x=x0;x<=x1;x++){
      uint32_t c;
      if(v<0.42f){ float k=v/0.42f; c=col_lerp(col_lerp(rim,HEX(0x2a2236),0.4f),HEX(0x07040c),k); if(((x*3+y*7)%11)==0) c=col_scale(c,0.85f); }
      else c=HEX(0x050309);
      pxs(x,y,c);
    }
    /* levre du bord proche, eclairee */
    if(dy>0.55f){ pxs(x0,y,rim); pxs(x1,y,rim); }
  }
  for(int x=(int)(cx-RX*0.9f);x<=(int)(cx+RX*0.9f);x++){ float u=(x+0.5f-cx)/RX; float yy=cy+RY*sqrtf(fmaxf(0,1-u*u)); pxs(x,(int)yy,rim); pxs(x,(int)yy+1,col_scale(rim,0.7f)); }
  if(glow>0.01f){ fx_glow(cx,cy+RY*0.1f,RX*1.1f,glowc,glow*0.6f); fx_glow(cx,cy,RX*0.45f,glowc,glow*0.8f); light_add(cx,cy-RY,RX*2.4f,glowc,glow*1.2f); }
}

/* ---------------- particules ---------------- */
enum { PK_MOTE, PK_DEBRIS, PK_SPARK, PK_SMOKE, PK_SNOW, PK_FW, PK_LEAF };
typedef struct { float x,y,vx,vy,g,life,max,r; uint32_t c; uint8_t k; } CP;
#define CPN 1800
static CP cp[CPN]; static int cpn=0;
static void cp_clear(void){ cpn=0; }
static void cp_add(float x,float y,float vx,float vy,float g,float life,float r,uint32_t c,int k){
  if(cpn>=CPN) return; CP *p=&cp[cpn++]; p->x=x; p->y=y; p->vx=vx; p->vy=vy; p->g=g; p->life=p->max=life; p->r=r; p->c=c; p->k=(uint8_t)k; }
static void cp_update(float dt){
  for(int i=0;i<cpn;){ CP *p=&cp[i]; p->life-=dt; if(p->life<=0){ cp[i]=cp[--cpn]; continue; }
    p->vy+=p->g*dt; p->x+=p->vx*dt; p->y+=p->vy*dt;
    if(p->k==PK_SNOW||p->k==PK_LEAF) p->x+=sinf(p->life*2.6f+(float)i)*10*dt;
    if(p->k==PK_FW){ p->vx*=1-1.2f*dt; p->vy*=1-1.2f*dt; }
    i++; }
}
static void cp_draw(int pass){   /* 0 : matiere (debris, fumee, neige) ; 1 : lumiere */
  int ly=layer_current(); if(pass==1) emis_on(&ly);
  for(int i=0;i<cpn;i++){ CP *p=&cp[i]; float f=p->life/p->max;
    switch(p->k){
      case PK_DEBRIS: if(pass==0){ int s=(int)(p->r+0.5f); gfx_rect((int)p->x,(int)p->y,s,s,p->c); if(s>1) gfx_pset((int)p->x,(int)p->y,col_scale(p->c,1.4f)); } break;
      case PK_SMOKE: if(pass==0) fx_smoke(p->x,p->y,p->r*(1.6f-f*0.6f),p->c,0.35f*f); break;
      case PK_SNOW: if(pass==0) gfx_pset((int)p->x,(int)p->y,p->c); break;
      case PK_LEAF: if(pass==0){ gfx_pset((int)p->x,(int)p->y,p->c); gfx_pset((int)p->x+1,(int)p->y,col_scale(p->c,0.8f)); } break;
      case PK_MOTE: if(pass==1){ float a=f<0.2f?f/0.2f:1; a*=minf(1,(p->max-p->life)*3); fx_glow(p->x,p->y,p->r*3.5f,p->c,0.28f*a); fx_disc(p->x,p->y,p->r*0.55f,p->c,1.1f*a); } break;
      case PK_SPARK: if(pass==1){ fx_caps(p->x,p->y,p->x-p->vx*0.04f,p->y-p->vy*0.04f,maxf(0.5f,p->r*0.5f),p->c,1.3f*f); } break;
      case PK_FW: if(pass==1){ fx_disc(p->x,p->y,maxf(0.6f,p->r*f),p->c,1.5f*f); if(i%3==0) fx_glow(p->x,p->y,4,p->c,0.25f*f); } break;
    }
  }
  if(pass==1) emis_off(ly);
}

/* ---------------- personnages ---------------- */
/* les tailles sont exprimees en multiples des anciens sprites (14x17) ; les sprites du jeu sont deux fois plus fins */
static bool cn_lo=false;   /* petites silhouettes a l'ancienne (campement) */
static void hero_draw(int ch,float x,float feet,float sc,int walk,int flip,uint32_t tint,int alpha){
  int spr=CHARS[ch].sprite; const Sprite *s= cn_lo ? &SPR_LO[spr] : &SPR[spr];
  const Sprite *w0= cn_lo ? &SPR_LO_WALK[spr][0] : &SPR_WALK[spr][0], *w1= cn_lo ? &SPR_LO_WALK[spr][1] : &SPR_WALK[spr][1];
  if(walk>0 && w0->px){ int f=walk&3; if(f==1) s=w0; else if(f==3) s=w1; }
  if(!cn_lo) sc/=EN_SCALE;
  DrawOpt o=dopt(); o.sx=o.sy=sc; o.flags=flip?DS_FLIPX:0; o.tint=tint; o.alpha=alpha;
  draw_spr_c(s,x,feet-s->h*sc*0.5f,&o);
}
static void mon_draw(int sp,float x,float y,float sc,int flip,uint32_t tint,int alpha,int frame){
  const Sprite *s=&SPR[sp];
  if(frame && SPR_WALK[sp][0].px) s=&SPR_WALK[sp][(frame-1)&1];
  sc/=EN_SCALE;
  DrawOpt o=dopt(); o.sx=o.sy=sc; o.flags=flip?DS_FLIPX:0; o.tint=tint; o.alpha=alpha;
  draw_spr_c(s,x,y,&o);
}
/* texte facon logo (degrade or et feu), pour « FIN » */
static void logo_text(int cx,int y,const char *T,int sc){
  text_draw(cx,y+sc*0.7f,T,HEX(0x0a0608),sc,AL_CENTER,0);
  text_draw(cx,y,T,HEX(0x9a3a12),sc,AL_CENTER,TX_OUTLINE);
  Target *tg=gT; int ox0=tg->cx0, oy0=tg->cy0, ox1=tg->cx1, oy1=tg->cy1;
  gfx_clip(0,y,SCREEN_W,y+7*sc); text_draw(cx,y,T,HEX(0xe08a2a),sc,AL_CENTER,0);
  gfx_clip(0,y,SCREEN_W,y+5*sc); text_draw(cx,y,T,HEX(0xf8c848),sc,AL_CENTER,0);
  gfx_clip(0,y,SCREEN_W,y+3*sc+2); text_draw(cx,y,T,HEX(0xfff4c0),sc,AL_CENTER,0);
  gfx_clip(ox0,oy0,ox1,oy1);
}

/* =====================================================================
   INTRODUCTION
   ===================================================================== */
typedef struct { float day, pan, crack, hole, pillar, lights, sun, gold, mist; } Valley;
/* fissures qui partent du centre du futur gouffre */
#define NCRK 12
static float crk_x[NCRK][18], crk_y[NCRK][18];
static void cracks_build(float cx,float cy){
  cs_seed(911);
  for(int i=0;i<NCRK;i++){ float a=i*TAU/NCRK+cs_rg(-0.2f,0.2f), x=cx, y=cy, len=cs_rg(110,250);
    for(int k=0;k<18;k++){ crk_x[i][k]=x; crk_y[i][k]=y; a+=cs_rg(-0.35f,0.35f); x+=cosf(a)*len/17; y+=sinf(a)*len/17*0.24f; } }
}
static void cracks_draw(float grow,float glow,float t){
  if(grow<=0) return;
  int ly=layer_current();
  for(int pass=0;pass<2;pass++){
    if(pass==1) emis_on(&ly);
    for(int i=0;i<NCRK;i++){ float g=clampf(grow*1.25f-(i%3)*0.12f,0,1); int n=(int)(g*17);
      for(int k=0;k<n;k++){ float x0=crk_x[i][k], y0=crk_y[i][k], x1=crk_x[i][k+1], y1=crk_y[i][k+1];
        if(pass==0){ gfx_thick_line(x0,y0,x1,y1,1.2f-k*0.05f,HEX(0x07040c)); }
        else { float a=glow*(1.0f-k/20.0f)*(0.75f+0.25f*sinf(t*9+i)); if(g_gpu) gfx_line((int)x0,(int)y0,(int)x1,(int)y1,col_scale(HEX(0xc070ff),a)); else gfx_line_a((int)x0,(int)y0,(int)x1,(int)y1,HEX(0xd090ff),(int)(255*a),true);
          if(k%4==1) fx_glow(x0,y0,10,HEX(0xa050ff),0.25f*a); } }
    }
    if(pass==1) emis_off(ly);
  }
}
static void valley(float t,const Valley *v){
  float day=v->day;
  /* ciel */
  grad3(VY0,250,col_lerp(HEX(0x060a1e),HEX(0x2a2c66),day),col_lerp(HEX(0x141a40),HEX(0xc0687a),day),col_lerp(HEX(0x3a2c5c),HEX(0xffc27a),day),0.62f);
  if(v->pillar>0) fx_glow(330-v->pan,200,260,HEX(0x8a3aff),0.22f*v->pillar);
  stars(31,170,VY0,210,t,1.0f-day*1.2f);
  if(day<0.5f) moon(505-v->pan*0.05f,84,15,1.0f-day*2);
  if(v->sun>0){ float sy=lerpf(250,168,v->sun), sx=470-v->pan*0.1f;
    fx_glow(sx,sy,190,HEX(0xffa050),0.30f); fx_glow(sx,sy,60,HEX(0xffe0a0),0.55f);
    int ly; emis_on(&ly); gfx_disc((int)sx,(int)sy,17,g_gpu?HEX(0xffd890):HEX(0xfff0c0)); emis_off(ly);
    gfx_disc((int)sx,(int)sy,17,HEX(0xfff2c8));
    for(int k=0;k<7;k++){ float a=-PI*0.5f+(k-3)*0.32f+sinf(t*0.3f+k)*0.04f; fx_shaft(sx,sy,sx+cosf(a)*360,sy+sinf(a)*360+260,22+k%3*10,HEX(0xffd8a0),0.05f+0.02f*(k&1)); } }
  /* montagnes */
  mountains(101,214,84,0.011f,v->pan*0.15f,col_lerp(HEX(0x1c2046),HEX(0x6a4a7a),day),col_lerp(HEX(0x2c3268),HEX(0xf0a08a),day),col_lerp(HEX(0x7a80b8),HEX(0xfff0dc),day),262,1);
  if(v->mist>0) for(int y=200;y<240;y++) gfx_rect_a(0,y,SCREEN_W,1,col_lerp(HEX(0x6a6a9a),HEX(0xffd8b8),day),(int)(v->mist*90*(1-fabsf(y-222)/22.0f)));
  mountains(202,232,48,0.02f,v->pan*0.35f,col_lerp(HEX(0x141936),HEX(0x4a3a5a),day),col_lerp(HEX(0x1e2650),HEX(0xc07a6a),day),0,262,1);
  /* champ et colline du village */
  field(v->pan,240,col_lerp(HEX(0x111c22),HEX(0x5a6a3a),day),col_lerp(HEX(0x070b0e),HEX(0x26341e),day),col_lerp(HEX(0x1c2c2a),HEX(0x7a8a4a),day),day);
  float hcx=150-v->pan;
  for(int x=0;x<SCREEN_W;x++){ float hy=hill_y((float)x,hcx,262); if(hy>256) continue; for(int y=(int)hy;y<262;y++) pxs(x,y,col_lerp(col_lerp(HEX(0x0e1420),HEX(0x4a4a34),day),col_lerp(HEX(0x0a0e16),HEX(0x34361e),day),(y-hy)/30.0f)); }
  village(hcx,262,v->lights,t,day);
  /* le gouffre */
  float pcx=330-v->pan, pcy=286;
  cracks_draw(v->crack,v->hole>0?0.6f:1.0f,t);
  uint32_t gl= v->gold>0 ? col_lerp(HEX(0x9a4aff),HEX(0xffc060),v->gold) : HEX(0x9a4aff);
  pit_hole(pcx,pcy,122,25,v->hole,col_lerp(col_lerp(HEX(0x3a3450),HEX(0x9a8a6a),day),HEX(0xffe0a0),v->gold*0.5f),gl,v->pillar>0?0.4f+0.5f*v->pillar:(v->hole>0?0.35f:0));
  if(v->pillar>0){ float a=v->pillar;
    fx_shaft(pcx,pcy,pcx,VY0-40,90*v->hole,gl,0.20f*a); fx_shaft(pcx,pcy,pcx,VY0-40,36*v->hole,col_lerp(gl,HEX(0xffffff),0.4f),0.35f*a);
    light_add(pcx,pcy-60,300,gl,0.7f*a); }
}
static void sh_valley(float t,float d){
  Valley v={0}; v.pan=30*smooth01(t/d); v.lights=1; v.mist=0.4f;
  light_add(505,84,200,HEX(0x8a90d0),0.15f);
  valley(t,&v);
  if(cn_prev<0) cp_clear();
  if(rfrand()<0.25f) cp_add(rfrange(20,620),rfrange(250,320),rfrange(-6,6),rfrange(-4,4),0,rfrange(2,4),1.2f,HEX(0xc8ff6a),PK_MOTE);
  cp_draw(0); cp_draw(1);
}
static void sh_rupture(float t,float d){
  (void)d;
  static const float BOOM[]={1.5f,2.44f,3.0f,4.12f,4.69f,5.06f};
  Valley v={0}; v.pan=30; v.mist=0.3f;
  if(cn_prev<0){ cp_clear(); cracks_build(330-30,286); }
  float collapse=5.25f;
  v.crack=t<1.5f?0:smooth01((t-1.5f)/3.7f);
  v.hole= t<collapse?0:ease_out((t-collapse)/1.1f);
  v.pillar= t<collapse?0:(t<collapse+0.3f?(t-collapse)/0.3f:1.0f-0.45f*smooth01((t-collapse-0.3f)/2.0f)+0.06f*sinf(t*7));
  v.lights= t<collapse+0.2f?1.0f:1.0f-0.8f*seg(t,collapse+0.2f,7.2f);
  /* secousses calees sur la musique */
  float sh=0; for(int i=0;i<(int)COUNTOF(BOOM);i++){ if(cue(t,BOOM[i])){ sfx_play(SFX_SLAM,0.55f+0.08f*i,0,0.8f); rumble(0.3f+0.05f*i,0.25f); } float k=t-BOOM[i]; if(k>=0 && k<0.6f) sh=maxf(sh,(1.2f+0.4f*i)*(1-k/0.6f)); }
  if(t>1.5f && t<collapse) sh=maxf(sh,0.6f*(t-1.5f)/3.7f);
  if(t>=collapse){ float k=t-collapse; sh=maxf(sh,5.0f*expf(-k*1.6f)); }
  cn_shake=sh;
  if(cue(t,collapse)){ sfx_play(SFX_EXPLODE,0.9f,0,0.6f); sfx_play(SFX_BOSS_ROAR,0.6f,0,0.7f); rumble(0.9f,0.6f); cn_flash=0.85f;
    for(int i=0;i<120;i++){ float a=rfrange(PI*1.05f,PI*1.95f); float sp=rfrange(40,190); cp_add(300+rfrange(-90,90),286+rfrange(-10,10),cosf(a)*sp*0.8f,sinf(a)*sp,260,rfrange(1.2f,2.6f),rfrange(1,3),i&1?HEX(0x3a3048):HEX(0x56506a),PK_DEBRIS); }
    for(int i=0;i<40;i++) cp_add(300+rfrange(-110,110),280+rfrange(-8,8),rfrange(-20,20),rfrange(-30,-8),0,rfrange(1.5f,3.0f),rfrange(4,9),HEX(0x4a4060),PK_SMOKE); }
  if(t>collapse && rfrand()<0.6f) cp_add(300+rfrange(-60,60),280,rfrange(-8,8),rfrange(-60,-25),0,rfrange(1.5f,3),rfrange(1,2),HEX(0xb070ff),PK_MOTE);
  if(t>5.6f && t<7.5f && rfrand()<0.3f){ float bx=rfrange(40,250); for(int k=0;k<3;k++) cp_add(bx+k*4,rfrange(190,230),rfrange(40,80),rfrange(-50,-30),0,2.5f,1,HEX(0x0a0a14),PK_LEAF); }
  valley(t,&v);
  cp_draw(0); cp_draw(1);
}
static void sh_eye(float t,float d){
  if(cn_prev<0) cp_clear();
  float z=smooth01(t/d);
  menu_tunnel(g_time*0.5f,4,(int)(190+40*z));
  fx_glow(320,201,280,HEX(0x5a1a9a),0.22f); fx_glow(320,201,60,HEX(0xa050ff),0.08f+0.05f*sinf(t*2));
  light_add(320,201,240,HEX(0x9a4aff),0.35f);
  float open = t<3.0f ? 0 : smooth01((t-3.0f)/1.4f);
  float lid = 1-open;
  float lx=0, ly=0.2f;
  if(t<6.0f){ lx=sinf(t*1.6f)*0.85f*open; ly=cosf(t*1.1f)*0.45f*open; } else { float k=smooth01((t-6.0f)/0.6f); lx=lerpf(sinf(6*1.6f)*0.85f,0,k); ly=lerpf(cosf(6*1.1f)*0.45f,0.12f,k); }
  if(cue(t,3.2f)){ sfx_play(SFX_BOSS_ROAR,0.8f,0,0.75f); rumble(0.6f,0.5f); }
  if(t>3.0f && t<4.6f) cn_shake=1.5f*(1-seg(t,3.0f,4.6f));
  g_boss_draw_scale=0.62f+0.3f*z;
  float fl = t>3.0f && t<3.6f ? (3.6f-t)*0.6f : 0;
  art_draw_boss(BOSS_OEIL,320,203,g_time,fl,lid,lx,ly,0);
  g_boss_draw_scale=1.0f;
  if(t>6.0f){ float p=0.5f+0.5f*sinf((t-6)*5); fx_glow(320+lx*11*0.6f,203+ly*7*0.6f,30,HEX(0xff3a6a),0.25f*p); }
  if(rfrand()<0.7f){ float a=rfrand()*TAU, r=rfrange(20,60); cp_add(320+cosf(a)*r,201+sinf(a)*r*0.7f,cosf(a)*rfrange(15,40),sinf(a)*rfrange(10,30)-8,0,rfrange(2,4),rfrange(0.8f,1.8f),HEX(0xb070ff),PK_MOTE); }
  cp_draw(1);
  if(g_gpu){ GP.cam_zoom=1.0f+0.18f*z; GP.cam_cx=320; GP.cam_cy=201; GP.vig=0.5f; GP.emis_gain=0.42f; GP.bloom_thr=0.92f; }
}
/* la horde remonte le puits : vue d'en bas, vers l'ouverture et le ciel */
static float wallL(float y){ return 238-(y-VY0)/(float)(VY1-VY0)*128+9*sinf(y*0.09f)+5*sinf(y*0.23f+1); }
static float wallR(float y){ return 402+(y-VY0)/(float)(VY1-VY0)*128+9*sinf(y*0.08f+2)+5*sinf(y*0.21f); }
static void sh_horde(float t,float d){
  (void)d;
  if(cn_prev<0) cp_clear();
  grad3(VY0,150,HEX(0x070a20),HEX(0x161a44),HEX(0x2a2050),0.6f);
  stars(77,80,VY0,140,t,1);
  moon(330,74,9,1);
  /* paroi du fond, eclairee par le bas */
  for(int x=0;x<SCREEN_W;x++){ int top=(int)(118+10*sinf(x*0.03f)+6*sinf(x*0.071f+2)); for(int y=top;y<VY1;y++){ float k=(float)(y-top)/(float)(VY1-top);
      uint32_t c=col_lerp(HEX(0x23203a),HEX(0x5a2a88),k*k); float n=vnoise(x*0.12f,y*0.05f,5); if(n>0.62f) c=col_scale(c,0.82f); else if(n<0.3f) c=col_scale(c,1.1f); pxs(x,y,c); } }
  /* grimpeurs */
  static const int SP[8]={SP_SQUELETTE,SP_GELEE,SP_ARAIGNEE,SP_DIABLOTIN,SP_CHAMPIGNON,SP_CRANE,SP_SCARABEE,SP_SPECTRE};
  for(int i=0;i<34;i++){
    float sp=18+h01(300u+(uint32_t)i)*26, ph=h01(400u+(uint32_t)i);
    float y=VY1+20-fmodf(t*sp+ph*260,260); float x=wallL(y)+20+h01(500u+(uint32_t)i)*(wallR(y)-wallL(y)-40);
    float sc=1.4f+(y-VY0)/(float)(VY1-VY0)*1.4f; float a=clampf((y-120)/40.0f,0,1);
    if(a<=0) continue;
    mon_draw(SP[i&7],x+sinf(t*3+i)*2,y,sc,(i&1),HEX(0xc8b0ff),(int)(255*a),((int)(t*6+i))&1?1:2);
  }
  /* parois laterales */
  for(int y=VY0;y<VY1;y++){ float l=wallL((float)y), r=wallR((float)y);
    for(int x=0;x<(int)l;x++){ float n=vnoise(x*0.09f,y*0.09f,9); uint32_t c=col_lerp(HEX(0x0c0a14),HEX(0x1e1a2c),n); if(x>l-3) c=col_lerp(HEX(0x6a3aa8),HEX(0x2a1a40),(l-x)/3.0f*((float)(VY1-y)/(VY1-VY0))); pxs(x,y,c); }
    for(int x=(int)r;x<SCREEN_W;x++){ float n=vnoise(x*0.09f,y*0.09f,11); uint32_t c=col_lerp(HEX(0x0c0a14),HEX(0x1e1a2c),n); if(x<r+3) c=col_lerp(HEX(0x6a3aa8),HEX(0x2a1a40),(x-r)/3.0f*((float)(VY1-y)/(VY1-VY0))); pxs(x,y,c); } }
  /* chauves-souris qui s'envolent par l'ouverture */
  for(int i=0;i<9;i++){ float ph=fmodf(t*0.22f+h01(600u+(uint32_t)i),1.0f); float y=VY1-ph*(VY1-40); float x=320+sinf(ph*7+i*2)*(60+i*9)*(1-ph*0.6f);
    float sc=(2.6f-ph*1.8f)/EN_SCALE; int fr=((int)(t*10+i))&1; DrawOpt o=dopt(); o.sx=o.sy=sc; o.tint=HEX(0xb8a8e8);
    draw_spr_c(&SPR[fr?SP_CHAUVE1:SP_CHAUVE2],x,y,&o); }
  light_add(320,VY1+30,330,HEX(0x9a4aff),0.9f);
  fx_glow(320,VY1+10,230,HEX(0x7a2ad0),0.32f);
  if(rfrand()<0.5f) cp_add(rfrange(200,440),VY1,rfrange(-10,10),rfrange(-60,-30),0,rfrange(2,4),rfrange(0.8f,1.6f),HEX(0xb070ff),PK_MOTE);
  cp_draw(1);
  if(cue(t,0.5f)) sfx_play(SFX_GHOST,0.45f,0,0.7f);
  if(cue(t,4.5f)) sfx_play(SFX_GHOST,0.4f,0.3f,0.6f);
}
/* les gardiens de la vallee au bord du Gouffre : contre-plongee depuis le puits */
static void sh_heroes(float t,float d){
  (void)d;
  if(cn_prev<0) cp_clear();
  grad3(VY0,262,HEX(0x070a20),HEX(0x18204a),HEX(0x3a2a5c),0.65f);
  stars(55,120,VY0,230,t,1);
  moon(500,96,20,1);
  /* nuages pousses par le vent */
  for(int i=0;i<6;i++){ float x=fmodf(h01(700u+(uint32_t)i)*900+t*(26+i*5),900)-130, y=70+h01(710u+(uint32_t)i)*120;
    for(int k=0;k<5;k++) gfx_ellipse_a((int)(x+k*14-28),(int)(y+sinf(k*1.7f+i)*3),18-abs(k-2)*3,5,HEX(0x2a2c52),70); }
  /* le bord */
  for(int x=0;x<SCREEN_W;x++){ int top=(int)(258+4*sinf(x*0.05f)+3*sinf(x*0.13f+1)+(fabsf(x-320)<40?4:0)); for(int y=top;y<VY1;y++){ float k=(float)(y-top)/(float)(VY1-top);
      uint32_t c=col_lerp(HEX(0x141020),HEX(0x4a1e78),k*k); if(y==top) c=HEX(0x3a3460); float n=vnoise(x*0.15f,y*0.1f,3); if(n>0.64f) c=col_scale(c,0.8f); pxs(x,y,c); } }
  fx_glow(320,VY1+20,300,HEX(0x8a2ae0),0.30f);
  light_add(320,VY1+40,360,HEX(0xa050ff),0.8f); light_add(500,96,260,HEX(0xa0a8ff),0.3f);
  /* les huit heros apparaissent du centre vers les bords */
  static const int ORD[8]={3,4,2,5,1,6,0,7};
  static const int CH[8]={CH_ALCHIMISTE,CH_FORGERONNE,CH_MAGNETISEUR,CH_SENTINELLE,CH_NECRO,CH_JONGLEUR,CH_HORLOGERE,CH_PYROMANE};
  for(int k=0;k<8;k++){ int i=ORD[k]; int ch=CH[i];
    float ap=seg(t,0.5f+k*0.38f,0.9f+k*0.38f); if(ap<=0) continue;
    if(cue(t,0.5f+k*0.38f)) sfx_play(SFX_SELECT,0.35f,(i-3.5f)/4.0f,0.8f+k*0.05f);
    bool lead= ch==cn_hero; float sc=4.0f; float x=52+i*76.6f; float feet=(lead?268:261)+sinf(x)*1.5f;
    float bob=sinf(t*2+i)*0.6f;
    hero_draw(ch,x,feet+bob*0.2f,sc,0,i>=4,0xFFFFFFFFu,(int)(255*ap));
    float bx=x+(i>=4?-1:1)*sc*5.5f, by=feet-sc*8+sinf(t*3+i)*2;
    fx_ball(bx,by,2.2f+0.4f*sc/3,CHARS[ch].start_ball,1); fx_glow(bx,by,10,BALLS[CHARS[ch].start_ball].col,0.25f*ap);
    light_add(bx,by,26,BALLS[CHARS[ch].start_ball].col,0.35f*ap);
  }
  if(rfrand()<0.5f) cp_add(-5,rfrange(60,250),rfrange(70,120),rfrange(-10,10),0,rfrange(5,7),1,HEX(0x8a7ab8),PK_LEAF);
  cp_draw(0);
}
/* les billes en action : un instant de la descente, comme dans le jeu */
typedef struct { float x,y,vx,vy; int kind; bool on; float trail_x[8], trail_y[8]; } CBall;
typedef struct { float x,y; int sp, hp; float flash, dead; } CMon;
#define NCB 10
#define NCM 18
static CBall cbl[NCB]; static CMon cmn[NCM]; static float cb_next=0; static int cb_count=0;
typedef struct { float x,y,t; int v; uint32_t c; } CNum;
static CNum cnum[24]; static int cnn=0;
static void sh_balls(float t,float d){
  (void)d;
  static const int SP[6]={SP_SQUELETTE,SP_GELEE,SP_CHAUVE1,SP_ARAIGNEE,SP_DIABLOTIN,SP_CRANE};
  static const int KB[6]={BK_FEU,BK_GLACE,BK_FOUDRE,BK_VENIN,BK_FER,BK_LUMIERE};
  if(cn_prev<0){ cp_clear(); memset(cbl,0,sizeof cbl); cb_next=0.2f; cb_count=0; cnn=0;
    for(int i=0;i<NCM;i++){ cmn[i].x=196+(i%6)*50.0f; cmn[i].y=92+(i/6)*36.0f; cmn[i].sp=SP[(i*5+i/6)%6]; cmn[i].hp=2+(i%3==0); cmn[i].flash=0; cmn[i].dead=0; } }
  float dt=cn_dt;
  /* sol et parois */
  grad3(VY0,VY1,HEX(0x2a2438),HEX(0x3a3048),HEX(0x241e30),0.5f);
  for(int y=VY0;y<VY1;y+=22) for(int x=130+((y/22)&1)*16;x<510;x+=32){ gfx_frame(x,y,32,22,HEX(0x1e1a28)); gfx_hline(x+1,x+30,y+1,HEX(0x4a4258)); }
  for(int y=VY0;y<VY1;y++){ float l=128+5*sinf(y*0.11f)+3*sinf(y*0.31f), r=512+5*sinf(y*0.1f+1)+3*sinf(y*0.27f);
    for(int x=0;x<(int)l;x++) pxs(x,y,col_lerp(HEX(0x100c18),HEX(0x2a2238),vnoise(x*0.1f,y*0.1f,21)*(x/l)));
    for(int x=(int)r;x<SCREEN_W;x++) pxs(x,y,col_lerp(HEX(0x100c18),HEX(0x2a2238),vnoise(x*0.1f,y*0.1f,23)*((SCREEN_W-x)/(SCREEN_W-r))));
    pxs((int)l,y,HEX(0x5a4a78)); pxs((int)r,y,HEX(0x5a4a78)); }
  /* simulation */
  for(int step=0;step<4;step++){ float h=dt/4;
    for(int i=0;i<NCB;i++){ CBall *b=&cbl[i]; if(!b->on) continue;
      b->x+=b->vx*h; b->y+=b->vy*h;
      if(b->x<136){ b->x=136; b->vx=fabsf(b->vx); sfx_play(SFX_BOUNCE,0.3f,-0.6f,1.0f); }
      if(b->x>504){ b->x=504; b->vx=-fabsf(b->vx); sfx_play(SFX_BOUNCE,0.3f,0.6f,1.0f); }
      if(b->y<VY0+6){ b->y=VY0+6; b->vy=fabsf(b->vy); }
      if(b->y>VY1+10) b->on=false;
      for(int m=0;m<NCM;m++){ CMon *e=&cmn[m]; if(e->dead>0) continue; float dx=b->x-e->x, dy=b->y-e->y; if(dx*dx+dy*dy<15*15){
          float l=sqrtf(dx*dx+dy*dy)+0.01f; float nx=dx/l, ny=dy/l; float dot=b->vx*nx+b->vy*ny; if(dot<0){ b->vx-=2*dot*nx; b->vy-=2*dot*ny; }
          b->x=e->x+nx*15; b->y=e->y+ny*15;
          e->hp--; e->flash=0.12f; int dmg=8+(int)(rfrand()*9);
          if(cnn<24){ cnum[cnn].x=e->x; cnum[cnn].y=e->y-12; cnum[cnn].t=0; cnum[cnn].v=dmg; cnum[cnn].c=BALLS[b->kind].col; cnn++; }
          sfx_play(SFX_HIT,0.4f,(e->x-320)/320,1.0f);
          if(e->hp<=0){ e->dead=0.01f; sfx_play(SFX_KILL,0.5f,(e->x-320)/320,1.0f);
            for(int k=0;k<18;k++){ float a=rfrand()*TAU, s=rfrange(40,140); cp_add(e->x,e->y,cosf(a)*s,sinf(a)*s,160,rfrange(0.4f,0.9f),rfrange(1,2.5f),k&1?BALLS[b->kind].col:HEX(0xfff0d0),PK_SPARK); }
            for(int k=0;k<8;k++){ float a=rfrand()*TAU, s=rfrange(20,70); cp_add(e->x,e->y,cosf(a)*s,sinf(a)*s-30,200,rfrange(0.5f,1.0f),2,HEX(0x5a5070),PK_DEBRIS); } }
          break; } }
    }
  }
  cb_next-=dt;
  if(cb_next<=0 && t<7.4f){ for(int i=0;i<NCB;i++) if(!cbl[i].on){ CBall *b=&cbl[i]; b->on=true; b->x=320; b->y=VY1-34; float a=-PI*0.5f+sinf(cb_count*2.3f)*0.75f; b->vx=cosf(a)*300; b->vy=sinf(a)*300; b->kind=KB[cb_count%6]; for(int k=0;k<8;k++){ b->trail_x[k]=b->x; b->trail_y[k]=b->y; } cb_count++; sfx_play(SFX_SHOOT,0.4f,0,1.0f); break; }
    cb_next=0.32f; }
  for(int i=0;i<NCM;i++){ CMon *e=&cmn[i]; e->y+=6*dt; if(e->flash>0) e->flash-=dt; if(e->dead>0) e->dead+=dt; }
  /* monstres sur leurs socles */
  for(int i=0;i<NCM;i++){ CMon *e=&cmn[i]; if(e->dead>0.0f) continue;
    gfx_ellipse_a((int)e->x,(int)e->y+12,11,3,HEX(0x000000),90);
    gfx_rect((int)e->x-11,(int)e->y+6,22,6,HEX(0x3a3448)); gfx_hline((int)e->x-11,(int)e->x+10,(int)e->y+6,HEX(0x6a6080)); gfx_rect((int)e->x-11,(int)e->y+12,22,2,HEX(0x1a1624));
    DrawOpt o=dopt(); o.sx=o.sy=1.0f; if(e->flash>0){ o.flash=HEX(0xffffff); o.flash_a=220; }
    const Sprite *s=&SPR[e->sp]; if(e->sp==SP_CHAUVE1 && ((int)(t*8+i)&1)) s=&SPR[SP_CHAUVE2];
    draw_spr_c(s,e->x,e->y-2,&o); }
  /* le heros */
  hero_draw(cn_hero,320,VY1-12,2.0f,0,0,0xFFFFFFFFu,255);
  light_add(320,VY1-30,90,HEX(0xffe0b0),0.4f);
  /* billes et traines */
  for(int i=0;i<NCB;i++){ CBall *b=&cbl[i]; if(!b->on) continue;
    for(int k=7;k>0;k--){ b->trail_x[k]=b->trail_x[k-1]; b->trail_y[k]=b->trail_y[k-1]; } b->trail_x[0]=b->x; b->trail_y[0]=b->y;
    int ly; emis_on(&ly);
    for(int k=0;k<7;k++){ float f=1-k/7.0f; fx_caps(b->trail_x[k],b->trail_y[k],b->trail_x[k+1],b->trail_y[k+1],2.4f*f+0.3f,BALLS[b->kind].col,0.7f*f); }
    emis_off(ly);
    fx_ball(b->x,b->y,4,b->kind,1); light_add(b->x,b->y,40,BALLS[b->kind].col,0.6f); }
  cp_draw(0); cp_draw(1);
  /* chiffres de degats */
  for(int i=0;i<cnn;){ CNum *n=&cnum[i]; n->t+=dt; if(n->t>0.8f){ cnum[i]=cnum[--cnn]; continue; }
    char b[8]; snprintf(b,sizeof b,"%d",n->v); int ly=layer_current(); if(g_gpu) layer(LY_UI);
    text_big_digits((int)n->x,(int)(n->y-n->t*26),b,n->t<0.08f?HEX(0xffffff):col_lerp(n->c,HEX(0xffffff),0.35f),1,AL_CENTER); layer(ly); i++; }
  fx_glow(320,VY0,240,HEX(0x6a2ad0),0.12f);
}
/* les quatre gardiens du Gouffre, presentes comme des cartes */
static void card_frame(int x,int y,int w,int h,uint32_t a,uint32_t b,uint32_t acc){
  gfx_rect(x-3,y-3,w+6,h+6,HEX(0x0a0810));
  int ox0=gT->cx0, oy0=gT->cy0, ox1=gT->cx1, oy1=gT->cy1; gfx_clip(x,y,x+w,y+h);
  grad3(y,y+h,col_scale(a,1.25f),a,b,0.55f);
  gfx_clip(ox0,oy0,ox1,oy1);
  gfx_frame(x-2,y-2,w+4,h+4,HEX(0xc9a24a)); gfx_frame(x-1,y-1,w+2,h+2,HEX(0x5a4020)); gfx_frame(x,y,w,h,col_scale(acc,0.6f));
  for(int k=0;k<4;k++){ int cx=k&1?x+w-1:x, cy=k&2?y+h-1:y; gfx_rect(cx-3,cy-3,6,6,HEX(0xe8c060)); gfx_rect(cx-2,cy-2,4,4,HEX(0x8a5a20)); }
}
static void sh_guardians(float t,float d){
  (void)d;
  if(cn_prev<0) cp_clear();
  grad3(VY0,VY1,HEX(0x0c0816),HEX(0x1c1028),HEX(0x0a0610),0.5f);
  for(int i=0;i<40;i++){ float ph=fmodf(t*0.12f+h01(800u+(uint32_t)i),1.0f); float x=h01(810u+(uint32_t)i)*SCREEN_W+sinf(t+i)*6, y=VY1-ph*(VY1-VY0); fx_disc(x,y,0.7f,i&1?HEX(0xff8a3a):HEX(0xb070ff),0.9f*sinf(ph*PI)); }
  static const int BOSS[4]={BOSS_ROI,BOSS_MERE,BOSS_COLOSSE,BOSS_OEIL};
  for(int i=0;i<4;i++){
    float t0=0.2f+i*3.0f; if(t<t0) continue;
    float k=seg(t,t0,t0+0.35f); float drop=(1-ease_out(k))*-220;
    int w=136, h=222, x=24+i*150, y=(int)(48+drop);
    const BiomeDef *bd=&BIOMES[i];
    card_frame(x,y,w,h,col_lerp(bd->floor_c,bd->accent,0.15f),col_scale(bd->floor_b,0.7f),bd->accent);
    if(cue(t,t0+0.3f)){ sfx_play(SFX_SLAM,0.7f,(i-1.5f)/2,0.9f); sfx_play(SFX_BOSS_ROAR,0.45f,(i-1.5f)/2,1.0f+0.1f*i); cn_shake=2.5f; rumble(0.5f,0.25f);
      for(int q=0;q<26;q++){ float a=rfrand()*TAU, s=rfrange(40,120); cp_add(x+w*0.5f,(float)y+h-10,cosf(a)*s,sinf(a)*s*0.5f-40,120,rfrange(0.5f,1.1f),1.5f,bd->accent,PK_SPARK); } }
    int ox0=gT->cx0, oy0=gT->cy0, ox1=gT->cx1, oy1=gT->cy1; gfx_clip(x,y,x+w,y+h);
    fx_glow(x+w*0.5f,y+92.0f,70,bd->accent,0.22f);
    light_add(x+w*0.5f,y+60.0f,120,bd->accent,0.5f);
    g_boss_draw_scale= BOSS[i]==BOSS_MERE?0.95f:1.0f;
    float an= BOSS[i]==BOSS_OEIL ? (t<t0+0.6f?1.0f:1.0f-smooth01((t-t0-0.6f)/0.8f)) : 0.5f+0.5f*sinf(t*2+i);
    art_draw_boss(BOSS[i],x+w*0.5f,y+94.0f,g_time+i,k<1?(1-k):0,an,sinf(t+i)*0.5f,0.2f,0);
    g_boss_draw_scale=1.0f;
    gfx_clip(ox0,oy0,ox1,oy1);
    float fl=1-seg(t,t0+0.3f,t0+0.8f); if(fl>0 && k>=1) gfx_rect_add(x,y,w,h,HEX(0xffffff),(int)(fl*160));
    int ly=layer_current(); if(g_gpu) layer(LY_UI);
    gfx_rect_a(x+4,y+h-46,w-8,40,HEX(0x0a0810),170);
    text_draw(x+w/2,y+h-40,BOSSES[BOSS[i]].name,HEX(0xffd84a),1,AL_CENTER,TX_OUTLINE);
    text_draw(x+w/2,y+h-24,bd->name,HEX(0xb0a8c0),1,AL_CENTER,TX_SHADOW);
    char nb[16]; snprintf(nb,sizeof nb,"Niveau %d",i+1); text_draw(x+w/2,y+h-13,nb,HEX(0x7a7290),1,AL_CENTER,0);
    layer(ly);
  }
  cp_draw(1);
}
/* la chute dans le Gouffre, puis le titre */
static void sh_descent(float t,float d){
  if(cn_prev<0) cp_clear();
  float fall=6.0f;
  if(t<fall){
    float k=t/fall; float sp=20+k*k*160;
    menu_tunnel(g_time*0.8f,sp,(int)(70+60*k));
    fx_glow(320,201,200,HEX(0x8a3aff),0.12f+0.16f*k);
    light_add(320,201,260,HEX(0x9a5aff),0.5f);
    /* traits de vitesse */
    int ly; emis_on(&ly);
    for(int i=0;i<40;i++){ float a=h01(900u+(uint32_t)i)*TAU; float ph=fmodf(t*(1.2f+k*2.5f)+h01(910u+(uint32_t)i),1.0f); float r0=20+ph*ph*420, r1=r0+10+k*40;
      fx_caps(320+cosf(a)*r0,201+sinf(a)*r0*0.75f,320+cosf(a)*r1,201+sinf(a)*r1*0.75f,0.6f,HEX(0xe0d0ff),0.5f*ph); }
    emis_off(ly);
    /* le heros tombe, de plus en plus loin */
    float sc=4.0f-2.6f*k; float hx=320+sinf(t*2.2f)*14*(1-k), hy=236-60*k+sinf(t*5)*3;
    hero_draw(cn_hero,hx,hy,sc,((int)(t*8))&3,((int)(t*1.5f))&1,0xFFFFFFFFu,255);
    light_add(hx,hy-10,60,HEX(0xffe0c0),0.4f);
    if(cue(t,0.3f)) sfx_play(SFX_WIND,0.8f,0,0.8f);
    if(cue(t,2.4f)) sfx_play(SFX_WIND,0.9f,0,1.0f);
    if(cue(t,4.4f)) sfx_play(SFX_WIND,1.0f,0,1.2f);
    cn_shake=0.6f*k;
    if(g_gpu){ GP.cam_zoom=1.0f+0.1f*k; GP.cam_cx=320; GP.cam_cy=201; GP.emis_gain=0.6f; }
  } else {
    float k=t-fall;
    if(cue(t,fall)){ cn_flash=1.0f; sfx_play(SFX_REVIVE,0.7f,0,0.8f); rumble(0.5f,0.3f); }
    menu_tunnel(g_time*0.3f,10,60);
    gfx_darken(0,0,SCREEN_W,SCREEN_H,110);
    fx_glow(320,201,240,HEX(0x7a2ae0),0.28f);
    fx_glow(SCREEN_W/2,110,220,HEX(0xff7a2a),0.12f*smooth01(k/1.0f));
    int ly=layer_current();
    if(g_gpu){ layer(LY_EMIS); draw_title_logo(SCREEN_W/2,92,g_time); layer(ly); }
    draw_title_logo(SCREEN_W/2,92,g_time);
    if(g_gpu) layer(LY_UI);
    float ta=seg(k,1.0f,2.0f);
    if(ta>0) text_draw(SCREEN_W/2,152,"Des billes. Des monstres. Un gouffre sans fond.",col_lerp(HEX(0x000000),HEX(0xd8c8b0),ta),1,AL_CENTER,TX_SHADOW);
    layer(ly);
    if(rfrand()<0.6f) cp_add(rfrange(200,440),VY1,rfrange(-8,8),rfrange(-50,-20),0,rfrange(2,4),1.2f,rfrand()<0.5f?HEX(0xff8a3a):HEX(0xffd36a),PK_MOTE);
  }
  cp_draw(1);
  (void)d;
}

/* =====================================================================
   FINS
   ===================================================================== */
#define NSH 16
static float shx[NSH][8], shy[NSH][8];
static void shards_build(uint32_t seed,float sc){
  cs_seed(seed);
  for(int i=0;i<NSH;i++){ float a=i*TAU/NSH+cs_rg(-0.2f,0.2f), x=0, y=0; for(int k=0;k<8;k++){ shx[i][k]=x; shy[i][k]=y; a+=cs_rg(-0.5f,0.5f); x+=cosf(a)*sc*cs_rg(4,7); y+=sinf(a)*sc*cs_rg(4,7)*0.8f; } }
}
/* le gardien se fissure de lumiere puis vole en eclats */
static void shatter(float t,int boss,uint32_t bg_a,uint32_t bg_b,uint32_t light,float cy){
  if(cn_prev<0){ cp_clear(); shards_build(boss*31+7,2.0f); }
  grad3(VY0,VY1,bg_a,bg_b,bg_a,0.5f);
  if(boss==BOSS_NEANT) stars(1234,200,VY0,VY1,t,1);
  float boom=6.0f;
  float crack=seg(t,0.8f,5.6f);
  if(t<boom){
    float lid= boss==BOSS_OEIL ? smooth01((t-3.0f)/2.0f) : 0;
    float sc=2.0f; if(boss==BOSS_NEANT) sc=2.0f*(1.0f-0.85f*smooth01((t-4.6f)/1.4f));
    float sh=0.4f+2.6f*crack; cn_shake=sh;
    g_boss_draw_scale=sc;
    art_draw_boss(boss,320+sinf(t*37)*crack*1.5f,cy,g_time,crack>0.8f?(crack-0.8f)*2.5f:0,lid,sinf(t*3)*0.6f*(1-lid),0.1f,1);
    g_boss_draw_scale=1.0f;
    /* fissures lumineuses */
    int ly; emis_on(&ly);
    for(int i=0;i<NSH;i++){ int n=(int)(crack*7.99f*(0.6f+0.4f*h01((uint32_t)i))); for(int k=0;k<n;k++){
        float x0=320+shx[i][k]*sc/2, y0=cy+shy[i][k]*sc/2, x1=320+shx[i][k+1]*sc/2, y1=cy+shy[i][k+1]*sc/2;
        if(g_gpu) gfx_line((int)x0,(int)y0,(int)x1,(int)y1,col_scale(light,1.2f)); else gfx_line_a((int)x0,(int)y0,(int)x1,(int)y1,light,230,true);
        if(k==n-1) fx_glow(x1,y1,8,light,0.4f); } }
    emis_off(ly);
    fx_glow(320,cy,80+crack*60,light,0.1f+0.35f*crack);
    light_add(320,cy,200,light,0.4f+0.8f*crack);
    for(int k=0;k<3;k++) if(cue(t,1.5f+k*1.4f)){ sfx_play(SFX_SHIELD,0.6f,0,0.7f+k*0.1f); rumble(0.3f,0.2f); }
    if(boss==BOSS_NEANT && t>4.6f && rfrand()<0.9f){ float a=rfrand()*TAU, r=rfrange(80,200); cp_add(320+cosf(a)*r,cy+sinf(a)*r,-cosf(a)*r*1.4f,-sinf(a)*r*1.4f,0,0.7f,1.2f,HEX(0xd8e0ff),PK_SPARK); }
  } else {
    float k=t-boom;
    if(cue(t,boom)){ cn_flash=1.0f; sfx_play(SFX_BOSS_DIE,1.0f,0,0.8f); sfx_play(SFX_EXPLODE,0.8f,0,0.5f); rumble(1.0f,0.7f);
      for(int i=0;i<220;i++){ float a=rfrand()*TAU, s=rfrange(60,330); cp_add(320,cy,cosf(a)*s,sinf(a)*s,40,rfrange(0.8f,2.6f),rfrange(1,3),i%3?light:HEX(0xffffff),PK_SPARK); }
      for(int i=0;i<90;i++){ float a=rfrand()*TAU, s=rfrange(40,200); cp_add(320+cosf(a)*20,cy+sinf(a)*20,cosf(a)*s,sinf(a)*s-40,180,rfrange(1.0f,2.4f),rfrange(2,4),boss==BOSS_NEANT?HEX(0x1a1830):HEX(0x5a2a7a),PK_DEBRIS); } }
    cn_shake=4.0f*expf(-k*2);
    for(int r=0;r<3;r++){ float rr=k*(260+r*90); float a=maxf(0,1-k*0.7f-r*0.15f); fx_ring(320,cy,rr,3+r*2,r?light:HEX(0xffffff),0.9f*a); }
    int ly; emis_on(&ly);
    for(int i=0;i<12;i++){ float a=i*TAU/12+k*0.2f; float al=maxf(0,0.55f-k*0.18f); fx_shaft(320,cy,320+cosf(a)*500,cy+sinf(a)*500,30+(i&1)*30,light,al*0.35f); }
    emis_off(ly);
    fx_glow(320,cy,160,light,maxf(0,0.8f-k*0.3f));
    light_add(320,cy,300,light,maxf(0,1.4f-k*0.5f));
  }
  if(rfrand()<0.4f) cp_add(rfrange(0,640),rfrange(VY0,VY1),rfrange(-5,5),rfrange(-15,-5),0,rfrange(2,4),1,light,PK_MOTE);
  cp_draw(0); cp_draw(1);
}
static void sh_shatter_eye(float t,float d){ (void)d; shatter(t,BOSS_OEIL,HEX(0x0c0614),HEX(0x2a1240),HEX(0xffe0a0),190); }
static void sh_shatter_void(float t,float d){ (void)d; shatter(t,BOSS_NEANT,HEX(0x04030a),HEX(0x141030),HEX(0xd8e8ff),190); }
static void sh_shatter_sun(float t,float d){ (void)d; shatter(t,BOSS_SOLEIL,HEX(0x120804),HEX(0x3a1e08),HEX(0xffd080),190); }
/* le silence : les creatures s'eteignent en lumiere doree */
static void sh_silence(float t,float d){
  if(cn_prev<0) cp_clear();
  menu_tunnel(g_time*0.25f,3,90);
  float k=smooth01(t/d);
  gfx_rect_add(0,0,SCREEN_W,SCREEN_H,HEX(0x402a10),(int)(40+60*k));
  fx_glow(320,201,300,HEX(0xffb050),0.12f+0.2f*k); fx_glow(320,201,70,HEX(0xffe0a0),0.3f+0.3f*k);
  light_add(320,201,300,HEX(0xffc070),0.6f+0.6f*k);
  static const int SP0[10]={SP_SQUELETTE,SP_GELEE,SP_SPECTRE,SP_CHAMPIGNON,SP_DIABLOTIN,SP_ARAIGNEE,SP_CRANE,SP_GOLEM,SP_CHAUVE1,SP_HORREUR};
  static const int SP3[10]={SP_LUCIOLE,SP_FLEUR,SP_CRAPAUD,SP_MOMIE,SP_GOLEM_CRISTAL,SP_PAPILLON1,SP_PRETRE,SP_SYLVAIN,SP_PHENIX1,SP_SPHINX};
  const int *SP= cn_id==3 ? SP3 : SP0;
  for(int i=0;i<10;i++){
    float a=i*TAU/10+0.3f, r=70+(i%3)*45; float x=320+cosf(a)*r, y=201+sinf(a)*r*0.72f;
    float t0=0.6f+i*0.62f; float f=1-seg(t,t0,t0+1.2f);
    if(f>0){ DrawOpt o=dopt(); o.sx=o.sy=(1.4f+(i%3)*0.5f)/EN_SCALE; o.alpha=(int)(255*f); o.flash=HEX(0xffe0a0); o.flash_a=(int)(255*(1-f)); draw_spr_c(&SPR[SP[i]],x,y+sinf(t+i)*3,&o); }
    if(t>t0 && t<t0+1.2f && rfrand()<0.8f) for(int q=0;q<2;q++) cp_add(x+rfrange(-10,10),y+rfrange(-10,10),rfrange(-10,10),rfrange(-40,-15),0,rfrange(1.5f,3.0f),rfrange(1,2),rfrand()<0.5f?HEX(0xffd890):HEX(0xffffff),PK_MOTE);
    if(cue(t,t0)) sfx_play(SFX_GHOST,0.25f,(x-320)/320,1.2f+0.05f*i);
  }
  if(rfrand()<0.5f){ float a=rfrand()*TAU; cp_add(320+cosf(a)*20,201+sinf(a)*14,cosf(a)*rfrange(20,60),sinf(a)*rfrange(15,45),0,rfrange(2,4),1.2f,HEX(0xffe0a0),PK_MOTE); }
  cp_draw(1);
  if(g_gpu){ GP.cam_zoom=1.0f+0.05f*k; GP.cam_cx=320; GP.cam_cy=201; }
}
/* les Profondeurs s'illuminent une a une */
static void sh_montage(float t,float d){
  (void)d;
  if(cn_prev<0) cp_clear();
  grad3(VY0,VY1,HEX(0x08060e),HEX(0x141022),HEX(0x08060e),0.5f);
  for(int i=0;i<4;i++){
    int w=136, h=230, x=24+i*150, y=48; int bi=4+i; const BiomeDef *bd=&BIOMES[bi];
    float t0=0.3f+i*2.2f; float lit=seg(t,t0,t0+0.8f);
    card_frame(x,y,w,h,col_lerp(col_scale(bd->floor_c,0.45f),col_lerp(bd->floor_c,HEX(0xffe0a0),0.25f),lit),col_lerp(col_scale(bd->floor_b,0.35f),bd->floor_b,lit),bd->accent);
    int ox0=gT->cx0, oy0=gT->cy0, ox1=gT->cx1, oy1=gT->cy1; gfx_clip(x,y,x+w,y+h);
    float cx=x+w*0.5f;
    switch(i){
      case 0: /* le givre fond */
        for(int k=0;k<9;k++){ float px=x+8+k*15.0f, hh=(30+h01(1000u+(uint32_t)k)*50)*(1-0.6f*lit); gfx_tri(px-6,(float)y,px+6,(float)y,px,(float)y+hh,col_lerp(HEX(0x9ad8ff),HEX(0xd8f4ff),lit));
          if(lit>0.2f){ float ph=fmodf(t*0.9f+k*0.3f,1.0f); gfx_disc((int)px,(int)(y+hh+ph*120),1,HEX(0xbfe8ff)); } }
        for(int k=0;k<5;k++){ float px=x+14+k*27.0f; gfx_tri(px-9,(float)y+h,px+9,(float)y+h,px,(float)y+h-40*(1-0.7f*lit),HEX(0xb8e4ff)); }
        break;
      case 1: /* la cite engloutie */
        for(int k=0;k<4;k++){ int cx2=x+18+k*32; gfx_rect(cx2,y+90,12,120,col_lerp(HEX(0x1a3a3a),HEX(0x5a8a80),lit)); gfx_rect(cx2-2,y+88,16,4,col_lerp(HEX(0x24504a),HEX(0x8ab8a8),lit)); }
        for(int k=0;k<10;k++){ float ph=fmodf(t*0.3f+h01(1100u+(uint32_t)k),1.0f); gfx_ring((int)(x+10+h01(1110u+(uint32_t)k)*(w-20)),(int)(y+h-ph*h),1+(k&1),col_lerp(HEX(0x3a8a8a),HEX(0xc8fff0),lit)); }
        break;
      case 2: /* la ruche s'endort */
        for(int r=0;r<9;r++) for(int c=0;c<6;c++){ float hx=x+12+c*22+(r&1)*11, hy=y+18+r*22; gfx_disc((int)hx,(int)hy,9,col_lerp(HEX(0x3a2208),HEX(0x8a5a1a),lit)); gfx_disc((int)hx,(int)hy,7,col_lerp(HEX(0x5a3a10),HEX(0xe8b048),lit*(0.6f+0.4f*h01((uint32_t)(r*7+c))))); }
        break;
      default: /* le neant se dissipe */
        stars(1300,90,y,y+h,t,0.3f+0.7f*lit);
        for(int k=0;k<5;k++){ float a=(1-lit)*0.9f; gfx_disc_a((int)(cx+sinf(k*2.1f)*40),(int)(y+50+k*38),(int)(22-k*2),HEX(0x000000),(int)(200*a)); }
        break;
    }
    if(lit>0){ fx_glow(cx,y+h*0.5f,90,HEX(0xffd890),0.22f*lit); light_add(cx,y+h*0.5f,150,HEX(0xffd890),0.6f*lit); }
    gfx_clip(ox0,oy0,ox1,oy1);
    if(cue(t,t0)){ sfx_play(SFX_CRYSTAL,0.6f,(i-1.5f)/2,0.8f+0.12f*i); for(int q=0;q<30;q++){ float a=rfrand()*TAU, s=rfrange(30,100); cp_add(cx,y+h*0.5f,cosf(a)*s,sinf(a)*s,30,rfrange(0.6f,1.4f),1.4f,HEX(0xffe0a0),PK_SPARK); } }
    int ly=layer_current(); if(g_gpu) layer(LY_UI);
    gfx_rect_a(x+4,y+h-26,w-8,22,HEX(0x0a0810),170);
    text_draw(x+w/2,y+h-20,bd->name,lit>0.5f?HEX(0xffd84a):HEX(0x8a82a0),1,AL_CENTER,TX_SHADOW);
    layer(ly);
  }
  cp_draw(1);
}
/* l'aube sur la vallee : le heros sort du Gouffre */
static void dawn(float t,float d,bool gold){
  if(cn_prev<0) cp_clear();
  Valley v={0}; v.day=1; v.pan=10+20*smooth01(t/d); v.hole=1; v.lights=0.25f; v.sun=0.15f+0.85f*smooth01(t/(d*0.9f)); v.mist=0.7f; v.gold=gold?1.0f:0.35f;
  if(gold) v.pillar=0.55f+0.1f*sinf(t*2);
  valley(t,&v);
  /* le heros remonte du bord et s'arrete face au soleil */
  float pcx=330-v.pan, pcy=286;
  float wt=seg(t,1.2f,6.5f); float hx=pcx+18+wt*120, hy=pcy+2-6*smooth01(seg(t,1.2f,2.0f));
  float ap=seg(t,1.0f,1.6f);
  if(ap>0){ bool walking=t>1.2f && t<6.5f; hero_draw(cn_hero,hx,hy,2.0f,walking?((int)(t*7))&3:0,0,0xFFFFFFFFu,(int)(255*ap));
    gfx_ellipse_a((int)hx,(int)hy+1,6,1,HEX(0x000000),80); }
  if(gold){ for(int i=0;i<26;i++){ float fx=pcx-140+h01(1200u+(uint32_t)i)*280, fy=pcy+18+h01(1210u+(uint32_t)i)*22; float g=seg(t,1+h01(1220u+(uint32_t)i)*4,2+h01(1220u+(uint32_t)i)*4);
      if(g>0){ gfx_vline((int)fx,(int)(fy-3*g),(int)fy,HEX(0x4a7a3a)); uint32_t fc=i%3==0?HEX(0xffd84a):(i%3==1?HEX(0xf5aac8):HEX(0xffffff)); gfx_pset((int)fx,(int)(fy-3*g)-1,fc); gfx_pset((int)fx-1,(int)(fy-3*g),fc); gfx_pset((int)fx+1,(int)(fy-3*g),fc); } } }
  /* oiseaux */
  for(int i=0;i<5;i++){ float ph=fmodf(t*0.08f+h01(1300u+(uint32_t)i),1.0f); float bx=-20+ph*700, by=120+h01(1310u+(uint32_t)i)*60+sinf(t*2+i)*4; int f=((int)(t*6+i))&1;
    gfx_line((int)bx-3,(int)by-(f?2:0),(int)bx,(int)by,HEX(0x3a2a3a)); gfx_line((int)bx,(int)by,(int)bx+3,(int)by-(f?2:0),HEX(0x3a2a3a)); }
  if(cue(t,0.5f)) sfx_play(SFX_REVIVE,0.4f,0,0.6f);
  if(gold && rfrand()<0.6f) cp_add(pcx+rfrange(-60,60),pcy,rfrange(-6,6),rfrange(-60,-30),0,rfrange(2,4),1.2f,HEX(0xffe0a0),PK_MOTE);
  if(rfrand()<0.15f) cp_add(rfrange(0,640),rfrange(150,300),rfrange(4,12),rfrange(-3,3),0,rfrange(3,5),1,HEX(0xfff0c0),PK_MOTE);
  cp_draw(1);
  GP.sat=1.1f;
}
static void sh_dawn(float t,float d){ dawn(t,d,false); }
static void sh_dawn_gold(float t,float d){ dawn(t,d,true); }
/* la fete au campement */
static float fw_next=0;
static void sh_feast(float t,float d){
  (void)d;
  if(cn_prev<0){ cp_clear(); fw_next=0.4f; }
  camp_world(cn_dt,1.45f);
  light_add(320,226,240,HEX(0xffa050),1.2f);
  /* les huit heros de la legende dansent autour du feu */
  int list[CH_COUNT], n=0; for(int c=0;c<CH_COUNT;c++) list[n++]=c;
  cn_lo=true;
  for(int i=0;i<n;i++){ float a=t*0.9f+i*TAU/n; float x=320+cosf(a)*56, y=232+sinf(a)*15; float hop=fabsf(sinf(t*6+i*1.3f))*4;
    hero_draw(list[i],x,y-hop,1.0f,((int)(t*8+i))&3,sinf(a)>0?(cosf(a)<0):(cosf(a)>0),0xFFFFFFFFu,255); gfx_ellipse_a((int)x,(int)y+1,5,1,HEX(0x000000),90); }
  cn_lo=false;
  /* guirlandes */
  int ly; emis_on(&ly);
  for(int g=0;g<3;g++){ float x0=120+g*150.0f, x1=x0+130; for(int k=0;k<=16;k++){ float u=k/16.0f; float x=lerpf(x0,x1,u), y=150+g*6+sinf(u*PI)*14; uint32_t c=BALLS[BK_FEU+(k+g)%8].col; float tw=0.6f+0.4f*sinf(t*4+k+g);
      if(g_gpu){ gfx_pset((int)x,(int)y,col_scale(c,tw*1.6f)); gfx_pset((int)x,(int)y+1,col_scale(c,tw*0.8f)); } else { gfx_pset((int)x,(int)y,c); gfx_pset((int)x,(int)y+1,c); } fx_glow(x,y,5,c,0.35f*tw); light_add(x,y,16,c,0.25f*tw); } }
  /* lanternes */
  for(int k=0;k<6;k++){ float lx=150+k*68.0f, ly=200+((k*7)%3)*8; gfx_vline((int)lx,(int)ly,(int)ly+26,HEX(0x3a2618)); gfx_rect((int)lx-2,(int)ly-4,5,5,HEX(0x2a1810));
    gfx_rect((int)lx-1,(int)ly-3,3,3,HEX(0xffd070)); fx_glow(lx,ly-1,9,HEX(0xffb040),0.45f+0.1f*sinf(t*5+k)); light_add(lx,ly,50,HEX(0xffb060),0.5f); }
  emis_off(ly);
  /* feux d'artifice */
  fw_next-=cn_dt;
  if(fw_next<=0 && t<d-1.5f){ fw_next=rfrange(0.45f,0.9f); float x=rfrange(140,500), y=rfrange(60,130); uint32_t c=BALLS[BK_FEU+rirand(12)].col;
    for(int i=0;i<46;i++){ float a=rfrand()*TAU, s=rfrange(30,95); cp_add(x,y,cosf(a)*s,sinf(a)*s,26,rfrange(1.0f,1.8f),rfrange(1.2f,2.0f),i%4?c:HEX(0xffffff),PK_FW); }
    cp_add(x,y,0,0,0,0.25f,8,HEX(0xffffff),PK_MOTE);
    sfx_play(SFX_EXPLODE,0.25f,(x-320)/320,1.5f+rfrand()*0.4f); light_add(x,y,200,c,1.0f); }
  cp_draw(1);
  GP.vig=0.5f;
}
/* mais quelque chose bat encore : le froid des Profondeurs */
static void sh_teaser(float t,float d){
  (void)d;
  if(cn_prev<0) cp_clear();
  grad3(VY0,150,HEX(0x05070f),HEX(0x0c1224),HEX(0x141a30),0.5f);
  stars(2024,90,VY0,140,t,0.8f);
  for(int x=0;x<SCREEN_W;x++){ for(int y=140;y<VY1;y++){ float k=(float)(y-140)/(float)(VY1-140); pxs(x,y,col_lerp(HEX(0x0e1418),HEX(0x080b0e),k)); } }
  field(0,140,HEX(0x0e1418),HEX(0x06090c),HEX(0x18242a),0);
  float frost=seg(t,1.0f,8.0f);
  /* battements calqués sur la musique */
  static const float BEAT[12]={0,0.5625f,1.5f,2.0625f,3.0f,3.5625f,4.5f,5.0625f,6.0f,6.5625f,7.5f,8.0625f};
  float pulse=0; for(int i=0;i<12;i++){ float k=t-BEAT[i]; if(k>=0 && k<0.5f) pulse=maxf(pulse,(i&1?0.6f:1.0f)*(1-k/0.5f)); }
  pit_hole(320,232,230,64,1,col_lerp(HEX(0x2a3040),HEX(0xc8f0ff),frost*0.6f),HEX(0x4ac8ff),0.18f+0.5f*pulse*(0.4f+0.6f*frost));
  if(cue(t,0.4f)) sfx_play(SFX_EVENT,0.7f,0,0.8f);
  /* le givre gagne le bord */
  for(int i=0;i<70;i++){ float a=h01(1400u+(uint32_t)i)*TAU; float on=seg(frost,h01(1410u+(uint32_t)i)*0.8f,h01(1410u+(uint32_t)i)*0.8f+0.2f); if(on<=0) continue;
    float x=320+cosf(a)*232, y=232+sinf(a)*66; float hh=(3+h01(1420u+(uint32_t)i)*7)*on;
    gfx_tri(x-2,y,x+2,y,x,y-hh,HEX(0xd8f4ff)); gfx_pset((int)x,(int)(y-hh),HEX(0xffffff)); }
  if(t>2.0f && rfrand()<0.6f) cp_add(rfrange(0,640),VY0,rfrange(-6,6),rfrange(14,28),0,rfrange(8,12),1,HEX(0xe8f4ff),PK_SNOW);
  for(int k=0;k<4;k++) if(cue(t,2.0f+k*1.7f)) sfx_play(SFX_CRYSTAL,0.3f,(k-1.5f)/2,0.6f+0.1f*k);
  light_add(320,232,260,HEX(0x5ad0ff),0.3f+0.9f*pulse);
  cp_draw(0);
  GP.sat=0.95f; GP.vig=0.6f;
}
/* FIN : bilan et remerciements */
static void sh_fin(float t,float d){
  (void)d;
  if(cn_prev<0) cp_clear();
  grad3(0,SCREEN_H,HEX(0x07050e),HEX(0x140c22),HEX(0x07050e),0.5f);
  stars(4321,160,0,SCREEN_H,t,0.8f);
  fx_glow(320,120,260,cn_id==2?HEX(0x6aa8ff):(cn_id==3?HEX(0xffc040):HEX(0xff8a3a)),0.12f);
  int ly=layer_current();
  float a0=seg(t,0.3f,1.3f);
  if(a0>0){ if(g_gpu){ layer(LY_EMIS); logo_text(SCREEN_W/2,52,"FIN",7); layer(ly); } logo_text(SCREEN_W/2,52,"FIN",7); }
  if(cue(t,0.4f)){ sfx_play(SFX_ACHIEVE,0.7f,0,0.8f); for(int i=0;i<60;i++){ float a=rfrand()*TAU, s=rfrange(30,120); cp_add(320,78,cosf(a)*s,sinf(a)*s*0.6f,30,rfrange(0.8f,1.6f),1.5f,i&1?HEX(0xffd84a):HEX(0xffffff),PK_SPARK); } }
  if(g_gpu) layer(LY_UI);
  char b[160];
  float a1=seg(t,1.4f,2.2f);
  if(a1>0) text_draw(SCREEN_W/2,112,cn_id==3?"Le Royaume oublié s'éveille":(cn_id==2?"Les Profondeurs se taisent":"Le Gouffre est purifié"),col_lerp(HEX(0x000000),HEX(0xf0e6d0),a1),2,AL_CENTER,TX_OUTLINE);
  int hc=0; for(int c=0;c<CH_COUNT;c++) hc+=S.unlocked[c];
  int pt=S.play_time;
  const char *L[5]; char s0[96],s1[96],s2[96],s3[96],s4[96];
  snprintf(s0,sizeof s0,"Descentes : ^1%d^0      Victoires : ^1%d",S.runs,S.wins);
  snprintf(s1,sizeof s1,"Monstres vaincus : ^1%d",S.total_kills);
  snprintf(s2,sizeof s2,"Temps passé dans le Gouffre : ^1%d h %02d",pt/3600,(pt/60)%60);
  snprintf(s3,sizeof s3,"Héros réunis : ^1%d / %d",hc,CH_COUNT);
  snprintf(s4,sizeof s4,"Journal : ^1%d %%",(int)completion_pct());
  L[0]=s0; L[1]=s1; L[2]=s2; L[3]=s3; L[4]=s4;
  for(int i=0;i<5;i++){ float a=seg(t,2.4f+i*0.45f,2.9f+i*0.45f); if(a<=0) continue; text_draw(SCREEN_W/2,148+i*15,L[i],col_lerp(HEX(0x000000),HEX(0xc8c0d8),a),1,AL_CENTER,TX_SHADOW); }
  float a2=seg(t,5.2f,6.0f);
  if(a2>0) text_draw(SCREEN_W/2,236,"Merci d'avoir joué !",col_lerp(HEX(0x000000),HEX(0xffd84a),a2),2,AL_CENTER,TX_OUTLINE);
  float a3=seg(t,6.4f,7.2f);
  if(a3>0) text_draw(SCREEN_W/2,262,cn_id>=2?"Le Journal vous attend pour viser les 100 %.":"Les Profondeurs et le Royaume oublié sont ouverts : choisissez avant de descendre.",col_lerp(HEX(0x000000),HEX(0x9ad8ff),a3),1,AL_CENTER,TX_SHADOW);
  if(t>7.5f && fmodf(t,1.2f)<0.8f){ snprintf(b,sizeof b,"%s pour continuer",A.pad?btn(B_CONFIRM):"Entrée"); text_draw(SCREEN_W/2,296,b,HEX(0x8a82a0),1,AL_CENTER,0); }
  layer(ly);
  cp_draw(1);
}

/* ---------------- scenarios ---------------- */
static const Shot INTRO[]={
  {9,sh_valley,"Il y a bien longtemps, la vallée dormait en paix sous les étoiles.",1.0f,NULL,0,0,FD_BLACK,FD_NONE},
  {9,sh_rupture,"Puis, une nuit, la terre s'est déchirée…",0.6f,"…et le Gouffre est né.",5.9f,0,FD_NONE,FD_BLACK},
  {9,sh_eye,"Tout au fond, quelque chose a ouvert les yeux.",0.8f,"Ce qui regarde depuis le fond. Ce que le fond regarde.",5.3f,0,FD_BLACK,FD_BLACK},
  {9,sh_horde,"Depuis, chaque nuit, ses créatures remontent vers la lumière, toujours plus nombreuses.",0.8f,NULL,0,0,FD_BLACK,FD_BLACK},
  {9,sh_heroes,"Alors les gardiens de la vallée se sont dressés au bord du Gouffre…",0.8f,NULL,0,0,FD_BLACK,FD_NONE},
  {9,sh_balls,"…armés de billes enchantées qui rebondissent sur la roche et brisent tout ce qu'elles touchent.",0.5f,NULL,0,0,FD_NONE,FD_BLACK},
  {12,sh_guardians,"Quatre gardiens règnent sur ses profondeurs.",0.6f,"Tant qu'ils vivront, le Gouffre restera ouvert.",7.0f,0,FD_BLACK,FD_BLACK},
  {12,sh_descent,"Descendez. Brisez-les. Ne laissez rien remonter.",0.6f,NULL,0,5.8f,FD_BLACK,FD_BLACK},
};
static const Shot FIN_A[]={
  {9,sh_shatter_eye,"L'Œil du Gouffre s'est fermé, pour toujours.",0.8f,NULL,0,0,FD_BLACK,FD_BLACK},
  {9,sh_silence,"Une à une, ses créatures se sont éteintes, comme des braises sous la pluie.",0.8f,NULL,0,0,FD_BLACK,FD_BLACK},
  {12,sh_dawn,"Au matin, pour la première fois depuis des années, le soleil s'est levé sur la vallée.",0.8f,NULL,0,0,FD_BLACK,FD_BLACK},
  {12,sh_feast,"On a allumé des feux au bord du Gouffre, et l'on a chanté le nom des héros jusqu'à l'aube.",0.8f,NULL,0,0,FD_BLACK,FD_BLACK},
  {9,sh_teaser,"Mais sous le fond du Gouffre, quelque chose d'autre bat encore…",0.8f,"Les Profondeurs vous attendent.",5.6f,0,FD_BLACK,FD_BLACK},
  {15,sh_fin,NULL,0,NULL,0,0,FD_BLACK,FD_BLACK},
};
static const Shot FIN_B[]={
  {9,sh_shatter_void,"Le Cœur du Néant s'est effondré sur lui-même.",0.8f,NULL,0,0,FD_BLACK,FD_BLACK},
  {9,sh_montage,"Le givre a fondu, la cité s'est tue, la ruche s'est endormie…",0.6f,"…et les ombres du Néant se sont dissipées.",6.4f,0,FD_BLACK,FD_BLACK},
  {12,sh_dawn_gold,"Là où s'ouvrait le Gouffre, c'est désormais la lumière qui monte des profondeurs.",0.8f,NULL,0,0,FD_BLACK,FD_BLACK},
  {12,sh_feast,"Les héros peuvent enfin se reposer… ou redescendre, pour la gloire.",0.8f,NULL,0,0,FD_BLACK,FD_BLACK},
  {15,sh_fin,NULL,0,NULL,0,0,FD_BLACK,FD_BLACK},
};
static const Shot FIN_C[]={
  {9,sh_shatter_sun,"Le Soleil Noir s'est éteint dans un dernier éclat.",0.8f,NULL,0,0,FD_BLACK,FD_BLACK},
  {9,sh_silence,"Les lucioles, les marais, les mines de cristal : tout le royaume oublié s'est tu.",0.8f,NULL,0,0,FD_BLACK,FD_BLACK},
  {12,sh_dawn_gold,"Et pour la première fois depuis des siècles, un vrai soleil a éclairé ses jardins.",0.8f,NULL,0,0,FD_BLACK,FD_BLACK},
  {12,sh_feast,"Au campement, on a fêté le retour des héros… et de nouveaux visages autour du feu.",0.8f,NULL,0,0,FD_BLACK,FD_BLACK},
  {15,sh_fin,NULL,0,NULL,0,0,FD_BLACK,FD_BLACK},
};

/* ---------------- sous-titres (machine a ecrire, sur deux lignes si besoin) ---------------- */
static int u8_len(const char *s){ int c=0; for(;*s;s++) if((*s&0xC0)!=0x80) c++; return c; }
static int u8_bytes(const char *s,int n){ int i=0,c=0; while(s[i]){ if((s[i]&0xC0)!=0x80){ if(c==n) break; c++; } i++; } return i; }
static void caption(const char *s,float age,float alpha){
  if(!s || alpha<=0.01f) return;
  char l1[256], l2[256]; int n=(int)strlen(s); if(n>250) n=250;
  int split=-1;
  if(text_w(s,1)>584){ int best=1<<30; for(int i=1;i<n;i++) if(s[i]==' '){ char tmp[256]; memcpy(tmp,s,(size_t)i); tmp[i]=0; int w1=text_w(tmp,1), w2=text_w(s+i+1,1); int sc=abs(w1-w2); if(w1<=584 && w2<=584 && sc<best){ best=sc; split=i; } } }
  if(split>0){ memcpy(l1,s,(size_t)split); l1[split]=0; snprintf(l2,sizeof l2,"%s",s+split+1); } else { memcpy(l1,s,(size_t)n); l1[n]=0; l2[0]=0; }
  int shown=(int)(age*44.0f); int n1=u8_len(l1);
  uint32_t col=col_lerp(HEX(0x000000),HEX(0xece4d4),alpha);
  char buf[256];
  int y = l2[0] ? SCREEN_H-LB+4 : SCREEN_H-LB+10;
  int k1=mini(shown,n1); int b1=u8_bytes(l1,k1); memcpy(buf,l1,(size_t)b1); buf[b1]=0;
  /* on centre la ligne complete pour qu'elle ne bouge pas pendant qu'elle s'ecrit */
  int full=text_w(l1,1); text_draw(SCREEN_W/2-full/2,y,buf,col,1,AL_LEFT,TX_SHADOW);
  if(l2[0] && shown>n1){ int k2=mini(shown-n1-1,u8_len(l2)); if(k2>0){ int b2=u8_bytes(l2,k2); memcpy(buf,l2,(size_t)b2); buf[b2]=0; int f2=text_w(l2,1); text_draw(SCREEN_W/2-f2/2,y+12,buf,col,1,AL_LEFT,TX_SHADOW); } }
}

/* ---------------- moteur ---------------- */
static float list_total(void){ float s=0; for(int i=0;i<cn_n;i++) s+=cn_list[i].dur; return s; }
void cine_start(int id,int ret){
  cn_id=clampi(id,0,3); cn_ret=ret; cn_t=-0.6f; cn_hold=0; cn_shot=-1; cn_prev=-1; cn_done=false; cn_hint=0;
  cn_list= cn_id==0?INTRO:(cn_id==1?FIN_A:(cn_id==2?FIN_B:FIN_C)); cn_n= cn_id==0?COUNTOF(INTRO):(cn_id==1?COUNTOF(FIN_A):(cn_id==2?COUNTOF(FIN_B):COUNTOF(FIN_C)));
  cn_hero= cn_id==0 ? CH_SENTINELLE : clampi(S.last_char,0,CH_COUNT-1);
  cp_clear();
  int mus= cn_id==0?MUS_INTRO:(cn_id==2?MUS_FIN2:MUS_FIN);
  music_prepare(mus); music_play(mus,1.0f);
}
static void cine_finish(void){
  if(cn_done) return; cn_done=true;
  if(cn_id==0) S.intro_seen=true; else S.ends_seen|=1<<(cn_id-1);
  save_write();
  goto_state(cn_ret);
}
static int pend_id=0, pend_ret=GS_TITLE;
void cine_request(int id,int ret){ pend_id=id; pend_ret=ret; goto_state(GS_CINE); }
void cine_enter(void){ cine_start(pend_id,pend_ret); }
bool cine_unlocked(int id){
  if(id<=0) return true;
  if(id>3) return false;
  return ((S.ends_seen>>(id-1))&1) || lvl_maxwon(id*LEVELS_PER_DEST-1)>0;
}
void cine_frame(float dt){
  cn_dt=dt; cn_t+=dt;
  /* maintenir une touche pour passer */
  bool held=false;
  if(g_in){ held=g_in->key[KEY_ESCAPE]||g_in->key[KEY_ENTER]||g_in->key[KEY_SPACE]||g_in->mb[MOUSE_LEFT];
    if(g_in->pad_on) held=held||g_in->pad[PAD_A]||g_in->pad[PAD_B]||g_in->pad[PAD_START]; }
  if(held){ cn_hold+=dt; cn_hint=2.5f; } else { cn_hold=maxf(0,cn_hold-dt*3); if(cn_hint>0) cn_hint-=dt; }
  if(cn_hold>=1.0f) cine_finish();
  /* plan courant */
  float acc=0; int si=-1; float lt=0;
  for(int i=0;i<cn_n;i++){ if(cn_t<acc+cn_list[i].dur){ si=i; lt=cn_t-acc; break; } acc+=cn_list[i].dur; }
  if(cn_t>=list_total()){ cine_finish(); si=cn_n-1; lt=cn_list[si].dur; }
  cn_shake=0; cn_flash=maxf(0,cn_flash-dt*1.6f);
  layer(LY_SCENE); gfx_noclip();
  if(g_gpu){ GP.bloom=S.bloom?0.7f:0; GP.bloom_thr=0.78f; GP.sat=1.06f; GP.con=1.04f; GP.vig=0.45f; GP.grain=S.bloom?0.02f:0; GP.chroma=S.bloom?0.04f:0; GP.emis_gain=1.0f;
    GP.light_gain=0.55f; GP.light_range=1.0f; GP.normal_amt=0.5f; }
  light_begin(0,0,SCREEN_W,SCREEN_H,256,256,256); light_gain(0.55f);
  if(cn_t<0 || si<0){ gfx_clear(HEX(0x000000)); }
  else {
    if(si!=cn_shot){ cn_shot=si; cn_prev=-1; }
    const Shot *s=&cn_list[si];
    cp_update(dt);
    s->fn(lt,s->dur);
    cn_prev=lt;
    if(cn_shot==cn_n-1 && s->fn==sh_fin && lt>5.0f && (A.confirm||A.click||A.back)) cine_finish();
  }
  light_apply();
  if(!g_gpu && S.bloom){ post_bloom(0,LB,SCREEN_W,SCREEN_H-LB,200,0.5f,0.85f); post_grade(0,LB,SCREEN_W,SCREEN_H-LB,1.06f,1.04f,HEX(0x100c20),0.08f,0.5f); }
  /* camera */
  if(cn_shake>0 && S.shake){ float k=cn_shake, tt=g_time;
    float sx=k*(sinf(tt*53.1f)*0.55f+sinf(tt*31.7f+1.3f)*0.3f), sy=k*(sinf(tt*47.3f+0.7f)*0.55f+sinf(tt*27.1f+2.1f)*0.3f);
    if(g_gpu){ GP.cam_x=sx; GP.cam_y=sy; }
    else { static uint32_t tmp[SCREEN_W*SCREEN_H]; int dx=(int)roundf(sx), dy=(int)roundf(sy); if(dx||dy){ memcpy(tmp,g_screen,sizeof tmp);
        for(int y=0;y<SCREEN_H;y++){ int sy2=clampi(y-dy,0,SCREEN_H-1); for(int x=0;x<SCREEN_W;x++) g_screen[y*SCREEN_W+x]=tmp[sy2*SCREEN_W+clampi(x-dx,0,SCREEN_W-1)]; } } } }
  /* fondus */
  layer(LY_UI); gfx_noclip();
  float fade=0, white=cn_flash;
  if(cn_t<0) fade=1;
  else if(si>=0){ const Shot *s=&cn_list[si];
    if(s->fin==FD_BLACK && lt<0.7f) fade=maxf(fade,1-lt/0.7f);
    if(s->fout==FD_BLACK && lt>s->dur-0.7f) fade=maxf(fade,(lt-(s->dur-0.7f))/0.7f);
    if(s->fin==FD_WHITE && lt<0.7f) white=maxf(white,1-lt/0.7f); }
  if(white>0){ if(g_gpu){ GP.flash[0]=1; GP.flash[1]=0.97f; GP.flash[2]=0.92f; GP.flash[3]=clampf(white,0,1); } else gfx_rect_add(0,0,SCREEN_W,SCREEN_H,HEX(0xfff8ec),(int)(clampf(white,0,1)*255)); }
  if(fade>0) gfx_darken(0,0,SCREEN_W,SCREEN_H,(int)(clampf(fade,0,1)*255));
  /* bandes noires et sous-titres */
  gfx_rect(0,0,SCREEN_W,LB,HEX(0x000000)); gfx_rect(0,SCREEN_H-LB,SCREEN_W,LB,HEX(0x000000));
  if(si>=0 && cn_t>=0){ const Shot *s=&cn_list[si];
    float end= s->cend>0 ? s->cend : s->dur-0.4f;
    if(s->c1 && lt>=s->t1){ float a=1; float e1= s->c2 ? s->t2-0.25f : end; if(lt>e1) a=1-(lt-e1)/0.3f; caption(s->c1,lt-s->t1,clampf(a,0,1)); }
    if(s->c2 && lt>=s->t2){ float a= lt>end ? 1-(lt-end)/0.3f : 1; caption(s->c2,lt-s->t2,clampf(a,0,1)); } }
  /* « maintenir pour passer » */
  { float a = cn_hold>0 ? 1.0f : (cn_hint>0 ? 0.8f : (cn_t>1.5f ? 0.38f : 0));
    if(a>0){ char b[96]; snprintf(b,sizeof b,"Maintenir %s pour passer",A.pad?btn(B_CONFIRM):"Échap"); int w=text_w(b,1);
      text_draw(SCREEN_W-12,11,b,col_lerp(HEX(0x000000),HEX(0xb0a8c0),a),1,AL_RIGHT,0);
      int cx=SCREEN_W-12-w-10, cy=15; gfx_ring(cx,cy,5,col_lerp(HEX(0x000000),HEX(0x4a4458),a));
      int seg2=(int)(clampf(cn_hold,0,1)*28); for(int k=0;k<seg2;k++){ float an=-PI*0.5f+k*TAU/28; gfx_pset(cx+(int)roundf(cosf(an)*5),cy+(int)roundf(sinf(an)*5),HEX(0xffd84a)); gfx_pset(cx+(int)roundf(cosf(an)*4),cy+(int)roundf(sinf(an)*4),HEX(0xc9a24a)); } } }
  layer(LY_SCENE);
}
