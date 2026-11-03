/* GOUFFRE - atelier HD : flancs du puits (replat + falaise a facettes), parois du puits (vue 2.5D),
   et fond peint du campement. */
#include "hdgen.h"
#include <string.h>
#include <stdlib.h>

#define SWW SC_W
#define SHH SC_H

typedef struct { float a[5], p[5]; } Wv;
static const int WF[5]={1,2,3,5,8};
static Wv wv_new(float amp){ Wv w; for(int k=0;k<5;k++){ w.a[k]=amp*hg_range(0.4f,1.0f)/(1+0.55f*WF[k]); w.p[k]=hg_range(0,TAU); } return w; }
static float wv(const Wv *w,float y){ float s=0; for(int k=0;k<5;k++) s+=w->a[k]*sinf(TAU*WF[k]*y/SHH+w->p[k]); return s; }

/* palette du biome */
typedef struct { C3 rock[6], cov[5], rim[4], line; float top, shelf, spec; } Pal;
static Pal pal_of(int b){ uint32_t r[6],c[5],ri[4],l; Pal p; scenery_pal(b,r,c,&l,ri,&p.top,&p.shelf,&p.spec);
  for(int i=0;i<6;i++) p.rock[i]=hx(r[i]); for(int i=0;i<5;i++) p.cov[i]=hx(c[i]); for(int i=0;i<4;i++) p.rim[i]=hx(ri[i]); p.line=hx(l); return p; }
static C3 ramp6(const C3 *r,float t){ t=sat1(t)*4.999f; int i=(int)t; return cmix(r[i],r[i+1],t-i); }
static C3 ramp5(const C3 *r,float t){ t=sat1(t)*3.999f; int i=(int)t; return cmix(r[i],r[i+1],t-i); }

/* ======================================================================= */
/*                                 flancs                                   */
/* ======================================================================= */
static int g_side=0;
static inline float TXu(float u){ return g_side==0 ? SWW-u : u; }          /* u (distance au chemin) -> colonne de la texture */
static float EDGE[SHH*8];

static void side_terrain(Cv *v,int biome,const Pal *p,uint32_t s){
  bool voidb = biome==7;
  float tstep=hg_range(13,17);
  for(int y=0;y<v->h;y++) for(int x=0;x<v->w;x++){
    float gx=(x+0.5f)/HS, gy=(y+0.5f)/HS; float u= g_side==0 ? SWW-gx : gx;
    int i=y*v->w+x; float eu=EDGE[y]+9.0f*fbm2(u,gy,24,2,s+1,SHH);
    float n=fbm2(u,gy,10,3,s+2,SHH);
    if(u<eu){ /* replat : couverture du sol */
      float sv=0.55f+0.32f*n+0.18f*fbm2(u,gy,2.5f,2,s+3,SHH); float foot=sat1((eu-u)/9.0f); sv-=(1-foot)*0.3f;
      cv_set(v,i,ramp5(p->cov,sv)); v->ht[i]=0.25f*n+0.12f*fbm2(u,gy,1,2,s+4,SHH); v->sp[i]=p->spec*0.4f;
      if(voidb) cv_set(v,i,cmix(ramp6(p->rock,0.35f+0.3f*n),ramp5(p->cov,sv),0.3f));
      continue; }
    float d=u-eu;
    if(voidb){ float isl=0.5f+0.5f*fbm2(u,gy,22,3,s+9,SHH)+(d<18?0.25f:0)-(u>110?0.15f:0);
      if(isl<0.62f){ float g=0.5f+0.5f*fbm2(u,gy,40,3,s+12,SHH); cv_set(v,i,cmix(hx(0x07060c),hx(0x161230),sat1(g*1.3f-0.3f))); v->ht[i]=-6; v->sp[i]=0; continue; } }
    /* gradins : marches paralleles au bord, contours dechiquetes */
    float w=d+7.0f*fbm2(u,gy,18,3,s+31,SHH)+2.5f*fbm2(u,gy,5,2,s+32,SHH);
    float k=floorf(w/tstep), fr=w/tstep-k; float riser=sst(0.0f,0.2f,fr);
    float H=2.6f*(k+riser)+1.6f*sst(0,4,d);
    /* blocs : fissures entre gros blocs, plans legerement inclines */
    Vo o=voro(u,gy,24,SHH,s+5,0.95f); float e=(o.f2-o.f1)*0.5f;
    float gxp=(hfl(o.id)-0.5f)*0.12f, gyp=(hfl(o.id>>8)-0.5f)*0.12f;
    float rt=ridge2(u,gy,4,3,s+7,SHH), rt2=ridge2(u,gy,1.2f,2,s+8,SHH);
    H+=gxp*(u-o.px)+gyp*(gy-o.py)+0.45f*rt+0.15f*rt2;
    bool crack = e<0.9f && hfl(o.id>>4)<0.75f;
    if(crack) H-=1.9f*(1-e/0.9f);
    float strata=fabsf(fmodf(w*0.9f+40,4.0f)-2.0f); bool sline = strata<0.18f && riser>0.98f && hfl(o.id>>12)<0.6f;
    v->ht[i]=H*sst(0,3,d)+0.3f*n;
    /* couleur peinte : dessus clairs, contremarches plus sombres, roche plus froide au loin */
    float lit=0.6f+0.16f*n+0.12f*(rt-0.5f)-0.32f*(1-riser)+0.08f*(hfl(o.id>>16)-0.5f)-0.05f*fminf(k,3);
    C3 c=ramp6(p->rock,lit);
    if(crack) c=cmix(c,p->line,0.7f*(1-e/0.9f));
    if(sline) c=cmix(c,p->line,0.35f);
    float cov=0.55f*riser+0.35f*(0.5f+0.5f*fbm2(u,gy,9,3,s+11,SHH))+0.12f*n;
    float th=1.0f-0.55f*p->top;
    if(p->top>0 && cov>th && !crack){ float kk=sat1((cov-th)*7); c=cmix(c,ramp5(p->cov,0.38f+0.42f*kk+0.2f*n),kk); v->ht[i]+=0.35f*kk; }
    cv_set(v,i,c); v->sp[i]=p->spec*(0.4f+0.6f*sat1(lit));
  }
  /* blocs eboules au pied des gradins */
  for(int b=0;b<34;b++){ float y=hg_range(0,SHH); int yi=(int)(y*HS)%(SHH*HS); float u=EDGE[yi]+hg_range(2,60); if(u>SWW-6) continue;
    acc_rock(v,TXu(u),y,hg_range(2.2f,4.5f),p->rock[1],p->rock[4],hg_range(2,3.5f),s+700u+(uint32_t)b); }
}
static void side_wall(Cv *v,const Pal *p,uint32_t s){
  float y=0;
  while(y<SHH-0.01f){ float seg=hg_range(7,14); if(SHH-y<9 || y+seg>SHH) seg=SHH-y;
    bool capped=hg_chance(p->top*0.9f); float tone=hg_range(-0.08f,0.08f);
    float u0=0.3f, u1=6.6f; float x0=fminf(TXu(u0),TXu(u1)), x1=fmaxf(TXu(u0),TXu(u1));
    acc_box(v,x0,y+0.35f,x1,y+seg-0.35f,csc(p->rim[2],1+tone),2.6f,1.2f,0.12f);
    if(capped) acc_blob(v,TXu(3.4f),y+seg*0.45f,2.6f,seg*0.38f,0,ramp5(p->cov,0.65f),2.9f,0.06f,0,0);
    if(hg_chance(0.5f)) acc_crack(v,TXu(hg_range(1,6)),y+hg_range(1,seg-1),hg_range(2,4),0.25f,0.3f,p->line,0,0,0,s+(uint32_t)y);
    y+=seg; }
}
static void side_torches(Cv *v,const Pal *p){
  for(int k=0;k<3;k++){ float ty=(float)SC_TORCH_Y[g_side][k];
    float x0=fminf(TXu(5),TXu(12)), x1=fmaxf(TXu(5),TXu(12));
    acc_box(v,x0,ty-2.2f,x1,ty+6.2f,p->rim[2],3.6f,0.9f,0.15f);
    acc_box(v,x0+0.8f,ty-1.4f,x1-0.8f,ty+5.4f,p->rim[3],3.9f,0.6f,0.15f); }
}
static bool near_torch(float u,float y){ for(int k=0;k<3;k++){ float d=fabsf(y-SC_TORCH_Y[g_side][k]); if(d>SHH/2) d=SHH-d; if(d<14 && u<16) return true; } return false; }
static void side_glow(Glows *g,float u,float y,uint32_t c,float r){
  float sx = g_side==0 ? (SWW-u) : (PIT_X1+u);
  glow_add(g,sx,fmodf(y+SHH*4,SHH),c,r);
}
static void side_props(Cv *v,int biome,const Pal *p,uint32_t s,Glows *g){
  for(int i=0;i<46;i++){
    float y=hg_range(0,SHH); int yi=(int)(y*HS)%(SHH*HS); float u=hg_range(10,fmaxf(11,EDGE[yi]-4));
    if(near_torch(u,y)) continue;
    float X=TXu(u);
    switch(biome){
      case 0: if(i%3==0){ float a=hg_range(0,TAU); acc_bone(v,X,y,X+cosf(a)*4,y+sinf(a)*4,0.55f,hx(0xd8ccb4)); if(hg_chance(0.4f)) acc_skull(v,X+2,y-2,1.9f,hg_range(-0.6f,0.6f),hx(0xd8ccb4)); }
              else if(i%3==1) acc_rock(v,X,y,hg_range(2.5f,4.5f),p->rock[1],p->rock[4],2.5f,s+(uint32_t)i);
              else if(i%5==2){ for(int k=0;k<3;k++) acc_candle(v,X+k*2.6f-2.6f,y+hg_range(-1,1),0.85f,hg_range(3,5),1+hg_irange(0,200),g,X,y); side_glow(g,u,y-4,0xffb060,30); }
              else acc_grass(v,X,y,2,6,hx(0x2e4a2e),hx(0x4a6a3a),2.4f,s+(uint32_t)i); break;
      case 1: if(i%4==0){ bool t=hg_chance(0.35f); acc_mushroom(v,X,y,hg_range(2.6f,4.2f),t,t?128+hg_irange(0,60):1+hg_irange(0,60)); side_glow(g,u,y,t?0x45c2c8:0xf59ac0,30); }
              else if(i%4==1){ acc_rock(v,X,y,hg_range(2.5f,4.5f),p->rock[1],p->rock[4],2.6f,s+(uint32_t)i); acc_moss(v,X-0.5f,y-0.8f,2.2f,hx(0x3a7a32),hx(0x6aa848),s+(uint32_t)i); }
              else acc_grass(v,X,y,2.5f,9,hx(0x3a8a3a),hx(0x7ac85a),3.0f,s+(uint32_t)i); break;
      case 2: if(i%3==0) acc_rock(v,X,y,hg_range(2.5f,4.5f),p->rock[1],p->rock[4],2.5f,s+(uint32_t)i);
              else if(i%4==1){ for(int k=0;k<6;k++) acc_blob(v,X+hg_range(-3,3),y+hg_range(-2,2),hg_range(0.6f,1.1f),hg_range(0.5f,0.9f),hg_range(0,PI),hx(0xff8a2a),0.6f,0.1f,0.9f,140+hg_irange(0,100)); side_glow(g,u,y,0xff6a1a,20); }
              else acc_grass(v,X,y,2,5,hx(0x3a2a28),hx(0x5a4038),2.2f,s+(uint32_t)i); break;
      case 3: if(i%3==0){ acc_crystals(v,X,y,4.5f,3,hx(0xd2b0f5),hx(0x7a4ab8),0.85f,128+hg_irange(0,80),s+(uint32_t)i); side_glow(g,u,y-3,0xa060ff,26); }
              else if(i%3==1) acc_rock(v,X,y,hg_range(2.5f,4.5f),p->rock[1],p->rock[4],2.5f,s+(uint32_t)i);
              else acc_grass(v,X,y,2,6,hx(0x4a2266),hx(0x7a4ab8),2.4f,s+(uint32_t)i); break;
      case 4: if(i%3==0){ for(int k=0;k<10;k++){ float a=hg_range(-PI,0), l=hg_range(3,6); acc_line(v,X,y,X+cosf(a)*l,y+sinf(a)*l,0.4f,k&1?hx(0x9a8ad0):hx(0xd8ccff),1.4f,0.4f,HM_ON); } }
              else if(i%3==1){ acc_rock(v,X,y,hg_range(3,6),p->rock[1],p->rock[4],3.0f,s+(uint32_t)i); acc_blob(v,X-0.6f,y-1,2.6f,1.6f,0,hx(0xeef4ff),3.6f,0.2f,0,0); }
              else if(i%5==2){ acc_crystals(v,X,y,4,3,hx(0xe8faff),hx(0x7ac8f0),0.7f,128+hg_irange(0,80),s+(uint32_t)i); side_glow(g,u,y-3,0x9ae8ff,24); }
              else acc_grass(v,X,y,2,6,hx(0x8a7ac0),hx(0xc8bcf0),2.4f,s+(uint32_t)i); break;
      case 5: if(i%4==0){ float w=hg_range(9,13); acc_box(v,X-w*0.5f,y-3,X+w*0.5f,y+3,hx(0x6a8a80),3.5f,1.4f,0.2f); acc_blob(v,X,y,w*0.42f,2.4f,0,hx(0x9ab0a8),3.8f,0.2f,0,0); }
              else if(i%4==1){ bool pk=hg_chance(0.5f); acc_coral(v,X,y,4,pk?hx(0xf59ac0):hx(0x5af0d0),0.8f,pk?30+hg_irange(0,60):140+hg_irange(0,60)); side_glow(g,u,y-3,pk?0xf59ac0:0x5af0d0,22); }
              else if(i%4==2) acc_rock(v,X,y,hg_range(2.5f,4.5f),p->rock[1],p->rock[4],2.5f,s+(uint32_t)i);
              else for(int k=0;k<4;k++) acc_reed(v,X+k-2,y,hg_range(4,7),-PI*0.5f+hg_range(-0.4f,0.4f),hx(0x3aa85a),false); break;
      case 6: if(i%3==0) acc_rock(v,X,y,hg_range(2.5f,4.5f),p->rock[1],p->rock[4],2.5f,s+(uint32_t)i);
              else if(i%3==1){ acc_blob(v,X,y,hg_range(3,5),hg_range(2,3.5f),hg_range(0,PI),hx(0xe0a030),2.4f,0.95f,0.7f,1+hg_irange(0,80)); side_glow(g,u,y,0xffa030,22); }
              else acc_grass(v,X,y,2,5,hx(0xa07840),hx(0xd8b070),2.2f,s+(uint32_t)i); break;
      case 8: if(i%4==0){ acc_mushroom(v,X,y,hg_range(2.8f,4.2f),true,128+hg_irange(0,60)); side_glow(g,u,y,0x45c2c8,28); }
              else if(i%4==1){ acc_rock(v,X,y,hg_range(2.5f,4.5f),p->rock[1],p->rock[4],2.5f,s+(uint32_t)i); acc_moss(v,X,y-0.8f,2.2f,hx(0x3a8a4a),hx(0x7ac06a),s+(uint32_t)i); }
              else if(i%4==2){ static const uint32_t FC[3]={0xf6f2ea,0xf59ac0,0xc8a8ff}; for(int k=0;k<5;k++) acc_flower(v,X+hg_range(-5,5),y+hg_range(-3,3),0.9f,hx(FC[k%3]),hx(0xf9dc5c),5,hg_range(0,TAU)); }
              else { acc_grass(v,X,y,2.5f,9,hx(0x2e7a3a),hx(0x6ac04a),3.2f,s+(uint32_t)i); if(hg_chance(0.3f)){ acc_blob(v,X,y-4,0.6f,0.6f,0,hx(0xeaff6a),2.5f,0.3f,1.0f,1+hg_irange(0,100)); side_glow(g,u,y-4,0xd8ff6a,16); } } break;
      case 9: if(i%4==0) acc_rock(v,X,y,hg_range(2.5f,4.5f),p->rock[1],p->rock[4],2.0f,s+(uint32_t)i);
              else if(i%4==1){ for(int k=0;k<4;k++) acc_reed(v,X+k*1.2f-2,y,hg_range(5,8),-PI*0.5f+hg_range(-0.3f,0.3f),hx(0x7a7a3a),hg_chance(0.5f)); }
              else if(i%5==2){ acc_blob(v,X,y,4.5f,2.4f,0,hx(0x1c2414),-1.0f,0.95f,0,0); acc_blob(v,X,y,0.6f,0.6f,0,hx(0xb8e040),0.2f,0.5f,1.0f,1+hg_irange(0,120)); side_glow(g,u,y,0xb8e040,18); }
              else acc_grass(v,X,y,2,6,hx(0x3a4020),hx(0x6a7a2a),2.6f,s+(uint32_t)i); break;
      case 10: if(i%3==0){ bool cy=hg_chance(0.4f); acc_crystals(v,X,y,5,3,cy?hx(0xc8f8ff):hx(0xffc8f0),cy?hx(0x3a8ab8):hx(0x9a3a8a),0.85f,cy?128+hg_irange(0,90):1+hg_irange(0,80),s+(uint32_t)i); side_glow(g,u,y-3,cy?0x9af8ff:0xff8ad8,24); }
               else if(i%3==1) acc_rock(v,X,y,hg_range(2.5f,4.5f),p->rock[1],p->rock[4],2.5f,s+(uint32_t)i); else acc_grass(v,X,y,2,5,hx(0x4a4258),hx(0x6a6278),2.2f,s+(uint32_t)i); break;
      case 11: if(i%4==0){ float w=hg_range(9,13); acc_box(v,X-w*0.5f,y-3,X+w*0.5f,y+3,hx(0xc8a46e),3.5f,1.4f,0.2f); acc_blob(v,X,y,w*0.42f,2.4f,0,hx(0xf0dcb0),3.9f,0.25f,0,0); }
               else if(i%4==1) acc_rock(v,X,y,hg_range(2.5f,4.5f),p->rock[1],p->rock[4],2.5f,s+(uint32_t)i);
               else if(i%4==2){ for(int k=0;k<4;k++) acc_coin(v,X+hg_range(-4,4),y+hg_range(-3,3),0.7f,hx(0xffd84a)); acc_blob(v,X,y,0.8f,0.8f,0,hx(0xffd84a),0.6f,0.9f,0.9f,1+hg_irange(0,100)); side_glow(g,u,y,0xffc040,18); }
               else acc_grass(v,X,y,2,5,hx(0xa08850),hx(0xd8c088),2.2f,s+(uint32_t)i); break;
      default: if(i%3==0) acc_rock(v,X,y,hg_range(2.5f,4.5f),p->rock[1],p->rock[4],2.5f,s+(uint32_t)i);
               else for(int k=0;k<3;k++) acc_blob(v,X+hg_range(-6,6),y+hg_range(-6,6),0.3f,0.3f,0,hx(0xe0e8ff),0.2f,0.5f,1.0f,1+hg_irange(0,60)); break;
    }
  }
  /* details dans la falaise */
  for(int i=0;i<40;i++){
    float y=hg_range(0,SHH); int yi=(int)(y*HS)%(SHH*HS); float u=hg_range(EDGE[yi]+8,SWW-8); if(u>=SWW-8) continue;
    float X=TXu(u);
    switch(biome){
      case 0: if(i%4==0){ acc_box(v,fminf(X,TXu(u+9)),y,fmaxf(X,TXu(u+9)),y+9,hx(0x1e1a22),-1.5f,0.6f,0.02f); acc_skull(v,TXu(u+4.5f),y+5,1.7f,0,hx(0xc8bca4)); }
              else if(i%7==3){ for(int k=0;k<3;k++) acc_candle(v,X+k*2.4f,y,0.8f,hg_range(3,5),1+hg_irange(0,200),g,X,y); side_glow(g,u,y-4,0xffb060,28); } break;
      case 1: if(i%3==0) acc_grass(v,X,y,2.5f,8,hx(0x3a8a3a),hx(0x5aa846),2.6f,s+(uint32_t)i); else if(i%5==1){ bool t=hg_chance(0.4f); acc_mushroom(v,X,y,3,t,t?128+hg_irange(0,60):1+hg_irange(0,60)); } break;
      case 2: if(i%4==0) acc_crack(v,X,y,hg_range(16,40),0.7f,0.6f,hx(0xff7a22),1.0f,1,1,s+(uint32_t)i*7); break;
      case 3: if(i%5==0){ acc_blob(v,X,y,3.2f,2.2f,0,hx(0xf0e8d8),1.4f,0.6f,0,0); acc_blob(v,X,y,1.4f,1.4f,0,hx(0x8a1a3a),1.6f,0.8f,0.6f,128+hg_irange(0,40)); acc_blob(v,X,y,0.6f,0.6f,0,hx(0x0a0608),1.7f,0.9f,0,0); }
              else if(i%3==0){ acc_crystals(v,X,y,5,3,hx(0xd2b0f5),hx(0x7a4ab8),0.85f,128+hg_irange(0,80),s+(uint32_t)i); side_glow(g,u,y-3,0xa060ff,22); } break;
      case 5: if(i%3==0) for(int k=0;k<5;k++) acc_reed(v,X+k-2,y,hg_range(5,8),-PI*0.5f+hg_range(-0.4f,0.4f),hx(0x2a8a4a),false); else if(i%7==1) acc_coral(v,X,y,3.5f,hg_chance(0.5f)?hx(0xf59ac0):hx(0x5af0d0),0.7f,140); break;
      case 6: if(i%3==0) acc_blob(v,X,y,1.4f,hg_range(4,9),0,hx(0xe0a030),0.8f,0.95f,0.5f,1+hg_irange(0,60)); break;
      case 8: if(i%3==0) acc_grass(v,X,y,2.5f,8,hx(0x2e7a3a),hx(0x6ac04a),3,s+(uint32_t)i); else if(i%5==1) acc_mushroom(v,X,y,3,true,128+hg_irange(0,60)); break;
      case 9: if(i%3==0) acc_grass(v,X,y,2.5f,8,hx(0x3a4020),hx(0x6a7a2a),3,s+(uint32_t)i); break;
      case 10: if(i%3==0){ bool cy=hg_chance(0.5f); acc_crystals(v,X,y,5,3,cy?hx(0xc8f8ff):hx(0xffc8f0),cy?hx(0x3a8ab8):hx(0x9a3a8a),0.85f,cy?128+hg_irange(0,90):1+hg_irange(0,80),s+(uint32_t)i); side_glow(g,u,y-3,cy?0x9af8ff:0xff8ad8,22); } break;
      case 11: if(i%4==0) acc_crack(v,X,y,hg_range(12,28),0.6f,0.3f,hx(0xffd84a),0.9f,1,2,s+(uint32_t)i*5); break;
      default: break;
    }
  }
}
void hd_side(Cv *v,int biome,int side,uint32_t seed,Glows *g){
  g_side=side; Pal p=pal_of(biome);
  Wv w1=wv_new(16), w2=wv_new(6);
  for(int y=0;y<SHH*HS;y++){ float gy=(y+0.5f)/HS; EDGE[y]=p.shelf+wv(&w1,gy)+wv(&w2,gy); }
  side_terrain(v,biome,&p,seed);
  side_wall(v,&p,seed+50);
  side_torches(v,&p);
  side_props(v,biome,&p,seed+90,g);
  if(biome==7) for(int i=0;i<260;i++){ float x=hg_range(0,SWW), y=hg_range(0,SHH); int k=cv_i(v,(int)(x*HS),(int)(y*HS)); if(v->ht[k]<-5) acc_blob(v,x,y,0.3f,0.3f,0,hx(0xe8eeff),-5.8f,0,1.0f,1+hg_irange(0,60)); }
}

/* ======================================================================= */
/*                     parois du puits (u : 0 = pied, 25 = rebord)           */
/* ======================================================================= */
void hd_face(Cv *v,int biome,int side,uint32_t seed){
  (void)side; Pal p=pal_of(biome);
  const float CH=6.5f;
  for(int y=0;y<v->h;y++) for(int x=0;x<v->w;x++){
    float u=(x+0.5f)/HS, gy=(y+0.5f)/HS; int i=y*v->w+x;
    int course=(int)(u/CH); float cu=u-course*CH;
    int nb=(int)lroundf(SHH/(10+3*hfl(hh3(course,1,seed)))); float bl=(float)SHH/nb;
    float off=(course&1)*bl*0.5f+hfl(hh3(course,0,seed))*3;
    float yy=gy+off; int blk=(int)floorf(yy/bl); float cy=yy-blk*bl; blk=((blk%nb)+nb)%nb;
    float e=fminf(fminf(cu,CH-cu),fminf(cy,bl-cy))-0.45f+0.25f*fbm2(u,gy,1.2f,2,seed+3,SHH);
    uint32_t id=hh3(course,blk,seed);
    float tn=(hfl(id)-0.5f)*0.16f; float n=fbm2(u,gy,4,3,seed+5,SHH);
    C3 c=ramp6(p.rock,0.42f+0.25f*(u/SC_FW)+0.15f*n); c=csc(c,1+tn);
    float h=0.6f*u;                                /* la paroi regarde le puits */
    if(e<0){ c=cmix(c,p.line,0.75f); h-=0.5f; }
    else { float t=sst(0,1.1f,e); h+=0.9f*t+0.1f*n; }
    if(u>SC_FW-3){ c=cmix(c,p.rim[3],0.55f); h+=0.5f; }
    if(u<2.2f) c=csc(c,0.7f+0.12f*u);
    float drip=fbm2(u*0.3f,gy,6,3,seed+9,SHH);
    if(p.top>0 && u>SC_FW-8 && drip>0.15f-0.06f*(SC_FW-u)){ c=cmix(c,ramp5(p.cov,0.6f+0.3f*n),0.8f); h+=0.25f; }
    cv_set(v,i,c); v->ht[i]=h; v->sp[i]=p.spec*0.6f;
  }
  for(int i=0;i<14;i++){ float y=hg_range(0,SHH), u=hg_range(2,SC_FW-2);
    switch(biome){
      case 2: acc_crack(v,u,y,hg_range(6,14),0.5f,0.4f,hx(0xff7a22),0.9f,1,2,seed+(uint32_t)i); break;
      case 3: if(i%2==0) acc_crack(v,u,y,hg_range(6,12),0.4f,0.3f,hx(0x8a4ad8),0.9f,1,4,seed+(uint32_t)i); break;
      case 4: acc_blob(v,u,y,0.8f,hg_range(2,5),0,hx(0xd8ecff),0.4f,0.9f,0,0); break;
      case 5: acc_reed(v,u,y,hg_range(3,6),hg_range(0,TAU),hx(0x2a8a4a),false); break;
      case 6: acc_blob(v,u,y,0.9f,hg_range(2,5),0,hx(0xe0a030),0.4f,0.95f,0.4f,1+hg_irange(0,60)); break;
      case 7: acc_blob(v,u,y,0.3f,0.3f,0,hx(0xd8e0f8),0.1f,0.5f,0.9f,1+hg_irange(0,60)); break;
      case 8: acc_moss(v,u,y,1.8f,hx(0x3a8a4a),hx(0x6ac04a),seed+(uint32_t)i); break;
      case 9: acc_blob(v,u,y,0.7f,hg_range(2,6),0,hx(0x4a5a26),0.3f,0.8f,0,0); break;
      case 10: if(i%2==0){ bool cy=hg_chance(0.5f); acc_crystal(v,u,y,1.6f,2,hg_range(0,PI),cy?hx(0xc8f8ff):hx(0xffc8f0),cy?hx(0x3a8ab8):hx(0x9a3a8a),0.8f,cy?140:20); } break;
      case 11: if(i%3==0) acc_box(v,SC_FW-3.2f,y,SC_FW-1.8f,y+hg_range(8,16),hx(0xe8c050),0.6f,0.3f,0.9f); break;
      case 0: if(i%3==0) acc_skull(v,u,y,1.3f,PI*0.5f,hx(0xc8bca4)); break;
      case 1: acc_moss(v,u,y,1.6f,hx(0x3a7a32),hx(0x6aa848),seed+(uint32_t)i); break;
      default: break;
    }
  }
}

/* ======================================================================= */
/*                              campement                                    */
/* ======================================================================= */
static int SEASON=3;   /* v9 : 0 hiver, 1 printemps, 2 ete, 3 automne */
static float hill1(float x){ return 118+sinf(x*0.013f)*10+sinf(x*0.031f+1)*6; }
static float hill2(float x){ return 126+sinf(x*0.021f+2)*6; }
typedef struct { uint32_t s; } SkyU;
static void sh_sky(float x,float y,void *u,Fr *f){
  SkyU *k=(SkyU*)u; if(y>hill2(x)+0.5f) return;
  float t=sat1(y/130.0f);
  C3 c= t<0.55f ? cmix(hx(0x05051a),hx(0x120f2e),t/0.55f) : cmix(hx(0x120f2e),hx(0x2e2448),(t-0.55f)/0.45f);
  float n1=0.5f+0.5f*fbm2(x,y,90,5,k->s,0), n2=0.5f+0.5f*fbm2(x+300,y,60,4,k->s+1,0);
  c=cadd(c,csc(hx(0x2a1a5a),0.35f*sst(0.55f,0.85f,n1)*(1-t*0.6f)));
  c=cadd(c,csc(hx(0x103a6a),0.22f*sst(0.6f,0.9f,n2)*(1-t*0.5f)));
  /* voie lactee : bande diagonale */
  float d=fabsf((y-20)-(x-60)*0.16f-0.0f*x)/40.0f; float band=sst(1.0f,0.0f,d);
  float mw=ridge2(x,y,22,5,k->s+3,0); float lane=sst(0.62f,0.8f,0.5f+0.5f*fbm2(x,y,14,4,k->s+4,0));
  c=cadd(c,csc(cmix(hx(0x6a5aa8),hx(0xc8b8e8),mw),0.32f*band*mw*(1-0.75f*lane)));
  if(SEASON==0){ float yc=46+14*sinf(x*0.012f+1)+6*sinf(x*0.031f), d=(y-yc)/30.0f; float au= d<0 ? sst(-1.6f,0,d) : sst(1.0f,0,d);
    float cur=0.55f+0.45f*sinf(x*0.16f+6*fbm2(x,0,40,2,k->s+9,0)); au*=cur*(0.6f+0.4f*fbm2(x,y,18,3,k->s+9,0));
    c=cadd(c,csc(cmix(hx(0x2aff9a),hx(0x8a5aff),sat1((yc-y)/40.0f)),0.62f*au)); }   /* aurore boreale : rideaux */
  if(SEASON==2) c=cadd(c,csc(hx(0x3a1a2a),0.4f*t*t));
  f->c=c; f->a=1; f->em=1; f->code=7; f->hm=HM_DECAL; f->sp=0;
}
static void star(Cv *v,float x,float y,float r,C3 c,float b){
  int x0=(int)((x-r*4)*HS), x1=(int)((x+r*4)*HS), y0=(int)((y-r*4)*HS), y1=(int)((y+r*4)*HS);
  for(int yy=y0;yy<=y1;yy++) for(int xx=x0;xx<=x1;xx++){ if(xx<0||yy<0||xx>=v->w||yy>=v->h) continue;
    float dx=(xx+0.5f)/HS-x, dy=(yy+0.5f)/HS-y; float d2=(dx*dx+dy*dy)/(r*r); float k=expf(-d2*2.2f)*b;
    if(b>0.75f){ k+=0.25f*b*expf(-(dx*dx)/(r*r*0.05f))*expf(-fabsf(dy)/(r*2.2f)); k+=0.25f*b*expf(-(dy*dy)/(r*r*0.05f))*expf(-fabsf(dx)/(r*2.2f)); }
    if(k<0.004f) continue; int i=yy*v->w+xx; cv_set(v,i,cadd(cv_col(v,i),csc(c,k))); }
}
typedef struct { float cx,cy,r; uint32_t s; } MoonU;
static void sh_moon(float lx,float ly,void *u,Fr *f){
  MoonU *m=(MoonU*)u; float d=sqrtf(lx*lx+ly*ly);
  if(d<m->r){ float t=sqrtf(sat1(1-d*d/(m->r*m->r)));
    float cr=0.5f+0.5f*fbm2(lx+m->cx,ly+m->cy,5,4,m->s,0); float mare=sst(0.55f,0.7f,0.5f+0.5f*fbm2(lx,ly,7,3,m->s+2,0));
    C3 c=cmix(hx(0xf6f2e0),hx(0xb8b4a8),mare*0.6f); c=csc(c,0.8f+0.25f*cr);
    for(int k=0;k<7;k++){ uint32_t h=hh3(k,3,m->s); float px=(hfl(h)-0.5f)*m->r*1.4f, py=(hfl(h>>8)-0.5f)*m->r*1.4f, rr=0.6f+hfl(h>>16)*1.8f; float dc=sqrtf((lx-px)*(lx-px)+(ly-py)*(ly-py));
      if(dc<rr) c=csc(c,0.82f+0.18f*sst(0,rr,dc)+(dc>rr*0.7f?0.1f:0)); }
    float lim=0.72f+0.28f*t; c=csc(c,lim);
    f->c=c; f->a=sat1((m->r-d)*HS); f->em=1; f->code=7; f->hm=HM_DECAL; return; }
  float halo=expf(-(d-m->r)/9.0f)*0.55f+expf(-(d-m->r)/30.0f)*0.18f;
  f->c=hx(0xb8b4ff); f->a=sat1(halo)*0.6f; f->hm=HM_DECAL; f->em=1; f->code=7;
}
typedef struct { float base; int layer; uint32_t s; } MtnU;
static void sh_mtn(float x,float y,void *u,Fr *f){
  MtnU *m=(MtnU*)u; float top=m->base-(m->layer?20:34)*(0.5f+0.5f*ridge2(x,0,m->layer?70:110,4,m->s,0))-(m->layer?0:6)*sinf(x*0.01f+1);
  if(y<top || y>hill1(x)+6) return;
  float rim=sst(2.5f,0,y-top)*sat1(0.5f+0.5f*gnoise(x/40,0,0,m->s+2));
  C3 c=m->layer? hx(0x16122a) : hx(0x1c1834); float fog=sat1((y-top)/40.0f);
  c=cmix(c,hx(0x2a2448),fog*0.45f); c=cadd(c,csc(hx(0x8a88c8),rim*0.35f));
  float n=fbm2(x,y,6,3,m->s+5,0); c=csc(c,0.92f+0.1f*n);
  f->c=c; f->a=1; f->em=1; f->code=7; f->hm=HM_DECAL;
}
static void pine(Cv *v,float x,float y,float h,C3 c,C3 rim){
  int x0=(int)((x-h*0.35f)*HS), x1=(int)((x+h*0.35f)*HS), y0=(int)((y-h)*HS), y1=(int)(y*HS);
  for(int yy=y0;yy<=y1;yy++) for(int xx=x0;xx<=x1;xx++){ if(xx<0||yy<0||xx>=v->w||yy>=v->h) continue;
    float lx=(xx+0.5f)/HS-x, ly=(y-(yy+0.5f)/HS)/h; float tier=fmodf(ly*4.0f,1.0f); float w=h*0.33f*(1-ly)*(0.75f+0.25f*(1-tier));
    if(fabsf(lx)<w || (fabsf(lx)<0.35f && ly<0.15f)){ int i=yy*v->w+xx; float r=sst(w-0.6f,w,lx)*0.6f; cv_set(v,i,cadd(c,csc(rim,r))); v->em[i]=1; v->cd[i]=7; } }
}
typedef struct { uint32_t s; } GrassU;
static void sh_ground(float x,float y,void *u,Fr *f){
  GrassU *k=(GrassU*)u; if(y<hill2(x)-1) return;
  static const uint32_t G0[4]={0x2e3a44,0x2a3a22,0x343a22,0x3a3020}, G1[4]={0x161c24,0x141c10,0x1a1e10,0x1c160e};
  float t=sat1((y-138)/222.0f); C3 c=cmix(hx(G0[SEASON]),hx(G1[SEASON]),t);
  float n=fbm2(x,y,30,4,k->s,0), m=fbm2(x,y,6,3,k->s+1,0);
  c=csc(c,0.9f+0.22f*n); c=cmix(c,hx(SEASON==3?0x4a3418:0x3a3a20),sst(0.35f,0.6f,m)*0.35f);
  if(SEASON==0){ float sn=sst(0.2f,0.5f,0.5f+0.5f*fbm2(x,y,22,4,k->s+7,0)); c=cmix(c,cmix(hx(0x8a92b0),hx(0x5a6280),t),sn*0.85f); }
  f->c=c; f->a=1; f->h=0.25f*m+0.1f*fbm2(x,y,1,2,k->s+2,0); f->sp=0.05f; f->hm=HM_ADD;
}
static void path_seg(Cv *v,float x0,float y0,float x1,float y1,uint32_t s){
  float dx=x1-x0, dy=y1-y0, l=sqrtf(dx*dx+dy*dy); int n=(int)(l/1.2f)+1; float px=-dy/l, py=dx/l;
  for(int i=0;i<=n;i++){ float t=(float)i/n; float wob=1.6f*gnoise(t*2.5f,(float)(s&63)+0.5f,0,s+3)*sinf(t*PI);
    float x=x0+dx*t+px*wob, y=y0+dy*t+py*wob*0.6f; float w=(3.4f+0.7f*gnoise(t*6,(float)(s&63),0,s))*(0.75f+0.25f*t);
    acc_blob(v,x,y,w,w*0.4f,0,cmix(hx(0x3a3024),hx(0x4a3c2c),hfl(hh3(i,(int)s,5))),-0.2f,0.04f,0,0); }
  for(int i=0;i<(int)(l*0.6f);i++){ float t=hg_rand(); acc_pebbles(v,x0+dx*t+hg_range(-4,4),y0+dy*t+hg_range(-1.2f,1.2f),0.5f,1,hx(0x6a5e50),s+(uint32_t)i); }
}
static bool near_path(float x,float y){
  if(fabsf(x-320)<26 && fabsf(y-228)<10) return true;
  for(int b=0;b<BLD_COUNT;b++){ float ax=320, ay=226, bx=(float)BUILDINGS[b].x, by=(float)BUILDINGS[b].y-2; float dx=bx-ax, dy=by-ay, dd=dx*dx+dy*dy;
    float t=sat1(((x-ax)*dx+(y-ay)*dy)/dd); float ex=x-ax-dx*t, ey=(y-ay-dy*t)*2.4f; if(ex*ex+ey*ey<16) return true; }
  return false;
}
void hd_camp(Cv *v,uint32_t seed,Glows *g,int season){
  (void)g; SEASON=clampi(season,0,3);
  SkyU sk={seed}; fill(v,sh_sky,&sk);
  for(int i=0;i<1400;i++){ float x=hg_range(0,SCREEN_W), y=hg_range(0,130); if(y>hill1(x)-2) continue; float b=powf(hg_rand(),3.2f);
    static const uint32_t SC[5]={0xffffff,0xd8e4ff,0xfff0d8,0xc8d0ff,0xffe8c8}; star(v,x,y,0.18f+0.55f*b,hx(SC[hg_irange(0,4)]),0.25f+0.9f*b); }
  MoonU mo={548,40,13,seed+7}; stamp(v,548,40,60,60,sh_moon,&mo);
  MtnU m0={112,0,seed+10}; fill(v,sh_mtn,&m0); MtnU m1={120,1,seed+11}; fill(v,sh_mtn,&m1);
  for(float x=-4;x<SCREEN_W+4;x+=hg_range(2.5f,5.5f)){ float base=hill1(x)+4; float h=hg_range(9,20)*(0.7f+0.3f*sinf(x*0.05f)); pine(v,x,base,h,hx(0x0c0a18),SEASON==0?hx(0xb8c0e8):(SEASON==3?hx(0x8a5a40):hx(0x5a5aa0))); }
  for(int y=0;y<v->h;y++) for(int x=0;x<v->w;x++){ float gx=(x+0.5f)/HS, gy=(y+0.5f)/HS; int i=y*v->w+x;
    if(gy>hill1(gx) && gy<=hill2(gx)+2){ cv_set(v,i,SEASON==0?cmix(hx(0x3a3e5a),hx(0x2a2e48),sat1((gy-hill1(gx))/8)):cmix(hx(0x15122a),hx(0x1c1830),sat1((gy-hill1(gx))/8))); v->em[i]=1; v->cd[i]=7; } }
  GrassU gu={seed+20}; fill(v,sh_ground,&gu);
  for(int y=0;y<v->h;y++) for(int x=0;x<v->w;x++){ float gx=(x+0.5f)/HS, gy=(y+0.5f)/HS; if(gy>hill2(gx)+1.5f){ int i=y*v->w+x; v->em[i]=0; v->cd[i]=0; } }
  for(int b=0;b<BLD_COUNT;b++){ if(b==BLD_CREUSET) continue; path_seg(v,320,226,(float)BUILDINGS[b].x,(float)BUILDINGS[b].y-2,seed+30u+(uint32_t)b); }
  { static const uint32_t GA[4]={0x5a6478,0x3a5a2a,0x4a5228,0x6a4a24}, GB[4]={0xc8d0e8,0x6aba40,0x9aa83a,0xc8803a}; int ng= SEASON==0 ? 700 : 2600;
  for(int i=0;i<ng;i++){ float x=hg_range(0,SCREEN_W), y=hg_range(140,SCREEN_H); if(near_path(x,y)) continue;
    float tt=sat1((y-138)/222.0f); acc_grass(v,x,y,1.6f,4,cmix(hx(GA[SEASON]),hx(0x222a18),tt*0.6f),cmix(hx(GB[SEASON]),hx(0x34482a),tt*0.5f),2.0f,seed+100u+(uint32_t)i); } }
  if(SEASON==3) for(int i=0;i<1300;i++){ static const uint32_t LF[5]={0xe0782a,0xc0401e,0xf0b040,0x9a6a2a,0xd8581e}; float x=hg_range(0,SCREEN_W), y=hg_range(140,SCREEN_H); if(near_path(x,y) && hg_range(0,1)<0.6f) continue; acc_leaf(v,x,y,hg_range(1.3f,2.4f),hg_range(0,TAU),csc(hx(LF[i%5]),0.95f)); }
  if(SEASON!=0) for(int i=0;i<(SEASON==1?520:(SEASON==2?260:30));i++){ float x=hg_range(0,SCREEN_W), y=hg_range(150,SCREEN_H); static const uint32_t FC[4]={0xf6f2ea,0xc8a8ff,0xf9dc5c,0xf59ac0}; acc_flower(v,x,y,0.7f,csc(hx(FC[i&3]),0.7f),hx(0xd8b040),5,hg_range(0,TAU)); }
  for(int i=0;i<60;i++) acc_rock(v,hg_range(0,SCREEN_W),hg_range(150,SCREEN_H),hg_range(0.8f,2.2f),hx(0x3a3a40),hx(0x6a6a74),1.2f,seed+500u+(uint32_t)i);
  if(SEASON==0) for(int i=0;i<160;i++){ float x=hg_range(0,SCREEN_W), y=hg_range(140,SCREEN_H); acc_blob(v,x,y,hg_range(2,7),hg_range(1,2.5f),hg_range(-0.2f,0.2f),hx(0xd8dcf0),0.4f,0.3f,0,0); }   /* congeres */
  /* le gouffre au loin */
  acc_blob(v,320,152,50,11,0,hx(0x241830),0.8f,0.1f,0,0);
  for(int k=0;k<34;k++){ float a=k*TAU/34; acc_rock(v,320+cosf(a)*47,152+sinf(a)*9.5f,hg_range(1.6f,2.8f),hx(0x2a2236),hx(0x5a4a6a),1.5f,seed+700u+(uint32_t)k); }
  acc_blob(v,320,152,42,8,0,hx(0x05030a),-3.0f,0,0,0);
  /* palissade */
  for(int x=-4;x<SCREEN_W;x+=9){ if(x>266&&x<374) continue;
    float cx=x+3.5f; float top=122+hg_range(-1,1);
    for(int yy=(int)(top*HS);yy<(int)(150*HS);yy++) for(int xx=(int)(x*HS);xx<(int)((x+7)*HS);xx++){ if(xx<0||xx>=v->w) continue;
      float gx=(xx+0.5f)/HS, gy=(yy+0.5f)/HS; float lx=(gx-cx)/3.5f; float pt=top+6*fabsf(lx); if(gy<pt) continue;
      int i=yy*v->w+xx; float bark=0.5f+0.5f*gnoise(gx*2.2f,gy*0.25f,0,seed+900+(uint32_t)x);
      C3 c=cmix(hx(0x3a2416),hx(0x7a5030),sat1(0.55f-0.45f*lx+0.15f*bark)); if(gy<pt+1.2f) c=cmix(c,hx(0xa07850),0.4f);
      if(fabsf(gy-134)<0.7f || fabsf(gy-145)<0.7f) c=cmix(c,hx(0x8a7a5a),0.75f);
      if(SEASON==0 && gy<pt+1.6f) c=hx(0xe8ecf8);   /* neige sur les pieux */
      cv_set(v,i,c); v->ht[i]=3.5f*sqrtf(sat1(1-lx*lx))+0.25f*bark; v->sp[i]=0.06f; v->em[i]=0; v->cd[i]=0; } }
  /* foyer du feu de camp : cendres, pierres, buches */
  acc_blob(v,320,228,19,6,0,hx(0x2a2420),-0.2f,0.05f,0,0);
  acc_blob(v,320,228,12,4,0,hx(0x3a3430),-0.1f,0.02f,0,0);
  for(int k=0;k<10;k++){ float a=k*TAU/10; acc_rock(v,320+cosf(a)*15,228+sinf(a)*4.6f,hg_range(1.8f,2.4f),hx(0x4a4652),hx(0x8a8494),2.0f,seed+800u+(uint32_t)k); }
  acc_line(v,309,229.5f,331,224.5f,2.4f,hx(0x5c3a24),1.6f,0.06f,HM_ON); acc_line(v,309,224.5f,331,229.5f,2.2f,hx(0x4a2e1c),2.0f,0.06f,HM_ON);
  for(int k=0;k<14;k++) acc_blob(v,320+hg_range(-6,6),227+hg_range(-2,2),0.5f,0.5f,0,hx(0xff7a22),0.3f,0,0,0);
}
