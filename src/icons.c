/* GOUFFRE - icones realistes : generation en tache de fond, atlas, affichage.
   Les 60 icones (billes et objets) sont rendues en 3D par icongen.c dans un atlas
   de 1024x1024 (cases de 128 px) avec toute sa chaine de reductions.
   - rendu logiciel : l'icone est echantillonnee proprement (filtre de boite) a la
     taille voulue dans l'image 640x360 ;
   - rendu GPU : on dessine aussi cette version reduite, mais on note ou elle se trouve ;
     a la composition, la carte graphique la remplace par l'icone en pleine definition
     (a la resolution de l'ecran), sauf la ou quelque chose a ete dessine par-dessus
     (texte, curseur...). Un voile uniforme (menu de pause, fondu) est reproduit. */
#include "game.h"
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define AC 8
#define AR 11
#define AW (AC*ICON_N)
#define AH (AR*ICON_N)
#define NLEV 11

static uint8_t *ATL[NLEV]; static int ALW[NLEV], ALH[NLEV];
static atomic_int g_gen_done;      /* 1 quand l'atlas est complet (reductions comprises) */
static bool g_ready=false, g_started=false, g_threaded=false;
static int g_version=0, g_frame_id=0;
static int inc_id=0; static bool inc_begun=false;   /* generation etalee (sans tache de fond) */

static void build_mips(void){
  for(int l=1;l<NLEV;l++){
    int w=ALW[l], h=ALH[l], pw=ALW[l-1], ph=ALH[l-1]; uint8_t *d=ATL[l], *s=ATL[l-1];
    for(int y=0;y<h;y++) for(int x=0;x<w;x++){
      int x0=mini(x*2,pw-1), y0=mini(y*2,ph-1), x1=mini(x0+1,pw-1), y1=mini(y0+1,ph-1);
      for(int c=0;c<4;c++){ int v=s[(y0*pw+x0)*4+c]+s[(y0*pw+x1)*4+c]+s[(y1*pw+x0)*4+c]+s[(y1*pw+x1)*4+c]; d[(y*w+x)*4+c]=(uint8_t)((v+2)>>2); }
    }
  }
}
static inline uint8_t *cell0(int id){ return &ATL[0][((id/AC)*ICON_N*AW+(id%AC)*ICON_N)*4]; }
/* cache disque (dossier de la sauvegarde) : l'atlas n'est calcule qu'une fois par version.
   Changer ICON_GEN_VERSION des que le rendu des icones change. */
#define ICON_GEN_VERSION 4
_Static_assert(ICON_COUNT<=AC*AR,"atlas des icones trop petit");
static char g_cache[600]="";
static uint32_t fnv(const uint8_t *d,size_t n){ uint32_t h=2166136261u; for(size_t i=0;i<n;i++){ h^=d[i]; h*=16777619u; } return h; }
static bool disk_load(void){
  if(!g_cache[0]) return false;
  FILE *f=fopen(g_cache,"rb"); if(!f) return false;
  char mg[16]; uint32_t hd[4]; bool ok=fread(mg,1,16,f)==16 && fread(hd,4,4,f)==4 && !memcmp(mg,"GOUFFRE ICONES\n",16)
    && hd[0]==ICON_GEN_VERSION && hd[1]==(uint32_t)AW && hd[2]==(uint32_t)AH;
  if(ok) ok=fread(ATL[0],1,(size_t)AW*AH*4,f)==(size_t)AW*AH*4 && fnv(ATL[0],(size_t)AW*AH*4)==hd[3];
  fclose(f);
  return ok;
}
static void disk_save(void){
  if(!g_cache[0]) return;
  char tmp[620]; snprintf(tmp,sizeof tmp,"%s.tmp",g_cache);
  FILE *f=fopen(tmp,"wb"); if(!f) return;
  uint32_t hd[4]={ICON_GEN_VERSION,(uint32_t)AW,(uint32_t)AH,fnv(ATL[0],(size_t)AW*AH*4)};
  bool ok=fwrite("GOUFFRE ICONES\n",1,16,f)==16 && fwrite(hd,4,4,f)==4 && fwrite(ATL[0],1,(size_t)AW*AH*4,f)==(size_t)AW*AH*4;
  if(fclose(f)!=0) ok=false;
  if(ok){ remove(g_cache); if(rename(tmp,g_cache)!=0) remove(tmp); } else remove(tmp);
}
static void worker(void *arg){
  (void)arg;
  if(!disk_load()){
    for(int i=0;i<ICON_COUNT;i++) icg_render(i,cell0(i),AW*4);
    disk_save();
  }
  build_mips();
  atomic_store(&g_gen_done,1);
}

static bool alloc_atlas(void){
  for(int l=0;l<NLEV;l++){ ALW[l]=maxi(1,AW>>l); ALH[l]=maxi(1,AH>>l); ATL[l]=(uint8_t*)calloc((size_t)ALW[l]*ALH[l]*4,1); if(!ATL[l]) return false; }
  return true;
}
/* cache disque (tests sans tache de fond) : GOUFFRE_ICONES=chemin */
static bool cache_load(const char *p){
  FILE *f=p?fopen(p,"rb"):NULL; if(!f) return false;
  size_t n=fread(ATL[0],1,(size_t)AW*AH*4,f); fclose(f);
  return n==(size_t)AW*AH*4;
}
static void cache_save(const char *p){ FILE *f=p?fopen(p,"wb"):NULL; if(!f) return; fwrite(ATL[0],1,(size_t)AW*AH*4,f); fclose(f); }

void icons_init(void){
  if(g_started) return;
  g_started=true;
  if(!alloc_atlas()) return;
  atomic_store(&g_gen_done,0);
  /* fichier de cache a cote de la sauvegarde */
  { const char *sp=plat_save_path(); if(sp && strlen(sp)<sizeof g_cache-16){ snprintf(g_cache,sizeof g_cache,"%s",sp);
      char *e=strrchr(g_cache,'/'), *e2=strrchr(g_cache,'\\'); if(e2 && (!e || e2>e)) e=e2; if(e) snprintf(e+1,sizeof g_cache-(size_t)(e+1-g_cache),"icones.bin"); else g_cache[0]=0; } }
  if(plat_thread_start(worker,NULL)){ g_threaded=true; return; }
  g_cache[0]=0;   /* sans tache de fond (tests) : pas de cache automatique */
  /* pas de tache de fond : cache de test, sinon generation etalee sur les images */
  const char *cp=getenv("GOUFFRE_ICONES");
  if(cp && cache_load(cp)){ build_mips(); atomic_store(&g_gen_done,1); return; }
  if(cp || getenv("GOUFFRE_ICONES_SYNC")){ worker(NULL); if(cp) cache_save(cp); }
}
void icons_frame(void){
  g_frame_id++;
  if(g_ready || !g_started || !ATL[0]) return;
  if(atomic_load(&g_gen_done)){ g_ready=true; g_version++; return; }
  if(g_threaded) return;
  /* generation etalee : environ 4 ms par image */
  clock_t t0=clock();
  while(inc_id<ICON_COUNT){
    if(!inc_begun){ icg_begin(inc_id); inc_begun=true; }
    if(icg_rows(8)){ icg_finish(cell0(inc_id),AW*4); inc_begun=false; inc_id++; }
    if((clock()-t0)*1000>=4*CLOCKS_PER_SEC) break;
  }
  if(inc_id>=ICON_COUNT){ build_mips(); atomic_store(&g_gen_done,1); }
}
bool icons_ready(void){ return g_ready; }
int icons_frame_id(void){ return g_frame_id; }
int icons_atlas_version(void){ return g_ready?g_version:0; }
const uint8_t *icons_atlas_level(int l,int *w,int *h){ if(l<0||l>=NLEV||!g_ready) return NULL; *w=ALW[l]; *h=ALH[l]; return ATL[l]; }
int icons_atlas_levels(void){ return NLEV; }

/* ---------------- dessin ---------------- */
/* lecture bilineaire d'un niveau, bornee a la case de l'icone */
static inline void bil(int l,int id,float u,float v,float *o){
  int cs=ICON_N>>l; if(cs<1) cs=1; int w=ALW[l];
  int bx=(id%AC)*cs, by=(id/AC)*cs;
  float x=u*cs-0.5f, y=v*cs-0.5f; int x0=(int)floorf(x), y0=(int)floorf(y); float fx=x-x0, fy=y-y0;
  int xa=clampi(x0,0,cs-1), xb=clampi(x0+1,0,cs-1), ya=clampi(y0,0,cs-1), yb=clampi(y0+1,0,cs-1);
  const uint8_t *p=ATL[l];
  const uint8_t *a=&p[((by+ya)*w+bx+xa)*4], *b=&p[((by+ya)*w+bx+xb)*4], *c=&p[((by+yb)*w+bx+xa)*4], *d=&p[((by+yb)*w+bx+xb)*4];
  for(int k=0;k<4;k++) o[k]=(a[k]*(1-fx)+b[k]*fx)*(1-fy)+(c[k]*(1-fx)+d[k]*fx)*fy;
}
static void draw_lowres(int id,float x0,float y0,float sz,int cx0,int cy0,int cx1,int cy1,const DrawOpt *o){
  int l=0; while(l<4 && (ICON_N>>(l+1))>=sz) l++;   /* plus petit niveau encore plus grand que l'icone */
  float fp=(ICON_N>>l)/sz;                           /* texels par pixel */
  float off=fp>1.2f?0.25f:0.0f;
  float al=(o->alpha>=255?1.0f:o->alpha/255.0f);
  float fa=o->flash_a>0?minf(o->flash_a,255)/255.0f:0; float fr=cr(o->flash), fg=cg(o->flash), fb=cb(o->flash);
  bool tint=o->tint!=0xFFFFFFFFu; float tr=cr(o->tint)/255.0f, tg=cg(o->tint)/255.0f, tb=cb(o->tint)/255.0f;
  for(int py=cy0;py<cy1;py++){
    uint32_t *row=&gT->px[py*gT->w];
    for(int px=cx0;px<cx1;px++){
      float u=(px+0.5f-x0)/sz, v=(py+0.5f-y0)/sz;
      float s[4]={0,0,0,0}, t[4];
      if(off>0){ float du=off/sz, dv=off/sz;
        bil(l,id,u-du,v-dv,t); for(int k=0;k<4;k++) s[k]+=t[k];
        bil(l,id,u+du,v-dv,t); for(int k=0;k<4;k++) s[k]+=t[k];
        bil(l,id,u-du,v+dv,t); for(int k=0;k<4;k++) s[k]+=t[k];
        bil(l,id,u+du,v+dv,t); for(int k=0;k<4;k++) s[k]+=t[k];
        for(int k=0;k<4;k++) s[k]*=0.25f; }
      else bil(l,id,u,v,s);
      float a=s[3]/255.0f; if(a<=0.002f && s[0]+s[1]+s[2]<1) continue;
      float r=s[0], g=s[1], b=s[2];
      if(fa>0){ r=r+(fr*a-r)*fa; g=g+(fg*a-g)*fa; b=b+(fb*a-b)*fa; }
      if(tint){ r*=tr; g*=tg; b*=tb; }
      r*=al; g*=al; b*=al; a*=al;
      uint32_t d=row[px]; float ia=1-a;
      int nr=(int)(r+((d>>16)&255)*ia+0.5f), ng=(int)(g+((d>>8)&255)*ia+0.5f), nb=(int)(b+(d&255)*ia+0.5f);
      if(nr>255) nr=255; if(ng>255) ng=255; if(nb>255) nb=255;
      uint32_t na = gT->alpha ? (uint32_t)clampi((int)(a*255+(d>>24)*ia+0.5f),0,255) : 255u;
      row[px]=(na<<24)|((uint32_t)nr<<16)|((uint32_t)ng<<8)|(uint32_t)nb;
    }
  }
}

/* ---------------- icones a composer par le GPU ---------------- */
typedef struct { float x0,y0,sz; int id; int rx0,ry0,rx1,ry1; int off; uint32_t flash,tint; int flash_a,alpha; } Inst;
#define MAX_INST 255
#define POOL_N (600*1024)
static Inst INST[MAX_INST]; static int NINST=0, inst_frame=-1;
static uint32_t *POOL; static int pool_used=0;

/* en attendant l'atlas : les anciennes icones en pixels, a l'echelle entiere la plus proche */
static void old_icon(int id,float cx,float cy,float sz,const DrawOpt *o){
  DrawOpt d=*o; d.flags|=DS_NOOCC; float k=(float)maxi(1,(int)(sz/16.0f+0.25f)); d.sx=d.sy=k;
  if(id<BK_COUNT) draw_spr_c(&SPR_BALL[id],cx,cy,&d);
  else draw_spr_c(&SPR[PASSIVES[id-BK_COUNT].sprite],cx,cy,&d);
}

void icon_draw(int id,float cx,float cy,float sz,const DrawOpt *o){
  DrawOpt d=o?*o:dopt();
  if(id<0||id>=ICON_COUNT||sz<=0) return;
  if(!g_ready){ old_icon(id,cx,cy,sz,&d); return; }
  float x0=cx-sz*0.5f, y0=cy-sz*0.5f;
  int rx0=(int)floorf(x0), ry0=(int)floorf(y0), rx1=(int)ceilf(x0+sz), ry1=(int)ceilf(y0+sz);
  int cx0=maxi(rx0,gT->cx0), cy0=maxi(ry0,gT->cy0), cx1=mini(rx1,gT->cx1), cy1=mini(ry1,gT->cy1);
  if(cx1<=cx0||cy1<=cy0) return;
  Inst *in=NULL;
  if(gfx_target_is_ui() && gpu_icons_active()){
    if(inst_frame!=g_frame_id){ inst_frame=g_frame_id; NINST=0; pool_used=0; }
    int area=(cx1-cx0)*(cy1-cy0);
    if(!POOL) POOL=(uint32_t*)malloc(POOL_N*sizeof(uint32_t));
    if(POOL && NINST<MAX_INST && pool_used+area*2<=POOL_N){
      in=&INST[NINST++]; in->x0=x0; in->y0=y0; in->sz=sz; in->id=id; in->rx0=cx0; in->ry0=cy0; in->rx1=cx1; in->ry1=cy1;
      in->off=pool_used; pool_used+=area*2; in->flash=d.flash; in->flash_a=d.flash_a; in->tint=d.tint; in->alpha=d.alpha;
      int w=cx1-cx0; for(int y=cy0;y<cy1;y++) memcpy(&POOL[in->off+(y-cy0)*w],&gT->px[y*gT->w+cx0],(size_t)w*4);
    }
  }
  draw_lowres(id,x0,y0,sz,cx0,cy0,cx1,cy1,&d);
  if(in){ int w=cx1-cx0, area=w*(cy1-cy0); for(int y=cy0;y<cy1;y++) memcpy(&POOL[in->off+area+(y-cy0)*w],&gT->px[y*gT->w+cx0],(size_t)w*4); }
}

/* Avant l'envoi de l'interface au GPU : pour chaque icone (de la derniere dessinee a la premiere),
   on compare l'image finale a ce qu'elle etait juste apres l'icone. La ou rien n'a change
   (ou seulement un voile uniforme), on efface la version reduite (on remet le fond d'avant)
   et on marque le pixel : le GPU y dessinera l'icone en pleine definition.
   mask : 2 octets par pixel (numero de l'icone, voile 0..255) ; idata : 256x4 texels RGBA. */
static inline int lum3(uint32_t c){ return (int)(((c>>16)&255)*77+((c>>8)&255)*150+(c&255)*29)>>8; }
int icons_gpu_prepare(uint8_t *mask,uint8_t *idata){
  if(inst_frame!=g_frame_id || NINST==0) return 0;
  memset(mask,0,(size_t)SCREEN_W*SCREEN_H*2);
  for(int i=NINST-1;i>=0;i--){
    Inst *in=&INST[i]; int w=in->rx1-in->rx0, h=in->ry1-in->ry0, area=w*h;
    const uint32_t *pre=&POOL[in->off], *post=&POOL[in->off+area];
    /* voile eventuel : rapport de luminosite le plus frequent sur les pixels changes */
    int hist[65]; memset(hist,0,sizeof hist); int nch=0;
    for(int y=0;y<h;y++) for(int x=0;x<w;x++){ uint32_t F=g_ui[(in->ry0+y)*SCREEN_W+in->rx0+x], P=post[y*w+x]; if(F==P) continue; nch++;
      int lp=lum3(P); if(lp<28) continue; float r=(float)lum3(F)/lp; if(r>1.03f) continue; hist[clampi((int)(r*64+0.5f),0,64)]++; }
    float kv=-1; { int best=0, bi=-1; for(int b=0;b<=64;b++) if(hist[b]>best){ best=hist[b]; bi=b; } if(bi>=0 && best>=6 && best*3>=nch) kv=bi/64.0f; }
    for(int y=0;y<h;y++) for(int x=0;x<w;x++){
      int idx=(in->ry0+y)*SCREEN_W+in->rx0+x;
      if(mask[idx*2]) continue;
      mask[idx*2]=(uint8_t)(i+1);
      uint32_t F=g_ui[idx], P=post[y*w+x], B=pre[y*w+x];
      float k;
      if(F==P) k=1;
      else if(kv>=0){
        bool ok=true;
        for(int c=0;c<24 && ok;c+=8){ float f=(F>>c)&255, p=(P>>c)&255; if(fabsf(f-p*kv)>10+0.06f*p) ok=false; }
        { float fa=F>>24, pa=P>>24; if(fabsf(fa-(pa*kv+255*(1-kv)))>12) ok=false; }
        if(!ok) continue;
        k=kv;
      } else continue;
      uint32_t o=0;
      for(int c=0;c<32;c+=8){ float v=((F>>c)&255)+(((float)((B>>c)&255))-((P>>c)&255))*k; o|=(uint32_t)clampi((int)(v+0.5f),0,255)<<c; }
      g_ui[idx]=o;
      mask[idx*2+1]=(uint8_t)clampi((int)(k*255+0.5f),1,255);
    }
  }
  /* donnees des icones : 4 lignes de 256 texels */
  memset(idata,0,256*4*4);
  for(int i=0;i<NINST;i++){
    Inst *in=&INST[i]; int t=i+1;
    int vx=clampi((int)((in->x0+512)*16+0.5f),0,65535), vy=clampi((int)((in->y0+512)*16+0.5f),0,65535), vs=clampi((int)(in->sz*16+0.5f),0,65535);
    uint8_t *r0=&idata[(0*256+t)*4], *r1=&idata[(1*256+t)*4], *r2=&idata[(2*256+t)*4], *r3=&idata[(3*256+t)*4];
    r0[0]=(uint8_t)(vx>>8); r0[1]=(uint8_t)(vx&255); r0[2]=(uint8_t)(vy>>8); r0[3]=(uint8_t)(vy&255);
    r1[0]=(uint8_t)(vs>>8); r1[1]=(uint8_t)(vs&255); r1[2]=(uint8_t)in->id; r1[3]=(uint8_t)clampi(in->alpha,0,255);
    r2[0]=(uint8_t)cr(in->flash); r2[1]=(uint8_t)cg(in->flash); r2[2]=(uint8_t)cb(in->flash); r2[3]=(uint8_t)clampi(in->flash_a,0,255);
    r3[0]=(uint8_t)cr(in->tint); r3[1]=(uint8_t)cg(in->tint); r3[2]=(uint8_t)cb(in->tint); r3[3]=255;
  }
  return NINST;
}
