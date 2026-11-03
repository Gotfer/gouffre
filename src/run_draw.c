/* GOUFFRE - rendu d'une descente : decor, entites, lumieres, interface */
#include "game.h"

#define TEXH 384
static Sprite bg_tex;
static uint8_t bg_em[PIT_W*TEXH];
static int bg_biome=0;
static float glow_x[80], glow_y[80]; static uint32_t glow_c[80]; static float glow_r[80]; static int nglow;
static int OX, OY;
static uint32_t bg_nrm[PIT_W*TEXH];

Sprite *bg_texture(void){ if(!bg_tex.px) bg_build(0); return &bg_tex; }
static void plinth_build(int biome);
static void add_glow(float x,float y,uint32_t c,float r){ if(nglow<80){ glow_x[nglow]=x; glow_y[nglow]=y; glow_c[nglow]=c; glow_r[nglow]=r; nglow++; } }

/* ---------- primitives "enroulees" sur la texture du sol (raccord vertical parfait) ---------- */
static inline int wy(int y){ return ((y%TEXH)+TEXH)%TEXH; }
static inline void tps(int x,int y,uint32_t c){ if(x>=0&&x<PIT_W) bg_tex.px[wy(y)*PIT_W+x]=c|0xFF000000u; }
static inline uint32_t tgt(int x,int y){ x=clampi(x,0,PIT_W-1); return bg_tex.px[wy(y)*PIT_W+x]; }
static inline void tem(int x,int y,int v){ if(x>=0&&x<PIT_W) bg_em[wy(y)*PIT_W+x]=(uint8_t)clampi(v,1,255); }
static inline void tblend(int x,int y,uint32_t c,float a){ if(x<0||x>=PIT_W) return; uint32_t *p=&bg_tex.px[wy(y)*PIT_W+x]; *p=col_lerp(*p,c,a); }
static void tdisc(float cx,float cy,float r,uint32_t c){ for(int y=(int)floorf(cy-r);y<=(int)ceilf(cy+r);y++) for(int x=(int)floorf(cx-r);x<=(int)ceilf(cx+r);x++){ float dx=x+0.5f-cx, dy=y+0.5f-cy; if(dx*dx+dy*dy<=r*r) tps(x,y,c); } }
static void tline(float x0,float y0,float x1,float y1,float r,uint32_t c){ float dx=x1-x0, dy=y1-y0; int n=(int)(sqrtf(dx*dx+dy*dy))+1; for(int i=0;i<=n;i++){ float t=(float)i/n; tdisc(x0+dx*t,y0+dy*t,r,c); } }
static float tnoise(float x,float y,float sc,uint32_t seed){ float t=y/TEXH; return lerpf(fbm(x*sc,y*sc,seed),fbm(x*sc,(y-TEXH)*sc,seed),t); }

static void crack(int x,int y,int len,uint32_t c,uint32_t c2,int emis){
  float fx=(float)x, fy=(float)y; float a=frand()*TAU;
  for(int i=0;i<len;i++){
    a+=frange(-0.6f,0.6f); fx+=cosf(a); fy+=sinf(a)*0.8f;
    tps((int)fx,(int)fy,c); if(c2) tps((int)fx+1,(int)fy,c2);
    if(emis){ tem((int)fx,(int)fy,(int)(fy*2)+emis); if((i%10)==5) add_glow(fx,(float)wy((int)fy),c,22); }
  }
}
static void skull_deco(int x,int y,uint32_t c){
  tps(x+1,y,c); tps(x+2,y,c); tps(x+3,y,c); for(int i=0;i<5;i++) tps(x+i,y+1,c); for(int i=0;i<5;i++) tps(x+i,y+2,c);
  tps(x+1,y+2,HEX(0x1a1420)); tps(x+3,y+2,HEX(0x1a1420)); tps(x+1,y+3,c); tps(x+2,y+3,c); tps(x+3,y+3,c); tps(x+2,y+4,col_scale(c,0.7f));
}
static void candles(int x,int y){
  int n=irange(2,3);
  for(int k=0;k<n;k++){
    int cx=x+k*4-(n*2), h=irange(4,8);
    for(int j=0;j<h;j++){ tps(cx,y-j,HEX(0xeadfc4)); tps(cx+1,y-j,HEX(0xc8bca4)); }
    tps(cx,y+1,HEX(0xd8ccb4)); tps(cx-1,y+1,HEX(0xd8ccb4)); tps(cx+2,y+1,HEX(0xb8ac94));
    tps(cx,y-h,HEX(0x2a1c14)); tps(cx,y-h-1,HEX(0xffd36a)); tps(cx,y-h-2,HEX(0xffb040)); tps(cx+1,y-h-1,HEX(0xff8a2a));
    int ph=irand(200)+1; tem(cx,y-h-1,ph); tem(cx,y-h-2,ph); tem(cx+1,y-h-1,ph);
  }
  add_glow((float)x,(float)wy(y-6),HEX(0xffb060),34);
}
static void mushroom(int x,int y,int teal){
  uint32_t cap=teal?HEX(0x45c2c8):HEX(0xf59ac0), capd=teal?HEX(0x1f6e78):HEX(0xa8507f);
  int h=irange(3,6), r=irange(2,4);
  for(int j=0;j<h;j++){ tps(x,y-j,HEX(0xd8c8b0)); tps(x+1,y-j,HEX(0xb8a890)); }
  for(int dy=-r;dy<=0;dy++) for(int dx=-r-1;dx<=r+1;dx++){ if(dx*dx*0.7f+dy*dy*1.6f>r*r+1) continue; int px=x+dx, py=y-h+dy; tps(px,py,dy==0?capd:cap); tem(px,py,(teal?128:1)+irand(60)); }
  tps(x-1,y-h-r+1,HEX(0xffffff));
}
static void root(float x,float y,float len,float w){
  float a=frange(0,TAU);
  for(float i=0;i<len;i+=1){
    a+=frange(-0.25f,0.25f); float nx=x+cosf(a)*1.2f, ny=y+sinf(a)*1.2f; float ww=w*(1-i/len)+0.6f;
    tline(x,y,nx,ny,ww+0.8f,HEX(0x1e140e)); tline(x-0.4f,y-0.4f,nx-0.4f,ny-0.4f,ww,HEX(0x5c3a24)); if(ww>1.2f) tps((int)nx,(int)ny-1,HEX(0x8a5a38));
    x=nx; y=ny;
  }
}

/* fond « peint » : taches douces tramees sur 5 tons, touches de pinceau en diagonale */
static void paint_base(const uint32_t *pal,uint32_t seed,float contrast,float sc){
  for(int y=0;y<TEXH;y++) for(int x=0;x<PIT_W;x++){
    float n1=tnoise((float)x,(float)y,0.032f*sc,seed+101), n2=tnoise((float)x,(float)y,0.085f*sc,seed+102), n3=tnoise((float)x,(float)y,0.21f*sc,seed+103);
    float v=0.5f+contrast*((n1-0.5f)*1.35f+(n2-0.5f)*0.6f+(n3-0.5f)*0.22f);
    static const uint8_t BY[4][4]={{0,8,2,10},{12,4,14,6},{3,11,1,9},{15,7,13,5}};
    float f=clampf(v,0,0.999f)*5+((BY[y&3][x&3]+0.5f)/16.0f-0.5f)*0.85f;
    bg_tex.px[y*PIT_W+x]=pal[clampi((int)floorf(f),0,4)];
  }
}
static void paint_streaks(uint32_t c,int n,float ang,float a,int lmin,int lmax){
  for(int k=0;k<n;k++){ float x=frand()*PIT_W, y=frand()*TEXH; int len=irange(lmin,lmax); float aa=ang+frange(-0.12f,0.12f); bool thick=chance(0.3f);
    for(int i=0;i<len;i++){ x+=cosf(aa); y+=sinf(aa); float f=a*(i<3||i>len-4?0.5f:1.0f); tblend((int)x,(int)y,c,f); if(thick) tblend((int)x,(int)y+1,c,f*0.6f); } }
}
/* touches de pinceau epaisses (capsules effilees, bords crantes), regroupees en plaques par un bruit basse frequence */
static void paint_dabs(uint32_t c,int n,float ang,float wmin,float wmax,int lmin,int lmax,float a,float thr,uint32_t seed){
  for(int k=0,tries=0;k<n && tries<n*30;tries++){
    float x=frand()*PIT_W, y=frand()*TEXH;
    if(thr>-1.0f){ float g=tnoise(x,y,0.022f,seed); if(thr>=0 ? g<thr : g>-thr) continue; }
    k++;
    float L=(float)irange(lmin,lmax), W=frange(wmin,wmax), aa=ang+frange(-0.1f,0.1f); float c2=cosf(aa), s2=sinf(aa); float hl=L*0.5f;
    int R=(int)(hl+W)+1; uint32_t hs=(uint32_t)(k*2654435761u)^seed;
    for(int dy=-R;dy<=R;dy++) for(int dx=-R;dx<=R;dx++){
      float al=dx*c2+dy*s2, ac=-dx*s2+dy*c2;
      float ex=fabsf(al)>hl ? fabsf(al)-hl : 0; float d=sqrtf(ex*ex+ac*ac);
      float wl=W*0.5f*(1.0f-0.45f*fminf(1.0f,fabsf(al)/hl));
      uint32_t h=hs^(((uint32_t)dx*73856093u)^((uint32_t)dy*19349663u)); h^=h>>13; h*=0x5bd1e995u; h^=h>>15;
      float jit=((h&255)/255.0f-0.5f)*0.9f;
      if(d<wl+jit) tblend((int)x+dx,(int)y+dy,c,a);
    }
  }
}
void bg_build(int biome){
  const BiomeDef *b=&BIOMES[biome];
  if(!bg_tex.px) bg_tex=spr_alloc(PIT_W,TEXH);
  memset(bg_em,0,sizeof bg_em);
  bg_biome=biome; nglow=0;
  uint32_t seed=rng_next();
  (void)b;
  /* v8 : decor HD precalcule (dossier « donnees ») : version basse definition pour le processeur */
  if(hd_floor_low(biome,bg_tex.px,bg_em,bg_nrm,PIT_W,TEXH,add_glow)){
    for(int i=0;i<PIT_W*TEXH;i++) bg_tex.px[i]|=0xFF000000u;
    plinth_build(biome);
    { extern bool g_norender; if(!g_norender) scenery_build(biome,seed^0x5bd1e995u); }
    return;
  }
  switch(biome){
    case 0: { /* catacombes : dalles de pierre claire, joints doux, os, bougies */
      static const uint32_t PAL[5]={HEX(0x5c5666),HEX(0x6a6474),HEX(0x787282),HEX(0x868092),HEX(0x9890a4)};
      paint_base(PAL,seed,0.5f,1.0f);
      paint_dabs(HEX(0x504a5c),110,-0.62f,3,7,14,40,0.55f,0.52f,seed+71);
      paint_dabs(HEX(0xa8a0b6),90,-0.62f,2,5,10,30,0.45f,-0.5f,seed+71);
      int y=0; while(y<TEXH){
        int h=irange(22,30); if(TEXH-y<38) h=TEXH-y;
        int x=-irand(30);
        while(x<PIT_W){
          int w=irange(26,46); float tint=frange(-0.07f,0.07f);
          for(int py=y;py<y+h;py++) for(int px=x;px<x+w;px++){
            if(px<0||px>=PIT_W) continue;
            uint32_t c=col_scale(tgt(px,py),1.0f+tint);
            if(py==y+h-1||px==x+w-1) c=col_lerp(c,HEX(0x2a2532),0.62f);
            else if(py==y||px==x) c=col_lerp(c,HEX(0xd8d0e4),0.2f);
            tps(px,py,c);
          }
          if(chance(0.3f)) crack(x+irand(maxi(1,w)),y+irand(h),irange(6,14),HEX(0x3a3444),0,0);
          x+=w;
        }
        y+=h;
      }
      paint_streaks(HEX(0xbab2ca),30,-0.62f,0.25f,6,20);
      for(int i=0;i<26;i++){ int x=irand(PIT_W), y=irand(TEXH); int l=irange(3,6); bool hz=chance(0.5f);
        for(int k=0;k<l;k++) tps(hz?x+k:x,hz?y:y+k,HEX(0xd8ccb4));
        tps(x-(hz?1:0),y-(hz?0:1),HEX(0xf0e6d0)); tps(hz?x+l:x,hz?y:y+l,HEX(0xf0e6d0)); }
      for(int i=0;i<5;i++) skull_deco(8+irand(PIT_W-16),irand(TEXH),HEX(0xe0d4bc));
      for(int i=0;i<10;i++){ int x=irand(PIT_W), y=irand(TEXH); for(int k=0;k<8;k++){ int px=x+irange(-4,4), py=y+irange(-3,3); tblend(px,py,HEX(0x4a7a44),0.45f); } }
      for(int i=0;i<4;i++) candles(14+irand(PIT_W-28),irand(TEXH));
    } break;
    case 1: { /* foret : mousse et terre, racines, champignons */
      static const uint32_t PAL[5]={HEX(0x3c5a34),HEX(0x48683a),HEX(0x547640),HEX(0x62864a),HEX(0x749a56)};
      paint_base(PAL,seed,0.5f,1.0f);
      paint_dabs(HEX(0x34502e),130,-0.62f,3,8,14,44,0.7f,0.52f,seed+71);
      paint_dabs(HEX(0x82a85e),100,-0.62f,2,6,10,32,0.55f,-0.5f,seed+71);
      for(int y=0;y<TEXH;y++) for(int x=0;x<PIT_W;x++){ float m=tnoise((float)x,(float)y,0.05f,seed+11);
        if(m>0.6f){ static const uint8_t BY[4][4]={{0,8,2,10},{12,4,14,6},{3,11,1,9},{15,7,13,5}}; int lv=(int)((m-0.6f)*70); if(BY[y&3][x&3]<lv) tps(x,y,col_lerp(HEX(0x6a5a3a),HEX(0x7e6a46),m)); } }
      paint_streaks(HEX(0x9ac06a),30,-0.62f,0.3f,6,20);
      for(int i=0;i<4;i++) root(frand()*PIT_W,frand()*TEXH,frange(24,44),frange(1.0f,1.6f));
      for(int i=0;i<80;i++){ int x=irand(PIT_W), y=irand(TEXH); uint32_t g=chance(0.5f)?HEX(0x7ac85a):HEX(0x5aa846); int h=irange(2,4); for(int k=0;k<h;k++) tps(x+(k==h-1?irange(-1,1):0),y-k,g); tps(x+1,y,g); }
      for(int i=0;i<14;i++){ int cx=10+irand(PIT_W-20), cy=irand(TEXH); bool teal=chance(0.35f); int n=irange(1,3); for(int k=0;k<n;k++) mushroom(cx+k*5-n*2,cy+irange(-2,2),teal); add_glow((float)cx,(float)wy(cy-5),teal?HEX(0x45c2c8):HEX(0xf59ac0),24); }
    } break;
    case 2: { /* forges : pierre brune chaude, grilles, rivieres de lave */
      static const uint32_t PAL[5]={HEX(0x463632),HEX(0x54423c),HEX(0x624e46),HEX(0x725c52),HEX(0x846c60)};
      paint_base(PAL,seed,0.5f,1.0f);
      paint_dabs(HEX(0x3c2e2a),120,-0.62f,3,8,14,44,0.6f,0.52f,seed+71);
      paint_dabs(HEX(0x94786a),90,-0.62f,2,6,10,32,0.5f,-0.5f,seed+71);
      paint_streaks(HEX(0x9a8070),24,-0.62f,0.28f,6,20);
      int nr=2;
      for(int r=0;r<nr;r++){
        float x0=PIT_W*(r+0.5f)/nr+frange(-20,20); int k1=irange(1,2), k2=irange(2,4); float a1=frange(10,26), a2=frange(4,9), p1=frand()*TAU, p2=frand()*TAU;
        for(int y=0;y<TEXH;y++){
          float t=TAU*y/TEXH; float xc=x0+a1*sinf(k1*t+p1)+a2*sinf(k2*t+p2); float w=1.9f+1.3f*sinf(3*t+p2)+frand()*0.4f;
          for(int x=(int)(xc-w-3);x<=(int)(xc+w+3);x++){
            float d=fabsf(x+0.5f-xc);
            if(d<w){ tps(x,y,HEX(0xff7a22)); tem(x,y,1+((int)(y*1.3f+d*9)&127)); }
            else if(d<w+1.5f) tps(x,y,HEX(0x2a0c08)); else if(d<w+3) tblend(x,y,HEX(0x5a1a10),0.5f);
          }
          if((y%48)==20) add_glow(xc,(float)y,HEX(0xff7a22),30);
        }
      }
      for(int i=0;i<4;i++){ int gx=10+irand(PIT_W-40), gy=irand(TEXH); for(int y=0;y<12;y++) for(int x=0;x<22;x++){ bool bar=(x%4)==0||y==0||y==11; if(bar) tps(gx+x,gy+y,HEX(0x2e2224)); else { tps(gx+x,gy+y,HEX(0xff6a1a)); tem(gx+x,gy+y,130+(x*7+y*13)%100); } } add_glow((float)gx+11,(float)wy(gy+6),HEX(0xff6a1a),26); }
      for(int i=0;i<10;i++) crack(irand(PIT_W),irand(TEXH),irange(10,24),HEX(0xff8a2a),HEX(0x5a1a10),40);
    } break;
    case 4: { /* grottes de givre : glace peinte, touches de pinceau, quelques fissures et cristaux */
      static const uint32_t PAL[5]={HEX(0x6676b4),HEX(0x7888c4),HEX(0x8a9ad2),HEX(0x9eace0),HEX(0xb6c4ee)};
      paint_base(PAL,seed,0.45f,1.0f);
      paint_dabs(HEX(0x5c6aae),150,-0.62f,3,8,14,44,0.85f,0.52f,seed+71);
      paint_dabs(HEX(0x6c7cbc),90,-0.62f,2,6,10,30,0.8f,0.45f,seed+71);
      paint_dabs(HEX(0xc4d0f6),110,-0.62f,2,6,10,36,0.75f,-0.5f,seed+71);
      paint_streaks(HEX(0xd4def8),50,-0.62f,0.55f,8,34);
      for(int i=0;i<10;i++) crack(irand(PIT_W),irand(TEXH),irange(8,20),HEX(0xd8e4ff),HEX(0x5866a6),0);
      for(int i=0;i<7;i++){ int cx=10+irand(PIT_W-20), cy=irand(TEXH); int n=irange(1,3);
        for(int k=0;k<n;k++){ int x=cx+irange(-4,4), h=irange(3,7); for(int j=0;j<h;j++){ int ww=(h-j)/3; for(int q=-ww;q<=ww;q++){ tps(x+q,cy-j,q<0?HEX(0xeefaff):HEX(0x8ad0f4)); tem(x+q,cy-j,128+irand(100)); } } }
        add_glow((float)cx,(float)wy(cy-4),HEX(0x9ae8ff),22); }
      for(int i=0;i<140;i++){ int x=irand(PIT_W), y=irand(TEXH); tps(x,y,chance(0.7f)?HEX(0xeef4ff):HEX(0x5a68a8)); }
    } break;
    case 5: { /* cite engloutie : dalles vertes, canal, algues, coraux */
      static const uint32_t PAL[5]={HEX(0x34605a),HEX(0x3e6e66),HEX(0x4a7c74),HEX(0x588a82),HEX(0x689c92)};
      paint_base(PAL,seed,0.45f,1.0f);
      paint_dabs(HEX(0x2c544e),110,-0.62f,3,7,14,40,0.55f,0.52f,seed+71);
      paint_dabs(HEX(0x7cb0a4),90,-0.62f,2,5,10,30,0.45f,-0.5f,seed+71);
      for(int py=0;py<TEXH;py+=24) for(int px=0;px<PIT_W;px+=24){
        float tint=frange(-0.06f,0.06f);
        for(int y=py;y<py+24;y++) for(int x=px;x<px+24 && x<PIT_W;x++){
          uint32_t c=col_scale(tgt(x,y),1.0f+tint);
          if(y==py||x==px) c=col_lerp(c,HEX(0xb8f0e0),0.2f); if(y==py+23||x==px+23) c=col_lerp(c,HEX(0x0e2422),0.6f);
          tps(x,y,c); } }
      paint_streaks(HEX(0x9ad8c8),34,-0.62f,0.28f,6,20);
      { float x0=PIT_W*0.5f+frange(-30,30); float a1=frange(8,18), p1=frand()*TAU;
        for(int y=0;y<TEXH;y++){ float t=TAU*y/TEXH; float xc=x0+a1*sinf(2*t+p1); float w=9+2*sinf(3*t);
          for(int x=(int)(xc-w-2);x<=(int)(xc+w+2);x++){ float d=fabsf(x+0.5f-xc);
            if(d<w){ tps(x,y,col_scale(HEX(0x2a6a78),0.85f+0.3f*tnoise((float)x,(float)y,0.15f,seed+43))); if(((x*7+y*3)%13)==0) tem(x,y,1+((x*5+y*3)&15)); }
            else if(d<w+2) tps(x,y,HEX(0x6a948a)); }
          if((y%96)==40) add_glow(xc,(float)y,HEX(0x3ab8c0),26); } }
      for(int i=0;i<40;i++){ int x=irand(PIT_W), y=irand(TEXH); int h=irange(3,7); for(int k=0;k<h;k++) tps(x+(int)(sinf(k*0.9f+i)*1.2f),y-k,chance(0.5f)?HEX(0x3a9a5a):HEX(0x4ab86a)); }
      for(int i=0;i<10;i++){ int cx=8+irand(PIT_W-16), cy=irand(TEXH); bool pink=chance(0.5f);
        for(int k=0;k<5;k++){ float a=-PI*0.5f+(k-2)*0.4f; for(int j=0;j<5;j++){ int x=cx+(int)(cosf(a)*j), y=cy+(int)(sinf(a)*j); tps(x,y,pink?HEX(0xf59ac0):HEX(0x5af0d0)); if(j>=3) tem(x,y,pink?30+irand(60):140+irand(60)); } }
        add_glow((float)cx,(float)wy(cy-3),pink?HEX(0xf59ac0):HEX(0x5af0d0),22); }
    } break;
    case 6: { /* ruche d'ambre : alveoles hexagonales, miel, cire */
      const float hs=16.0f/1.7320508f, cw=1.5f*hs;
      for(int y=0;y<TEXH;y++) for(int x=0;x<PIT_W;x++){
        int ci=(int)floorf(x/cw); float best=1e9f, sec=1e9f; int bi=0,bj=0;
        for(int i=ci-1;i<=ci+1;i++){ float ccx=i*cw; float off=(i&1)?8.0f:0.0f; int cj=(int)floorf((y-off)/16.0f);
          for(int j=cj-1;j<=cj+1;j++){ float ccy=j*16.0f+off; float dx=x+0.5f-ccx, dy=y+0.5f-ccy; float d=sqrtf(dx*dx+dy*dy); if(d<best){ sec=best; best=d; bi=i; bj=j; } else if(d<sec) sec=d; } }
        float edge=(sec-best)*0.5f; int jj=((bj%24)+24)%24;
        uint32_t h=(uint32_t)bi*73856093u ^ (uint32_t)jj*19349663u ^ seed; h^=h>>13; h*=0x5bd1e995u; h^=h>>15; int type=(int)(h%10);
        float n=tnoise((float)x,(float)y,0.3f,seed+51);
        uint32_t c;
        if(edge<1.0f) c=HEX(0x5a3a14);
        else if(edge<2.3f) c=col_scale(HEX(0xd0a050),0.8f+0.25f*n);
        else if(type<1 || (type==1 && (h>>8)%3==0)){ c=col_lerp(HEX(0xf0a830),HEX(0xa05010),clampf(best/9.0f,0,1)); if(best<6.5f) tem(x,y,1+(int)clampf(best*8,0,90)); }
        else if(type<3) c=col_lerp(HEX(0xf0d090),HEX(0xc09050),clampf(best/9.0f,0,1));
        else c=col_lerp(HEX(0x7a5222),HEX(0x9a6a30),clampf(best/9.0f,0,1));
        tps(x,y,c);
      }
      for(int i=0,g=0;i<4000 && g<14;i++){ int x=irand(PIT_W), y=irand(TEXH); if(bg_em[y*PIT_W+x]>40){ add_glow((float)x,(float)y,HEX(0xffa030),24); g++; } }
    } break;
    case 7: { /* le neant : obsidienne violacee, etoiles, fissures de lumiere */
      static const uint32_t PAL[5]={HEX(0x24203a),HEX(0x2c2846),HEX(0x363050),HEX(0x40395e),HEX(0x4c446e)};
      paint_base(PAL,seed,0.5f,1.0f);
      paint_dabs(HEX(0x1c182e),120,-0.62f,3,8,14,44,0.65f,0.52f,seed+71);
      paint_dabs(HEX(0x5e5690),90,-0.62f,2,6,10,32,0.5f,-0.5f,seed+71);
      paint_streaks(HEX(0x6a62a0),30,-0.62f,0.35f,6,22);
      for(int i=0;i<160;i++){ int x=irand(PIT_W), y=irand(TEXH); tps(x,y,chance(0.3f)?HEX(0x9a8aff):HEX(0xd8e0f8)); if(chance(0.3f)) tem(x,y,1+irand(60)); }
      for(int i=0;i<9;i++){ float x=frand()*PIT_W, y=frand()*TEXH, a=frand()*TAU; int len=irange(30,70);
        for(int k=0;k<len;k++){ a+=frange(-0.35f,0.35f); x+=cosf(a); y+=sinf(a)*0.9f; tps((int)x,(int)y,HEX(0xe0f0ff)); tem((int)x,(int)y,128+((k*3)&127)); tps((int)x+1,(int)y,HEX(0x4a4a78));
          if(k%14==7) add_glow(x,(float)wy((int)y),HEX(0xc8d8ff),22);
          if(chance(0.06f)){ float b2=a+frange(-1.2f,1.2f); float bx=x, by=y; int nq=irange(6,14); for(int q=0;q<nq;q++){ bx+=cosf(b2); by+=sinf(b2)*0.9f; tps((int)bx,(int)by,HEX(0xa8b8e8)); tem((int)bx,(int)by,128+((q*5)&127)); } } } }
    } break;
    case 8: { /* jardin des lucioles : prairie bleu-vert, pas japonais, parterres de fleurs, fleurs-lanternes */
      static const uint32_t PAL[5]={HEX(0x356a4c),HEX(0x3e7a54),HEX(0x48895c),HEX(0x549a66),HEX(0x62aa72)};
      static const uint32_t FL[4]={HEX(0xf6f2ea),HEX(0xf59ac0),HEX(0xc8a8ff),HEX(0xf9dc5c)};
      paint_base(PAL,seed,0.5f,1.0f);
      paint_dabs(HEX(0x2e5e44),120,-0.62f,3,8,14,44,0.6f,0.52f,seed+71);
      paint_dabs(HEX(0x78c084),100,-0.62f,2,6,10,32,0.5f,-0.5f,seed+71);
      paint_streaks(HEX(0x9ad8a0),34,-0.62f,0.3f,6,20);
      { float x0=PIT_W*frange(0.35f,0.65f), a1=frange(30,48), p1=frand()*TAU, a2=frange(6,12), p2=frand()*TAU;
        for(int y=0;y<TEXH;y+=13){ float t=TAU*y/TEXH; float xc=x0+a1*sinf(t+p1)+a2*sinf(3*t+p2)+frange(-3,3);
          float rx=frange(5,7), ry=frange(3.5f,4.5f);
          for(int dy=-6;dy<=6;dy++) for(int dx=-9;dx<=9;dx++){ float q=(dx*dx)/(rx*rx)+(dy*dy)/(ry*ry); int px=(int)xc+dx, py=y+dy;
            if(q<1.0f){ float n=tnoise((float)px,(float)py,0.4f,seed+31); uint32_t c=col_lerp(HEX(0xa8a490),HEX(0xd0ccb4),n);
              if(dy<-ry*0.4f) c=col_lerp(c,HEX(0xf0ecd8),0.3f); else if(dy>ry*0.5f) c=col_lerp(c,HEX(0x6a6a58),0.35f); tps(px,py,c); }
            else if(q<1.45f) tblend(px,py,HEX(0x1e3a2a),0.55f); } } }
      for(int i=0;i<110;i++){ int x=irand(PIT_W), y=irand(TEXH); uint32_t g=chance(0.5f)?HEX(0x8ad88a):HEX(0x5aae6a); int h=irange(2,5); for(int k=0;k<h;k++) tps(x+(k==h-1?irange(-1,1):0),y-k,g); tps(x+1,y,col_scale(g,0.8f)); }
      for(int i=0;i<18;i++){ int cx=irand(PIT_W), cy=irand(TEXH); for(int k=0;k<6;k++){ int x=cx+irange(-5,5), y=cy+irange(-3,3); tps(x,y,HEX(0x2e7a4a)); tps(x+1,y,HEX(0x3a8a52)); tps(x,y-1,HEX(0x4a9a5e)); } }
      for(int i=0;i<34;i++){ int cx=irand(PIT_W), cy=irand(TEXH); uint32_t fc=FL[irand(4)]; int n=irange(4,9);
        for(int k=0;k<n;k++){ int x=cx+irange(-6,6), y=cy+irange(-4,4); tps(x,y+1,HEX(0x2e6a3a)); tps(x-1,y,fc); tps(x+1,y,fc); tps(x,y-1,fc); tps(x,y+1,col_scale(fc,0.75f)); tps(x,y,HEX(0xf9c84a)); } }
      for(int i=0;i<12;i++){ int cx=10+irand(PIT_W-20), cy=irand(TEXH); bool teal=chance(0.4f); int h=irange(4,7);
        for(int j=0;j<h;j++) tps(cx,cy-j,HEX(0x2e7a3a)); tps(cx-1,cy-h+2,HEX(0x4a9a4a)); tps(cx+1,cy-h+3,HEX(0x4a9a4a));
        for(int dy=-2;dy<=1;dy++) for(int dx=-1;dx<=1;dx++){ if(abs(dx)+abs(dy)>2) continue; int px=cx+dx, py=cy-h+dy; tps(px,py,teal?HEX(0x9af8e8):HEX(0xeaff7a)); tem(px,py,(teal?128:1)+irand(90)); }
        add_glow((float)cx,(float)wy(cy-h),teal?HEX(0x7af0e0):HEX(0xd8ff6a),26); }
      for(int i=0;i<40;i++){ int x=irand(PIT_W), y=irand(TEXH); bool c2=chance(0.3f); tps(x,y,c2?HEX(0xb8fff0):HEX(0xf0ffa0)); tem(x,y,(c2?128:1)+irand(120)); }
    } break;
    case 9: { /* marais maudit : vase olive, mares croupies, nenuphars, roseaux, ossements, feux follets */
      static const uint32_t PAL[5]={HEX(0x2e2c1c),HEX(0x383522),HEX(0x433f28),HEX(0x4e4a2e),HEX(0x5a5636)};
      paint_base(PAL,seed,0.55f,1.0f);
      paint_dabs(HEX(0x26241a),130,-0.62f,3,8,14,44,0.65f,0.52f,seed+71);
      paint_dabs(HEX(0x6a6a3a),90,-0.62f,2,6,10,30,0.45f,-0.5f,seed+71);
      paint_streaks(HEX(0x7a7a48),20,-0.62f,0.22f,6,18);
      for(int y=0;y<TEXH;y++) for(int x=0;x<PIT_W;x++){ float m=tnoise((float)x,(float)y,0.028f,seed+11)+0.12f*(tnoise((float)x,(float)y,0.12f,seed+12)-0.5f);
        if(m>0.60f){ tps(x,y, m>0.655f ? (((x*7+y*13)%37)==0 ? HEX(0x3a4a2a) : HEX(0x1c2618)) : HEX(0x26321e)); }
        else if(m>0.578f) tps(x,y,HEX(0x1a180e)); else if(m>0.55f) tblend(x,y,HEX(0x1e1c10),0.45f); }
      for(int i=0,n=0;i<3000 && n<22;i++){ int x=irand(PIT_W), y=irand(TEXH); if((tgt(x,y)&0xFFFFFFu)!=0x1c2618u) continue; n++;
        float r=frange(2.5f,4.2f), na=frand()*TAU;
        for(int dy=-5;dy<=5;dy++) for(int dx=-5;dx<=5;dx++){ float dd=sqrtf(dx*dx+dy*dy*1.3f); if(dd>r) continue;
          float a=atan2f((float)dy,(float)dx); float da=fabsf(fmodf(a-na+3*PI,TAU)-PI); if(da<0.35f && dd>0.8f) continue;
          tps(x+dx,y+dy, dd>r-1 ? HEX(0x2a4a18) : (dy<0?HEX(0x5a8a2e):HEX(0x4a7a26))); }
        if(chance(0.3f)){ uint32_t fc=chance(0.5f)?HEX(0xf0d8e8):HEX(0xe8f0d0); tps(x,y-1,fc); tps(x-1,y,fc); tps(x+1,y,fc); tps(x,y,HEX(0xf9dc5c)); } }
      for(int i=0,n=0;i<4000 && n<40;i++){ int x=irand(PIT_W), y=irand(TEXH); if((tgt(x,y)&0xFFFFFFu)!=0x1a180eu) continue; n++;
        int k=irange(2,4); for(int q=0;q<k;q++){ int xx=x+q*2-k, h=irange(5,10); for(int j=0;j<h;j++) tps(xx+(j>h-3?(q&1):0),y-j,j<h/2?HEX(0x4a5a26):HEX(0x7a8a3a));
          if(chance(0.5f)){ tps(xx,y-h,HEX(0x5a3a1a)); tps(xx,y-h-1,HEX(0x6a4422)); tps(xx,y-h-2,HEX(0x5a3a1a)); } } }
      for(int i=0,n=0;i<5000 && n<60;i++){ int x=irand(PIT_W), y=irand(TEXH); uint32_t c=tgt(x,y)&0xFFFFFFu; if(c!=0x1c2618u && c!=0x26321eu) continue; n++;
        if(chance(0.6f)){ tps(x,y,HEX(0x8ab040)); tem(x,y,1+irand(120)); } else { tps(x,y,HEX(0xa8c070)); tps(x+1,y,HEX(0x5a6a3a)); tem(x,y,128+irand(120)); } }
      for(int i=0;i<10;i++){ int x=irand(PIT_W), y=irand(TEXH); if((tgt(x,y)&0xFFFFFFu)==0x1c2618u) continue; int l=irange(4,7); bool hz=chance(0.6f);
        for(int k=0;k<l;k++) tps(hz?x+k:x,hz?y:y+k,HEX(0xb8b090)); tps(x-1,y-1,HEX(0xd8d0b0)); tps(hz?x+l:x+1,hz?y+1:y+l,HEX(0xd8d0b0)); }
      for(int i=0;i<3;i++){ int x=8+irand(PIT_W-16), y=irand(TEXH); skull_deco(x,y,HEX(0xb8b090)); for(int k=0;k<6;k++) tblend(x+irange(-1,5),y+irange(3,6),HEX(0x26241a),0.6f); }
      for(int i=0;i<8;i++){ int x=10+irand(PIT_W-20), y=irand(TEXH); for(int dy=-1;dy<=1;dy++) for(int dx=-1;dx<=1;dx++) if(abs(dx)+abs(dy)<2){ tps(x+dx,y+dy,HEX(0xc8f070)); tem(x+dx,y+dy,1+irand(60)); } add_glow((float)x,(float)y,HEX(0xa8e040),26); }
    } break;
    case 10: { /* mines de cristal : ardoise, voie de mine, filons et grappes de cristaux, lanternes */
      static const uint32_t PAL[5]={HEX(0x34303e),HEX(0x3c3848),HEX(0x464252),HEX(0x504c5e),HEX(0x5c586c)};
      paint_base(PAL,seed,0.5f,1.0f);
      paint_dabs(HEX(0x2c2836),120,-0.62f,3,8,14,44,0.6f,0.52f,seed+71);
      paint_dabs(HEX(0x6a6680),90,-0.62f,2,6,10,32,0.5f,-0.5f,seed+71);
      paint_streaks(HEX(0x787490),26,-0.62f,0.25f,6,20);
      for(int i=0;i<16;i++){ float x=frand()*PIT_W, y=frand()*TEXH, a=frand()*PI; int l=irange(10,26); for(int k=0;k<l;k++){ x+=cosf(a); y+=sinf(a); tps((int)x,(int)y,HEX(0x1e1c26)); if(k%5==0) a+=frange(-0.5f,0.5f); } }
      { int xr=PIT_W/2+irange(-50,50);
        for(int i=0;i<220;i++){ int x=xr+irange(-15,15), y=irand(TEXH); tps(x,y,chance(0.5f)?HEX(0x5a5668):HEX(0x2a2834)); }
        for(int y=0;y<TEXH;y+=8){ for(int yy=y;yy<y+4;yy++) for(int x=xr-12;x<=xr+12;x++){ uint32_t c= yy==y ? HEX(0x8a6440) : (yy==y+3 ? HEX(0x3a2616) : HEX(0x6a4a2e)); if(((x+yy*3)%7)==0) c=col_scale(c,0.85f); tps(x,yy,c); }
          tps(xr-10,y+1,HEX(0x9a9aa8)); tps(xr+10,y+1,HEX(0x9a9aa8)); }
        for(int y=0;y<TEXH;y++) for(int s=-1;s<=1;s+=2){ int x=xr+s*7; tps(x-1,y,HEX(0x4a4a58)); tps(x,y,HEX(0xc8c8d8)); tps(x+1,y,HEX(0x6a6a7a)); } }
      for(int i=0;i<8;i++){ bool cyan=chance(0.5f); float x=frand()*PIT_W, y=frand()*TEXH, a=frand()*TAU; int l=irange(14,30);
        for(int k=0;k<l;k++){ a+=frange(-0.6f,0.6f); x+=cosf(a); y+=sinf(a)*0.8f; tps((int)x,(int)y,cyan?HEX(0x9af8ff):HEX(0xff8ad8)); tem((int)x,(int)y,cyan?128+((k*5)%120):1+((k*5)%90));
          tps((int)x+1,(int)y,cyan?HEX(0x1a3a5a):HEX(0x4a1a4a)); if(k%12==6) add_glow(x,(float)wy((int)y),cyan?HEX(0x9af8ff):HEX(0xff8ad8),20); } }
      for(int i=0;i<10;i++){ int cx=10+irand(PIT_W-20), cy=irand(TEXH); bool cyan=chance(0.45f); int n=irange(2,4);
        uint32_t lit=cyan?HEX(0xe0fcff):HEX(0xffd8f4), mid=cyan?HEX(0x6ad8f0):HEX(0xe878c8), dk=cyan?HEX(0x2a6a9a):HEX(0x8a2a7a);
        for(int k=0;k<n;k++){ int x=cx+irange(-5,5), h=irange(4,10); for(int j=0;j<h;j++){ int ww=(h-j)/3; for(int q=-ww;q<=ww;q++){ tps(x+q,cy-j,q<0?lit:(q==0?mid:dk)); tem(x+q,cy-j,cyan?128+irand(100):1+irand(90)); } } }
        add_glow((float)cx,(float)wy(cy-4),cyan?HEX(0x9af8ff):HEX(0xff8ad8),24); }
      for(int i=0;i<120;i++){ int x=irand(PIT_W), y=irand(TEXH); int r=irand(10); tps(x,y, r<5?HEX(0x6a6680):(r<8?HEX(0x24222e):(r==8?HEX(0xffb8e8):HEX(0xb8f4ff)))); }
      for(int i=0;i<2;i++){ int x=12+irand(PIT_W-24), y=irand(TEXH); for(int dy=0;dy<5;dy++) for(int dx=-2;dx<=2;dx++){ bool fr=abs(dx)==2||dy==0||dy==4; tps(x+dx,y+dy,fr?HEX(0x2a2a34):HEX(0xffc860)); if(!fr) tem(x+dx,y+dy,100+irand(20)); }
        tps(x,y-1,HEX(0x4a4a58)); add_glow((float)x,(float)wy(y+2),HEX(0xffc060),30); }
    } break;
    case 11: { /* sanctuaire solaire : dalles de gres, incrustations d'or, soleils graves, sable */
      static const uint32_t PAL[5]={HEX(0x9a7a4a),HEX(0xa8885a),HEX(0xb69666),HEX(0xc4a472),HEX(0xd2b282)};
      paint_base(PAL,seed,0.45f,1.0f);
      paint_dabs(HEX(0x8a6a3e),110,-0.62f,3,7,14,40,0.5f,0.52f,seed+71);
      paint_dabs(HEX(0xe0c890),90,-0.62f,2,5,10,30,0.45f,-0.5f,seed+71);
      enum { TS=32 };   /* 9 colonnes x 12 rangs : raccord vertical exact */
      for(int py=0;py<TEXH;py+=TS) for(int px=0;px<PIT_W;px+=TS){
        float tint=frange(-0.06f,0.06f);
        for(int y=py;y<py+TS;y++) for(int x=px;x<px+TS && x<PIT_W;x++){
          uint32_t c=col_scale(tgt(x,y),1.0f+tint);
          if(y==py+TS-1||x==px+TS-1) c=HEX(0x5a4224); else if(y==py+TS-2||x==px+TS-2) c=col_lerp(c,HEX(0x5a4224),0.4f);
          else if(y==py||x==px) c=col_lerp(c,HEX(0xf0dcaa),0.3f);
          tps(x,y,c); }
        if(chance(0.25f)) crack(px+irand(TS),py+irand(TS),irange(6,14),HEX(0x6a5030),0,0);
        if(chance(0.22f)){ int gx=px+6+irand(10), gy=py+6+irand(14);
          for(int r=0;r<3;r++) for(int c=0;c<4;c++){ uint32_t h=(uint32_t)(r*31+c*17)^(uint32_t)(px*7+py*13)^seed; h^=h>>7; h*=0x9e3779b1u; if((h>>9)&1) tps(gx+c*3,gy+r*4,HEX(0x6a4e2a)); if((h>>11)&1) tps(gx+c*3+1,gy+r*4,HEX(0x6a4e2a)); if((h>>13)&1) tps(gx+c*3,gy+r*4+1,HEX(0x6a4e2a)); } }
      }
      for(int py=TS*3-1;py<TEXH;py+=TS*4) for(int x=0;x<PIT_W;x++){ tps(x,py,HEX(0xe8c050)); tem(x,py,1+((x*3)%126)); tps(x,py-1,HEX(0x8a6a2a)); }
      for(int x0=TS*4-1;x0<=TS*5-1;x0+=TS) for(int y=0;y<TEXH;y++){ tps(x0,y,HEX(0xe8c050)); tem(x0,y,1+((y*3)%126)); }
      for(int py=0;py<TEXH;py+=TS) for(int px=0;px<PIT_W-TS;px+=TS){ if(((px/TS)*5+(py/TS)*3)%11!=2) continue;
        float cx=px+TS*0.5f-0.5f, cy=py+TS*0.5f-0.5f;
        for(int dy=-13;dy<=13;dy++) for(int dx=-13;dx<=13;dx++){ float rr=sqrtf((float)(dx*dx+dy*dy)); float a=atan2f((float)dy,(float)dx); int x=(int)(cx+dx+0.5f), y=(int)(cy+dy+0.5f);
          bool disc=rr<4.2f, ring=rr>5.6f&&rr<6.8f, ray=rr>8.0f&&rr<12.5f&&fabsf(fmodf(a*8/TAU+64.5f,1.0f)-0.5f)<0.12f*(12.5f-rr)/4.5f+0.03f;
          if(disc){ tps(x,y, dx<0&&dy<0 ? HEX(0xfff0b0) : HEX(0xffd84a)); tem(x,y,128+(int)(rr*9)%120); }
          else if(ring||ray) tps(x,y, ray&&rr>10.5f ? HEX(0xb08a3a) : HEX(0xd8b050));
          else if(rr<13.0f && (rr<4.9f || (rr>5.0f&&rr<7.4f))) tblend(x,y,HEX(0x5a3a14),0.45f); }
        add_glow(cx,cy,HEX(0xffc040),30); }
      paint_dabs(HEX(0xe8d4a0),70,-0.3f,3,8,16,48,0.5f,0.56f,seed+91);
      for(int i=0;i<40;i++){ int x=irand(PIT_W), y=irand(TEXH); tps(x,y,HEX(0xffe070)); tem(x,y,1+irand(126)); }
    } break;
    default: { /* coeur du gouffre : pierre violette, veines lumineuses (voronoi), cristaux */
      static const uint32_t PAL[5]={HEX(0x4a3a68),HEX(0x564476),HEX(0x624e84),HEX(0x705a94),HEX(0x8068a6)};
      paint_base(PAL,seed,0.5f,1.0f);
      paint_dabs(HEX(0x423460),110,-0.62f,3,7,14,40,0.55f,0.52f,seed+71);
      paint_dabs(HEX(0x927cba),90,-0.62f,2,5,10,30,0.45f,-0.5f,seed+71);
      float pxs[60], pys[60]; int np=0;
      for(int tries=0;tries<4000 && np<48;tries++){ float x=frand()*PIT_W, y=frand()*TEXH; bool ok=true;
        for(int i=0;i<np && ok;i++){ float dx=x-pxs[i], dy=fabsf(y-pys[i]); if(dy>TEXH/2) dy=TEXH-dy; if(dx*dx+dy*dy<26*26) ok=false; }
        if(ok){ pxs[np]=x; pys[np]=y; np++; } }
      for(int y=0;y<TEXH;y++) for(int x=0;x<PIT_W;x++){
        float d1=1e9f, d2=1e9f; float ax=0,ay=0,bx=0,by=0;
        for(int i=0;i<np;i++) for(int o=-1;o<=1;o++){ float sx=pxs[i], sy=pys[i]+o*TEXH; float dx=x+0.5f-sx, dy=y+0.5f-sy; float d=dx*dx+dy*dy;
          if(d<d1){ d2=d1; bx=ax; by=ay; d1=d; ax=sx; ay=sy; } else if(d<d2){ d2=d; bx=sx; by=sy; } }
        float ab=sqrtf((bx-ax)*(bx-ax)+(by-ay)*(by-ay)); float e=ab>0.01f?(d2-d1)/(2*ab):99;
        uint32_t c=bg_tex.px[y*PIT_W+x];
        c=col_scale(c,0.86f+0.22f*clampf(sqrtf(d1)/24.0f,0,1));
        if(e<0.7f){ c=HEX(0x8a4ad8); bg_em[y*PIT_W+x]=(uint8_t)(1+((int)(sqrtf(d1)*0.9f+y*0.15f)&127)); }
        else if(e<1.9f) c=col_lerp(c,HEX(0x2a1a40),0.45f);
        bg_tex.px[y*PIT_W+x]=c;
      }
      paint_streaks(HEX(0xa890d0),30,-0.62f,0.25f,6,18);
      for(int i=0;i<9;i++){ int cx=10+irand(PIT_W-20), cy=irand(TEXH); int n=irange(2,4);
        for(int k=0;k<n;k++){ int x=cx+irange(-5,5), h=irange(4,9); for(int j=0;j<h;j++){ int ww=(h-j)/3; for(int q=-ww;q<=ww;q++){ tps(x+q,cy-j,q<0?HEX(0xe2c8ff):HEX(0x8a5ac8)); tem(x+q,cy-j,128+irand(100)); } } }
        add_glow((float)cx,(float)wy(cy-4),HEX(0xa060ff),22); }
      for(int i=0;i<60;i++){ int x=irand(PIT_W), y=irand(TEXH); tps(x,y,HEX(0x9a80c8)); }
    } break;
  }
  /* ombre portee des murs */
  for(int y=0;y<TEXH;y++) for(int x=0;x<9;x++){ float f=0.55f+0.05f*x; uint32_t *p=&bg_tex.px[y*PIT_W+x]; *p=col_scale(*p,f); p=&bg_tex.px[y*PIT_W+PIT_W-1-x]; *p=col_scale(*p,f); }

  /* reliefs (normales) et brillance : flaques d'eau tres brillantes, metal des forges, cristaux */
  { static float h[PIT_W*TEXH]; static uint8_t sp[PIT_W*TEXH];
    static const float K[BIOME_COUNT]={2.6f,2.0f,3.0f,2.4f, 2.4f,2.4f,2.8f,2.2f, 2.0f,2.2f,2.8f,2.6f}, SPB[BIOME_COUNT]={0.22f,0.08f,0.30f,0.32f, 0.34f,0.20f,0.26f,0.36f, 0.10f,0.24f,0.34f,0.26f};
    for(int i=0;i<PIT_W*TEXH;i++){
      uint32_t c=bg_tex.px[i]&0xFFFFFFu; float l=(cr(c)*0.3f+cg(c)*0.59f+cb(c)*0.11f)/255.0f;
      bool wet = c==0x1a2232u||c==0x26324au||c==0x6a8ab0u||c==0x12302au||c==0x1f4a40u||c==0x7ad8c0u||c==0x2a4a6au||c==0x3e6a8eu||c==0xc8e8ffu||c==0x1c2618u||c==0x26321eu||c==0x3a4a2au;
      if(biome==5 && cg(c)>cr(c)+20 && cb(c)>cg(c)-8 && cb(c)<0x70) wet=true;   /* eau du canal */
      h[i] = wet ? 0.08f : l;
      float sv = wet ? 0.38f : SPB[biome]*(0.5f+1.0f*l);
      if(bg_em[i]) sv=0;
      sp[i]=(uint8_t)clampi((int)(sv*255),0,255);
    }
    nrm_from_height(h,PIT_W,TEXH,bg_nrm,K[biome],sp,true);
  }
  plinth_build(biome);
  /* falaises de part et d'autre du puits (pas de hasard de la partie consomme) */
  { extern bool g_norender; if(!g_norender) scenery_build(biome,seed^0x5bd1e995u); }
}

static uint32_t emis_col(int em,uint32_t base,int y){
  float t=g_time;
  switch(bg_biome){
    case 0: { float f=0.75f+0.25f*sinf(t*17+em*0.7f)+0.1f*sinf(t*41+em); return col_lerp(HEX(0xff8a2a),HEX(0xfff0b0),clampf(f-0.3f,0,1)); }
    case 1: { float f=0.5f+0.5f*sinf(t*1.6f+(em&63)*0.2f); return col_lerp(base,em>=128?HEX(0xb0fff4):HEX(0xffd0ea),f*0.7f); }
    case 2: { static uint32_t L[64]; static bool ok=false;
      if(!ok){ for(int i=0;i<64;i++){ float a=0.5f+0.5f*sinf(i*TAU/64); L[i]=a<0.5f?col_lerp(HEX(0xa02008),HEX(0xf06a14),a*2):col_lerp(HEX(0xf06a14),HEX(0xffc050),(a-0.5f)*2); } ok=true; }
      if(em>=128) return col_lerp(HEX(0xff5a10),HEX(0xffb040),0.5f+0.5f*sinf(t*4+em));
      return L[((int)(em*0.5f+y*0.25f-t*22))&63]; }
    case 4: { float f=0.5f+0.5f*sinf(t*1.3f+em*0.4f); return col_lerp(HEX(0x3a8ad8),HEX(0xe8faff),0.35f+0.65f*f*f); }
    case 5: { float f=0.5f+0.5f*sinf(t*2.2f+em*0.7f+y*0.09f);
      if(em<16) return col_lerp(HEX(0x0a2a30),HEX(0x4ab8c0),f*f*0.7f);
      if(em<128) return col_lerp(HEX(0xa8507f),HEX(0xffb8e0),f); return col_lerp(HEX(0x1a8a7a),HEX(0x9affe8),f); }
    case 6: { float f=0.5f+0.5f*sinf(t*1.1f+em*0.06f+y*0.02f); if(em>=90) return col_lerp(HEX(0xa84a08),HEX(0xffc850),0.4f+0.6f*f); return col_lerp(HEX(0x5a2004),HEX(0xc87818),0.3f+0.5f*f); }
    case 7: { if(em<128){ float f=0.5f+0.5f*sinf(t*3.0f+em*1.7f); return col_lerp(HEX(0x30304a),HEX(0xe8eeff),f*f); }
      float f=0.5f+0.5f*sinf(t*3.0f-(em&127)*0.25f); return col_lerp(HEX(0x5a5aa0),HEX(0xffffff),f*f); }
    case 8: { float f=0.5f+0.5f*sinf(t*1.8f+em*0.9f); if(em>=128) return col_lerp(HEX(0x2a8a7a),HEX(0xd8fff8),f*f); return col_lerp(HEX(0x6a8a20),HEX(0xf6ffb0),f*f); }
    case 9: { float f=0.5f+0.5f*sinf(t*1.2f+em*0.5f+y*0.05f); if(em>=128){ float g=f*f; g*=g; return col_lerp(HEX(0x3a4a2a),HEX(0xe0f8b0),g); } return col_lerp(HEX(0x2a3a10),HEX(0xc8f050),f*f*0.9f); }
    case 10: { float f=0.5f+0.5f*sinf(t*1.6f+em*0.37f); if(em>=128) return col_lerp(HEX(0x1a5a7a),HEX(0xd8fcff),0.3f+0.7f*f*f);
      if(em>=100){ float g=0.8f+0.2f*sinf(t*13+em); return col_lerp(HEX(0xc87820),HEX(0xfff0b0),clampf(g-0.2f,0,1)); } return col_lerp(HEX(0x6a1a5a),HEX(0xffb0ea),0.3f+0.7f*f*f); }
    case 11: { if(em>=128){ float f=0.5f+0.5f*sinf(t*2.0f+em*0.05f); return col_lerp(HEX(0xffb030),HEX(0xfff8d0),f); }
      float f=0.5f+0.5f*sinf(t*1.4f-(em&127)*0.15f+y*0.02f); return col_lerp(HEX(0x9a6a18),HEX(0xffe89a),f*f); }
    default: { float f=0.5f+0.5f*sinf(t*2.0f-(em&127)*0.35f); f=f*f; if(em>=128) return col_lerp(HEX(0x8a4ad8),HEX(0xe8c8ff),0.5f+0.5f*sinf(t*3+em)); return col_lerp(HEX(0x3a1070),HEX(0xb070f0),f); }
  }
}

/* banniere de la liste des niveaux : falaises et sol du puits recadres (le centre s'assombrit comme un gouffre) */
void bg_banner(int biome,Sprite *out,int y0){
  { int v=hd_get_variant(); hd_set_variant(0); bg_build(biome); hd_set_variant(v); }
  int W=out->w, H=out->h, x0=(SCREEN_W-W)/2;
  const Sprite *Ls=scenery_tex(0), *Rs=scenery_tex(1); const uint8_t *eL=scenery_em(0), *eR=scenery_em(1);
  for(int y=0;y<H;y++){ int ty=(y+y0)%TEXH, sy=(y+y0)%SC_H;
    for(int x=0;x<W;x++){ int sx=x0+x; uint32_t c; int em=0;
      if(sx<PIT_X0){ if(Ls->px){ c=Ls->px[sy*SC_W+sx]; em=eL?eL[sy*SC_W+sx]:0; } else c=col_scale(BIOMES[biome].wall_a,0.8f+0.2f*((x*7+y*3)%5)/4.0f); }
      else if(sx>=PIT_X1){ int lx=sx-PIT_X1; if(Rs->px){ c=Rs->px[sy*SC_W+lx]; em=eR?eR[sy*SC_W+lx]:0; } else c=col_scale(BIOMES[biome].wall_a,0.8f+0.2f*((x*5+y*7)%5)/4.0f); }
      else { int fx=sx-PIT_X0; c=bg_tex.px[ty*PIT_W+fx]; em=bg_em[ty*PIT_W+fx];
        float d=fabsf((fx-PIT_W*0.5f)/(PIT_W*0.5f)); c=col_scale(c,0.40f+0.34f*d*d); if(em) em=0; }
      if(em) c=emis_col(em,c,ty);
      out->px[y*W+x]=c|0xFF000000u;
    }
  }
  /* ombre portee des falaises sur le bord du puits */
  for(int y=0;y<H;y++) for(int k=0;k<10;k++){ float a=(1-k/10.0f)*0.45f;
    int xl=PIT_X0-x0+k, xr=PIT_X1-x0-1-k;
    if(xl>=0&&xl<W) out->px[y*W+xl]=col_scale(out->px[y*W+xl],1-a)|0xFF000000u;
    if(xr>=0&&xr<W) out->px[y*W+xr]=col_scale(out->px[y*W+xr],1-a)|0xFF000000u; }
}
static void draw_floor(void){
  int scroll=(int)R.scroll;
  if(g_gpu){
    bool hd=hd_gpu_active(bg_biome);
    hd_frame(scroll,g_p25>0?SC_FW:0);
    for(int sy=0;sy<SCREEN_H;sy++){
      int ty=wy(sy-scroll); int o=sy*SCREEN_W+PIT_X0;
      uint32_t *dst=&g_screen[o], *de=&g_emis[o]; const uint32_t *src=&bg_tex.px[ty*PIT_W]; const uint8_t *em=&bg_em[ty*PIT_W];
      memcpy(&g_nrm[o],&bg_nrm[ty*PIT_W],PIT_W*4);
      for(int x=0;x<PIT_W;x++){
        if(em[x]){ uint32_t c=emis_col(em[x],src[x],ty); dst[x]= hd ? ((src[x]&0xFFFFFFu)|0x01000000u) : col_scale(c,0.3f); de[x]= (bg_biome==2||bg_biome==6||bg_biome==11) ? col_scale(c,0.68f) : c; }
        else dst[x]= hd ? ((src[x]&0xFFFFFFu)|0x01000000u) : src[x];
      }
    }
    return;
  }
  for(int sy=0;sy<SCREEN_H;sy++){
    int ty=wy(sy-OY-scroll);
    uint32_t *dst=&g_screen[sy*SCREEN_W+PIT_X0]; const uint32_t *src=&bg_tex.px[ty*PIT_W]; const uint8_t *em=&bg_em[ty*PIT_W];
    for(int x=0;x<PIT_W;x++){ int sx=x-OX; if(sx<0) sx=0; if(sx>=PIT_W) sx=PIT_W-1; dst[x]= em[sx] ? emis_col(em[sx],src[sx],ty) : src[sx]; }
  }
}
static void floor_emissive_light(void){
  int scroll=(int)R.scroll;
  for(int sy=0;sy<SCREEN_H;sy++){
    int ty=wy(sy-OY-scroll); const uint8_t *em=&bg_em[ty*PIT_W];
    for(int x=0;x<PIT_W;x++){ int sx=x-OX; if(sx<0) sx=0; if(sx>=PIT_W) sx=PIT_W-1; if(em[sx]){ uint32_t c=g_screen[sy*SCREEN_W+PIT_X0+x]; (void)c; light_floor(PIT_X0+x,sy,bg_biome==2?250:(bg_biome==3?230:260)); } }
  }
}
/* falaises : copie des bandes de decor (couleur, normales, emission) */
/* vue 2.5D : le sol s'assombrit au pied des parois (le puits est encaisse) */
static void floor_wall_shade(void){
  const int D=16;
  for(int y=0;y<SCREEN_H;y++){ uint32_t *row=&g_screen[y*SCREEN_W];
    for(int i=0;i<D;i++){ float k=1.0f-0.42f*(1.0f-i/(float)D)*(1.0f-i/(float)D);
      uint32_t *a=&row[PIT_X0+i], *b=&row[PIT_X1-1-i];
      *a=(*a&0xFF000000u)|(col_scale(*a,k)&0xFFFFFFu); *b=(*b&0xFF000000u)|(col_scale(*b,k)&0xFFFFFFu); } }
}
/* vue 2.5D : parois verticales du puits, entre le sol et le replat */
static void draw_faces(void){
  int scroll=(int)R.scroll;
  bool hd=g_gpu && hd_gpu_active(bg_biome);
  for(int side=0;side<2;side++){
    const Sprite *t=scenery_face(side); if(!t->px) continue;
    const uint32_t *nr=scenery_face_nrm(side); const uint8_t *em=scenery_face_em(side);
    uint32_t code=(uint32_t)(4+side)<<24;
    for(int y=0;y<SCREEN_H;y++){
      int ty=(((y-(g_gpu?0:OY)-scroll)%SC_H)+SC_H)%SC_H;
      for(int u=0;u<SC_FW;u++){
        int x= side==0 ? PIT_X0-1-u : PIT_X1+u; int o=y*SCREEN_W+x, i=ty*SC_FW+u;
        uint32_t c=t->px[i];
        if(em[i]){ uint32_t e=emis_col(em[i],c,ty); if(g_gpu){ g_screen[o]= hd ? ((c&0xFFFFFFu)|code) : col_scale(e,0.3f); g_emis[o]=e; } else g_screen[o]=e; }
        else { g_screen[o]= hd ? ((c&0xFFFFFFu)|code) : c; if(g_gpu) g_emis[o]=0; }
        if(g_gpu){ g_nrm[o]=nr[i]; g_occ[o]=0; }
      }
    }
  }
}
static void draw_scenery(void){
  int scroll=(int)R.scroll;
  bool hd=g_gpu && hd_gpu_active(bg_biome);
  for(int side=0;side<2;side++){
    const Sprite *t=scenery_tex(side); if(!t->px) continue;
    uint32_t code=(uint32_t)(2+side)<<24;
    const uint32_t *nr=scenery_nrm(side); const uint8_t *em=scenery_em(side);
    /* vue 2.5D : le decor commence en haut de la paroi (on le decale vers l'exterieur, rien n'est cache) */
    int sh = g_p25>0 ? SC_FW : 0;
    int x0 = side==0 ? 0 : PIT_X1+sh, n=SC_W-sh, c0 = side==0 ? sh : 0;
    for(int y=0;y<SCREEN_H;y++){
      int ty=(((y-(g_gpu?0:OY)-scroll)%SC_H)+SC_H)%SC_H;
      const uint32_t *row=&t->px[ty*SC_W+c0]; const uint8_t *erow=&em[ty*SC_W+c0];
      int o=y*SCREEN_W+x0;
      if(g_gpu) memcpy(&g_nrm[o],&nr[ty*SC_W+c0],(size_t)n*4);
      for(int x=0;x<n;x++){
        if(erow[x]){ uint32_t e=emis_col(erow[x],row[x],ty); if(g_gpu){ g_screen[o+x]= hd ? ((row[x]&0xFFFFFFu)|code) : col_scale(e,0.3f); g_emis[o+x]= (bg_biome==2||bg_biome==6||bg_biome==11) ? col_scale(e,0.68f) : e; } else g_screen[o+x]=e; }
        else g_screen[o+x]= hd ? ((row[x]&0xFFFFFFu)|code) : row[x];
      }
    }
  }
}
static const uint32_t TORCHC[BIOME_COUNT][3]={
  {HEX(0xc0300a),HEX(0xff8a22),HEX(0xffe080)},{HEX(0xc0300a),HEX(0xff8a22),HEX(0xffe080)},{HEX(0xc0300a),HEX(0xff8a22),HEX(0xffe080)},{HEX(0xc0300a),HEX(0xff8a22),HEX(0xffe080)},
  {HEX(0x2a5ab8),HEX(0x6ad8ff),HEX(0xe8faff)},{HEX(0x1a7a6a),HEX(0x5af0d0),HEX(0xd8fff4)},{HEX(0xc0600a),HEX(0xffb030),HEX(0xfff0a0)},{HEX(0x4a3aa8),HEX(0xb0a0ff),HEX(0xffffff)},
  {HEX(0x5a9a1a),HEX(0xd8ff6a),HEX(0xfaffd8)},{HEX(0x3a6a10),HEX(0xb8e040),HEX(0xf0ffb0)},{HEX(0x9a2a8a),HEX(0xff8ad8),HEX(0xffe8fa)},{HEX(0xc0500a),HEX(0xffc040),HEX(0xfff4c0)}};
static const uint32_t TORCHL[BIOME_COUNT]={HEX(0xff9a4a),HEX(0xff9a4a),HEX(0xff9a4a),HEX(0xff9a4a),HEX(0x8ad8ff),HEX(0x6af0d0),HEX(0xffb040),HEX(0xb8b0ff),HEX(0xd8ff8a),HEX(0xb8e060),HEX(0xff9ae0),HEX(0xffc860)};
static void torch_pos(int side,int k,float *x,float *y){ *x = side==0 ? PIT_X0-9 : PIT_X1+8; *y=(float)(((SC_TORCH_Y[side][k]+(int)R.scroll)%SC_H+SC_H)%SC_H)+OY; }
static void draw_torches(void){
  int ly=layer_current(); if(g_gpu) layer(LY_EMIS);
  for(int side=0;side<2;side++) for(int k=0;k<3;k++){
    float tx,ty; torch_pos(side,k,&tx,&ty); if(ty<-10||ty>SCREEN_H+10) continue;
    float f=sinf(g_time*13+k*2+side*3)*0.5f+0.5f; int fh=8+(int)(f*3.0f);
    int ix=(int)tx+OX, iy=(int)ty-3;
    const uint32_t *tc=TORCHC[bg_biome];
    gfx_tri((float)ix-3.5f,(float)iy+1,(float)ix+4.5f,(float)iy+1,(float)ix+0.5f+sinf(g_time*9+k)*1.6f,(float)iy-fh-2,tc[0]);
    gfx_tri((float)ix-2.5f,(float)iy,(float)ix+3.5f,(float)iy,(float)ix+0.5f+sinf(g_time*11+k)*1.0f,(float)iy-fh,tc[1]);
    gfx_tri((float)ix-1.0f,(float)iy,(float)ix+2.0f,(float)iy,(float)ix+0.5f+sinf(g_time*7+k)*0.5f,(float)iy-fh+3,tc[2]);
    gfx_pset(ix,iy-1,HEX(0xffffff));
  }
  if(g_gpu) layer(ly);
}
static void torch_glows(void){
  for(int side=0;side<2;side++) for(int k=0;k<3;k++){
    float tx,ty; torch_pos(side,k,&tx,&ty); if(ty<-30||ty>SCREEN_H+30) continue;
    float f=0.8f+0.2f*sinf(g_time*11+k*1.7f+side);
    if(g_gpu){
      fx_glow(tx+0.5f,ty-7,20,TORCHC[bg_biome][1],0.16f*f); fx_glow(tx+0.5f,ty-7,7,TORCHC[bg_biome][2],0.32f*f);
      if(rirand(8)==0) fx_disc(tx+0.5f+rfrange(-1.5f,1.5f),ty-12-rfrand()*7,0.55f,TORCHC[bg_biome][2],1.4f);
      continue;
    }
    gfx_glow(tx+OX+0.5f,ty-8,16,TORCHL[bg_biome],0.5f*f);
    if(rirand(10)==0){ gfx_padd((int)tx+OX+(rirand(3)-1),(int)ty-12-rirand(6),HEX(0xffc060),200); }
  }
}

static void curio_lights(void);
static void world_to_screen(float x,float y,float *X,float *Y);
static void add_scene_lights(void){
  for(int side=0;side<2;side++) for(int k=0;k<3;k++){
    float tx,ty; torch_pos(side,k,&tx,&ty); if(ty<-80||ty>SCREEN_H+80) continue;
    float f=0.82f+0.18f*sinf(g_time*11+k*1.7f+side)+0.05f*sinf(g_time*31+k);
    light_add(tx+OX,ty-5,84,TORCHL[bg_biome],0.95f*f);
  }
  for(int i=0;i<nglow;i++){
    float sy=fmodf(glow_y[i]+R.scroll,(float)TEXH); if(sy>SCREEN_H+30) sy-=TEXH;
    if(sy<-30||sy>SCREEN_H+30) continue;
    float pulse = (bg_biome==2||bg_biome==6) ? 0.55f+0.15f*sinf(g_time*3+i) : (bg_biome==0 ? 0.5f+0.08f*sinf(g_time*15+i) : 0.42f+0.1f*sinf(g_time*1.6f+i));
    light_add(PIT_X0+glow_x[i]+OX,sy+OY,glow_r[i],glow_c[i],pulse);
  }
  { const float *gx,*gy,*gr; const uint32_t *gc; int n=scenery_glows(&gx,&gy,&gr,&gc);
    for(int i=0;i<n;i++){ float sy=fmodf(gy[i]+R.scroll,(float)SC_H); if(sy>SCREEN_H+30) sy-=SC_H; if(sy<-30||sy>SCREEN_H+30) continue;
      float gxx=gx[i]; if(g_p25>0) gxx+= gx[i]<PIT_X0 ? -SC_FW : SC_FW;   /* decor decale par la paroi */
      light_add(gxx+OX,sy+OY,gr[i],gc[i],0.45f+0.1f*sinf(g_time*1.7f+i)); } }
  bool fog=run_has_mod(MOD_BROUILLARD);
  light_add(R.px+OX,R.py-8+OY,fog?62:86,HEX(0xffe0b0),0.7f);
  for(int i=0;i<MAX_BALLS;i++){ Ball *b=&R.balls_e[i]; if(!b->on||b->state==BS_READY||b->state==BS_RETURN) continue; light_add(b->x+OX,b->y+OY,IS_EVO(b->kind)?38:(b->temp?12:24),BALLS[b->kind].col,b->state==BS_RETURN?0.35f:0.75f); }
  for(int i=0;i<MAX_BUL;i++){ Bullet *u=&R.bul[i]; if(u->on) light_add(u->x+OX,u->y-(BUL_LOB(u)?u->tz:0)+OY,14+u->r*2,u->col,0.6f); }
  for(int i=0;i<MAX_PICK;i++){ Pickup *p=&R.pick[i]; if(p->on && p->kind<=PK_GEM3) light_add(p->x+OX,p->y+OY,10,HEX(0x6ad8ff),0.35f); }
  for(int i=0;i<MAX_FLASH;i++){ Flash *f=&R.flashes[i]; if(f->on) light_add(f->x+OX,f->y+OY,f->r,f->c,f->inten*(f->life/f->max)); }
  for(int i=0;i<MAX_BEAM;i++){ Beam *b=&R.beams[i]; if(!b->on) continue; for(float y=b->y1;y>b->y0;y-=36) light_add(b->x+OX,y+OY,30,b->c,0.8f*b->life/b->max); }
  for(int i=0;i<MAX_EN;i++){ Enemy *e=&R.en[i]; if(!e->on) continue;
    if(e->burn>0) light_add(e->x+OX,e->y+OY,18,HEX(0xff8a2a),0.45f);
    if(e->kind==EK_MAGMITE||e->kind==EK_DIABLOTIN||e->kind==EK_CHAUVE_FEU||e->kind==EK_CRACHEUR) light_add(e->x+OX,e->y+OY,22,HEX(0xff7a22),0.45f);
    if(e->kind==EK_OBELISQUE||e->kind==EK_SPECTRE||e->kind==EK_SENTVIDE) light_add(e->x+OX,e->y+OY,24,HEX(0xc08aff),0.45f);
    if(e->kind==EK_FOLLET) light_add(e->x+OX,e->y+OY,30,HEX(0x63d6f2),0.7f);
    if(e->kind==EK_ESPRIT) light_add(e->x+OX,e->y+OY,28,HEX(0x9ae8ff),0.6f);
    if(e->kind==EK_MEDUSE) light_add(e->x+OX,e->y+OY,24,HEX(0xe868c0),0.55f);
    if(e->kind==EK_GARDIENNE||e->kind==EK_COCON) light_add(e->x+OX,e->y+OY,22,HEX(0xffb030),0.45f);
    if(e->kind==EK_HERAUT||e->kind==EK_ECHO||e->kind==EK_COLOSSE_NEANT) light_add(e->x+OX,e->y+OY,26,HEX(0xd0d8ff),0.45f);
    if(e->champ) light_add(e->x+OX,e->y+OY,26,HEX(0xffd84a),0.3f);
    if(g_gpu && e->flash>0) light_add(e->x,e->y,24,HEX(0xfff0d8),minf(1.0f,e->flash*10)*0.55f);   /* eclair d'impact */
  }
  for(int i=0;i<MAX_ZONES;i++){ Zone *z=&R.zones[i]; if(!z->on) continue;
    if(z->kind==ZN_WARN) light_add(z->x+OX,ZONE_TOP+40,60,HEX(0xff3030),0.5f*(0.5f+0.5f*sinf(g_time*30)));
    if(z->kind==ZN_LAVA||z->kind==ZN_FIRE) light_add(z->x+OX,z->y+OY,z->r*2.4f,HEX(0xff7a22),0.8f*minf(1,z->life));
    if(z->kind==ZN_AMBER) light_add(z->x+OX,z->y+OY,z->r*2.0f,z->aux?HEX(0x8aa030):HEX(0xffb030),0.45f*minf(1,z->life));
    if(z->kind==ZN_ACIDE) light_add(z->x+OX,z->y+OY,z->r*1.8f,HEX(0xb8f040),0.35f*minf(1,z->life));
    if(z->kind==ZN_ECHO) light_add(z->x+OX,z->y+OY,z->r*1.4f,z->aux?HEX(0xd8d0ff):HEX(0xa898ff),0.3f);
  }
  if(R.boss.on){
    static const uint32_t BC[BOSS_COUNT]={HEX(0xffb080),HEX(0xf5aac8),HEX(0xff8a2a),HEX(0xc08aff),HEX(0x9ae8ff),HEX(0x5af0d0),HEX(0xffb030),HEX(0xe0f0ff),HEX(0xd8ff9a),HEX(0xb8d040),HEX(0xff8ad8),HEX(0xffc040)};
    if(!R.boss.dive) light_add(R.boss.x+OX,R.boss.y+OY,110,BC[R.boss.type],0.38f+(R.boss.dying>0?0.8f:0));
    if(R.boss.laser_st==2) for(int y=(int)R.boss.y;y<SCREEN_H;y+=40) light_add(R.boss.laser_x+OX,y+OY,46,HEX(0xd090ff),1.2f);
  }
  if(R.danger>0) light_add((PIT_X0+PIT_X1)*0.5f,FLOOR_Y,220,HEX(0xff2020),R.danger*0.8f);
  curio_lights();
}

/* ---------------- curiosites du decor (v8) ---------------- */
static void draw_curios(void){
  for(int i=0;i<MAX_CURIO;i++){ const Curio *c=&R.cur[i]; if(!c->on) continue;
    int ax,ay; const Sprite *s=curio_sprite(R.biome,c->kind,&ax,&ay);
    float wob= c->wob>0 ? sinf(c->t*42)*c->wob*1.6f : 0;
    float x=c->x+OX+wob, y=c->y+OY;
    gfx_ellipse_a((int)x,(int)y+1,s->w-3,3,HEX(0x000000),95);
    if(c->kind==CU_PIEGE){ float p=0.5f+0.5f*sinf(g_time*5+i); uint32_t wc= R.biome==4||R.biome==7 ? HEX(0x8ad8ff) : (R.biome==1||R.biome==8||R.biome==9 ? HEX(0xc8ff6a) : HEX(0xff5a2a));
      if(g_gpu) fx_glow(x,y-2,16,wc,0.10f+0.10f*p); else gfx_ellipse_a((int)x,(int)y,s->w-1,4,wc,(int)(30+40*p)); }
    DrawOpt o=dopt(); o.sx=o.sy=2;
    if(c->flash>0){ o.flash=HEX(0xfff4d8); o.flash_a=(int)(clampf(c->flash/0.12f,0,1)*210); }
    draw_spr_c(s,x+(s->w*0.5f-ax)*2.0f,y+(s->h*0.5f-ay)*2.0f,&o);
    if(c->kind==CU_TRESOR){
      float ph=fmodf(g_time*0.7f+i*0.37f,1.0f);   /* eclat qui court sur le tresor */
      if(ph<0.25f){ float k=ph/0.25f; float gx=x-s->w*0.6f+k*s->w*1.2f, gy=y-s->h*1.2f+k*4; if(g_gpu) fx_star(gx,gy,3.0f,HEX(0xfff4c0),0.9f*sinf(k*PI)); else gfx_padd((int)gx,(int)gy,HEX(0xfff4c0),(int)(220*sinf(k*PI))); }
      if(c->hits<c->hmax) for(int k=0;k<c->hmax;k++){ int px=(int)x-(c->hmax*4)/2+k*4, py=(int)(y-s->h*2.0f-4); gfx_rect(px,py,3,3,HEX(0x0a0810)); if(k<c->hits) gfx_rect(px,py,2,2,HEX(0xffd84a)); }
    }
  }
}
static void curio_lights(void){
  for(int i=0;i<MAX_CURIO;i++){ const Curio *c=&R.cur[i]; if(!c->on) continue;
    if(c->kind==CU_PIEGE){ uint32_t wc= R.biome==4||R.biome==7 ? HEX(0x8ad8ff) : (R.biome==1||R.biome==8||R.biome==9 ? HEX(0xc8ff6a) : HEX(0xff7a3a));
      light_add(c->x+OX,c->y-8+OY,34,wc,0.35f+0.2f*sinf(g_time*5+i)); }
    else light_add(c->x+OX,c->y-10+OY,30,HEX(0xffe0a0),0.3f);
  }
}
/* nom de la curiosite quand elle apparait */
static void draw_curio_labels(void){
  for(int i=0;i<MAX_CURIO;i++){ const Curio *c=&R.cur[i]; if(!c->on || c->lab<=0 || c->lab>3.2f) continue;
    int ax,ay; const Sprite *s=curio_sprite(R.biome,c->kind,&ax,&ay); (void)ax;
    float tx,ty; world_to_screen(c->x+OX,c->y-ay*2.0f-8+OY,&tx,&ty);
    if(c->lab>2.7f && ((int)(c->lab*14))%2) continue;   /* clignote avant de disparaitre */
    uint32_t col= c->kind==CU_TRESOR ? HEX(0xffd84a) : HEX(0xff8a5a);
    text_draw((int)tx,(int)ty-(c->hits<c->hmax?6:0),curio_name(R.biome,c->kind),col,1,AL_CENTER,TX_OUTLINE); (void)s;
  }
}

/* ---------------- entites ---------------- */
static void shadow(float x,float y,int rx,int a){ gfx_ellipse_a((int)(x+OX),(int)(y+OY),rx,maxi(1,rx/3),HEX(0x000000),a); }
static void draw_pickups(void){
  for(int i=0;i<MAX_PICK;i++){
    Pickup *p=&R.pick[i]; if(!p->on) continue;
    if(p->t>11 && ((int)(p->t*10))%2) continue;
    float bob=p->vy==0?sinf(g_time*4+i)*1.0f:0;
    int x=(int)(p->x+OX), y=(int)(p->y+OY+bob);
    if(p->vy==0) gfx_ellipse_a(x,(int)(p->y+OY)+3,3,1,HEX(0x000000),90);
    if(g_gpu && p->kind!=PK_HEART){ float tw=sinf(g_time*3.1f+i*1.7f); if(tw>0.6f){ uint32_t sc= p->kind==PK_COIN?HEX(0xfff0a0):(p->kind==PK_GEM2?HEX(0xd8ffc0):(p->kind==PK_GEM3?HEX(0xf0d0ff):HEX(0xd8f8ff)));
        fx_star(x+0.5f,y-1.5f,2.6f+(p->kind==PK_GEM3?1.4f:0),sc,(tw-0.6f)*2.2f); } }
    switch(p->kind){
      case PK_GEM: gfx_pset(x,y-2,HEX(0xc8f4ff)); gfx_rect(x-1,y-1,3,2,HEX(0x6ad8ff)); gfx_pset(x,y+1,HEX(0x3a8ad8)); break;
      case PK_GEM2: gfx_pset(x,y-3,HEX(0xd8ffd0)); gfx_rect(x-1,y-2,3,1,HEX(0x9af05a)); gfx_rect(x-2,y-1,5,2,HEX(0x9af05a)); gfx_rect(x-1,y+1,3,1,HEX(0x3a8a3a)); gfx_pset(x,y+2,HEX(0x3a8a3a)); gfx_pset(x-1,y-1,HEX(0xffffff)); break;
      case PK_GEM3: gfx_tri((float)x,(float)y-5,(float)x-4,(float)y,(float)x+4,(float)y,HEX(0xd090ff)); gfx_tri((float)x-4,(float)y,(float)x+4,(float)y,(float)x,(float)y+5,HEX(0x8a4ad8)); gfx_pset(x-1,y-2,HEX(0xffffff)); break;
      case PK_COIN: { int w=(int)(fabsf(sinf(g_time*5+i))*2.2f+0.5f); gfx_rect(x-w,y-2,w*2+1,5,HEX(0xc99a2e)); gfx_rect(x-w,y-2,maxi(1,w*2),4,HEX(0xf9dc5c)); if(w>=1) gfx_pset(x-w,y-2,HEX(0xfff4c0)); } break;
      default: { int s=((int)(g_time*4))&1; if(g_gpu) fx_glow(x+0.5f,y+0.5f,6,HEX(0xff4a4a),0.25f); gfx_rect(x-2,y-1,2,2,HEX(0xe0453a)); gfx_rect(x+1,y-1,2,2,HEX(0xe0453a)); gfx_rect(x-2,y,5,2,HEX(0xe0453a)); gfx_rect(x-1,y+2,3,1,HEX(0xe0453a)); gfx_pset(x,y+3,HEX(0xe0453a)); gfx_pset(x-2,y-1,s?HEX(0xffffff):HEX(0xffa0a0)); } break;
    }
  }
}
static const uint32_t CHAMP_COL[CHAMP_COUNT]={0,HEX(0xffe04a),HEX(0xff5a4a),HEX(0x7af07a),HEX(0x5ab8ff),HEX(0xff8a2a),HEX(0xc08aff),HEX(0x8bffb0),HEX(0xd8e8ff)};

/* ---------- socles : chaque monstre au sol se tient sur un bloc de pierre (comme une brique) ---------- */
typedef struct { uint32_t line, hi, mid, inner, deep, front, frontd, deco; } PlinthPal;
static const PlinthPal PLP[BIOME_COUNT]={
  {HEX(0x141118),HEX(0x9a92a4),HEX(0x625a6c),HEX(0x403848),HEX(0x2c2632),HEX(0x4a4252),HEX(0x302a38),HEX(0xc8bca4)},   /* catacombes : pierre */
  {HEX(0x0c120e),HEX(0x8aa878),HEX(0x4e6c4a),HEX(0x2c3c2e),HEX(0x1e2a20),HEX(0x34463a),HEX(0x222e26),HEX(0x5aa846)},   /* foret : pierre moussue */
  {HEX(0x0e0809),HEX(0x9a7464),HEX(0x5e3e36),HEX(0x34222a),HEX(0x22161a),HEX(0x3e2a2c),HEX(0x2a1a1c),HEX(0xff7a22)},   /* forges : fonte */
  {HEX(0x0c0614),HEX(0xa888d0),HEX(0x6a4e8a),HEX(0x3a2850),HEX(0x26183a),HEX(0x46325e),HEX(0x2e1e42),HEX(0xd2b0f5)},   /* coeur : pierre violette */
  {HEX(0x1a1a34),HEX(0xd2f0ff),HEX(0x86b0e6),HEX(0x343a6c),HEX(0x242850),HEX(0x6c88c4),HEX(0x4c62a0),HEX(0xf0faff)},   /* givre : glace */
  {HEX(0x081416),HEX(0x9ae0cc),HEX(0x4a8a7a),HEX(0x24443f),HEX(0x16302c),HEX(0x2e5a52),HEX(0x1e3e38),HEX(0x5af0d0)},   /* cite : pierre verte */
  {HEX(0x160c04),HEX(0xffd884),HEX(0xc08a3a),HEX(0x6a4418),HEX(0x4a2e10),HEX(0x8a5a22),HEX(0x5e3c14),HEX(0xffb030)},   /* ruche : cire */
  {HEX(0x06050a),HEX(0x9a94d0),HEX(0x4e4878),HEX(0x1e1a2c),HEX(0x14111e),HEX(0x2a2640),HEX(0x1a172a),HEX(0xe0f0ff)},   /* neant : obsidienne */
  {HEX(0x08120e),HEX(0x8ac8a0),HEX(0x4a8a6a),HEX(0x24443a),HEX(0x162e26),HEX(0x2e5a48),HEX(0x1e3e32),HEX(0xd8ff6a)},   /* jardin : pierre moussue */
  {HEX(0x0a0906),HEX(0x8a8a5a),HEX(0x56543a),HEX(0x2e2c1e),HEX(0x1e1c12),HEX(0x3a3824),HEX(0x26241a),HEX(0xb8e040)},   /* marais : souche */
  {HEX(0x0a0810),HEX(0x9a90b8),HEX(0x5a5272),HEX(0x2a2638),HEX(0x1a1824),HEX(0x3a3448),HEX(0x26222e),HEX(0xff8ad8)},   /* mines : ardoise */
  {HEX(0x140a04),HEX(0xf0d090),HEX(0xb08a4a),HEX(0x6e5428),HEX(0x4a3618),HEX(0x8a6a36),HEX(0x5e4620),HEX(0xffd84a)},   /* sanctuaire : gres */
};
static Sprite plinth[3];
static const int PL_W[3]={22,30,56}, PL_TH[3]={14,18,28}, PL_FH[3]={5,7,9};
static uint32_t nrmc(float nx,float ny,float nz,int sp){ float l=sqrtf(nx*nx+ny*ny+nz*nz); nx/=l; ny/=l; nz/=l;
  return ((uint32_t)sp<<24)|((uint32_t)clampi((int)(nx*127.5f+128),0,255)<<16)|((uint32_t)clampi((int)(ny*127.5f+128),0,255)<<8)|(uint32_t)clampi((int)(nz*127.5f+128),0,255); }
static void plinth_build(int biome){
  const PlinthPal *pp=&PLP[biome];
  for(int k=0;k<3;k++){
    int W=PL_W[k], TH=PL_TH[k], FH=PL_FH[k], SWd=W+2, SHt=TH+FH+2;
    if(plinth[k].px){ free(plinth[k].px); free(plinth[k].nm); }
    plinth[k]=spr_alloc(SWd,SHt); plinth[k].nm=(uint32_t*)calloc((size_t)SWd*SHt,4);
    uint32_t *px=plinth[k].px, *nm=plinth[k].nm;
    #define PP(x,y,c,n) do{ int _x=(x),_y=(y); if(_x>=0&&_x<SWd&&_y>=0&&_y<SHt){ px[_y*SWd+_x]=(c)|0xFF000000u; nm[_y*SWd+_x]=(n); } }while(0)
    uint32_t NF=nrmc(0,0,1,40), NU=nrmc(0,-0.6f,0.8f,130), NL=nrmc(-0.6f,0,0.8f,110), NR=nrmc(0.6f,0,0.8f,70), ND=nrmc(0,0.6f,0.8f,60), NFR=nrmc(0,0.78f,0.62f,70);
    for(int y=0;y<SHt;y++) for(int x=0;x<SWd;x++) PP(x,y,pp->line,NF);
    /* face superieure : rebord clair en haut a gauche, sombre en bas a droite, creux au centre */
    for(int y=1;y<=TH;y++) for(int x=1;x<=W;x++){
      uint32_t c=pp->inner, n=NF;
      if(y==1){ c=pp->hi; n=NU; } else if(x==1){ c=pp->hi; n=NL; }
      else if(y==TH){ c=pp->mid; n=ND; } else if(x==W){ c=pp->mid; n=NR; }
      else if(y==2 || x==2) { c=pp->deep; }
      else if(y==3 || x==3) c=col_lerp(pp->deep,pp->inner,0.5f);
      else if(((x*7+y*13)%17)==0) c=col_lerp(pp->inner,pp->deep,0.4f);   /* grain de la pierre */
      PP(x,y,c,n);
    }
    /* face avant : arete claire, jointures verticales */
    for(int y=TH+1;y<=TH+FH;y++) for(int x=1;x<=W;x++){
      uint32_t c = y==TH+1 ? pp->mid : pp->front;
      if(y>TH+1 && (x%9)==0) c=pp->frontd;
      if(y>TH+1 && y<TH+FH && ((x*5+y*3)%11)==0) c=col_lerp(pp->front,pp->frontd,0.5f);
      if(y==TH+FH) c=pp->frontd;
      PP(x,y,c,NFR);
    }
    /* decor du biome sur les coins (givre, mousse, cristaux...) */
    for(int q=0;q<3+k;q++){ int x=1+((q*5+k*3)%W), y=1+((q*3)%2);
      PP(x,y,pp->deco,NU); if(biome==4||biome==1) PP(x+1,y,col_lerp(pp->deco,pp->hi,0.5f),NU); }
    if(biome==4) for(int x=2;x<W;x+=3+(x%2)){ int l=1+((x*7)%3); for(int j=0;j<l;j++) PP(x,TH+1+j,j==0?pp->deco:pp->hi,NFR); }   /* glacons */
    #undef PP
  }
}
static void draw_plinth(const Enemy *e,float x,float feet,int alpha){
  float ew=e->w/EN_SCALE; int k = ew<=9 ? 0 : (ew>=20 ? 2 : 1);
  const Sprite *s=&plinth[k]; if(!s->px) return;
  int W=PL_W[k], TH=PL_TH[k];
  int x0=(int)floorf(x+0.5f)-(W+2)/2, y0=(int)floorf(feet+0.5f)-(int)(TH*0.62f)-1;
  /* ombre de contact */
  gfx_ellipse_a((int)x,y0+s->h,W/2+2,2,HEX(0x000000),alpha*90/255);
  if(alpha>=255) draw_spr(s,x0,y0,DS_NOOCC);
  else { DrawOpt o=dopt(); o.alpha=alpha; o.flags=DS_NOOCC; draw_spr_c(s,x0+s->w/2.0f-0.5f+0.5f,y0+s->h/2.0f,&o); }
  /* vue 2.5D : la face laterale tournee vers l'axe du puits se voit (plus large loin du centre) */
  if(g_p25>0 && alpha>=128){
    float off=(x-(PIT_X0+PIT_X1)*0.5f)/((PIT_X1-PIT_X0)*0.5f);
    int sw=(int)(fabsf(off)*4.0f*g_p25+0.5f);
    if(sw>0){
      const PlinthPal *pp=&PLP[bg_biome];
      int TH2=TH+PL_FH[k], xs= off<0 ? x0+W+1 : x0-sw;   /* cote droit si le socle est a gauche, et inversement */
      uint32_t nside = off<0 ? nrmc(0.8f,0,0.6f,50) : nrmc(-0.8f,0,0.6f,50);
      for(int i=0;i<sw;i++){
        int col = off<0 ? xs+i : xs+sw-1-i;         /* i : distance a l'arete du socle */
        for(int j=1+i;j<=TH2;j++){
          int py=y0+j; uint32_t c = j==1+i ? pp->mid : (j>TH ? pp->frontd : col_lerp(pp->front,pp->frontd,0.5f));
          if(j==TH2) c=pp->line;
          gfx_pset(col,py,c);
          if(g_gpu && gT==&g_scr && col>=0 && col<SCREEN_W && py>=0 && py<SCREEN_H) g_nrm[py*SCREEN_W+col]=nside;
        }
      }
    }
  }
  /* sang et eclats selon les degats subis */
  float dmg=1.0f-clampf(e->hp/e->hpmax,0,1);
  int ns=(int)(dmg*(k==2?14:8));
  uint32_t bc=ENEMIES[e->kind].blood;
  for(int i=0;i<ns;i++){ uint32_t h=(uint32_t)e->id*2654435761u+(uint32_t)i*40503u; h^=h>>15; h*=0x2c1b3c6du; h^=h>>12;
    int px=x0+1+(int)(h%(uint32_t)W), py=y0+1+(int)((h>>8)%(uint32_t)(TH+PL_FH[k]-1));
    gfx_pset(px,py,col_scale(bc,0.75f)); if(h&0x100000) gfx_pset(px+1,py,col_scale(bc,0.55f)); if(h&0x200000) gfx_pset(px,py+1,col_scale(bc,0.6f)); }
}
static bool has_plinth(const Enemy *e){
  const EnemyDef *d=&ENEMIES[e->kind];
  return !(d->flags&(EF_FLY|EF_DIVE)) && e->kind!=EK_TENTACULE && !e->guard;
}
/* position a l'ecran d'un point du jeu (camera + vue 2.5D) : pour ce qui se dessine dans l'interface */
static void world_to_screen(float x,float y,float *X,float *Y){
  if(g_gpu){ float z=GP.cam_zoom>0.05f?GP.cam_zoom:1; x=(x-GP.cam_x-GP.cam_cx)*z+GP.cam_cx; y=(y-GP.cam_y-GP.cam_cy)*z+GP.cam_cy; }
  p25_fwd(x,y,X,Y);
}
/* v10 : chaines entre monstres (bille de chaine, entrave) */
static void draw_links(void){
  for(int i=0;i<MAX_EN;i++){ const Enemy *e=&R.en[i]; if(!e->on || e->link_t<=0) continue;
    const Enemy *o=NULL; for(int k=0;k<MAX_EN;k++) if(R.en[k].on && R.en[k].id==e->link_id){ o=&R.en[k]; break; }
    if(!o || (o->link_id==e->id && o->id<e->id)) continue;
    float dx=o->x-e->x, dy=o->y-e->y, d=sqrtf(dx*dx+dy*dy); int n=(int)(d/5)+1; float a=clampf(e->link_t,0,1);
    uint32_t c= e->link_k>=0.55f ? HEX(0x9ac8ff) : HEX(0xd8dce8);
    for(int k=0;k<=n;k++){ float t=(float)k/n; float sag=sinf(t*PI)*3; float x=e->x+dx*t, y=e->y+dy*t+sag;
      if(g_gpu) fx_disc(x,y,(k&1)?0.8f:1.1f,c,0.8f*a); else gfx_pset((int)x,(int)y,c); } }
}
static void draw_enemy(Enemy *e){
  const EnemyDef *d=&ENEMIES[e->kind];
  bool fly=(d->flags&EF_FLY)!=0;
  float x=e->x+OX, y=e->y+OY-e->kick;
  float bob = fly ? sinf(e->t*4)*1.5f : 0;
  if(e->freeze_t>0) bob=0;
  if((d->flags&EF_DIVE) && e->sstate==1){   /* sous l'eau : seulement des remous */
    gfx_ellipse_a((int)x,(int)y+1,5,2,HEX(0x061412),110);
    for(int k=0;k<2;k++){ float ph=fmodf(e->t*1.8f+k*0.5f,1.0f);
      if(g_gpu) fx_ring(x,y,3+ph*7,0.7f,HEX(0x9af0e0),(1-ph)*0.6f); else gfx_ring_a((int)x,(int)y,(int)(3+ph*7),HEX(0x9af0e0),(int)((1-ph)*140),true); }
    return;
  }
  if(e->kind==EK_MANTE && e->sstate==1) x+=rfrange(-1,1);
  bool pl=has_plinth(e);
  if(pl){ int al=e->flee?150:255; if(e->spawn<1) al=(int)(al*e->spawn); draw_plinth(e,x,y+e->h/2,al); { float ew=e->w/EN_SCALE; y-=PL_TH[ew<=9?0:(ew>=20?2:1)]*0.28f; } }
  else shadow(e->x,e->y+e->h/2+(fly?9:0),(int)(e->w*(fly?0.4f:0.55f)),fly?60:100);
  if(e->mboss){ float pa=0.5f+0.5f*sinf(g_time*3+e->id); uint32_t mc= e->sstate?HEX(0xff6a2a):HEX(0xffc060);
    if(g_gpu){ fx_glow(x,y,maxf(e->w,e->h)*0.75f,mc,0.10f+0.06f*pa); fx_ring(x,y,maxf(e->w,e->h)*0.55f,1.4f,mc,0.25f+0.25f*pa); }
    else gfx_ring_a((int)x,(int)y,(int)(maxf(e->w,e->h)*0.55f),mc,(int)(60+60*pa),true); }
  if(e->champ){ float pa=0.5f+0.5f*sinf(g_time*5+e->id);
    if(g_gpu){ fx_ring(x,y+bob,maxf(e->w,e->h)*0.62f+1,1.2f,CHAMP_COL[e->champ],0.35f+0.45f*pa); fx_glow(x,y+bob,maxf(e->w,e->h)*0.9f,CHAMP_COL[e->champ],0.12f+0.1f*pa); }
    else gfx_ring_a((int)x,(int)(y+bob),(int)(maxf(e->w,e->h)*0.62f)+1,CHAMP_COL[e->champ],(int)(70+80*pa),true); }
  DrawOpt o=dopt();
  if(e->flash>0){ o.flash=HEX(0xffffff); o.flash_a=230; }
  if(e->freeze_t>0) o.tint=HEX(0x9ad8ff);
  else if(e->burn>0 && ((int)(g_time*12)&1)) o.tint=HEX(0xffb070);
  else if(e->poison>0) o.tint=HEX(0xb8f0a0);
  else if(e->acid>0) o.tint=e->acid>=3?HEX(0xe0ff90):HEX(0xf0ffc8);
  if(e->kind==EK_MANTE && e->sstate>=1){ o.flash=HEX(0xff4a4a); o.flash_a=e->sstate==1?(int)(120+100*sinf(g_time*40)):80; }
  if(e->champ==CHAMP_SPECTRAL) o.alpha= fmodf(e->t,3.0f)>2.0f ? 60 : 210;   /* v10 : spectral */
  if((d->flags&EF_PHASE) && fmodf(e->phase,3.2f)>2.2f) o.alpha=70;
  else if(d->flags&EF_PHASE) o.alpha=200;
  if(e->kind==EK_FOLLET) o.alpha=210;
  if(e->kind==EK_ECHO||e->kind==EK_ECHO_MINEUR) o.alpha=200+(int)(40*sinf(e->t*6));
  if(e->kind==EK_MEDUSE||e->kind==EK_ESPRIT) o.alpha=225;
  if(e->kind==EK_MURENE && sinf(e->t*5)>0) o.flags|=DS_FLIPX;
  if(e->flee){ o.alpha=mini(o.alpha,150); o.tint=HEX(0xa89ec0); }   /* s'enfuit devant le gardien */
  float sc=e->scale>0?e->scale:1;
  o.sx=o.sy=sc;
  if(fly) o.lift=9+bob;
  if(e->spawn<1){ o.sx*=0.6f+0.4f*e->spawn; o.sy*=0.6f+0.4f*e->spawn; }
  if(e->kind==EK_GELEE||e->kind==EK_GELEE_TOX||e->kind==EK_GELETTE||e->kind==EK_GELETTE_TOX){ float s=sinf(e->t*5)*0.08f; o.sx*=1+s; o.sy*=1-s; }
  if(e->kick>0){ o.sx*=1+e->kick*0.03f; o.sy*=1-e->kick*0.03f; }
  if(e->kind==EK_TENTACULE){
    uint32_t c1=e->flash>0?HEX(0xffffff):HEX(0xa35fe0), c2=HEX(0x4a2266);
    if(e->freeze_t>0) c1=HEX(0x9ad8ff);
    gfx_ellipse((int)x,(int)(y+e->h/2),6,2,HEX(0x1a0a1a));
    art_draw_tentacle(x,y+e->h/2,e->t,e->h+2,c1,c2,-PI*0.5f);
    if(e->guard){ if(g_gpu) fx_ring(x,y,12,1.2f,HEX(0xc08aff),0.45f); else gfx_ring_a((int)x,(int)y,12,HEX(0xc08aff),90,true); }
  } else {
    int spr=d->sprite;
    if(d->sprite2>=0 && ((int)(e->t*6))&1) spr=d->sprite2;
    if(e->kind==EK_SCARABEE) o.flags|=DS_FLIPY;
    const Sprite *s=&SPR[spr];
    if(!fly && SPR_WALK[spr][0].px && e->freeze_t<=0 && e->root_t<=0 && e->stun_t<=0 && e->spawn>=1){ int f=((int)(e->t*(e->champ==CHAMP_RAPIDE?11:7)))&3; if(f==1) s=&SPR_WALK[spr][0]; else if(f==3) s=&SPR_WALK[spr][1]; }
    draw_spr_c(s,x,y+bob,&o);
  }
  if(d->flags&EF_ORBS){
    for(int k=0;k<2;k++){ float ox=x+cosf(e->orb+k*PI)*13, oy=y+bob+sinf(e->orb+k*PI)*9;
      gfx_disc((int)ox,(int)oy,3,HEX(0x1a0a2a)); gfx_disc((int)ox,(int)oy,2,HEX(0x8a4ad8)); gfx_pset((int)ox-1,(int)oy-1,HEX(0xf0d8ff));
      if(g_gpu){ fx_glow(ox,oy,7,HEX(0x9a5aff),0.35f); fx_disc(ox,oy,1.4f,HEX(0xc89aff),0.5f); } }
  }
  if(e->freeze_t>0){ for(int k=0;k<4;k++){ int px=(int)(x-e->w/2+((e->id*7+k*5)%(int)maxf(1,e->w))), py=(int)(y-e->h/2+((e->id*3+k*7)%(int)maxf(1,e->h)));
      if(g_gpu) fx_star(px+0.5f,py+0.5f,1.6f+0.6f*sinf(g_time*7+k+e->id),HEX(0xd4f4ff),0.9f); else { gfx_pset(px,py,HEX(0xffffff)); gfx_pset(px+1,py,HEX(0xd4f4ff)); } } }
  if(e->mark_t>0){   /* v9 : marque de l'ombre (oeil violet au-dessus de la tete) */
    float my=y-e->h*0.5f-5+bob+sinf(g_time*4+e->id)*1.0f, a=minf(1,e->mark_t*2);
    if(g_gpu){ fx_glow(x,my,6,HEX(0x8a4aff),0.35f*a); fx_ring(x,my,3.2f,0.9f,HEX(0xc09aff),0.8f*a); fx_disc(x,my,1.0f,HEX(0xffffff),0.9f*a); }
    else { gfx_ring_a((int)x,(int)my,3,HEX(0xb08aff),(int)(200*a),true); gfx_pset((int)x,(int)my,HEX(0xffffff)); } }
  if(e->root_t>0){   /* v7 : racines enroulees autour du bas du corps */
    float k=minf(1,e->root_t*2.5f), by=y+e->h/2;
    for(int j=0;j<4;j++){ float sx=x+(j-1.5f)*e->w*0.26f, hh=e->h*(0.30f+0.10f*((e->id+j)%3))*k, cx=sx+sinf(j*2.1f+e->id)*2.5f;
      gfx_thick_line(sx,by+1,cx,by-hh*0.55f,0.9f,HEX(0x5a3a1c)); gfx_thick_line(cx,by-hh*0.55f,sx+(j<2?2.0f:-2.0f),by-hh,0.6f,HEX(0x7a5a2a));
      gfx_pset((int)(sx+(j<2?2:-2)),(int)(by-hh),HEX(0x8ac850)); gfx_pset((int)cx+1,(int)(by-hh*0.5f),HEX(0x6ab040)); } }
  if(e->stun_t>0) for(int k=0;k<3;k++){ float a=g_time*7+k*TAU/3; float sx=x+cosf(a)*6, sy=y-e->h/2-4+sinf(a)*1.8f;
    if(g_gpu) fx_star(sx,sy,1.7f,HEX(0xfff070),1.0f); else gfx_pset((int)sx,(int)sy,HEX(0xfff070)); }
  if(e->shield>0){ if(g_gpu) fx_ring(x,y,maxf(e->w,e->h)*0.6f+2,1.4f,HEX(0x5ab8ff),0.45f+0.3f*sinf(g_time*6)); else gfx_ring_a((int)x,(int)y,(int)(maxf(e->w,e->h)*0.6f)+2,HEX(0x5ab8ff),(int)(90+60*sinf(g_time*6)),true); }
  if(e->blind_t>0) for(int k=0;k<3;k++){ float a=g_time*5+k*TAU/3; if(g_gpu) fx_disc(x+cosf(a)*5,y-e->h/2-3+sinf(a)*1.5f,0.7f,HEX(0xfff4c0),1.2f); else gfx_pset((int)(x+cosf(a)*5),(int)(y-e->h/2-3+sinf(a)*1.5f),HEX(0xfff4c0)); }
  int ly=layer_current(); if(g_gpu) layer(LY_UI);
  float ux=x, uy=y; if(g_gpu){ float X,Y; world_to_screen(x,y-e->h/2,&X,&Y); ux=X; uy=Y+e->h/2; }   /* l'interface ne passe pas par la camera */
  if(e->hpbar_t>0 && e->hp<e->hpmax && e->hp>0 && !e->mboss){
    int bw=(int)maxf(10,e->w), bx=(int)(ux-bw/2), by=(int)(uy-e->h/2-5);
    gfx_rect(bx-1,by-1,bw+2,4,HEX(0x0a0810)); gfx_rect(bx,by,bw,2,HEX(0x3a1a1a));
    uint32_t hc=(d->flags&EF_ELITE)?HEX(0xffb040):(e->champ?CHAMP_COL[e->champ]:HEX(0xff4a4a));
    gfx_rect(bx,by,(int)(bw*clampf(e->hp/e->hpmax,0,1)),2,hc); gfx_rect(bx,by,(int)(bw*clampf(e->hp/e->hpmax,0,1)),1,col_lerp(hc,HEX(0xffffff),0.4f));
  }
  if(e->champ){ int cx=(int)ux, cy=(int)(uy-e->h/2-9); uint32_t c=CHAMP_COL[e->champ]; gfx_pset(cx-2,cy,c); gfx_pset(cx,cy-1,c); gfx_pset(cx+2,cy,c); gfx_hline(cx-2,cx+2,cy+1,c);
    if(e->champ<CHAMP_COUNT && e->y>-4) text_draw(cx,cy-10,CHAMP_NAMES[e->champ],c,1,AL_CENTER,TX_OUTLINE); }   /* v10 : pouvoir du champion */
  if(g_gpu) layer(ly);
  if(e->bleed>0) for(int k=0;k<mini(e->bleed,6);k++) gfx_pset((int)(x-5+k*2),(int)(y+e->h/2+2),HEX(0xff4a5a));
}

static void draw_companion(void){
  if(R.ch2<0) return;
  float x=R.comp_x+OX, y=R.comp_y+OY;
  gfx_ellipse_a((int)x,(int)y,10,3,HEX(0x000000),110);
  uint32_t hc=CHARS[R.ch2].col;
  if(g_gpu){ fx_ring(x,y,11,1.0f,hc,0.2f); } else gfx_ring_a((int)x,(int)y,11,hc,40,true);
  int spr=CHARS[R.ch2].sprite; const Sprite *s=&SPR[spr];
  bool moving=sqrf(R.comp_x-(R.px-16*R.facing))+sqrf(R.comp_y-(R.py+3))>20;
  if(moving && SPR_WALK[spr][0].px){ int f=((int)(R.comp_walk*9))&3; if(f==1) s=&SPR_WALK[spr][0]; else if(f==3) s=&SPR_WALK[spr][1]; }
  DrawOpt o=dopt(); if(R.comp_face<0) o.flags=DS_FLIPX;
  float bob = moving ? 0 : sinf(g_time*2.5f+1.3f)*0.6f;
  draw_spr_c(s,x,y-s->h/2-bob,&o);
}
static void draw_player(void){
  if(R.ch2>=0 && R.comp_y<=R.py) draw_companion();
  float x=R.px+OX, y=R.py+OY;
  gfx_ellipse_a((int)x,(int)y,12,3,HEX(0x000000),130);
  uint32_t hc=CHARS[R.ch].col;
  if(g_gpu){ fx_ring(x,y,14,1.1f,hc,0.28f); fx_glow(x,y,18,hc,0.08f); } else gfx_ring_a((int)x,(int)y,14,hc,60,true);
  if(R.iframes>0 && ((int)(R.iframes*16))&1) return;
  bool moving=fabsf(A.movex)>0.1f||fabsf(A.movey)>0.1f;
  int spr=CHARS[R.ch].sprite; const Sprite *s=&SPR[spr];
  if(moving && SPR_WALK[spr][0].px){ int f=((int)(R.walk_t*9))&3; if(f==1) s=&SPR_WALK[spr][0]; else if(f==3) s=&SPR_WALK[spr][1]; }
  float bob = moving ? 0 : sinf(g_time*2.5f)*0.6f;
  DrawOpt o=dopt(); if(R.facing<0) o.flags=DS_FLIPX;
  if(R.chill_t>0) o.tint=HEX(0xa8d8ff);
  if(R.hurt_t>0){ o.flash=HEX(0xff3030); o.flash_a=180; }
  if(R.rank>=10){ for(int k=0;k<3;k++){ float a=g_time*1.7f+k*TAU/3; float sx=x+cosf(a)*8, sy=y-8+sinf(a)*4-sinf(g_time*3+k)*3;
      if(g_gpu){ fx_star(sx,sy-6,1.8f,HEX(0xffe08a),0.6f+0.3f*sinf(g_time*5+k)); } else gfx_padd((int)sx,(int)sy-6,HEX(0xffe08a),200); }
    if(g_gpu) fx_glow(x,y-14,20,HEX(0xffc040),0.1f); }
  draw_spr_c(s,x,y-s->h/2-bob,&o);
  if(R.ch2>=0 && R.comp_y>R.py) draw_companion();
  if(R.amber_t>0){ gfx_ellipse_a((int)x,(int)y,6,2,HEX(0xffb030),150); }
  if(has_ch(CH_MAGNETISEUR)){ int a=40+(int)(20*sinf(g_time*4)); float mr=mag_radius();
    if(g_gpu){ uint32_t lc=col_lerp(HEX(0xe0453a),HEX(0x5a9aff),0.5f+0.5f*sinf(g_time*3)); fx_ring(x,FLOOR_Y,mr,1.0f,HEX(0x5a9aff),a/510.0f); fx_caps(x-mr,FLOOR_Y,x+mr,FLOOR_Y,0.6f,lc,0.7f); }
    else { gfx_ring_a((int)x,FLOOR_Y+OY,(int)mr,HEX(0x5a9aff),a/2,true); gfx_hline((int)(x-mr),(int)(x+mr),FLOOR_Y+OY,col_lerp(HEX(0xe0453a),HEX(0x5a9aff),0.5f+0.5f*sinf(g_time*3))); } }
}
static void draw_player_fx(void){
  float x=R.px+OX, y=R.py+OY-8;
  if(g_gpu){
    if(R.shield_up) for(int k=0;k<3;k++){ float a=g_time*2.2f+k*TAU/3; float px=x+cosf(a)*12, py=y+sinf(a)*7; fx_glow(px,py,6,HEX(0x5ab8ff),0.45f); fx_disc(px,py,1.1f,HEX(0xd8f4ff),1.3f); }
    if(R.shield_flash>0) fx_ring(x,y,14+(1-R.shield_flash)*10,1.6f,HEX(0x9ad8ff),R.shield_flash*1.1f);
    if(R.timeslow_t>0){ float k=minf(1,R.timeslow_t); fx_ring(x,y,16,1.2f,HEX(0x45c2a8),0.7f*k); float a=-g_time*3;
      fx_caps(x,y,x+cosf(a)*10,y+sinf(a)*10,0.5f,HEX(0x9af0e0),0.9f*k); fx_caps(x,y,x+cosf(a*0.08f)*6,y+sinf(a*0.08f)*6,0.5f,HEX(0x9af0e0),0.9f*k); fx_glow(x,y,22,HEX(0x45c2a8),0.15f*k); }
    return;
  }
  if(R.shield_up) for(int k=0;k<3;k++){ float a=g_time*2.2f+k*TAU/3; int px=(int)(x+cosf(a)*12), py=(int)(y+sinf(a)*7); gfx_padd(px,py,HEX(0x9ad8ff),240); gfx_padd(px+1,py,HEX(0x5ab8ff),160); gfx_padd(px,py-1,HEX(0x5ab8ff),160); }
  if(R.shield_flash>0) gfx_ring_a((int)x,(int)y,(int)(14+(1-R.shield_flash)*10),HEX(0x9ad8ff),(int)(R.shield_flash*230),true);
  if(R.timeslow_t>0){ float k=minf(1,R.timeslow_t); gfx_ring_a((int)x,(int)y,16,HEX(0x45c2a8),(int)(150*k),true); float a=-g_time*3; gfx_capsule_add(x,y,x+cosf(a)*10,y+sinf(a)*10,0.6f,HEX(0x9af0e0),(int)(200*k)); gfx_capsule_add(x,y,x+cosf(a*0.08f)*6,y+sinf(a*0.08f)*6,0.6f,HEX(0x9af0e0),(int)(200*k)); }
}

static void draw_aim(void){
  if(R.sub!=RS_PLAY && R.sub!=RS_INTRO) return;
  float ax=R.aimx-R.px, ay=R.aimy-(R.py-8); float d=sqrtf(ax*ax+ay*ay); if(d<1) return; ax/=d; ay/=d;
  if(ay>-0.26f){ ay=-0.26f; ax=signf(ax)*sqrtf(1-ay*ay); }
  if(g_gpu){
    for(int i=0;i<7;i++){ float t=10+i*7+fmodf(g_time*20,7); float a=(210-i*28)/255.0f; fx_disc(R.px+ax*t,R.py-8+ay*t,0.75f,HEX(0xffffff),a*1.1f); }
    float t=10+7*7.5f; float tx=R.px+ax*t, ty=R.py-8+ay*t;
    fx_caps(tx,ty,tx-ax*4-ay*3,ty-ay*4+ax*3,0.45f,HEX(0xffffff),0.8f); fx_caps(tx,ty,tx-ax*4+ay*3,ty-ay*4-ax*3,0.45f,HEX(0xffffff),0.8f);
    return;
  }
  for(int i=0;i<7;i++){ float t=10+i*7+fmodf(g_time*20,7); int a=210-i*28; gfx_padd((int)(R.px+OX+ax*t),(int)(R.py-8+OY+ay*t),HEX(0xffffff),a); }
  float t=10+7*7.5f; float tx=R.px+OX+ax*t, ty=R.py-8+OY+ay*t;
  gfx_capsule_add(tx,ty,tx-ax*4-ay*3,ty-ay*4+ax*3,0.5f,HEX(0xffffff),120); gfx_capsule_add(tx,ty,tx-ax*4+ay*3,ty-ay*4-ax*3,0.5f,HEX(0xffffff),120);
}

static void draw_ball_trails(void){
  for(int i=0;i<MAX_BALLS;i++){
    Ball *b=&R.balls_e[i]; if(!b->on || b->state==BS_READY) continue;
    const BallDef *d=&BALLS[b->kind];
    if(b->state==BS_RETURN && sqrf(b->x-R.px)+sqrf(b->y-(R.py-8))<30*30) continue;
    bool evo=IS_EVO(b->kind); int n=b->state==BS_RETURN?4:(b->temp?6:TRAIL_N);
    float r0=b->r*(evo?1.0f:0.85f);
    for(int k=0;k<n-1;k++){
      float f=1.0f-(float)k/n; float rr=maxf(0.5f,r0*f);
      float x0=b->tx[k]+OX, y0=b->ty[k]+OY, x1=b->tx[k+1]+OX, y1=b->ty[k+1]+OY;
      if(fabsf(x1-x0)+fabsf(y1-y0)>40) continue;
      if(g_gpu){ fx_caps(x0,y0,x1,y1,rr*1.3f+0.3f,d->col,(b->state==BS_RETURN?0.3f:0.8f)*f); if(evo && k<5) fx_caps(x0,y0,x1,y1,maxf(0.6f,rr*0.5f),d->col2,0.9f*f); continue; }
      gfx_capsule_add(x0,y0,x1,y1,rr,d->col,(int)((b->state==BS_RETURN?60:150)*f));
      if(evo && k<4) gfx_capsule_add(x0,y0,x1,y1,maxf(0.5f,rr*0.4f),d->col2,(int)(120*f));
    }
  }
}
static void draw_balls(void){
  for(int i=0;i<MAX_BALLS;i++){
    Ball *b=&R.balls_e[i]; if(!b->on || b->state==BS_READY) continue;
    gfx_ellipse_a((int)(b->x+OX+2),(int)(b->y+OY+4),(int)maxf(1,b->r),(int)maxf(1,b->r*0.5f),HEX(0x000000),70);
  }
  for(int i=0;i<MAX_BALLS;i++){
    Ball *b=&R.balls_e[i]; if(!b->on || b->state==BS_READY) continue;
    if(b->state==BS_RETURN){ float d=sqrtf(sqrf(b->x-R.px)+sqrf(b->y-(R.py-8))); if(d<14) continue; if(g_gpu) fx_ball(b->x,b->y,maxf(1,b->r-1),b->kind,0); else art_draw_ball(b->x+OX,b->y+OY,maxf(1,b->r-1),b->kind,g_time,false); continue; }
    float dp=sqrf(b->x-R.px)+sqrf(b->y-(R.py-8));
    if(g_gpu) fx_ball(b->x,b->y,b->r,b->kind,dp>18*18?1.0f:0.0f); else art_draw_ball(b->x+OX,b->y+OY,b->r,b->kind,g_time,dp>18*18);
  }
}
static void draw_bullets(void){
  for(int i=0;i<MAX_BUL;i++){
    Bullet *u=&R.bul[i]; if(!u->on) continue;
    if(BUL_LOB(u) && g_gpu){
      bool am=u->kind==4; float p=clampf(u->t/u->vz,0,1);
      int ly=layer_current(); layer(LY_EMIS); int rr=(int)(4+p*10); gfx_ellipse_a((int)u->tx,(int)u->ty,rr,maxi(1,rr/3),am?HEX(0xc07010):HEX(0xff3010),(int)(40+90*p)); layer(ly);
      fx_ring(u->tx,u->ty,4+p*10,0.9f,am?HEX(0xffb030):HEX(0xff5020),0.3f+0.5f*p);
      gfx_ellipse_a((int)u->x,(int)u->y,3,1,HEX(0x000000),90);
      float y=u->y-u->tz; fx_glow(u->x,y,12,am?HEX(0xffb030):HEX(0xff7a22),0.7f); if(am) fx_orb(u->x,y,4,HEX(0xffb030),1); else fx_ball(u->x,y,4,BK_FEU,0);
      continue;
    }
    if(g_gpu){
      gfx_disc((int)floorf(u->x),(int)floorf(u->y),(int)(u->r+1.5f),HEX(0x140a18));   /* contour sombre : lisible sur les sols clairs */
      float sp=sqrtf(u->vx*u->vx+u->vy*u->vy); if(sp>1) fx_caps(u->x,u->y,u->x-u->vx/sp*7,u->y-u->vy/sp*7,maxf(0.6f,u->r-0.5f),u->col,0.5f);
      fx_glow(u->x,u->y,u->r*3.5f+3,u->col,0.55f); fx_orb(u->x,u->y,u->r,u->col,1);
      if(u->kind==3) fx_star(u->x,u->y,u->r*2.4f,HEX(0xe8faff),0.7f+0.3f*sinf(u->t*20));
      continue;
    }
    if(BUL_LOB(u)){
      float p=clampf(u->t/u->vz,0,1);
      int tx=(int)(u->tx+OX), ty=(int)(u->ty+OY); int rr=(int)(4+p*10);
      gfx_ellipse_a(tx,ty,rr,maxi(1,rr/3),u->kind==4?HEX(0xc07010):HEX(0xff3010),(int)(40+80*p));
      gfx_ellipse_a((int)(u->x+OX),(int)(u->y+OY),3,1,HEX(0x000000),90);
      float y=u->y-u->tz+OY;
      gfx_glow(u->x+OX,y,10,HEX(0xff7a22),0.7f);
      gfx_disc((int)(u->x+OX),(int)y,4,HEX(0x5a1a0a)); gfx_disc((int)(u->x+OX),(int)y,3,HEX(0xff8a2a)); gfx_pset((int)(u->x+OX)-1,(int)y-1,HEX(0xffe080));
      continue;
    }
    int x=(int)(u->x+OX), y=(int)(u->y+OY);
    float sp=sqrtf(u->vx*u->vx+u->vy*u->vy); if(sp>1) gfx_capsule_add(u->x+OX,u->y+OY,u->x+OX-u->vx/sp*6,u->y+OY-u->vy/sp*6,maxf(0.5f,u->r-1),u->col,110);
    gfx_glow(u->x+OX,u->y+OY,u->r*3+2,u->col,0.6f);
    gfx_disc(x,y,(int)u->r+1,HEX(0x140a10)); gfx_disc(x,y,(int)u->r,col_scale(u->col,0.7f)); gfx_disc(x,y,(int)u->r-1,u->col); gfx_pset(x,y,HEX(0xffffff));
  }
}
static void draw_particles(bool additive_pass){
  for(int i=0;i<MAX_PART;i++){
    Part *p=&R.part[i]; if(p->life<=0) continue;
    float f=p->life/p->max; int x=(int)(p->x+OX), y=(int)(p->y+OY);
    switch(p->kind){
      case PT_PIX: if(!additive_pass) gfx_pset(x,y,p->c); break;
      case PT_SQ: if(!additive_pass) gfx_rect(x,y,p->size,p->size,p->c); break;
      case PT_SHARD: if(!additive_pass && (f>0.3f || ((i+(int)(g_time*30))&1))) gfx_rect(x,y,p->size,p->size,p->c); break;
      case PT_SMOKE: if(!additive_pass){ if(g_gpu) fx_smoke(p->x,p->y,p->size+(1-f)*3+1.2f,p->c,f*0.55f); else gfx_disc_a(x,y,(int)(p->size+(1-f)*3),p->c,(int)(f*110)); } break;
      case PT_GLOW: if(additive_pass){ if(g_gpu) fx_disc(p->x,p->y,p->size,p->c,f*1.1f); else gfx_disc_add(x,y,p->size,p->c,(int)(f*200)); } break;
      case PT_EMBER: if(additive_pass){ if(g_gpu) fx_disc(p->x,p->y,0.55f,p->c,f*1.7f); else gfx_padd(x,y,p->c,(int)(f*255)); } break;
      case PT_MOTE: if(additive_pass){ float a=sinf(f*PI);
        if(g_gpu){
          if(bg_biome==4){ fx_disc(p->x,p->y,p->size>1?0.9f:0.6f,p->c,a*0.95f); if(p->size>1) fx_glow(p->x,p->y,2.6f,p->c,a*0.25f); }
          else if(bg_biome==5) fx_ring(p->x,p->y,p->size>1?1.8f:1.1f,0.45f,p->c,a*0.8f);
          else if(bg_biome==1||(bg_biome>=6&&bg_biome!=11)) fx_glow(p->x,p->y,p->size>1?3.2f:2.2f,p->c,a*0.85f);
          else fx_dust(p->x,p->y,p->size>1?3.0f:2.0f,p->c,a*(bg_biome==0?0.9f:1.2f)); break; }
        int al=(int)(a*(bg_biome==0?70:170)); gfx_padd(x,y,p->c,al); if(p->size>1){ gfx_padd(x+1,y,p->c,al/3); gfx_padd(x-1,y,p->c,al/3); gfx_padd(x,y+1,p->c,al/3); gfx_padd(x,y-1,p->c,al/3); } } break;
      case PT_SPARK: if(additive_pass){ if(g_gpu) fx_caps(p->x,p->y,p->x-p->vx*0.03f,p->y-p->vy*0.03f,0.45f,p->c,f*1.5f); else gfx_line_a(x,y,(int)(p->x+OX-p->vx*0.03f),(int)(p->y+OY-p->vy*0.03f),p->c,(int)(f*255),true); } break;
      case PT_STREAK: if(additive_pass){ if(g_gpu) fx_caps(p->x,p->y,p->x-p->vx*0.05f,p->y-p->vy*0.05f,0.6f,p->c,f*1.1f); else gfx_capsule_add(p->x+OX,p->y+OY,p->x+OX-p->vx*0.05f,p->y+OY-p->vy*0.05f,0.6f,p->c,(int)(f*200)); } break;
      case PT_RING: if(additive_pass){ if(g_gpu) fx_ring(p->x,p->y,p->size*(1.1f-f*0.9f),1.3f,p->c,f*1.1f); else gfx_ring_a(x,y,(int)(p->size*(1.1f-f*0.9f)),p->c,(int)(f*220),true); } break;
    }
  }
}
static void draw_bolts(void){
  for(int i=0;i<MAX_BOLTS;i++){
    Bolt *b=&R.bolts[i]; if(!b->on) continue;
    float x0=b->x0+OX, y0=b->y0+OY; int segs=6;
    if(g_gpu){ fx_glow(b->x0,b->y0,10,b->c,0.5f); fx_glow(b->x1,b->y1,12,b->c,0.6f); }
    for(int k=1;k<=segs;k++){
      float t=(float)k/segs; float x1=lerpf(b->x0,b->x1,t)+OX+(k<segs?rfrange(-4,4):0), y1=lerpf(b->y0,b->y1,t)+OY+(k<segs?rfrange(-4,4):0);
      if(g_gpu){ fx_caps(x0,y0,x1,y1,1.8f,b->c,0.55f); fx_caps(x0,y0,x1,y1,0.5f,HEX(0xffffff),1.6f); x0=x1; y0=y1; continue; }
      gfx_capsule_add(x0,y0,x1,y1,1.2f,b->c,120); gfx_line_a((int)x0,(int)y0,(int)x1,(int)y1,HEX(0xffffff),255,true);
      x0=x1; y0=y1;
    }
  }
}
static void draw_beams(void){
  for(int i=0;i<MAX_BEAM;i++){
    Beam *b=&R.beams[i]; if(!b->on) continue; float f=b->life/b->max;
    if(g_gpu){ fx_beam(b->x,b->y0,b->y1,b->w*f+1.2f,b->c,f*1.3f); continue; }
    int w=(int)(b->w*f+1); int x=(int)(b->x+OX);
    gfx_rect_add(x-w,(int)b->y0,w*2+1,(int)(b->y1-b->y0),b->c,(int)(110*f));
    gfx_rect_add(x-w/2,(int)b->y0,w+1,(int)(b->y1-b->y0),HEX(0xffffff),(int)(160*f));
  }
}
static void draw_zones(void){
  for(int i=0;i<MAX_ZONES;i++){
    Zone *z=&R.zones[i]; if(!z->on) continue; float f=z->life/z->max;
    int zx=(int)(z->x+OX), zy=(int)(z->y+OY);
    if(z->kind==ZN_ECHO){   /* v7 : l'onde se resserre avant d'eclater */
      float k=1-f, rr=z->r*(1.0f-0.75f*k); uint32_t c=z->aux?HEX(0xd8d0ff):HEX(0xa898ff);
      if(g_gpu){ fx_ring(z->x,z->y,rr,1.0f,c,0.25f+0.55f*k); fx_ring(z->x,z->y,rr*0.45f,0.8f,c,0.15f+0.45f*k); fx_glow(z->x,z->y,6+6*k,c,0.15f+0.25f*k); }
      else { gfx_ring_a(zx,zy,(int)rr,c,(int)(60+130*k),true); gfx_ring_a(zx,zy,(int)(rr*0.45f),c,(int)(40+100*k),true); }
      continue; }
    if(z->kind==ZN_STAR){   /* v9 : etoile filante qui tombe sur sa cible */
      float k=1-f; float sx=z->x-70*(1-k), sy=z->y-170*(1-k); uint32_t c= z->aux==2 ? HEX(0xffc070) : HEX(0xd0dcff);
      if(g_gpu){ fx_caps(sx,sy,sx-22,sy-54,1.6f,c,1.2f); fx_glow(sx,sy,7,HEX(0xffffff),0.9f); fx_star(sx,sy,6,c,1.0f); fx_ring(z->x,z->y,z->r*(0.4f+0.6f*k),0.8f,c,0.5f*k); }
      else { gfx_capsule_add(sx,sy,sx-14,sy-36,1.2f,c,220); gfx_disc_a((int)sx,(int)sy,2,HEX(0xffffff),255); gfx_ring_a(zx,zy,(int)(z->r*(0.4f+0.6f*k)),c,(int)(150*k),true); }
      continue; }
    if(z->kind==ZN_RONCES){   /* v7 : roncier */
      float g=minf(1,(z->max-z->life)*4)*minf(1,z->life*1.5f); if(g<=0.02f) continue;
      gfx_ellipse_a(zx,zy,(int)(z->r*g),(int)(z->r*0.38f*g),HEX(0x1a1408),110);
      for(int k=0;k<7;k++){ float a=k*TAU/7+i*0.7f, bx=z->x+cosf(a)*z->r*0.62f*g, by=z->y+sinf(a)*z->r*0.24f*g;
        float h=(6+3*((k+i)%3))*g, sw=sinf(g_time*1.5f+k)*1.2f;
        gfx_thick_line(bx,by,bx+sw+2,by-h*0.6f,0.8f,HEX(0x4a2c14)); gfx_thick_line(bx+sw+2,by-h*0.6f,bx+sw-1,by-h,0.6f,HEX(0x6a4420));
        gfx_pset((int)(bx+sw+3),(int)(by-h*0.6f),HEX(0xd8c8a0)); gfx_pset((int)(bx+sw-2),(int)(by-h*0.8f),HEX(0xd8c8a0));
        gfx_pset((int)(bx+sw-1),(int)(by-h),HEX(0x8ac850)); gfx_pset((int)(bx+sw),(int)(by-h),HEX(0x6ab040)); }
      continue; }
    if(z->kind==ZN_ACIDE){   /* v7 : flaque acide */
      float g=minf(1,(z->max-z->life)*5)*minf(1,z->life*1.3f); int rx=(int)(z->r*g), ry=(int)(z->r*0.42f*g); if(rx<1) continue;
      gfx_ellipse(zx,zy,rx+1,ry+1,HEX(0x2a3a08)); gfx_ellipse(zx,zy,rx,ry,HEX(0x8ab820)); gfx_ellipse(zx-rx/4,zy-ry/4,maxi(1,rx/2),maxi(1,ry/2),HEX(0xc8f040));
      for(int k=0;k<3;k++){ float ph=fmodf(g_time*1.7f+k*0.37f+i*0.21f,1.0f); int bx=zx+(int)((k-1)*rx*0.45f), by=zy+(int)((k&1)?1:-1);
        if(g_gpu) fx_ring((float)bx,(float)by,0.6f+ph*2.2f,0.6f,HEX(0xf0ffa0),(1-ph)*g*0.9f); else gfx_ring_a(bx,by,(int)(ph*2)+1,HEX(0xf0ffa0),(int)(200*(1-ph)*g),true); }
      if(g_gpu){ int ly=layer_current(); layer(LY_EMIS); gfx_ellipse_a(zx,zy,rx,ry,HEX(0x4a6a08),110); layer(ly); }
      continue; }
    if(z->kind==ZN_AMBER){ float g=minf(1,(z->max-z->life)*5)*minf(1,z->life*1.2f); int rx=(int)(z->r*g), ry=(int)(z->r*0.45f*g); if(rx<1) continue;
      bool vase=z->aux!=0;   /* v7 : vase des marais (meme effet, autre matiere) */
      gfx_ellipse(zx,zy,rx+1,ry+1,vase?HEX(0x1e2410):HEX(0x5a3008)); gfx_ellipse(zx,zy,rx,ry,vase?HEX(0x5a6a24):HEX(0xd08a20)); gfx_ellipse(zx-rx/3,zy-ry/3,maxi(1,rx/3),maxi(1,ry/3),vase?HEX(0x9ab040):HEX(0xffd070));
      if(g_gpu){ int ly=layer_current(); layer(LY_EMIS); gfx_ellipse_a(zx,zy,rx,ry,vase?HEX(0x2a3a08):HEX(0x804010),90); layer(ly); }
      continue; }
    if(g_gpu && z->kind==ZN_STEAM){ for(int k=0;k<3;k++){ float a=g_time*0.8f+k*2.1f; fx_smoke(z->x+cosf(a)*z->r*0.3f,z->y+sinf(a)*z->r*0.2f,z->r*0.8f,HEX(0xe8eef8),f*0.32f); } continue; }
    if(g_gpu && z->kind==ZN_POISON){ fx_ring(z->x,z->y,z->r*(1.2f-f),1.4f,HEX(0x9af05a),f*0.9f); fx_glow(z->x,z->y,z->r,HEX(0x9af05a),f*0.12f); continue; }
    if(g_gpu && z->kind==ZN_WARN){ int ly=layer_current(); layer(LY_EMIS); int a=(int)(60+60*sinf(g_time*30)); gfx_rect_a((int)(z->x-9),0,18,SCREEN_H,HEX(0xff2020),a/2); gfx_vline((int)(z->x-9),0,SCREEN_H,HEX(0xff4040)); gfx_vline((int)(z->x+9),0,SCREEN_H,HEX(0xff4040)); layer(ly); continue; }
    if(z->kind==ZN_STEAM){ for(int k=0;k<3;k++){ float a=g_time*0.8f+k*2.1f; gfx_disc_a(zx+(int)(cosf(a)*z->r*0.3f),zy+(int)(sinf(a)*z->r*0.2f),(int)(z->r*0.7f),HEX(0xe8eef8),(int)(f*45)); } }
    else if(z->kind==ZN_POISON) gfx_ring_a(zx,zy,(int)(z->r*(1.2f-f)),HEX(0x9af05a),(int)(f*200),true);
    else if(z->kind==ZN_FIRE){ gfx_ellipse_a(zx,zy,(int)z->r,(int)(z->r*0.45f),HEX(0x3a0a04),(int)(f*120)); }
    else if(z->kind==ZN_LAVA){ float g=minf(1,(z->max-z->life)*5)*minf(1,z->life*1.5f); int rx=(int)(z->r*g), ry=(int)(z->r*0.45f*g); gfx_ellipse(zx,zy,rx+1,ry+1,HEX(0x2a0a06)); gfx_ellipse(zx,zy,rx,ry,HEX(0xc0300a)); }
    else if(z->kind==ZN_WARN){ int a=(int)(60+60*sinf(g_time*30)); gfx_rect_a((int)(z->x-9+OX),0,18,SCREEN_H,HEX(0xff2020),a/2); gfx_vline((int)(z->x-9+OX),0,SCREEN_H,HEX(0xff4040)); gfx_vline((int)(z->x+9+OX),0,SCREEN_H,HEX(0xff4040)); }
  }
}
static void draw_zones_add(void){
  for(int i=0;i<MAX_ZONES;i++){
    Zone *z=&R.zones[i]; if(!z->on) continue; float f=z->life/z->max;
    int zx=(int)(z->x+OX), zy=(int)(z->y+OY);
    if(g_gpu){
      if(z->kind==ZN_FIRE){ for(int k=0;k<5;k++){ float ph=fmodf(g_time*2+k*0.2f+i,1.0f); float fx=z->x+(k-2)*z->r*0.35f, fy=z->y-ph*14; fx_disc(fx,fy,3*(1-ph)+1,ph<0.5f?HEX(0xffb040):HEX(0xff5a1a),f*(1-ph)*1.2f); } fx_glow(z->x,z->y-4,z->r*1.4f,HEX(0xff6a1a),0.25f*f); }
      if(z->kind==ZN_LAVA){ float g=minf(1,(z->max-z->life)*5)*minf(1,z->life*1.5f); int ly=layer_current(); layer(LY_EMIS); gfx_ellipse(zx,zy,(int)(z->r*g),(int)(z->r*0.45f*g),HEX(0xc0300a)); gfx_ellipse_a(zx,zy,(int)(z->r*g*0.7f),(int)(z->r*0.3f*g),HEX(0xff9a3a),220); layer(ly);
        for(int k=0;k<2;k++){ float ph=fmodf(g_time*1.5f+k*0.5f+i*0.3f,1.0f); fx_ring(z->x+(k?3:-4),z->y+(k?1:0),ph*3+1,0.8f,HEX(0xffe080),(1-ph)*g*1.0f); } }
      continue;
    }
    if(z->kind==ZN_FIRE){ for(int k=0;k<5;k++){ float ph=fmodf(g_time*2+k*0.2f+i,1.0f); int fx=zx+(int)((k-2)*z->r*0.35f), fy=zy-(int)(ph*14); gfx_disc_add(fx,fy,(int)(3*(1-ph))+1,ph<0.5f?HEX(0xffb040):HEX(0xff5a1a),(int)(220*f*(1-ph))); } }
    if(z->kind==ZN_LAVA){ float g=minf(1,(z->max-z->life)*5)*minf(1,z->life*1.5f); gfx_ellipse_a(zx,zy,(int)(z->r*g*0.7f),(int)(z->r*0.3f*g),HEX(0xff8a2a),200); for(int k=0;k<2;k++){ float ph=fmodf(g_time*1.5f+k*0.5f+i*0.3f,1.0f); gfx_ring_a(zx+(k?3:-4),zy+(k?1:0),(int)(ph*3)+1,HEX(0xffe080),(int)(200*(1-ph)*g),true); } }
  }
}
static void draw_boss(void){
  Boss *b=&R.boss; if(!b->on) return;
  if(b->dive){
    if(b->type==BOSS_LEVIATHAN){ gfx_ellipse_a((int)b->x,(int)b->y+8,40,8,HEX(0x061412),110);
      for(int k=0;k<3;k++){ float ph=fmodf(g_time*1.2f+k*0.33f,1.0f); if(g_gpu) fx_ring(b->x,b->y+6,10+ph*40,1.2f,HEX(0x9af0e0),(1-ph)*0.6f); else gfx_ring_a((int)b->x,(int)b->y+6,(int)(10+ph*40),HEX(0x9af0e0),(int)((1-ph)*150),true); } }
    else if(b->type==BOSS_CRAPAUD){ float k=clampf(1-b->dive_t/1.3f,0,1); gfx_ellipse_a((int)b->tx,(int)(72+b->h*0.5f+6),(int)(16+28*k),(int)(4+3*k),HEX(0x000000),(int)(50+100*k)); }
    else if(g_gpu) fx_glow(b->tx,b->y,30,b->type==BOSS_SOLEIL?HEX(0xffc040):HEX(0xe0f0ff),0.35f*(1-b->dive_t/0.45f));
    return;
  }
  float lx=clampf((R.px-b->x)/120,-1,1), ly=clampf(((R.py-6)-b->y)/180,-1,1);
  float anim=b->anim; if(b->roar>0 && b->type!=BOSS_OEIL) anim=maxf(anim,minf(1,b->roar*2));
  float jx = b->dying>0 ? rfrange(-2,2) : 0;
  gfx_ellipse_a((int)(b->x+OX),(int)(b->y+OY+b->h/2+8),(int)(b->w*0.5f),6,HEX(0x000000),110);
  art_draw_boss(b->type,b->x+OX+jx,b->y+OY,b->t,b->flash>0?0.32f:(b->dying>0?0.3f+0.3f*sinf(g_time*40):0),anim,lx,ly,b->phase);
  if(b->type==BOSS_OEIL && b->laser_st>0){
    int lx2=(int)(b->laser_x+OX);
    if(b->laser_st==1){ if(((int)(g_time*20))&1) for(int y=(int)(b->y+20);y<SCREEN_H;y+=4){ if(g_gpu) fx_disc(b->laser_x,(float)y,0.6f,HEX(0xff4a8a),1.3f); else gfx_padd(lx2,y+OY,HEX(0xff4a8a),220); } }
  }
}
static void draw_laser_beam(void){
  Boss *b=&R.boss; if(!b->on || b->type!=BOSS_OEIL || b->laser_st!=2) return;
  int lx=(int)(b->laser_x+OX); int y0=(int)(b->y+18+OY);
  float f=1.0f-fabsf(b->laser_t-0.45f)/0.45f; int w=(int)(6+f*6);
  if(g_gpu){ fx_beam(b->laser_x,(float)y0,SCREEN_H,6+f*6,HEX(0xa040ff),1.7f); fx_glow(b->laser_x,(float)y0,26,HEX(0xd090ff),1.0f); return; }
  gfx_rect_add(lx-w,y0,w*2+1,SCREEN_H-y0,HEX(0xa040ff),160);
  gfx_rect_add(lx-w/2,y0,w+1,SCREEN_H-y0,HEX(0xffa0ff),200);
  gfx_rect_add(lx-1,y0,3,SCREEN_H-y0,HEX(0xffffff),255);
}
static void draw_danger_line(void){
  float a=R.danger>0?R.danger:0;
  for(int x=PIT_X0;x<PIT_X1;x+=8){
    int ph=(x/8+(int)(g_time*4))&7; uint32_t c=col_lerp(HEX(0x5a1a24),HEX(0xff4a4a),a); if(ph==0) c=col_lerp(c,HEX(0xff8a8a),0.4f);
    gfx_hline(x+OX+1,x+OX+5,FLOOR_Y+OY,c); gfx_pset(x+OX+3,FLOOR_Y+OY-1,c);
  }
  for(int x=PIT_X0;x<PIT_X1;x+=10) gfx_pset(x+OX,ZONE_TOP-6+OY,HEX(0x4a4458));
}
static void draw_ftext(void){
  for(int i=0;i<MAX_TXT;i++){ FText *t=&R.txt[i]; if(!t->on) continue;
    uint32_t c=t->life<0.25f?col_scale(t->c,t->life*4):t->c;
    float tx,ty; world_to_screen(t->x+OX,t->y+OY,&tx,&ty);
    if(text_is_number(t->s)){   /* chiffres gras ; les critiques jaillissent en grand un instant */
      if(t->life<0.12f) continue; c=t->c;
      int sc = (t->scale>=2 && t->age<0.09f) ? 2 : 1;
      text_big_digits((int)tx,(int)ty-6-(sc-1)*6,t->s,c,sc,AL_CENTER); continue; }
    int sc=t->scale; if(t->age<0.07f && sc>=2) sc++;
    text_draw((int)tx,(int)ty-(sc-t->scale)*4,t->s,c,sc,AL_CENTER,TX_OUTLINE); }
}

/* ---------------- interface ---------------- */
static void fancy_bar(int x,int y,int w,int h,float v,float trail,uint32_t c,uint32_t back,bool shimmer){
  gfx_rect(x-1,y-1,w+2,h+2,HEX(0x0a0810)); gfx_rect(x,y,w,h,back);
  int tw=(int)(w*clampf(trail,0,1)), fw=(int)(w*clampf(v,0,1));
  if(tw>fw) gfx_rect(x+fw,y,tw-fw,h,HEX(0xf0e0d0));
  for(int j=0;j<h;j++){ float k=1.25f-0.5f*j/maxi(1,h-1); gfx_rect(x,y+j,fw,1,col_scale(c,k)); }
  if(fw>0) gfx_rect(x,y,fw,1,col_lerp(c,HEX(0xffffff),0.45f));
  if(shimmer && fw>4){ int sx=x+(int)fmodf(g_time*60,(float)(w+40))-20; for(int k=0;k<6;k++){ int px=sx+k; if(px>=x && px<x+fw) gfx_rect_add(px,y,1,h,HEX(0xffffff),120-k*18); } }
}
static void fmt_time(float t,char *b,int n){ int s=(int)t; snprintf(b,n,"%d:%02d",s/60,s%60); }
/* ---------------- interface en surimpression sur les falaises ---------------- */
/* jauge de vie : un pilier de pierre sculpte qui renferme un tube de verre, le liquide rouge ondule */
static void hud_hp_gauge(void){
  const int x=10, y0=118, w=20, h=198;
  const int tx=x+5, tw=w-10, ty=y0+10, th=h-20;
  float hpf=clampf(R.hp/R.hpmax,0,1);
  bool low = R.low_hp_t>0;
  /* pilier */
  gfx_rect(x-1,y0-1,w+2,h+2,HEX(0x0a0810));
  for(int y=y0;y<y0+h;y++){ uint32_t c=col_lerp(HEX(0x3a3046),HEX(0x241d2e),(float)(y-y0)/h); gfx_hline(x,x+w-1,y,c); }
  gfx_vline(x,y0,y0+h-1,HEX(0x6a5e7c)); gfx_vline(x+1,y0,y0+h-1,HEX(0x4e4460)); gfx_vline(x+w-1,y0,y0+h-1,HEX(0x17121e));
  for(int k=0;k<=8;k++){ int yy=y0+4+k*(h-8)/8; gfx_hline(x,x+w-1,yy,HEX(0x5a4e6c)); gfx_hline(x,x+w-1,yy+1,HEX(0x16111c)); gfx_pset(x+2,yy-2,HEX(0xc9a24a)); gfx_pset(x+w-3,yy-2,HEX(0x8a6a2a)); }
  /* chapiteau : cristal sculpte */
  gfx_tri((float)x-3,(float)y0+1,(float)x+w+3,(float)y0+1,(float)x+w*0.5f,(float)y0-13,HEX(0x0a0810));
  gfx_tri((float)x-1,(float)y0,(float)x+w+1,(float)y0,(float)x+w*0.5f,(float)y0-11,HEX(0x4e4460));
  gfx_tri((float)x+w*0.5f,(float)y0,(float)x+w+1,(float)y0,(float)x+w*0.5f,(float)y0-11,HEX(0x2e2638));
  { uint32_t gc = low ? col_lerp(HEX(0xa01020),HEX(0xff6a6a),0.5f+0.5f*sinf(g_time*8)) : HEX(0xd8303a);
    gfx_disc(x+w/2,y0-4,3,HEX(0x0a0810)); gfx_disc(x+w/2,y0-4,2,gc); gfx_pset(x+w/2-1,y0-5,HEX(0xffc0c0));
    if(g_gpu){ int ly=layer_current(); layer(LY_EMIS); gfx_disc(x+w/2,y0-4,2,col_scale(gc,0.6f)); layer(ly); } }
  /* tube : fond, liquide, trainee des degats recents */
  gfx_rect(tx-1,ty-1,tw+2,th+2,HEX(0x0a0810)); gfx_rect(tx,ty,tw,th,HEX(0x1c0a12));
  int lh=(int)(th*hpf+0.5f), top=ty+th-lh;
  int trh=(int)(th*clampf(R.hp_trail,0,1)+0.5f), ttop=ty+th-trh;
  if(ttop<top) gfx_rect(tx,ttop,tw,top-ttop,HEX(0xf0c0c8));
  for(int y=top;y<ty+th;y++) for(int i=0;i<tw;i++){
    float cyl=sinf(PI*(i+0.5f)/tw); float k=0.62f+0.5f*cyl; if(i==1) k+=0.25f;
    uint32_t base = R.hurt_t>0 ? HEX(0xff7070) : (low ? col_lerp(HEX(0xc81c30),HEX(0xff5050),0.5f+0.5f*sinf(g_time*8)) : HEX(0xd02838));
    gfx_pset(tx+i,y,col_scale(base,k*(0.92f+0.08f*((y-top)<2))));
  }
  if(lh>2){ for(int i=0;i<tw;i++){ int wv=(int)roundf(sinf(g_time*4+i*0.9f)*0.8f); gfx_pset(tx+i,top+wv,HEX(0xff9a9a)); }
    for(int b=0;b<3;b++){ float ph=fmodf(g_time*0.35f+b*0.37f,1.0f); int by=ty+th-1-(int)(ph*lh); if(by>top+1) gfx_pset(tx+1+(b*3+(int)(g_time*2))%(tw-2),by,HEX(0xffb0b0)); } }
  gfx_vline(tx+1,ty,ty+th-1,HEX(0xffffff)&0x40ffffffu);
  /* experience : fine colonne bleue a droite du pilier */
  { int ex=x+w+2, eh=th; int fh=(int)(eh*clampf(R.xp_show,0,1)+0.5f);
    gfx_rect(ex-1,ty-1,5,eh+2,HEX(0x0a0810)); gfx_rect(ex,ty,3,eh,HEX(0x0e1a2a));
    uint32_t xc=R.levelup_flash>0?HEX(0xffffff):HEX(0x5ab8ff);
    gfx_rect(ex,ty+eh-fh,3,fh,xc); gfx_vline(ex,ty+eh-fh,ty+eh-1,col_lerp(xc,HEX(0xffffff),0.5f)); }
  /* socle : medaillon du niveau du heros */
  { int cx=x+w/2, cy=y0+h+9; gfx_disc(cx,cy,11,HEX(0x0a0810)); gfx_disc(cx,cy,10,HEX(0xc9a24a)); gfx_disc(cx,cy,8,HEX(0x2a1e14)); gfx_ring(cx,cy,8,HEX(0x6a4e2a));
    char b[8]; snprintf(b,sizeof b,"%d",R.plevel); text_draw(cx,cy-4,b,HEX(0xffe8a0),1,AL_CENTER,0); }
  /* valeur, au niveau du liquide */
  { char b[24]; snprintf(b,sizeof b,"%d",(int)ceilf(R.hp)); int yy=clampi(top-5,ty-4,ty+th-10);
    text_draw(x+w+8,yy,IC_HEART,R.hurt_t>0?HEX(0xffffff):HEX(0xff6a6a),1,AL_LEFT,TX_OUTLINE);
    text_big_digits(x+w+18,yy-1,b,low?HEX(0xff8a8a):HEX(0xffffff),1,AL_LEFT); }
}
/* jauge de descente : une chaine qui descend vers le gardien ; le heros glisse le long */
static void hud_level_gauge(void){
  const int x=620, y0=70, h=240;
  float p = R.boss_spawned ? 1.0f : clampf(R.ltime/R.lduration,0,1);
  int py=y0+(int)(p*h);
  for(int y=y0;y<=y0+h;y++){
    bool filled=y<=py; int ph=((y-y0)>>2)&1;
    uint32_t c1=filled?HEX(0xe0b050):HEX(0x3a3242), c2=filled?HEX(0x8a6420):HEX(0x1e1826);
    gfx_pset(x-2,y,HEX(0x0a0810)); gfx_pset(x+2,y,HEX(0x0a0810));
    if(ph){ gfx_pset(x-1,y,c1); gfx_pset(x,y,c2); gfx_pset(x+1,y,c1); } else { gfx_pset(x,y,c1); gfx_pset(x-1,y,HEX(0x0a0810)); gfx_pset(x+1,y,HEX(0x0a0810)); }
  }
  if(R.single){
    for(int k=1;k<3;k++){ int yy=y0+k*h/3; bool done=R.seg>=k, cur=(R.seg==k-1)&&(R.mb_on||R.mb_warned); float pul=cur?0.5f+0.5f*sinf(g_time*7):0;
      gfx_disc(x,yy,8,HEX(0x0a0810)); gfx_disc(x,yy,7,done?HEX(0xc9a24a):col_lerp(HEX(0x5a4a3a),HEX(0xff5a3a),pul)); gfx_disc(x,yy,5,HEX(0x1a1018));
      text_draw(x+1,yy-4,done?IC_STAR:IC_SKULL,done?HEX(0xffd84a):col_lerp(HEX(0xb0a8c0),HEX(0xffb0a0),pul),1,AL_CENTER,0); }
  } else for(int k=1;k<4;k++){ int yy=y0+k*h/4; gfx_rect(x-3,yy-1,7,3,HEX(0x0a0810)); gfx_hline(x-2,x+2,yy,(y0+k*h/4)<=py?HEX(0xffd884):HEX(0x4a4054)); }
  /* medaillon du niveau en haut */
  { int cy=y0-13; gfx_disc(x,cy,11,HEX(0x0a0810)); gfx_disc(x,cy,10,HEX(0xc9a24a)); gfx_disc(x,cy,8,col_scale(BIOMES[R.biome].floor_c,0.8f));
    char b[8]; snprintf(b,sizeof b,"%d",R.mode==MODE_ENDLESS?R.stage+1:(R.single?R.lvl+1:R.level+1)); text_big_digits(x+1,cy-5,b,HEX(0xffe8a0),1,AL_CENTER); }
  /* gardien en bas */
  { int cy=y0+h+15; float pul = R.boss.on ? 0.5f+0.5f*sinf(g_time*7) : (p>0.85f ? 0.3f+0.3f*sinf(g_time*4) : 0);
    gfx_disc(x,cy,11,HEX(0x0a0810)); gfx_disc(x,cy,10,col_lerp(HEX(0x6a4e2a),HEX(0xff5a3a),pul)); gfx_disc(x,cy,8,HEX(0x1a1018));
    text_draw(x+1,cy-4,IC_SKULL,col_lerp(HEX(0xd8d0e0),HEX(0xffb0a0),pul),1,AL_CENTER,0);
    if(pul>0 && g_gpu){ int ly=layer_current(); layer(LY_EMIS); gfx_ring(x,cy,10,col_scale(HEX(0xff5a3a),pul)); layer(ly); } }
  /* le heros */
  { const Sprite *s=&SPR_LO[CHARS[R.ch].sprite]; int yy=clampi(py,y0,y0+h);
    gfx_disc(x,yy,7,HEX(0x0a0810)); gfx_disc(x,yy,6,col_scale(CHARS[R.ch].col,0.55f));
    DrawOpt o=dopt(); o.sx=o.sy=0.75f; draw_spr_c(s,(float)x,(float)yy-1,&o); }
}
static void hud_slot(int x,int y,bool filled,uint32_t acc){
  gfx_rect(x-1,y-1,20,20,HEX(0x0a0810));
  gfx_rect_a(x,y,18,18,filled?HEX(0x241c30):HEX(0x14101c),filled?225:150);
  gfx_frame(x,y,18,18,filled?acc:HEX(0x3a3046));
}
/* boutons de vitesse facon cadre de bronze : >  >>  >>> */
extern float g_speed_msg;
static void chevron(int x,int y,uint32_t c){ for(int k=0;k<7;k++){ int dx= k<=3 ? k : 6-k; gfx_pset(x+dx,y+k,c); gfx_pset(x+dx+1,y+k,c); } }
static void speed_buttons(void){
  int cur=clampi(S.game_speed,0,2);
  for(int i=0;i<3;i++){
    int x,y,w,h; speed_buttons_rect(i,&x,&y,&w,&h); bool on=i==cur, hov=mouse_in(x-1,y-1,w+2,h+2) && !A.pad;
    gfx_rect(x-1,y-1,w+2,h+2,HEX(0x0a0608));
    gfx_rect(x,y,w,h,on?HEX(0xd88a3a):HEX(0x8a5a2e));
    for(int j=1;j<h-1;j++) gfx_hline(x+1,x+w-2,y+j,on?col_lerp(HEX(0xa8602a),HEX(0x7a3e1a),(float)j/h):col_lerp(hov?HEX(0x4a2418):HEX(0x2e1610),HEX(0x1a0c08),(float)j/h));
    static const int CX[4][2]={{0,0},{1,0},{0,1},{1,1}};
    for(int k=0;k<4;k++){ int cx= CX[k][0] ? x+w-2 : x-1, cy= CX[k][1] ? y+h-2 : y-1; gfx_rect(cx,cy,3,3,HEX(0x0a0608)); gfx_pset(cx+1,cy+1,on?HEX(0xffe0a0):HEX(0xe8a860)); }
    uint32_t cc= on ? HEX(0xffe7a8) : (hov?HEX(0xd89a5a):HEX(0xb06c38));
    int n=i+1, tw=(n-1)*4+5, sx=x+(w-tw)/2, sy=y+(h-7)/2;
    for(int c=0;c<n;c++){ chevron(sx+c*4+1,sy+1,HEX(0x2a1208)); chevron(sx+c*4,sy,cc); }
  }
  { int x,y,w,h; speed_buttons_rect(0,&x,&y,&w,&h);
    if(g_speed_msg>0){ static const char *L[3]={"Vitesse x1","Vitesse x1,5","Vitesse x2"}; float a=clampf(g_speed_msg*3,0,1);
      text_draw(x-5,y+5,L[cur],col_scale(HEX(0xffe7a8),a),1,AL_RIGHT,TX_OUTLINE); }
    else if(A.pad) pad_badge(x-22,y+4,BG_R1); else text_badge(x-13,y+4,"R"); }
}
static void draw_hud(void){
  char b[96];
  /* compteurs en haut a gauche */
  int x=8, y=6;
  text_draw(x,y,"^1"IC_COIN,HEX(0xffffff),1,AL_LEFT,TX_OUTLINE);
  snprintf(b,sizeof b,"%d",R.gold); int w=text_big_digits(x+11,y-1,b,HEX(0xffd84a),1,AL_LEFT);
  text_draw(x+20+w,y,"^6"IC_GEM,HEX(0xffffff),1,AL_LEFT,TX_OUTLINE);
  snprintf(b,sizeof b,"%d",R.crystals); int w2=text_big_digits(x+31+w,y-1,b,HEX(0xd8b0ff),1,AL_LEFT);
  if(R.prisms>0){ int px=x+31+w+w2+9; text_draw(px,y,IC_PRISM,HEX(0xffffff),1,AL_LEFT,TX_OUTLINE);
    snprintf(b,sizeof b,"%d",R.prisms); text_big_digits(px+11,y-1,b,HEX(0xb8ecff),1,AL_LEFT); }
  { char tb[16]; fmt_time(R.total_time,tb,sizeof tb); snprintf(b,sizeof b,"^7"IC_SKULL"^0 %d   ^7"IC_CLOCK"^0 %s",R.kills,tb); text_draw(x,y+15,b,HEX(0xe8e0f0),1,AL_LEFT,TX_OUTLINE); }
  /* billes */
  y=34;
  for(int i=0;i<MAX_SLOTS;i++){
    int sx=x+i*22; int k=i<R.nballs?R.balls[i]:-1;
    uint32_t acc= k<0?HEX(0x3a3046):(IS_EVO(k)?HEX(0xc08aff):col_lerp(HEX(0x6a5a7a),BALLS[k].col,0.55f));
    hud_slot(sx,y,k>=0,acc);
    if(k>=0){ if(IS_EVO(k)) gfx_glow((float)sx+9,(float)y+9,12,HEX(0xa060ff),0.25f+0.1f*sinf(g_time*3+i)); icon_draw(k,sx+9.0f,y+9.0f,19,NULL);
      if(IS_EVO(k)) text_draw(sx+15,y-4,IC_STAR,HEX(0xffd84a),1,AL_CENTER,TX_OUTLINE);
      else for(int q=0;q<3;q++) gfx_rect(sx+4+q*4,y+19,3,2,q<R.ball_lvl[i]?BALLS[k].col:HEX(0x2a2232)); }
  }
  { int ready=0,total=0; for(int i=0;i<MAX_BALLS;i++){ Ball *bb=&R.balls_e[i]; if(bb->on && !bb->temp){ total++; if(bb->state==BS_READY) ready++; } }
    snprintf(b,sizeof b,"%d/%d",ready,total); text_draw(x+MAX_SLOTS*22+2,y+5,b,HEX(0xb0a8c0),1,AL_LEFT,TX_OUTLINE); }
  /* objets : numero du niveau au-dessus, comme des trophees */
  y=66;
  for(int i=0;i<MAX_SLOTS;i++){
    int sx=x+i*22; bool f=i<R.npas;
    hud_slot(sx,y,f,HEX(0x6a8a4a));
    if(f){ icon_draw(ICON_PAS(R.pas[i]),sx+9.0f,y+9.0f,19,NULL);
      snprintf(b,sizeof b,"%d",R.pas_lvl[i]); text_draw(sx+16,y+11,b,HEX(0xfff0c0),1,AL_RIGHT,TX_OUTLINE); }
  }
  /* etats */
  { int ix=x, iy=92;
    if(R.shield_up){ text_draw(ix,iy,"^4"IC_STAR,HEX(0xffffff),1,AL_LEFT,TX_OUTLINE); ix+=13; }
    if(R.revives>0){ snprintf(b,sizeof b,"^6"IC_HEART"^0%d",R.revives); text_draw(ix,iy,b,HEX(0xffffff),1,AL_LEFT,TX_OUTLINE); ix+=20; }
    if(R.pact){ text_draw(ix,iy,"^3"IC_SKULL,HEX(0xffffff),1,AL_LEFT,TX_OUTLINE); ix+=13; }
    if(has_ch(CH_HORLOGERE)){ snprintf(b,sizeof b,R.timeslow_t>0?"^2"IC_CLOCK" %.0f":"^7"IC_CLOCK" %.0f",R.timeslow_t>0?R.timeslow_t:R.timeslow_cd); text_draw(ix,iy,b,HEX(0xffffff),1,AL_LEFT,TX_OUTLINE); ix+=30; }
    if(R.chill_t>0){ text_draw(ix,iy,"^4"IC_ARROW,HEX(0xffffff),1,AL_LEFT,TX_OUTLINE); ix+=13; }
    if(R.rank>1){ snprintf(b,sizeof b,"^1R%d",R.rank); text_draw(ix,iy,b,HEX(0xffffff),1,AL_LEFT,TX_OUTLINE); } }
  hud_hp_gauge();
  /* menus ouverts (amelioration, pause) : on n'affiche ni textes de droite ni aides par-dessus */
  if(R.sub==RS_LEVELUP || R.sub==RS_PAUSE || R.sub==RS_EVOLIST){ hud_level_gauge(); return; }
  /* vitesse de jeu (en haut a droite) */
  speed_buttons();
  /* niveau, biome, modificateurs en haut a droite */
  int rx=SCREEN_W-8;
  if(R.mode==MODE_ENDLESS) snprintf(b,sizeof b,"ÉTAGE %d",R.stage+1); else if(R.mode==MODE_RUSH) snprintf(b,sizeof b,"RUSH %d / %d",R.level+1,LEVELS_PER_DEST); else if(R.single) snprintf(b,sizeof b,"NIVEAU %d",R.lvl+1); else snprintf(b,sizeof b,"NIVEAU %d / %d",R.level+1,LEVELS_PER_DEST);
  text_draw(rx,27,b,HEX(0xffd84a),1,AL_RIGHT,TX_OUTLINE);
  text_draw(rx,37,BIOMES[R.biome].name,HEX(0xf0e6d0),1,AL_RIGHT,TX_OUTLINE);
  { int yy=48, ex=rx-26;
    if(R.mode==MODE_DAILY){ text_draw(ex,yy,"^6Défi du jour",HEX(0xffffff),1,AL_RIGHT,TX_OUTLINE); yy+=11; }
    else if(R.depth>1 || (R.dest0 && !R.single)){ snprintf(b,sizeof b,"%s %d",R.dest0&&!R.single?"Profondeurs, prof.":"Profondeur",R.depth); text_draw(ex,yy,b,HEX(0xb0a8c0),1,AL_RIGHT,TX_OUTLINE); yy+=11; }
    if(R.nmods>3){ snprintf(b,sizeof b,"^6"IC_STAR"^0 %d mutateurs (+%d %%)",R.nmods,mut_bonus(R.muts)); text_draw(ex,yy,b,HEX(0xd8d0e4),1,AL_RIGHT,TX_OUTLINE); yy+=11; }
    else for(int i=0;i<R.nmods;i++){ snprintf(b,sizeof b,"^6"IC_STAR"^0 %s",MODS[R.mods[i]].name); text_draw(ex,yy,b,HEX(0xd8d0e4),1,AL_RIGHT,TX_OUTLINE); yy+=11; }
    if(R.hunt>0){ snprintf(b,sizeof b,"^2Retour gagnant +%d %%",2*R.hunt); text_draw(ex,yy,b,HEX(0xffffff),1,AL_RIGHT,TX_OUTLINE); yy+=11; }
    if(R.curses>0){ snprintf(b,sizeof b,"^3"IC_SKULL"^0 %d malédiction%s",R.curses,R.curses>1?"s":""); text_draw(ex,yy,b,HEX(0xd8d0e4),1,AL_RIGHT,TX_OUTLINE); yy+=11; }
    if(R.combo>=8){ float k=minf(1,R.combo/80.0f); uint32_t cc=col_lerp(HEX(0xffd84a),HEX(0xff4a2a),k);
      text_draw(ex,yy+4,"COMBO",cc,1,AL_RIGHT,TX_OUTLINE);
      snprintf(b,sizeof b,"%d",R.combo); text_big_digits(ex,yy+15,b,cc,R.combo>=50?2:1,AL_RIGHT);
      int bw=40; gfx_rect(ex-bw,yy+(R.combo>=50?37:27),(int)(bw*clampf(R.combo_t/1.2f,0,1)),2,cc); } }
  hud_level_gauge();
  /* aide discrete */
  if(A.pad) snprintf(b,sizeof b,"Visée : %s",S.pad_autoaim?"^2auto":"^4stick"); else snprintf(b,sizeof b,"Visée : %s",R.autoaim?"^2auto":"^4souris");
  text_draw(SCREEN_W-30,348,b,HEX(0xc8c0d8),1,AL_RIGHT,TX_OUTLINE);
  if(S.runs<3){ if(A.pad) snprintf(b,sizeof b,"%s : pause   %s : visée",btn(B_START),btn(B_ALT)); else snprintf(b,sizeof b,"Échap : pause   Espace : visée");
    text_draw(SCREEN_W-30,337,b,HEX(0x9a94a8),1,AL_RIGHT,TX_OUTLINE); }
  /* v9 : objectifs du niveau, au debut de la descente */
  if(R.mode==MODE_NORMAL && R.single && R.sub==RS_PLAY && R.total_time>1.0f && R.total_time<10.0f){
    float a=clampf(minf(R.total_time-1.0f,10.0f-R.total_time)*2,0,1); int pw=210, px=(PIT_X0+PIT_X1)/2-pw/2, py=74;
    gfx_rect_a(px,py,pw,48,HEX(0x0a0810),(int)(190*a)); gfx_hline(px,px+pw-1,py,col_scale(HEX(0x8a5a2e),a));
    text_draw(px+pw/2,py+3,"Objectifs du niveau",col_scale(HEX(0xffd84a),a),1,AL_CENTER,TX_OUTLINE);
    for(int k=0;k<3;k++){ char t[80], u[96]; obj_text(R.lvl,k,t,sizeof t); bool dn=(S.obj_done[R.lvl]>>k)&1;
      snprintf(u,sizeof u,"%s %s",dn?"^2"IC_STAR"^0":"^7"IC_STAR"^0",t); text_draw(px+8,py+15+k*11,u,col_scale(HEX(0xe8e0f0),a),1,AL_LEFT,TX_OUTLINE); } }
}
static void draw_mb_bar(void){
  if(!R.mb_on) return;
  const Enemy *e=NULL; for(int i=0;i<MAX_EN;i++) if(R.en[i].on && R.en[i].mboss){ e=&R.en[i]; break; }
  if(!e) return;
  float a=clampf((e->y+40)/110.0f,0,1);
  int w=200, x=(PIT_X0+PIT_X1)/2-w/2, y=10;
  gfx_rect_a(x-6,y-6,w+12,26,HEX(0x0a0810),(int)(170*a));
  text_draw((PIT_X0+PIT_X1)/2,y-4,lvl_mb_name(R.lvl,e->mboss-1),col_scale(HEX(0xffe0c0),a),1,AL_CENTER,TX_OUTLINE);
  uint32_t c=e->sstate?HEX(0xff8a2a):HEX(0xd8a03a);
  fancy_bar(x,y+9,w,5,clampf(e->hp/e->hpmax,0,1),R.boss_trail,c,HEX(0x2a1a0c),false);
  gfx_vline(x+w/2,y+8,y+14,HEX(0xffd84a));
}
static void draw_boss_bar(void){
  draw_mb_bar();
  Boss *b=&R.boss; if(!b->on || b->enter<=0.05f) return;
  float a=clampf(b->enter*2,0,1);
  int w=240, x=(PIT_X0+PIT_X1)/2-w/2, y=10;
  gfx_rect_a(x-6,y-6,w+12,26,HEX(0x0a0810),(int)(170*a));
  text_draw((PIT_X0+PIT_X1)/2,y-4,BOSSES[b->type].name,col_scale(HEX(0xffd0c0),a),1,AL_CENTER,TX_OUTLINE);
  uint32_t c=b->guards>0?HEX(0x8a7aa0):(b->phase?HEX(0xff5a2a):HEX(0xd8303a));
  fancy_bar(x,y+9,w,6,clampf(b->hp/b->hpmax,0,1),R.boss_trail,c,HEX(0x2a0c12),false);
  gfx_vline(x+w/2,y+8,y+15,HEX(0xffd84a));
}

/* ---------------- surcouches ---------------- */
static void banner(void){
  if(R.banner_t<=0 || R.sub==RS_LEVELUP || R.sub==RS_PAUSE || R.sub==RS_EVOLIST) return;
  float a=clampf(R.banner_t*2,0,1);
  int cx=(PIT_X0+PIT_X1)/2;
  int w=maxi(text_w(R.banner,2),text_w(R.banner2,1))+40; int h=R.banner2[0]?52:34; int y=112-(int)((1-a)*10);
  gfx_rect_a(cx-w/2,y,w,h,HEX(0x0a0810),(int)(a*200));
  for(int k=0;k<2;k++){ int yy=k?y+h:y; gfx_hline(cx-w/2,cx+w/2,yy,col_scale(HEX(0xc9a24a),a)); gfx_rect(cx-w/2-2,yy-1,3,3,col_scale(HEX(0xc9a24a),a)); gfx_rect(cx+w/2-1,yy-1,3,3,col_scale(HEX(0xc9a24a),a)); }
  text_draw(cx,y+6,R.banner,col_scale(HEX(0xffe8b0),a),2,AL_CENTER,TX_OUTLINE);
  if(R.banner2[0]) text_draw(cx,y+34,R.banner2,col_scale(HEX(0xd0c8e0),a),1,AL_CENTER,TX_SHADOW);
}
static void tutorial(void){
  if((R.single?R.lvl:R.level)!=0 || R.tutorial_t<=0 || R.sub!=RS_PLAY) return;
  float a=clampf(R.tutorial_t,0,1);
  int cx=(PIT_X0+PIT_X1)/2;
  gfx_rect_a(cx-132,176,264,48,HEX(0x0a0810),(int)(a*180));
  text_draw(cx,180,A.pad?"Stick gauche : se déplacer   Stick droit : viser":"ZQSD / Flèches : se déplacer   Souris : viser",col_scale(HEX(0xf0e6d0),a),1,AL_CENTER,TX_SHADOW);
  text_draw(cx,193,"Les billes partent seules et reviennent au sol.",col_scale(HEX(0xb0a8c0),a),1,AL_CENTER,TX_SHADOW);
  text_draw(cx,206,"Ne laissez pas les monstres atteindre le fond !",col_scale(HEX(0xff8a7a),a),1,AL_CENTER,TX_SHADOW);
}
static void draw_evolist(void){
  gfx_darken(0,0,SCREEN_W,SCREEN_H,205);
  ui_fancy_panel(60,10,520,340);
  text_draw(SCREEN_W/2,18,"ÉVOLUTIONS",HEX(0xd2b0f5),2,AL_CENTER,TX_OUTLINE);
  text_draw(SCREEN_W/2,36,"Deux billes au niveau 3 peuvent fusionner.",HEX(0xb0a8c0),1,AL_CENTER,0);
  int ox0=gT->cx0, oy0=gT->cy0, ox1=gT->cx1, oy1=gT->cy1;
  for(int k=BK_FIRST_EVO;k<=BK_LAST_EVO;k++){
    int i=k-BK_FIRST_EVO; int x=74+(i%2)*254, y=50+(i/2)*31;
    bool seen = (S.evo_seen>>(i))&1;
    icon_draw(BALLS[k].evo_a,x+8.0f,y+6.0f,17,NULL); text_draw(x+18,y+2,"+",HEX(0x9a94a8),1,AL_LEFT,0);
    icon_draw(BALLS[k].evo_b,x+32.0f,y+6.0f,17,NULL); text_draw(x+42,y+2,IC_ARROW,HEX(0x9a94a8),1,AL_LEFT,0);
    icon_draw(k,x+59.0f,y+6.0f,17,NULL);
    text_draw(x+71,y+2,BALLS[k].name,seen?HEX(0xffd84a):HEX(0xf0e6d0),1,AL_LEFT,TX_SHADOW);
    char b[200]; ball_desc(k,1,b,sizeof b);
    char *dot=strstr(b," Dégâts"); if(dot) *dot=0;
    gfx_clip(maxi(ox0,x),maxi(oy0,y+12),mini(ox1,x+244),mini(oy1,y+30));
    text_wrap(x,y+13,240,b,HEX(0x8a84a0),1,0,9);
    gfx_clip(ox0,oy0,ox1,oy1);
  }
  text_draw(SCREEN_W/2,336,"Appuyez sur une touche pour revenir",HEX(0x6a6478),1,AL_CENTER,0);
}
static void draw_event(void){
  /* decor : une grotte eclairee par un feu */
  gfx_gradient_v(0,0,SCREEN_W,SCREEN_H,HEX(0x0c0812),HEX(0x1a1218));
  for(int i=0;i<60;i++){ uint32_t h=(uint32_t)i*2654435761u; int x=(int)(h%SCREEN_W), y=(int)((h>>11)%SCREEN_H); gfx_rect(x,y,(int)(h>>20)%30+10,(int)(h>>25)%6+2,HEX(0x15101a)); }
  float f=0.85f+0.15f*sinf(g_time*12)+0.05f*sinf(g_time*29);
  gfx_glow(160,200,170,HEX(0xff8a3a),0.22f*f);
  { static Sprite cv; if(!cv.px) cv=spr_alloc(136,104);
    Target tg=spr_target(&cv); Target *old=gT; gfx_target(&tg);
    gfx_gradient_v(0,0,136,104,HEX(0x140e1c),HEX(0x2a1e24));
    for(int i=0;i<14;i++){ uint32_t h=(uint32_t)(i+3)*2654435761u; gfx_rect((int)(h%136),(int)((h>>9)%60),(int)(h>>20)%20+6,2,HEX(0x1e1626)); }
    gfx_glow(68,92,60,HEX(0xff8a3a),0.25f*f);
    art_draw_event(R.ev_id,68,62,g_time);
    gfx_target(old);
    int fx=160-136, fy=200-104;
    gfx_rect(fx-6,fy-6,136*2+12,104*2+12,HEX(0x2a1e14)); gfx_frame(fx-6,fy-6,136*2+12,104*2+12,HEX(0xc9a24a)); gfx_frame(fx-3,fy-3,136*2+6,104*2+6,HEX(0x5a4228));
    draw_spr_scaled(&cv,fx,fy,2,0xFFFFFFFFu);
    for(int k=0;k<4;k++){ int cx2=k&1?fx+136*2+2:fx-3, cy2=k&2?fy+104*2+2:fy-3; gfx_rect(cx2-2,cy2-2,5,5,HEX(0xc9a24a)); gfx_pset(cx2,cy2,HEX(0xfff0b0)); }
  }
  for(int i=0;i<8;i++){ float ph=fmodf(g_time*0.5f+i*0.13f,1.0f); gfx_padd(160+(int)(sinf(i*5.0f+g_time)*60),300-(int)(ph*200),HEX(0xffb060),(int)(200*(1-ph))); }
  const char *title,*desc; run_event_text(&title,&desc);
  ui_fancy_panel(318,40,306,280);
  text_draw(471,52,title,HEX(0xffd84a),2,AL_CENTER,TX_OUTLINE);
  text_wrap(332,80,278,desc,HEX(0xd8d0e4),1,0,12);
  char b[120]; snprintf(b,sizeof b,"^1"IC_COIN" %d^0   "IC_HEART" %d/%d   ^6"IC_GEM" %d",R.gold,(int)R.hp,(int)R.hpmax,R.crystals); if(R.prisms>0){ size_t l=strlen(b); snprintf(b+l,sizeof b-l,"   "IC_PRISM" %d",R.prisms); }
  text_draw(471,124,b,HEX(0xb0a8c0),1,AL_CENTER,0);
  if(R.ev_result<0){
    char lab[4][96]; bool en[4]; int n=run_event_options(lab,en);
    ui_begin();
    for(int i=0;i<n;i++){ if(ui_button(332,146+i*30,278,24,lab[i],en[i])){ run_event_choose(i); break; } }
    ui_end();
  } else {
    text_wrap(332,150,278,R.ev_msg,HEX(0xf0e6d0),1,TX_SHADOW,12);
    if(R.ev_t>0.4f){ snprintf(b,sizeof b,A.pad?"%s : continuer la descente":"Entrée / clic : continuer la descente",btn(B_CONFIRM)); text_draw(471,294,b,HEX(0x8a82a0),1,AL_CENTER,0); }
  }
  text_draw(SCREEN_W/2,8,R.mode==MODE_ENDLESS?"Entre deux étages":(R.single?"Une halte dans la descente":"Entre deux niveaux"),HEX(0x6a6478),1,AL_CENTER,0);
}

/* ---------------- rendu GPU : couches, lumieres, parametres d'image ---------------- */
static void event_gpu(void){
  /* grotte eclairee par le feu : decor dans la scene, lumiere dynamique, panneaux dans l'interface */
  gfx_gradient_v(0,0,SCREEN_W,SCREEN_H,HEX(0x2a2030),HEX(0x3a2c30));
  for(int i=0;i<60;i++){ uint32_t h=(uint32_t)i*2654435761u; int x=(int)(h%SCREEN_W), y=(int)((h>>11)%SCREEN_H); gfx_rect(x,y,(int)(h>>20)%30+10,(int)(h>>25)%6+2,HEX(0x231a28)); }
  float f=0.85f+0.15f*sinf(g_time*12)+0.05f*sinf(g_time*29);
  { static Sprite cv; if(!cv.px) cv=spr_alloc(136,104);
    Target tg=spr_target(&cv); Target *old=gT; gfx_target(&tg);
    gfx_gradient_v(0,0,136,104,HEX(0x2a2236),HEX(0x40323a));
    for(int i=0;i<14;i++){ uint32_t h=(uint32_t)(i+3)*2654435761u; gfx_rect((int)(h%136),(int)((h>>9)%60),(int)(h>>20)%20+6,2,HEX(0x32283c)); }
    art_draw_event(R.ev_id,68,62,g_time);
    gfx_target(old);
    int fx=160-136, fy=200-104;
    gfx_rect(fx-6,fy-6,136*2+12,104*2+12,HEX(0x2a1e14)); gfx_frame(fx-6,fy-6,136*2+12,104*2+12,HEX(0xc9a24a)); gfx_frame(fx-3,fy-3,136*2+6,104*2+6,HEX(0x5a4228));
    draw_spr_scaled(&cv,fx,fy,2,0xFFFFFFFFu);
    for(int k=0;k<4;k++){ int cx2=k&1?fx+136*2+2:fx-3, cy2=k&2?fy+104*2+2:fy-3; gfx_rect(cx2-2,cy2-2,5,5,HEX(0xc9a24a)); gfx_pset(cx2,cy2,HEX(0xfff0b0)); }
  }
  light_begin(0,0,SCREEN_W,SCREEN_H,70,60,86);
  light_add(160,268,230,HEX(0xff8a3a),1.25f*f);
  light_add(160,200,90,HEX(0xffc070),0.45f*f);
  light_add(470,60,200,HEX(0x6a5aa8),0.35f);
  for(int i=0;i<8;i++){ float ph=fmodf(g_time*0.5f+i*0.13f,1.0f); fx_disc(160+sinf(i*5.0f+g_time)*60,300-ph*200,0.6f,HEX(0xffb060),(1-ph)*1.4f); }
  fx_glow(160,290,70,HEX(0xff7a2a),0.22f*f);
  GP.fog[0]=0.75f; GP.fog[1]=0.55f; GP.fog[2]=0.45f; GP.fog[3]=0.12f; GP.bloom=S.bloom?0.6f:0; GP.bloom_thr=0.85f;
  GP.sat=1.08f; GP.con=1.05f; GP.vig=0.65f; GP.sh_tint[0]=0.16f; GP.sh_tint[1]=0.06f; GP.sh_tint[2]=0.2f; GP.tint_amt=0.12f;
  GP.hi_tint[0]=1.04f; GP.hi_tint[1]=1.0f; GP.hi_tint[2]=0.95f;
  layer(LY_UI);
  const char *title,*desc; run_event_text(&title,&desc);
  ui_fancy_panel(318,40,306,280);
  text_draw(471,52,title,HEX(0xffd84a),2,AL_CENTER,TX_OUTLINE);
  text_wrap(332,80,278,desc,HEX(0xd8d0e4),1,0,12);
  char b[120]; snprintf(b,sizeof b,"^1"IC_COIN" %d^0   "IC_HEART" %d/%d   ^6"IC_GEM" %d",R.gold,(int)R.hp,(int)R.hpmax,R.crystals); if(R.prisms>0){ size_t l=strlen(b); snprintf(b+l,sizeof b-l,"   "IC_PRISM" %d",R.prisms); }
  text_draw(471,124,b,HEX(0xb0a8c0),1,AL_CENTER,0);
  if(R.ev_result<0){
    char lab[4][96]; bool en[4]; int n=run_event_options(lab,en);
    ui_begin();
    for(int i=0;i<n;i++){ if(ui_button(332,146+i*30,278,24,lab[i],en[i])){ run_event_choose(i); break; } }
    ui_end();
  } else {
    text_wrap(332,150,278,R.ev_msg,HEX(0xf0e6d0),1,TX_SHADOW,12);
    if(R.ev_t>0.4f){ snprintf(b,sizeof b,A.pad?"%s : continuer la descente":"Entrée / clic : continuer la descente",btn(B_CONFIRM)); text_draw(471,294,b,HEX(0x8a82a0),1,AL_CENTER,0); }
  }
  text_draw(SCREEN_W/2,8,R.mode==MODE_ENDLESS?"Entre deux étages":(R.single?"Une halte dans la descente":"Entre deux niveaux"),HEX(0x6a6478),1,AL_CENTER,0);
}

/* eclairage « plein jour peint » : ambiance forte par biome (les couleurs peintes s'affichent telles quelles) */
static const float AMB[BIOME_COUNT][3]={{0.92f,0.88f,0.94f},{0.90f,0.98f,0.88f},{1.02f,0.86f,0.80f},{0.94f,0.86f,1.02f},
                                        {0.96f,0.98f,1.06f},{0.86f,0.98f,0.98f},{1.02f,0.94f,0.82f},{0.88f,0.88f,1.0f},
                                        {0.88f,1.0f,0.94f},{0.92f,0.94f,0.80f},{0.94f,0.86f,1.04f},{1.06f,0.96f,0.80f}};
static void run_draw_gpu(void){
  if(R.sub==RS_EVENT){ g_p25=0; event_gpu(); if(R.fade>0) gfx_darken(0,0,SCREEN_W,SCREEN_H,(int)(clampf(R.fade,0,1)*255)); return; }
  OX=OY=0;
  g_p25=S.view25?1.0f:0.0f; GP.p25=g_p25;   /* vue 2.5D : camera inclinee */
  int lv=R.biome;
  /* camera : tremblement lisse sous-pixel, zoom a la mort d'un gardien */
  GP.cam_cx=(PIT_X0+PIT_X1)*0.5f; GP.cam_cy=SCREEN_H*0.5f;
  if(R.shake>0 && S.shake){ float t=g_time, k=R.shake*0.8f;
    GP.cam_x=k*(sinf(t*53.1f)*0.55f+sinf(t*31.7f+1.3f)*0.3f+sinf(t*97.3f+0.4f)*0.15f);
    GP.cam_y=k*(sinf(t*47.3f+0.7f)*0.55f+sinf(t*27.1f+2.1f)*0.3f+sinf(t*89.9f+1.9f)*0.15f); }
  if(R.boss.on && R.boss.dying>0){ float k=sinf(minf(1,R.boss.dying/2.3f)*PI); GP.cam_zoom=1.0f+0.06f*k; GP.cam_cx=lerpf(GP.cam_cx,R.boss.x,0.6f); GP.cam_cy=lerpf(GP.cam_cy,R.boss.y,0.6f); }
  GP.view[0]=0; GP.view[1]=0; GP.view[2]=SCREEN_W; GP.view[3]=SCREEN_H;
  /* mise en scene : la camera se pose au debut du niveau, s'approche du gardien qui arrive, de la chute du heros */
  float cine=0;
  if(R.sub==RS_INTRO){ float k=clampf(R.sub_t/1.6f,0,1); k=(1-k)*(1-k); GP.cam_zoom=1.0f+0.07f*k; GP.cam_cx=lerpf(GP.cam_cx,R.px,0.8f*k); GP.cam_cy=lerpf(GP.cam_cy,R.py-30,0.8f*k); }
  if(R.boss.on && R.boss.enter<1){ cine=sinf(clampf(R.boss.enter,0,1)*PI); GP.cam_zoom=1.0f+0.045f*cine; GP.cam_cx=lerpf(GP.cam_cx,R.boss.x,0.5f*cine); GP.cam_cy=lerpf(GP.cam_cy,maxf(R.boss.y,40),0.45f*cine); }
  if(R.sub==RS_DEAD){ float a=clampf(R.dead_t/2.0f,0,1); a=a*a*(3-2*a); GP.cam_zoom=1.0f+0.07f*a; GP.cam_cx=lerpf(GP.cam_cx,R.px,a); GP.cam_cy=lerpf(GP.cam_cy,R.py-10,a); }
  /* scene */
  gfx_clip(PIT_X0,0,PIT_X1,SCREEN_H);
  draw_floor();
  if(g_p25>0) floor_wall_shade();
  layer(LY_EMIS); draw_danger_line(); layer(LY_SCENE);
  draw_zones();
  draw_pickups();
  draw_curios();
  for(int i=0;i<MAX_EN;i++) if(R.en[i].on && (ENEMIES[R.en[i].kind].flags&EF_FLY)==0) draw_enemy(&R.en[i]);
  draw_boss();
  if(R.sub!=RS_DEAD || R.dead_t<0.1f) draw_player();
  for(int i=0;i<MAX_EN;i++) if(R.en[i].on && (ENEMIES[R.en[i].kind].flags&EF_FLY)) draw_enemy(&R.en[i]);
  draw_links();
  draw_particles(false);
  draw_balls();
  draw_bullets();
  gfx_noclip();
  draw_scenery();
  if(g_p25>0) draw_faces();
  draw_torches();
  /* eclairage « plein jour peint » : ambiance forte (les couleurs peintes s'affichent telles quelles),
     les lumieres ne sont que des accents */
  float ak = (run_has_mod(MOD_BROUILLARD) ? 0.62f : 1.0f)*0.9f;
  light_begin(0,0,SCREEN_W,SCREEN_H,(int)(AMB[lv][0]*256*ak),(int)(AMB[lv][1]*256*ak),(int)(AMB[lv][2]*256*ak));
  GP.light_gain=0.40f; GP.light_range=0.95f;
  add_scene_lights();
  light_add(R.px,R.py-6,52,HEX(0xffd8a8),0.25f);     /* lanterne du heros */
  gfx_clip(PIT_X0,0,PIT_X1,SCREEN_H);
  draw_zones_add();
  draw_ball_trails();
  draw_aim();
  draw_beams();
  draw_laser_beam();
  draw_bolts();
  draw_particles(true);
  draw_player_fx();
  gfx_noclip();
  torch_glows();
  /* ondes de choc, air brulant */
  int ns=0;
  for(int i=0;i<MAX_SHOCK && ns<8;i++){ Shock *sk=&R.shocks[i]; if(!sk->on) continue; float k=sk->t/sk->max; float e=1-(1-k)*(1-k);
    GP.shock[ns][0]=sk->x; GP.shock[ns][1]=sk->y; GP.shock[ns][2]=sk->rmax*e; GP.shock[ns][3]=sk->str*(1-k); ns++; }
  if(lv==2) GP.haze=0.9f;
  /* brouillard volumetrique eclaire par les lumieres, voile sombre en haut du puits */
  static const uint32_t FOGC[BIOME_COUNT]={HEX(0x8a92c8),HEX(0x8ad8b0),HEX(0xffa070),HEX(0xb48aff),HEX(0xa8d8ff),HEX(0x6ae0c8),HEX(0xffc070),HEX(0xc8c8ff),
                                           HEX(0xa8f0b0),HEX(0xa0b070),HEX(0xd8a0ff),HEX(0xffd890)};
  static const float FOGD[BIOME_COUNT]={0.05f,0.06f,0.05f,0.06f,0.04f,0.06f,0.04f,0.06f, 0.04f,0.09f,0.05f,0.04f};
  GP.fog[0]=cr(FOGC[lv])/255.0f; GP.fog[1]=cg(FOGC[lv])/255.0f; GP.fog[2]=cb(FOGC[lv])/255.0f; GP.fog[3]=FOGD[lv]*(run_has_mod(MOD_BROUILLARD)?2.6f:1.0f);
  GP.fog_dark=g_p25>0?0.38f:0.28f; GP.scroll=R.scroll; GP.fog_speed=1.0f;   /* le fond du couloir se perd dans l'ombre */
  GP.emis_gain=1.1f;
  /* image */
  static const float BTH[BIOME_COUNT]={0.90f,0.90f,0.86f,0.88f,0.94f,0.90f,0.90f,0.84f, 0.88f,0.90f,0.86f,0.88f}, BST[BIOME_COUNT]={0.45f,0.45f,0.55f,0.5f,0.4f,0.45f,0.45f,0.6f, 0.5f,0.45f,0.55f,0.5f};
  GP.bloom=S.bloom?BST[lv]:0; GP.bloom_thr=BTH[lv];
  static const uint32_t SH[BIOME_COUNT]={HEX(0x2a2050),HEX(0x103828),HEX(0x401808),HEX(0x300c40),HEX(0x102a50),HEX(0x0a3830),HEX(0x402008),HEX(0x141430),HEX(0x0c3020),HEX(0x1e2410),HEX(0x2a1040),HEX(0x402a08)};
  static const float HI[BIOME_COUNT][3]={{1.04f,1.0f,0.95f},{0.98f,1.03f,0.99f},{1.05f,0.99f,0.92f},{1.01f,0.96f,1.05f},{0.96f,1.0f,1.06f},{0.95f,1.03f,1.02f},{1.06f,1.0f,0.9f},{1.0f,0.98f,1.06f},
                                         {0.98f,1.05f,0.96f},{1.0f,1.02f,0.92f},{1.03f,0.96f,1.06f},{1.07f,1.01f,0.9f}};
  uint32_t sh=SH[lv]; float sa=0.10f; GP.sat=1.08f; GP.con=1.04f; GP.vig=0.28f;
  if(R.timeslow_t>0){ GP.sat=0.5f; sh=HEX(0x105a50); sa=0.35f; }
  if(R.pact){ sh=HEX(0x400818); sa=0.24f; }
  GP.sh_tint[0]=cr(sh)/255.0f; GP.sh_tint[1]=cg(sh)/255.0f; GP.sh_tint[2]=cb(sh)/255.0f; GP.tint_amt=sa;
  for(int k=0;k<3;k++) GP.hi_tint[k]=HI[lv][k];
  GP.chroma = S.bloom ? 0.05f : 0; GP.grain = S.bloom ? 0.022f : 0;
  GP.dof=0.25f; GP.dof_y0=0; GP.dof_y1=40;
  if(R.levelup_flash>0){ GP.flash[0]=1.0f; GP.flash[1]=0.88f; GP.flash[2]=0.63f; GP.flash[3]=R.levelup_flash*0.22f; }
  if(R.hurt_t>0){ GP.flash[0]=1.0f; GP.flash[1]=0.12f; GP.flash[2]=0.12f; GP.flash[3]=R.hurt_t*0.32f; GP.chroma+=R.hurt_t*2.2f; }
  if(R.hitstop>0) GP.chroma+=R.hitstop*4.0f;
  if(R.low_hp_t>0 && R.sub==RS_PLAY){ GP.lowhp=0.16f+0.12f*sinf(g_time*6); GP.desat=0.18f+0.08f*sinf(g_time*6); }
  { extern int g_dbg; if(g_dbg&1) GP.fog[3]=0; if(g_dbg&2) GP.bloom=0; if(g_dbg&4) GP.normal_amt=0; if(g_dbg&8){ GP.dof=0; GP.chroma=0; GP.grain=0; } if(g_dbg&16) GP.fog_dark=0; }
  /* interface */
  layer(LY_UI);
  gfx_noclip();
  draw_ftext();
  draw_curio_labels();
  gfx_clip(PIT_X0,0,PIT_X1,SCREEN_H);
  if(cine>0.01f){ int bt=(int)(16*cine+0.5f), bb=(int)(10*cine+0.5f); gfx_noclip(); gfx_rect(0,0,SCREEN_W,bt,HEX(0x000000)); gfx_rect(0,SCREEN_H-bb,SCREEN_W,bb,HEX(0x000000)); gfx_clip(PIT_X0,0,PIT_X1,SCREEN_H); }
  draw_boss_bar();
  banner();
  tutorial();
  gfx_noclip();
  draw_hud();
  if(R.sub==RS_DEAD){
    float a=clampf(R.dead_t/2.0f,0,1);
    GP.desat=a*0.85f; GP.blur_all=a*0.6f;
    gfx_darken(0,0,SCREEN_W,SCREEN_H,(int)(a*150));
    if(R.dead_t>0.5f){ layer(LY_EMIS); text_draw(SCREEN_W/2,150,"VOUS ÊTES TOMBÉ…",HEX(0xc04030),3,AL_CENTER,0); layer(LY_UI);
      text_draw(SCREEN_W/2,150,"VOUS ÊTES TOMBÉ…",HEX(0xff6a5a),3,AL_CENTER,TX_OUTLINE); }
  }
  if(R.sub==RS_WIN){
    float a=clampf(R.sub_t*2,0,1);
    GP.blur_all=a*0.5f; GP.exposure=1.0f+0.15f*a;
    { int dy=(int)((1-a)*20); layer(LY_EMIS); text_draw(SCREEN_W/2,110-dy,"VICTOIRE !",HEX(0xd8a830),5,AL_CENTER,0); layer(LY_UI); }
    gfx_darken(0,0,SCREEN_W,SCREEN_H,(int)(a*110));
    int dy=(int)((1-a)*20);
    gfx_glow(SCREEN_W/2,130,160,HEX(0xffc040),0.25f*a);
    text_draw(SCREEN_W/2,110-dy,"VICTOIRE !",HEX(0xffd84a),5,AL_CENTER,TX_OUTLINE);
    if(a>=1) text_draw(SCREEN_W/2,184,R.dest0?"Les Profondeurs se taisent.":"Le Gouffre est purifié.",HEX(0xf0e6d0),2,AL_CENTER,TX_OUTLINE);
  }
  if(R.sub==RS_LEVELUP){ GP.blur_all=0.9f*clampf(R.card_t*5,0,1); GP.desat=0.25f; levelup_draw(); }
  if(R.sub==RS_PAUSE){ GP.blur_all=0.85f; GP.desat=0.3f; if(g_state==GS_RUN) pause_ui(); else gfx_darken(0,0,SCREEN_W,SCREEN_H,100); }
  if(R.sub==RS_EVOLIST){ GP.blur_all=0.9f; draw_evolist(); }
  if(R.fade>0) gfx_darken(0,0,SCREEN_W,SCREEN_H,(int)(clampf(R.fade,0,1)*255));
}

void run_draw(void){
  if(g_gpu){ run_draw_gpu(); return; }
  g_p25 = R.sub==RS_EVENT ? 0.0f : (S.view25?1.0f:0.0f);
  if(R.sub==RS_EVENT){ draw_event(); if(R.fade>0) gfx_darken(0,0,SCREEN_W,SCREEN_H,(int)(clampf(R.fade,0,1)*255)); return; }
  OX=OY=0;
  if(R.shake>0 && S.shake){ OX=(int)roundf(rfrange(-R.shake,R.shake)); OY=(int)roundf(rfrange(-R.shake,R.shake)); }
  gfx_clip(PIT_X0,0,PIT_X1,SCREEN_H);
  draw_floor();
  if(g_p25>0) floor_wall_shade();
  draw_danger_line();
  draw_zones();
  draw_pickups();
  draw_curios();
  for(int i=0;i<MAX_EN;i++) if(R.en[i].on && (ENEMIES[R.en[i].kind].flags&EF_FLY)==0) draw_enemy(&R.en[i]);
  draw_boss();
  if(R.sub!=RS_DEAD || R.dead_t<0.1f) draw_player();
  for(int i=0;i<MAX_EN;i++) if(R.en[i].on && (ENEMIES[R.en[i].kind].flags&EF_FLY)) draw_enemy(&R.en[i]);
  draw_links();
  draw_particles(false);
  draw_balls();
  draw_bullets();
  gfx_noclip();
  draw_scenery();
  if(g_p25>0) draw_faces();
  /* voile du haut */
  uint32_t fogc=col_scale(BIOMES[R.biome].amb,0.12f);
  for(int y=0;y<48;y++) gfx_rect_a(0,y,SCREEN_W,1,fogc,(int)(80*powf(1-y/48.0f,1.5f)));
  draw_torches();
  { int lv=R.biome; float ak=(run_has_mod(MOD_BROUILLARD) ? 0.62f : 1.0f)*0.9f;
    light_begin(0,0,SCREEN_W,SCREEN_H,(int)(AMB[lv][0]*256*ak),(int)(AMB[lv][1]*256*ak),(int)(AMB[lv][2]*256*ak)); }
  light_gain(0.40f);
  add_scene_lights();
  floor_emissive_light();
  light_apply();
  gfx_clip(PIT_X0,0,PIT_X1,SCREEN_H);
  draw_zones_add();
  draw_ball_trails();
  draw_aim();
  draw_beams();
  draw_laser_beam();
  draw_bolts();
  draw_particles(true);
  draw_player_fx();
  gfx_noclip();
  torch_glows();
  /* post-traitement */
  for(int i=0;i<MAX_SHOCK;i++){ Shock *s=&R.shocks[i]; if(!s->on) continue; float k=s->t/s->max; float e=1-(1-k)*(1-k); post_shockwave(s->x+OX,s->y+OY,s->rmax*e,7,s->str*(1-k),PIT_X0,0,PIT_X1,SCREEN_H); }
  if(R.biome==2) post_haze(PIT_X0,0,PIT_X1,SCREEN_H,g_time,0.9f);
  if(S.bloom){ static const int TH[BIOME_COUNT]={228,228,218,224,238,228,228,214, 224,230,216,222}; static const float ST[BIOME_COUNT]={0.4f,0.4f,0.5f,0.45f,0.35f,0.4f,0.4f,0.55f, 0.45f,0.4f,0.52f,0.48f}; post_bloom(0,0,SCREEN_W,SCREEN_H,TH[R.biome],ST[R.biome],0.85f); }
  {
    static const uint32_t SH[BIOME_COUNT]={HEX(0x2a2050),HEX(0x103828),HEX(0x401808),HEX(0x300c40),HEX(0x102a50),HEX(0x0a3830),HEX(0x402008),HEX(0x141430),HEX(0x0c3020),HEX(0x1e2410),HEX(0x2a1040),HEX(0x402a08)};
    float sat=1.08f; uint32_t sh=SH[R.biome]; float sa=0.10f;
    if(R.timeslow_t>0){ sat=0.45f; sh=HEX(0x105a50); sa=0.35f; }
    if(R.pact){ sh=HEX(0x400818); sa=0.24f; }
    if(S.bloom) post_grade(0,0,SCREEN_W,SCREEN_H,sat,1.04f,sh,sa,0.55f);
  }
  p25_warp_cpu();   /* vue 2.5D */
  draw_ftext();
  draw_curio_labels();
  gfx_clip(PIT_X0,0,PIT_X1,SCREEN_H);
  if(R.levelup_flash>0) gfx_rect_add(PIT_X0,0,PIT_W,SCREEN_H,HEX(0xffe0a0),(int)(R.levelup_flash*60));
  if(R.hurt_t>0) gfx_rect_add(PIT_X0,0,PIT_W,SCREEN_H,HEX(0xff2020),(int)(R.hurt_t*90));
  if(R.low_hp_t>0 && R.sub==RS_PLAY){ int a=(int)(40+30*sinf(g_time*6)); for(int k=0;k<12;k++){ gfx_rect_add(PIT_X0+k,0,1,SCREEN_H,HEX(0xff1010),a*(12-k)/12); gfx_rect_add(PIT_X1-1-k,0,1,SCREEN_H,HEX(0xff1010),a*(12-k)/12); } }
  draw_boss_bar();
  banner();
  tutorial();
  gfx_noclip();
  draw_hud();
  /* etats speciaux */
  if(R.sub==RS_DEAD){
    float a=clampf(R.dead_t/2.0f,0,1);
    gfx_darken(0,0,SCREEN_W,SCREEN_H,(int)(a*200));
    if(R.dead_t>0.5f) text_draw(SCREEN_W/2,150,"VOUS ÊTES TOMBÉ…",HEX(0xff6a5a),3,AL_CENTER,TX_OUTLINE);
  }
  if(R.sub==RS_WIN){
    float a=clampf(R.sub_t*2,0,1);
    gfx_darken(0,0,SCREEN_W,SCREEN_H,(int)(a*140));
    int dy=(int)((1-a)*20);
    gfx_glow(SCREEN_W/2,130,160,HEX(0xffc040),0.25f*a);
    text_draw(SCREEN_W/2,110-dy,"VICTOIRE !",HEX(0xffd84a),5,AL_CENTER,TX_OUTLINE);
    if(a>=1) text_draw(SCREEN_W/2,184,R.dest0?"Les Profondeurs se taisent.":"Le Gouffre est purifié.",HEX(0xf0e6d0),2,AL_CENTER,TX_OUTLINE);
  }
  if(R.sub==RS_LEVELUP) levelup_draw();
  if(R.sub==RS_PAUSE && g_state==GS_RUN) pause_ui();
  if(R.sub==RS_PAUSE && g_state!=GS_RUN) gfx_darken(0,0,SCREEN_W,SCREEN_H,120);
  if(R.sub==RS_EVOLIST) draw_evolist();
  if(R.fade>0) gfx_darken(0,0,SCREEN_W,SCREEN_H,(int)(clampf(R.fade,0,1)*255));
}
