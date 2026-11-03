/* GOUFFRE - portraits peints des heros (64x64) pour l'ecran de choix.
   Chaque portrait est d'abord pose en aplats (« regions » : armure, capuche, visage...), puis repeint :
   volume du buste eclaire en haut a gauche, ombres violettes, contre-jour colore sur le bord droit,
   reflets des metaux, meches des cheveux, visages dans l'ombre, contour sombre. */
#include "game.h"

#define PW 64
enum { MT_CLOTH, MT_METAL, MT_HAIR, MT_FACE, MT_GLOW, MT_GLASS, MT_SKIN2, MT_LEATHER };
typedef struct { uint32_t c; int mt; } Reg;
static Reg RG[48]; static int nrg;
static uint8_t rmap[PW*PW];     /* 0 : vide ; sinon region+1 */
static Sprite PORT[CH_COUNT], PORT_LOCK[CH_COUNT];
static uint32_t port_rim;       /* couleur du contre-jour */

static int reg(uint32_t c,int mt){ if(nrg>=47) return nrg-1; RG[nrg].c=c; RG[nrg].mt=mt; return nrg++; }
static inline void rp(int x,int y,int r){ if(x<0||y<0||x>=PW||y>=PW) return; rmap[y*PW+x]=(uint8_t)(r+1); }
static inline int rg_at(int x,int y){ if(x<0||y<0||x>=PW||y>=PW) return -1; return (int)rmap[y*PW+x]-1; }
static void R_ellipse(float cx,float cy,float rx,float ry,int r){
  for(int y=(int)floorf(cy-ry);y<=(int)ceilf(cy+ry);y++) for(int x=(int)floorf(cx-rx);x<=(int)ceilf(cx+rx);x++){
    float dx=(x+0.5f-cx)/rx, dy=(y+0.5f-cy)/ry; if(dx*dx+dy*dy<=1.0f) rp(x,y,r); } }
static void R_rect(int x0,int y0,int x1,int y1,int r){ for(int y=y0;y<=y1;y++) for(int x=x0;x<=x1;x++) rp(x,y,r); }
static void R_poly(const float *p,int n,int r){   /* polygone (x0,y0,x1,y1...) */
  float minx=1e9f,maxx=-1e9f,miny=1e9f,maxy=-1e9f; for(int i=0;i<n;i++){ minx=fminf(minx,p[i*2]); maxx=fmaxf(maxx,p[i*2]); miny=fminf(miny,p[i*2+1]); maxy=fmaxf(maxy,p[i*2+1]); }
  for(int y=(int)floorf(miny);y<=(int)ceilf(maxy);y++) for(int x=(int)floorf(minx);x<=(int)ceilf(maxx);x++){
    float px=x+0.5f, py=y+0.5f; bool in=false;
    for(int i=0,j=n-1;i<n;j=i++){ float xi=p[i*2],yi=p[i*2+1],xj=p[j*2],yj=p[j*2+1];
      if(((yi>py)!=(yj>py)) && (px < (xj-xi)*(py-yi)/(yj-yi+1e-6f)+xi)) in=!in; }
    if(in) rp(x,y,r); } }
static void R_line(float x0,float y0,float x1,float y1,float th,int r){
  float dx=x1-x0, dy=y1-y0; int n=(int)(sqrtf(dx*dx+dy*dy)*2)+1;
  for(int i=0;i<=n;i++){ float t=(float)i/n; float x=x0+dx*t, y=y0+dy*t; if(th<=0.6f) rp((int)x,(int)y,r); else R_ellipse(x,y,th,th,r); } }

static void R_ring(float cx,float cy,float r0,float r1,int r){
  for(int y=(int)(cy-r1)-1;y<=(int)(cy+r1)+1;y++) for(int x=(int)(cx-r1)-1;x<=(int)(cx+r1)+1;x++){ float d=sqrtf((x+0.5f-cx)*(x+0.5f-cx)+(y+0.5f-cy)*(y+0.5f-cy)); if(d>=r0 && d<=r1) rp(x,y,r); } }

static float h_hash(int x,int y){ uint32_t h=((uint32_t)x*73856093u)^((uint32_t)y*19349663u); h^=h>>13; h*=0x5bd1e995u; h^=h>>15; return (h&1023)/1023.0f*3.0f; }
static inline float lum(uint32_t c){ return (cr(c)*0.3f+cg(c)*0.59f+cb(c)*0.11f)/255.0f; }
/* ---------------- peinture ---------------- */
static void paint(Sprite *s){
  int n=PW*PW;
  /* distance au vide (volume du buste) */
  float *dg=(float*)malloc((size_t)n*sizeof(float)), *dr=(float*)malloc((size_t)n*sizeof(float));
  for(int i=0;i<n;i++){ dg[i]=rmap[i]?99.0f:0.0f; dr[i]=rmap[i]?99.0f:0.0f; }
  for(int pass=0;pass<2;pass++){
    for(int y=0;y<PW;y++) for(int x=0;x<PW;x++){ int i=y*PW+x; if(!rmap[i]) continue;
      /* le bas de l'image coupe le buste : pas de bord la */
      float m=dg[i], q=dr[i];
      if(x>0){ m=fminf(m,dg[i-1]+1); q=fminf(q,(rmap[i-1]==rmap[i]?dr[i-1]:0)+1); } else { m=fminf(m,1); q=fminf(q,1); }
      if(y>0){ m=fminf(m,dg[i-PW]+1); q=fminf(q,(rmap[i-PW]==rmap[i]?dr[i-PW]:0)+1); } else { m=fminf(m,1); q=fminf(q,1); }
      dg[i]=m; dr[i]=q; }
    for(int y=PW-1;y>=0;y--) for(int x=PW-1;x>=0;x--){ int i=y*PW+x; if(!rmap[i]) continue;
      float m=dg[i], q=dr[i];
      if(x<PW-1){ m=fminf(m,dg[i+1]+1); q=fminf(q,(rmap[i+1]==rmap[i]?dr[i+1]:0)+1); } else { m=fminf(m,1); q=fminf(q,1); }
      if(y<PW-1){ m=fminf(m,dg[i+PW]+1); q=fminf(q,(rmap[i+PW]==rmap[i]?dr[i+PW]:0)+1); }
      dg[i]=m; dr[i]=q; }
  }
  for(int i=0;i<n;i++){ dg[i]=fminf(dg[i],9.0f); dr[i]=fminf(dr[i],4.0f); }
  static const uint8_t B2[4][4]={{0,8,2,10},{12,4,14,6},{3,11,1,9},{15,7,13,5}};
  for(int y=0;y<PW;y++) for(int x=0;x<PW;x++){
    int i=y*PW+x; if(!rmap[i]){ s->px[i]=0; continue; }
    const Reg *R=&RG[rmap[i]-1]; uint32_t c=R->c;
    if(R->mt==MT_GLOW){ s->px[i]=c|0xFF000000u; if(!s->em) s->em=(uint32_t*)calloc((size_t)n,4); s->em[i]=c; continue; }
    float gl=x>0?dg[i-1]:0, gr=x<PW-1?dg[i+1]:0, gu=y>0?dg[i-PW]:0, gd=y<PW-1?dg[i+PW]:9;
    float ll=x>0&&rmap[i-1]==rmap[i]?dr[i-1]:0, lr=x<PW-1&&rmap[i+1]==rmap[i]?dr[i+1]:0, lu=y>0&&rmap[i-PW]==rmap[i]?dr[i-PW]:0, ld=y<PW-1&&rmap[i+PW]==rmap[i]?dr[i+PW]:4;
    float nx=-(gr-gl)*0.7f-(lr-ll)*0.5f, ny=-(gd-gu)*0.7f-(ld-lu)*0.5f, nz=2.2f; float nl=sqrtf(nx*nx+ny*ny+nz*nz); nx/=nl; ny/=nl; nz/=nl;
    float lam=clampf(nx*-0.62f+ny*-0.58f+nz*0.53f,0,1);
    float f=0.42f+0.78f*lam;
    f*=1.06f-0.22f*(float)y/PW;
    if(dr[i]<=1.0f) f-=0.05f;                               /* liseré sombre au bord de chaque masse */
    float spec=0;
    switch(R->mt){
      case MT_METAL: f=0.30f+1.0f*lam; f*=1.04f-0.15f*(float)y/PW; if(lam>0.86f) spec=(lam-0.86f)*6; if(((x+y*2)%9)==0 && lam>0.6f) spec+=0.25f; break;
      case MT_HAIR: { float st=sinf(x*1.1f+y*0.35f+h_hash(x,y)); if(st>0.55f) f-=0.18f; else if(st<-0.8f) f+=0.08f; } break;
      case MT_FACE: f=0.35f+0.45f*lam; break;
      case MT_GLASS: f=0.25f+0.4f*lam; if(((x-y)&7)==0 && lam>0.5f) spec=0.9f; break;
      case MT_LEATHER: if(((x*3+y*5)%13)==0) f-=0.1f; break;
      default: break;
    }
    float q=(f-0.3f)/0.95f*4.0f + ((B2[y&3][x&3]+0.5f)/16.0f-0.5f)*0.45f; int lv=clampi((int)floorf(q+0.5f),0,4);
    static const float LV[5]={0.38f,0.62f,0.84f,1.0f,1.16f};
    float k=LV[lv];
    uint32_t o;
    if(k<1.0f){ uint32_t d=col_scale(c,0.18f+0.82f*k); o=col_lerp(d,HEX(0x1a0c2a),(1.0f-k)*0.6f); }
    else o=col_lerp(c,HEX(0xfff0dc),(k-1.0f)*1.6f);
    if(spec>0) o=col_lerp(o,HEX(0xffffff),clampf(spec,0,0.85f));
    /* contre-jour colore sur le cote droit */
    if(dg[i]<=2.6f && nx>0.12f){ float a=(1-dg[i]/2.6f)*clampf(nx*1.8f,0,1)*0.75f; o=col_lerp(o,port_rim,a); }
    s->px[i]=o|0xFF000000u;
  }
  /* traits entre masses contrastees, contour exterieur */
  uint32_t *cp=(uint32_t*)malloc((size_t)n*4); memcpy(cp,s->px,(size_t)n*4);
  for(int y=0;y<PW;y++) for(int x=0;x<PW;x++){ int i=y*PW+x; if(!rmap[i]) continue; if(s->em && s->em[i]) continue;
    int nb[2]={ x<PW-1?i+1:-1, y<PW-1?i+PW:-1 };
    for(int k=0;k<2;k++){ int j=nb[k]; if(j<0||!rmap[j]||rmap[j]==rmap[i]) continue;
      float a=lum(cp[i]), b=lum(cp[j]); if(fabsf(a-b)>0.22f){ int dk= a<b ? i : j; s->px[dk]=col_lerp(cp[dk],HEX(0x120a18),0.55f)|0xFF000000u; } } }
  for(int y=0;y<PW;y++) for(int x=0;x<PW;x++){ int i=y*PW+x; if(rmap[i]) continue;
    bool e=(x>0&&rmap[i-1])||(x<PW-1&&rmap[i+1])||(y>0&&rmap[i-PW])||(y<PW-1&&rmap[i+PW]);
    if(e) s->px[i]=HEX(0x0c0812); }
  free(cp); free(dg); free(dr);
}

/* ---------------- les huit heros ---------------- */
static void p_bust(uint32_t cloth,int mt,uint32_t cape){
  if(cape){ int c=reg(cape,MT_CLOTH); R_ellipse(32,66,31,20,c); }
  int b=reg(cloth,mt); R_ellipse(32,64,26,15,b);
}
static void p_sentinelle(void){
  port_rim=HEX(0x7ab0ff);
  p_bust(HEX(0x2a58b8),MT_CLOTH,HEX(0x1a2a62));
  int steel=reg(HEX(0x9aa6bc),MT_METAL), steeld=reg(HEX(0x6a748a),MT_METAL), gold=reg(HEX(0xd8a83a),MT_METAL), blue=reg(HEX(0x2f62c8),MT_CLOTH);
  int plume=reg(HEX(0x3a78e8),MT_HAIR), visor=reg(HEX(0x0e0a16),MT_CLOTH), glow=reg(HEX(0x7ad8ff),MT_GLOW);
  /* panache */
  { float p[]={30,14, 34,8, 42,3, 52,4, 58,10, 56,16, 48,12, 40,13, 35,18}; R_poly(p,9,plume); }
  /* gorgerin et epaulieres */
  R_rect(25,38,39,47,steeld);
  R_ellipse(13,52,13,10,steel); R_ellipse(51,52,13,10,steel);
  R_ring(13,52,10,12.5f,gold); R_ring(51,52,10,12.5f,gold);
  R_ellipse(13,53,8,6,steel); R_ellipse(51,53,8,6,steel);
  /* tabard et embleme */
  { float p[]={23,46, 41,46, 44,64, 20,64}; R_poly(p,4,blue); }
  R_ring(32,55,3,5,gold); R_rect(31,50,33,60,gold);
  /* heaume */
  R_ellipse(32,27,14,15.5f,steel);
  R_rect(19,26,45,33,steel);
  R_rect(21,24,43,27,visor);                     /* fente du heaume */
  R_rect(31,13,33,40,steeld);                    /* arete centrale */
  R_rect(31,24,33,27,visor);
  for(int k=0;k<4;k++) R_rect(36+k*2,31,36+k*2,32,visor);
  R_rect(24,25,25,26,glow); R_rect(38,25,39,26,glow);
}
static void p_alchimiste(void){
  port_rim=HEX(0x9aff7a);
  p_bust(HEX(0x2f7a3a),MT_CLOTH,0);
  int hood=reg(HEX(0x3a8a3e),MT_CLOTH), hoodd=reg(HEX(0x1e4a26),MT_CLOTH), face=reg(HEX(0xb03a8a),MT_FACE), mask=reg(HEX(0x9a947c),MT_CLOTH);
  int brass=reg(HEX(0xd0a03a),MT_METAL), glass=reg(HEX(0x2a5a6a),MT_GLASS), strap=reg(HEX(0x6a4428),MT_LEATHER);
  int v1=reg(HEX(0x9cff5a),MT_GLOW), v2=reg(HEX(0xd070ff),MT_GLOW), v3=reg(HEX(0xffa040),MT_GLOW), vg=reg(HEX(0xc8d8d0),MT_GLASS);
  /* capuche */
  { float p[]={32,2, 46,16, 50,34, 48,46, 16,46, 14,34, 18,16}; R_poly(p,7,hood); }
  R_ellipse(32,30,11.5f,13.5f,hoodd);
  R_ellipse(32,31,10,12,face);
  { int eye=reg(HEX(0xffd36a),MT_GLOW); R_rect(27,28,28,28,eye); R_rect(36,28,37,28,eye); }
  { float p[]={22,34, 42,34, 40,42, 32,44, 24,42}; R_poly(p,5,mask); } for(int k=0;k<4;k++) rp(26+k*4,38,hoodd);
  /* lunettes de cuivre relevees sur la capuche */
  R_rect(20,18,44,20,strap);
  R_ellipse(26,19,4.5f,4.5f,brass); R_ellipse(38,19,4.5f,4.5f,brass);
  R_ellipse(26,19,2.8f,2.8f,glass); R_ellipse(38,19,2.8f,2.8f,glass);
  /* baudrier et fioles */
  R_line(14,46,50,62,2.0f,strap);
  { int VC[3]={v1,v2,v3}; for(int k=0;k<3;k++){ float x=22+k*10, y=49+k*4.4f; R_rect((int)x-2,(int)y-5,(int)x+2,(int)y+1,vg); R_rect((int)x-1,(int)y-3,(int)x+1,(int)y,VC[k]); R_rect((int)x-1,(int)y-7,(int)x+1,(int)y-6,strap); } }
}
static void p_jongleur(void){
  port_rim=HEX(0xffd84a);
  p_bust(HEX(0xd8402e),MT_CLOTH,0);
  int red=reg(HEX(0xd23c32),MT_CLOTH), yel=reg(HEX(0xf2c43a),MT_CLOTH), face=reg(HEX(0xc23a6e),MT_FACE), maskw=reg(HEX(0xece4dc),MT_CLOTH), dark=reg(HEX(0x140a18),MT_CLOTH);
  int bell=reg(HEX(0xf0c040),MT_METAL), ruff=reg(HEX(0xf4ecdc),MT_CLOTH), ruff2=reg(HEX(0xc8c0b0),MT_CLOTH), ball1=reg(HEX(0x5ad8ff),MT_GLOW), ball2=reg(HEX(0xff7a5a),MT_GLOW);
  /* habit en losanges */
  for(int y=48;y<64;y++) for(int x=6;x<58;x++){ if(rg_at(x,y)<0) continue; if((((x+2)/5+(y)/5)&1)) rp(x,y,yel); }
  /* col a fraise */
  for(int k=0;k<9;k++){ float a=PI*0.12f+k*PI*0.76f/8; float x=32-cosf(a)*21, y=44+sinf(a)*5; R_ellipse(x,y,4.2f,3.2f,k&1?ruff:ruff2); }
  /* visage et loup */
  R_ellipse(32,30,11,12.5f,face);
  { float p[]={20,25, 44,25, 42,31, 35,32, 32,29, 29,32, 22,31}; R_poly(p,7,maskw); }
  R_ellipse(26.5f,28,2.2f,1.4f,dark); R_ellipse(37.5f,28,2.2f,1.4f,dark);
  /* bonnet a trois pointes */
  R_rect(19,19,45,23,yel);
  { float p[]={19,21, 10,8, 4,15, 8,20, 16,24}; R_poly(p,5,red); }
  { float p[]={27,20, 32,2, 37,20}; R_poly(p,3,yel); }
  { float p[]={45,21, 54,8, 60,15, 56,20, 48,24}; R_poly(p,5,red); }
  R_rect(19,22,45,23,red);
  R_ellipse(5,15,2.6f,2.6f,bell); R_ellipse(32,2.5f,2.6f,2.6f,bell); R_ellipse(59,15,2.6f,2.6f,bell);
  R_ellipse(54,36,3,3,ball1); R_ellipse(58,28,2.5f,2.5f,ball2);
}
static void p_forgeronne(void){
  port_rim=HEX(0xff9a4a);
  p_bust(HEX(0xc8b08c),MT_CLOTH,0);
  int hair=reg(HEX(0xd2462e),MT_HAIR), haird=reg(HEX(0x8a2420),MT_HAIR), face=reg(HEX(0x3a7a9a),MT_FACE), band=reg(HEX(0x2a2a3a),MT_CLOTH);
  int apron=reg(HEX(0x6a4428),MT_LEATHER), strap=reg(HEX(0x3a2416),MT_LEATHER), wood=reg(HEX(0x8a5a30),MT_LEATHER), iron=reg(HEX(0x7a7a88),MT_METAL), soot=reg(HEX(0x24364a),MT_FACE);
  /* cheveux en masse, tresse sur l'epaule droite */
  R_ellipse(32,24,17,17,hair); R_ellipse(20,30,8,12,hair); R_ellipse(44,30,8,12,hair);
  for(int k=0;k<6;k++) R_ellipse(46+k*1.2f,38+k*4.5f,3.6f,3.0f,k&1?haird:hair);
  /* visage, bandeau, suie */
  R_ellipse(32,30,10,11.5f,face);
  R_rect(17,17,47,20,band);
  R_line(26,35,29,37,0.5f,soot); R_line(37,33,40,32,0.5f,soot);
  { int eye=reg(HEX(0x10202c),MT_CLOTH); R_rect(26,28,29,29,eye); R_rect(35,28,38,29,eye); int gl=reg(HEX(0xffe0b0),MT_GLOW); rp(27,28,gl); rp(36,28,gl); }
  /* tablier de cuir */
  { float p[]={21,46, 43,46, 46,64, 18,64}; R_poly(p,4,apron); }
  R_line(21,46,14,40,1.2f,strap); R_line(43,46,50,40,1.2f,strap);
  /* gros marteau sur l'epaule gauche */
  R_line(4,64,17,30,1.9f,wood);
  { float p[]={4,22, 24,30, 20,40, 0,32}; R_poly(p,4,iron); }
  { int iron2=reg(HEX(0xb8b8c8),MT_METAL); R_line(5,23,22,30,0.6f,iron2); }
}
static void p_magnetiseur(void){
  port_rim=HEX(0x6ad8ff);
  { int suit=reg(HEX(0xb8642e),MT_LEATHER); R_ellipse(32,66,22,16,suit); int pad=reg(HEX(0xd88a3a),MT_METAL); R_ellipse(14,52,8,6,pad); R_ellipse(50,52,8,6,pad); }
  int red=reg(HEX(0xd83a32),MT_METAL), blue=reg(HEX(0x3a6ad8),MT_METAL), mag=reg(HEX(0x8a8a9a),MT_METAL), brass=reg(HEX(0xc89a3a),MT_METAL);
  int glass=reg(HEX(0x1a3a4a),MT_GLASS), face=reg(HEX(0x9a5a3a),MT_FACE), pipe=reg(HEX(0x5a5a6a),MT_METAL), spark=reg(HEX(0xbff4ff),MT_GLOW), rivet=reg(HEX(0xffe6a0),MT_METAL);
  /* l'aimant en fer a cheval dans le dos */
  R_line(10,48,10,14,4.0f,mag); R_line(54,48,54,14,4.0f,mag);
  R_rect(6,6,14,14,red); R_rect(50,6,58,14,blue);
  /* casque rond a hublot */
  R_ellipse(32,27,15,15,brass);
  R_ellipse(32,28,10,9,glass);
  R_ellipse(32,31,7,5,face);
  for(int k=0;k<6;k++){ float a=k*TAU/6; rp((int)(32+cosf(a)*13),(int)(27+sinf(a)*13),rivet); }
  R_rect(30,9,34,12,pipe); R_line(32,9,32,3,0.5f,pipe); R_ellipse(32,3,1.5f,1.5f,spark);
  /* tuyaux et rivets */
  R_line(18,40,12,52,1.4f,pipe); R_line(46,40,52,52,1.4f,pipe);
  for(int k=0;k<4;k++) rp(24+k*5,54,rivet);
  /* etincelles */
  R_line(14,8,20,4,0.5f,spark); R_line(20,4,23,7,0.5f,spark); R_line(50,9,45,4,0.5f,spark); R_line(45,4,42,6,0.5f,spark);
}
static void p_necro(void){
  port_rim=HEX(0xb07aff);
  p_bust(HEX(0x3a2266),MT_CLOTH,0);
  int hood=reg(HEX(0x5a3296),MT_CLOTH), hoodd=reg(HEX(0x24123e),MT_CLOTH), face=reg(HEX(0x2e1a44),MT_FACE), eye=reg(HEX(0x9cff7a),MT_GLOW);
  int bone=reg(HEX(0xe2d6b8),MT_CLOTH), boned=reg(HEX(0x2a2026),MT_CLOTH), staff=reg(HEX(0xc8b898),MT_LEATHER), soul=reg(HEX(0x7aff9a),MT_GLOW), rune=reg(HEX(0x8a6ad8),MT_GLOW);
  /* baton d'os */
  R_line(55,64,55,18,1.4f,staff);
  R_ellipse(55,13,5,5.5f,bone); R_rect(52,12,53,13,boned); R_rect(57,12,58,13,boned); R_rect(54,16,56,17,boned);
  R_ellipse(55,5,2.5f,3.5f,soul);
  /* capuche profonde */
  { float p[]={32,4, 47,14, 50,40, 46,48, 18,48, 14,40, 17,14}; R_poly(p,7,hood); }
  R_ellipse(32,30,11,13,hoodd);
  R_ellipse(32,32,8.5f,10,face);
  R_rect(26,29,28,30,eye); R_rect(36,29,38,30,eye);
  /* pendentif crane */
  R_ellipse(32,52,4,3.5f,bone); R_rect(30,52,30,52,boned); R_rect(34,52,34,52,boned); R_rect(31,55,33,56,bone);
  for(int k=0;k<4;k++) rp(18+k*2,58-k,rune);
  for(int k=0;k<4;k++) rp(42+k*2,55+k,rune);
}
static void p_horlogere(void){
  port_rim=HEX(0x7af0d8);
  p_bust(HEX(0x2a8a7a),MT_CLOTH,0);
  int gear=reg(HEX(0x8a6a3a),MT_METAL), hair=reg(HEX(0xf0c860),MT_HAIR), haird=reg(HEX(0xb8902a),MT_HAIR), face=reg(HEX(0x3a72b0),MT_FACE);
  int brass=reg(HEX(0xd8a83a),MT_METAL), lens=reg(HEX(0xbff4ff),MT_GLOW), lace=reg(HEX(0xe8e0d0),MT_CLOTH), teal=reg(HEX(0x1e6a62),MT_CLOTH);
  /* engrenage derriere */
  R_ring(13,15,6,10,gear); for(int k=0;k<8;k++){ float a=k*TAU/8; R_ellipse(13+cosf(a)*11,15+sinf(a)*11,1.6f,1.6f,gear); }
  /* chignon et coiffure */
  R_ellipse(32,9,8,6,hair); R_ellipse(32,25,15,15,hair); R_ellipse(19,32,5,9,haird); R_ellipse(45,32,5,9,haird);
  R_line(36,4,44,12,0.6f,brass);
  /* visage et monocle */
  R_ellipse(32,31,10,11.5f,face);
  R_rect(20,20,44,22,hair);
  R_ring(37,29,2.6f,4.2f,brass); R_ellipse(37,29,2.4f,2.4f,lens); R_line(41,31,44,42,0.5f,brass);
  /* col de dentelle, broche engrenage */
  for(int k=0;k<7;k++) R_ellipse(23+k*3,45,2.2f,2.2f,lace);
  { float p[]={24,47, 40,47, 44,64, 20,64}; R_poly(p,4,teal); }
  R_ring(32,54,2,4,brass); for(int k=0;k<6;k++){ float a=k*TAU/6; rp((int)(32.5f+cosf(a)*5),(int)(54.5f+sinf(a)*5),brass); }
}
static void p_pyromane(void){
  port_rim=HEX(0xffb03a);
  p_bust(HEX(0x2e2024),MT_LEATHER,0);
  int fl1=reg(HEX(0xff6a1a),MT_GLOW), fl2=reg(HEX(0xffc840),MT_GLOW), fl3=reg(HEX(0xfff0a0),MT_GLOW), face=reg(HEX(0x7a3aa0),MT_FACE);
  int scarf=reg(HEX(0x9a2a2a),MT_CLOTH), trim=reg(HEX(0xe08a2a),MT_CLOTH), coat=reg(HEX(0x3a2a2e),MT_LEATHER), hand=reg(HEX(0x5a2a6a),MT_FACE);
  /* cheveux de flammes */
  { float p[]={14,30, 12,14, 18,20, 20,4, 26,14, 32,0, 38,12, 44,3, 46,18, 52,12, 50,30}; R_poly(p,11,fl1); }
  { float p[]={18,28, 20,14, 25,20, 32,6, 38,18, 44,10, 46,28}; R_poly(p,7,fl2); }
  { float p[]={24,26, 32,14, 40,26}; R_poly(p,3,fl3); }
  /* visage, echarpe */
  R_ellipse(32,30,10,11.5f,face);
  { int eye=reg(HEX(0xffa030),MT_GLOW); R_rect(26,28,29,29,eye); R_rect(35,28,38,29,eye); }
  { float p[]={20,36, 44,36, 46,45, 18,45}; R_poly(p,4,scarf); }
  R_line(40,44,46,56,1.4f,scarf);
  /* manteau a revers */
  { float p[]={14,48, 32,64, 50,48, 54,64, 10,64}; R_poly(p,5,coat); }
  R_line(20,47,32,62,0.6f,trim); R_line(44,47,32,62,0.6f,trim);
  /* main en feu */
  R_ellipse(10,52,4,4.5f,hand);
  { float p[]={6,50, 8,38, 10,44, 12,34, 14,46, 15,50}; R_poly(p,6,fl1); }
  { float p[]={8,50, 10,42, 12,50}; R_poly(p,3,fl2); }
}
/* v7 */
static void p_druide(void){
  port_rim=HEX(0xa8ff7a);
  p_bust(HEX(0x3a6a2a),MT_CLOTH,HEX(0x24401a));
  int hood=reg(HEX(0x4a8a34),MT_CLOTH), hoodd=reg(HEX(0x1e3a16),MT_CLOTH), face=reg(HEX(0x2a6a5a),MT_FACE);
  int bark=reg(HEX(0x7a5230),MT_LEATHER), barkd=reg(HEX(0x4a3018),MT_LEATHER), leaf=reg(HEX(0x7ad04a),MT_CLOTH), leafd=reg(HEX(0x3f8a2a),MT_CLOTH);
  int glow=reg(HEX(0xb8ff7a),MT_GLOW), eye=reg(HEX(0xd8ff9a),MT_GLOW), berry=reg(HEX(0xe0453a),MT_CLOTH);
  /* baton noueux */
  R_line(55,64,53,17,1.6f,bark); R_ellipse(53,12,4.8f,4.8f,barkd); R_ellipse(53,12,2.8f,2.8f,glow);
  R_line(53,16,48,10,0.6f,bark); R_line(53,16,58,9,0.6f,bark); R_ellipse(47,9,2.2f,1.4f,leaf); R_ellipse(59,8,2.2f,1.4f,leaf);
  /* bois de cerf faits de branches */
  R_line(24,17,15,5,1.0f,bark); R_line(19,11,11,11,0.7f,bark); R_line(17,7,19,1,0.6f,bark);
  R_line(40,17,49,5,1.0f,bark); R_line(45,11,53,11,0.7f,bark); R_line(47,7,45,1,0.6f,bark);
  R_ellipse(12,10,2.4f,1.5f,leaf); R_ellipse(52,10,2.4f,1.5f,leaf); R_ellipse(10,4,2,1.3f,leafd); R_ellipse(54,3,2,1.3f,leafd);
  /* capuche de feuilles */
  { float q[]={32,8, 45,16, 49,34, 47,47, 17,47, 15,34, 19,16}; R_poly(q,7,hood); }
  for(int k=0;k<7;k++){ float an=PI*0.15f+k*PI*0.7f/6; R_ellipse(32-cosf(an)*16,14+sinf(an)*4+k%2,3.2f,2.2f,k&1?leaf:leafd); }
  R_ellipse(32,30,11,13,hoodd);
  R_ellipse(32,31,9,11,face);
  R_rect(27,29,28,29,eye); R_rect(36,29,37,29,eye);
  /* liane et baies */
  R_line(16,48,48,56,1.0f,leafd); for(int k=0;k<5;k++) R_ellipse(19+k*7,49+k*1.6f,2.2f,1.5f,leaf);
  R_ellipse(26,53,1.6f,1.6f,berry); R_ellipse(38,55,1.6f,1.6f,berry);
}
static void p_marchand(void){
  port_rim=HEX(0xffd86a);
  p_bust(HEX(0x6a2a4a),MT_CLOTH,HEX(0x3a1430));
  int coat=reg(HEX(0x7a2a52),MT_CLOTH), trim=reg(HEX(0xe0b040),MT_METAL), hat=reg(HEX(0x3a2030),MT_LEATHER), hatb=reg(HEX(0xc89a3a),MT_METAL);
  int face=reg(HEX(0x9a5a3a),MT_FACE), feather=reg(HEX(0x5ad8c8),MT_HAIR), purse=reg(HEX(0x8a5a2a),MT_LEATHER), coin=reg(HEX(0xffd84a),MT_METAL), glint=reg(HEX(0xfff4c0),MT_GLOW);
  int beard=reg(HEX(0x4a3022),MT_HAIR), eye=reg(HEX(0xffe090),MT_GLOW);
  /* manteau a revers dores */
  { float q[]={12,48, 32,62, 52,48, 56,64, 8,64}; R_poly(q,5,coat); }
  R_line(20,47,32,61,0.7f,trim); R_line(44,47,32,61,0.7f,trim);
  /* bourse rebondie */
  R_ellipse(24,56,6,5,purse); R_rect(22,50,26,52,purse); R_line(21,51,27,51,0.5f,trim);
  /* visage, barbe taillee, dent d'or */
  R_ellipse(32,31,10,11.5f,face);
  { float q[]={23,34, 41,34, 39,42, 32,45, 25,42}; R_poly(q,5,beard); }
  R_rect(26,28,28,29,eye); R_rect(36,28,38,29,eye);
  rp(33,38,coin);
  /* grand chapeau a plume */
  R_ellipse(32,20,22,4.5f,hat); R_ellipse(32,14,12,8,hat); R_rect(20,16,44,18,hatb);
  { float q[]={40,12, 50,2, 58,0, 54,6, 46,14}; R_poly(q,5,feather); }
  /* pieces lancees */
  R_ellipse(55,34,3.4f,3.4f,coin); rp(54,33,glint); R_ellipse(50,44,2.4f,2.4f,coin);
}
/* v9 : l'Astronome, chapeau pointu et longue-vue */
static void p_astronome(void){
  port_rim=HEX(0x9ab8ff);
  p_bust(HEX(0x2a3b80),MT_CLOTH,HEX(0x141c48));
  int robe=reg(HEX(0x2a3b80),MT_CLOTH), robed=reg(HEX(0x1a2458),MT_CLOTH), face=reg(HEX(0xd8a080),MT_FACE), hair=reg(HEX(0xe8e4f0),MT_HAIR);
  int hat=reg(HEX(0x223070),MT_CLOTH), brim=reg(HEX(0x3b7fdc),MT_CLOTH), star=reg(HEX(0xf9dc5c),MT_GLOW), eye=reg(HEX(0x2a1c30),MT_CLOTH);
  int brass=reg(HEX(0xd0a03a),MT_METAL), lens=reg(HEX(0x9ad8ff),MT_GLOW), cstar=reg(HEX(0xd4f4ff),MT_GLOW);
  { float q[]={14,50, 32,60, 50,50, 54,64, 10,64}; R_poly(q,5,robe); } R_line(32,48,32,62,0.8f,robed);
  rp(22,56,star); rp(40,58,star); rp(46,53,cstar);
  R_ellipse(32,33,10,11,face); R_rect(24,38,26,46,hair); R_rect(38,38,40,46,hair);
  R_rect(27,32,28,33,eye); R_rect(36,32,37,33,eye);
  R_ellipse(32,24,20,4.0f,brim); { float q[]={20,23, 44,23, 36,8, 30,0, 28,8}; R_poly(q,5,hat); }
  rp(31,12,star); rp(35,18,star); rp(27,19,cstar);
  R_line(44,58,58,44,2.2f,brass); R_ellipse(58,44,2.6f,2.6f,lens);
}
/* v9 : la Voleuse d'ombres, capuche et regard violet */
static void p_voleuse(void){
  port_rim=HEX(0xc08aff);
  p_bust(HEX(0x2a1a3a),MT_CLOTH,HEX(0x140a1e));
  int cloak=reg(HEX(0x3a2252),MT_CLOTH), cloakd=reg(HEX(0x1e1030),MT_CLOTH), shadow=reg(HEX(0x0e0814),MT_CLOTH), eyes=reg(HEX(0xd2b0ff),MT_GLOW);
  int mask=reg(HEX(0x24182e),MT_LEATHER), blade=reg(HEX(0xd8dce8),MT_METAL), hilt=reg(HEX(0x8a5aff),MT_GLOW), strap=reg(HEX(0x4a3022),MT_LEATHER);
  { float q[]={10,64, 16,44, 32,52, 48,44, 54,64}; R_poly(q,5,cloak); } R_line(20,50,30,62,0.7f,strap);
  { float q[]={14,40, 18,18, 32,8, 46,18, 50,40, 32,50}; R_poly(q,6,cloakd); }
  R_ellipse(32,32,10,11,shadow); R_rect(24,34,40,40,mask);
  R_rect(26,29,29,30,eyes); R_rect(35,29,38,30,eyes);
  R_line(48,62,58,40,1.4f,blade); R_line(46,60,50,58,1.2f,hilt);
}
/* v10 : la Chasseresse, capuche verte et boomerang */
static void p_chasseresse(void){
  port_rim=HEX(0x8ae07a);
  p_bust(HEX(0x2a5a32),MT_CLOTH,HEX(0x12301a));
  int hood=reg(HEX(0x2f7a3f),MT_CLOTH), hoodd=reg(HEX(0x1a4a26),MT_CLOTH), face=reg(HEX(0xd8a080),MT_FACE), hair=reg(HEX(0x8a4a22),MT_HAIR);
  int eye=reg(HEX(0x2a3a1c),MT_CLOTH), leather=reg(HEX(0x7a4a26),MT_LEATHER), boom=reg(HEX(0xc88a48),MT_LEATHER), boomd=reg(HEX(0x6a3a1a),MT_LEATHER), paint=reg(HEX(0xe8e0c8),MT_CLOTH);
  { float q[]={10,64, 16,46, 32,54, 48,46, 54,64}; R_poly(q,5,hoodd); } R_line(18,50,34,62,1.2f,leather);
  { float q[]={14,42, 18,16, 32,6, 46,16, 50,42, 32,48}; R_poly(q,6,hood); }
  R_ellipse(32,32,10,11,face); R_rect(23,24,27,40,hair); R_rect(37,24,41,40,hair);
  R_rect(27,31,28,32,eye); R_rect(36,31,37,32,eye); R_line(25,36,29,35,0.5f,paint); R_line(35,35,39,36,0.5f,paint);
  { float q[]={44,46, 60,38, 62,42, 50,50, 58,60, 54,62, 42,52}; R_poly(q,7,boom); } R_line(46,48,58,40,0.6f,boomd); R_line(46,52,55,60,0.6f,boomd);
}
/* v10 : le Geomancien, barbe de pierre et baton de cristal */
static void p_geomancien(void){
  port_rim=HEX(0xffb050);
  p_bust(HEX(0x6a4a2a),MT_CLOTH,HEX(0x3a2614));
  int robe=reg(HEX(0x8a5a2e),MT_CLOTH), robed=reg(HEX(0x5a3a1c),MT_CLOTH), face=reg(HEX(0xc89070),MT_FACE), beard=reg(HEX(0xd8d0c0),MT_HAIR);
  int hood=reg(HEX(0x5a3a22),MT_CLOTH), eye=reg(HEX(0x2a1a10),MT_CLOTH), stone=reg(HEX(0x8a8490),MT_METAL), glow=reg(HEX(0xffb040),MT_GLOW), rune=reg(HEX(0xffd070),MT_GLOW);
  { float q[]={10,64, 16,46, 32,54, 48,46, 54,64}; R_poly(q,5,robe); } R_line(32,50,32,64,0.8f,robed);
  { float q[]={14,40, 18,16, 32,8, 46,16, 50,40, 32,46}; R_poly(q,6,hood); }
  R_ellipse(32,30,10,10,face); R_rect(27,28,28,29,eye); R_rect(36,28,37,29,eye);
  { float q[]={22,34, 42,34, 40,48, 32,56, 24,48}; R_poly(q,5,beard); }
  R_line(52,64,56,12,1.6f,stone); R_ellipse(56,10,3.4f,4.2f,glow); rp(24,58,rune); rp(40,58,rune); rp(32,60,rune);
}
typedef void (*PFn)(void);
static const PFn PF[CH_COUNT]={p_sentinelle,p_alchimiste,p_jongleur,p_forgeronne,p_magnetiseur,p_necro,p_horlogere,p_pyromane,p_druide,p_marchand,p_astronome,p_voleuse,p_chasseresse,p_geomancien};

void portraits_init(void){
  for(int c=0;c<CH_COUNT;c++){
    memset(rmap,0,sizeof rmap); nrg=0; port_rim=HEX(0xff6aa8);
    PF[c]();
    PORT[c]=spr_alloc(PW,PW); paint(&PORT[c]);
    /* silhouette des heros verrouilles : ombre teintee, contour discret */
    PORT_LOCK[c]=spr_alloc(PW,PW);
    for(int i=0;i<PW*PW;i++){ uint32_t px=PORT[c].px[i]; if(!(px>>24)) continue; float l=lum(px); PORT_LOCK[c].px[i]=col_lerp(HEX(0x120c1a),HEX(0x2e2440),clampf(l*1.4f,0,1))|0xFF000000u; }
  }
}
const Sprite *portrait(int ch,bool locked){ ch=clampi(ch,0,CH_COUNT-1); return locked?&PORT_LOCK[ch]:&PORT[ch]; }
