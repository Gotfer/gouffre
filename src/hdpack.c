/* GOUFFRE - textures HD precalculees (dossier « donnees » a cote du jeu).
   Chaque fichier .gx contient d'abord une version basse definition (sol, flancs, parois du puits,
   ou fond du campement) que le processeur dessine comme avant, puis l'atlas HD (couleur + lumiere
   propre, normales + brillance) que la carte graphique applique a pleine resolution.
   v9 : quatre jeux par niveau (les trois parties du niveau, puis l'arene du gardien), quatre campements
   (un par saison), atlas jusqu'a 6 texels par pixel du jeu (4K). Le jeu suivant est prepare en tache de fond.
   Sans le dossier, le jeu fabrique ses decors comme avant (procedural). */
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "game.h"
#include <stdio.h>
#include <stdatomic.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#endif

#define HD_MAXR 6
typedef struct { int id,gw,gh,ax,lx; } HReg;
typedef struct {
  int key; int kind, S, nreg; HReg reg[HD_MAXR];
  uint32_t *alb[HD_MAXR], *nrm[HD_MAXR]; uint8_t *em[HD_MAXR], *mv[HD_MAXR];
  int ng; float gx[96],gy[96],gr[96]; uint32_t gc[96]; int greg[96];
  int aw,ah,lw,lh; long hd_off;
} HFile;

static char hd_dir[600]; static int hd_state=-1;   /* -1 inconnu, 0 absent, 1 present */
static int hd_var=0;                                 /* partie du niveau : 0..2, 3 = arene du gardien */
static void dir_init(void){
  if(hd_state>=0) return;
  const char *e=getenv("GOUFFRE_DONNEES");
  if(e && *e) snprintf(hd_dir,sizeof hd_dir,"%s",e);
  else {
#ifdef _WIN32
    char p[MAX_PATH]; DWORD n=GetModuleFileNameA(NULL,p,MAX_PATH);
    if(n>0 && n<MAX_PATH){ char *s=strrchr(p,'\\'); if(s) *s=0; snprintf(hd_dir,sizeof hd_dir,"%s\\donnees",p); }
    else snprintf(hd_dir,sizeof hd_dir,"donnees");
#else
    snprintf(hd_dir,sizeof hd_dir,"donnees");
#endif
  }
  hd_state=0;
  static const char *probe[3]={"niveau_01.gx","campement_0.gx","campement.gx"};
  for(int i=0;i<3 && !hd_state;i++){ char p[700]; snprintf(p,sizeof p,"%s/%s",hd_dir,probe[i]); FILE *f=fopen(p,"rb"); if(f){ hd_state=1; fclose(f); } }
  { const char *off=getenv("GOUFFRE_SANS_HD"); if(off && *off=='1') hd_state=0; }
}
bool hd_available(void){ dir_init(); return hd_state==1; }
const char *hd_data_dir(void){ dir_init(); return hd_dir; }
void hd_set_variant(int v){ hd_var=clampi(v,0,3); }
int  hd_get_variant(void){ return hd_var; }
int  hd_level_key(int biome){ return clampi(biome,0,15)+16*hd_var; }
static int eff_key(int key){ return key<16 ? key+16*hd_var : key; }   /* un numero de niveau seul : la partie en cours */

/* nom du fichier ; les jeux absents retombent sur la premiere partie du niveau / le campement d'origine */
static void name_of(int key,char *out,int n){
  if(key>=HD_CAMP){ snprintf(out,n,"%s/campement_%d.gx",hd_dir,key-HD_CAMP); FILE *f=fopen(out,"rb"); if(f){ fclose(f); return; } snprintf(out,n,"%s/campement.gx",hd_dir); return; }
  int b=key%16, v=key/16;
  if(v>0){ snprintf(out,n,"%s/niveau_%02d_%d.gx",hd_dir,b+1,v); FILE *f=fopen(out,"rb"); if(f){ fclose(f); return; } }
  snprintf(out,n,"%s/niveau_%02d.gx",hd_dir,b+1);
}
static uint32_t rd32(FILE *f){ uint32_t v=0; if(fread(&v,4,1,f)!=1) v=0; return v; }

static void file_free(HFile *h){ for(int r=0;r<h->nreg;r++){ free(h->alb[r]); free(h->nrm[r]); free(h->em[r]); free(h->mv[r]); h->alb[r]=h->nrm[r]=NULL; h->em[r]=h->mv[r]=NULL; } h->nreg=0; }
/* lecture de l'en-tete et de la basse definition (sans etat partage : utilisable en tache de fond) */
static bool file_read(int key,HFile *h){
  memset(h,0,sizeof *h); h->key=key;
  char p[700]; name_of(key,p,sizeof p); FILE *f=fopen(p,"rb"); if(!f) return false;
  char m[4]; bool ok= fread(m,1,4,f)==4 && !memcmp(m,"GXH1",4) && rd32(f)==1;
  if(ok){ h->kind=(int)rd32(f); h->S=(int)rd32(f); h->nreg=(int)rd32(f); ok= h->nreg>0 && h->nreg<=HD_MAXR && h->S>=1 && h->S<=8; if(!ok) h->nreg=0; }
  for(int r=0;ok && r<h->nreg;r++){ h->reg[r].id=(int)rd32(f); h->reg[r].gw=(int)rd32(f); h->reg[r].gh=(int)rd32(f); h->reg[r].ax=(int)rd32(f); h->reg[r].lx=(int)rd32(f);
    if(h->reg[r].gw<=0||h->reg[r].gh<=0||h->reg[r].gw>4096||h->reg[r].gh>4096) ok=false; }
  if(ok){ h->aw=(int)rd32(f); h->ah=(int)rd32(f); h->lw=(int)rd32(f); h->lh=(int)rd32(f); ok= h->aw>0 && h->ah>0 && h->aw<=32768 && h->ah<=16384; }
  for(int r=0;ok && r<h->nreg;r++){ size_t n=(size_t)h->reg[r].gw*h->reg[r].gh;
    h->alb[r]=(uint32_t*)malloc(n*4); h->em[r]=(uint8_t*)malloc(n); h->nrm[r]=(uint32_t*)malloc(n*4); h->mv[r]=(uint8_t*)malloc(n);
    ok= h->alb[r] && h->em[r] && h->nrm[r] && h->mv[r] && fread(h->alb[r],4,n,f)==n && fread(h->em[r],1,n,f)==n && fread(h->nrm[r],4,n,f)==n && fread(h->mv[r],1,n,f)==n; }
  if(ok){ int n=(int)rd32(f); h->ng=0;
    for(int i=0;i<n;i++){ float a[3]; uint32_t c,rg; if(fread(a,4,3,f)!=3){ ok=false; break; } c=rd32(f); rg=rd32(f);
      if(h->ng<96){ h->gx[h->ng]=a[0]; h->gy[h->ng]=a[1]; h->gr[h->ng]=a[2]; h->gc[h->ng]=c; h->greg[h->ng]=(int)rg; h->ng++; } }
    h->hd_off=ftell(f); }
  fclose(f);
  if(!ok){ file_free(h); return false; }
  return true;
}

/* ---------------- basse definition (cache d'un fichier, fil principal) ---------------- */
static HFile lowc; static bool lowc_ok=false;
static bool low_load(int key){
  if(!hd_available()) return false;
  key=eff_key(key);
  if(lowc_ok && lowc.key==key) return true;
  if(lowc_ok){ file_free(&lowc); lowc_ok=false; }
  lowc_ok=file_read(key,&lowc);
  return lowc_ok;
}
static int reg_index(int id){ for(int r=0;r<lowc.nreg;r++) if(lowc.reg[r].id==id) return r; return -1; }

/* sol du puits : couleur, codes de lumiere propre, normales + lumieres */
bool hd_floor_low(int biome,uint32_t *alb,uint8_t *em,uint32_t *nrm,int w,int h,void (*glow)(float,float,uint32_t,float)){
  if(!low_load(biome)) return false;
  int r=reg_index(0); if(r<0 || lowc.reg[r].gw!=w || lowc.reg[r].gh!=h) return false;
  size_t n=(size_t)w*h; memcpy(alb,lowc.alb[r],n*4); memcpy(em,lowc.em[r],n); memcpy(nrm,lowc.nrm[r],n*4);
  for(int i=0;i<lowc.ng;i++) if(lowc.greg[i]==0 && glow) glow(lowc.gx[i],lowc.gy[i],lowc.gc[i],lowc.gr[i]);
  return true;
}
/* flancs (id 1 gauche, 2 droit) et parois (3, 4), fond du campement (5) */
bool hd_region_low(int key,int id,uint32_t *alb,uint8_t *em,uint32_t *nrm,int w,int h){
  if(!low_load(key)) return false;
  int r=reg_index(id); if(r<0 || lowc.reg[r].gw!=w || lowc.reg[r].gh!=h) return false;
  size_t n=(size_t)w*h; if(alb) memcpy(alb,lowc.alb[r],n*4); if(em) memcpy(em,lowc.em[r],n); if(nrm) memcpy(nrm,lowc.nrm[r],n*4);
  return true;
}
int hd_glows(int key,int reg,float *x,float *y,float *rr,uint32_t *c,int max){
  if(!low_load(key)) return 0;
  int n=0; for(int i=0;i<lowc.ng && n<max;i++) if(lowc.greg[i]==reg){ x[n]=lowc.gx[i]; y[n]=lowc.gy[i]; rr[n]=lowc.gr[i]; c[n]=lowc.gc[i]; n++; }
  return n;
}

/* ---------------- jeu de textures HD pour la carte graphique ---------------- */
typedef struct {
  int key, aw, ah, lw, lh, nlev; int S;
  uint8_t *A[16], *N[16]; int lw_[16], lh_[16];   /* niveaux de mip */
  uint8_t *L;                                       /* basse definition : rvb + couverture de lumiere propre */
  HReg reg[HD_MAXR]; int nreg;
} HSet;
static HSet cur_set={.key=-1}, pre_set={.key=-1};
static int want_key=-1, set_ready=0; static int set_version=0;
static atomic_int pre_state;          /* 0 libre, 1 en cours, 2 pret */
static atomic_int pre_key = -1;
static void set_free(HSet *s){ for(int l=0;l<s->nlev;l++){ free(s->A[l]); free(s->N[l]); } free(s->L); memset(s,0,sizeof *s); s->key=-1; }
static uint8_t *half(const uint8_t *src,int w,int h,int *ow,int *oh){
  int W=w>1?w/2:1, H=h>1?h/2:1; uint8_t *d=(uint8_t*)malloc((size_t)W*H*4); if(!d){ *ow=*oh=0; return NULL; }
  for(int y=0;y<H;y++) for(int x=0;x<W;x++) for(int c=0;c<4;c++){
    int x0=mini(x*2,w-1), x1=mini(x*2+1,w-1), y0=mini(y*2,h-1), y1=mini(y*2+1,h-1);
    int s=src[((size_t)y0*w+x0)*4+c]+src[((size_t)y0*w+x1)*4+c]+src[((size_t)y1*w+x0)*4+c]+src[((size_t)y1*w+x1)*4+c];
    d[((size_t)y*W+x)*4+c]=(uint8_t)((s+2)>>2); }
  *ow=W; *oh=H; return d;
}
static bool set_build(int key,HSet *s){
  HFile h; if(!file_read(key,&h)) return false;
  set_free(s); s->key=key; s->aw=h.aw; s->ah=h.ah; s->lw=h.lw; s->lh=h.lh; s->S=h.S; s->nreg=h.nreg; memcpy(s->reg,h.reg,sizeof s->reg);
  char p[700]; name_of(key,p,sizeof p); FILE *f=fopen(p,"rb"); if(!f){ file_free(&h); return false; }
  size_t n=(size_t)s->aw*s->ah*4; s->A[0]=(uint8_t*)malloc(n); s->N[0]=(uint8_t*)malloc(n);
  bool ok= s->A[0] && s->N[0] && fseek(f,h.hd_off,SEEK_SET)==0 && fread(s->A[0],1,n,f)==n && fread(s->N[0],1,n,f)==n;
  fclose(f);
  if(!ok){ s->nlev=1; set_free(s); file_free(&h); return false; }
  s->lw_[0]=s->aw; s->lh_[0]=s->ah; s->nlev=1;
  while(s->nlev<16 && (s->lw_[s->nlev-1]>1 || s->lh_[s->nlev-1]>1)){ int l=s->nlev;
    s->A[l]=half(s->A[l-1],s->lw_[l-1],s->lh_[l-1],&s->lw_[l],&s->lh_[l]); s->N[l]=half(s->N[l-1],s->lw_[l-1],s->lh_[l-1],&s->lw_[l],&s->lh_[l]); s->nlev++;
    if(!s->A[l]||!s->N[l]) break; }
  /* basse definition en atlas : couleur + couverture de lumiere propre (pour repartir la lumiere en HD) */
  s->L=(uint8_t*)calloc((size_t)s->lw*s->lh,4);
  if(s->L) for(int r=0;r<h.nreg;r++){ const HReg *g=&h.reg[r];
    for(int y=0;y<s->lh;y++) for(int x=-2;x<g->gw+2;x++){ int X=g->lx+x; if(X<0||X>=s->lw) continue; int sx=clampi(x,0,g->gw-1), sy=y%g->gh;
      uint32_t c=h.alb[r][sy*g->gw+sx]; uint8_t *d=&s->L[((size_t)y*s->lw+X)*4]; d[0]=(uint8_t)(c>>16); d[1]=(uint8_t)(c>>8); d[2]=(uint8_t)c; d[3]=h.mv[r][sy*g->gw+sx]; } }
  file_free(&h);
  return s->L!=NULL;
}
/* preparation en tache de fond (lecture du disque et reductions) */
static void pre_worker(void *arg){ (void)arg; int k=atomic_load(&pre_key); bool ok=set_build(k,&pre_set); atomic_store(&pre_state,ok?2:0); }
static void prefetch_key(int key){
  if(set_ready && cur_set.key==key) return;
  int st=atomic_load(&pre_state); if(st==1) return; if(st==2 && atomic_load(&pre_key)==key) return;
  if(st==2){ set_free(&pre_set); }
  atomic_store(&pre_key,key); atomic_store(&pre_state,1);
  if(!plat_thread_start(pre_worker,NULL)){ bool ok=set_build(key,&pre_set); atomic_store(&pre_state,ok?2:0); }
}
void hd_gpu_prefetch(int key){ if(!g_gpu_ok || !hd_available()) return; prefetch_key(eff_key(key)); }
/* niveau choisi dans la liste : on prepare son decor pendant le choix du heros */
void hd_gpu_prefetch_level(int biome,int var){ if(!g_gpu_ok || !hd_available()) return; prefetch_key(clampi(biome,0,15)+16*clampi(var,0,3)); }
static void sleep_ms(int ms){
#ifdef _WIN32
  Sleep((DWORD)ms);
#else
  struct timespec t={0,(long)ms*1000000L}; nanosleep(&t,NULL);
#endif
}
/* choix du decor HD affiche (niveau + 16 x partie, HD_CAMP + saison, -1 = aucun) */
void hd_gpu_select(int key){
  if(!g_gpu_ok || !hd_available()){ want_key=-1; set_ready=0; return; }
  if(key<0){ want_key=-1; return; }
  key=eff_key(key);
  if(key==cur_set.key && set_ready){ want_key=key; return; }
  if(atomic_load(&pre_key)==key){ while(atomic_load(&pre_state)==1) sleep_ms(2); }
  if(atomic_load(&pre_state)==2 && atomic_load(&pre_key)==key){
    set_free(&cur_set); cur_set=pre_set; memset(&pre_set,0,sizeof pre_set); pre_set.key=-1; atomic_store(&pre_state,0); atomic_store(&pre_key,-1);
    want_key=key; set_ready=1; set_version++; return; }
  want_key=key; set_ready= set_build(key,&cur_set) ? 1 : 0; if(!set_ready) want_key=-1; else set_version++;
}
bool hd_gpu_active(int key){ key=eff_key(key); return set_ready && want_key==key && cur_set.key==key; }
int  hd_gpu_key(void){ return set_ready ? want_key : -1; }
/* donnees pour gpu.c */
int hd_gpu_version(void){ return set_ready ? set_version : 0; }
int hd_gpu_levels(void){ return cur_set.nlev; }
const uint8_t *hd_gpu_level(int which,int l,int *w,int *h){ *w=cur_set.lw_[l]; *h=cur_set.lh_[l]; return which==0 ? cur_set.A[l] : cur_set.N[l]; }
const uint8_t *hd_gpu_low(int *w,int *h){ *w=cur_set.lw; *h=cur_set.lh; return cur_set.L; }
void hd_gpu_release_cpu(void){   /* une fois envoyees a la carte graphique, les donnees HD ne servent plus */
  for(int l=0;l<cur_set.nlev;l++){ free(cur_set.A[l]); free(cur_set.N[l]); cur_set.A[l]=cur_set.N[l]=NULL; }
}
/* correspondance code de region -> texture : M = (sens x, decalage x, largeur, defilement), P = (x atlas HD, x atlas bas, hauteur, 0) */
static int fr_scroll=0, fr_sh=0;
void hd_frame(int scroll,int sh){ fr_scroll=scroll; fr_sh=sh; }
void hd_gpu_params(float M[8][4],float P[8][4],float G[4],float *scroll){
  memset(M,0,sizeof(float)*32); memset(P,0,sizeof(float)*32);
  for(int r=0;r<cur_set.nreg;r++){ const HReg *g=&cur_set.reg[r]; int code=g->id+1; if(code<1||code>7) continue;
    float a=1,b=0,sc=1;
    switch(g->id){
      case 0: b=-(float)PIT_X0; break;
      case 1: b=(float)fr_sh; break;
      case 2: b=-(float)(PIT_X1+fr_sh); break;
      case 3: a=-1; b=(float)PIT_X0; break;
      case 4: b=-(float)PIT_X1; break;
      default: sc=0; break;
    }
    M[code][0]=a; M[code][1]=b; M[code][2]=(float)g->gw; M[code][3]=sc;
    P[code][0]=(float)g->ax/cur_set.S; P[code][1]=(float)g->lx; P[code][2]=(float)g->gh;
    if(g->id==5){ for(int k=0;k<4;k++){ M[7][k]=M[6][k]; P[7][k]=P[6][k]; } }
  }
  G[0]=(float)cur_set.aw/cur_set.S; G[1]=(float)cur_set.ah/cur_set.S; G[2]=(float)cur_set.lw; G[3]=(float)cur_set.lh;
  *scroll=(float)fr_scroll;
}
/* saison du campement (date du PC) : 0 hiver, 1 printemps, 2 ete, 3 automne */
int hd_season(void){
  const char *e=getenv("GOUFFRE_SAISON"); if(e && *e) return clampi(atoi(e),0,3);
  time_t t=time(NULL); struct tm *tm=localtime(&t); int m= tm ? tm->tm_mon : 10;
  return m==11||m<=1 ? 0 : (m<=4 ? 1 : (m<=7 ? 2 : 3));
}
