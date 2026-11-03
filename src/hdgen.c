/* GOUFFRE - atelier des textures HD (outil hors jeu) : toile, bruits, accessoires generiques,
   finitions (occlusion ambiante, normales), export des fichiers du dossier « donnees ».
   Le jeu n'execute jamais ce code : il lit seulement les fichiers produits (hdpack.c). */
#include "hdgen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------- hasard ---------------- */
static uint32_t hgs=0x9e3779b9u;
void hg_seed(uint32_t s){ hgs=s*2654435761u+0x6d2b79f5u; if(!hgs) hgs=1; for(int i=0;i<4;i++) hg_rand(); }
float hg_rand(void){ hgs^=hgs<<13; hgs^=hgs>>17; hgs^=hgs<<5; return (hgs>>8)*(1.0f/16777216.0f); }

/* ---------------- toile ---------------- */
Cv cv_new(int gw,int gh,bool wrap){
  Cv v; v.gw=gw; v.gh=gh; v.w=gw*HS; v.h=gh*HS; v.wrap=wrap; size_t n=(size_t)v.w*v.h;
  v.c=(float*)calloc(n*3,sizeof(float)); v.ht=(float*)calloc(n,sizeof(float)); v.sp=(float*)calloc(n,sizeof(float));
  v.em=(float*)calloc(n,sizeof(float)); v.cd=(uint8_t*)calloc(n,1);
  for(size_t i=0;i<n;i++) v.sp[i]=0.06f;
  return v;
}
void cv_free(Cv *v){ free(v->c); free(v->ht); free(v->sp); free(v->em); free(v->cd); memset(v,0,sizeof *v); }
float cv_ht_at(const Cv *v,float gx,float gy){ return v->ht[cv_i(v,(int)floorf(gx*HS),(int)floorf(gy*HS))]; }

/* ---------------- bruits ---------------- */
static float GX_[256], GY_[256]; static bool g_ok=false;
static void gtab(void){ if(g_ok) return; g_ok=true; for(int i=0;i<256;i++){ float a=(i+0.5f)*TAU/256.0f; GX_[i]=cosf(a); GY_[i]=sinf(a); } }
float gnoise(float x,float y,int py,uint32_t s){
  gtab();
  int X=(int)floorf(x), Y=(int)floorf(y); float fx=x-X, fy=y-Y;
  float u=fx*fx*fx*(fx*(fx*6-15)+10), v=fy*fy*fy*(fy*(fy*6-15)+10);
  float d[4];
  for(int k=0;k<4;k++){ int cx=X+(k&1), cy=Y+(k>>1); int wy= py>0 ? ((cy%py)+py)%py : cy;
    uint32_t h=hh3(cx,wy,s)&255; d[k]=GX_[h]*(fx-(k&1))+GY_[h]*(fy-(k>>1)); }
  float a=d[0]+(d[1]-d[0])*u, b=d[2]+(d[3]-d[2])*u;
  return (a+(b-a)*v)*1.414f;
}
/* raccord exact : la maille verticale est ajustee pour qu'un nombre entier de mailles couvre la periode */
static inline float cellv(float cell,int P,int *py){ if(P<=0){ *py=0; return cell; } int n=(int)lroundf(P/cell); if(n<1) n=1; *py=n; return (float)P/n; }
float fbm2(float x,float y,float cell,int oct,uint32_t s,int P){
  float sum=0,amp=1,nrm=0;
  for(int o=0;o<oct;o++){ int py; float cy=cellv(cell,P,&py); sum+=amp*gnoise(x/cell,y/cy,py,s+(uint32_t)o*1013u); nrm+=amp; amp*=0.5f; cell*=0.5f; }
  return sum/nrm;
}
float ridge2(float x,float y,float cell,int oct,uint32_t s,int P){
  float sum=0,amp=1,nrm=0;
  for(int o=0;o<oct;o++){ int py; float cy=cellv(cell,P,&py); float n=1.0f-fabsf(gnoise(x/cell,y/cy,py,s+(uint32_t)o*733u)); sum+=amp*n*n; nrm+=amp; amp*=0.5f; cell*=0.5f; }
  return sum/nrm;
}
Vo voro(float x,float y,float cell,int P,uint32_t s,float jit){
  int py; float cyl=cellv(cell,P,&py); y*=cell/cyl;      /* espace a mailles carrees, periode entiere */
  int cx=(int)floorf(x/cell), cy=(int)floorf(y/cell);
  Vo r; r.f1=r.f2=1e9f; r.id=0; r.px=r.py=0;
  for(int dy=-2;dy<=2;dy++) for(int dx=-2;dx<=2;dx++){
    int gx=cx+dx, gy=cy+dy; int wy= py>0 ? ((gy%py)+py)%py : gy;
    uint32_t h=hh3(gx,wy,s);
    float px=(gx+0.5f+jit*(hfl(h)-0.5f))*cell, pyy=(gy+0.5f+jit*(hfl(h*0x9e3779b1u+7)-0.5f))*cell;
    float ddx=x-px, ddy=y-pyy, d=sqrtf(ddx*ddx+ddy*ddy);
    if(d<r.f1){ r.f2=r.f1; r.f1=d; r.id=hh3(gx,wy,s^0x51ed27u); r.px=px; r.py=pyy*cyl/cell; } else if(d<r.f2) r.f2=d;
  }
  return r;
}

/* ---------------- fragments ---------------- */
static void put(Cv *v,int i,const Fr *f,float base){
  float a=f->a>1?1:f->a; if(a<=0) return;
  C3 o=cv_col(v,i); cv_set(v,i,cmix(o,f->c,a));
  if(f->hm==HM_ON){ float nh=base+f->h; if(nh>v->ht[i]) v->ht[i]+= (nh-v->ht[i])*a; else v->ht[i]+=(nh-v->ht[i])*a*0.25f; }
  else if(f->hm==HM_ADD) v->ht[i]+=f->h*a;
  if(f->hm!=HM_DECAL || f->sp>0) v->sp[i]+= (f->sp-v->sp[i])*a;
  float e=f->em*a; if(e>v->em[i]){ v->em[i]=e; v->cd[i]=(uint8_t)(f->code<1?1:(f->code>255?255:f->code)); }
  else if(f->hm==HM_ON) v->em[i]+=(f->em-v->em[i])*a;      /* un objet opaque cache la lumiere de dessous */
}
void stamp(Cv *v,float cx,float cy,float rx,float ry,Shf fn,void *u){
  float base=cv_ht_at(v,cx,cy);
  int x0=(int)floorf((cx-rx)*HS), x1=(int)ceilf((cx+rx)*HS), y0=(int)floorf((cy-ry)*HS), y1=(int)ceilf((cy+ry)*HS);
  if(x0<0) x0=0; if(x1>v->w-1) x1=v->w-1;
  if(!v->wrap){ if(y0<0) y0=0; if(y1>v->h-1) y1=v->h-1; }
  for(int y=y0;y<=y1;y++) for(int x=x0;x<=x1;x++){
    float gx=(x+0.5f)/HS, gy=(y+0.5f)/HS;
    Fr f; memset(&f,0,sizeof f); f.hm=HM_ON;
    fn(gx-cx,gy-cy,u,&f);
    if(f.a>0) put(v,cv_i(v,x,y),&f,base);
  }
}
void fill(Cv *v,Shf fn,void *u){
  for(int y=0;y<v->h;y++) for(int x=0;x<v->w;x++){
    Fr f; memset(&f,0,sizeof f); f.hm=HM_ON; f.a=0;
    fn((x+0.5f)/HS,(y+0.5f)/HS,u,&f);
    if(f.a>0){ int i=y*v->w+x; put(v,i,&f,0); }
  }
}

/* ---------------- finitions ---------------- */
void cv_ao(Cv *v,float strength,float radius){
  size_t n=(size_t)v->w*v->h; float *ao=(float*)malloc(n*sizeof(float));
  static const float DX[8]={1,0.7071f,0,-0.7071f,-1,-0.7071f,0,0.7071f}, DY[8]={0,0.7071f,1,0.7071f,0,-0.7071f,-1,-0.7071f};
  float R[4]={0.6f,1.4f,radius*0.5f,radius};
  for(int y=0;y<v->h;y++) for(int x=0;x<v->w;x++){
    int i=y*v->w+x; float h0=v->ht[i], occ=0;
    for(int d=0;d<8;d++) for(int k=0;k<4;k++){
      float r=R[k]; int sx=x+(int)lroundf(DX[d]*r*HS), sy=y+(int)lroundf(DY[d]*r*HS);
      float e=(v->ht[cv_i(v,sx,sy)]-h0)/r; occ+= e>0 ? (e>1.5f?1.5f:e) : 0;
    }
    ao[i]=1.0f-strength*occ/32.0f;
  }
  for(size_t i=0;i<n;i++){ if(v->cd[i]==7 && v->em[i]>=0.99f) continue; float a=ao[i]<0.25f?0.25f:ao[i]; v->c[i*3]*=a; v->c[i*3+1]*=a; v->c[i*3+2]*=a; }
  free(ao);
}
void cv_grain(Cv *v,float amt,uint32_t seed){
  for(int y=0;y<v->h;y++) for(int x=0;x<v->w;x++){ int i=y*v->w+x; float g=(hfl(hh3(x,y,seed))-0.5f)*amt*(v->cd[i]==7?0.3f:1.0f); v->c[i*3]+=g; v->c[i*3+1]+=g; v->c[i*3+2]+=g; }
}

/* ======================================================================= */
/*                     accessoires generiques (formes douces)              */
/* ======================================================================= */
static inline float aa(float d){ return sat1(0.5f-d*HS); }   /* couverture d'un bord a distance signee d (pixels du jeu) */

typedef struct { float rx,ry,ca,sa; C3 c; float h,sp,em; int code; } BlobU;
static void sh_blob(float lx,float ly,void *u,Fr *f){
  BlobU *b=(BlobU*)u; float x=lx*b->ca+ly*b->sa, y=-lx*b->sa+ly*b->ca;
  float q=sqrtf((x*x)/(b->rx*b->rx)+(y*y)/(b->ry*b->ry)); float d=(q-1)*fminf(b->rx,b->ry);
  f->a=aa(d); if(f->a<=0) return;
  float t=sat1(1-q*q); f->c=csc(b->c,0.86f+0.14f*t); f->h=b->h*sqrtf(t); f->sp=b->sp; f->em=b->em; f->code=b->code;
}
void acc_blob(Cv *v,float x,float y,float rx,float ry,float ang,C3 c,float h,float sp,float em,int code){
  BlobU b={rx,ry,cosf(ang),sinf(ang),c,h,sp,em,code}; float r=fmaxf(rx,ry)+1;
  stamp(v,x,y,r,r,sh_blob,&b);
}
typedef struct { float ax,ay,bx,by,w; C3 c; float h,sp; int hm; } LineU;
static void sh_line(float lx,float ly,void *u,Fr *f){
  LineU *l=(LineU*)u; float px=lx-l->ax, py=ly-l->ay, dx=l->bx-l->ax, dy=l->by-l->ay; float dd=dx*dx+dy*dy;
  float t= dd>1e-6f ? sat1((px*dx+py*dy)/dd) : 0; float ex=px-dx*t, ey=py-dy*t; float d=sqrtf(ex*ex+ey*ey)-l->w*0.5f;
  f->a=aa(d); if(f->a<=0) return;
  float k=sat1(1-(d+l->w*0.5f)/(l->w*0.5f+0.001f)); f->c=csc(l->c,0.8f+0.2f*k); f->h=l->h*sqrtf(k); f->sp=l->sp; f->hm=l->hm;
}
void acc_line(Cv *v,float x0,float y0,float x1,float y1,float w,C3 c,float h,float sp,int hm){
  float cx=(x0+x1)*0.5f, cy=(y0+y1)*0.5f; LineU l={x0-cx,y0-cy,x1-cx,y1-cy,w,c,h,sp,hm};
  stamp(v,cx,cy,fabsf(x1-x0)*0.5f+w+1,fabsf(y1-y0)*0.5f+w+1,sh_line,&l);
}
typedef struct { float hw,hh,bev; C3 c; float h,sp; } BoxU;
static void sh_box(float lx,float ly,void *u,Fr *f){
  BoxU *b=(BoxU*)u; float qx=fabsf(lx)-b->hw, qy=fabsf(ly)-b->hh; float d=fmaxf(qx,qy);
  f->a=aa(d); if(f->a<=0) return;
  float e=sat1(-d/fmaxf(b->bev,0.01f)); f->c=csc(b->c,0.82f+0.18f*e); f->h=b->h*(0.55f+0.45f*e); f->sp=b->sp;
}
void acc_box(Cv *v,float x0,float y0,float x1,float y1,C3 c,float h,float bevel,float sp){
  BoxU b={(x1-x0)*0.5f,(y1-y0)*0.5f,bevel,c,h,sp}; stamp(v,(x0+x1)*0.5f,(y0+y1)*0.5f,b.hw+1,b.hh+1,sh_box,&b);
}

/* rocher : galet bossele, facettes douces, mousse ou poussiere sur le dessus */
typedef struct { float r; C3 c0,c1; float h; uint32_t s; } RockU;
static void sh_rock(float lx,float ly,void *u,Fr *f){
  RockU *k=(RockU*)u; float a=atan2f(ly,lx); float rr=k->r*(0.82f+0.18f*gnoise(cosf(a)*1.7f+3,sinf(a)*1.7f+3,0,k->s));
  float d=sqrtf(lx*lx+ly*ly)-rr; f->a=aa(d); if(f->a<=0) return;
  float t=sat1(-d/rr); float n=fbm2(lx+k->s%97,ly,1.2f,3,k->s,0);
  float fac=floorf((lx*0.8f-ly*0.6f)/(rr*0.45f)+n*1.2f);
  f->h=k->h*sqrtf(t)*(0.9f+0.1f*n)+fac*0.08f*t;
  float lit=sat1(0.5f-0.35f*(lx+ly)/rr+0.2f*n);
  f->c=cmix(k->c0,k->c1,lit); f->sp=0.08f;
}
void acc_rock(Cv *v,float x,float y,float r,C3 c0,C3 c1,float h,uint32_t s){ RockU k={r,c0,c1,h,s}; stamp(v,x,y,r*1.3f+1,r*1.3f+1,sh_rock,&k); }
void acc_pebbles(Cv *v,float x,float y,float r,int n,C3 c,uint32_t s){
  for(int i=0;i<n;i++){ uint32_t h=hh3(i,(int)s,77); float a=hfl(h)*TAU, d=sqrtf(hfl(h>>3))*r; float pr=0.35f+hfl(h>>9)*0.7f;
    C3 cc=csc(c,0.75f+0.45f*hfl(h>>15)); acc_rock(v,x+cosf(a)*d,y+sinf(a)*d,pr,csc(cc,0.7f),cc,pr*0.8f,h); }
}

/* crane vu de dessus : dome, orbites, machoire */
typedef struct { float sc,ca,sa; C3 c; } SkU;
static void sh_skull(float lx,float ly,void *u,Fr *f){
  SkU *k=(SkU*)u; float x=(lx*k->ca+ly*k->sa)/k->sc, y=(-lx*k->sa+ly*k->ca)/k->sc;
  float dc=sqrtf(x*x/1.0f+(y+0.3f)*(y+0.3f)/1.1f)-1.0f;                   /* crane */
  float dj=fmaxf(fabsf(x)-0.55f,fabsf(y-0.85f)-0.35f);                    /* machoire */
  float d=fminf(dc,dj)*k->sc; f->a=aa(d); if(f->a<=0) return;
  float t=sat1(1-(x*x+(y+0.3f)*(y+0.3f))); float h=1.1f*sqrtf(t)+ (dj<0?0.45f:0);
  C3 c=k->c; float sh=0.9f+0.1f*t;
  for(int s=-1;s<=1;s+=2){ float ex=x-s*0.42f, ey=y-0.05f; float de=sqrtf(ex*ex+ey*ey*1.3f)-0.27f; if(de<0){ h-=0.8f*sat1(-de*6); sh*=0.25f+0.75f*sat1(1+de*9); } }
  { float nx=x, ny=y-0.5f; if(fabsf(nx)<0.12f && ny>-0.12f && ny<0.12f){ h-=0.35f; sh*=0.4f; } }
  if(dj<0 && dc>0){ float tx=fmodf(fabsf(x)+10,0.22f); if(tx<0.04f) sh*=0.55f; }
  f->c=csc(c,sh); f->h=h*k->sc*0.9f; f->sp=0.12f;
}
void acc_skull(Cv *v,float x,float y,float sc,float ang,C3 c){ SkU k={sc,cosf(ang),sinf(ang),c}; stamp(v,x,y,sc*1.7f+1,sc*1.7f+1,sh_skull,&k); }
void acc_bone(Cv *v,float x0,float y0,float x1,float y1,float r,C3 c){
  acc_line(v,x0,y0,x1,y1,r*1.5f,c,r*0.9f,0.1f,HM_ON);
  float dx=x1-x0, dy=y1-y0, l=sqrtf(dx*dx+dy*dy)+1e-4f; float nx=-dy/l*r*0.6f, ny=dx/l*r*0.6f;
  acc_blob(v,x0+nx,y0+ny,r*0.75f,r*0.75f,0,c,r,0.1f,0,0); acc_blob(v,x0-nx,y0-ny,r*0.75f,r*0.75f,0,c,r,0.1f,0,0);
  acc_blob(v,x1+nx,y1+ny,r*0.75f,r*0.75f,0,c,r,0.1f,0,0); acc_blob(v,x1-nx,y1-ny,r*0.75f,r*0.75f,0,c,r,0.1f,0,0);
}
/* bougie : cire, coulures, flamme (lumiere propre) */
typedef struct { float r,h; int code; } CandU;
static void sh_candle(float lx,float ly,void *u,Fr *f){
  CandU *k=(CandU*)u; float d=sqrtf(lx*lx+ly*ly);
  float pool=k->r*2.1f*(0.85f+0.15f*sinf(atan2f(ly,lx)*5));
  if(d<k->r){ f->a=aa(d-k->r); f->c=cmix(hx(0xf4ead2),hx(0xd8ccb0),sat1(d/k->r)); f->h=k->h; f->sp=0.2f;
    float fd=sqrtf(lx*lx+(ly+0.15f)*(ly+0.15f)); if(fd<k->r*0.62f){ float t=1-fd/(k->r*0.62f); f->c=cmix(hx(0xff9a3a),hx(0xfff4c0),t); f->em=sat1(t*1.6f); f->code=k->code; }
    if(fd<0.18f){ f->c=hx(0x2a1c14); f->em=0.3f; } return; }
  if(d<pool){ f->a=aa(d-pool)*0.95f; f->c=hx(0xe8dcc0); f->h=0.25f*(1-d/pool); f->sp=0.25f; }
}
void acc_candle(Cv *v,float x,float y,float r,float h,int code,Glows *g,float gx,float gy){
  CandU k={r,h,code}; stamp(v,x,y,r*2.4f+1,r*2.4f+1,sh_candle,&k); (void)g; (void)gx; (void)gy;
}
/* champignon : chapeau bombe tachete, lamelles a l'ombre, lumineux */
typedef struct { float r; bool teal; int code; } MushU;
static void sh_mush(float lx,float ly,void *u,Fr *f){
  MushU *k=(MushU*)u; float d=sqrtf(lx*lx+ly*ly*1.08f)-k->r; f->a=aa(d); if(f->a<=0) return;
  float t=sat1(1-(lx*lx+ly*ly)/(k->r*k->r));
  C3 cap= k->teal ? hx(0x3ab8c0) : hx(0xe88ab4), capd= k->teal ? hx(0x1a6a78) : hx(0x9a4a72), caph= k->teal ? hx(0xb8fff4) : hx(0xffd8ec);
  C3 c=cmix(capd,cap,sqrtf(t)); float lit=sat1(0.6f-0.5f*(lx+ly)/k->r); c=cmix(c,caph,lit*lit*0.5f);
  float sp=0, sr=fmaxf(k->r*0.16f,0.4f); for(int s=0;s<5;s++){ float a=s*1.9f+k->r, rr=k->r*0.55f*(s?1:0); float sx=cosf(a)*rr, sy=sin(a)*rr*0.9f; float ds=sqrtf((lx-sx)*(lx-sx)+(ly-sy)*(ly-sy))-sr; if(ds<0) sp=fmaxf(sp,sat1(-ds*HS)); }
  c=cmix(c,hx(0xfff8f0),sp*0.85f);
  float rim=sat1((sqrtf(lx*lx+ly*ly)/k->r-0.82f)*6); c=csc(c,1-0.35f*rim);
  f->c=c; f->h=k->r*0.9f*sqrtf(t)+k->r*0.6f; f->sp=0.35f; f->em=0.7f*sp; f->code=k->code;
}
void acc_mushroom(Cv *v,float x,float y,float r,bool teal,int code){ MushU k={r,teal,code}; stamp(v,x,y,r+1,r+1,sh_mush,&k); }
/* cristal : pyramide a facettes vue de dessus */
typedef struct { float r,h,ca,sa; C3 lit,dark; float em; int code; } CrysU;
static void sh_crys(float lx,float ly,void *u,Fr *f){
  CrysU *k=(CrysU*)u; float x=lx*k->ca+ly*k->sa, y=-lx*k->sa+ly*k->ca;
  float ax=fabsf(x)/(k->r*0.55f), ay=fabsf(y)/k->r; float d=(ax+ay-1)*k->r*0.4f;   /* losange allonge */
  f->a=aa(d); if(f->a<=0) return;
  float t=sat1(1-(ax+ay)); int face=(x>0)+2*(y>0);
  static const float FL[4]={1.0f,0.55f,0.8f,0.35f};
  C3 c=cmix(k->dark,k->lit,FL[face]*0.75f+0.25f*t);
  if(fabsf(x)<0.18f || fabsf(y)<0.18f) c=cmix(c,k->lit,0.5f);
  f->c=c; f->h=k->h*t; f->sp=0.9f; f->em=k->em*(0.45f+0.55f*t); f->code=k->code;
}
void acc_crystal(Cv *v,float x,float y,float r,float h,float ang,C3 lit,C3 dark,float em,int code){
  CrysU k={r,h,cosf(ang),sinf(ang),lit,dark,em,code}; stamp(v,x,y,r+1,r+1,sh_crys,&k);
}
void acc_crystals(Cv *v,float x,float y,float r,int n,C3 lit,C3 dark,float em,int code,uint32_t s){
  for(int i=0;i<n;i++){ uint32_t h=hh3(i,(int)s,31); float a=hfl(h)*TAU, d=hfl(h>>4)*r*0.6f; float rr=r*(0.45f+0.55f*hfl(h>>8));
    acc_crystal(v,x+cosf(a)*d,y+sinf(a)*d,rr,rr*1.6f,hfl(h>>12)*PI,lit,dark,em,code+(int)(hfl(h>>16)*40)); }
}
/* herbes : brins effiles */
void acc_grass(Cv *v,float x,float y,float r,int n,C3 c0,C3 c1,float len,uint32_t s){
  for(int i=0;i<n;i++){ uint32_t h=hh3(i,(int)s,53); float a=hfl(h)*TAU, d=sqrtf(hfl(h>>4))*r; float bx=x+cosf(a)*d, by=y+sinf(a)*d;
    float ang=-PI*0.5f+(hfl(h>>8)-0.5f)*1.6f, l=len*(0.5f+0.6f*hfl(h>>12));
    C3 c=cmix(c0,c1,hfl(h>>16));
    acc_line(v,bx,by,bx+cosf(ang)*l,by+sinf(ang)*l*0.7f,0.28f+0.12f*hfl(h>>20),c,0.5f+0.4f*hfl(h>>18),0.06f,HM_ON); }
}
/* fleur : petales elliptiques autour d'un coeur */
typedef struct { float r; C3 p,hc; int n; float a0; } FlU;
static void sh_flower(float lx,float ly,void *u,Fr *f){
  FlU *k=(FlU*)u; float d=sqrtf(lx*lx+ly*ly); float a=atan2f(ly,lx)-k->a0; float seg=TAU/k->n;
  float am=fmodf(a+TAU*8+seg*0.5f,seg)-seg*0.5f; float pr=k->r*(0.55f+0.45f*cosf(am*k->n*0.5f));
  float dd=d-pr; f->a=aa(dd); if(f->a<=0) return;
  float t=sat1(d/k->r); f->c= d<k->r*0.28f ? k->hc : cmix(csc(k->p,0.8f),k->p,1-t*0.4f);
  f->h=0.6f+0.4f*(1-t); f->sp=0.15f;
}
void acc_flower(Cv *v,float x,float y,float r,C3 petal,C3 heart,int petals,float ang){ FlU k={r,petal,heart,petals,ang}; stamp(v,x,y,r+1,r+1,sh_flower,&k); }
void acc_leaf(Cv *v,float x,float y,float len,float ang,C3 c){
  acc_blob(v,x,y,len*0.5f,len*0.22f,ang,c,0.3f,0.1f,0,0);
  acc_line(v,x-cosf(ang)*len*0.45f,y-sinf(ang)*len*0.45f,x+cosf(ang)*len*0.45f,y+sinf(ang)*len*0.45f,0.12f,csc(c,0.7f),0.35f,0.1f,HM_ON);
}
/* fougere : tige courbe et folioles alternees qui raccourcissent vers la pointe */
void acc_fern(Cv *v,float x,float y,float len,float ang,C3 c0,C3 c1,uint32_t s){
  float px=x, py=y, a=ang; int n=(int)(len*1.6f);
  for(int i=0;i<n;i++){ float t=(float)i/n; a+=0.035f*(hfl(s)-0.5f>0?1:-1); float st=len/n; float nx=px+cosf(a)*st, ny=py+sinf(a)*st;
    acc_line(v,px,py,nx,ny,0.32f*(1-t)+0.12f,cmix(c0,c1,0.3f),0.9f*(1-t)+0.2f,0.06f,HM_ON);
    if(i%2==0 && t>0.08f){ float fl=(1-t)*len*0.32f+0.5f; for(int sd=-1;sd<=1;sd+=2){ float fa=a+sd*(1.05f-0.25f*t);
      float cx=nx+cosf(fa)*fl*0.5f, cy=ny+sinf(fa)*fl*0.5f; acc_blob(v,cx,cy,fl*0.5f,fl*0.17f+0.12f,fa,cmix(c0,c1,0.45f+0.55f*hfl(hh3(i,sd,s))),0.5f*(1-t)+0.25f,0.08f,0,0); } }
    px=nx; py=ny; }
}
/* brindille : petite branche seche */
void acc_twig(Cv *v,float x,float y,float len,float ang,C3 c,uint32_t s){
  float ex=x+cosf(ang)*len, ey=y+sinf(ang)*len; acc_line(v,x,y,ex,ey,0.42f,c,0.45f,0.05f,HM_ON);
  float t=0.35f+0.3f*hfl(s), ba=ang+(hfl(s>>5)>0.5f?0.7f:-0.7f), bl=len*(0.25f+0.2f*hfl(s>>9));
  float bx=x+(ex-x)*t, by=y+(ey-y)*t; acc_line(v,bx,by,bx+cosf(ba)*bl,by+sinf(ba)*bl,0.3f,csc(c,1.1f),0.35f,0.05f,HM_ON);
}
void acc_root(Cv *v,float x,float y,float len,float w,float ang,C3 c,uint32_t s){
  float px=x, py=y;
  for(float t=0;t<len;t+=0.5f){ ang+=gnoise(t*0.15f,(float)(s%100),0,s)*0.18f; float nx=px+cosf(ang)*0.5f, ny=py+sinf(ang)*0.5f; float ww=w*(1-t/len)+0.35f;
    acc_line(v,px,py,nx,ny,ww,cmix(c,csc(c,1.35f),0.5f+0.5f*gnoise(t*0.6f,1,0,s+3)),ww*0.55f,0.08f,HM_ON); px=nx; py=ny; }
}
void acc_crack(Cv *v,float x,float y,float len,float w,float depth,C3 c,float em,int code0,int codestep,uint32_t s){
  float a=hfl(s)*TAU, px=x, py=y;
  for(int i=0;i<(int)(len*2);i++){
    a+=gnoise(i*0.12f,(float)(s&255),0,s)*0.5f; float nx=px+cosf(a)*0.5f, ny=py+sinf(a)*0.5f; float ww=w*(0.6f+0.4f*sinf(i*0.37f));
    LineU l={px-nx,py-ny,0,0,ww,c,-depth,0.05f,HM_ADD};
    float cx=nx, cy=ny; (void)cx; (void)cy;
    if(em>0){ /* fissure lumineuse */
      BlobU b={ww*0.6f+0.2f,ww*0.6f+0.2f,1,0,c,-depth*0.5f,0.0f,em,code0+(i*codestep)%127}; stamp(v,nx,ny,ww+1,ww+1,sh_blob,&b);
    } else stamp(v,nx,ny,fabsf(l.ax)+ww+1,fabsf(l.ay)+ww+1,sh_line,&l);
    if(hg_chance(0.03f)) acc_crack(v,nx,ny,len*0.3f,w*0.6f,depth*0.7f,c,em,code0,codestep,s*7919u+i);
    px=nx; py=ny;
  }
}
/* corail : branches rayonnantes */
void acc_coral(Cv *v,float x,float y,float r,C3 c,float em,int code){
  for(int k=0;k<6;k++){ float a=-PI*0.5f+(k-2.5f)*0.42f; float bx=x+cosf(a)*r, by=y+sinf(a)*r;
    acc_line(v,x,y,bx,by,0.55f,c,1.1f,0.2f,HM_ON);
    BlobU b={0.55f,0.55f,1,0,cmix(c,c3(1,1,1),0.35f),1.3f,0.3f,em,code}; stamp(v,bx,by,1.5f,1.5f,sh_blob,&b); }
}
/* nenuphar : disque a encoche, nervures */
typedef struct { float r,a0; C3 c; bool fl; } LilyU;
static void sh_lily(float lx,float ly,void *u,Fr *f){
  LilyU *k=(LilyU*)u; float d=sqrtf(lx*lx+ly*ly); float a=atan2f(ly,lx)-k->a0; a=fmodf(a+TAU*4,TAU);
  if(a<0.32f && d>0.4f){ f->a=0; return; }
  f->a=aa(d-k->r); if(f->a<=0) return;
  float vein=fabsf(sinf(a*7))<0.12f ? 0.82f : 1.0f; f->c=csc(cmix(csc(k->c,0.75f),k->c,sat1(1-d/k->r+0.3f)),vein);
  f->h=0.35f; f->sp=0.5f;
  if(k->fl && d<k->r*0.35f){ float t=1-d/(k->r*0.35f); f->c=cmix(hx(0xf0d0e0),hx(0xfff4c0),t); f->h=0.8f+0.6f*t; }
}
void acc_lily(Cv *v,float x,float y,float r,float ang,C3 c,bool flower){ LilyU k={r,ang,c,flower}; stamp(v,x,y,r+1,r+1,sh_lily,&k); }
void acc_reed(Cv *v,float x,float y,float len,float ang,C3 c,bool cattail){
  float ex=x+cosf(ang)*len, ey=y+sinf(ang)*len;
  acc_line(v,x,y,ex,ey,0.3f,c,1.5f,0.1f,HM_ON);
  if(cattail){ acc_blob(v,ex,ey,0.9f,0.45f,ang,hx(0x5a3a1a),2.0f,0.05f,0,0); }
}
typedef struct { float r; C3 c; } CoinU;
static void sh_coin(float lx,float ly,void *u,Fr *f){
  CoinU *k=(CoinU*)u; float d=sqrtf(lx*lx+ly*ly); f->a=aa(d-k->r); if(f->a<=0) return;
  float rim=d>k->r*0.75f?1.0f:0.0f; f->c=csc(k->c,0.85f+0.25f*rim-0.2f*sat1((lx+ly)/k->r)); f->h=0.3f+0.15f*rim; f->sp=0.95f;
}
void acc_coin(Cv *v,float x,float y,float r,C3 c){ CoinU k={r,c}; stamp(v,x,y,r+1,r+1,sh_coin,&k); }
typedef struct { float r; C3 c0,c1; uint32_t s; } MossU;
static void sh_moss(float lx,float ly,void *u,Fr *f){
  MossU *k=(MossU*)u; float n=fbm2(lx+k->s%50,ly,1.5f,3,k->s,0); float d=sqrtf(lx*lx+ly*ly)-k->r*(0.75f+0.35f*n);
  f->a=aa(d)*0.95f; if(f->a<=0) return;
  float g=fbm2(lx,ly+7,0.5f,2,k->s+5,0); f->c=cmix(k->c0,k->c1,sat1(0.5f+g)); f->h=0.35f+0.25f*g; f->sp=0.04f;
}
void acc_moss(Cv *v,float x,float y,float r,C3 c0,C3 c1,uint32_t s){ MossU k={r,c0,c1,s}; stamp(v,x,y,r*1.4f+1,r*1.4f+1,sh_moss,&k); }

/* ======================================================================= */
/*                     export : fichiers du dossier « donnees »             */
/* ======================================================================= */
typedef struct { int id, gw, gh, ax, lx; Cv *v; } XReg;
static uint8_t to8(float x){ int v=(int)lroundf(x*255.0f); return (uint8_t)(v<0?0:(v>255?255:v)); }
static void normal_at(const Cv *v,int x,int y,float *nx,float *ny,float *nz){
  float hl=v->ht[cv_i(v,x-1,y)], hr=v->ht[cv_i(v,x+1,y)], hu=v->ht[cv_i(v,x,y-1)], hd=v->ht[cv_i(v,x,y+1)];
  float dx=(hr-hl)*HS*0.5f, dy=(hd-hu)*HS*0.5f;
  float l=sqrtf(dx*dx+dy*dy+1); *nx=-dx/l; *ny=-dy/l; *nz=1/l;
}
static void wr32(FILE *f,uint32_t v){ fwrite(&v,4,1,f); }
static bool export_gx(const char *path,int kind,XReg *R,int nr,const Glows *gl,const int *glreg,int ng){
  int aw=0, lw=0, ah=0, lh=0;
  for(int i=0;i<nr;i++){ R[i].ax=aw; R[i].lx=lw; aw+=R[i].gw*HS+16; lw+=R[i].gw+4; if(R[i].gh*HS>ah) ah=R[i].gh*HS; if(R[i].gh>lh) lh=R[i].gh; }
  aw=(aw+3)&~3; lw=(lw+3)&~3;
  FILE *f=fopen(path,"wb"); if(!f){ printf("impossible d'ecrire %s\n",path); return false; }
  fwrite("GXH1",1,4,f); wr32(f,1); wr32(f,(uint32_t)kind); wr32(f,HS); wr32(f,(uint32_t)nr);
  for(int i=0;i<nr;i++){ wr32(f,(uint32_t)R[i].id); wr32(f,(uint32_t)R[i].gw); wr32(f,(uint32_t)R[i].gh); wr32(f,(uint32_t)R[i].ax); wr32(f,(uint32_t)R[i].lx); }
  wr32(f,(uint32_t)aw); wr32(f,(uint32_t)ah); wr32(f,(uint32_t)lw); wr32(f,(uint32_t)lh);
  /* basse definition : moyenne des blocs HSxHS */
  for(int r=0;r<nr;r++){
    Cv *v=R[r].v; int gw=R[r].gw, gh=R[r].gh; size_t n=(size_t)gw*gh;
    uint32_t *alb=(uint32_t*)malloc(n*4), *nrm=(uint32_t*)malloc(n*4); uint8_t *em=(uint8_t*)malloc(n), *mv=(uint8_t*)malloc(n);
    for(int y=0;y<gh;y++) for(int x=0;x<gw;x++){
      float cr=0,cg=0,cb=0,nx=0,ny=0,nz=0,sp=0,me=0,emax=0; int code=0;
      for(int j=0;j<HS;j++) for(int i=0;i<HS;i++){
        int X=x*HS+i, Y=y*HS+j, k=Y*v->w+X;
        cr+=v->c[k*3]; cg+=v->c[k*3+1]; cb+=v->c[k*3+2]; float a,b,c; normal_at(v,X,Y,&a,&b,&c); if(R[r].id==3) a=-a; nx+=a; ny+=b; nz+=c; sp+=v->sp[k]; me+=v->em[k];
        if(v->em[k]>emax){ emax=v->em[k]; code=v->cd[k]; }
      }
      float q=1.0f/(HS*HS); cr*=q; cg*=q; cb*=q; sp*=q; me*=q; float l=sqrtf(nx*nx+ny*ny+nz*nz)+1e-6f; nx/=l; ny/=l; nz/=l;
      size_t o=(size_t)y*gw+x;
      alb[o]=0xFF000000u|((uint32_t)to8(cr)<<16)|((uint32_t)to8(cg)<<8)|to8(cb);
      nrm[o]=((uint32_t)to8(sp)<<24)|((uint32_t)to8(nx*0.5f+0.5f)<<16)|((uint32_t)to8(ny*0.5f+0.5f)<<8)|to8(nz*0.5f+0.5f);
      em[o]= me>=0.07f ? (uint8_t)(code?code:1) : 0; mv[o]=to8(me);
      if(kind==1) em[o]= (code==7 && me>0.5f) ? 7 : 6;      /* fond : 7 = ciel lumineux, 6 = sol eclaire */
    }
    fwrite(alb,4,n,f); fwrite(em,1,n,f); fwrite(nrm,4,n,f); fwrite(mv,1,n,f);
    free(alb); free(nrm); free(em); free(mv);
  }
  wr32(f,(uint32_t)ng);
  for(int i=0;i<ng;i++){ float a[3]={gl->x[i],gl->y[i],gl->r[i]}; fwrite(a,4,3,f); wr32(f,gl->c[i]); wr32(f,(uint32_t)glreg[i]); }
  /* atlas HD : couleur + lumiere propre, normales + brillance */
  uint8_t *A=(uint8_t*)calloc((size_t)aw*ah,4), *N=(uint8_t*)calloc((size_t)aw*ah,4);
  for(int r=0;r<nr;r++){
    Cv *v=R[r].v; int x0=R[r].ax;
    for(int y=0;y<ah;y++) for(int xx=-8;xx<v->w+8;xx++){
      int X=x0+xx; if(X<0||X>=aw) continue; int sx=xx<0?0:(xx>=v->w?v->w-1:xx); int sy=y%v->h; int k=sy*v->w+sx;
      uint8_t *pa=&A[((size_t)y*aw+X)*4], *pn=&N[((size_t)y*aw+X)*4];
      pa[0]=to8(v->c[k*3]); pa[1]=to8(v->c[k*3+1]); pa[2]=to8(v->c[k*3+2]); pa[3]=to8(v->em[k]);
      float a,b,c; normal_at(v,sx,sy,&a,&b,&c); if(R[r].id==3) a=-a; pn[0]=to8(a*0.5f+0.5f); pn[1]=to8(b*0.5f+0.5f); pn[2]=to8(c*0.5f+0.5f); pn[3]=to8(v->sp[k]);
    }
  }
  fwrite(A,4,(size_t)aw*ah,f); fwrite(N,4,(size_t)aw*ah,f);
  free(A); free(N);
  long sz=ftell(f); fclose(f);
  printf("  %s : %.1f Mo (atlas %dx%d)\n",path,sz/1048576.0,aw,ah);
  return true;
}

static void finish(Cv *v,float ao,float rad,uint32_t seed){ cv_ao(v,ao,rad); cv_grain(v,0.018f,seed); }
void cv_tint(Cv *v,C3 mul,C3 toward,float k){ size_t n=(size_t)v->w*v->h; for(size_t i=0;i<n;i++){ if(v->cd[i]==7 && v->em[i]>=0.99f) continue; C3 c=cv_col(v,(int)i); c=c3(c.r*mul.r,c.g*mul.g,c.b*mul.b); cv_set(v,(int)i,cmix(c,toward,k*(1-v->em[i]))); } }

int HS=3;
/* teinte des parties du niveau : plus on descend, plus c'est sombre et teinte de la lumiere du niveau */
static const uint32_t VGLOW[BIOME_COUNT]={0xffb060,0x45c2c8,0xff7a22,0xa060ff,0x9ae8ff,0x3ab8c0,0xffa030,0xc8d8ff,0xd8ff6a,0xa8e040,0x9af8ff,0xffc040};
static void variant_tint(Cv *v,int b,int var){
  if(var<=0) return;
  float m= var==1 ? 0.94f : 0.88f; C3 g=hx(VGLOW[b]);
  cv_tint(v,c3(m,m,m*1.02f),csc(g,0.35f),var==1?0.05f:0.08f);
}
int hd_bake(const char *dir,int b0,int b1,bool camp,int v0,int v1){
  for(int b=b0;b<=b1 && b<BIOME_COUNT;b++) for(int var=v0;var<=v1;var++){
    printf("niveau %d : %s (partie %d, %d texels/px)\n",b+1,BIOMES[b].name,var,HS); fflush(stdout);
    uint32_t seed=1000u+(uint32_t)b*7919u+(uint32_t)var*104729u;
    Glows gf={0}, gs={0}; int greg[192]; int ng=0;
    Cv fl=cv_new(PIT_W,384,true); hg_seed(seed); hd_floor_variant(&fl,b,var,seed,&gf); finish(&fl,0.55f,5.0f,seed); variant_tint(&fl,b,var);
    Cv sd[2], fc[2];
    for(int s=0;s<2;s++){ sd[s]=cv_new(SC_W,SC_H,true); hg_seed(seed+11u+(uint32_t)s); hd_side(&sd[s],b,s,seed+(uint32_t)s*977u,&gs); finish(&sd[s],0.6f,7.0f,seed+3u+(uint32_t)s); variant_tint(&sd[s],b,var); }
    for(int s=0;s<2;s++){ fc[s]=cv_new(SC_FW,SC_H,true); hg_seed(seed+23u+(uint32_t)s); hd_face(&fc[s],b,s,seed+(uint32_t)s*331u); finish(&fc[s],0.5f,3.0f,seed+5u+(uint32_t)s); variant_tint(&fc[s],b,var); }
    Glows all={0};
    for(int i=0;i<gf.n && all.n<96;i++){ glow_add(&all,gf.x[i],gf.y[i],gf.c[i],gf.r[i]); greg[ng++]=0; }
    for(int i=0;i<gs.n && all.n<96;i++){ glow_add(&all,gs.x[i],gs.y[i],gs.c[i],gs.r[i]); greg[ng++]=1; }
    XReg R[5]={{0,PIT_W,384,0,0,&fl},{1,SC_W,SC_H,0,0,&sd[0]},{2,SC_W,SC_H,0,0,&sd[1]},{3,SC_FW,SC_H,0,0,&fc[0]},{4,SC_FW,SC_H,0,0,&fc[1]}};
    char p[512]; if(var==0) snprintf(p,sizeof p,"%s/niveau_%02d.gx",dir,b+1); else snprintf(p,sizeof p,"%s/niveau_%02d_%d.gx",dir,b+1,var);
    export_gx(p,0,R,5,&all,greg,ng);
    cv_free(&fl); for(int s=0;s<2;s++){ cv_free(&sd[s]); cv_free(&fc[s]); }
  }
  if(camp) for(int season=0;season<4;season++){
    printf("campement (saison %d)\n",season); fflush(stdout);
    Glows g={0}; int greg[96]={0};
    Cv c=cv_new(SCREEN_W,SCREEN_H,false); hg_seed(4242); hd_camp(&c,4242,&g,season); finish(&c,0.5f,6.0f,4243);
    XReg R[1]={{5,SCREEN_W,SCREEN_H,0,0,&c}};
    char p[512]; snprintf(p,sizeof p,"%s/campement_%d.gx",dir,season);
    export_gx(p,1,R,1,&g,greg,g.n);
    cv_free(&c);
  }
  return 0;
}
