#define _POSIX_C_SOURCE 199309L
#include <time.h>
/* GOUFFRE - banc de test sans fenetre (Linux) : pilote automatique + captures */
#include "game.h"

static char save_path[512]="test_sauvegarde.txt";
const char *plat_save_path(void){ return save_path; }
static int rumble_calls=0;
void plat_rumble(float strength,float seconds){ (void)strength; (void)seconds; rumble_calls++; }
extern bool g_bot_pad;
void game_debug_scene(const char *name,int arg);

bool plat_thread_start(void (*fn)(void*),void *arg){ (void)fn; (void)arg; return false; }
static void write_ppm(const char *fn,const uint32_t *fb){
  FILE *f=fopen(fn,"wb"); if(!f) return;
  fprintf(f,"P6\n%d %d\n255\n",SCREEN_W,SCREEN_H);
  for(int i=0;i<SCREEN_W*SCREEN_H;i++){ unsigned char c[3]={(unsigned char)(fb[i]>>16),(unsigned char)(fb[i]>>8),(unsigned char)fb[i]}; fwrite(c,1,3,f); }
  fclose(f);
}
static const char *SNAMES[]={"titre","heros","camp","partie","resultats","options","defis-codex","cinematique","niveaux","ameliorations","entrainement","aide"};

/* ---------------- rendu GPU hors ecran (EGL sans surface, Mesa) ---------------- */
#include <dlfcn.h>
typedef void *EGLDisplay_, *EGLConfig_, *EGLContext_, *EGLSurface_;
static void *(*egl_gpa)(const char*);
static void *egl_proc(const char *n){ return egl_gpa(n); }
static bool egl_start(char *info,int n){
  void *h=dlopen("libEGL.so.1",RTLD_NOW); if(!h){ snprintf(info,n,"libEGL absent"); return false; }
  egl_gpa=(void*(*)(const char*))dlsym(h,"eglGetProcAddress"); if(!egl_gpa) return false;
  EGLDisplay_ (*getdisp)(unsigned,void*,const int*)=(EGLDisplay_(*)(unsigned,void*,const int*))egl_gpa("eglGetPlatformDisplayEXT");
  unsigned (*init)(EGLDisplay_,int*,int*)=(unsigned(*)(EGLDisplay_,int*,int*))dlsym(h,"eglInitialize");
  unsigned (*bindapi)(unsigned)=(unsigned(*)(unsigned))dlsym(h,"eglBindAPI");
  unsigned (*choose)(EGLDisplay_,const int*,EGLConfig_*,int,int*)=(unsigned(*)(EGLDisplay_,const int*,EGLConfig_*,int,int*))dlsym(h,"eglChooseConfig");
  EGLContext_ (*cctx)(EGLDisplay_,EGLConfig_,EGLContext_,const int*)=(EGLContext_(*)(EGLDisplay_,EGLConfig_,EGLContext_,const int*))dlsym(h,"eglCreateContext");
  unsigned (*mk)(EGLDisplay_,EGLSurface_,EGLSurface_,EGLContext_)=(unsigned(*)(EGLDisplay_,EGLSurface_,EGLSurface_,EGLContext_))dlsym(h,"eglMakeCurrent");
  if(!getdisp||!init||!bindapi||!choose||!cctx||!mk){ snprintf(info,n,"EGL incomplet"); return false; }
  EGLDisplay_ d=getdisp(0x31DD,NULL,NULL); int ma=0,mi=0; if(!d || !init(d,&ma,&mi)){ snprintf(info,n,"eglInitialize"); return false; }
  bindapi(0x30A2);
  int attr[]={0x3040,0x0008,0x3033,0,0x3038}; EGLConfig_ cfg=NULL; int nc=0; choose(d,attr,&cfg,1,&nc);
  EGLContext_ c=cctx(d,nc?cfg:NULL,NULL,NULL); if(!c){ snprintf(info,n,"eglCreateContext"); return false; }
  if(!mk(d,NULL,NULL,c)){ snprintf(info,n,"eglMakeCurrent"); return false; }
  return true;
}
static void write_ppm_rgb(const char *fn,const uint8_t *rgb,int w,int h){
  FILE *f=fopen(fn,"wb"); if(!f) return; fprintf(f,"P6\n%d %d\n255\n",w,h); fwrite(rgb,1,(size_t)w*h*3,f); fclose(f);
}

int main(int argc,char **argv){
  int gpu_w=0, gpu_h=0; uint8_t *gpu_rgb=NULL; unsigned gpu_fbo=0; int quality=-1;
  int frames=600, every=0; bool bot=false; const char *dump=NULL, *scene=NULL; int arg=0; const char *at=NULL;
  int max_runs=0, runs_done=0; bool want_intro=false; int ks_key[64], ks_frame[64], nks=0;
  float mx=320,my=120; int status_every=1800; bool weak_boss=false; bool pad=false; bool padhold=false; const char *wav=NULL; int keys_at=-1; int key_id=-1;
  for(int i=1;i<argc;i++){
    if(!strcmp(argv[i],"--frames")&&i+1<argc) frames=atoi(argv[++i]);
    else if(!strcmp(argv[i],"--bot")) bot=true;
    else if(!strcmp(argv[i],"--dump")&&i+1<argc) dump=argv[++i];
    else if(!strcmp(argv[i],"--every")&&i+1<argc) every=atoi(argv[++i]);
    else if(!strcmp(argv[i],"--at")&&i+1<argc) at=argv[++i];
    else if(!strcmp(argv[i],"--scene")&&i+1<argc) scene=argv[++i];
    else if(!strcmp(argv[i],"--arg")&&i+1<argc) arg=atoi(argv[++i]);
    else if(!strcmp(argv[i],"--mouse")&&i+1<argc){ sscanf(argv[++i],"%f,%f",&mx,&my); }
    else if(!strcmp(argv[i],"--save")&&i+1<argc) snprintf(save_path,sizeof save_path,"%s",argv[++i]);
    else if(!strcmp(argv[i],"--status")&&i+1<argc) status_every=atoi(argv[++i]);
    else if(!strcmp(argv[i],"--wav")&&i+1<argc) wav=argv[++i];
    else if(!strcmp(argv[i],"--weak-boss")) weak_boss=true;
    else if(!strcmp(argv[i],"--runs")&&i+1<argc) max_runs=atoi(argv[++i]);
    else if(!strcmp(argv[i],"--botsmart")){ extern bool g_bot_smart; g_bot_smart=true; }
    else if(!strcmp(argv[i],"--hero")&&i+1<argc){ extern int g_force_hero; g_force_hero=atoi(argv[++i]); }
    else if(!strcmp(argv[i],"--pad")) pad=true;
    else if(!strcmp(argv[i],"--padhold")){ pad=true; padhold=true; }
    else if(!strcmp(argv[i],"--key")&&i+2<argc){ key_id=atoi(argv[++i]); keys_at=atoi(argv[++i]); }
    else if(!strcmp(argv[i],"--keys")&&i+1<argc){ const char *p=argv[++i]; while(*p && nks<64){ int k=0,f=0; if(sscanf(p,"%d:%d",&k,&f)==2){ ks_key[nks]=k; ks_frame[nks]=f; nks++; } const char *c=strchr(p,','); if(!c) break; p=c+1; } }
    else if(!strcmp(argv[i],"--gpu")&&i+1<argc){ sscanf(argv[++i],"%dx%d",&gpu_w,&gpu_h); }
    else if(!strcmp(argv[i],"--quality")&&i+1<argc){ quality=atoi(argv[++i]); }
    else if(!strcmp(argv[i],"--dbg")&&i+1<argc){ extern int g_dbg; g_dbg=atoi(argv[++i]); }
    else if(!strcmp(argv[i],"--norender")){ extern bool g_norender; g_norender=true; }
    else if(!strcmp(argv[i],"--intro")) want_intro=true;
    else if(!strcmp(argv[i],"--botskill")&&i+1<argc){ extern float g_bot_skill; g_bot_skill=(float)atof(argv[++i]); }
    else if(!strcmp(argv[i],"--botendless")){ extern bool g_bot_endless; g_bot_endless=true; }
    else if(!strcmp(argv[i],"--seed")&&i+1<argc){ extern uint32_t g_seed_override; g_seed_override=(uint32_t)strtoul(argv[++i],NULL,10); }
    else if(!strcmp(argv[i],"--voxsheet")&&i+1<argc){ vox_debug_sheet(argv[++i]); return 0; }
    else if(!strcmp(argv[i],"--iconsheet")&&i+1<argc){   /* tests : icones 3D des billes v9 */
      const char *fn=argv[++i]; static const int IDS[14]={BK_BOOMERANG,BK_SEISME,BK_CHAINE,BK_CHAKRAM,BK_FAILLE,BK_ENTRAVE,BK_APOCALYPSE,ICON_PAS(PS_PENDULE),ICON_PAS(PS_BOUSSOLE),ICON_PAS(PS_GRIMOIRE),BK_ETOILE,BK_VOLCAN,BK_OUROBOROS,ICON_PAS(PS_TOTEM)};
      int W=7*ICON_N, H=2*ICON_N; uint8_t *img=calloc((size_t)W*H*4,1), *rgb=malloc((size_t)W*H*3);
      for(int k=0;k<14;k++) icg_render(IDS[k],img+((size_t)(k/7)*ICON_N*W+(k%7)*ICON_N)*4,W*4);
      for(int j=0;j<W*H;j++){ float a=img[j*4+3]/255.0f; for(int c=0;c<3;c++) rgb[j*3+c]=(uint8_t)(img[j*4+c]*1.0f+40*(1-a)); }
      write_ppm_rgb(fn,rgb,W,H); return 0; }
    else if(!strcmp(argv[i],"--curiosheet")&&i+1<argc){ extern void curio_sheet(const char*); gfx_init(); curio_sheet(argv[++i]); return 0; }
#ifdef GOUFFRE_ATELIER
    else if(!strcmp(argv[i],"--bake-hd")&&i+1<argc){   /* atelier HD : --bake-hd DOSSIER [premier dernier camp] */
      extern int hd_bake(const char *dir,int b0,int b1,bool camp,int v0,int v1); extern int HS;
      const char *d=argv[++i]; int b0=0,b1=BIOME_COUNT-1; bool cp=true; int v0=0,v1=3;
      if(i+2<argc){ b0=atoi(argv[i+1]); b1=atoi(argv[i+2]); cp= i+3<argc ? atoi(argv[i+3])!=0 : true; }
      if(i+5<argc){ v0=atoi(argv[i+4]); v1=atoi(argv[i+5]); }
      if(i+6<argc){ HS=atoi(argv[i+6]); if(HS<1) HS=1; if(HS>8) HS=8; }
      gfx_init(); return hd_bake(d,b0,b1,cp,v0,v1); }
    else if(!strcmp(argv[i],"--bake-amb")&&i+1<argc){   /* atelier sonore : --bake-amb DOSSIER [premiere derniere] (0..27) */
      extern int amb_bake(const char *dir,int t0,int t1);
      const char *d=argv[++i]; int t0=0,t1=27; if(i+2<argc){ t0=atoi(argv[i+1]); t1=atoi(argv[i+2]); }
      return amb_bake(d,t0,t1); }
#endif
    else if(!strcmp(argv[i],"--figsheet")&&i+1<argc){ extern void hero_fig_sheet(const char*); hero_fig_sheet(argv[++i]); return 0; }
    else if(!strcmp(argv[i],"--figbench")){
      /* temps de rendu des figurines de l'ecran d'entrainement */
      extern const VoxModel *hero_fig_model(int ch);
      Sprite out=spr_alloc(270,248); out.em=(uint32_t*)calloc(270*248,4);
      for(int c=0;c<CH_COUNT;c++){ struct timespec b0,b1; clock_gettime(CLOCK_MONOTONIC,&b0); const VoxModel *m=hero_fig_model(c); clock_gettime(CLOCK_MONOTONIC,&b1);
        printf("construction : %.1f ms   ",(b1.tv_sec-b0.tv_sec)*1e3+(b1.tv_nsec-b0.tv_nsec)/1e6);
        struct timespec t0,t1; clock_gettime(CLOCK_MONOTONIC,&t0);
        for(int k=0;k<120;k++){ VoxCam cam={k*0.05f,0.26f,2.8f,135,210}; vox_render(m,k/60.0f,0,&cam,&out); }
        clock_gettime(CLOCK_MONOTONIC,&t1);
        double ms=((t1.tv_sec-t0.tv_sec)*1e3+(t1.tv_nsec-t0.tv_nsec)/1e6)/120;
        printf("%-18s %.2f ms/image  (%dx%dx%d)\n",CHARS[c].name,ms,m->nx,m->ny,m->nz);
        { Sprite b=spr_alloc(208,172); b.em=(uint32_t*)calloc(208*172,4); clock_gettime(CLOCK_MONOTONIC,&t0);
          for(int k=0;k<20;k++){ VoxCam cam={-0.24f,-0.1f,3.25f,104,8+66*3.25f}; vox_render(m,k/60.0f,VR_SMOOTH,&cam,&b); }
          clock_gettime(CLOCK_MONOTONIC,&t1); printf("      buste du titre : %.2f ms\n",((t1.tv_sec-t0.tv_sec)*1e3+(t1.tv_nsec-t0.tv_nsec)/1e6)/20); free(b.px); free(b.em); } }
      return 0; }
    else if(!strcmp(argv[i],"--voxbench")){
      /* temps de rendu de la maquette tournante, par batiment au niveau max */
      Sprite out=spr_alloc(358,248); out.em=(uint32_t*)calloc(358*248,4); VoxModel m={0};
      for(int b=0;b<BLD_COUNT;b++){ bld_model(b,BUILDINGS[b].max,true,&m); float sc,cy; vox_fit(&m,0.5f,342,214,3.6f,&sc,&cy);
        struct timespec t0,t1; clock_gettime(CLOCK_MONOTONIC,&t0);
        for(int k=0;k<120;k++){ VoxCam c={k*0.05f,0.5f,sc,179,124+cy}; vox_render(&m,k/60.0f,0,&c,&out); }
        clock_gettime(CLOCK_MONOTONIC,&t1);
        double ms=((t1.tv_sec-t0.tv_sec)*1e3+(t1.tv_nsec-t0.tv_nsec)/1e6)/120;
        printf("%-20s echelle %.2f : %.2f ms/image  (%dx%dx%d)\n",BUILDINGS[b].name,sc,ms,m.nx,m.ny,m.nz);
#ifdef VOXSTAT
        { extern long long g_vsteps,g_vskips,g_vcasts; printf("   rayons %lld/img, pas %.1f/rayon, sauts %.1f/rayon\n",g_vcasts/120,(double)g_vsteps/g_vcasts,(double)g_vskips/g_vcasts); g_vsteps=g_vskips=g_vcasts=0; }
#endif
      }
      return 0; }
  }
  setvbuf(stdout,NULL,_IOLBF,0);
  if(gpu_w>0 && gpu_h>0){
    char info[4096]; info[0]=0;
    if(!egl_start(info,sizeof info)){ printf("GPU : EGL indisponible (%s)\n",info); return 2; }
    if(!gpu_init(egl_proc,info,sizeof info)){ printf("GPU : echec\n%s\n",info); return 3; }
    printf("%s",info);
    g_gpu_ok=true; gpu_fbo=gpu_test_target(gpu_w,gpu_h); gpu_rgb=(uint8_t*)malloc((size_t)gpu_w*gpu_h*3);
  }
  { extern bool g_skip_intro; g_skip_intro=!want_intro; }
  game_init();
  if(quality>=0) S.quality=quality;
  else if(!gpu_rgb) S.quality=0;
  if(scene) game_debug_scene(scene,arg);
  Input in; memset(&in,0,sizeof in);
  static int16_t audio[800*2];
  FILE *wf=NULL; long wav_samples=0;
  if(wav){ wf=fopen(wav,"wb"); if(wf){ unsigned char h[44]={0}; fwrite(h,1,44,wf); } }
  double peak=0; long clipped=0;
  int last_state=-1;
  for(int f=0;f<frames;f++){
    memset(in.key,0,sizeof in.key); memset(in.key_pressed,0,sizeof in.key_pressed);
    memset(in.mb_pressed,0,sizeof in.mb_pressed); memset(in.pad_pressed,0,sizeof in.pad_pressed);
    memset(in.pad,0,sizeof in.pad); in.lx=in.ly=in.rx=in.ry=0;
    if(pad){ g_bot_pad=true; in.pad_on=true; in.pad_type=PADT_PS; snprintf(in.pad_name,sizeof in.pad_name,"DualSense (PS5)"); snprintf(in.pad_info,sizeof in.pad_info,"lecture directe USB"); in.mouse_inside=false; }
    in.mx=mx; in.my=my+sinf(f*0.02f)*20; in.mouse_inside=!pad; in.mouse_moved=false;
    if(f==keys_at && key_id>=0){ in.key_pressed[key_id]=true; }
    for(int q=0;q<nks;q++) if(f==ks_frame[q] && ks_key[q]>=0 && ks_key[q]<KEY_COUNT){ in.key_pressed[ks_key[q]]=true; in.key[ks_key[q]]=true; }
    if(bot) game_bot_input(&in,1.0f/60);
    if(padhold){ in.pad[PAD_A]=in.pad[PAD_UP]=in.pad[PAD_RB]=in.pad[PAD_START]=true; in.lx=0.8f; in.ly=-0.3f; in.rx=-0.2f; in.ry=0.9f; in.rt=0.6f; }
    if(weak_boss && g_state==GS_RUN && R.boss.on && R.boss.enter>=1 && R.boss.hp>R.boss.hpmax*0.02f && R.boss.dying<=0) R.boss.hp=R.boss.hpmax*0.02f;
    if(weak_boss && g_state==GS_RUN && R.mb_on){ for(int q=0;q<MAX_EN;q++){ Enemy *e=&R.en[q]; if(e->on && e->mboss && e->y>=70 && e->hp>e->hpmax*0.02f) e->hp=e->hpmax*0.02f; } }
    for(int k=0;k<KEY_COUNT;k++) if(in.key_pressed[k]) in.key[k]=true;
    game_frame(&in,1.0f/60);
    game_audio_mix(audio,735);
    for(int k=0;k<735*2;k++){ int v=abs(audio[k]); if(v>peak) peak=v; if(v>=32000) clipped++; }
    if(wf){ fwrite(audio,2,735*2,wf); wav_samples+=735; }
    bool d=false;
    if(dump && every>0 && f%every==0) d=true;
    if(dump && at){ char buf[256]; snprintf(buf,sizeof buf,",%s,",at); char me[32]; snprintf(me,sizeof me,",%d,",f); if(strstr(buf,me)) d=true; }
    if(d){ char fn[512]; snprintf(fn,sizeof fn,"%s/f%06d.ppm",dump,f);
      if(gpu_rgb && g_gpu){ gpu_render(gpu_fbo,gpu_w,gpu_h,0,0,gpu_w,gpu_h); gpu_test_read(gpu_w,gpu_h,gpu_rgb); write_ppm_rgb(fn,gpu_rgb,gpu_w,gpu_h);
        if(getenv("GOUFFRE_LAYERS")){ char fl[512]; static uint8_t tmp[SCREEN_W*SCREEN_H*3];
          for(int i=0;i<SCREEN_W*SCREEN_H;i++){ tmp[i*3]=(uint8_t)(g_occ[i]*8); tmp[i*3+1]=(uint8_t)(g_emis[i]>>8); tmp[i*3+2]=(uint8_t)(g_nrm[i]>>16); }
          snprintf(fl,sizeof fl,"%s/L%06d.ppm",dump,f); write_ppm_rgb(fl,tmp,SCREEN_W,SCREEN_H); }
        if(gpu_failed()){ printf("GPU : erreur de rendu\n"); } }
      else write_ppm(fn,game_framebuffer()); }
    if(g_state!=last_state){ printf("[%6d] etat -> %s\n",f,SNAMES[g_state]); last_state=g_state;
      if(g_state==GS_RESULTS) printf("RUN ch=%d prof=%d mode=%d gagne=%d niveau=%d temps=%.0f or=%d cristaux=%d kills=%d | total or=%d cristaux=%d runs=%d wins=%d defis=%d\n",
        LAST.ch,LAST.depth,LAST.mode,LAST.won,LAST.level,LAST.time,LAST.gold,LAST.crystals,LAST.kills,S.gold,S.crystals,S.runs,S.wins,meta_new_count());
      if(g_state==GS_RESULTS){ printf("PROG h=%.2f dest=%d pct=%.2f",S.play_time/3600.0,LAST.dest,completion_pct());
        for(int c=0;c<COMP_CATS;c++) printf(" c%d=%.3f",c,completion_part(c,NULL,NULL));
        int rs=0; for(int h=0;h<CH_COUNT;h++) rs+=hero_rank(h);
        printf(" dmax=%d/%d dwon=%d/%d ranks=%d stars=%d bld=%d kills=%d xp=%d\n",S.depth_max,S.depth_max2,S.depth_won[0],S.depth_won[1],rs,total_stars(),0,S.total_kills,LAST.hxp);
        { extern float g_hurt_by[5];
          printf("MORT lvl=%d seg=%d mb=%d duo=%d gagne=%d ch=%d niveau=%d t=%.0f/%.0f gardien=%d gardienpv=%.3f phase=%d pvmax=%.0f heros=%d billes=%d degats breche=%.0f tirs=%.0f contact=%.0f dgardien=%.0f lave=%.0f\n",
            R.lvl+1,R.seg,R.mb_kills,R.ch2,LAST.won,LAST.ch,R.level+1,R.ltime,R.lduration,R.boss_spawned?1:0,R.boss.hpmax>0?R.boss.hp/R.boss.hpmax:0.0f,R.boss.phase,R.hpmax,R.plevel,R.nballs,
            g_hurt_by[0],g_hurt_by[1],g_hurt_by[2],g_hurt_by[3],g_hurt_by[4]);
          extern float g_inflow, g_dealt; printf("FLUX pv_entres=%.0f degats_infliges=%.0f par_s=%.0f/%.0f\n",g_inflow,g_dealt,g_inflow/maxf(1,R.ltime),g_dealt/maxf(1,R.ltime));
          extern int g_breach_kind[EK_COUNT]; printf("BRECHES"); for(int k=0;k<EK_COUNT;k++) if(g_breach_kind[k]) printf(" %s=%d",ENEMIES[k].name,g_breach_kind[k]); printf("\n"); }
        runs_done++; } }
    if(max_runs>0 && runs_done>=max_runs) break;
    { extern float g_inflow, g_dealt; static float pin=0, pde=0, plt=0; static int pst=-1, plv=-1, php=0, phpm=0;
      if(g_state==GS_RUN){ if(pst>=0 && R.stage!=pst) printf("NIVEAU %d fini : pv_entres=%.0f degats=%.0f duree=%.0f  par_s=%.0f/%.0f  pv %d/%d\n",plv+1,pin,pde,plt,pin/maxf(1,plt),pde/maxf(1,plt),php,phpm);
        pst=R.stage; plv=R.level; pin=g_inflow; pde=g_dealt; plt=R.ltime; php=(int)R.hp; phpm=(int)R.hpmax; } else pst=-1; }
    if(status_every>0 && f%status_every==0 && g_state==GS_RUN){
      int nb=0, ne=0; for(int i=0;i<MAX_BALLS;i++) if(R.balls_e[i].on) nb++; for(int i=0;i<MAX_EN;i++) if(R.en[i].on) ne++;
      char an[40]; amb_playing(an,sizeof an);
      printf("[%6d] niv %d seg %d mb=%d t=%.0f/%.0f sub=%d pv=%.0f/%.0f heros=%d xp=%.1f/%.1f or=%d kills=%d billes=%d ennemis=%d boss=%s %.0f/%.0f decor=%d amb=%s\n",
        f,R.single?R.lvl+1:R.level+1,R.seg,R.mb_on?1:0,R.ltime,R.lduration,R.sub,R.hp,R.hpmax,R.plevel,R.xp,R.xpnext,R.gold,R.kills,nb,ne,R.boss.on?"oui":"non",R.boss.hp,R.boss.hpmax,hd_get_variant(),an[0]?an:"-");
    }
    if(game_should_quit()) break;
  }
  if(wf){
    unsigned int datasz=(unsigned int)(wav_samples*4); unsigned char h[44];
    memcpy(h,"RIFF",4); unsigned int v=36+datasz; memcpy(h+4,&v,4); memcpy(h+8,"WAVEfmt ",8);
    v=16; memcpy(h+16,&v,4); unsigned short s=1; memcpy(h+20,&s,2); s=2; memcpy(h+22,&s,2); v=AUDIO_RATE; memcpy(h+24,&v,4);
    v=AUDIO_RATE*4; memcpy(h+28,&v,4); s=4; memcpy(h+32,&s,2); s=16; memcpy(h+34,&s,2); memcpy(h+36,"data",4); memcpy(h+40,&datasz,4);
    fseek(wf,0,SEEK_SET); fwrite(h,1,44,wf); fclose(wf);
  }
  printf("vibrations demandees : %d\n",rumble_calls);
  printf("FIN : etat=%s runs=%d wins=%d or=%d cristaux=%d best=%d | audio pic=%.0f satures=%ld\n",SNAMES[g_state],S.runs,S.wins,S.gold,S.crystals,S.best_level,peak,clipped);
  game_shutdown();
  return 0;
}
