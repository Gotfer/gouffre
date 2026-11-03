/* GOUFFRE - atelier HD : les sols des douze niveaux (texture raccordable de 288 x 384 pixels du jeu).
   Chaque recette : fond peint (bruit sur 5 tons), structure (dalles, pierres, alveoles...),
   reliefs, details et accessoires en relief, lumieres propres avec les codes d'animation du jeu. */
#include "hdgen.h"
#include <string.h>
#include <stdlib.h>

#define FW PIT_W
#define FH 384

/* ---------------- fond peint ---------------- */
typedef struct { C3 p[5]; float con, cell, hamp, sp; uint32_t s; } BaseU;
static void sh_base(float x,float y,void *u,Fr *f){
  BaseU *b=(BaseU*)u;
  float n=0.5f+b->con*(0.72f*fbm2(x,y,b->cell,4,b->s,FH)+0.38f*fbm2(x,y,b->cell*0.25f,3,b->s+9,FH));
  n=sat1(n); float t=n*3.999f; int i=(int)t; float fr=t-i; fr=fr*0.55f+0.45f*sst(0.3f,0.7f,fr);
  f->c=cmix(b->p[i],b->p[i+1],fr); f->a=1; f->h=b->hamp*fbm2(x,y,2.0f,3,b->s+21,FH); f->sp=b->sp; f->hm=HM_ADD;
}
static void base(Cv *v,const uint32_t *pal,float con,float cell,float hamp,float sp,uint32_t s){
  BaseU b; for(int i=0;i<5;i++) b.p[i]=hx(pal[i]); b.con=con; b.cell=cell; b.hamp=hamp; b.sp=sp; b.s=s; fill(v,sh_base,&b);
}
/* touches de pinceau (decalcomanies douces, regroupees par un bruit lent) */
typedef struct { float hl,w,ca,sa,a; C3 c; uint32_t h; } DabU;
static void sh_dab(float lx,float ly,void *u,Fr *f){
  DabU *d=(DabU*)u; float al=lx*d->ca+ly*d->sa, ac=-lx*d->sa+ly*d->ca;
  float ex=fabsf(al)>d->hl?fabsf(al)-d->hl:0; float dd=sqrtf(ex*ex+ac*ac);
  float wl=d->w*0.5f*(1.0f-0.45f*fminf(1.0f,fabsf(al)/d->hl));
  float jit=(hfl(hh3((int)(al*3),(int)(ac*3),d->h))-0.5f)*0.5f;
  f->a=sat1((wl+jit-dd)*HS*0.7f)*d->a; f->c=d->c; f->hm=HM_DECAL;
}
static void dabs(Cv *v,uint32_t col,int n,float ang,float wmin,float wmax,float lmin,float lmax,float a,float thr,uint32_t s){
  for(int k=0,tries=0;k<n && tries<n*30;tries++){
    float x=hg_range(0,FW), y=hg_range(0,FH);
    if(thr>-1){ float g=0.5f+0.5f*fbm2(x,y,48,2,s,FH); if(thr>=0 ? g<thr : g>-thr) continue; }
    k++; float L=hg_range(lmin,lmax), W=hg_range(wmin,wmax), aa=ang+hg_range(-0.1f,0.1f);
    DabU d={L*0.5f,W,cosf(aa),sinf(aa),a,hx(col),(uint32_t)k*2654435761u^s};
    stamp(v,x,y,L*0.5f+W+1,L*0.5f+W+1,sh_dab,&d);
  }
}

/* ---------------- dalles en rangees (catacombes, sanctuaire) ---------------- */
typedef struct { int nr; float ry[40]; int nc[40]; float cx[40][24]; C3 mort; float bevel,sh,tint,chip,mw; uint32_t s; C3 light; } SlabU;
static void sh_slab(float x,float y,void *u,Fr *f){
  SlabU *b=(SlabU*)u; int r=0; while(r<b->nr-1 && y>=b->ry[r+1]) r++;
  float y0=b->ry[r], y1=b->ry[r+1]; int c=0; while(c<b->nc[r]-1 && x>=b->cx[r][c+1]) c++;
  float x0=b->cx[r][c], x1=b->cx[r][c+1];
  float e=fminf(fminf(x-x0,x1-x),fminf(y-y0,y1-y))-b->mw;
  e+=b->chip*(0.55f*fbm2(x,y,1.6f,2,b->s+5,FH)-0.12f*sst(0.6f,0.95f,0.5f+0.5f*fbm2(x,y,6,2,b->s+6,FH)));
  uint32_t id=hh3(r,c,b->s);
  f->a=1; f->hm=HM_ADD;
  if(e<0){ f->c=b->mort; f->h=-0.25f+0.1f*fbm2(x,y,1,2,b->s+8,FH); f->sp=0.03f; return; }
  float t=sst(0,b->bevel,e); float tilt=((hfl(id)-0.5f)*(x-(x0+x1)*0.5f)+(hfl(id>>8)-0.5f)*(y-(y0+y1)*0.5f))*0.03f;
  float tn=(hfl(id>>16)-0.5f)*b->tint;
  f->h=b->sh*t+tilt+0.12f*fbm2(x,y,1.0f,3,b->s+9,FH);
  f->c=c3(1+tn,1+tn,1+tn*0.8f);              /* facteur applique plus bas sur la couleur deja peinte */
  f->sp=0.05f+0.05f*t;
  f->code=-1;
}
static void slabs_apply(Cv *v,SlabU *b){
  for(int y=0;y<v->h;y++) for(int x=0;x<v->w;x++){
    float gx=(x+0.5f)/HS, gy=(y+0.5f)/HS; Fr f; memset(&f,0,sizeof f); sh_slab(gx,gy,b,&f);
    int i=y*v->w+x; C3 o=cv_col(v,i);
    if(f.code==-1){ C3 k=c3(o.r*f.c.r,o.g*f.c.g,o.b*f.c.b); float edge=sat1(1-(f.h-0.1f)/b->sh); k=cmix(k,b->light,0.10f*(1-edge)); cv_set(v,i,k); }
    else cv_set(v,i,cmix(o,f.c,0.85f));
    v->ht[i]+=f.h; v->sp[i]=f.sp;
  }
}
static void slabs_rows(SlabU *b,float rh0,float rh1,float cw0,float cw1,float off){
  float y=0; b->nr=0;
  while(y<FH-0.01f && b->nr<38){ float h=hg_range(rh0,rh1); if(FH-y<rh1*1.4f) h=FH-y; b->ry[b->nr]=y;
    int n=0; float x=-hg_range(0,off); b->cx[b->nr][n++]=x;
    while(x<FW && n<23){ x+=hg_range(cw0,cw1); b->cx[b->nr][n++]=x; } b->nc[b->nr]=n-1; b->nr++; y+=h; }
  b->ry[b->nr]=FH; b->nc[b->nr]=1; b->cx[b->nr][0]=-10; b->cx[b->nr][1]=FW+10;
}

/* ---------------- dalles irregulieres (voronoi) ---------------- */
typedef struct { float cell, gap, bevel, sh, tint; C3 mort; uint32_t s; float jit; } FlagU;
static void flag_apply(Cv *v,FlagU *b){
  for(int y=0;y<v->h;y++) for(int x=0;x<v->w;x++){
    float gx=(x+0.5f)/HS, gy=(y+0.5f)/HS;
    Vo o=voro(gx,gy,b->cell,FH,b->s,b->jit); float e=(o.f2-o.f1)*0.5f-b->gap+0.35f*fbm2(gx,gy,1.5f,2,b->s+3,FH);
    int i=y*v->w+x; C3 c=cv_col(v,i);
    if(e<0){ cv_set(v,i,cmix(c,b->mort,0.85f)); v->ht[i]+=-0.3f; v->sp[i]=0.03f; continue; }
    float t=sst(0,b->bevel,e); float tn=(hfl(o.id)-0.5f)*b->tint;
    float dome=0.25f*sat1(1-o.f1/(b->cell*0.8f));
    cv_set(v,i,csc(c,1+tn)); v->ht[i]+=b->sh*t+dome+0.1f*fbm2(gx,gy,0.8f,2,b->s+7,FH);
  }
}

/* ---------------- petites aides ---------------- */
static float rnd_x(void){ return hg_range(4,FW-4); }
static float rnd_y(void){ return hg_range(0,FH); }
static int ci(int v){ return v<1?1:(v>255?255:v); }

/* ======================================================================= */
static void f_catacombes(Cv *v,uint32_t s,Glows *g){
  static const uint32_t P[5]={0x5c5666,0x6a6474,0x787282,0x868092,0x9890a4};
  base(v,P,0.42f,64,0.15f,0.05f,s);
  dabs(v,0x504a5c,110,-0.62f,3,7,14,40,0.35f,0.52f,s+71);
  dabs(v,0xa8a0b6,90,-0.62f,2,5,10,30,0.28f,-0.5f,s+72);
  SlabU *b=(SlabU*)calloc(1,sizeof(SlabU)); slabs_rows(b,22,30,26,46,30);
  b->mort=hx(0x2a2532); b->bevel=1.4f; b->sh=1.3f; b->tint=0.14f; b->chip=1.0f; b->mw=0.55f; b->s=s+4; b->light=hx(0xd8d0e4);
  slabs_apply(v,b); free(b);
  for(int i=0;i<14;i++) acc_crack(v,rnd_x(),rnd_y(),hg_range(5,14),0.35f,0.35f,hx(0x3a3444),0,0,0,s+100+i);
  for(int i=0;i<18;i++){ float x=rnd_x(), y=rnd_y(); for(int k=0;k<10;k++){ float a=hg_range(0,TAU), d=hg_range(0,3.5f); acc_blob(v,x+cosf(a)*d,y+sinf(a)*d*0.6f,hg_range(0.6f,1.4f),hg_range(0.4f,0.9f),a,hx(0x4a6a40),0.2f,0.04f,0,0); } }
  for(int i=0;i<26;i++){ float x=rnd_x(), y=rnd_y(), a=hg_range(0,TAU), l=hg_range(2.5f,5.5f); acc_bone(v,x,y,x+cosf(a)*l,y+sinf(a)*l,hg_range(0.45f,0.65f),hx(0xd8ccb4)); }
  for(int i=0;i<6;i++) acc_skull(v,hg_range(10,FW-10),rnd_y(),hg_range(1.9f,2.4f),hg_range(-0.5f,0.5f),hx(0xe0d4bc));
  for(int i=0;i<4;i++){ float x=hg_range(14,FW-14), y=rnd_y(); int n=hg_irange(2,4);
    for(int k=0;k<n;k++) acc_candle(v,x+k*3.2f-n*1.6f+hg_range(-0.6f,0.6f),y+hg_range(-1.2f,1.2f),hg_range(0.8f,1.05f),hg_range(3,6),hg_irange(1,200),g,x,y);
    glow_add(g,x,y-4,0xffb060,34); }
  acc_pebbles(v,FW*0.5f,FH*0.5f,140,60,hx(0x8a8494),s+9);
}
static void f_foret(Cv *v,uint32_t s,Glows *g){
  static const uint32_t P[5]={0x2a3c22,0x334a28,0x3e5830,0x4a6838,0x5a7a42};
  base(v,P,0.5f,40,0.35f,0.04f,s);
  dabs(v,0x26381e,90,-0.62f,3,8,14,44,0.22f,0.52f,s+71);
  /* humus : zones de terre et de feuilles mortes, fondues dans la mousse */
  for(int y=0;y<v->h;y++) for(int x=0;x<v->w;x++){ float gx=(x+0.5f)/HS, gy=(y+0.5f)/HS; int i=y*v->w+x;
    float m=0.5f+0.5f*fbm2(gx,gy,44,4,s+11,FH);
    if(m>0.5f){ float k=sst(0.5f,0.7f,m); float q=0.5f+0.5f*fbm2(gx,gy,2.2f,3,s+12,FH);
      C3 d=cmix(hx(0x33281c),hx(0x5a4630),q); cv_set(v,i,cmix(cv_col(v,i),d,k*0.9f)); v->ht[i]-=0.5f*k; }
    /* coussins de mousse : touffes rondes et vives */
    float mm=0.5f+0.5f*fbm2(gx,gy,26,3,s+14,FH);
    if(mm>0.6f && m<0.62f){ float k=sst(0.6f,0.67f,mm); Vo o=voro(gx,gy,2.6f,FH,s+13,0.9f); float bump=sqrtf(sat1(1-o.f1/2.2f));
      C3 mc=cmix(hx(0x3e6a26),hx(0x86b244),0.25f+0.6f*bump+0.15f*hfl(o.id)); cv_set(v,i,cmix(cv_col(v,i),mc,k)); v->ht[i]+=k*(0.5f*bump+0.35f); v->sp[i]=0.03f; }
  }
  /* racines noueuses qui traversent le sol */
  for(int i=0;i<5;i++){ float x=rnd_x(), y=rnd_y(), a=hg_range(0,TAU); acc_root(v,x,y,hg_range(50,90),hg_range(2.4f,3.4f),a,hx(0x4e3220),s+30+i);
    for(int k=0;k<2;k++) acc_root(v,x+cosf(a)*hg_range(10,30),y+sinf(a)*hg_range(10,30),hg_range(14,26),hg_range(1.0f,1.6f),a+hg_range(-1.2f,1.2f),hx(0x5a3a24),s+60+i*3+k); }
  /* litiere : feuilles mortes en tas */
  static const uint32_t L[6]={0x9a6a30,0x7a4e24,0xb88a40,0x6a5a2a,0x8a3a1e,0x5e6a2e};
  for(int c=0;c<46;c++){ float cx=rnd_x(), cy=rnd_y(); int n=hg_irange(8,18);
    for(int k=0;k<n;k++){ float a=hg_range(0,TAU), d=sqrtf(hg_rand())*hg_range(4,9); acc_leaf(v,cx+cosf(a)*d,cy+sinf(a)*d*0.8f,hg_range(1.4f,2.4f),hg_range(0,TAU),csc(hx(L[hg_irange(0,5)]),hg_range(0.8f,1.1f))); } }
  for(int i=0;i<40;i++) acc_twig(v,rnd_x(),rnd_y(),hg_range(3,7),hg_range(0,TAU),hx(0x5a4230),s+500+(uint32_t)i);
  /* fougeres en bouquets */
  for(int i=0;i<16;i++){ float x=hg_range(8,FW-8), y=rnd_y(); int n=hg_irange(3,5); float a0=hg_range(0,TAU);
    for(int k=0;k<n;k++) acc_fern(v,x,y,hg_range(6,10),a0+k*TAU/n+hg_range(-0.3f,0.3f),hx(0x2e5e22),hx(0x6aa040),s+700+(uint32_t)(i*8+k)); }
  /* trefles et herbes fines */
  for(int i=0;i<30;i++){ float x=rnd_x(), y=rnd_y(); for(int k=0;k<6;k++) acc_flower(v,x+hg_range(-4,4),y+hg_range(-3,3),0.85f,hx(0x4a8a3a),hx(0x2e6a2a),3,hg_range(0,TAU)); }
  for(int i=0;i<60;i++) acc_grass(v,rnd_x(),rnd_y(),2.0f,6,hx(0x3e7a32),hx(0x7ab858),2.4f,s+200+i);
  for(int i=0;i<12;i++){ float x=rnd_x(), y=rnd_y(); acc_rock(v,x,y,hg_range(1.8f,3.2f),hx(0x4a4a40),hx(0x8a8a78),hg_range(1.5f,2.4f),s+800+(uint32_t)i); acc_moss(v,x-0.6f,y-0.8f,hg_range(1.0f,1.8f),hx(0x3e6a26),hx(0x7aa83e),s+820+(uint32_t)i); }
  /* champignons lumineux en touffes, et un cercle de fees */
  for(int i=0;i<15;i++){ float x=hg_range(10,FW-10), y=rnd_y(); bool teal=hg_chance(0.35f); int n=hg_irange(2,5);
    for(int k=0;k<n;k++){ float a=hg_range(0,TAU), d=hg_range(0,4.5f); acc_mushroom(v,x+cosf(a)*d,y+sinf(a)*d*0.8f,hg_range(1.5f,3.4f),teal,teal?128+hg_irange(0,60):16+hg_irange(0,60)); }
    glow_add(g,x,y-3,teal?0x45c2c8:0xf59ac0,24); }
  { float x=hg_range(30,FW-30), y=rnd_y(); bool teal=hg_chance(0.5f); for(int k=0;k<14;k++){ float a=k*TAU/14+hg_range(-0.1f,0.1f); acc_mushroom(v,x+cosf(a)*11,y+sinf(a)*9,hg_range(1.3f,1.9f),teal,teal?128+k*4:16+k*4); } glow_add(g,x,y,teal?0x45c2c8:0xf59ac0,30); }
  for(int i=0;i<60;i++){ bool c2=hg_chance(0.4f); acc_blob(v,rnd_x(),rnd_y(),0.3f,0.3f,0,c2?hx(0x9af0f0):hx(0xffb8de),0.4f,0.3f,0.9f,c2?128+hg_irange(0,120):16+hg_irange(0,110)); }
  acc_pebbles(v,FW*0.5f,FH*0.5f,150,40,hx(0x7a7a64),s+9);
}
static void f_forges(Cv *v,uint32_t s,Glows *g){
  static const uint32_t P[5]={0x463632,0x54423c,0x624e46,0x725c52,0x846c60};
  base(v,P,0.42f,48,0.15f,0.06f,s);
  FlagU fl={22,0.55f,1.3f,1.1f,0.16f,hx(0x241816),s+3,0.85f}; flag_apply(v,&fl);
  dabs(v,0x2a1e1a,80,-0.62f,3,8,14,44,0.35f,0.55f,s+71);
  for(int r=0;r<2;r++){
    float x0=FW*(r+0.5f)/2+hg_range(-20,20); int k1=hg_irange(1,2), k2=hg_irange(2,4); float a1=hg_range(10,26), a2=hg_range(4,9), p1=hg_range(0,TAU), p2=hg_range(0,TAU);
    for(int y=0;y<v->h;y++){
      float gy=(y+0.5f)/HS; float t=TAU*gy/FH; float xc=x0+a1*sinf(k1*t+p1)+a2*sinf(k2*t+p2); float w=2.6f+1.5f*sinf(3*t+p2);
      for(int x=(int)((xc-w-4)*HS);x<=(int)((xc+w+4)*HS);x++){ if(x<0||x>=v->w) continue;
        float gx=(x+0.5f)/HS; float d=fabsf(gx-xc); int i=y*v->w+x;
        if(d<w){ /* lave et croute flottante */
          Vo o=voro(gx,gy-0.0f,3.2f,FH,s+40+r,1.0f); float cr=(o.f2-o.f1)*0.5f;
          bool crust = cr>0.55f && d<w-0.6f && hfl(o.id)<0.55f;
          if(crust){ cv_set(v,i,cmix(hx(0x2a1410),hx(0x4a2418),hfl(o.id>>8))); v->ht[i]=0.25f; v->sp[i]=0.1f; v->em[i]=0.08f; }
          else { cv_set(v,i,hx(0x5a1a08)); v->ht[i]=-0.4f; v->sp[i]=0.0f; v->em[i]=1.0f; }
          v->cd[i]=(uint8_t)ci(1+((int)(gy*1.3f+d*9)&127));
        } else if(d<w+1.6f){ cv_set(v,i,hx(0x1e1210)); v->ht[i]=0.9f*sat1((d-w)/0.8f); v->sp[i]=0.15f; float e=sat1(1-(d-w)/1.2f)*0.35f; if(e>v->em[i]){ v->em[i]=e; v->cd[i]=(uint8_t)ci(1+((int)(gy*1.3f+w*9)&127)); } }
        else if(d<w+4){ C3 c=cv_col(v,i); cv_set(v,i,cmix(c,hx(0x3a1a12),0.5f*(1-(d-w-1.6f)/2.4f))); }
      }
      if(y%(48*HS)==20*HS) glow_add(g,xc,gy,0xff7a22,30);
    }
  }
  for(int k=0;k<4;k++){ float gx=hg_range(10,FW-34), gy=rnd_y();
    acc_box(v,gx-1,gy-1,gx+23,gy+13,hx(0x2a2226),0.6f,0.3f,0.4f);
    for(int y=(int)(gy*HS);y<(int)((gy+12)*HS);y++) for(int x=(int)(gx*HS);x<(int)((gx+22)*HS);x++){ int i=cv_i(v,x,y); cv_set(v,i,hx(0x8a2a08)); v->ht[i]=-1.5f; v->em[i]=0.9f; v->cd[i]=(uint8_t)(130+((x/HS)*7+(y/HS)*13)%100); v->sp[i]=0; }
    for(int b=0;b<=22;b+=4) acc_box(v,gx+b-0.6f,gy-0.6f,gx+b+0.6f,gy+12.6f,hx(0x5a5258),1.8f,0.4f,0.7f);
    acc_box(v,gx-0.8f,gy-0.8f,gx+22.8f,gy+0.6f,hx(0x4a4248),1.9f,0.4f,0.7f); acc_box(v,gx-0.8f,gy+11.4f,gx+22.8f,gy+12.8f,hx(0x4a4248),1.9f,0.4f,0.7f);
    for(int r=0;r<4;r++) acc_blob(v,gx+(r&1?22:0),gy+(r&2?12:0),0.7f,0.7f,0,hx(0x9a9098),2.2f,0.8f,0,0);
    glow_add(g,gx+11,gy+6,0xff6a1a,26); }
  for(int i=0;i<10;i++) acc_crack(v,rnd_x(),rnd_y(),hg_range(10,24),0.5f,0.4f,hx(0xff8a2a),0.9f,40,3,s+300+i);
  for(int i=0;i<30;i++) acc_blob(v,rnd_x(),rnd_y(),hg_range(0.5f,1.1f),hg_range(0.3f,0.6f),hg_range(0,PI),hx(0x6a6268),0.5f,0.7f,0,0);
}
static void f_coeur(Cv *v,uint32_t s,Glows *g){
  static const uint32_t P[5]={0x4a3a68,0x564476,0x624e84,0x705a94,0x8068a6};
  base(v,P,0.45f,48,0.2f,0.08f,s);
  dabs(v,0x423460,110,-0.62f,3,7,14,40,0.3f,0.52f,s+71); dabs(v,0x927cba,90,-0.62f,2,5,10,30,0.25f,-0.5f,s+72);
  for(int y=0;y<v->h;y++) for(int x=0;x<v->w;x++){
    float gx=(x+0.5f)/HS, gy=(y+0.5f)/HS; Vo o=voro(gx,gy,30,FH,s+5,0.9f); float e=(o.f2-o.f1)*0.5f+0.3f*fbm2(gx,gy,2,2,s+6,FH);
    int i=y*v->w+x; C3 c=cv_col(v,i);
    float dome=0.9f*sqrtf(sat1(1-o.f1/24.0f));
    if(e<0.75f){ float k=1-e/0.75f; cv_set(v,i,cmix(c,hx(0x8a4ad8),k)); v->ht[i]=-0.5f*k; v->em[i]=k*k; v->cd[i]=(uint8_t)ci(1+((int)(o.f1*0.9f+gy*0.15f)&127)); v->sp[i]=0.3f; }
    else { float rim=sst(2.2f,0.75f,e); cv_set(v,i,cmix(csc(c,0.86f+0.22f*sat1(o.f1/24)),hx(0x2a1a40),rim*0.5f)); v->ht[i]+=dome*sst(0.75f,2.0f,e); v->sp[i]=0.12f; }
  }
  for(int i=0;i<9;i++){ float x=hg_range(10,FW-10), y=rnd_y(); acc_crystals(v,x,y,5,hg_irange(2,4),hx(0xe2c8ff),hx(0x8a5ac8),0.8f,128+hg_irange(0,80),s+50+i); glow_add(g,x,y-2,0xa060ff,22); }
  for(int i=0;i<70;i++) acc_blob(v,rnd_x(),rnd_y(),0.35f,0.35f,0,hx(0xb8a0e0),0.2f,0.3f,0,0);
}
static void f_givre(Cv *v,uint32_t s,Glows *g){
  static const uint32_t P[5]={0x6676b4,0x7888c4,0x8a9ad2,0x9eace0,0xb6c4ee};
  base(v,P,0.4f,64,0.08f,0.75f,s);
  for(int y=0;y<v->h;y++) for(int x=0;x<v->w;x++){
    float gx=(x+0.5f)/HS, gy=(y+0.5f)/HS; int i=y*v->w+x; C3 c=cv_col(v,i);
    Vo o=voro(gx,gy,40,FH,s+3,0.9f); float e=(o.f2-o.f1)*0.5f;
    if(e<0.35f){ cv_set(v,i,cmix(c,hx(0xe8f2ff),0.7f*(1-e/0.35f))); v->ht[i]+=0.15f; }
    float deep=0.5f+0.5f*fbm2(gx,gy,32,3,s+4,FH); if(deep<0.35f) cv_set(v,i,cmix(cv_col(v,i),hx(0x4a5a9a),(0.35f-deep)*1.6f));
    float fr=ridge2(gx,gy,6,3,s+5,FH); if(fr>0.82f) cv_set(v,i,cmix(cv_col(v,i),hx(0xf4f8ff),(fr-0.82f)*3.0f));
    float sn=0.5f+0.5f*fbm2(gx,gy,36,4,s+6,FH);
    if(sn>0.62f){ float k=sst(0.62f,0.72f,sn); cv_set(v,i,cmix(cv_col(v,i),cmix(hx(0xdfe8fb),hx(0xf8fbff),0.5f+0.5f*fbm2(gx,gy,1.5f,2,s+7,FH)),k)); v->ht[i]+=1.6f*k*(sn-0.55f)*3; v->sp[i]=0.75f-0.6f*k; }
  }
  for(int i=0;i<40;i++){ float x=rnd_x(), y=rnd_y(); float r=hg_range(0.4f,1.0f); acc_blob(v,x,y,r,r,0,hx(0xc8d8ff),0.1f,0.9f,0,0); acc_blob(v,x-0.2f*r,y-0.2f*r,r*0.4f,r*0.4f,0,hx(0xffffff),0.15f,0.9f,0,0); }
  for(int i=0;i<8;i++) acc_crack(v,rnd_x(),rnd_y(),hg_range(8,20),0.3f,0.2f,hx(0xd8e4ff),0,0,0,s+300+i);
  for(int i=0;i<7;i++){ float x=hg_range(10,FW-10), y=rnd_y(); acc_crystals(v,x,y,5,hg_irange(2,3),hx(0xeefaff),hx(0x6ab0e4),0.6f,128+hg_irange(0,80),s+60+i); glow_add(g,x,y-2,0x9ae8ff,22); }
}
static void f_cite(Cv *v,uint32_t s,Glows *g){
  static const uint32_t P[5]={0x34605a,0x3e6e66,0x4a7c74,0x588a82,0x689c92};
  base(v,P,0.4f,48,0.12f,0.2f,s);
  dabs(v,0x2c544e,110,-0.62f,3,7,14,40,0.3f,0.52f,s+71);
  for(int y=0;y<v->h;y++) for(int x=0;x<v->w;x++){
    float gx=(x+0.5f)/HS, gy=(y+0.5f)/HS; int i=y*v->w+x; C3 c=cv_col(v,i);
    int tx=(int)(gx/24), ty=(int)(gy/24); float lx=gx-tx*24, ly=gy-ty*24; float e=fminf(fminf(lx,24-lx),fminf(ly,24-ly))-0.6f+0.3f*fbm2(gx,gy,1.2f,2,s+3,FH);
    uint32_t id=hh3(tx,ty%16,s); float tn=(hfl(id)-0.5f)*0.12f;
    if(e<0){ cv_set(v,i,cmix(hx(0x0e2422),hx(0x2a5a3a),sat1(0.5f+fbm2(gx,gy,2,2,s+4,FH)))); v->ht[i]-=0.3f; v->sp[i]=0.5f; continue; }
    float t=sst(0,1.2f,e); cv_set(v,i,csc(c,1+tn)); v->ht[i]+=1.0f*t;
    if((id>>12)%5==0){ float cx=lx-12, cy=ly-12; float m=fmaxf(fabsf(cx),fabsf(cy)); float rr=fmodf(m,4.0f); if(m<9 && rr<0.7f){ v->ht[i]-=0.35f; cv_set(v,i,csc(cv_col(v,i),0.8f)); } }
  }
  float x0=FW*0.5f+hg_range(-30,30), a1=hg_range(8,18), p1=hg_range(0,TAU);
  for(int y=0;y<v->h;y++){ float gy=(y+0.5f)/HS; float t=TAU*gy/FH; float xc=x0+a1*sinf(2*t+p1); float w=9+2*sinf(3*t);
    for(int x=(int)((xc-w-3)*HS);x<=(int)((xc+w+3)*HS);x++){ if(x<0||x>=v->w) continue; float gx=(x+0.5f)/HS; float d=fabsf(gx-xc); int i=y*v->w+x;
      if(d<w){ float dep=sat1((w-d)/3.0f); float ca=ridge2(gx,gy,5,3,s+20,FH);
        cv_set(v,i,cmix(hx(0x3a8a8a),hx(0x163e48),dep)); v->ht[i]=-1.6f*dep-0.2f; v->sp[i]=0.95f;
        float e=sat1((ca-0.76f)*3.2f)*0.28f; if(e>0.02f){ v->em[i]=e; v->cd[i]=(uint8_t)ci(1+(((x/HS)*5+(y/HS)*3)&14)); } }
      else if(d<w+2){ cv_set(v,i,hx(0x6a948a)); v->ht[i]=1.1f; v->sp[i]=0.3f; }
    }
    if(y%(96*HS)==40*HS) glow_add(g,xc,gy,0x3ab8c0,26); }
  for(int i=0;i<40;i++){ float x=rnd_x(), y=rnd_y(); float a=-PI*0.5f+hg_range(-0.6f,0.6f); for(int k=0;k<3;k++) acc_reed(v,x+k*0.8f,y,hg_range(3,7),a+hg_range(-0.3f,0.3f),hg_chance(0.5f)?hx(0x3a9a5a):hx(0x4ab86a),false); }
  for(int i=0;i<10;i++){ float x=hg_range(8,FW-8), y=rnd_y(); bool pink=hg_chance(0.5f); acc_coral(v,x,y,4.5f,pink?hx(0xf59ac0):hx(0x5af0d0),0.8f,pink?30+hg_irange(0,60):140+hg_irange(0,60)); glow_add(g,x,y-3,pink?0xf59ac0:0x5af0d0,22); }
  for(int i=0;i<30;i++) acc_blob(v,rnd_x(),rnd_y(),0.6f,0.5f,hg_range(0,PI),hx(0xd8d0b8),0.5f,0.5f,0,0);
}
static void f_ruche(Cv *v,uint32_t s,Glows *g){
  const float hs=16.0f/1.7320508f, cw=1.5f*hs;
  for(int y=0;y<v->h;y++) for(int x=0;x<v->w;x++){
    float gx=(x+0.5f)/HS, gy=(y+0.5f)/HS; int ci0=(int)floorf(gx/cw); float best=1e9f, sec=1e9f; int bi=0,bj=0;
    for(int i=ci0-1;i<=ci0+1;i++){ float ccx=i*cw, off=(i&1)?8.0f:0.0f; int cj=(int)floorf((gy-off)/16.0f);
      for(int j=cj-1;j<=cj+1;j++){ float ccy=j*16.0f+off; float dx=gx-ccx, dy=gy-ccy; float d=sqrtf(dx*dx+dy*dy); if(d<best){ sec=best; best=d; bi=i; bj=j; } else if(d<sec) sec=d; } }
    float edge=(sec-best)*0.5f; int jj=((bj%24)+24)%24; uint32_t h=hh3(bi,jj,s); int type=(int)(h%10);
    float n=fbm2(gx,gy,1.5f,2,s+51,FH); int i=y*v->w+x;
    if(edge<1.4f){ float t=sst(1.4f,0.3f,edge); cv_set(v,i,cmix(hx(0xd8a85a),hx(0xf0c878),t*0.5f+0.2f*n)); v->ht[i]=1.6f+0.4f*t; v->sp[i]=0.45f; }
    else if(type<1 || (type==1 && (h>>8)%3==0)){ float k=sat1(best/9.0f); cv_set(v,i,cmix(hx(0xf8b030),hx(0x9a4a08),k)); v->ht[i]=0.6f-0.4f*k; v->sp[i]=0.95f;
      float e=sat1(1-best/7.0f); if(e>0){ v->em[i]=e*0.85f; v->cd[i]=(uint8_t)ci(1+(int)clampf(best*8,0,89)+(e>0.6f?90:0)); } }
    else if(type<3){ float k=sat1(best/8.0f); cv_set(v,i,cmix(hx(0xf4dca0),hx(0xc89a58),k)); v->ht[i]=1.3f-0.5f*k*k; v->sp[i]=0.35f; }
    else { float k=sat1(best/8.0f); cv_set(v,i,cmix(hx(0x2e1806),hx(0x7a5222),k*k)); v->ht[i]=-1.2f+1.4f*k*k; v->sp[i]=0.2f; }
  }
  for(int i=0;i<16;i++){ float x=rnd_x(), y=rnd_y(); acc_blob(v,x,y,hg_range(1.5f,3),hg_range(2.5f,5),hg_range(-0.3f,0.3f)+PI*0.5f,hx(0xf0a830),1.9f,0.95f,0.4f,60); }
  for(int k=0,t=0;k<14 && t<3000;t++){ int x=hg_irange(0,v->w-1), y=hg_irange(0,v->h-1); if(v->em[y*v->w+x]>0.6f){ glow_add(g,(x+0.5f)/HS,(y+0.5f)/HS,0xffa030,24); k++; } }
}
static void f_neant(Cv *v,uint32_t s,Glows *g){
  static const uint32_t P[5]={0x24203a,0x2c2846,0x363050,0x40395e,0x4c446e};
  base(v,P,0.45f,48,0.1f,0.6f,s);
  for(int y=0;y<v->h;y++) for(int x=0;x<v->w;x++){
    float gx=(x+0.5f)/HS, gy=(y+0.5f)/HS; int i=y*v->w+x; Vo o=voro(gx,gy,26,FH,s+3,0.9f); float e=(o.f2-o.f1)*0.5f;
    float rip=0.25f*sinf(o.f1*1.6f+hfl(o.id)*6)*sst(0,3,e);
    if(e<0.5f){ cv_set(v,i,cmix(cv_col(v,i),hx(0x140f22),0.8f)); v->ht[i]=-0.4f; v->sp[i]=0.2f; }
    else { v->ht[i]+=0.9f*sst(0.5f,2,e)+rip; cv_set(v,i,csc(cv_col(v,i),0.92f+0.16f*hfl(o.id))); v->sp[i]=0.85f; }
  }
  dabs(v,0x5e5690,90,-0.62f,2,6,10,32,0.25f,-0.5f,s+72);
  for(int i=0;i<160;i++){ float x=rnd_x(), y=rnd_y(); bool c=hg_chance(0.3f); acc_blob(v,x,y,0.3f,0.3f,0,c?hx(0x9a8aff):hx(0xd8e0f8),0.1f,0.5f,hg_chance(0.4f)?0.9f:0.0f,1+hg_irange(0,60)); }
  for(int i=0;i<9;i++){ float x=hg_range(0,FW), y=rnd_y(), a=hg_range(0,TAU); int len=hg_irange(30,70);
    for(int k=0;k<len;k++){ a+=hg_range(-0.35f,0.35f); x+=cosf(a); y+=sinf(a)*0.9f;
      acc_blob(v,x,y,0.45f,0.45f,0,hx(0xe0f0ff),-0.3f,0.2f,1.0f,128+((k*3)&127));
      if(k%14==7) glow_add(g,x,fmodf(y+FH*4,FH),0xc8d8ff,22);
      if(hg_chance(0.06f)){ float b2=a+hg_range(-1.2f,1.2f), bx=x, by=y; int nq=hg_irange(6,14); for(int q=0;q<nq;q++){ bx+=cosf(b2); by+=sinf(b2)*0.9f; acc_blob(v,bx,by,0.3f,0.3f,0,hx(0xa8b8e8),-0.2f,0.2f,0.8f,128+((q*5)&127)); } } } }
}
static void f_jardin(Cv *v,uint32_t s,Glows *g){
  static const uint32_t P[5]={0x356a4c,0x3e7a54,0x48895c,0x549a66,0x62aa72};
  base(v,P,0.45f,48,0.35f,0.05f,s);
  dabs(v,0x2e5e44,120,-0.62f,3,8,14,44,0.35f,0.52f,s+71); dabs(v,0x78c084,100,-0.62f,2,6,10,32,0.28f,-0.5f,s+72);
  for(int i=0;i<260;i++) acc_grass(v,rnd_x(),rnd_y(),2.2f,7,hx(0x3e8a4a),hx(0x9ae09a),2.6f,s+400+i);
  { float x0=FW*hg_range(0.35f,0.65f), a1=hg_range(30,48), p1=hg_range(0,TAU), a2=hg_range(6,12), p2=hg_range(0,TAU);
    for(int y=0;y<FH;y+=13){ float t=TAU*y/FH; float xc=x0+a1*sinf(t+p1)+a2*sinf(3*t+p2)+hg_range(-3,3);
      acc_blob(v,xc,y+0.4f,hg_range(5.2f,6.6f),hg_range(3.6f,4.4f),hg_range(-0.2f,0.2f),hx(0x2e4a3a),0.0f,0.05f,0,0);
      acc_rock(v,xc,(float)y,hg_range(4.6f,5.6f),hx(0x8e8a78),hx(0xd4d0bc),1.4f,s+(uint32_t)y); } }
  for(int i=0;i<18;i++){ float x=rnd_x(), y=rnd_y(); for(int k=0;k<7;k++) acc_flower(v,x+hg_range(-4,4),y+hg_range(-3,3),0.9f,hx(0x3a9a5a),hx(0x2e7a4a),3,hg_range(0,TAU)); }
  static const uint32_t FL[4]={0xf6f2ea,0xf59ac0,0xc8a8ff,0xf9dc5c};
  for(int i=0;i<34;i++){ float x=rnd_x(), y=rnd_y(); C3 fc=hx(FL[hg_irange(0,3)]); int n=hg_irange(4,9);
    for(int k=0;k<n;k++) acc_flower(v,x+hg_range(-6,6),y+hg_range(-4,4),hg_range(0.8f,1.2f),fc,hx(0xf9c84a),5,hg_range(0,TAU)); }
  for(int i=0;i<12;i++){ float x=hg_range(10,FW-10), y=rnd_y(); bool teal=hg_chance(0.4f);
    acc_line(v,x,y,x,y-5,0.35f,hx(0x2e7a3a),1.5f,0.05f,HM_ON);
    acc_blob(v,x,y-5.5f,1.3f,1.7f,0,teal?hx(0x9af8e8):hx(0xeaff7a),2.4f,0.4f,1.0f,teal?128+hg_irange(0,90):1+hg_irange(0,90));
    glow_add(g,x,y-5,teal?0x7af0e0:0xd8ff6a,26); }
  for(int i=0;i<40;i++){ bool c2=hg_chance(0.3f); acc_blob(v,rnd_x(),rnd_y(),0.32f,0.32f,0,c2?hx(0xb8fff0):hx(0xf0ffa0),0.6f,0.3f,1.0f,c2?128+hg_irange(0,120):1+hg_irange(0,120)); }
}
static void f_marais(Cv *v,uint32_t s,Glows *g){
  static const uint32_t P[5]={0x3a3824,0x46432a,0x524e32,0x5e5a3a,0x6c6844};
  base(v,P,0.5f,48,0.35f,0.3f,s);
  dabs(v,0x2e2c1e,110,-0.62f,3,8,14,44,0.3f,0.52f,s+71);
  for(int y=0;y<v->h;y++) for(int x=0;x<v->w;x++){
    float gx=(x+0.5f)/HS, gy=(y+0.5f)/HS; int i=y*v->w+x;
    float m=0.5f+0.5f*fbm2(gx,gy,36,3,s+11,FH)+0.06f*fbm2(gx,gy,8,2,s+12,FH);
    if(m>0.6f){ float k=sst(0.6f,0.68f,m); cv_set(v,i,cmix(cv_col(v,i),cmix(hx(0x26321e),hx(0x141c10),sat1((m-0.6f)*6)),k)); v->ht[i]=-1.4f*k+v->ht[i]*(1-k); v->sp[i]=0.3f+0.65f*k;
      float al=ridge2(gx,gy,7,3,s+13,FH); if(k>0.9f && al>0.85f){ v->em[i]=(al-0.85f)*4.0f; v->cd[i]=(uint8_t)ci(1+(int)(al*119)); } }
    else if(m>0.56f){ float k=sst(0.56f,0.6f,m); cv_set(v,i,cmix(cv_col(v,i),hx(0x1a180e),k*0.7f)); v->sp[i]=0.5f; }
  }
  for(int i=0,n=0;i<4000 && n<22;i++){ int x=hg_irange(0,v->w-1), y=hg_irange(0,v->h-1); if(v->ht[y*v->w+x]>-1.2f) continue; n++;
    acc_lily(v,(x+0.5f)/HS,(y+0.5f)/HS,hg_range(2.5f,4.2f),hg_range(0,TAU),hx(0x5a8a2e),hg_chance(0.3f)); }
  for(int i=0,n=0;i<6000 && n<40;i++){ int x=hg_irange(0,v->w-1), y=hg_irange(0,v->h-1); float h=v->ht[y*v->w+x]; if(h<-0.7f || h>-0.2f) continue; n++;
    float gx=(x+0.5f)/HS, gy=(y+0.5f)/HS; int k=hg_irange(2,4); for(int q=0;q<k;q++) acc_reed(v,gx+q*1.1f-k*0.5f,gy,hg_range(5,9),-PI*0.5f+hg_range(-0.35f,0.35f),q&1?hx(0x7a8a3a):hx(0x5a6a2a),hg_chance(0.5f)); }
  for(int i=0,n=0;i<6000 && n<30;i++){ int x=hg_irange(0,v->w-1), y=hg_irange(0,v->h-1); if(v->ht[y*v->w+x]>-1.2f) continue; n++;
    acc_blob(v,(x+0.5f)/HS,(y+0.5f)/HS,hg_range(0.4f,0.8f),hg_range(0.4f,0.8f),0,hx(0xa8c070),0.4f,0.95f,0.8f,128+hg_irange(0,120)); }
  for(int i=0,n=0;i<20000 && n<700;i++){ int x=hg_irange(0,v->w-1), y=hg_irange(0,v->h-1); float h=v->ht[y*v->w+x]; if(h>-0.9f) continue; n++;   /* lentilles d'eau */
    float gx=(x+0.5f)/HS, gy=(y+0.5f)/HS; acc_blob(v,gx,gy,hg_range(0.25f,0.5f),hg_range(0.25f,0.45f),hg_range(0,PI),hg_chance(0.5f)?hx(0x7aa83a):hx(0x9ac24a),0.15f,0.2f,0,0); }
  for(int i=0;i<24;i++){ float x=rnd_x(), y=rnd_y(); acc_moss(v,x,y,hg_range(2,4.5f),hx(0x4a5a26),hx(0x7a8a3a),s+900+(uint32_t)i); }
  for(int i=0;i<16;i++) acc_root(v,rnd_x(),rnd_y(),hg_range(10,22),hg_range(0.8f,1.4f),hg_range(0,TAU),hx(0x3e3220),s+940+(uint32_t)i);
  for(int i=0;i<10;i++){ float x=rnd_x(), y=rnd_y(), a=hg_range(0,TAU), l=hg_range(3,6); acc_bone(v,x,y,x+cosf(a)*l,y+sinf(a)*l,0.55f,hx(0xb8b090)); }
  for(int i=0;i<3;i++) acc_skull(v,hg_range(10,FW-10),rnd_y(),2.1f,hg_range(-0.6f,0.6f),hx(0xb0a888));
  for(int i=0;i<8;i++){ float x=hg_range(10,FW-10), y=rnd_y(); acc_blob(v,x,y,1.3f,1.3f,0,hx(0xc8f070),0.3f,0.3f,1.0f,1+hg_irange(0,60)); glow_add(g,x,y,0xa8e040,26); }
}
static void f_mines(Cv *v,uint32_t s,Glows *g){
  static const uint32_t P[5]={0x34303e,0x3c3848,0x464252,0x504c5e,0x5c586c};
  base(v,P,0.42f,48,0.15f,0.12f,s);
  FlagU fl={18,0.35f,0.6f,0.6f,0.18f,hx(0x1e1c26),s+3,0.95f}; flag_apply(v,&fl);
  for(int i=0;i<16;i++) acc_crack(v,rnd_x(),rnd_y(),hg_range(10,26),0.35f,0.3f,hx(0x1a1822),0,0,0,s+100+i);
  float xr=FW*0.5f+hg_range(-50,50);
  acc_pebbles(v,xr,FH*0.25f,13,220,hx(0x5a5668),s+20); acc_pebbles(v,xr,FH*0.75f,13,220,hx(0x5a5668),s+21);
  for(int y=0;y<v->h;y++) for(int x=(int)((xr-15)*HS);x<(int)((xr+15)*HS);x++){ int i=cv_i(v,x,y); float gx=(x+0.5f)/HS, gy=(y+0.5f)/HS; float n=hfl(hh3(x,y,s+22));
    cv_set(v,i,cmix(cv_col(v,i),n<0.5f?hx(0x2a2834):hx(0x6a6680),0.35f)); v->ht[i]+=0.3f*n; (void)gx; (void)gy; }
  for(int yy=0;yy<FH;yy+=8){ acc_box(v,xr-12.5f,yy+0.2f,xr+12.5f,yy+3.6f,hx(0x6a4a2e),1.2f,0.5f,0.08f);
    for(int x=(int)((xr-12.5f)*HS);x<(int)((xr+12.5f)*HS);x++) for(int y=(int)((yy+0.4f)*HS);y<(int)((yy+3.4f)*HS);y++){ int i=cv_i(v,x,y); float gr=0.5f+0.5f*sinf(((x+0.5f)/HS)*0.3f+gnoise((x+0.5f)/HS*0.5f,(y+0.5f)/HS*3,0,s+23)*3); cv_set(v,i,csc(cv_col(v,i),0.82f+0.3f*gr)); }
    acc_blob(v,xr-10,yy+1.9f,0.45f,0.45f,0,hx(0x9a9aa8),1.6f,0.8f,0,0); acc_blob(v,xr+10,yy+1.9f,0.45f,0.45f,0,hx(0x9a9aa8),1.6f,0.8f,0,0); }
  for(int sgn=-1;sgn<=1;sgn+=2){ acc_box(v,xr+sgn*7-0.9f,-2,xr+sgn*7+0.9f,FH+2,hx(0x8a8a9a),2.2f,0.6f,0.95f);
    acc_line(v,xr+sgn*7-0.3f,-2,xr+sgn*7-0.3f,FH+2,0.35f,hx(0xd8d8e8),2.3f,0.95f,HM_ON); }
  for(int i=0;i<8;i++){ bool cy=hg_chance(0.5f); acc_crack(v,rnd_x(),rnd_y(),hg_range(14,30),0.4f,0.2f,cy?hx(0x9af8ff):hx(0xff8ad8),0.9f,cy?128:1,5,s+300+i); }
  for(int i=0;i<10;i++){ float x=hg_range(10,FW-10), y=rnd_y(); bool cy=hg_chance(0.45f);
    acc_crystals(v,x,y,6,hg_irange(2,4),cy?hx(0xe0fcff):hx(0xffd8f4),cy?hx(0x2a6a9a):hx(0x8a2a7a),0.85f,cy?128+hg_irange(0,90):1+hg_irange(0,80),s+60+i); glow_add(g,x,y-3,cy?0x9af8ff:0xff8ad8,24); }
  for(int i=0;i<2;i++){ float x=hg_range(12,FW-12), y=rnd_y(); acc_box(v,x-2.2f,y-0.2f,x+2.2f,y+5.2f,hx(0x2a2a34),2.4f,0.4f,0.6f);
    acc_box(v,x-1.4f,y+0.8f,x+1.4f,y+4.2f,hx(0xffc860),2.6f,0.2f,0.2f);
    for(int yy=(int)((y+0.8f)*HS);yy<(int)((y+4.2f)*HS);yy++) for(int xx=(int)((x-1.4f)*HS);xx<(int)((x+1.4f)*HS);xx++){ int i2=cv_i(v,xx,yy); v->em[i2]=1.0f; v->cd[i2]=(uint8_t)(100+hg_irange(0,19)); }
    glow_add(g,x,y+2,0xffc060,30); }
  for(int i=0;i<120;i++){ float x=rnd_x(), y=rnd_y(); int r=hg_irange(0,9); C3 c= r<5?hx(0x6a6680):(r<8?hx(0x24222e):(r==8?hx(0xffb8e8):hx(0xb8f4ff))); acc_blob(v,x,y,0.35f,0.3f,0,c,0.3f,0.5f,r>=8?0.6f:0,r==8?1+hg_irange(0,80):128+hg_irange(0,90)); }
}
static void f_sanctuaire(Cv *v,uint32_t s,Glows *g){
  static const uint32_t P[5]={0x9a7a4a,0xa8885a,0xb69666,0xc4a472,0xd2b282};
  base(v,P,0.38f,48,0.1f,0.12f,s);
  dabs(v,0x8a6a3e,110,-0.62f,3,7,14,40,0.3f,0.52f,s+71);
  const int TS=32;
  for(int y=0;y<v->h;y++) for(int x=0;x<v->w;x++){
    float gx=(x+0.5f)/HS, gy=(y+0.5f)/HS; int i=y*v->w+x; int tx=(int)(gx/TS), ty=(int)(gy/TS); float lx=gx-tx*TS, ly=gy-ty*TS;
    float e=fminf(fminf(lx,TS-lx),fminf(ly,TS-ly))-0.6f+0.35f*fbm2(gx,gy,1.4f,2,s+3,FH);
    uint32_t id=hh3(tx,ty%12,s); float tn=(hfl(id)-0.5f)*0.12f;
    if(e<0){ cv_set(v,i,hx(0x5a4224)); v->ht[i]-=0.35f; v->sp[i]=0.05f; continue; }
    float t=sst(0,1.3f,e); cv_set(v,i,csc(cv_col(v,i),1+tn)); v->ht[i]+=1.2f*t+0.15f*fbm2(gx,gy,0.7f,2,s+5,FH);
    if((id>>10)%9<2){ float cx=lx-16, cy=ly-16; int gxi=(int)floorf((cx+12)/4), gyi=(int)floorf((cy+12)/6); if(gxi>=0&&gxi<6&&gyi>=0&&gyi<4){ uint32_t gh=hh3(gxi,gyi,id); float ux=fmodf(cx+12,4), uy=fmodf(cy+12,6);
      bool on= ((gh&1) && ux>0.8f&&ux<1.6f&&uy>0.8f&&uy<5) || ((gh&2) && uy>0.8f&&uy<1.6f&&ux>0.6f&&ux<3.2f) || ((gh&4) && fabsf(ux-2)+fabsf(uy-3)<1.4f && fabsf(ux-2)+fabsf(uy-3)>0.8f);
      if(on){ v->ht[i]-=0.45f; cv_set(v,i,cmix(cv_col(v,i),hx(0x6a4e2a),0.6f)); } } }
  }
  for(int py=TS*3;py<FH;py+=TS*4){ acc_box(v,-2,py-0.75f,FW+2,py+0.45f,hx(0xe8c050),1.4f,0.3f,0.9f);
    for(int x=0;x<v->w;x++) for(int y=(int)((py-0.75f)*HS);y<(int)((py+0.45f)*HS);y++){ int i=cv_i(v,x,y); v->em[i]=0.55f; v->cd[i]=(uint8_t)ci(1+((x/HS)*3)%126); } }
  for(int k=4;k<=5;k++){ float x0=k*TS-0.6f; acc_box(v,x0-0.6f,-2,x0+0.6f,FH+2,hx(0xe8c050),1.4f,0.3f,0.9f);
    for(int y=0;y<v->h;y++) for(int x=(int)((x0-0.6f)*HS);x<(int)((x0+0.6f)*HS);x++){ int i=cv_i(v,x,y); v->em[i]=0.55f; v->cd[i]=(uint8_t)ci(1+((y/HS)*3)%126); } }
  for(int py=0;py<FH;py+=TS) for(int px=0;px<FW-TS;px+=TS){ if(((px/TS)*5+(py/TS)*3)%11!=2) continue;
    float cx=px+TS*0.5f-0.5f, cy=py+TS*0.5f-0.5f;
    for(int y=(int)((cy-13)*HS);y<(int)((cy+13)*HS);y++) for(int x=(int)((cx-13)*HS);x<(int)((cx+13)*HS);x++){
      float gx=(x+0.5f)/HS-cx, gy=(y+0.5f)/HS-cy; float rr=sqrtf(gx*gx+gy*gy), a=atan2f(gy,gx); int i=cv_i(v,x,y);
      float ray=fabsf(fmodf(a*8/TAU+64.5f,1.0f)-0.5f);
      bool disc=rr<4.2f, ring=rr>5.6f&&rr<6.8f, rays=rr>8.0f&&rr<12.5f&&ray<0.12f*(12.5f-rr)/4.5f+0.03f;
      if(disc){ cv_set(v,i,gx<0&&gy<0?hx(0xfff0b0):hx(0xffd84a)); v->ht[i]+=0.8f*sqrtf(sat1(1-rr/4.2f)); v->sp[i]=0.9f; v->em[i]=0.95f; v->cd[i]=(uint8_t)ci(128+(int)(rr*9)%120); }
      else if(ring||rays){ cv_set(v,i,rays&&rr>10.5f?hx(0xb08a3a):hx(0xd8b050)); v->ht[i]-=0.25f; v->sp[i]=0.85f; }
      else if(rr<13 && (rr<4.9f||(rr>5.0f&&rr<7.4f))) cv_set(v,i,cmix(cv_col(v,i),hx(0x5a3a14),0.35f));
    }
    glow_add(g,cx,cy,0xffc040,30); }
  for(int y=0;y<v->h;y++) for(int x=0;x<v->w;x++){ float gx=(x+0.5f)/HS, gy=(y+0.5f)/HS; float m=0.5f+0.5f*fbm2(gx,gy,40,3,s+90,FH);
    if(m>0.66f){ int i=y*v->w+x; float k=sst(0.66f,0.76f,m); float rip=0.5f+0.5f*sinf(gx*0.9f-gy*0.4f+fbm2(gx,gy,6,2,s+91,FH)*3);
      cv_set(v,i,cmix(cv_col(v,i),cmix(hx(0xe0c890),hx(0xf0dcae),rip),k*0.9f)); v->ht[i]+=k*(0.6f+0.25f*rip); v->sp[i]=v->sp[i]*(1-k)+0.06f*k; v->em[i]*=1-k; } }
  for(int i=0;i<40;i++) acc_coin(v,rnd_x(),rnd_y(),hg_range(0.6f,0.9f),hx(0xffd860));
}

void hd_floor(Cv *v,int biome,uint32_t seed,Glows *g){
  switch(biome){
    case 0: f_catacombes(v,seed,g); break;
    case 1: f_foret(v,seed,g); break;
    case 2: f_forges(v,seed,g); break;
    case 4: f_givre(v,seed,g); break;
    case 5: f_cite(v,seed,g); break;
    case 6: f_ruche(v,seed,g); break;
    case 7: f_neant(v,seed,g); break;
    case 8: f_jardin(v,seed,g); break;
    case 9: f_marais(v,seed,g); break;
    case 10: f_mines(v,seed,g); break;
    case 11: f_sanctuaire(v,seed,g); break;
    default: f_coeur(v,seed,g); break;
  }
}

/* ======================================================================= */
/*          v9 : parties du niveau (accents, ruines) et arene du gardien    */
/* ======================================================================= */
static const uint32_t AGLOW[BIOME_COUNT]={0xffb060,0x45c2c8,0xff7a22,0xa060ff,0x9ae8ff,0x3ab8c0,0xffa030,0xc8d8ff,0xd8ff6a,0xa8e040,0x9af8ff,0xffc040};
static void accents(Cv *v,int b,uint32_t s,Glows *g){   /* partie 2 : la signature du niveau, plus dense */
  for(int i=0;i<18;i++){ float x=hg_range(14,FW-14), y=(i+hg_range(0.15f,0.85f))*FH/18.0f;
    switch(b){
      case 0: acc_skull(v,x,y,hg_range(1.9f,2.4f),hg_range(-0.5f,0.5f),hx(0xe0d4bc)); for(int k=0;k<3;k++) acc_candle(v,x+6+k*3.2f,y+hg_range(-1,1),0.9f,hg_range(3,6),hg_irange(1,200),g,x,y); glow_add(g,x+9,y-4,0xffb060,30); break;
      case 1: { bool t=hg_chance(0.4f); for(int k=0;k<4;k++){ float a=hg_range(0,TAU), d=hg_range(0,4); acc_mushroom(v,x+cosf(a)*d,y+sinf(a)*d*0.8f,hg_range(1.6f,3.2f),t,t?128+hg_irange(0,60):16+hg_irange(0,60)); } glow_add(g,x,y-3,t?0x45c2c8:0xf59ac0,24); } break;
      case 2: acc_crack(v,x,y,hg_range(14,26),0.6f,0.45f,hx(0xff8a2a),0.95f,40,3,s+400+(uint32_t)i); glow_add(g,x,y,0xff7a22,26); break;
      case 3: acc_crystals(v,x,y,5,hg_irange(2,4),hx(0xe2c8ff),hx(0x8a5ac8),0.8f,128+hg_irange(0,80),s+60+(uint32_t)i); glow_add(g,x,y-2,0xa060ff,22); break;
      case 4: acc_crystals(v,x,y,5,hg_irange(2,3),hx(0xeefaff),hx(0x6ab0e4),0.6f,128+hg_irange(0,80),s+70+(uint32_t)i); glow_add(g,x,y-2,0x9ae8ff,22); break;
      case 5: { bool pink=hg_chance(0.5f); acc_coral(v,x,y,4.5f,pink?hx(0xf59ac0):hx(0x5af0d0),0.8f,pink?30+hg_irange(0,60):140+hg_irange(0,60)); glow_add(g,x,y-3,pink?0xf59ac0:0x5af0d0,22); } break;
      case 6: acc_blob(v,x,y,hg_range(1.5f,3),hg_range(2.5f,5),hg_range(-0.3f,0.3f)+PI*0.5f,hx(0xf0a830),1.9f,0.95f,0.4f,60); break;
      case 7: { float a=hg_range(0,TAU); for(int k=0;k<30;k++){ a+=hg_range(-0.35f,0.35f); x+=cosf(a); y+=sinf(a)*0.9f; acc_blob(v,x,y,0.45f,0.45f,0,hx(0xe0f0ff),-0.3f,0.2f,1.0f,128+((k*3)&127)); } } break;
      case 8: { static const uint32_t FL[4]={0xf6f2ea,0xf59ac0,0xc8a8ff,0xf9dc5c}; C3 fc=hx(FL[i&3]); for(int k=0;k<7;k++) acc_flower(v,x+hg_range(-6,6),y+hg_range(-4,4),hg_range(0.8f,1.2f),fc,hx(0xf9c84a),5,hg_range(0,TAU)); } break;
      case 9: acc_moss(v,x,y,hg_range(2,4.5f),hx(0x4a5a26),hx(0x7a8a3a),s+900+(uint32_t)i); acc_bone(v,x+3,y,x+7,y+2,0.55f,hx(0xb8b090)); break;
      case 10: { bool cy=hg_chance(0.5f); acc_crystals(v,x,y,6,hg_irange(2,4),cy?hx(0xe0fcff):hx(0xffd8f4),cy?hx(0x2a6a9a):hx(0x8a2a7a),0.85f,cy?128+hg_irange(0,90):1+hg_irange(0,80),s+60+(uint32_t)i); glow_add(g,x,y-3,cy?0x9af8ff:0xff8ad8,24); } break;
      default: for(int k=0;k<6;k++) acc_coin(v,x+hg_range(-5,5),y+hg_range(-3,3),hg_range(0.6f,0.9f),hx(0xffd860)); break;
    } }
}
static void ruins(Cv *v,int b,uint32_t s,Glows *g){   /* partie 3 : le sol se fend, colonnes tombees, eboulis, suie */
  C3 gc=hx(AGLOW[b]);
  for(int y=0;y<v->h;y++) for(int x=0;x<v->w;x++){ float gx=(x+0.5f)/HS, gy=(y+0.5f)/HS; int i=y*v->w+x;   /* traces de suie et d'usure */
    float d=sst(0.52f,0.78f,0.5f+0.5f*fbm2(gx,gy,36,4,s+77,FH)); if(d>0){ cv_set(v,i,cmix(cv_col(v,i),hx(0x1e1a22),0.42f*d)); v->sp[i]*=1-0.5f*d; } }
  for(int i=0;i<10;i++){ float x=rnd_x(), y=(i+hg_range(0.2f,0.8f))*FH/10.0f; float len=hg_range(26,48); uint32_t cs=s+600+(uint32_t)i;
    acc_crack(v,x,y,len,2.1f,1.0f,hx(0x120f16),0,0,0,cs);                                    /* fissure sombre */
    acc_crack(v,x,y,len*0.75f,0.42f,0.3f,csc(gc,0.5f),0.3f,1+hg_irange(0,40),3,cs);         /* lueur au fond */
    if(i%2==0) glow_add(g,x,y,AGLOW[b],16); }
  for(int i=0;i<3;i++){ float x=hg_range(46,FW-46), y=(i+0.5f)*FH/3.0f+hg_range(-30,30), a=hg_range(-0.6f,0.6f)+(i&1?PI:0), L=hg_range(26,38);   /* colonne tombee */
    float x1=x+cosf(a)*L, y1=y+sinf(a)*L; C3 st=hx(0x8e8a98);
    acc_line(v,x+1.4f,y+1.8f,x1+1.4f,y1+1.8f,8.0f,hx(0x18141c),0,0,HM_DECAL);                 /* ombre portee */
    acc_line(v,x,y,x1,y1,6.5f,st,3.2f,0.14f,HM_ON);
    for(int k=1;k<5;k++){ float t=k/5.0f; float mx=x+(x1-x)*t, my=y+(y1-y)*t; acc_line(v,mx-sinf(a)*3.3f,my+cosf(a)*3.3f,mx+sinf(a)*3.3f,my-cosf(a)*3.3f,0.45f,csc(st,0.5f),-0.4f,0.0f,HM_ADD); }
    for(int k=0;k<7;k++) acc_rock(v,x1+hg_range(-7,7),y1+hg_range(-6,6),hg_range(0.9f,2.2f),hx(0x4a4650),hx(0x8a8494),1.4f,s+760+(uint32_t)(i*8+k)); }
  for(int i=0;i<16;i++){ float x=rnd_x(), y=rnd_y(); int n=hg_irange(3,7); for(int k=0;k<n;k++) acc_rock(v,x+hg_range(-5,5),y+hg_range(-4,4),hg_range(0.8f,2.2f),hx(0x3a3640),hx(0x7a7488),1.4f,s+700+(uint32_t)(i*8+k)); }
}
/* arene du gardien : chaussee rituelle (dalles polies), deux canaux de runes, sceaux et braseros.
   Le sol defile : tout se repete proprement sur la hauteur de la texture (384). */
static void arena(Cv *v,int b,uint32_t s,Glows *g){
  C3 gc=hx(AGLOW[b]); const float xl=FW*0.17f, xr=FW*0.83f, cx=FW*0.5f;
  for(int y=0;y<v->h;y++) for(int x=0;x<v->w;x++){
    float gx=(x+0.5f)/HS, gy=(y+0.5f)/HS; int i=y*v->w+x; C3 c=cv_col(v,i);
    if(gx>xl+3 && gx<xr-3){   /* dalles polies, joints decales */
      c=cmix(c,csc(c,0.8f),0.45f); v->ht[i]*=0.5f; v->sp[i]=fmaxf(v->sp[i],0.32f);
      float lx=gx-xl-3; int col=(int)floorf(lx/32.0f); float sx=fmodf(lx,32.0f), sy=fmodf(gy+(col&1)*16.0f,32.0f);
      float e=fminf(fminf(sx,32-sx),fminf(sy,32-sy)); if(e<0.5f){ c=csc(c,0.6f); v->ht[i]-=0.35f; v->sp[i]=0.05f; }
      float yy=fmodf(gy,96.0f)-48.0f, dd=fabsf(gx-cx)+fabsf(yy);   /* sceau : losange au milieu de la chaussee */
      if(fabsf(yy)<0.3f && fabsf(gx-cx)>8.5f){ c=cmix(c,cmix(gc,c3(1,1,1),0.2f),0.3f); v->em[i]=fmaxf(v->em[i],0.14f); v->cd[i]=(uint8_t)ci(1+(int)(gx*2)%120); v->ht[i]-=0.2f; }
      if(fabsf(dd-7.5f)<0.45f || dd<1.8f || (fabsf(dd-4.5f)<0.3f && fabsf(fabsf(gx-cx)-fabsf(yy))<0.6f)){ c=cmix(c,cmix(gc,c3(1,1,1),0.3f),0.8f); v->em[i]=fmaxf(v->em[i],0.6f); v->cd[i]=(uint8_t)ci(128+(int)(dd*8)%100); v->ht[i]-=0.25f; }
    }
    for(int side=0;side<2;side++){ float ch= side ? xr : xl; float d=fabsf(gx-ch);   /* canaux de runes */
      if(d<3.0f){ float t=sst(3.0f,2.2f,d); float n=0.5f+0.5f*fbm2(gx,gy,3,3,s+5,FH);
        c=cmix(c,cmix(hx(0x5a5664),hx(0x8e8a9c),n),0.85f*t); v->ht[i]+=1.1f*t; v->sp[i]=0.14f;
        if(d<1.7f){ c=csc(c,0.5f); v->ht[i]-=0.6f;
          float u=fmodf(gy,8.0f)/8.0f, w=(gx-(ch-1.7f))/3.4f; int cell=(int)floorf(gy/8.0f)+side*97; uint32_t h=hh3(cell,b,s);
          bool on= ((h&1)&&fabsf(w-0.5f)<0.11f&&u>0.15f&&u<0.85f) || ((h&2)&&fabsf(u-0.5f)<0.09f&&w>0.2f&&w<0.8f) || ((h&4)&&fabsf(fabsf(w-0.5f)-(u-0.5f))<0.08f) || ((h&8)&&fabsf(fabsf(w-0.5f)+fabsf(u-0.5f)-0.3f)<0.07f);
          if(on){ c=cmix(c,cmix(gc,c3(1,1,1),0.25f),0.85f); v->em[i]=0.7f; v->cd[i]=(uint8_t)ci(1+(cell*5)%120); } } } }
    cv_set(v,i,c);
  }
  for(int k=0;k<6;k++) for(int side=0;side<2;side++){ float x= side ? FW-20.0f : 20.0f, y=32+64.0f*k;   /* braseros */
    acc_rock(v,x,y,3.4f,hx(0x4a4652),hx(0x8a8494),2.6f,s+90+(uint32_t)(k*2+side));
    acc_blob(v,x,y-0.5f,1.8f,1.8f,0,cmix(gc,c3(1,1,1),0.35f),2.9f,0.2f,1.0f,1+(k*2+side)*13); glow_add(g,x,y,AGLOW[b],30); }
  for(int k=0;k<4;k++) glow_add(g,cx,48+96.0f*k,AGLOW[b],22);
}
void hd_floor_variant(Cv *v,int biome,int var,uint32_t seed,Glows *g){
  hd_floor(v,biome,seed,g);
  if(var==1) accents(v,biome,seed+31,g);
  if(var==2){ accents(v,biome,seed+37,g); ruins(v,biome,seed+41,g); }
  if(var==3) arena(v,biome,seed+43,g);
}
