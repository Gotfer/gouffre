/* GOUFFRE - rendu logiciel : primitives, sprites, eclairage */
#include "game.h"

uint32_t g_screen[SCREEN_W*SCREEN_H];
Target g_scr;
Target *gT;
/* couches du rendu GPU */
bool g_gpu=false, g_gpu_ok=false;
uint32_t g_emis[SCREEN_W*SCREEN_H], g_ui[SCREEN_W*SCREEN_H], g_nrm[SCREEN_W*SCREEN_H]; uint8_t g_occ[SCREEN_W*SCREEN_H];
static Target t_emis, t_ui; static int cur_layer=LY_SCENE;
GLightRec g_lights[GPU_MAX_LIGHTS]; int g_nlights=0;
FxItem g_fx[GPU_MAX_FX]; int g_nfx=0;
GpuParams GP;

/* ---------------- aleatoire (PCG32) ---------------- */
static uint64_t rng_state = 0x853c49e6748fea9bULL;
void rng_seed(uint32_t s){ rng_state = 0x853c49e6748fea9bULL ^ ((uint64_t)s * 0x9E3779B97F4A7C15ULL); rng_next(); rng_next(); }
uint32_t rng_next(void){
  uint64_t old = rng_state;
  rng_state = old * 6364136223846793005ULL + 1442695040888963407ULL;
  uint32_t xs = (uint32_t)(((old >> 18u) ^ old) >> 27u);
  uint32_t rot = (uint32_t)(old >> 59u);
  return (xs >> rot) | (xs << ((32u - rot) & 31u));
}
float frand(void){ return (float)(rng_next() >> 8) * (1.0f/16777216.0f); }
float frange(float a,float b){ return a + (b-a)*frand(); }
int irand(int n){ return n <= 0 ? 0 : (int)(rng_next() % (uint32_t)n); }
int irange(int a,int b){ return a + irand(b-a+1); }
bool chance(float p){ return frand() < p; }
static uint32_t rr_state=0x9E3779B9u;
static inline uint32_t rr_next(void){ uint32_t x=rr_state; x^=x<<13; x^=x>>17; x^=x<<5; return rr_state=x; }
float rfrand(void){ return (float)(rr_next()>>8)*(1.0f/16777216.0f); }
float rfrange(float a,float b){ return a+(b-a)*rfrand(); }
int rirand(int n){ return n<=0?0:(int)(rr_next()%(uint32_t)n); }

uint32_t col_lerp(uint32_t a,uint32_t b,float t){
  t = clampf(t,0,1);
  return rgb((int)(cr(a)+(cr(b)-cr(a))*t),(int)(cg(a)+(cg(b)-cg(a))*t),(int)(cb(a)+(cb(b)-cb(a))*t));
}
uint32_t col_scale(uint32_t c,float f){ return rgb((int)(cr(c)*f),(int)(cg(c)*f),(int)(cb(c)*f)); }

/* ---------------- cible ---------------- */
void gfx_init(void){
  g_scr.px = g_screen; g_scr.w = SCREEN_W; g_scr.h = SCREEN_H;
  g_scr.cx0 = 0; g_scr.cy0 = 0; g_scr.cx1 = SCREEN_W; g_scr.cy1 = SCREEN_H; g_scr.alpha=0;
  t_emis=g_scr; t_emis.px=g_emis; t_ui=g_scr; t_ui.px=g_ui; t_ui.alpha=1;
  gT = &g_scr;
}
void layer(int ly){
  Target *n = !g_gpu ? &g_scr : (ly==LY_EMIS ? &t_emis : (ly==LY_UI ? &t_ui : &g_scr));
  if(n!=gT){ n->cx0=gT->cx0; n->cy0=gT->cy0; n->cx1=gT->cx1; n->cy1=gT->cy1; }
  gT=n; cur_layer=ly;
}
int layer_current(void){ return cur_layer; }
bool gfx_target_is_ui(void){ return g_gpu && gT==&t_ui; }
void gfx_target(Target *t){ gT = t; }
void gfx_clip(int x0,int y0,int x1,int y1){
  gT->cx0 = clampi(x0,0,gT->w); gT->cy0 = clampi(y0,0,gT->h);
  gT->cx1 = clampi(x1,0,gT->w); gT->cy1 = clampi(y1,0,gT->h);
}
void gfx_noclip(void){ gT->cx0=0; gT->cy0=0; gT->cx1=gT->w; gT->cy1=gT->h; }
void gfx_clear(uint32_t c){ int n = gT->w*gT->h; for(int i=0;i<n;i++) gT->px[i]=c; }

#define INCLIP(x,y) ((x)>=gT->cx0 && (y)>=gT->cy0 && (x)<gT->cx1 && (y)<gT->cy1)

/* melanges : cible opaque, ou cible a alpha premultiplie (interface du rendu GPU) */
static inline uint32_t mixc(uint32_t d,uint32_t s,int a){ /* a 0..256 */
  uint32_t rb = (((s&0xFF00FFu)*(uint32_t)a + (d&0xFF00FFu)*(uint32_t)(256-a))>>8)&0xFF00FFu;
  uint32_t g  = (((s&0x00FF00u)*(uint32_t)a + (d&0x00FF00u)*(uint32_t)(256-a))>>8)&0x00FF00u;
  if(gT->alpha){ uint32_t na=(255u*(uint32_t)a + (d>>24)*(uint32_t)(256-a))>>8; return (na<<24)|rb|g; }
  /* v8 : l'octet alpha de la scene porte le code d'un decor HD (1..15) ; un voile leger le conserve */
  return (((d>>24)-1u)<15u && a<170 ? (d&0xFF000000u) : 0xFF000000u)|rb|g;
}
static inline uint32_t addc(uint32_t d,uint32_t s,int a){
  int r = cr(d) + ((cr(s)*a)>>8), g = cg(d) + ((cg(s)*a)>>8), b = cb(d) + ((cb(s)*a)>>8);
  if(r>255) r=255; if(g>255) g=255; if(b>255) b=255;
  return (gT->alpha || ((d>>24)-1u)<15u ? (d&0xFF000000u) : 0xFF000000u)|((uint32_t)r<<16)|((uint32_t)g<<8)|(uint32_t)b;
}
static inline int a256(int a){ return a + (a>>7); }
static inline uint32_t mixc_raw(uint32_t d,uint32_t s,int a){
  uint32_t rb = (((s&0xFF00FFu)*(uint32_t)a + (d&0xFF00FFu)*(uint32_t)(256-a))>>8)&0xFF00FFu;
  uint32_t g  = (((s&0x00FF00u)*(uint32_t)a + (d&0x00FF00u)*(uint32_t)(256-a))>>8)&0x00FF00u;
  return 0xFF000000u|rb|g;
}

void gfx_pset(int x,int y,uint32_t c){ if(INCLIP(x,y)) gT->px[y*gT->w+x] = c|0xFF000000u; }
void gfx_pblend(int x,int y,uint32_t c,int a){
  if(a<=0 || !INCLIP(x,y)) return;
  uint32_t *p = &gT->px[y*gT->w+x];
  *p = a>=255 ? (c|0xFF000000u) : mixc(*p,c,a256(a));
}
void gfx_padd(int x,int y,uint32_t c,int a){
  if(a<=0 || !INCLIP(x,y)) return;
  uint32_t *p = &gT->px[y*gT->w+x];
  *p = addc(*p,c,a256(a>255?255:a));
}

static bool clip_rect(int *x,int *y,int *w,int *h){
  int x0=*x, y0=*y, x1=*x+*w, y1=*y+*h;
  if(x0<gT->cx0) x0=gT->cx0; if(y0<gT->cy0) y0=gT->cy0;
  if(x1>gT->cx1) x1=gT->cx1; if(y1>gT->cy1) y1=gT->cy1;
  if(x1<=x0 || y1<=y0) return false;
  *x=x0; *y=y0; *w=x1-x0; *h=y1-y0; return true;
}
void gfx_rect(int x,int y,int w,int h,uint32_t c){
  if(!clip_rect(&x,&y,&w,&h)) return;
  c |= 0xFF000000u;
  for(int j=0;j<h;j++){ uint32_t *p=&gT->px[(y+j)*gT->w+x]; for(int i=0;i<w;i++) p[i]=c; }
}
void gfx_rect_a(int x,int y,int w,int h,uint32_t c,int a){
  if(a<=0) return; if(a>=255){ gfx_rect(x,y,w,h,c); return; }
  if(!clip_rect(&x,&y,&w,&h)) return;
  int aa=a256(a);
  for(int j=0;j<h;j++){ uint32_t *p=&gT->px[(y+j)*gT->w+x]; for(int i=0;i<w;i++) p[i]=mixc(p[i],c,aa); }
}
void gfx_rect_add(int x,int y,int w,int h,uint32_t c,int a){
  if(a<=0 || !clip_rect(&x,&y,&w,&h)) return;
  int aa=a256(a>255?255:a);
  for(int j=0;j<h;j++){ uint32_t *p=&gT->px[(y+j)*gT->w+x]; for(int i=0;i<w;i++) p[i]=addc(p[i],c,aa); }
}
void gfx_hline(int x0,int x1,int y,uint32_t c){ if(x1<x0){int t=x0;x0=x1;x1=t;} gfx_rect(x0,y,x1-x0+1,1,c); }
void gfx_vline(int x,int y0,int y1,uint32_t c){ if(y1<y0){int t=y0;y0=y1;y1=t;} gfx_rect(x,y0,1,y1-y0+1,c); }
void gfx_frame(int x,int y,int w,int h,uint32_t c){
  gfx_rect(x,y,w,1,c); gfx_rect(x,y+h-1,w,1,c); gfx_rect(x,y,1,h,c); gfx_rect(x+w-1,y,1,h,c);
}
void gfx_line(int x0,int y0,int x1,int y1,uint32_t c){
  int dx=abs(x1-x0), sx=x0<x1?1:-1, dy=-abs(y1-y0), sy=y0<y1?1:-1, err=dx+dy;
  for(int guard=0;guard<4000;guard++){
    gfx_pset(x0,y0,c);
    if(x0==x1 && y0==y1) break;
    int e2=2*err;
    if(e2>=dy){ err+=dy; x0+=sx; }
    if(e2<=dx){ err+=dx; y0+=sy; }
  }
}
void gfx_line_a(int x0,int y0,int x1,int y1,uint32_t c,int a,bool additive){
  int dx=abs(x1-x0), sx=x0<x1?1:-1, dy=-abs(y1-y0), sy=y0<y1?1:-1, err=dx+dy;
  for(int guard=0;guard<4000;guard++){
    if(additive) gfx_padd(x0,y0,c,a); else gfx_pblend(x0,y0,c,a);
    if(x0==x1 && y0==y1) break;
    int e2=2*err;
    if(e2>=dy){ err+=dy; x0+=sx; }
    if(e2<=dx){ err+=dx; y0+=sy; }
  }
}
void gfx_thick_line(float x0,float y0,float x1,float y1,float r,uint32_t c){
  float dx=x1-x0, dy=y1-y0; float len=sqrtf(dx*dx+dy*dy); int n=(int)len+1;
  for(int i=0;i<=n;i++){ float t=(float)i/n; gfx_disc((int)floorf(x0+dx*t+0.5f),(int)floorf(y0+dy*t+0.5f),(int)r,c); }
}
static void disc_impl(int cx,int cy,int r,uint32_t c,int a,int mode){
  if(r<0) return;
  for(int dy=-r;dy<=r;dy++){
    int v = r*r + r - dy*dy; if(v<0) continue;
    int hw = (int)sqrtf((float)v);
    int y=cy+dy;
    if(mode==0) gfx_rect(cx-hw,y,2*hw+1,1,c);
    else if(mode==1) gfx_rect_a(cx-hw,y,2*hw+1,1,c,a);
    else gfx_rect_add(cx-hw,y,2*hw+1,1,c,a);
  }
}
void gfx_disc(int cx,int cy,int r,uint32_t c){ disc_impl(cx,cy,r,c,255,0); }
void gfx_disc_a(int cx,int cy,int r,uint32_t c,int a){ disc_impl(cx,cy,r,c,a,1); }
void gfx_disc_add(int cx,int cy,int r,uint32_t c,int a){ disc_impl(cx,cy,r,c,a,2); }
void gfx_ring_a(int cx,int cy,int r,uint32_t c,int a,bool additive){
  if(r<=0){ if(additive) gfx_padd(cx,cy,c,a); else gfx_pblend(cx,cy,c,a); return; }
  int ro = r*r + r, ri = (r-1)*(r-1) + (r-1);
  for(int dy=-r;dy<=r;dy++) for(int dx=-r;dx<=r;dx++){
    int d=dx*dx+dy*dy;
    if(d<=ro && d>ri){ if(additive) gfx_padd(cx+dx,cy+dy,c,a); else gfx_pblend(cx+dx,cy+dy,c,a); }
  }
}
void gfx_ring(int cx,int cy,int r,uint32_t c){ gfx_ring_a(cx,cy,r,c,255,false); }
void gfx_ellipse_a(int cx,int cy,int rx,int ry,uint32_t c,int a){
  if(rx<0||ry<0) return;
  for(int dy=-ry;dy<=ry;dy++){
    float fy=(float)dy/((float)ry+0.5f); float v=1.f-fy*fy; if(v<0) continue;
    int hw=(int)floorf((float)rx*sqrtf(v)+0.5f);
    if(a>=255) gfx_rect(cx-hw,cy+dy,2*hw+1,1,c); else gfx_rect_a(cx-hw,cy+dy,2*hw+1,1,c,a);
  }
}
void gfx_ellipse(int cx,int cy,int rx,int ry,uint32_t c){ gfx_ellipse_a(cx,cy,rx,ry,c,255); }
void gfx_glow(float cx,float cy,float r,uint32_t c,float inten){
  if(r<1 || inten<=0) return;
  int x0=(int)floorf(cx-r), x1=(int)ceilf(cx+r), y0=(int)floorf(cy-r), y1=(int)ceilf(cy+r);
  if(x0<gT->cx0) x0=gT->cx0; if(y0<gT->cy0) y0=gT->cy0; if(x1>gT->cx1-1) x1=gT->cx1-1; if(y1>gT->cy1-1) y1=gT->cy1-1;
  float ir2=1.f/(r*r);
  for(int y=y0;y<=y1;y++) for(int x=x0;x<=x1;x++){
    float dx=x+0.5f-cx, dy=y+0.5f-cy; float d=(dx*dx+dy*dy)*ir2; if(d>=1) continue;
    float f=(1-d); f*=f; int a=(int)(f*inten*255); if(a<=0) continue;
    uint32_t *p=&gT->px[y*gT->w+x]; *p=addc(*p,c,a>256?256:a);
  }
}
void gfx_darken(int x,int y,int w,int h,int a){ gfx_rect_a(x,y,w,h,HEX(0x000000),a); }
void gfx_gradient_v(int x,int y,int w,int h,uint32_t top,uint32_t bot){
  for(int j=0;j<h;j++) gfx_rect(x,y+j,w,1,col_lerp(top,bot,h>1?(float)j/(h-1):0));
}
static const uint8_t BAYER4[4][4]={{0,8,2,10},{12,4,14,6},{3,11,1,9},{15,7,13,5}};
void gfx_dither_rect(int x,int y,int w,int h,uint32_t c,int level){
  for(int j=0;j<h;j++) for(int i=0;i<w;i++){ int px=x+i, py=y+j; if(BAYER4[py&3][px&3]<level) gfx_pset(px,py,c); }
}
void gfx_tri(float x0,float y0,float x1,float y1,float x2,float y2,uint32_t c){
  int minx=(int)floorf(fminf(x0,fminf(x1,x2))), maxx=(int)ceilf(fmaxf(x0,fmaxf(x1,x2)));
  int miny=(int)floorf(fminf(y0,fminf(y1,y2))), maxy=(int)ceilf(fmaxf(y0,fmaxf(y1,y2)));
  float area=(x1-x0)*(y2-y0)-(y1-y0)*(x2-x0); if(fabsf(area)<0.01f) return;
  for(int y=miny;y<=maxy;y++) for(int x=minx;x<=maxx;x++){
    float px=x+0.5f, py=y+0.5f;
    float w0=(x1-px)*(y2-py)-(y1-py)*(x2-px);
    float w1=(x2-px)*(y0-py)-(y2-py)*(x0-px);
    float w2=(x0-px)*(y1-py)-(y0-py)*(x1-px);
    if(area<0){ w0=-w0; w1=-w1; w2=-w2; }
    if(w0>=0 && w1>=0 && w2>=0) gfx_pset(x,y,c);
  }
}

/* ---------------- sprites ---------------- */
Sprite spr_alloc(int w,int h){ Sprite s; s.w=w; s.h=h; s.px=(uint32_t*)calloc((size_t)w*h,4); s.nm=NULL; s.em=NULL; return s; }
Target spr_target(Sprite *s){ Target t; t.px=s->px; t.w=s->w; t.h=s->h; t.cx0=0; t.cy0=0; t.cx1=s->w; t.cy1=s->h; t.alpha=0; return t; }
void spr_clear(Sprite *s){ memset(s->px,0,(size_t)s->w*s->h*4); }
void spr_outline(Sprite *s,uint32_t col){
  int w=s->w, h=s->h; uint8_t *m=(uint8_t*)calloc((size_t)w*h,1);
  for(int y=0;y<h;y++) for(int x=0;x<w;x++){
    if(s->px[y*w+x]>>24) continue;
    bool n=false;
    if(x>0 && (s->px[y*w+x-1]>>24)) n=true;
    if(x<w-1 && (s->px[y*w+x+1]>>24)) n=true;
    if(y>0 && (s->px[(y-1)*w+x]>>24)) n=true;
    if(y<h-1 && (s->px[(y+1)*w+x]>>24)) n=true;
    if(n) m[y*w+x]=1;
  }
  for(int i=0;i<w*h;i++) if(m[i]) s->px[i]=col|0xFF000000u;
  free(m);
}

/* palette commune des sprites ASCII */
static uint32_t pal(char ch){
  switch(ch){
    case 'k': return HEX(0x1a1420); case 'K': return HEX(0x2e2638);
    case 'w': return HEX(0xf6f2ea); case 'W': return HEX(0xc9c4d0);
    case 'g': return HEX(0x8e8898); case 'G': return HEX(0x5a5466);
    case 'b': return HEX(0xeadfc4); case 'B': return HEX(0xb3a07e);
    case 'r': return HEX(0xe0453a); case 'R': return HEX(0x8f2233);
    case 'o': return HEX(0xf3962f); case 'O': return HEX(0xb35a22);
    case 'y': return HEX(0xf9dc5c); case 'Y': return HEX(0xc99a2e);
    case 'l': return HEX(0x9be05a); case 'L': return HEX(0x3fa34d);
    case 'E': return HEX(0x235a3e);
    case 'c': return HEX(0x63d6f2); case 'C': return HEX(0x3b7fdc);
    case 'n': return HEX(0x2a3b80);
    case 'p': return HEX(0xa35fe0); case 'P': return HEX(0x5d2d8c);
    case 'm': return HEX(0xe868c0); case 'M': return HEX(0x9a3a7e);
    case 's': return HEX(0xf2c09a); case 'S': return HEX(0xc98264);
    case 'h': return HEX(0x9a6438); case 'H': return HEX(0x5c3a24);
    case 'u': return HEX(0xdc8a48); case 'U': return HEX(0x8f4d28);
    case 'v': return HEX(0xd2b0f5);
    case 'x': return HEX(0xff3b3b); case 'z': return HEX(0xeaff6a);
    case 't': return HEX(0x45c2a8); case 'T': return HEX(0x1f6e6a);
    case 'f': return HEX(0xf59ac0); case 'F': return HEX(0xa8507f);
    case 'a': return HEX(0x7a7486); case 'A': return HEX(0x4a4554);
    case 'i': return HEX(0xd4f4ff);
    case 'j': return HEX(0xffd36a); case 'J': return HEX(0xff7a22);
    case 'q': return HEX(0xa8f07a); case 'Q': return HEX(0x4fa846);
    case 'd': return HEX(0x4b3122);
    case 'e': return HEX(0xffffff);
    default: return 0;
  }
}
#define OUTLINE_COL HEX(0x120e18)
uint32_t art_pal(char c){ return pal(c); }

/* sprite brut (marge de 1 px, sans contour) */
Sprite spr_art_raw(const char *const *rows,int nrows,bool mirror,const char *swap){
  int hw=(int)strlen(rows[0]);
  int w = mirror ? hw*2 : hw;
  Sprite s = spr_alloc(w+2, nrows+2);
  for(int y=0;y<nrows;y++){
    int len=(int)strlen(rows[y]);
    for(int x=0;x<hw && x<len;x++){
      char ch=rows[y][x];
      if(swap){ for(int k=0;swap[k] && swap[k+1];k+=2) if(swap[k]==ch){ ch=swap[k+1]; break; } }
      char och=rows[y][x];
      uint32_t c=pal(ch); if(!c) continue;
      s.px[(y+1)*s.w + (x+1)] = c;
      if(mirror) s.px[(y+1)*s.w + (w-x)] = c;
      /* yeux, flammes, feux follets : lumiere propre */
      if(strchr("xzjJi",ch) || och=='x'){
        if(!s.em) s.em=(uint32_t*)calloc((size_t)s.w*s.h,4);
        s.em[(y+1)*s.w + (x+1)] = c; if(mirror) s.em[(y+1)*s.w + (w-x)] = c;
      }
    }
  }
  return s;
}
Sprite spr_copy(const Sprite *a){
  Sprite s=spr_alloc(a->w,a->h); size_t n=(size_t)a->w*a->h*4; memcpy(s.px,a->px,n);
  if(a->nm){ s.nm=(uint32_t*)malloc(n); memcpy(s.nm,a->nm,n); }
  if(a->em){ s.em=(uint32_t*)malloc(n); memcpy(s.em,a->em,n); }
  return s;
}
/* pixels d'une couleur donnee -> lumiere propre (fenetres eclairees, foyers...) */
void spr_emis_colors(Sprite *s,const uint32_t *cols,int n,float k){
  for(int i=0;i<s->w*s->h;i++){
    uint32_t c=s->px[i]; if(!(c>>24)) continue;
    for(int j=0;j<n;j++) if(((c^cols[j])&0xFFFFFFu)==0){ if(!s->em) s->em=(uint32_t*)calloc((size_t)s->w*s->h,4); s->em[i]=col_scale(c,k); break; }
  }
}

/* normales a partir d'une carte de hauteur ; spec optionnelle (0..255) ; wrapy : raccord vertical */
void nrm_from_height(const float *h,int w,int hgt,uint32_t *out,float k,const uint8_t *spec,bool wrapy){
  for(int y=0;y<hgt;y++) for(int x=0;x<w;x++){
    int xl=x>0?x-1:x, xr=x<w-1?x+1:x;
    int yu=y>0?y-1:(wrapy?hgt-1:y), yd=y<hgt-1?y+1:(wrapy?0:y);
    float dx=(h[y*w+xr]-h[y*w+xl])*k, dy=(h[yd*w+x]-h[yu*w+x])*k;
    float nx=-dx, ny=-dy, nz=1.0f; float l=sqrtf(nx*nx+ny*ny+nz*nz); nx/=l; ny/=l; nz/=l;
    uint32_t sp = spec ? spec[y*w+x] : 16;
    out[y*w+x]=(sp<<24)|((uint32_t)clampi((int)(nx*127.5f+128),0,255)<<16)|((uint32_t)clampi((int)(ny*127.5f+128),0,255)<<8)|(uint32_t)clampi((int)(nz*127.5f+128),0,255);
  }
}
/* normales automatiques d'un sprite : bombe douce depuis les bords + relief de luminance */
void spr_make_normals(Sprite *s,float bump){
  int w=s->w, hh=s->h, n=w*hh;
  float *d=(float*)malloc((size_t)n*sizeof(float)); uint8_t *sp=(uint8_t*)malloc((size_t)n);
  for(int i=0;i<n;i++) d[i]=(s->px[i]>>24)?99.0f:0.0f;
  for(int pass=0;pass<2;pass++){
    for(int y=0;y<hh;y++) for(int x=0;x<w;x++){ int i=y*w+x; if(d[i]==0) continue;
      float m=d[i]; if(x>0) m=fminf(m,d[i-1]+1); if(y>0) m=fminf(m,d[i-w]+1); if(x>0&&y>0) m=fminf(m,d[i-w-1]+1.414f); if(x<w-1&&y>0) m=fminf(m,d[i-w+1]+1.414f); d[i]=m; }
    for(int y=hh-1;y>=0;y--) for(int x=w-1;x>=0;x--){ int i=y*w+x; if(d[i]==0) continue;
      float m=d[i]; if(x<w-1) m=fminf(m,d[i+1]+1); if(y<hh-1) m=fminf(m,d[i+w]+1); if(x<w-1&&y<hh-1) m=fminf(m,d[i+w+1]+1.414f); if(x>0&&y<hh-1) m=fminf(m,d[i+w-1]+1.414f); d[i]=m; }
  }
  float *h=(float*)malloc((size_t)n*sizeof(float));
  for(int i=0;i<n;i++){
    uint32_t c=s->px[i]; if(!(c>>24)){ h[i]=0; sp[i]=0; continue; }
    float e=fminf(d[i],3.5f)/3.5f; e=sqrtf(e);
    float lum=(cr(c)*0.3f+cg(c)*0.59f+cb(c)*0.11f)/255.0f;
    h[i]=e*1.6f+lum*0.45f*bump;
    int mx=maxi(cr(c),maxi(cg(c),cb(c))), mn=mini(cr(c),mini(cg(c),cb(c)));
    float sat=mx>0?(float)(mx-mn)/mx:0;
    sp[i]=(uint8_t)clampi((int)((clampf((lum-0.35f)*1.4f,0,1)*(1.0f-sat*0.85f)*0.85f+0.08f)*255),0,255);
  }
  if(!s->nm) s->nm=(uint32_t*)malloc((size_t)n*4);
  nrm_from_height(h,w,hh,s->nm,1.0f,sp,false);
  free(d); free(h); free(sp);
}

/* ombrage automatique : bords hauts eclaires (teinte chaude), bords bas assombris (teinte froide) */
static inline bool opq(const Sprite *s,int x,int y){ return x>=0 && y>=0 && x<s->w && y<s->h && (s->px[y*s->w+x]>>24); }
void spr_shade(Sprite *s,float amount){
  uint32_t *src=(uint32_t*)malloc((size_t)s->w*s->h*4); memcpy(src,s->px,(size_t)s->w*s->h*4);
  Sprite o={s->w,s->h,src};
  int top=s->h, bot=0; for(int y=0;y<s->h;y++) for(int x=0;x<s->w;x++) if(opq(&o,x,y)){ if(y<top) top=y; if(y>bot) bot=y; }
  float span = bot>top ? (float)(bot-top) : 1.0f;
  for(int y=0;y<s->h;y++) for(int x=0;x<s->w;x++){
    if(!opq(&o,x,y)) continue;
    float f=1.0f;
    if(!opq(&o,x,y-1)) f+=0.17f; else if(!opq(&o,x,y-2)) f+=0.05f;
    if(!opq(&o,x-1,y)) f+=0.06f;
    if(!opq(&o,x,y+1)) f-=0.20f; else if(!opq(&o,x,y+2)) f-=0.06f;
    if(!opq(&o,x+1,y)) f-=0.09f;
    f *= 1.05f - 0.13f*(y-top)/span;
    f = 1.0f + (f-1.0f)*amount;
    uint32_t c=src[y*s->w+x];
    if(f>=1.0f) c=col_lerp(c,HEX(0xfff2d6),clampf((f-1.0f)*1.25f,0,1));
    else c=col_lerp(col_scale(c,0.55f+0.45f*f),HEX(0x1c1430),clampf((1.0f-f)*0.55f,0,1));
    s->px[y*s->w+x]=c;
  }
  free(src);
}
/* contour sombre teinte par la couleur voisine */
void spr_outline_tinted(Sprite *s){
  int w=s->w, h=s->h; uint32_t *out=(uint32_t*)calloc((size_t)w*h,4);
  for(int y=0;y<h;y++) for(int x=0;x<w;x++){
    if(s->px[y*w+x]>>24) continue;
    int n=0, r=0,g=0,b=0;
    static const int DX[4]={-1,1,0,0}, DY[4]={0,0,-1,1};
    for(int k=0;k<4;k++){ int xx=x+DX[k], yy=y+DY[k]; if(xx<0||yy<0||xx>=w||yy>=h) continue; uint32_t c=s->px[yy*w+xx]; if(!(c>>24)) continue; r+=cr(c); g+=cg(c); b+=cb(c); n++; }
    if(!n) continue;
    r/=n; g/=n; b/=n;
    out[y*w+x]=rgb(0x12+(int)((r*0.33f-0x12)*0.38f),14+(int)((g*0.33f-14)*0.38f),0x18+(int)((b*0.33f-0x18)*0.38f));
  }
  for(int i=0;i<w*h;i++) if(out[i]) s->px[i]=out[i];
  free(out);
}
/* image de marche : un pied leve (side 0 = gauche, 1 = droite) */
void spr_lift_foot(Sprite *s,int side){
  int yb=-1; for(int y=s->h-1;y>=0 && yb<0;y--) for(int x=0;x<s->w;x++) if(s->px[y*s->w+x]>>24){ yb=y; break; }
  if(yb<1) return;
  int mid=s->w/2;
  for(int x=0;x<s->w;x++){
    if(side==0 ? x>=mid : x<mid) continue;
    uint32_t c=s->px[yb*s->w+x]; if(!(c>>24)) continue;
    s->px[(yb-1)*s->w+x]=c; s->px[yb*s->w+x]=0;
    if(s->em){ s->em[(yb-1)*s->w+x]=s->em[yb*s->w+x]; s->em[yb*s->w+x]=0; }
  }
}

Sprite spr_from_art(const char *const *rows,int nrows,bool mirror,const char *swap){
  Sprite s=spr_art_raw(rows,nrows,mirror,swap);
  spr_shade(&s,1.0f);
  spr_outline_tinted(&s);
  return s;
}

DrawOpt dopt(void){ DrawOpt o; o.flags=0; o.sx=1; o.sy=1; o.tint=0xFFFFFFFFu; o.alpha=255; o.flash=0xFFFFFFFFu; o.flash_a=0; o.lift=0; return o; }

/* normale d'un pixel de sprite, avec retournements */
static inline uint32_t nrm_flip(uint32_t n,int flags){
  if(flags&DS_FLIPX) n=(n&0xFF00FFFFu)|((255u-((n>>16)&255u))<<16);
  if(flags&DS_FLIPY) n=(n&0xFFFF00FFu)|((255u-((n>>8)&255u))<<8);
  return n;
}
void draw_spr(const Sprite *s,int x,int y,int flags){
  bool wn = g_gpu && gT==&g_scr && s->nm, we = g_gpu && gT==&g_scr && s->em, wo = g_gpu && gT==&g_scr && !(flags&DS_NOOCC);
  int foot=y+s->h-1;
  for(int j=0;j<s->h;j++){
    int py=y+j; if(py<gT->cy0 || py>=gT->cy1) continue;
    uint8_t oh=(uint8_t)clampi(foot-py+1,1,255);
    int sy=(flags&DS_FLIPY)? s->h-1-j : j;
    for(int i=0;i<s->w;i++){
      int px=x+i; if(px<gT->cx0 || px>=gT->cx1) continue;
      int sx=(flags&DS_FLIPX)? s->w-1-i : i;
      uint32_t c=s->px[sy*s->w+sx]; if(!(c>>24)) continue;
      gT->px[py*gT->w+px]=c;
      if(wn) g_nrm[py*SCREEN_W+px]=nrm_flip(s->nm[sy*s->w+sx],flags);
      if(we){ uint32_t e=s->em[sy*s->w+sx]; if(e) g_emis[py*SCREEN_W+px]=e; }
      if(wo) g_occ[py*SCREEN_W+px]=oh;
    }
  }
}

void draw_spr_c(const Sprite *s,float cx,float cy,const DrawOpt *o){
  int dw=(int)(s->w*o->sx+0.5f), dh=(int)(s->h*o->sy+0.5f);
  if(dw<=0 || dh<=0 || o->alpha<=0) return;
  int x0=(int)floorf(cx+0.5f) - dw/2, y0=(int)floorf(cy+0.5f) - dh/2;
  bool tint = o->tint != 0xFFFFFFFFu;
  int tr=cr(o->tint), tg=cg(o->tint), tb=cb(o->tint);
  int fa = o->flash_a>0 ? a256(o->flash_a>255?255:o->flash_a) : 0;
  int al = a256(o->alpha>255?255:o->alpha);
  bool wn = g_gpu && gT==&g_scr && s->nm && !(o->flags&DS_ADD) && al>=128;
  bool we = g_gpu && gT==&g_scr && s->em && !(o->flags&DS_ADD);
  bool wo = g_gpu && gT==&g_scr && !(o->flags&(DS_ADD|DS_NOOCC)) && al>=128;
  int foot=y0+dh-1;
  for(int j=0;j<dh;j++){
    int py=y0+j; if(py<gT->cy0 || py>=gT->cy1) continue;
    uint8_t oh=(uint8_t)clampi((int)(foot-py+o->lift)+1,1,255);
    int sy=(int)((j+0.5f)*s->h/dh); if(sy>=s->h) sy=s->h-1; if(o->flags&DS_FLIPY) sy=s->h-1-sy;
    uint32_t *row=&gT->px[py*gT->w];
    for(int i=0;i<dw;i++){
      int px=x0+i; if(px<gT->cx0 || px>=gT->cx1) continue;
      int sx=(int)((i+0.5f)*s->w/dw); if(sx>=s->w) sx=s->w-1; if(o->flags&DS_FLIPX) sx=s->w-1-sx;
      uint32_t c=s->px[sy*s->w+sx]; if(!(c>>24)) continue;
      if(tint) c=0xFF000000u|((uint32_t)(cr(c)*tr/255)<<16)|((uint32_t)(cg(c)*tg/255)<<8)|(uint32_t)(cb(c)*tb/255);
      if(fa) c=mixc(c,o->flash,fa);
      if(o->flags&DS_ADD) row[px]=addc(row[px],c,al);
      else if(al<256) row[px]=mixc(row[px],c,al);
      else row[px]=c;
      if(wn) g_nrm[py*SCREEN_W+px]=nrm_flip(s->nm[sy*s->w+sx],o->flags);
      if(we){ uint32_t e=s->em[sy*s->w+sx]; if(e){ uint32_t *d=&g_emis[py*SCREEN_W+px]; *d = al>=256 ? e : mixc_raw(*d,e,al); } }
      if(wo) g_occ[py*SCREEN_W+px]=oh;
    }
  }
}

void draw_spr_scaled(const Sprite *s,int x,int y,int scale,uint32_t tint){
  bool t = tint!=0xFFFFFFFFu;
  for(int j=0;j<s->h;j++) for(int i=0;i<s->w;i++){
    uint32_t c=s->px[j*s->w+i]; if(!(c>>24)) continue;
    if(t) c=0xFF000000u|((uint32_t)(cr(c)*cr(tint)/255)<<16)|((uint32_t)(cg(c)*cg(tint)/255)<<8)|(uint32_t)(cb(c)*cb(tint)/255);
    gfx_rect(x+i*scale,y+j*scale,scale,scale,c);
  }
}

/* ---------------- eclairage ---------------- */
static uint16_t LR[SCREEN_W*SCREEN_H], LG[SCREEN_W*SCREEN_H], LB[SCREEN_W*SCREEN_H];
static int Lx0,Ly0,Lx1,Ly1;
static float Lgain=1.0f;   /* gain des lumieres (rendu logiciel), remis a 1 par light_begin */
void light_gain(float g){ Lgain=g; }
void light_begin(int x0,int y0,int x1,int y1,int ar,int ag,int ab){
  Lx0=clampi(x0,0,SCREEN_W); Ly0=clampi(y0,0,SCREEN_H); Lx1=clampi(x1,0,SCREEN_W); Ly1=clampi(y1,0,SCREEN_H);
  if(g_gpu){ GP.lit_on=1; GP.lit[0]=(float)Lx0; GP.lit[1]=(float)Ly0; GP.lit[2]=(float)Lx1; GP.lit[3]=(float)Ly1; GP.amb[0]=ar/256.0f; GP.amb[1]=ag/256.0f; GP.amb[2]=ab/256.0f; g_nlights=0; return; }
  Lgain=1.0f;
  for(int y=Ly0;y<Ly1;y++) for(int x=Lx0;x<Lx1;x++){ int i=y*SCREEN_W+x; LR[i]=(uint16_t)ar; LG[i]=(uint16_t)ag; LB[i]=(uint16_t)ab; }
}
static uint16_t FALL[1025]; static bool fall_ok=false;
void light_add(float x,float y,float r,uint32_t c,float inten){
  if(r<2 || inten<=0) return;
  if(g_gpu){
    if(g_nlights>=GPU_MAX_LIGHTS || x+r<Lx0 || x-r>Lx1 || y+r<Ly0 || y-r>Ly1) return;
    GLightRec *l=&g_lights[g_nlights++]; l->x=x; l->y=y; l->r=r*GP.light_range; l->h=clampf(r*0.32f,9,34);
    float k=inten*GP.light_gain;
    l->cr=cr(c)/255.0f*k; l->cg=cg(c)/255.0f*k; l->cb=cb(c)/255.0f*k; l->namt=GP.normal_amt;
    l->sh = r>=36 ? GP.shadows : 0;
    return;
  }
  if(!fall_ok){ for(int i=0;i<=1024;i++){ float f=1-i/1024.0f; FALL[i]=(uint16_t)(f*f*4096); } fall_ok=true; }
  int x0=(int)(x-r), x1=(int)(x+r)+1, y0=(int)(y-r), y1=(int)(y+r)+1;
  if(x0<Lx0) x0=Lx0; if(y0<Ly0) y0=Ly0; if(x1>Lx1) x1=Lx1; if(y1>Ly1) y1=Ly1;
  if(x1<=x0||y1<=y0) return;
  /* distances en 1/16 de pixel ; d2 normalise sur 1024 */
  int cx16=(int)(x*16), cy16=(int)(y*16); int r16=(int)(r*16); long long r2=(long long)r16*r16;
  int inv=(int)((1024LL<<20)/r2);
  inten*=Lgain;
  int kr=(int)(cr(c)*inten), kg=(int)(cg(c)*inten), kb=(int)(cb(c)*inten);
  for(int py=y0;py<y1;py++){
    int dy=py*16+8-cy16; int dy2=dy*dy; if(dy2>=r2) continue;
    uint16_t *pr=&LR[py*SCREEN_W], *pg=&LG[py*SCREEN_W], *pb=&LB[py*SCREEN_W];
    for(int px=x0;px<x1;px++){
      int dx=px*16+8-cx16; int d2=dx*dx+dy2; if(d2>=r2) continue;
      int f=FALL[(int)(((long long)d2*inv)>>20)];
      int vr=pr[px]+((kr*f)>>12), vg=pg[px]+((kg*f)>>12), vb=pb[px]+((kb*f)>>12);
      pr[px]=(uint16_t)(vr>900?900:vr); pg[px]=(uint16_t)(vg>900?900:vg); pb[px]=(uint16_t)(vb>900?900:vb);
    }
  }
}
void light_apply(void){
  if(g_gpu) return;
  for(int y=Ly0;y<Ly1;y++){
    for(int x=Lx0;x<Lx1;x++){
      int i=y*SCREEN_W+x; int d=BAYER4[y&3][x&3];
      int lr=((LR[i]+d)>>4)<<4, lg=((LG[i]+d)>>4)<<4, lb=((LB[i]+d)>>4)<<4;
      uint32_t c=g_screen[i];
      int r=(cr(c)*lr)>>8, g=(cg(c)*lg)>>8, b=(cb(c)*lb)>>8;
      if(r>255) r=255; if(g>255) g=255; if(b>255) b=255;
      g_screen[i]=0xFF000000u|((uint32_t)r<<16)|((uint32_t)g<<8)|(uint32_t)b;
    }
  }
}

/* ---------------- primitives additionnelles ---------------- */
uint16_t *light_chan(int ch){ return ch==0?LR:(ch==1?LG:LB); }
void light_floor(int x,int y,int v){
  if(g_gpu || x<Lx0||y<Ly0||x>=Lx1||y>=Ly1) return; int i=y*SCREEN_W+x;
  if(LR[i]<v) LR[i]=(uint16_t)v; if(LG[i]<v) LG[i]=(uint16_t)v; if(LB[i]<v) LB[i]=(uint16_t)v;
}
/* segment epais additif (rubans, faisceaux) */
void gfx_capsule_add(float x0,float y0,float x1,float y1,float r,uint32_t c,int a){
  if(a<=0 || r<=0) return;
  int bx0=(int)floorf(fminf(x0,x1)-r-1), bx1=(int)ceilf(fmaxf(x0,x1)+r+1);
  int by0=(int)floorf(fminf(y0,y1)-r-1), by1=(int)ceilf(fmaxf(y0,y1)+r+1);
  if(bx0<gT->cx0) bx0=gT->cx0; if(by0<gT->cy0) by0=gT->cy0; if(bx1>gT->cx1-1) bx1=gT->cx1-1; if(by1>gT->cy1-1) by1=gT->cy1-1;
  float dx=x1-x0, dy=y1-y0, l2=dx*dx+dy*dy; float ir=1.0f/(r+0.5f);
  for(int y=by0;y<=by1;y++) for(int x=bx0;x<=bx1;x++){
    float px=x+0.5f-x0, py=y+0.5f-y0;
    float t = l2>1e-4f ? clampf((px*dx+py*dy)/l2,0,1) : 0;
    float ex=px-dx*t, ey=py-dy*t; float d=sqrtf(ex*ex+ey*ey);
    if(d>r+0.5f) continue;
    float f=1.0f-d*ir; int aa=(int)(a*(0.35f+0.65f*f));
    uint32_t *p=&gT->px[y*gT->w+x]; *p=addc(*p,c,aa>256?256:aa);
  }
}
/* triangle additif */
void gfx_tri_add(float x0,float y0,float x1,float y1,float x2,float y2,uint32_t c,int a){
  int minx=(int)floorf(fminf(x0,fminf(x1,x2))), maxx=(int)ceilf(fmaxf(x0,fmaxf(x1,x2)));
  int miny=(int)floorf(fminf(y0,fminf(y1,y2))), maxy=(int)ceilf(fmaxf(y0,fmaxf(y1,y2)));
  if(minx<gT->cx0) minx=gT->cx0; if(miny<gT->cy0) miny=gT->cy0; if(maxx>gT->cx1-1) maxx=gT->cx1-1; if(maxy>gT->cy1-1) maxy=gT->cy1-1;
  float area=(x1-x0)*(y2-y0)-(y1-y0)*(x2-x0); if(fabsf(area)<0.01f) return;
  int aa=a256(a>255?255:a);
  for(int y=miny;y<=maxy;y++) for(int x=minx;x<=maxx;x++){
    float px=x+0.5f, py=y+0.5f;
    float w0=(x1-px)*(y2-py)-(y1-py)*(x2-px), w1=(x2-px)*(y0-py)-(y2-py)*(x0-px), w2=(x0-px)*(y1-py)-(y0-py)*(x1-px);
    if(area<0){ w0=-w0; w1=-w1; w2=-w2; }
    if(w0>=0 && w1>=0 && w2>=0){ uint32_t *p=&gT->px[y*gT->w+x]; *p=addc(*p,c,aa); }
  }
}
/* bruit de valeur (generation de textures) */
static float hash2(int x,int y,uint32_t seed){ uint32_t h=(uint32_t)x*374761393u+(uint32_t)y*668265263u+seed*2246822519u; h=(h^(h>>13))*1274126177u; h^=h>>16; return (h&0xFFFF)/65535.0f; }
float vnoise(float x,float y,uint32_t seed){
  int ix=(int)floorf(x), iy=(int)floorf(y); float fx=x-ix, fy=y-iy;
  fx=fx*fx*(3-2*fx); fy=fy*fy*(3-2*fy);
  float a=hash2(ix,iy,seed), b=hash2(ix+1,iy,seed), c=hash2(ix,iy+1,seed), d=hash2(ix+1,iy+1,seed);
  return lerpf(lerpf(a,b,fx),lerpf(c,d,fx),fy);
}
float fbm(float x,float y,uint32_t seed){ return vnoise(x,y,seed)*0.55f+vnoise(x*2.1f,y*2.1f,seed+17)*0.3f+vnoise(x*4.3f,y*4.3f,seed+41)*0.15f; }

/* ---------------- post-traitement ---------------- */
#define BW (SCREEN_W/2)
#define BH (SCREEN_H/2)
static int32_t HR[BW*BH], HG[BW*BH], HB[BW*BH];
static int32_t QR[(BW/2+1)*(BH/2+1)], QG[(BW/2+1)*(BH/2+1)], QB[(BW/2+1)*(BH/2+1)];
static int32_t TMP[BW*BH];
static void blur1(int32_t *a,int w,int h,int r){
  int rc=65536/(2*r+1);
  for(int y=0;y<h;y++){
    int32_t *row=&a[y*w], *out=&TMP[y*w]; int s=0;
    for(int k=-r;k<=r;k++) s+=row[clampi(k,0,w-1)];
    for(int x=0;x<w;x++){ out[x]=(s*rc)>>16; int xa=x+r+1, xb=x-r; s+=row[xa<w?xa:w-1]-row[xb>0?xb:0]; }
  }
  for(int x=0;x<w;x++){
    int s=0;
    for(int k=-r;k<=r;k++) s+=TMP[clampi(k,0,h-1)*w+x];
    for(int y=0;y<h;y++){ a[y*w+x]=(s*rc)>>16; int ya=y+r+1, yb=y-r; s+=TMP[(ya<h?ya:h-1)*w+x]-TMP[(yb>0?yb:0)*w+x]; }
  }
}
/* echantillonnage bilineaire d'un tampon w*h (coordonnees en 1/256 de cellule) */
static inline int bil(const int32_t *a,int w,int h,int fx,int fy){
  int x0=fx>>8, y0=fy>>8; int tx=fx&255, ty=fy&255;
  int x1=x0+1<w?x0+1:w-1, y1=y0+1<h?y0+1:h-1; if(x0<0){x0=0;tx=0;} if(y0<0){y0=0;ty=0;}
  int v0=(a[y0*w+x0]*(256-tx)+a[y0*w+x1]*tx)>>8, v1=(a[y1*w+x0]*(256-tx)+a[y1*w+x1]*tx)>>8;
  return (v0*(256-ty)+v1*ty)>>8;
}
void post_bloom(int x0,int y0,int x1,int y1,int thresh,float strength,float wide){
  x0=clampi(x0,0,SCREEN_W); x1=clampi(x1,0,SCREEN_W); y0=clampi(y0,0,SCREEN_H); y1=clampi(y1,0,SCREEN_H);
  int w=(x1-x0)/2, h=(y1-y0)/2; if(w<4||h<4) return;
  int inv=thresh<255 ? 65536/(255-thresh) : 0;
  for(int j=0;j<h;j++){
    const uint32_t *r0=&g_screen[(y0+2*j)*SCREEN_W+x0], *r1=r0+SCREEN_W;
    for(int i=0;i<w;i++){
      uint32_t a=r0[2*i], b=r0[2*i+1], c=r1[2*i], d=r1[2*i+1];
      int R=(cr(a)+cr(b)+cr(c)+cr(d))>>2, G=(cg(a)+cg(b)+cg(c)+cg(d))>>2, B=(cb(a)+cb(b)+cb(c)+cb(d))>>2;
      int m=R>G?R:G; if(B>m) m=B;
      int k = m>thresh ? ((m-thresh)*inv)>>8 : 0;   /* 0..256 */
      if(k>256) k=256;
      HR[j*w+i]=(R*k)>>8; HG[j*w+i]=(G*k)>>8; HB[j*w+i]=(B*k)>>8;
    }
  }
  int qw=w/2, qh=h/2;
  for(int j=0;j<qh;j++) for(int i=0;i<qw;i++){
    int a=(2*j)*w+2*i;
    QR[j*qw+i]=(HR[a]+HR[a+1]+HR[a+w]+HR[a+w+1])>>2; QG[j*qw+i]=(HG[a]+HG[a+1]+HG[a+w]+HG[a+w+1])>>2; QB[j*qw+i]=(HB[a]+HB[a+1]+HB[a+w]+HB[a+w+1])>>2;
  }
  blur1(HR,w,h,1); blur1(HG,w,h,1); blur1(HB,w,h,1);
  blur1(HR,w,h,2); blur1(HG,w,h,2); blur1(HB,w,h,2);
  blur1(QR,qw,qh,2); blur1(QG,qw,qh,2); blur1(QB,qw,qh,2);
  blur1(QR,qw,qh,2); blur1(QG,qw,qh,2); blur1(QB,qw,qh,2);
  int wq=(int)(wide*256);
  for(int j=0;j<h;j++){ int fy=j*128-64; for(int i=0;i<w;i++){ int fx=i*128-64;
    HR[j*w+i]+=(bil(QR,qw,qh,fx,fy)*wq)>>8; HG[j*w+i]+=(bil(QG,qw,qh,fx,fy)*wq)>>8; HB[j*w+i]+=(bil(QB,qw,qh,fx,fy)*wq)>>8; } }
  int st=(int)(strength*256);
  /* agrandissement x2 bilineaire a poids fixes (9-3-3-1) */
  for(int y=y0;y<y0+2*h;y++){
    int j=(y-y0)>>1, jy=((y-y0)&1)? (j+1<h?j+1:j) : (j>0?j-1:0);
    const int32_t *r0r=&HR[j*w], *r0g=&HG[j*w], *r0b=&HB[j*w], *r1r=&HR[jy*w], *r1g=&HG[jy*w], *r1b=&HB[jy*w];
    uint32_t *row=&g_screen[y*SCREEN_W+x0];
    for(int xx=0;xx<2*w;xx++){
      int i=xx>>1, ix=(xx&1)? (i+1<w?i+1:i) : (i>0?i-1:0);
      int vr=9*r0r[i]+3*r0r[ix]+3*r1r[i]+r1r[ix], vg=9*r0g[i]+3*r0g[ix]+3*r1g[i]+r1g[ix], vb=9*r0b[i]+3*r0b[ix]+3*r1b[i]+r1b[ix];
      if((vr|vg|vb)<16) continue;
      int br=(vr*st)>>12, bg=(vg*st)>>12, bb=(vb*st)>>12;
      uint32_t c=row[xx]; int r=cr(c)+br, g=cg(c)+bg, b=cb(c)+bb;
      if(r>255) r=255; if(g>255) g=255; if(b>255) b=255;
      row[xx]=0xFF000000u|((uint32_t)r<<16)|((uint32_t)g<<8)|(uint32_t)b;
    }
  }
}
/* etalonnage : saturation, contraste, teinte des ombres, vignette */
void post_grade(int x0,int y0,int x1,int y1,float sat,float contrast,uint32_t shadow,float shadow_amt,float vig){
  x0=clampi(x0,0,SCREEN_W); x1=clampi(x1,0,SCREEN_W); y0=clampi(y0,0,SCREEN_H); y1=clampi(y1,0,SCREEN_H);
  static uint8_t lut[256]; static int colv[SCREEN_W];
  for(int i=0;i<256;i++){ float v=i/255.0f; v=0.5f+(v-0.5f)*contrast; lut[i]=(uint8_t)clampi((int)(v*255+0.5f),0,255); }
  float cx=(x0+x1)*0.5f, cy=(y0+y1)*0.5f, hw=(x1-x0)*0.5f, hh=(y1-y0)*0.5f;
  for(int x=x0;x<x1;x++){ float d=(x+0.5f-cx)/hw; colv[x]=(int)(vig*d*d*0.55f*256); }
  int s256=(int)(sat*256), sa=(int)(shadow_amt*256);
  int sr=cr(shadow), sg=cg(shadow), sbb=cb(shadow);
  for(int y=y0;y<y1;y++){
    float d=(y+0.5f-cy)/hh; int rv=(int)(vig*d*d*0.75f*256);
    uint32_t *row=&g_screen[y*SCREEN_W];
    for(int x=x0;x<x1;x++){
      uint32_t c=row[x]; int r=lut[cr(c)], g=lut[cg(c)], b=lut[cb(c)];
      int l=(r*77+g*150+b*29)>>8;
      r=l+(((r-l)*s256)>>8); g=l+(((g-l)*s256)>>8); b=l+(((b-l)*s256)>>8);
      int dk=((255-l)*sa)>>8; r+=(sr*dk)>>8; g+=(sg*dk)>>8; b+=(sbb*dk)>>8;
      int v=256-colv[x]-rv; if(v<0) v=0;
      r=(r*v)>>8; g=(g*v)>>8; b=(b*v)>>8;
      if(r<0) r=0; if(g<0) g=0; if(b<0) b=0; if(r>255) r=255; if(g>255) g=255; if(b>255) b=255;
      row[x]=0xFF000000u|((uint32_t)r<<16)|((uint32_t)g<<8)|(uint32_t)b;
    }
  }
}
/* onde de choc : deformation radiale d'un anneau */
static uint32_t SHK[SCREEN_W*SCREEN_H];
void post_shockwave(float cx,float cy,float rad,float thick,float str,int cx0,int cy0,int cx1,int cy1){
  if(rad<1 || str<=0.05f) return;
  int x0=(int)(cx-rad-thick-2), x1=(int)(cx+rad+thick+2), y0=(int)(cy-rad-thick-2), y1=(int)(cy+rad+thick+2);
  if(x0<cx0) x0=cx0; if(y0<cy0) y0=cy0; if(x1>cx1) x1=cx1; if(y1>cy1) y1=cy1;
  if(x1<=x0||y1<=y0) return;
  for(int y=y0;y<y1;y++) memcpy(&SHK[y*SCREEN_W+x0],&g_screen[y*SCREEN_W+x0],(size_t)(x1-x0)*4);
  for(int y=y0;y<y1;y++) for(int x=x0;x<x1;x++){
    float dx=x+0.5f-cx, dy=y+0.5f-cy; float d=sqrtf(dx*dx+dy*dy); float e=fabsf(d-rad);
    if(e>=thick || d<0.5f) continue;
    float k=1.0f-e/thick; k=k*k*str; float off=k*(d<rad?-1.0f:1.0f);
    int sx=(int)(x-dx/d*off*3.0f), sy=(int)(y-dy/d*off*3.0f);
    sx=clampi(sx,x0,x1-1); sy=clampi(sy,y0,y1-1);
    uint32_t c=SHK[sy*SCREEN_W+sx]; int add=(int)(k*40);
    g_screen[y*SCREEN_W+x]=rgb(cr(c)+add,cg(c)+add,cb(c)+add);
  }
}
/* air brulant : ondulation horizontale des lignes */
void post_haze(int x0,int y0,int x1,int y1,float t,float amp){
  static uint32_t line[SCREEN_W];
  for(int y=y0;y<y1;y++){
    float o=sinf(y*0.21f+t*3.3f)*sinf(y*0.045f-t*0.9f)*amp; int off=(int)roundf(o);
    if(!off) continue;
    uint32_t *row=&g_screen[y*SCREEN_W]; memcpy(line+x0,row+x0,(size_t)(x1-x0)*4);
    for(int x=x0;x<x1;x++){ int sx=clampi(x+off,x0,x1-1); row[x]=line[sx]; }
  }
}

/* ---------------- vue 2.5D du puits ----------------
   Une camera inclinee : chaque ligne y du jeu est agrandie de s(y) autour de l'axe du puits
   (1-K tout en haut, 1+K tout en bas) et les lignes se resserrent ou s'ecartent d'autant
   (Y = integrale de s), si bien que l'image remplit toujours l'ecran. Les decors des cotes
   s'etirent du bord de l'ecran jusqu'au bord du puits. */
float g_p25=0;
#define P25_CX ((PIT_X0+PIT_X1)*0.5f)
#define P25_HW ((PIT_X1-PIT_X0)*0.5f)
float p25_scale(float y){ float K=P25_K*g_p25; return 1-K+2*K*y/SCREEN_H; }
/* une ligne : agrandissement, bords du puits, largeur apparente des parois */
typedef struct { float s, xl, xr, F; } P25Row;
static inline void p25_rowp(float K,float y,P25Row *r){
  float str=K/P25_K;
  r->s=1-K+2*K*y/SCREEN_H; r->xl=P25_CX-P25_HW*r->s; r->xr=P25_CX+P25_HW*r->s;
  r->F=SC_FW*(1+(P25_FWA+P25_FWB*y/SCREEN_H-1)*str);
}
#define P25_A0 ((float)(PIT_X0-SC_FW))
#define P25_A1 ((float)(PIT_X1+SC_FW))
static inline float p25_xf(const P25Row *r,float x){
  if(x<P25_A0) return x*(r->xl-r->F)/P25_A0;
  if(x<PIT_X0) return r->xl-r->F+(x-P25_A0)*r->F/SC_FW;
  if(x<=PIT_X1) return P25_CX+(x-P25_CX)*r->s;
  if(x<P25_A1) return r->xr+(x-PIT_X1)*r->F/SC_FW;
  return r->xr+r->F+(x-P25_A1)*(SCREEN_W-(r->xr+r->F))/(SCREEN_W-P25_A1);
}
static inline float p25_xi(const P25Row *r,float X){
  if(X<r->xl-r->F) return X*P25_A0/(r->xl-r->F);
  if(X<r->xl) return P25_A0+(X-(r->xl-r->F))*SC_FW/r->F;
  if(X<=r->xr) return P25_CX+(X-P25_CX)/r->s;
  if(X<r->xr+r->F) return PIT_X1+(X-r->xr)*SC_FW/r->F;
  return P25_A1+(X-(r->xr+r->F))*(SCREEN_W-P25_A1)/(SCREEN_W-(r->xr+r->F));
}
void p25_fwd(float x,float y,float *X,float *Y){
  float K=P25_K*g_p25; if(K<=0){ *X=x; *Y=y; return; }
  P25Row r; p25_rowp(K,y,&r);
  *Y=(1-K)*y+K*y*y/SCREEN_H;
  *X=p25_xf(&r,x);
}
static inline float p25_row(float K,float Y){ float a=K/SCREEN_H, b=1-K, d=b*b+4*a*Y; if(d<0) d=0; return (-b+sqrtf(d))/(2*a); }
void p25_inv(float X,float Y,float *x,float *y){
  float K=P25_K*g_p25; if(K<=0){ *x=X; *y=Y; return; }
  float yy=p25_row(K,Y); P25Row r; p25_rowp(K,yy,&r);
  *x=p25_xi(&r,X); *y=yy;
}
/* rendu logiciel : on reprojette l'image du puits deja dessinee (avant l'interface) */
void p25_warp_cpu(void){
  float K=P25_K*g_p25; if(K<=0) return;
  static uint32_t src[SCREEN_W*SCREEN_H];
  memcpy(src,g_screen,sizeof src);
  for(int Y=0;Y<SCREEN_H;Y++){
    float yy=p25_row(K,Y+0.5f); int sy=clampi((int)yy,0,SCREEN_H-1);
    P25Row r; p25_rowp(K,yy,&r);
    const uint32_t *row=&src[sy*SCREEN_W]; uint32_t *dst=&g_screen[Y*SCREEN_W];
    /* cinq morceaux lineaires par ligne : x = a*X + b */
    float B[5]={r.xl-r.F, r.xl, r.xr, r.xr+r.F, (float)SCREEN_W+1};
    float A[5], C[5];
    A[0]=P25_A0/B[0];                     C[0]=0;
    A[1]=SC_FW/r.F;                       C[1]=P25_A0-B[0]*A[1];
    A[2]=1.0f/r.s;                        C[2]=P25_CX-P25_CX*A[2];
    A[3]=SC_FW/r.F;                       C[3]=PIT_X1-r.xr*A[3];
    A[4]=(SCREEN_W-P25_A1)/(SCREEN_W-B[3]); C[4]=P25_A1-B[3]*A[4];
    int sgi=0;
    for(int X=0;X<SCREEN_W;X++){
      float fx=X+0.5f; while(sgi<4 && fx>=B[sgi]) sgi++;
      int x=(int)(A[sgi]*fx+C[sgi]); if(x<0) x=0; if(x>=SCREEN_W) x=SCREEN_W-1;
      dst[X]=row[x];
    }
  }
}

/* ---------------- rendu GPU : debut de frame et liste d'effets ---------------- */
void gpu_frame_begin(void){
  g_nlights=0; g_nfx=0; cur_layer=LY_SCENE; gT=&g_scr;
  memset(&GP,0,sizeof GP);
  GP.cam_zoom=1; GP.cam_cx=SCREEN_W*0.5f; GP.cam_cy=SCREEN_H*0.5f;
  GP.view[0]=0; GP.view[1]=0; GP.view[2]=SCREEN_W; GP.view[3]=SCREEN_H;
  GP.lit[2]=SCREEN_W; GP.lit[3]=SCREEN_H;
  GP.emis_gain=1.0f; GP.bloom=0.5f; GP.bloom_thr=0.9f; GP.sat=1.04f; GP.con=1.02f; GP.vig=0.3f; GP.exposure=1.0f;
  GP.hi_tint[0]=GP.hi_tint[1]=GP.hi_tint[2]=1; GP.grain=0.018f; GP.normal_amt=0.85f; GP.spec=1.0f; GP.shadows=0.8f; GP.light_gain=1.0f; GP.light_range=1.0f; GP.time=g_time; GP.fog_speed=1.0f; GP.scan=S.crt?0.11f:0.0f;
  if(!g_gpu) return;
  memset(g_screen,0,sizeof g_screen); memset(g_emis,0,sizeof g_emis); memset(g_ui,0,sizeof g_ui); memset(g_occ,0,sizeof g_occ);
  for(int i=0;i<SCREEN_W*SCREEN_H;i++) g_nrm[i]=NRM_FLAT;
  g_scr.cx0=t_emis.cx0=t_ui.cx0=0; g_scr.cy0=t_emis.cy0=t_ui.cy0=0;
  g_scr.cx1=t_emis.cx1=t_ui.cx1=SCREEN_W; g_scr.cy1=t_emis.cy1=t_ui.cy1=SCREEN_H;
}
/* effets dessines par le CPU : rendu logiciel, interface, ou cible autre que les couches */
static bool fx_cpu(void){ return !g_gpu || cur_layer==LY_UI || (gT!=&g_scr && gT!=&t_emis); }
static FxItem *fx_new(int type,uint32_t c,float a){
  if(g_nfx>=GPU_MAX_FX || a<=0.003f) return NULL;
  FxItem *f=&g_fx[g_nfx++]; memset(f,0,sizeof *f); f->type=(uint8_t)type; f->c=c; f->a=a;
  f->clip[0]=(int16_t)gT->cx0; f->clip[1]=(int16_t)gT->cy0; f->clip[2]=(int16_t)gT->cx1; f->clip[3]=(int16_t)gT->cy1;
  return f;
}
static inline int a8(float a){ int v=(int)(a*255); return v>255?255:v; }
void fx_glow(float x,float y,float r,uint32_t c,float a){
  if(fx_cpu()){ gfx_glow(x,y,r,c,minf(a,1.0f)); return; }
  FxItem *f=fx_new(FX_GLOW,c,a); if(f){ f->x0=x; f->y0=y; f->r=r; }
}
void fx_dust(float x,float y,float r,uint32_t c,float a){
  if(fx_cpu()){ gfx_glow(x,y,r,c,minf(a,1.0f)); return; }
  FxItem *f=fx_new(FX_GLOW,c,a); if(f){ f->x0=x; f->y0=y; f->r=r; f->p=1; }
}
void fx_disc(float x,float y,float r,uint32_t c,float a){
  if(fx_cpu()){ gfx_disc_add((int)floorf(x+0.5f),(int)floorf(y+0.5f),(int)r,c,a8(a)); return; }
  FxItem *f=fx_new(FX_DISC,c,a); if(f){ f->x0=x; f->y0=y; f->r=r; }
}
void fx_ring(float x,float y,float r,float w,uint32_t c,float a){
  if(fx_cpu()){ gfx_ring_a((int)floorf(x+0.5f),(int)floorf(y+0.5f),(int)r,c,a8(a),true); return; }
  FxItem *f=fx_new(FX_RING,c,a); if(f){ f->x0=x; f->y0=y; f->r=r; f->p=w; }
}
void fx_caps(float x0,float y0,float x1,float y1,float r,uint32_t c,float a){
  if(fx_cpu()){ gfx_capsule_add(x0,y0,x1,y1,r,c,a8(a)); return; }
  FxItem *f=fx_new(FX_CAPS,c,a); if(f){ f->x0=x0; f->y0=y0; f->x1=x1; f->y1=y1; f->r=r; }
}
static int ball_style(int k){
  switch(k){
    case BK_SPECTRE: case BK_FAUCHEUSE: case BK_GHOST: case BK_RAYON: return 1;
    case BK_CRISTAL: case BK_GEODE: return 2;
    case BK_PRISME: case BK_KALEIDO: case BK_FOLLE: return 3;
    case BK_AIMANT: case BK_SINGULARITE: case BK_FLEAU: return 4;
    case BK_LUMIERE: case BK_AURORE: return 5;
    case BK_FOUDRE: case BK_TEMPETE: return 6;
    case BK_RAFALE: case BK_BLIZZARD: case BK_TEMPFEU: return 7;
    case BK_FEU: case BK_METEORE: case BK_BOMBE: case BK_VAPEUR: return 8;
    case BK_GLACE: case BK_ECLAT: case BK_SHARD: return 9;
    case BK_ECHO: case BK_TONNERRE: return 11;
    case BK_DOREE: case BK_MIDAS: return 12;
    case BK_RACINE: case BK_RONCES: return 13;
    case BK_ACIDE: case BK_PLUIE: return 14;
    default: return 0;
  }
}
void fx_ball(float x,float y,float r,int kind,float a){
  if(fx_cpu()){ art_draw_ball(x,y,r,kind,g_time,a>0.5f); return; }
  FxItem *f=fx_new(FX_BALL,BALLS[kind].col,1.0f); if(!f) return;
  f->x0=x; f->y0=y; f->r=r; f->p=(float)ball_style(kind); f->q=a;
  uint32_t c2=BALLS[kind].col2;
  if(kind==BK_AIMANT) c2=HEX(0xe0453a); else if(kind==BK_SINGULARITE) c2=HEX(0x1a0a2a); else if(kind==BK_FLEAU) c2=HEX(0x8a3ab8);
  f->c2=(c2&0xFFFFFFu)|(IS_EVO(kind)?0x01000000u:0u);
}
void fx_smoke(float x,float y,float r,uint32_t c,float a){
  if(fx_cpu()){ gfx_disc_a((int)floorf(x+0.5f),(int)floorf(y+0.5f),(int)r,c,a8(a)); return; }
  FxItem *f=fx_new(FX_SMOKE,c,a); if(f){ f->x0=x; f->y0=y; f->r=r; }
}
void fx_shaft(float x0,float y0,float x1,float y1,float w,uint32_t c,float a){
  if(fx_cpu()){ int aa=a8(a); float h=w*0.5f; gfx_tri_add(x0-h,y0,x0+h,y0,x1+h,y1,c,aa); gfx_tri_add(x0-h,y0,x1+h,y1,x1-h,y1,c,aa); return; }
  FxItem *f=fx_new(FX_SHAFT,c,a); if(f){ f->x0=x0; f->y0=y0; f->x1=x1; f->y1=y1; f->r=w; }
}
void fx_star(float x,float y,float r,uint32_t c,float a){
  if(fx_cpu()){ int aa=a8(a); gfx_capsule_add(x-r,y,x+r,y,0.5f,c,aa); gfx_capsule_add(x,y-r,x,y+r,0.5f,c,aa); return; }
  FxItem *f=fx_new(FX_STAR,c,a); if(f){ f->x0=x; f->y0=y; f->r=r; }
}
void fx_beam(float x,float y0,float y1,float w,uint32_t c,float a){
  if(fx_cpu()){ int iw=(int)(w+0.5f); gfx_rect_add((int)x-iw,(int)y0,iw*2+1,(int)(y1-y0),c,a8(a*0.45f)); gfx_rect_add((int)x-iw/2,(int)y0,iw+1,(int)(y1-y0),HEX(0xffffff),a8(a*0.6f)); return; }
  FxItem *f=fx_new(FX_BEAM,c,a); if(f){ f->x0=x; f->y0=y0; f->x1=x; f->y1=y1; f->r=w; }
}
void fx_orb(float x,float y,float r,uint32_t c,float a){
  if(fx_cpu()){ int ix=(int)floorf(x+0.5f), iy=(int)floorf(y+0.5f), ir=(int)r; gfx_disc(ix,iy,ir+1,HEX(0x140a10)); gfx_disc(ix,iy,ir,col_scale(c,0.7f)); gfx_disc(ix,iy,ir-1,c); gfx_pset(ix,iy,HEX(0xffffff)); return; }
  FxItem *f=fx_new(FX_BALL,c,1.0f); if(!f) return;
  f->x0=x; f->y0=y; f->r=r+0.8f; f->p=10; f->q=0; f->c2=HEX(0x140a10)&0xFFFFFFu; (void)a;
}
