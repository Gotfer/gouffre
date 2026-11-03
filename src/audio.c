/* GOUFFRE - sons synthetises, musique chiptune generee, mixeur */
#include "game.h"
#include <stdatomic.h>

volatile float g_vol_music = 0.5f, g_vol_sfx = 0.7f, g_vol_amb = 0.6f;

typedef struct { int16_t *d; int n; } Buf;
static Buf SB[SFX_COUNT];
static Buf MB[MUS_COUNT];

/* ---------------- file de commandes (jeu -> thread audio) ---------------- */
typedef struct { int type; int id; float vol, pan, pitch; } Cmd;
enum { CMD_PLAY, CMD_MUSIC, CMD_MUSIC_STOP };
#define CMDQ 256
static Cmd cmdq[CMDQ];
static atomic_int q_head, q_tail;
static void push_cmd(Cmd c){
  int h=atomic_load(&q_head); int nh=(h+1)&(CMDQ-1);
  if(nh==atomic_load(&q_tail)) return;
  cmdq[h]=c; atomic_store(&q_head,nh);
}

/* ---------------- synthese ---------------- */
enum { W_SQ, W_TRI, W_SIN, W_SAW, W_NOISE };
typedef struct { int wave; float f0,f1,dur,att,decp,vol,duty,vibf,vibd,arpt,arpm,lp,noise; } SP;
static SP sp(int wave,float f0,float f1,float dur,float vol){
  SP p; memset(&p,0,sizeof p); p.wave=wave; p.f0=f0; p.f1=f1; p.dur=dur; p.vol=vol;
  p.att=0.002f; p.decp=1.0f; p.duty=0.5f; p.lp=1.0f; p.arpm=1.0f; return p;
}
/* bruit : un generateur par fil d'execution (les musiques des niveaux se calculent en tache de fond) */
static _Thread_local uint32_t nseed=0x1234567u;
static float nrand(void){ nseed^=nseed<<13; nseed^=nseed>>17; nseed^=nseed<<5; return (float)(nseed&0xFFFF)/32768.0f-1.0f; }
/* table de sinus (interpolee) : la synthese est bien plus rapide qu'avec sinf */
#define SINT 4096
static float SIN_T[SINT+1]; static bool sin_ok=false;
static void sin_init(void){ if(sin_ok) return; for(int i=0;i<=SINT;i++) SIN_T[i]=sinf(TAU*i/SINT); sin_ok=true; }
static inline float tsin(float ph){ float x=ph*SINT; int i=(int)x; float f=x-(float)i; i&=SINT-1; return SIN_T[i]+(SIN_T[i+1]-SIN_T[i])*f; }
static inline float env_at(const SP *p,float t,float inv){
  if(t<p->att) return t/p->att;
  float x=clampf(1.0f-(t-p->att)*inv,0,1);
  return p->decp==1.0f ? x : (x<=0 ? 0 : powf(x,p->decp));
}
static void synth_into(float *out,int n,int off,SP p){
  int len=(int)(p.dur*AUDIO_RATE);
  const float IR=1.0f/AUDIO_RATE;
  float ph=0, nph=0, nz=nrand(), lpv=0, vph=0, vinc=p.vibf*IR;
  bool sweep=p.f1!=p.f0; float lr= sweep ? log2f(p.f1/p.f0) : 0;
  float inv=1.0f/fmaxf(p.dur-p.att,1e-6f), e0=0, e1=0;
  for(int i=0;i<len;i++){
    int idx=off+i; if(idx>=n) break;
    float t=(float)i*IR;
    float f= sweep ? p.f0*exp2f(lr*(t/p.dur)) : p.f0;
    if(p.vibd>0){ f*=1.0f+p.vibd*tsin(vph); vph+=vinc; vph-=floorf(vph); }
    if(p.arpt>0 && t>p.arpt) f*=p.arpm;
    ph+=f*IR; ph-=floorf(ph);
    nph+=f*6.0f*IR; if(nph>=1){ nph-=floorf(nph); nz=nrand(); }
    float s;
    switch(p.wave){
      case W_SQ: s=ph<p.duty?0.8f:-0.8f; break;
      case W_TRI: s=4.0f*fabsf(ph-0.5f)-1.0f; break;
      case W_SIN: s=tsin(ph); break;
      case W_SAW: s=(2.0f*ph-1.0f)*0.8f; break;
      default: s=nz; break;
    }
    if(p.noise>0) s=s*(1-p.noise)+nz*p.noise;
    /* enveloppe : calculee tous les 16 echantillons, interpolee entre deux */
    if((i&15)==0){ e0=env_at(&p,t,inv); e1=env_at(&p,t+16.0f/AUDIO_RATE,inv); }
    float env=e0+(e1-e0)*(float)(i&15)*(1.0f/16);
    if(p.lp<1.0f){ lpv+=p.lp*(s-lpv); s=lpv; }
    out[idx]+=s*env*p.vol;
  }
}
static Buf to_buf(float *f,int n){
  Buf b; b.n=n; b.d=(int16_t*)malloc((size_t)n*2);
  for(int i=0;i<n;i++){ float v=clampf(f[i],-1,1); b.d[i]=(int16_t)(v*32000); }
  return b;
}
static Buf mk(SP p){ int n=(int)(p.dur*AUDIO_RATE)+1; float *f=(float*)calloc((size_t)n,4); synth_into(f,n,0,p); Buf b=to_buf(f,n); free(f); return b; }
static Buf mk2(SP a,SP b){
  int n=(int)(fmaxf(a.dur,b.dur)*AUDIO_RATE)+1; float *f=(float*)calloc((size_t)n,4);
  synth_into(f,n,0,a); synth_into(f,n,0,b); Buf r=to_buf(f,n); free(f); return r;
}
static float midi(float m){ return 440.0f*powf(2.0f,(m-69.0f)/12.0f); }
static Buf arpeggio(const int *notes,int nn,float step,float notedur,int wave,float vol){
  int n=(int)((step*(nn-1)+notedur)*AUDIO_RATE)+1; float *f=(float*)calloc((size_t)n,4);
  for(int i=0;i<nn;i++){ SP p=sp(wave,midi((float)notes[i]),midi((float)notes[i]),notedur,vol); p.decp=2.0f; p.duty=0.25f; synth_into(f,n,(int)(i*step*AUDIO_RATE),p); }
  Buf r=to_buf(f,n); free(f); return r;
}

static void build_sfx(void){
  SP p;
  p=sp(W_SQ,880,520,0.07f,0.16f); p.lp=0.5f; SB[SFX_SHOOT]=mk(p);
  p=sp(W_TRI,640,600,0.035f,0.16f); SB[SFX_BOUNCE]=mk(p);
  p=sp(W_SQ,320,120,0.07f,0.26f); p.duty=0.25f; p.noise=0.35f; p.lp=0.6f; SB[SFX_HIT]=mk(p);
  { SP a=sp(W_NOISE,1600,240,0.2f,0.3f); a.decp=1.5f; SP b=sp(W_SQ,200,50,0.12f,0.22f); b.lp=0.3f; SB[SFX_KILL]=mk2(a,b); }
  p=sp(W_SIN,1500,1500,0.09f,0.18f); p.arpt=0.035f; p.arpm=1.5f; SB[SFX_GEM]=mk(p);
  p=sp(W_SQ,1050,1050,0.14f,0.12f); p.arpt=0.05f; p.arpm=1.335f; p.decp=1.5f; SB[SFX_COIN]=mk(p);
  { int n[]={60,64,67,72,76,79,84}; SB[SFX_LEVELUP]=arpeggio(n,7,0.055f,0.25f,W_SQ,0.16f); }
  { SP a=sp(W_SAW,240,80,0.28f,0.32f); a.lp=0.4f; SP b=sp(W_NOISE,800,200,0.15f,0.2f); SB[SFX_HURT]=mk2(a,b); }
  { SP a=sp(W_NOISE,700,60,0.55f,0.5f); a.decp=1.8f; a.lp=0.25f; SP b=sp(W_SIN,90,30,0.4f,0.45f); SB[SFX_EXPLODE]=mk2(a,b); }
  p=sp(W_NOISE,3200,900,0.16f,0.22f); p.vibf=40; p.vibd=0.5f; SB[SFX_ZAP]=mk(p);
  { SP a=sp(W_SIN,2200,3200,0.22f,0.12f); a.vibf=30; a.vibd=0.08f; SP b=sp(W_NOISE,6000,6000,0.18f,0.06f); SB[SFX_FREEZE]=mk2(a,b); }
  { SP a=sp(W_SAW,130,55,1.3f,0.45f); a.vibf=7; a.vibd=0.15f; a.lp=0.12f; a.att=0.1f; SP b=sp(W_NOISE,300,80,1.0f,0.25f); b.lp=0.2f; b.att=0.1f; SB[SFX_BOSS_ROAR]=mk2(a,b); }
  { SP a=sp(W_NOISE,500,40,1.8f,0.55f); a.decp=2.0f; a.lp=0.2f; SP b=sp(W_SIN,70,25,1.6f,0.5f); SB[SFX_BOSS_DIE]=mk2(a,b); }
  p=sp(W_SQ,1200,1200,0.03f,0.10f); SB[SFX_CLICK]=mk(p);
  p=sp(W_TRI,900,900,0.025f,0.08f); SB[SFX_HOVER]=mk(p);
  p=sp(W_SQ,660,660,0.13f,0.13f); p.arpt=0.05f; p.arpm=1.5f; SB[SFX_SELECT]=mk(p);
  { SP a=sp(W_SQ,110,45,0.38f,0.32f); a.lp=0.3f; SP b=sp(W_NOISE,400,100,0.3f,0.25f); b.lp=0.3f; SB[SFX_BREACH]=mk2(a,b); }
  p=sp(W_SQ,520,300,0.09f,0.10f); p.duty=0.125f; SB[SFX_ENEMY_SHOT]=mk(p);
  p=sp(W_SIN,180,950,1.0f,0.18f); p.vibf=14; p.vibd=0.05f; p.att=0.6f; p.decp=0.5f; SB[SFX_LASER_CHARGE]=mk(p);
  { SP a=sp(W_SAW,110,100,0.8f,0.3f); a.vibf=30; a.vibd=0.05f; a.lp=0.35f; SP b=sp(W_NOISE,2000,1500,0.8f,0.12f); SB[SFX_LASER]=mk2(a,b); }
  p=sp(W_TRI,1300,1900,0.06f,0.12f); SB[SFX_SPLIT]=mk(p);
  p=sp(W_SIN,600,1200,0.22f,0.14f); p.arpt=0.1f; p.arpm=1.25f; SB[SFX_HEAL]=mk(p);
  { int n[]={67,71,74,79}; SB[SFX_UPGRADE]=arpeggio(n,4,0.06f,0.2f,W_SQ,0.15f); }
  p=sp(W_SQ,160,140,0.16f,0.16f); SB[SFX_DENY]=mk(p);
  { int n[]={60,63,67,70,72,75,79,82,84,87,91}; SB[SFX_EVOLVE]=arpeggio(n,11,0.045f,0.35f,W_SQ,0.14f); }
  { int n[]={81,69,81,69}; SB[SFX_WARN]=arpeggio(n,4,0.14f,0.12f,W_SQ,0.12f); }
  { SP a=sp(W_SIN,90,30,0.4f,0.6f); SP b=sp(W_NOISE,300,60,0.35f,0.35f); b.lp=0.2f; SB[SFX_SLAM]=mk2(a,b); }
  p=sp(W_SQ,1800,1400,0.05f,0.10f); p.duty=0.125f; SB[SFX_SHIELD]=mk(p);
  p=sp(W_SIN,500,900,0.05f,0.06f); SB[SFX_RETURN]=mk(p);
  { int n[]={72,76,79,84,88,91,96}; SB[SFX_REVIVE]=arpeggio(n,7,0.07f,0.5f,W_TRI,0.2f); }
  { int n[]={67,72,76,79,84,88}; SB[SFX_ACHIEVE]=arpeggio(n,6,0.07f,0.6f,W_SQ,0.13f); }
  { SP a=sp(W_SIN,midi(57),midi(57),1.1f,0.12f); a.att=0.25f; a.vibf=5; a.vibd=0.01f; SP b=sp(W_SIN,midi(64),midi(63),1.1f,0.10f); b.att=0.35f; b.vibf=4; b.vibd=0.012f; SB[SFX_EVENT]=mk2(a,b); }
  { SP a=sp(W_SIN,1900,900,0.26f,0.13f); a.vibf=40; a.vibd=0.03f; SP b=sp(W_NOISE,5000,3000,0.2f,0.05f); SB[SFX_BEAM]=mk2(a,b); }
  p=sp(W_NOISE,500,1400,0.32f,0.22f); p.lp=0.12f; p.att=0.08f; p.decp=1.4f; SB[SFX_WIND]=mk(p);
  p=sp(W_SIN,2600,2600,0.2f,0.11f); p.arpt=0.06f; p.arpm=1.5f; p.decp=2.0f; SB[SFX_CRYSTAL]=mk(p);
  { SP a=sp(W_SAW,900,110,0.75f,0.18f); a.lp=0.25f; a.vibf=9; a.vibd=0.05f; SP b=sp(W_SIN,300,60,0.7f,0.2f); SB[SFX_TIME]=mk2(a,b); }
  { SP a=sp(W_SQ,1500,1150,0.12f,0.13f); a.duty=0.125f; SP b=sp(W_NOISE,4000,2000,0.08f,0.1f); SB[SFX_BLOCK]=mk2(a,b); }
  p=sp(W_SIN,480,950,0.38f,0.11f); p.vibf=8; p.vibd=0.04f; p.att=0.05f; SB[SFX_GHOST]=mk(p);
  p=sp(W_NOISE,2400,400,0.16f,0.16f); p.lp=0.3f; SB[SFX_DASH]=mk(p);
  p=sp(W_TRI,1200,2600,0.08f,0.08f); SB[SFX_BLINK]=mk(p);
  /* v7 : echo, tonnerre, racines, acide, or */
  { SP a=sp(W_SIN,520,500,0.55f,0.14f); a.vibf=6; a.vibd=0.02f; a.decp=1.6f; SP b=sp(W_SIN,1310,1290,0.4f,0.06f); b.decp=2.2f; SB[SFX_ECHO]=mk2(a,b); }
  { SP a=sp(W_NOISE,1800,60,0.95f,0.5f); a.decp=1.4f; a.lp=0.22f; SP b=sp(W_SIN,70,30,0.75f,0.42f); SB[SFX_THUNDER]=mk2(a,b); }
  p=sp(W_SAW,160,90,0.22f,0.15f); p.vibf=25; p.vibd=0.12f; p.lp=0.3f; SB[SFX_ROOT]=mk(p);
  p=sp(W_NOISE,5200,2600,0.3f,0.11f); p.vibf=30; p.vibd=0.4f; p.att=0.02f; SB[SFX_ACID]=mk(p);
  p=sp(W_SQ,2100,2100,0.12f,0.07f); p.duty=0.125f; p.arpt=0.03f; p.arpm=1.26f; p.decp=2.0f; SB[SFX_GOLD]=mk(p);
}

/* ---------------- musique ---------------- */
typedef struct { float *f; int n; float spb; } Song; /* secondes par pas (double-croche) */
static void song_note(Song *s,float step,float len_steps,float m,int wave,float vol,float decp,float duty,float vib){
  SP p=sp(wave,midi(m),midi(m),len_steps*s->spb,vol); p.decp=decp; p.duty=duty; p.att=0.004f;
  if(vib>0){ p.vibf=5.5f; p.vibd=vib; }
  int len=(int)(p.dur*AUDIO_RATE)+1; float *tmp=(float*)calloc((size_t)len,4);
  synth_into(tmp,len,0,p);
  int off=(int)(step*s->spb*AUDIO_RATE);
  for(int i=0;i<len;i++) s->f[(off+i)%s->n]+=tmp[i];
  free(tmp);
}
static void song_drum(Song *s,float step,int kind,float vol){
  float dur = kind==0?0.28f:(kind==1?0.2f:(kind==2?0.05f:0.22f));
  int len=(int)(dur*AUDIO_RATE); int off=(int)(step*s->spb*AUDIO_RATE);
  float ph=0, prev=0;
  for(int i=0;i<len;i++){
    float t=(float)i/AUDIO_RATE, v=0;
    if(kind==0){ float f=45+110*expf(-t*28); ph+=f/AUDIO_RATE; v=sinf(TAU*ph)*expf(-t*9)*1.0f; }
    else if(kind==1){ float nz=nrand(); ph+=190.0f/AUDIO_RATE; v=nz*expf(-t*16)*0.55f + sinf(TAU*ph)*expf(-t*30)*0.35f; }
    else { float nz=nrand(); float hp=nz-prev; prev=nz; v=hp*expf(-t*(kind==2?70.0f:14.0f))*0.4f; }
    s->f[(off+i)%s->n]+=v*vol;
  }
}
/* "A4:4 C5:2 R:4 G#4:8" */
static void song_melody(Song *s,float start,const char *str,int wave,float vol,float decp,float duty,float vib,int transpose){
  const char *p=str; float step=start;
  while(*p){
    while(*p==' ') p++; if(!*p) break;
    int m=-1;
    if(*p=='R'){ p++; }
    else {
      static const int base[7]={9,11,0,2,4,5,7}; /* A B C D E F G */
      int n=base[*p-'A']; p++;
      if(*p=='#'){ n++; p++; } else if(*p=='b'){ n--; p++; }
      int oct=*p-'0'; p++;
      m=12*(oct+1)+n+transpose;
    }
    int len=0; if(*p==':'){ p++; while(*p>='0'&&*p<='9'){ len=len*10+(*p-'0'); p++; } }
    if(m>=0) song_note(s,step,(float)len*0.92f,(float)m,wave,vol,decp,duty,vib);
    step+=len;
  }
}
typedef struct { int root; int q; } Chord; /* q: 0 maj, 1 min, 2 dim */
static int chord_tone(Chord c,int i){
  static const int T[3][4]={{0,4,7,12},{0,3,7,12},{0,3,6,12}};
  return c.root+T[c.q][i&3];
}
static Buf song_finish(Song *s){
  float peak=0.001f; for(int i=0;i<s->n;i++) if(fabsf(s->f[i])>peak) peak=fabsf(s->f[i]);
  float k=0.85f/peak; for(int i=0;i<s->n;i++) s->f[i]*=k;
  Buf b=to_buf(s->f,s->n); free(s->f); return b;
}

static Buf build_title(void){
  Song s; float bpm=78; s.spb=60.0f/bpm/4; int bars=8; s.n=(int)(bars*16*s.spb*AUDIO_RATE); s.f=(float*)calloc((size_t)s.n,4);
  Chord ch[8]={{57,1},{53,0},{48,0},{55,0},{57,1},{53,0},{52,0},{52,0}};
  for(int b=0;b<bars;b++){
    float st=b*16.0f;
    for(int i=0;i<3;i++) song_note(&s,st,16,(float)chord_tone(ch[b],i),W_TRI,0.05f,0.6f,0.5f,0);
    song_note(&s,st,8,(float)(ch[b].root-12),W_TRI,0.13f,1.2f,0.5f,0);
    song_note(&s,st+8,8,(float)(ch[b].root-12),W_TRI,0.10f,1.2f,0.5f,0);
    for(int k=0;k<8;k++){
      static const int pat[8]={0,1,2,3,2,1,2,1};
      song_note(&s,st+k*2,4,(float)(chord_tone(ch[b],pat[k])+12),W_SIN,0.045f,3.0f,0.5f,0);
      song_note(&s,st+k*2+3,4,(float)(chord_tone(ch[b],pat[k])+12),W_SIN,0.015f,3.0f,0.5f,0);
    }
  }
  song_melody(&s,0,"E5:8 C5:4 A4:4 F4:8 A4:4 C5:4 E5:8 D5:4 C5:4 B4:8 G4:8 "
                   "A4:4 C5:4 E5:4 A5:4 G5:8 F5:4 E5:4 D5:4 C5:4 B4:8 G#4:12 R:4",W_SIN,0.07f,1.6f,0.5f,0.004f,0);
  return song_finish(&s);
}

static Buf build_battle(void){
  Song s; float bpm=128; s.spb=60.0f/bpm/4; int bars=16; s.n=(int)(bars*16*s.spb*AUDIO_RATE); s.f=(float*)calloc((size_t)s.n,4);
  Chord ch[8]={{57,1},{53,0},{48,0},{55,0},{57,1},{53,0},{55,0},{52,0}};
  for(int b=0;b<bars;b++){
    Chord c=ch[b%8]; float st=b*16.0f;
    for(int k=0;k<8;k++){ int m=c.root-12+((k&1)?12:0); if(c.root>=55) m-=12; song_note(&s,st+k*2,1.8f,(float)m,W_TRI,0.20f,0.8f,0.5f,0); }
    for(int k=0;k<16;k++){ static const int pat[4]={0,1,2,1}; song_note(&s,st+k,1.2f,(float)(chord_tone(c,pat[k&3])+12),W_SQ,0.035f,2.0f,0.25f,0); }
    song_drum(&s,st+0,0,0.55f); song_drum(&s,st+8,0,0.55f); if(b&1) song_drum(&s,st+10,0,0.4f);
    song_drum(&s,st+4,1,0.35f); song_drum(&s,st+12,1,0.35f);
    for(int k=0;k<16;k+=2) song_drum(&s,st+k,2,0.18f);
    if((b%4)==3){ song_drum(&s,st+14,3,0.15f); song_drum(&s,st+13,1,0.2f); song_drum(&s,st+15,1,0.25f); }
  }
  song_melody(&s,8*16,"A4:4 C5:2 E5:2 D5:4 C5:4 A4:6 G4:2 F4:4 A4:4 G4:4 E4:2 G4:2 C5:8 B4:4 D5:4 G5:6 F5:2 "
                      "E5:4 D5:2 C5:2 A4:8 C5:4 A4:2 F4:2 A4:8 B4:4 G4:4 D5:4 B4:4 G#4:8 B4:4 E5:4",W_SQ,0.075f,0.7f,0.5f,0.006f,12);
  return song_finish(&s);
}

static Buf build_boss(void){
  Song s; float bpm=148; s.spb=60.0f/bpm/4; int bars=16; s.n=(int)(bars*16*s.spb*AUDIO_RATE); s.f=(float*)calloc((size_t)s.n,4);
  Chord ch[8]={{52,1},{52,1},{48,0},{50,0},{52,1},{52,1},{53,0},{47,0}};
  for(int b=0;b<bars;b++){
    Chord c=ch[b%8]; float st=b*16.0f;
    for(int k=0;k<16;k++){ int m=c.root-12+((k%4)==2?12:0); song_note(&s,st+k,0.9f,(float)m,W_SQ,0.11f,1.0f,0.5f,0); }
    for(int k=0;k<4;k++) song_drum(&s,st+k*4,0,0.6f);
    song_drum(&s,st+4,1,0.42f); song_drum(&s,st+12,1,0.42f);
    for(int k=0;k<16;k++) song_drum(&s,st+k,2,(k&1)?0.10f:0.16f);
    if((b%8)==7){ song_drum(&s,st+13,1,0.3f); song_drum(&s,st+14,1,0.35f); song_drum(&s,st+15,1,0.4f); }
    static const int stab[4]={2,6,10,14};
    for(int k=0;k<4;k++) for(int i=0;i<3;i++) song_note(&s,st+stab[k],1.0f,(float)(chord_tone(c,i)+12),W_SAW,0.03f,1.5f,0.5f,0);
  }
  const char *riff="E5:2 E5:2 G5:2 E5:2 B5:4 A5:2 G5:2 F#5:2 G5:2 F#5:2 E5:2 D5:4 E5:4 "
                   "E5:2 E5:2 G5:2 E5:2 C6:4 B5:2 A5:2 A5:4 F#5:4 D5:4 F#5:4 "
                   "E5:2 E5:2 G5:2 E5:2 B5:4 A5:2 G5:2 B5:2 C6:2 B5:2 A5:2 G5:4 F#5:4 "
                   "F5:4 A5:4 C6:4 A5:4 B5:6 A5:2 G5:2 F#5:2 D#5:4";
  song_melody(&s,8*16,riff,W_SQ,0.065f,0.6f,0.5f,0.008f,-12);
  return song_finish(&s);
}

static Buf build_camp(void){
  Song s; float bpm=72; s.spb=60.0f/bpm/4; int bars=8; s.n=(int)(bars*16*s.spb*AUDIO_RATE); s.f=(float*)calloc((size_t)s.n,4);
  Chord ch[8]={{62,0},{59,1},{55,0},{57,0},{62,0},{59,1},{55,0},{57,0}};
  for(int b=0;b<bars;b++){
    float st=b*16.0f;
    for(int i=0;i<3;i++) song_note(&s,st,16,(float)chord_tone(ch[b],i)-12,W_TRI,0.045f,0.5f,0.5f,0);
    song_note(&s,st,16,(float)(ch[b].root-24),W_SIN,0.14f,1.0f,0.5f,0);
    for(int k=0;k<8;k++){ static const int pat[8]={0,2,1,3,2,1,2,3}; song_note(&s,st+k*2,3,(float)(chord_tone(ch[b],pat[k])),W_TRI,0.05f,2.5f,0.5f,0); }
  }
  song_melody(&s,0,"F#5:8 E5:4 D5:4 B4:8 D5:4 E5:4 D5:8 B4:4 G4:4 A4:16 "
                   "F#5:8 A5:4 F#5:4 E5:8 D5:4 B4:4 D5:6 E5:2 D5:4 B4:4 A4:16",W_SIN,0.06f,1.4f,0.5f,0.006f,0);
  return song_finish(&s);
}

/* ---------------- musique des cinematiques ----------------
   Partitions en sections calees sur les plans (80 battements/min : une mesure = 3 s).
   Ces pistes se jouent une seule fois : les notes ne bouclent pas en fin de piste. */
static void cnote(Song *s,float step,float len,float m,int wave,float vol,float decp,float att,float lp,float vib){
  SP p=sp(wave,midi(m),midi(m),len*s->spb,vol); p.decp=decp; p.att=att>0?att:0.004f; p.lp=lp>0?lp:1.0f; p.duty=wave==W_SQ?0.25f:0.5f;
  if(vib>0){ p.vibf=5.0f; p.vibd=vib; }
  int n=(int)(p.dur*AUDIO_RATE)+1; float *tmp=(float*)calloc((size_t)n,4);
  synth_into(tmp,n,0,p);
  int off=(int)(step*s->spb*AUDIO_RATE);
  for(int i=0;i<n;i++){ int k=off+i; if(k>=s->n) break; s->f[k]+=tmp[i]; }
  free(tmp);
}
static void cdrum(Song *s,float step,int kind,float vol){
  float dur = kind==0?0.30f:(kind==1?0.22f:(kind==2?0.05f:0.30f));
  int n=(int)(dur*AUDIO_RATE); int off=(int)(step*s->spb*AUDIO_RATE); float ph=0, prev=0;
  for(int i=0;i<n;i++){
    float t=(float)i/AUDIO_RATE, v=0;
    if(kind==0){ float f=42+105*expf(-t*26); ph+=f/AUDIO_RATE; v=sinf(TAU*ph)*expf(-t*8.5f); }
    else if(kind==1){ float nz=nrand(); ph+=185.0f/AUDIO_RATE; v=nz*expf(-t*15)*0.55f+sinf(TAU*ph)*expf(-t*28)*0.35f; }
    else { float nz=nrand(); float hp=nz-prev; prev=nz; v=hp*expf(-t*(kind==2?70.0f:12.0f))*0.4f; }
    int k=off+i; if(k>=s->n) break; s->f[k]+=v*vol;
  }
}
/* coup de timbale grave (sinus qui descend + souffle) */
static void cboom(Song *s,float step,float vol){
  int n=(int)(1.6f*AUDIO_RATE); int off=(int)(step*s->spb*AUDIO_RATE); float ph=0, lp=0;
  for(int i=0;i<n;i++){ float t=(float)i/AUDIO_RATE; float f=34+70*expf(-t*9); ph+=f/AUDIO_RATE;
    float nz=nrand(); lp+=0.04f*(nz-lp);
    float v=sinf(TAU*ph)*expf(-t*2.6f)*0.9f + lp*expf(-t*3.5f)*1.6f;
    int k=off+i; if(k>=s->n) break; s->f[k]+=v*vol; }
}
/* cymbale : souffle aigu a longue extinction */
static void ccrash(Song *s,float step,float vol){
  int n=(int)(2.2f*AUDIO_RATE); int off=(int)(step*s->spb*AUDIO_RATE); float prev=0, lp=0;
  for(int i=0;i<n;i++){ float t=(float)i/AUDIO_RATE; float nz=nrand(); float hp=nz-prev; prev=nz; lp+=0.5f*(hp-lp);
    float v=lp*expf(-t*1.8f)*0.5f; int k=off+i; if(k>=s->n) break; s->f[k]+=v*vol; }
}
static void cchord(Song *s,float step,float len,Chord c,int wave,float vol,float decp,float att,int oct,float lp){
  for(int i=0;i<3;i++) cnote(s,step,len,(float)(chord_tone(c,i)+oct),wave,vol,decp,att,lp,0);
}
static void cmelody(Song *s,float start,const char *str,int wave,float vol,float decp,float att,float vib,int transpose){
  const char *p=str; float step=start;
  while(*p){
    while(*p==' '||*p=='|') p++; if(!*p) break;
    int m=-1;
    if(*p=='R'){ p++; }
    else { static const int base[7]={9,11,0,2,4,5,7}; int n=base[*p-'A']; p++;
      if(*p=='#'){ n++; p++; } else if(*p=='b'){ n--; p++; }
      int oct=*p-'0'; p++; m=12*(oct+1)+n+transpose; }
    int len=0; if(*p==':'){ p++; while(*p>='0'&&*p<='9'){ len=len*10+(*p-'0'); p++; } }
    if(m>=0) cnote(s,step,(float)len*0.94f,(float)m,wave,vol,decp,att,1.0f,vib);
    step+=len;
  }
}
enum { SEC_CALM, SEC_QUAKE, SEC_EYE, SEC_HORDE, SEC_HEROES, SEC_ACTION, SEC_GUARDS, SEC_DESCENT, SEC_TITLE,
       SEC_SHATTER, SEC_SILENCE, SEC_MONTAGE, SEC_DAWN, SEC_FEAST, SEC_TEASER, SEC_FIN };
typedef struct { int kind, bars; } Sec;
static void hats(Song *s,float st,int every,float vol){ for(int k=0;k<16;k+=every) cdrum(s,st+k,2,vol*((k%4)==2?1.0f:0.7f)); }
static void sec_write(Song *s,int kind,float st,int bars){
  switch(kind){
    case SEC_CALM: { /* la vallee endormie : berceuse sur le theme */
      static const Chord C[4]={{57,1},{53,0},{55,0},{57,1}};
      for(int b=0;b<bars;b++){ Chord c=C[b%4]; float t=st+b*16;
        cchord(s,t,16,c,W_TRI,0.040f,0.7f,0.35f,0,1);
        cnote(s,t,16,(float)(c.root-24),W_SIN,0.12f,0.9f,0.05f,1,0);
        for(int k=0;k<8;k++){ static const int pat[8]={0,1,2,3,2,1,2,1}; cnote(s,t+k*2,3,(float)(chord_tone(c,pat[k])+12),W_SIN,0.032f,3.0f,0,1,0); }
      }
      cmelody(s,st,"E5:8 C5:4 A4:4 | F5:8 E5:4 C5:4 | D5:8 B4:8",W_SIN,0.065f,1.5f,0.02f,0.004f,0);
    } break;
    case SEC_QUAKE: { /* la terre gronde puis s'ouvre (effondrement : 2e mesure, 13e pas) */
      cnote(s,st,48,33,W_SAW,0.055f,0.6f,1.2f,0.08f,0);
      { SP p=sp(W_NOISE,midi(28),midi(34),5.0f,0.11f); p.att=4.0f; p.decp=1.5f; p.lp=0.03f;
        int n=(int)(p.dur*AUDIO_RATE)+1; float *tmp=(float*)calloc((size_t)n,4); synth_into(tmp,n,0,p);
        int off=(int)(st*s->spb*AUDIO_RATE); for(int i=0;i<n;i++){ int k=off+i; if(k>=s->n) break; s->f[k]+=tmp[i]; } free(tmp); }
      cboom(s,st+8,0.45f); cboom(s,st+13,0.4f);
      cboom(s,st+16,0.5f); cboom(s,st+22,0.48f); cboom(s,st+25,0.55f); cboom(s,st+27,0.6f);
      cboom(s,st+28,1.0f); ccrash(s,st+28,0.65f);
      cnote(s,st+28,20,45,W_SAW,0.05f,1.4f,0.01f,0.2f,0); cnote(s,st+28,20,46,W_SAW,0.045f,1.4f,0.01f,0.2f,0); cnote(s,st+28,20,52,W_SAW,0.04f,1.4f,0.01f,0.2f,0);
      cboom(s,st+40,0.35f);
    } break;
    case SEC_EYE: { /* l'Oeil s'ouvre (2e mesure) : battements de coeur, aigus inquietants */
      static const Chord C[3]={{50,1},{46,0},{45,0}};
      for(int b=0;b<bars;b++){ Chord c=C[b%3]; float t=st+b*16;
        cchord(s,t,16,c,W_TRI,0.045f,0.6f,0.8f,0,1);
        cnote(s,t,16,(float)(c.root-12),W_SIN,0.11f,0.8f,0.2f,1,0);
        cdrum(s,t,0,0.55f); cdrum(s,t+3,0,0.35f); cdrum(s,t+8,0,0.55f); cdrum(s,t+11,0,0.35f);
      }
      cnote(s,st,16,93,W_SIN,0.018f,0.8f,0.6f,1,0.02f); cnote(s,st+16,16,92,W_SIN,0.02f,0.8f,0.3f,1,0.02f); cnote(s,st+32,16,89,W_SIN,0.02f,0.8f,0.3f,1,0.02f);
      cboom(s,st+16,0.8f); ccrash(s,st+16,0.3f);
      cnote(s,st+16,24,38,W_SAW,0.06f,1.0f,0.05f,0.15f,0); cnote(s,st+16,24,39,W_SAW,0.05f,1.0f,0.05f,0.15f,0);
    } break;
    case SEC_HORDE: { /* les creatures remontent : ostinato */
      static const int OST[8]={0,0,3,0,7,0,5,3};
      for(int b=0;b<bars;b++){ float t=st+b*16; int root= b==2?41:45;
        for(int k=0;k<8;k++) cnote(s,t+k*2,1.8f,(float)(root+OST[k]),W_TRI,0.15f,0.8f,0,1,0);
        cdrum(s,t,0,0.5f); cdrum(s,t+8,0,0.5f); if(b) cdrum(s,t+10,0,0.4f);
        cdrum(s,t+4,1,0.32f); cdrum(s,t+12,1,0.32f);
        hats(s,t,b==2?1:2,0.12f);
        Chord c={root+12,b==2?0:1}; for(int k=0;k<3;k++) cchord(s,t+k*3,1,c,W_SQ,0.022f,1.5f,0,12,1);
      }
      cmelody(s,st,"A3:6 C4:2 B3:4 G#3:4 | A3:6 E4:2 D4:4 C4:4 | F3:6 A3:2 C4:4 E4:4",W_SAW,0.04f,0.9f,0.01f,0,0);
    } break;
    case SEC_HEROES: { /* les gardiens de la vallee : le theme heroique */
      static const Chord C[3]={{53,0},{55,0},{57,0}};
      for(int b=0;b<bars;b++){ Chord c=C[b%3]; float t=st+b*16;
        cchord(s,t,16,c,W_TRI,0.04f,0.8f,0.1f,0,1);
        for(int k=0;k<4;k++) cnote(s,t+k*4,3.5f,(float)(c.root-12),W_TRI,0.15f,1.0f,0,1,0);
        cdrum(s,t,0,0.5f); cdrum(s,t+8,0,0.5f); cdrum(s,t+4,1,0.33f); cdrum(s,t+12,1,0.33f); hats(s,t,2,0.11f);
        if(b==2){ cdrum(s,t+13,1,0.25f); cdrum(s,t+14,1,0.3f); cdrum(s,t+15,1,0.35f); }
      }
      cmelody(s,st,"C5:4 F5:4 A5:6 G5:2 | D5:4 G5:4 B5:6 A5:2 | C#6:6 B5:2 A5:4 E5:4",W_SQ,0.07f,0.7f,0.005f,0.004f,0);
    } break;
    case SEC_ACTION: { /* les billes rebondissent */
      static const Chord C[3]={{50,0},{52,0},{57,0}};
      for(int b=0;b<bars;b++){ Chord c=C[b%3]; float t=st+b*16;
        for(int k=0;k<16;k++){ cnote(s,t+k,0.9f,(float)(c.root-12),W_TRI,0.12f,1.0f,0,1,0);
          static const int pat[4]={0,1,2,1}; cnote(s,t+k,0.9f,(float)(chord_tone(c,pat[k&3])+12),W_SQ,0.026f,2.0f,0,1,0); }
        cdrum(s,t,0,0.5f); cdrum(s,t+3,0,0.4f); cdrum(s,t+8,0,0.5f); cdrum(s,t+11,0,0.4f);
        cdrum(s,t+4,1,0.35f); cdrum(s,t+12,1,0.35f); hats(s,t,1,0.11f);
      }
      cmelody(s,st,"F#5:2 A5:2 D6:4 C#6:2 A5:2 F#5:4 | G#5:2 B5:2 E6:4 D6:2 B5:2 G#5:4 | A5:2 C#6:2 E6:4 A6:8",W_SQ,0.068f,0.7f,0.004f,0.004f,0);
    } break;
    case SEC_GUARDS: { /* les quatre gardiens : un coup par carte */
      static const Chord C[4]={{45,1},{41,0},{38,1},{40,0}};
      for(int b=0;b<bars;b++){ Chord c=C[b%4]; float t=st+b*16;
        cboom(s,t,0.75f+0.06f*b); ccrash(s,t,0.32f+0.05f*b);
        cchord(s,t,10,c,W_SAW,0.045f,1.3f,0.005f,12,0.25f);
        cnote(s,t,16,(float)c.root,W_SIN,0.15f,0.7f,0.01f,1,0);
        cnote(s,t,16,(float)(c.root+12),W_TRI,0.03f,0.5f,0.3f,1,0);
        if(b<bars-1){ cdrum(s,t+13,0,0.25f); cdrum(s,t+14,0,0.3f); cdrum(s,t+15,0,0.36f); }
      }
      cnote(s,st+48,16,93,W_SIN,0.02f,0.7f,0.2f,1,0.02f);
    } break;
    case SEC_DESCENT: { /* la chute : tout accelere */
      static const int SC[7]={0,2,3,5,7,8,10};
      for(int i=0;i<32;i++){ int deg=i%7, oct=i/7; cnote(s,st+i,0.9f,(float)(57+SC[deg]+12*oct),W_SQ,0.045f,1.5f,0,1,0); }
      for(int k=0;k<16;k++) cnote(s,st+k*2,1.8f,45,W_TRI,0.14f,0.8f,0,1,0);
      for(int b=0;b<2;b++){ float t=st+b*16; for(int k=0;k<16;k+=4) cdrum(s,t+k,0,0.5f); hats(s,t,1,0.12f); }
      cdrum(s,st+4,1,0.3f); cdrum(s,st+12,1,0.3f);
      for(int k=8;k<16;k++) cdrum(s,st+16+k,1,0.15f+0.04f*(k-8));
      { SP p=sp(W_NOISE,midi(40),midi(84),6.0f,0.09f); p.att=5.6f; p.decp=1; p.lp=0.3f;
        int n=(int)(p.dur*AUDIO_RATE)+1; float *tmp=(float*)calloc((size_t)n,4); synth_into(tmp,n,0,p);
        int off=(int)(st*s->spb*AUDIO_RATE); for(int i=0;i<n;i++){ int k=off+i; if(k>=s->n) break; s->f[k]+=tmp[i]; } free(tmp); }
    } break;
    case SEC_TITLE: { /* GOUFFRE : le grand accord */
      cboom(s,st,1.0f); ccrash(s,st,0.7f);
      static const int AC[6]={45,52,57,60,64,71};
      for(int i=0;i<6;i++) cnote(s,st,32,(float)AC[i],W_TRI,0.05f,0.5f,0.01f,1,0);
      Chord am={57,1}; cchord(s,st,24,am,W_SQ,0.022f,1.0f,0.01f,0,1);
      cnote(s,st,32,33,W_SIN,0.2f,0.4f,0.01f,1,0);
      cmelody(s,st+4,"E5:8 C5:4 A4:12",W_SIN,0.08f,1.2f,0.02f,0.005f,0);
    } break;
    case SEC_SHATTER: { /* l'Oeil se brise (3e mesure) */
      for(int i=0;i<32;i++){ float v=0.015f+0.035f*i/31.0f; cnote(s,st+i,0.95f,(float)((i&1)?46:45),W_SAW,v,1.0f,0,0.2f,0); }
      for(int i=0;i<32;i+=2) cdrum(s,st+i,0,0.15f+0.45f*i/31.0f);
      cnote(s,st,32,33,W_SAW,0.05f,0.7f,2.0f,0.08f,0);
      cboom(s,st+32,1.0f); ccrash(s,st+32,0.8f);
      { SP p=sp(W_NOISE,midi(90),midi(70),1.2f,0.2f); p.decp=2; int n=(int)(p.dur*AUDIO_RATE)+1; float *tmp=(float*)calloc((size_t)n,4); synth_into(tmp,n,0,p);
        int off=(int)((st+32)*s->spb*AUDIO_RATE); for(int i=0;i<n;i++){ int k=off+i; if(k>=s->n) break; s->f[k]+=tmp[i]; } free(tmp); }
      static const int BELL[4]={81,85,88,93}; for(int i=0;i<4;i++) cnote(s,st+32+i*0.5f,16,(float)BELL[i],W_SIN,0.04f,1.2f,0,1,0);
      Chord a={57,0}; cchord(s,st+34,14,a,W_TRI,0.03f,0.6f,0.5f,0,1);
    } break;
    case SEC_SILENCE: case SEC_MONTAGE: { /* les creatures s'eteignent / les Profondeurs s'illuminent */
      static const Chord C[3]={{57,0},{54,1},{50,0}};
      static const Chord M[3]={{57,0},{52,0},{54,1}};
      for(int b=0;b<bars;b++){ Chord c= kind==SEC_MONTAGE ? M[b%3] : C[b%3]; float t=st+b*16;
        cchord(s,t,16,c,W_SIN,0.03f,0.6f,0.8f,12,1);
        cnote(s,t,16,(float)(chord_tone(c,1)+24),W_SIN,0.012f,0.6f,0.9f,1,0.01f);
        cnote(s,t,16,(float)(c.root-24),W_SIN,0.08f,0.7f,0.3f,1,0);
        for(int k=0;k<8;k++){ static const int pat[8]={0,2,1,3,0,2,1,2}; cnote(s,t+k*2,4,(float)(chord_tone(c,pat[k])+24),W_SIN,0.022f,3.0f,0,1,0); }
        if(kind==SEC_MONTAGE){ cdrum(s,t,0,0.22f); cdrum(s,t+8,0,0.2f); hats(s,t,2,0.05f); }
      }
      if(kind==SEC_MONTAGE){ static const int CH[4]={88,93,97,100}; static const float AT[4]={0,11,21,32};
        for(int i=0;i<4;i++) cnote(s,st+AT[i],12,(float)CH[i],W_SIN,0.045f,1.6f,0,1,0); }
    } break;
    case SEC_DAWN: { /* l'aube : le theme en majeur */
      static const Chord C[4]={{53,0},{55,0},{60,0},{60,0}};
      for(int b=0;b<bars;b++){ Chord c=C[b%4]; float t=st+b*16;
        cchord(s,t,16,c,W_TRI,0.045f,0.7f,0.3f,0,1);
        cnote(s,t,8,(float)(c.root-12),W_SIN,0.14f,1.0f,0.02f,1,0); cnote(s,t+8,8,(float)(c.root-12),W_SIN,0.12f,1.0f,0.02f,1,0);
        for(int k=0;k<8;k++){ static const int pat[8]={0,1,2,3,2,1,2,1}; cnote(s,t+k*2,3,(float)(chord_tone(c,pat[k])+12),W_SIN,0.03f,3.0f,0,1,0); }
        if(b>=1){ cdrum(s,t,0,0.3f); cdrum(s,t+8,0,0.3f); cdrum(s,t+4,2,0.06f); cdrum(s,t+12,2,0.06f); }
      }
      cmelody(s,st,"A4:4 C5:4 F5:8 | B4:4 D5:4 G5:8 | C5:4 E5:4 G5:4 C6:4 | B5:8 C6:8",W_SIN,0.075f,1.4f,0.02f,0.004f,0);
    } break;
    case SEC_FEAST: { /* la fete au campement : une gigue */
      static const Chord C[4]={{62,0},{55,0},{57,0},{62,0}};
      for(int b=0;b<bars;b++){ Chord c=C[b%4]; float t=st+b*16;
        static const int BS[4]={0,6,8,14}; for(int k=0;k<4;k++) cnote(s,t+BS[k],1.5f,(float)(c.root-12-(c.root>=60?12:0)),W_TRI,0.16f,0.9f,0,1,0);
        for(int k=2;k<16;k+=4) cchord(s,t+k,1,c,W_SQ,0.02f,1.5f,0,0,1);
        cdrum(s,t,0,0.45f); cdrum(s,t+8,0,0.45f); cdrum(s,t+4,1,0.3f); cdrum(s,t+12,1,0.3f); hats(s,t,2,0.12f); hats(s,t+1,2,0.05f);
      }
      cmelody(s,st,"F#5:2 A5:2 D6:2 A5:2 F#5:2 A5:2 D6:4 | G5:2 B5:2 D6:2 B5:2 G5:2 B5:2 D6:4 | A5:2 C#6:2 E6:2 C#6:2 A5:2 C#6:2 E6:4 | D6:4 A5:4 F#5:4 D5:4",W_SQ,0.068f,0.7f,0.004f,0.004f,0);
    } break;
    case SEC_TEASER: { /* mais quelque chose bat encore... (2e legende : 2e mesure, 14e pas) */
      cnote(s,st,48,45,W_SIN,0.06f,0.3f,1.0f,1,0);
      for(int b=0;b<bars;b++){ float t=st+b*16; float k=b==2?1.3f:1.0f; cdrum(s,t,0,0.45f*k); cdrum(s,t+3,0,0.3f*k); cdrum(s,t+8,0,0.45f*k); cdrum(s,t+11,0,0.3f*k); }
      cmelody(s,st,"E7:6 B6:6 C7:4 | R:4 A6:6 E6:6",W_SIN,0.03f,2.5f,0,0,0);
      cboom(s,st+29,0.5f);
      cnote(s,st+29,19,45,W_SAW,0.05f,1.0f,0.3f,0.15f,0); cnote(s,st+29,19,46,W_SAW,0.045f,1.0f,0.3f,0.15f,0);
    } break;
    case SEC_FIN: { /* FIN : reprise du theme, accord final majeur */
      static const Chord C[5]={{57,1},{53,0},{60,0},{52,0},{57,0}};
      for(int b=0;b<bars;b++){ Chord c=C[b%5]; float t=st+b*16; float len= b==bars-1?32:16;
        cchord(s,t,len,c,W_TRI,0.045f,b==bars-1?0.4f:0.7f,0.3f,0,1);
        cnote(s,t,len,(float)(c.root-12),W_SIN,0.13f,0.8f,0.02f,1,0);
        if(b<bars-1) for(int k=0;k<8;k++){ static const int pat[8]={0,1,2,3,2,1,2,1}; cnote(s,t+k*2,3,(float)(chord_tone(c,pat[k])+12),W_SIN,0.03f,3.0f,0,1,0); }
        if(b>=1 && b<bars-1){ cdrum(s,t,0,0.25f); cdrum(s,t+8,0,0.25f); }
      }
      cmelody(s,st,"E5:8 C5:4 A4:4 | F5:8 C5:4 A4:4 | G5:8 E5:4 C5:4 | G#5:8 E5:4 B4:4 | A5:24",W_SIN,0.08f,1.3f,0.02f,0.004f,0);
      cmelody(s,st+64,"C#5:24",W_SIN,0.04f,1.0f,0.05f,0.004f,0);
    } break;
  }
}
static Buf build_cine(const Sec *sec,int n){
  int bars=0; for(int i=0;i<n;i++) bars+=sec[i].bars;
  Song s; float bpm=80; s.spb=60.0f/bpm/4; s.n=(int)((bars+2)*16*s.spb*AUDIO_RATE); s.f=(float*)calloc((size_t)s.n,4);
  float st=0; for(int i=0;i<n;i++){ sec_write(&s,sec[i].kind,st,sec[i].bars); st+=sec[i].bars*16; }
  /* niveau : on vise le volume moyen des autres musiques, les coups de timbale sont arrondis par une saturation douce */
  double sum=0; long cnt=0; for(int i=0;i<s.n;i++){ float v=fabsf(s.f[i]); if(v>1e-4f){ sum+=(double)v*v; cnt++; } }
  float rms=cnt?(float)sqrt(sum/(double)cnt):1e-3f; float k=0.16f/fmaxf(rms,1e-5f);
  for(int i=0;i<s.n;i++){ float v=s.f[i]*k; v=clampf(v,-3,3); v=v*(27+v*v)/(27+9*v*v); s.f[i]=v*0.92f; }
  Buf b=to_buf(s.f,s.n); free(s.f); return b;
}
/* construit une piste de cinematique a la demande (la premiere fois qu'on la joue) */
void music_prepare(int id){
  if(id<MUS_INTRO || id>MUS_FIN2 || MB[id].d) return;
  static const Sec INTRO[]={{SEC_CALM,3},{SEC_QUAKE,3},{SEC_EYE,3},{SEC_HORDE,3},{SEC_HEROES,3},{SEC_ACTION,3},{SEC_GUARDS,4},{SEC_DESCENT,2},{SEC_TITLE,2}};
  static const Sec FIN[]={{SEC_SHATTER,3},{SEC_SILENCE,3},{SEC_DAWN,4},{SEC_FEAST,4},{SEC_TEASER,3},{SEC_FIN,5}};
  static const Sec FIN2[]={{SEC_SHATTER,3},{SEC_MONTAGE,3},{SEC_DAWN,4},{SEC_FEAST,4},{SEC_FIN,5}};
  if(id==MUS_INTRO) MB[id]=build_cine(INTRO,COUNTOF(INTRO));
  else if(id==MUS_FIN) MB[id]=build_cine(FIN,COUNTOF(FIN));
  else MB[id]=build_cine(FIN2,COUNTOF(FIN2));
}

/* ---------------- musiques des niveaux (v6.6) ----------------
   Un theme par niveau, dans l'humeur du lieu : danse macabre des Catacombes, clairiere de la Foret
   Fongique, enclumes des Forges, coeur qui bat au fond du Gouffre, cristaux du Givre, cloches noyees de la
   Cite Engloutie, bal des abeilles de la Ruche, requiem du Neant ; (v7.0) berceuse du Jardin des Lucioles,
   brumes du Marais maudit, galerie scintillante des Mines de cristal, hymne du Sanctuaire solaire.
   Chaque piste boucle sans couture : les notes qui depassent la fin reprennent au debut, l'echo aussi. */
static Song song_new(float bpm,int steps){ Song s; s.spb=60.0f/bpm/4; s.n=(int)(steps*s.spb*AUDIO_RATE); s.f=(float*)calloc((size_t)s.n,4); return s; }
static void addsp(Song *s,float step,SP p){
  int n=(int)(p.dur*AUDIO_RATE)+1; if(n>s->n) n=s->n;
  float *tmp=(float*)calloc((size_t)n,4); synth_into(tmp,n,0,p);
  int off=((int)(step*s->spb*AUDIO_RATE))%s->n; if(off<0) off+=s->n;
  for(int i=0;i<n;i++){ int k=off+i; if(k>=s->n) k-=s->n; s->f[k]+=tmp[i]; }
  free(tmp);
}
/* note complete : attaque, extinction, filtre, vibrato */
static void lnote(Song *s,float step,float len,float m,int wave,float vol,float att,float decp,float lp,float vib,float duty){
  if(len<=0 || vol<=0) return;
  SP p=sp(wave,midi(m),midi(m),len*s->spb,vol); p.att=att>0?att:0.004f; p.decp=decp; p.lp=lp>0?lp:1.0f; p.duty=duty>0?duty:0.5f;
  if(vib>0){ p.vibf=5.2f; p.vibd=vib; }
  addsp(s,step,p);
}
static void lglide(Song *s,float step,float dur_s,float m0,float m1,int wave,float vol,float att,float decp,float lp,float vib,float vibf){
  SP p=sp(wave,midi(m0),midi(m1),dur_s,vol); p.att=att; p.decp=decp; p.lp=lp; if(vib>0){ p.vibf=vibf; p.vibd=vib; }
  addsp(s,step,p);
}
/* ---- instruments ---- */
typedef void (*Inst)(Song*,float st,float len,float m,float vol);
static void i_pluck(Song *s,float st,float len,float m,float vol){ (void)len;   /* harpe */
  SP p=sp(W_TRI,midi(m),midi(m),0.9f,vol); p.decp=3.2f; p.lp=0.55f; p.att=0.002f; addsp(s,st,p);
  SP q=sp(W_SIN,midi(m+12),midi(m+12),0.35f,vol*0.25f); q.decp=3.0f; addsp(s,st,q); }
static void i_pizz(Song *s,float st,float len,float m,float vol){ (void)len;    /* pizzicato grave */
  SP p=sp(W_TRI,midi(m),midi(m),0.32f,vol); p.decp=3.5f; p.lp=0.35f; p.att=0.002f; addsp(s,st,p);
  SP q=sp(W_SQ,midi(m),midi(m),0.08f,vol*0.25f); q.duty=0.3f; q.lp=0.2f; q.decp=2.0f; addsp(s,st,q); }
static void i_xylo(Song *s,float st,float len,float m,float vol){ (void)len;    /* xylophone d'os */
  SP a=sp(W_SIN,midi(m),midi(m),0.26f,vol); a.decp=5.0f; a.att=0.001f; addsp(s,st,a);
  SP b=sp(W_SIN,midi(m)*3.0f,midi(m)*3.0f,0.07f,vol*0.32f); b.decp=6.0f; b.att=0.001f; addsp(s,st,b);
  SP c=sp(W_NOISE,4000,4000,0.012f,vol*0.35f); c.lp=0.6f; c.att=0.0005f; addsp(s,st,c); }
static void i_bell(Song *s,float st,float len,float m,float vol){             /* cloche : partiels inharmoniques */
  float d=1.4f+len*s->spb*0.6f;
  SP a=sp(W_SIN,midi(m),midi(m),d,vol); a.decp=2.4f; a.att=0.002f; addsp(s,st,a);
  SP b=sp(W_SIN,midi(m)*2.76f,midi(m)*2.76f,d*0.55f,vol*0.35f); b.decp=3.0f; b.att=0.002f; addsp(s,st,b);
  SP c=sp(W_SIN,midi(m)*5.40f,midi(m)*5.40f,d*0.3f,vol*0.14f); c.decp=3.5f; c.att=0.002f; addsp(s,st,c); }
static void i_mbox(Song *s,float st,float len,float m,float vol){ (void)len;    /* boite a musique un peu faussee */
  SP a=sp(W_SIN,midi(m+0.18f),midi(m+0.18f),0.9f,vol); a.decp=3.0f; a.att=0.001f; addsp(s,st,a);
  SP b=sp(W_SIN,midi(m+12.1f),midi(m+12.1f),0.35f,vol*0.3f); b.decp=4.0f; b.att=0.001f; addsp(s,st,b); }
static void i_organ(Song *s,float st,float len,float m,float vol){            /* orgue de crypte */
  lnote(s,st,len,m,W_SQ,vol*0.55f,0.012f,0.35f,0.16f,0,0.5f);
  lnote(s,st,len,m+12,W_SQ,vol*0.22f,0.012f,0.35f,0.22f,0,0.25f);
  lnote(s,st,len,m-12,W_SIN,vol*0.45f,0.012f,0.35f,1,0,0); }
static void i_stab(Song *s,float st,float len,float m,float vol){             /* accord d'orgue bref */
  lnote(s,st,len,m,W_SQ,vol*0.5f,0.004f,1.6f,0.2f,0,0.5f); lnote(s,st,len,m+12,W_SIN,vol*0.3f,0.004f,1.8f,1,0,0); }
static void i_flute(Song *s,float st,float len,float m,float vol){            /* flute : sinus, souffle, vibrato */
  lnote(s,st,len,m,W_SIN,vol,0.045f,0.7f,1,0.006f,0);
  lnote(s,st,len,m+12,W_TRI,vol*0.08f,0.05f,0.9f,0.5f,0.006f,0);
  SP b=sp(W_NOISE,midi(m),midi(m),minf(0.35f,len*s->spb),vol*0.07f); b.att=0.03f; b.decp=1.5f; b.lp=0.12f; addsp(s,st,b); }
static void i_glass(Song *s,float st,float len,float m,float vol){            /* verre frotte (givre) */
  lnote(s,st,len,m,W_SIN,vol,0.16f,0.8f,1,0.004f,0); lnote(s,st,len,m+12,W_SIN,vol*0.14f,0.2f,0.9f,1,0.004f,0); lnote(s,st,len,m+19,W_SIN,vol*0.05f,0.25f,1.0f,1,0,0); }
static void i_brass(Song *s,float st,float len,float m,float vol){            /* cuivres */
  lnote(s,st,len,m,W_SAW,vol*0.7f,0.03f,0.55f,0.26f,0.004f,0); lnote(s,st,len,m,W_SQ,vol*0.35f,0.03f,0.6f,0.3f,0.004f,0.3f);
  lnote(s,st,len,m-12,W_SAW,vol*0.25f,0.04f,0.6f,0.15f,0,0); }
static void i_horn(Song *s,float st,float len,float m,float vol){             /* cor lointain (Neant) : attaque lente */
  float att=minf(0.35f,len*s->spb*0.3f);
  lnote(s,st,len,m,W_SAW,vol*0.6f,att,0.5f,0.18f,0.005f,0); lnote(s,st,len,m,W_TRI,vol*0.5f,att,0.5f,0.6f,0.005f,0);
  lnote(s,st,len,m-12,W_SIN,vol*0.3f,att,0.5f,1,0,0); }
static void i_buzz(Song *s,float st,float len,float m,float vol){             /* abeille : scie et vibrato rapide */
  SP p=sp(W_SAW,midi(m),midi(m),len*s->spb*0.95f,vol*0.75f); p.att=0.004f; p.decp=0.8f; p.lp=0.34f; p.vibf=23; p.vibd=0.011f; addsp(s,st,p);
  SP q=sp(W_SQ,midi(m),midi(m),len*s->spb*0.95f,vol*0.3f); q.duty=0.15f; q.att=0.004f; q.decp=0.9f; q.lp=0.4f; q.vibf=23; q.vibd=0.011f; addsp(s,st,q); }
static void i_lead(Song *s,float st,float len,float m,float vol){             /* lead carre (refrain de la Ruche) */
  lnote(s,st,len,m,W_SQ,vol,0.006f,0.8f,0.45f,0.005f,0.3f); lnote(s,st,len,m+12,W_SIN,vol*0.15f,0.006f,1.0f,1,0,0); }
static void i_epiano(Song *s,float st,float len,float m,float vol){           /* piano electrique sous l'eau */
  float d=maxf(len*s->spb,0.7f)*1.3f;
  SP a=sp(W_SIN,midi(m),midi(m),d,vol); a.decp=1.8f; a.att=0.003f; a.vibf=4.5f; a.vibd=0.0035f; addsp(s,st,a);
  SP b=sp(W_SIN,midi(m+0.09f),midi(m+0.09f),d,vol*0.55f); b.decp=1.6f; b.att=0.003f; addsp(s,st,b);
  SP c=sp(W_TRI,midi(m+12),midi(m+12),d*0.4f,vol*0.18f); c.decp=3.0f; c.att=0.002f; addsp(s,st,c); }
static void i_choir(Song *s,float st,float len,float m,float vol){            /* choeur « aah » */
  float att=len*s->spb*0.32f;
  lnote(s,st,len,m-0.1f,W_TRI,vol*0.35f,att,0.55f,0.32f,0.007f,0);
  lnote(s,st,len,m,W_SIN,vol*0.45f,att,0.55f,1,0.006f,0);
  lnote(s,st,len,m+0.1f,W_TRI,vol*0.35f,att,0.55f,0.32f,0.007f,0); }
static void i_pad(Song *s,float st,float len,float m,float vol){              /* nappe douce */
  float att=len*s->spb*0.3f;
  lnote(s,st,len,m-0.06f,W_TRI,vol*0.5f,att,0.6f,0.4f,0.003f,0); lnote(s,st,len,m+0.06f,W_SIN,vol*0.5f,att,0.6f,1,0.003f,0); }
static void i_string(Song *s,float st,float len,float m,float vol){           /* cordes */
  float att=len*s->spb*0.25f;
  lnote(s,st,len,m-0.06f,W_SAW,vol*0.4f,att,0.5f,0.2f,0.004f,0); lnote(s,st,len,m+0.06f,W_SAW,vol*0.4f,att,0.5f,0.2f,0.004f,0); }
static void i_sub(Song *s,float st,float len,float m,float vol){ lnote(s,st,len,m,W_SIN,vol,0.04f,0.5f,1,0,0); }
static void i_bsaw(Song *s,float st,float len,float m,float vol){             /* basse des forges */
  lnote(s,st,len,m,W_SAW,vol,0.003f,1.1f,0.2f,0,0); lnote(s,st,len,m,W_SQ,vol*0.45f,0.003f,1.2f,0.12f,0,0.5f); }
static void i_bsq(Song *s,float st,float len,float m,float vol){ lnote(s,st,len,m,W_SQ,vol,0.003f,1.6f,0.32f,0,0.25f); lnote(s,st,len,m-12,W_SIN,vol*0.5f,0.003f,1.4f,1,0,0); }
static void i_anvil(Song *s,float st,float len,float m,float vol){ (void)len;   /* enclume : metal qui sonne */
  static const float R[5]={1.0f,2.41f,3.83f,5.21f,6.9f}, V[5]={1.0f,0.6f,0.45f,0.3f,0.2f}, D[5]={0.9f,0.6f,0.45f,0.32f,0.22f};
  for(int k=0;k<5;k++){ SP a=sp(W_SIN,midi(m)*R[k],midi(m)*R[k],D[k],vol*V[k]); a.decp=2.6f; a.att=0.001f; addsp(s,st,a); }
  SP c=sp(W_NOISE,6000,6000,0.03f,vol*0.5f); c.att=0.0005f; c.decp=2; addsp(s,st,c); }
static void i_drop(Song *s,float st,float len,float m,float vol){ (void)len;    /* goutte, bulle */
  SP a=sp(W_SIN,midi(m),midi(m)*1.9f,0.11f,vol); a.decp=2.0f; a.att=0.003f; addsp(s,st,a); }
static void i_tom(Song *s,float st,float len,float m,float vol){ (void)len;
  SP a=sp(W_SIN,midi(m)*1.7f,midi(m),0.4f,vol); a.decp=2.4f; a.att=0.001f; addsp(s,st,a);
  SP b=sp(W_NOISE,900,300,0.08f,vol*0.25f); b.lp=0.25f; b.decp=2; addsp(s,st,b); }
static void i_wind(Song *s,float st,float len,float m,float vol){             /* souffle de vent */
  SP p=sp(W_NOISE,midi(m),midi(m+5),len*s->spb,vol); p.att=len*s->spb*0.45f; p.decp=1.2f; p.lp=0.035f; addsp(s,st,p); }
static void i_swell(Song *s,float st,float len,float m,float vol){            /* souffle qui monte jusqu'au temps fort */
  SP p=sp(W_NOISE,midi(m),midi(m+36),len*s->spb,vol); p.att=len*s->spb*0.92f; p.decp=4.0f; p.lp=0.18f; addsp(s,st,p); }
static void i_whistle(Song *s,float st,float len,float m,float vol){          /* sifflement spectral */
  lnote(s,st,len,m,W_SIN,vol,len*s->spb*0.4f,0.7f,1,0.012f,0); }
static void i_croak(Song *s,float st,float len,float m,float vol){ (void)len;    /* coassement : deux salves rauques */
  for(int k=0;k<2;k++){ SP p=sp(W_SQ,midi(m)*1.3f,midi(m)*0.85f,0.12f,vol*(k?0.75f:1.0f)); p.duty=0.2f; p.lp=0.14f; p.decp=1.3f; p.att=0.006f; p.vibf=34; p.vibd=0.09f; addsp(s,st+k*1.1f,p); } }
static void i_boom(Song *s,float step,float vol){                             /* timbale grave */
  int n=(int)(1.6f*AUDIO_RATE); int off=(int)(step*s->spb*AUDIO_RATE); float ph=0, lp=0;
  for(int i=0;i<n;i++){ float t=(float)i/AUDIO_RATE; float f=34+70*expf(-t*9); ph+=f/AUDIO_RATE;
    float nz=nrand(); lp+=0.04f*(nz-lp);
    float v=tsin(ph-floorf(ph))*expf(-t*2.6f)*0.9f + lp*expf(-t*3.5f)*1.6f;
    s->f[(off+i)%s->n]+=v*vol; }
}
static void i_crash(Song *s,float step,float vol){
  int n=(int)(2.2f*AUDIO_RATE); int off=(int)(step*s->spb*AUDIO_RATE); float prev=0, lp=0;
  for(int i=0;i<n;i++){ float t=(float)i/AUDIO_RATE; float nz=nrand(); float hp=nz-prev; prev=nz; lp+=0.5f*(hp-lp);
    s->f[(off+i)%s->n]+=lp*expf(-t*1.8f)*0.5f*vol; }
}
static void i_heart(Song *s,float step,float vol){                            /* battement : « boum-boum » */
  for(int k=0;k<2;k++){ int n=(int)(0.32f*AUDIO_RATE); int off=(int)((step+k*1.2f)*s->spb*AUDIO_RATE); float ph=0; float v0=k?0.65f:1.0f;
    for(int i=0;i<n;i++){ float t=(float)i/AUDIO_RATE; float f=38+42*expf(-t*20); ph+=f/AUDIO_RATE; s->f[(off+i)%s->n]+=tsin(ph-floorf(ph))*expf(-t*9)*vol*v0; } }
}
/* melodie : « D5:4 F5:4 R:2 » (pas de double-croche), jouee par un instrument */
static void lmel(Song *s,float start,const char *str,Inst fn,float vol,int tr){
  const char *p=str; float step=start;
  while(*p){
    while(*p==' '||*p=='|') p++; if(!*p) break;
    int m=-1;
    if(*p=='R') p++;
    else if(*p>='A' && *p<='G'){ static const int base[7]={9,11,0,2,4,5,7}; int n=base[*p-'A']; p++;
      if(*p=='#'){ n++; p++; } else if(*p=='b'){ n--; p++; }
      int oct= (*p>='0'&&*p<='9') ? *p-'0' : 4; if(*p) p++; m=12*(oct+1)+n+tr; }
    else { p++; continue; }
    int len=0; if(*p==':'){ p++; while(*p>='0'&&*p<='9'){ len=len*10+(*p-'0'); p++; } }
    if(m>=0 && len>0) fn(s,step,(float)len,(float)m,vol);
    step+=len;
  }
}
/* echo de salle (petit Freeverb), calcule en boucle : la queue de la fin retombe sur le debut */
static void song_reverb(Song *s,float wet,float room,float damp){
  static const int CL[6]={1116,1188,1277,1356,1422,1491}, AL[3]={556,441,341};
  float *cb[6], st[6]={0}; int ci[6]={0}; float *ab[3]; int ai[3]={0};
  for(int k=0;k<6;k++) cb[k]=(float*)calloc((size_t)CL[k],4);
  for(int k=0;k<3;k++) ab[k]=(float*)calloc((size_t)AL[k],4);
  float *out=(float*)calloc((size_t)s->n,4);
  float hx=0, hy=0;   /* les graves ne sont pas renvoyes dans l'echo (pas de boue) */
  for(int pass=0;pass<2;pass++) for(int i=0;i<s->n;i++){
    hy=0.979f*(hy+s->f[i]-hx); hx=s->f[i];
    float x=hy*0.03f, acc=0;
    for(int k=0;k<6;k++){ float y=cb[k][ci[k]]; st[k]=y*(1-damp)+st[k]*damp; cb[k][ci[k]]=x+st[k]*room; if(++ci[k]>=CL[k]) ci[k]=0; acc+=y; }
    for(int k=0;k<3;k++){ float b=ab[k][ai[k]]; float o=b-acc; ab[k][ai[k]]=acc+b*0.5f; if(++ai[k]>=AL[k]) ai[k]=0; acc=o; }
    if(pass==1) out[i]=acc;
  }
  for(int i=0;i<s->n;i++) s->f[i]=s->f[i]*(1.0f-wet*0.35f)+out[i]*wet*3.0f;
  free(out); for(int k=0;k<6;k++) free(cb[k]); for(int k=0;k<3;k++) free(ab[k]);
}
/* sourdine (sous l'eau) : passe-bas d'ensemble, lui aussi en boucle */
static void song_muffle(Song *s,float a){
  float y=0; for(int pass=0;pass<2;pass++) for(int i=0;i<s->n;i++){ y+=a*(s->f[i]-y); if(pass==1) s->f[i]=y; }
}
/* volume moyen commun a toutes les musiques des niveaux, saturation douce des crêtes */
static Buf song_level(Song *s,float rms_target){
  /* sans composante continue (ondes carrees asymetriques) : passe-haut tres bas, en boucle */
  { float px=0, py=0; for(int pass=0;pass<2;pass++) for(int i=0;i<s->n;i++){ float x=s->f[i]; float y=x-px+0.9985f*py; px=x; py=y; if(pass==1) s->f[i]=y; } }
  double sum=0; for(int i=0;i<s->n;i++) sum+=(double)s->f[i]*s->f[i];
  float rms=(float)sqrt(sum/(double)maxi(1,s->n)); float k=rms_target/fmaxf(rms,1e-6f);
  for(int i=0;i<s->n;i++){ float v=s->f[i]*k; v=clampf(v,-3,3); v=v*(27+v*v)/(27+9*v*v); s->f[i]=v*0.95f; }
  Buf b=to_buf(s->f,s->n); free(s->f); s->f=NULL; return b;
}
/* accords : jusqu'a cinq notes (0 = rien) */
typedef struct { int n[5]; int bass; } LCh;
static void chord_play(Song *s,float st,float len,const LCh *c,Inst fn,float vol,int tr){ for(int i=0;i<5;i++) if(c->n[i]) fn(s,st,len,(float)(c->n[i]+tr),vol); }

/* ---- 1. Les Catacombes : danse macabre (valse en re mineur, xylophone d'os, orgue de crypte) ---- */
static Buf build_lvl_catacombes(void){
  const int BAR=12, BARS=32; Song s=song_new(160,BAR*BARS);
  static const LCh Dm={{53,57,62},38}, A7={{55,61,64},45}, Gm={{55,58,62},43}, Bb={{53,58,62},46};
  const LCh *P[16]={&Dm,&Dm,&A7,&A7,&Dm,&Dm,&Gm,&A7,&Bb,&Gm,&Dm,&A7,&Dm,&Gm,&A7,&Dm};
  for(int b=0;b<BARS;b++){ const LCh *c=P[b%16]; float t=b*BAR; bool two=b>=16;
    i_pizz(&s,t,4,(float)c->bass,0.30f); if(two) i_pizz(&s,t+8,4,(float)(c->bass+7),0.14f);
    chord_play(&s,t+4,2.5f,c,i_stab,0.045f,0); chord_play(&s,t+8,2.5f,c,i_stab,0.04f,0);
    song_drum(&s,t,0,0.32f); song_drum(&s,t+4,2,0.10f); song_drum(&s,t+8,2,0.08f);
    if(two){ static const int ARP[6]={0,1,2,1,2,1}; for(int k=0;k<6;k++) i_xylo(&s,t+k*2,2,(float)(c->n[ARP[k]]+24),0.055f); }
    if(b%4==3) song_drum(&s,t+10,1,0.14f);
  }
  i_organ(&s,0,BAR*4,38,0.05f); i_organ(&s,BAR*16,BAR*4,38,0.05f);
  i_bell(&s,0,24,50,0.16f); i_bell(&s,BAR*16,24,50,0.16f); i_bell(&s,BAR*8,12,74,0.06f); i_bell(&s,BAR*24,12,74,0.06f);
  const char *MEL="D5:4 F5:4 A5:4 | G#5:2 A5:2 Bb5:4 A5:4 | C#5:4 E5:4 A5:4 | G5:6 F5:2 E5:4 | D5:4 F5:4 A5:4 | D6:6 C6:2 Bb5:4 | G5:4 Bb5:4 D6:4 | C#6:8 A5:4 | "
                  "Bb5:4 A5:4 G5:4 | G5:2 F5:2 E5:4 D5:4 | F5:4 E5:4 D5:4 | C#5:8 A4:4 | D5:2 E5:2 F5:4 A5:4 | Bb5:2 A5:2 G5:4 Bb5:4 | A5:4 G#5:2 A5:2 C#6:4 | D6:8 R:4";
  lmel(&s,0,MEL,i_xylo,0.13f,0);
  lmel(&s,BAR*16,MEL,i_organ,0.085f,-12);
  lmel(&s,BAR*24,"Bb5:4 A5:4 G5:4 | G5:2 F5:2 E5:4 D5:4 | F5:4 E5:4 D5:4 | C#5:8 A4:4 | D5:2 E5:2 F5:4 A5:4 | Bb5:2 A5:2 G5:4 Bb5:4 | A5:4 G#5:2 A5:2 C#6:4 | D6:8 R:4",i_whistle,0.022f,12);
  song_reverb(&s,0.32f,0.83f,0.35f);
  return song_level(&s,0.13f);
}
/* ---- 2. La Foret Fongique : clairiere des spores (re lydien, harpe, flute, nappes) ---- */
static Buf build_lvl_foret(void){
  const int BAR=16, BARS=16; Song s=song_new(84,BAR*BARS);
  static const LCh D7={{50,57,61,66},38}, ED={{50,56,59,64},38}, Bm7={{54,57,62,66},35}, G7={{55,59,62,66},43}, Fm7={{54,57,61,64},42}, As={{57,62,64,69},45};
  const LCh *P[16]={&D7,&ED,&Bm7,&G7,&D7,&ED,&Fm7,&G7,&Bm7,&G7,&D7,&ED,&Bm7,&Fm7,&G7,&As};
  for(int b=0;b<BARS;b++){ const LCh *c=P[b]; float t=b*BAR;
    chord_play(&s,t,16,c,i_pad,0.032f,0);
    i_sub(&s,t,15,(float)c->bass,0.12f);
    static const int ARP[8]={0,1,2,3,2,3,1,2};
    for(int k=0;k<8;k++){ int i=ARP[k]; i_pluck(&s,t+k*2,2,(float)(c->n[i]+12),k==0?0.07f:0.05f); }
    if(b&1){ i_bell(&s,t+6,2,(float)(c->n[(b/2)%4]+24),0.025f); i_bell(&s,t+14,2,(float)(c->n[(b+1)%4]+36),0.016f); }
    else i_bell(&s,t+10,2,(float)(c->n[3]+24),0.022f);
    for(int k=0;k<16;k+=2) song_drum(&s,t+k,2,(k%4)?0.035f:0.05f);
    i_tom(&s,t,4,36,0.16f);
  }
  lmel(&s,0,"F#5:6 A5:2 B5:8 | A5:4 G#5:4 E5:8 | D5:6 E5:2 F#5:4 A5:4 | B5:12 R:4 | C#6:6 B5:2 A5:4 F#5:4 | G#5:8 E5:8 | F#5:4 E5:4 C#5:4 E5:4 | D5:8 E5:8 | "
            "F#5:6 B5:2 D6:8 | C#6:4 B5:4 G5:8 | A5:6 F#5:2 A5:4 C#6:4 | B5:8 G#5:8 | D6:6 C#6:2 B5:4 F#5:4 | A5:8 E5:8 | D5:4 E5:4 F#5:4 G5:4 | A5:16",i_flute,0.085f,0);
  song_reverb(&s,0.36f,0.85f,0.4f);
  return song_level(&s,0.125f);
}
/* ---- 3. Les Forges Englouties : enclumes (mi phrygien, basse martelee, cuivres, enclumes) ---- */
static Buf build_lvl_forges(void){
  const int BAR=16, BARS=24; Song s=song_new(138,BAR*BARS);
  static const LCh E={{52,59,64},40}, F={{53,60,65},41}, Am={{57,64,69},45}, G={{55,62,67},43}, C={{48,55,60},36}, D={{50,57,62},38};
  const LCh *P[24]={&E,&F,&E,&F,&Am,&G,&F,&E, &E,&F,&E,&F,&Am,&G,&F,&E, &C,&D,&E,&E,&Am,&G,&F,&E};
  for(int b=0;b<BARS;b++){ const LCh *c=P[b]; float t=b*BAR; bool brk=b>=16;
    for(int k=0;k<8;k++){ float m=(float)c->bass+((k==6)?12:0); i_bsaw(&s,t+k*2,1.6f,m,(k==0||k==3)?0.16f:0.12f); }
    chord_play(&s,t,3,c,i_brass,0.05f,0); if(!brk) chord_play(&s,t+10,2,c,i_brass,0.035f,0);
    if(!brk){ static const int AN[4]={2,6,10,14}; for(int k=0;k<4;k++) i_anvil(&s,t+AN[k],1,(k&1)?83:88,0.05f); }
    else { static const int AN[6]={0,3,6,8,11,14}; for(int k=0;k<6;k++) i_anvil(&s,t+AN[k],1,(k%3==2)?83:88,0.055f); }
    song_drum(&s,t,0,0.55f); song_drum(&s,t+6,0,0.42f); song_drum(&s,t+8,0,0.5f); if(b&1) song_drum(&s,t+11,0,0.38f);
    song_drum(&s,t+4,1,0.38f); song_drum(&s,t+12,1,0.38f);
    for(int k=0;k<16;k+=2) song_drum(&s,t+k,2,0.10f);
    if(b%4==3){ i_tom(&s,t+12,1,52,0.32f); i_tom(&s,t+13,1,49,0.32f); i_tom(&s,t+14,1,45,0.34f); i_tom(&s,t+15,1,40,0.36f); }
    if(b%8==0) i_crash(&s,t,0.28f);
  }
  i_swell(&s,BAR*15+4,12,40,0.08f); i_swell(&s,BAR*23+4,12,40,0.09f);
  const char *A="E5:2 E5:2 F5:2 G#5:4 F5:2 E5:4 | D5:2 E5:2 F5:4 A5:8 | G#5:2 G#5:2 A5:2 B5:4 A5:2 G#5:4 | A5:2 B5:2 C6:4 B5:8 | "
                "C6:4 B5:2 A5:2 E5:4 A5:4 | B5:4 A5:2 G5:2 D5:4 G5:4 | A5:2 G5:2 F5:2 E5:2 D5:4 C5:4 | B4:4 C5:2 D5:2 E5:8";
  lmel(&s,0,A,i_brass,0.10f,0);
  lmel(&s,BAR*8,A,i_brass,0.10f,0); lmel(&s,BAR*8,A,i_brass,0.06f,-12);
  lmel(&s,BAR*16,"G5:8 E5:8 | F#5:8 A5:8 | G#5:16 | B5:16 | C6:8 B5:4 A5:4 | B5:8 G5:8 | A5:4 G5:4 F5:4 D5:4 | E5:16",i_brass,0.11f,0);
  lmel(&s,BAR*16,"C4:16 | D4:16 | E4:32 | A3:16 | G3:16 | F3:16 | E3:16",i_choir,0.07f,0);
  song_reverb(&s,0.16f,0.72f,0.45f);
  return song_level(&s,0.14f);
}
/* ---- 4. Le Coeur du Gouffre : l'oeil qui bat (bourdon dissonant, coeur, boite a musique faussee) ---- */
static Buf build_lvl_coeur(void){
  const int BAR=16, BARS=16; Song s=song_new(70,BAR*BARS);
  for(int b=0;b<BARS;b+=2){ float t=b*BAR;
    lnote(&s,t,40,36,W_SAW,0.05f,1.6f,0.5f,0.05f,0.002f,0); lnote(&s,t,40,37,W_SAW,0.04f,1.8f,0.5f,0.05f,0.002f,0);
    lnote(&s,t,40,24,W_SIN,0.12f,1.2f,0.5f,1,0,0); }
  for(int b=0;b<BARS;b++){ float t=b*BAR;
    i_heart(&s,t,0.55f); if(b>=8) i_heart(&s,t+8,0.45f+0.02f*(b-8));
    if(b>=12) for(int k=0;k<16;k+=2) song_drum(&s,t+k,2,0.03f+0.004f*(b-12)*k);
  }
  lmel(&s,0,"G5:4 Eb5:4 C5:4 D5:4 | Eb5:4 D5:4 B4:8 | G5:4 Ab5:4 G5:4 F#5:4 | G5:16",i_mbox,0.06f,0);
  lmel(&s,BAR*8,"G5:4 Eb5:4 C5:4 D5:4 | Eb5:4 D5:4 B4:8 | Gb5:4 F5:4 E5:4 Eb5:4 | D5:16",i_mbox,0.055f,0);
  lglide(&s,BAR*4,6.0f,91,90,W_SIN,0.016f,2.0f,0.7f,1,0.012f,5.5f);
  lglide(&s,BAR*6,6.0f,90,86,W_SIN,0.016f,2.0f,0.7f,1,0.014f,5.0f);
  lglide(&s,BAR*12+6,2.6f,84,76,W_SAW,0.03f,0.3f,1.4f,0.3f,0.02f,9.0f);
  lmel(&s,BAR*8,"C4:32 | Db4:32 | C4:32 | B3:32",i_string,0.05f,0); lmel(&s,BAR*8,"G4:32 | Ab4:32 | G4:32 | F#4:32",i_string,0.04f,0); lmel(&s,BAR*8,"F#4:64 | F4:64",i_string,0.03f,0);
  i_swell(&s,BAR*4-12,12,30,0.06f); i_swell(&s,BAR*8-12,12,30,0.07f); i_swell(&s,BAR*12-12,12,30,0.075f); i_swell(&s,BAR*16-12,12,30,0.08f);
  i_boom(&s,BAR*8,0.5f); i_boom(&s,BAR*12,0.45f);
  i_bell(&s,BAR*2,8,49,0.05f); i_bell(&s,BAR*10,8,48,0.05f);
  song_reverb(&s,0.46f,0.88f,0.5f);
  return song_level(&s,0.12f);
}
/* ---- 5. Les Grottes de Givre : cristaux de glace (la majeur, celesta, verre frotte, vent) ---- */
static Buf build_lvl_givre(void){
  const int BAR=16, BARS=16; Song s=song_new(76,BAR*BARS);
  static const LCh A9={{57,61,64,68,71},45}, Fm9={{54,57,61,64,68},42}, D7={{50,54,57,61},38}, Es={{52,57,59,64},40}, E={{52,56,59,64},40}, Cm7={{49,52,56,59},37}, Fm7={{54,57,61,64},42}, D9={{50,54,57,61,64},38};
  const LCh *P[16]={&A9,&Fm9,&D7,&Es,&Cm7,&Fm7,&D9,&E,&A9,&Fm9,&D7,&Es,&Cm7,&Fm7,&D9,&E};
  for(int b=0;b<BARS;b++){ const LCh *c=P[b]; float t=b*BAR;
    chord_play(&s,t,16,c,i_pad,0.026f,0);
    if(b%4==3){ chord_play(&s,t+8,8,&E,i_pad,0.02f,0); }
    i_sub(&s,t,16,(float)c->bass,0.10f);
    static const int ARP[8]={0,2,1,3,2,4,3,1};
    for(int k=0;k<8;k++){ if(k==5 && (b&1)) continue; int i=ARP[k]; int m=c->n[i]?c->n[i]:c->n[0]; i_bell(&s,t+k*2,1,(float)(m+24),k==0?0.04f:0.028f); }
  }
  for(int b=0;b<BARS;b+=4) i_wind(&s,b*BAR,64,60,0.035f);
  for(int r=0;r<2;r++){ static const int CH[8]={100,97,95,92,90,88,85,83}; for(int k=0;k<8;k++) i_bell(&s,BAR*(3+r*8)+12+k*0.5f,1,(float)CH[k],0.025f); }
  lmel(&s,0,"E5:8 C#6:8 | B5:8 A5:4 G#5:4 | F#5:8 A5:4 C#6:4 | B5:16 | G#5:8 E5:8 | A5:8 C#6:4 E6:4 | D6:6 C#6:2 A5:8 | B5:8 G#5:8 | "
            "E6:8 C#6:8 | D6:4 C#6:4 B5:4 A5:4 | A5:8 F#5:8 | A5:8 G#5:8 | E5:8 G#5:4 B5:4 | C#6:8 A5:8 | F#5:4 A5:4 D6:4 C#6:4 | B5:12 R:4",i_glass,0.075f,0);
  song_reverb(&s,0.46f,0.88f,0.22f);
  return song_level(&s,0.12f);
}
/* ---- 6. La Cite Engloutie : cloches noyees (re dorien en 6/8, piano sous l'eau, bulles) ---- */
static Buf build_lvl_cite(void){
  const int BAR=12, BARS=24; Song s=song_new(100,BAR*BARS);
  static const LCh Dm9={{50,57,60,64,65},38}, GD={{50,55,59,62},38}, G6={{50,55,59,64},43}, F7={{53,57,60,64},41}, C9={{48,55,62,64},36}, Am7={{55,60,64,69},45},
                   Bb7={{50,53,57,58},46}, Cc={{52,55,60,64},36}, Dm={{50,53,57,62},38}, As4={{52,57,62,64},45}, Aa={{52,57,61,64},45};
  const LCh *P[24]={&Dm9,&GD,&Dm9,&G6,&F7,&C9,&Am7,&Am7, &Dm9,&GD,&Dm9,&G6,&F7,&C9,&Am7,&Am7, &Bb7,&Cc,&Am7,&Dm,&Bb7,&Cc,&As4,&Aa};
  for(int b=0;b<BARS;b++){ const LCh *c=P[b]; float t=b*BAR;
    static const int ARP[6]={0,1,2,3,2,1};
    for(int k=0;k<6;k++){ int i=ARP[k]; int m=c->n[i]?c->n[i]:c->n[0]; i_epiano(&s,t+k*2,2,(float)(m+12),k==0?0.05f:0.035f); }
    i_sub(&s,t,5,(float)c->bass,0.14f); i_sub(&s,t+6,5,(float)c->bass,0.09f);
    i_tom(&s,t,2,38,0.22f); i_tom(&s,t+6,2,40,0.13f); if(b&1) i_tom(&s,t+9,1,43,0.08f);
    static const float BU[4][3]={{3,72,0.03f},{8,79,0.025f},{5,76,0.03f},{10,84,0.02f}};
    for(int k=0;k<2;k++){ const float *q=BU[(b*2+k)%4]; i_drop(&s,t+q[0],1,q[1],q[2]); }
    if(b%2==0){ i_bell(&s,t,12,(float)(c->bass+24),0.05f); i_bell(&s,t+0.15f,12,(float)(c->bass+24)+0.15f,0.035f); }
  }
  const char *A="A4:6 D5:6 | E5:4 D5:2 B4:6 | C5:6 A4:6 | B4:12 | A4:6 C5:6 | E5:4 D5:2 G5:6 | E5:6 C5:6 | A4:12 | "
                "D5:6 F5:6 | G5:4 F5:2 E5:6 | F5:6 E5:4 D5:2 | D5:12 | C5:6 E5:6 | G5:6 E5:4 D5:2 | C5:6 E5:6 | A4:12 | "
                "D5:6 F5:6 | E5:4 G5:2 C6:6 | A5:6 G5:4 E5:2 | F5:12 | D6:6 C6:4 A5:2 | G5:6 E5:6 | D5:12 | C#5:12";
  lmel(&s,0,A,i_flute,0.07f,0);
  lmel(&s,BAR*16,"D4:12 | E4:12 | C4:12 | A3:12 | D4:12 | E4:12 | D4:12 | C#4:12",i_choir,0.05f,0);
  song_reverb(&s,0.42f,0.86f,0.55f);
  song_muffle(&s,0.42f);
  return song_level(&s,0.125f);
}
/* ---- 7. La Ruche d'Ambre : le bal des abeilles (sol majeur, bourdonnements chromatiques, pizzicati) ---- */
static Buf build_lvl_ruche(void){
  const int BAR=16, BARS=24; Song s=song_new(150,BAR*BARS);
  static const LCh G={{55,59,62},43}, C={{55,60,64},36}, D={{54,57,62},38}, Em={{55,59,64},40}, Bm={{54,59,62},35}, D7={{54,57,60},38};
  const LCh *P[24]={&G,&G,&C,&C,&D,&C,&G,&D, &G,&G,&C,&C,&Em,&C,&D,&D, &C,&D,&Bm,&Em,&C,&D,&G,&D7};
  for(int b=0;b<BARS;b++){ const LCh *c=P[b]; float t=b*BAR;
    static const int BL[8]={0,12,7,12,0,12,7,5};
    for(int k=0;k<8;k++) i_bsq(&s,t+k*2,1.4f,(float)(c->bass+BL[k]),(k&1)?0.07f:0.10f);
    static const int AR[4]={0,1,2,1};
    for(int k=0;k<16;k++) i_pluck(&s,t+k,1,(float)(c->n[AR[k&3]]+12),(k%4==0)?0.035f:0.022f);
    chord_play(&s,t,16,c,i_pad,0.02f,0);
    song_drum(&s,t,0,0.48f); song_drum(&s,t+8,0,0.45f); if(b%2) song_drum(&s,t+6,0,0.3f);
    song_drum(&s,t+4,1,0.34f); song_drum(&s,t+12,1,0.34f); song_drum(&s,t+4,2,0.12f); song_drum(&s,t+12,2,0.12f);
    for(int k=0;k<16;k++) song_drum(&s,t+k,2,(k&1)?0.035f:0.06f);
    if(b%2) song_drum(&s,t+14,3,0.08f);
  }
  const char *A="G5:1 F#5:1 F5:1 F#5:1 G5:1 F#5:1 F5:1 E5:1 Eb5:1 E5:1 F5:1 F#5:1 G5:4 | B5:2 A5:2 G5:2 D5:2 G5:4 B5:4 | "
                "C6:1 B5:1 Bb5:1 B5:1 C6:1 B5:1 Bb5:1 A5:1 Ab5:1 A5:1 Bb5:1 B5:1 C6:4 | E6:2 D6:2 C6:2 G5:2 C6:4 E6:4 | "
                "D6:2 F#6:2 A6:2 F#6:2 D6:2 A5:2 F#5:2 A5:2 | G5:2 E6:2 C6:2 G5:2 E5:2 G5:2 C6:4 | B5:2 G5:2 D5:2 G5:2 B5:2 D6:2 G6:4 | F#6:4 E6:2 D6:2 C6:2 A5:2 F#5:4";
  lmel(&s,0,A,i_buzz,0.085f,0);
  lmel(&s,BAR*8,A,i_buzz,0.08f,0); lmel(&s,BAR*8,A,i_buzz,0.03f,12);
  lmel(&s,BAR*16,"E5:4 G5:4 C6:8 | D6:4 C6:2 B5:2 A5:8 | B5:4 D6:4 F#6:8 | E6:4 D6:2 B5:2 G5:8 | A5:4 C6:4 E6:4 C6:4 | D6:8 A5:8 | G5:4 B5:4 D6:4 G6:4 | "
                 "F#6:2 E6:2 D6:2 C6:2 B5:2 A5:2 F#5:2 D5:2",i_lead,0.07f,0);
  song_reverb(&s,0.12f,0.7f,0.4f);
  return song_level(&s,0.14f);
}
/* ---- 8. Le Neant : le vide (do mineur, choeurs, timbales, cor lointain) ---- */
static Buf build_lvl_neant(void){
  const int BAR=16, BARS=16; Song s=song_new(66,BAR*BARS);
  static const LCh Cm={{48,55,60,63},36}, Ab={{48,56,60,63},32}, Fm={{48,53,56,60},29}, G={{50,55,59,62},31}, Db={{49,53,56,61},37}, Eb={{51,55,58,63},39}, Gs={{50,55,60,62},31};
  const LCh *P[16]={&Cm,&Ab,&Fm,&G,&Cm,&Db,&Ab,&Gs,&Fm,&Cm,&Ab,&Eb,&Fm,&Db,&G,&G};
  for(int b=0;b<BARS;b++){ const LCh *c=P[b]; float t=b*BAR;
    chord_play(&s,t,16,c,i_choir,0.045f,0);
    { int sb=c->bass+12; if(sb>45) sb-=12; i_sub(&s,t,16,(float)sb,0.13f); }
    i_boom(&s,t,b%4==0?0.75f:0.45f); if(b%4==3){ i_boom(&s,t+12,0.4f); i_boom(&s,t+14,0.5f); }
    if(b>=8){ static const int TR[4]={0,1,2,3}; for(int k=0;k<16;k++){ int i=TR[(k/2)&3]; lnote(&s,t+k,0.9f,(float)(c->n[i]+12),W_SAW,0.012f+0.0015f*(b-8),0.005f,1.5f,0.25f,0,0); } }
    if(b%4==3) i_swell(&s,t+4,12,24,0.06f);
  }
  i_bell(&s,0,16,60,0.08f); i_bell(&s,0,16,67,0.05f); i_bell(&s,BAR*8,16,60,0.08f); i_bell(&s,BAR*8,16,55,0.05f);
  i_crash(&s,0,0.25f); i_crash(&s,BAR*8,0.3f);
  lmel(&s,0,"G4:8 C5:8 | Eb5:12 D5:2 C5:2 | Ab4:8 C5:4 F5:4 | D5:16 | G5:8 Eb5:4 C5:4 | F5:8 Ab5:8 | Eb5:4 C5:4 Ab4:8 | B4:16 | "
            "C6:8 Ab5:8 | G5:8 Eb5:8 | C6:4 Bb5:4 Ab5:8 | G5:8 Bb5:8 | Ab5:4 G5:4 F5:4 C5:4 | Db5:8 F5:8 | D5:4 F5:4 G5:8 | B4:12 R:4",i_horn,0.10f,0);
  song_reverb(&s,0.5f,0.9f,0.45f);
  return song_level(&s,0.125f);
}
/* ---- 9. Le Jardin des Lucioles : berceuse au clair de lune (fa majeur a trois temps, boite a musique, harpe, flute, lucioles) ---- */
static Buf build_lvl_jardin(void){
  const int BAR=12, BARS=32; Song s=song_new(78,BAR*BARS);
  static const LCh F={{53,57,60,65},41}, F7={{52,57,60,65},41}, Bb={{50,53,58,62},46}, Dm={{50,53,57,62},38}, Gm7={{53,55,58,62},43}, C7={{52,55,58,64},48}, C={{52,55,60,64},48}, Am={{52,57,60,64},45};
  const LCh *P[16]={&F,&F7,&Bb,&F,&Dm,&Gm7,&C7,&F,&Bb,&C,&Am,&Dm,&Gm7,&C7,&F,&C7};
  for(int b=0;b<BARS;b++){ const LCh *c=P[b%16]; float t=b*BAR; bool two=b>=16;
    chord_play(&s,t,12,c,i_pad,two?0.026f:0.02f,0);
    i_sub(&s,t,11,(float)c->bass,0.10f);
    static const int ARP[6]={0,1,2,3,2,1};
    for(int k=0;k<6;k++) i_pluck(&s,t+k*2,2,(float)(c->n[ARP[k]]+12),k==0?0.06f:0.042f);
    static const int GL[4][2]={{5,2},{9,3},{3,1},{7,0}};   /* lucioles : tintements aigus qui s'allument ca et la */
    i_bell(&s,t+GL[b&3][0],1,(float)(c->n[GL[b&3][1]]+24),0.018f);
    if(b%4==1) i_bell(&s,t+10,1,(float)(c->n[3]+24),0.012f);
    if(two){ song_drum(&s,t+4,2,0.022f); song_drum(&s,t+8,2,0.018f); }
  }
  const char *A="C5:4 F5:4 A5:4 | G5:6 F5:2 E5:4 | F5:4 D5:4 Bb4:4 | C5:12 | D5:4 F5:4 A5:4 | Bb5:6 A5:2 G5:4 | E5:4 G5:4 Bb5:4 | A5:12 | "
                "D6:6 C6:2 Bb5:4 | C6:6 Bb5:2 A5:4 | A5:4 G5:4 E5:4 | F5:8 D5:4 | G5:4 Bb5:4 D6:4 | C6:4 Bb5:4 E5:4 | F5:12 | E5:4 G5:4 Bb5:4";
  lmel(&s,0,A,i_mbox,0.075f,0);
  lmel(&s,BAR*16,A,i_flute,0.08f,0); lmel(&s,BAR*16,A,i_mbox,0.028f,12);
  lmel(&s,BAR*16,"A4:24 Bb4:12 A4:24 Bb4:24 A4:12 D5:12 E5:24 D5:24 C5:24 Bb4:12",i_string,0.03f,0);
  song_reverb(&s,0.42f,0.87f,0.4f);
  return song_level(&s,0.12f);
}
/* ---- 10. Le Marais maudit : brumes et feux follets (mi phrygien, bourdon, plaintes spectrales, crapauds, glouglous) ---- */
static Buf build_lvl_marais(void){
  const int BAR=16, BARS=16; Song s=song_new(62,BAR*BARS);
  for(int b=0;b<BARS;b+=2){ float t=b*BAR;   /* bourdon : mi grave qui bat contre lui-meme */
    lnote(&s,t,32,28,W_SAW,0.04f,1.4f,0.5f,0.05f,0.002f,0); lnote(&s,t,32,28.25f,W_SAW,0.035f,1.6f,0.5f,0.05f,0.002f,0);
    lnote(&s,t,32,40,W_TRI,0.03f,1.8f,0.5f,0.3f,0.003f,0); }
  static const int CL[4][2]={{52,53},{52,55},{50,53},{51,52}};   /* grappes de cordes : secondes mineures */
  for(int b=0;b<BARS;b++){ float t=b*BAR; const int *q=CL[(b/2)&3];
    if(b%2==0){ i_string(&s,t,32,(float)q[0],0.03f); i_string(&s,t,32,(float)q[1],0.026f); }
    i_tom(&s,t,4,31,0.20f); i_tom(&s,t+10,4,31,0.12f);                 /* pas lents dans la vase */
    i_drop(&s,t+(float)((b*5)%12)+2,1,(float)(43+((b*7)%5)),0.05f);     /* glouglous */
    if(b%4==1) i_croak(&s,t+6,1,40,0.085f);
    if(b%4==3){ i_croak(&s,t+2,1,43,0.075f); i_croak(&s,t+9,1,38,0.07f); }
    if(b>=8) for(int k=0;k<16;k+=4) song_drum(&s,t+k+2,2,0.02f+0.002f*(b-8));
  }
  for(int b=0;b<BARS;b+=4) i_wind(&s,b*BAR+4,48,50,0.025f);
  lglide(&s,BAR*2,4.0f,84,79,W_SIN,0.018f,1.2f,0.8f,1,0.015f,4.5f);      /* plaintes des feux follets */
  lglide(&s,BAR*6+8,4.0f,83,77,W_SIN,0.018f,1.2f,0.8f,1,0.015f,4.0f);
  lglide(&s,BAR*10,4.0f,86,80,W_SIN,0.016f,1.2f,0.8f,1,0.018f,5.0f);
  lglide(&s,BAR*14+4,4.0f,82,76,W_SIN,0.018f,1.2f,0.8f,1,0.015f,4.5f);
  const char *A="E5:8 F5:8 | E5:4 D5:4 C5:8 | B4:8 Bb4:8 | B4:16 | E5:8 G5:8 | F5:4 E5:4 D5:8 | C5:4 B4:4 Bb4:4 A4:4 | B4:16";
  lmel(&s,0,A,i_mbox,0.065f,0);
  lmel(&s,BAR*8,A,i_whistle,0.04f,0); lmel(&s,BAR*8,A,i_mbox,0.05f,-12);
  i_bell(&s,0,16,40,0.06f); i_bell(&s,BAR*8,16,41,0.06f);
  song_reverb(&s,0.5f,0.9f,0.55f);
  return song_level(&s,0.115f);
}
/* ---- 11. Les Mines de cristal : galerie scintillante (la mineur mysterieux, marimba, pioches, wagonnets, cordes et verre) ---- */
static Buf build_lvl_mines(void){
  const int BAR=16, BARS=24; Song s=song_new(100,BAR*BARS);
  static const LCh Am9={{57,60,64,71},45}, Fl={{53,57,59,64},41}, Dm9={{53,57,60,64},38}, E7={{52,56,59,65},40}, C7M={{55,59,60,64},36}, Bm5={{50,53,57,59},47}, G6={{55,59,62,64},43};
  const LCh *P[24]={&Am9,&Fl,&Dm9,&E7,&Am9,&C7M,&Bm5,&E7, &Am9,&Fl,&Dm9,&E7,&Am9,&C7M,&Bm5,&E7, &Fl,&G6,&Am9,&Am9,&Dm9,&G6,&E7,&E7};
  for(int b=0;b<BARS;b++){ const LCh *c=P[b]; float t=b*BAR;
    static const int BP[4]={0,6,8,14};
    for(int k=0;k<4;k++) i_pizz(&s,t+BP[k],2,(float)(c->bass+(k==2?12:0)),k==0?0.26f:0.18f);
    static const int ARP[8]={0,1,2,3,2,1,3,2};
    for(int k=0;k<8;k++) i_xylo(&s,t+k*2,2,(float)(c->n[ARP[k]]+12),k==0?0.07f:0.05f);
    i_anvil(&s,t+4,1,96,0.018f); i_anvil(&s,t+12,1,(b&1)?93:98,0.016f);      /* pioches au loin */
    song_drum(&s,t,0,0.3f); song_drum(&s,t+8,0,0.24f); if(b%2) song_drum(&s,t+11,0,0.16f);
    for(int k=0;k<16;k++) if(k%4!=1) song_drum(&s,t+k,2,(k%4==0)?0.05f:0.03f);   /* roulement du wagonnet */
    if(b%2==0) i_bell(&s,t+(b%4?6:10),1,(float)(c->n[3]+24),0.02f);
    i_drop(&s,t+(float)((b*7)%14)+1,1,(float)(76+((b*5)%7)),0.025f);
    if(b%8==7) i_swell(&s,t+4,12,40,0.04f);
  }
  const char *A="E5:12 D5:2 C5:2 | B4:8 A4:4 B4:4 | C5:8 D5:4 F5:4 | E5:12 R:4 | A5:8 G5:4 E5:4 | G5:8 F5:4 E5:4 | D5:8 F5:4 A5:4 | G#5:16";
  lmel(&s,0,A,i_string,0.06f,0);
  lmel(&s,BAR*8,"C6:8 B5:8 | A5:8 B5:8 | A5:4 F5:4 A5:4 C6:4 | B5:16 | C6:8 E6:8 | D6:4 C6:4 B5:8 | A5:8 F5:8 | E5:16",i_glass,0.07f,0);
  lmel(&s,BAR*8,A,i_string,0.035f,-12);
  const char *B="A5:8 B5:8 | D6:8 B5:4 G5:4 | C6:16 | E5:8 A5:8 | F5:8 E5:4 D5:4 | B4:8 D5:8 | G#5:8 B5:8 | E6:12 R:4";
  lmel(&s,BAR*16,B,i_string,0.06f,0); lmel(&s,BAR*16,B,i_glass,0.04f,12);
  song_reverb(&s,0.4f,0.88f,0.3f);
  return song_level(&s,0.125f);
}
/* ---- 12. Le Sanctuaire solaire : l'hymne du soleil (re mixolydien, fanfare de cuivres, choeur, taikos, cordes au galop) ---- */
static Buf build_lvl_sanctuaire(void){
  const int BAR=16, BARS=24; Song s=song_new(116,BAR*BARS);
  static const LCh D={{50,54,57,62},38}, C={{48,52,55,60},36}, G={{50,55,59,62},43}, Bm={{50,54,59,62},35}, Aa={{49,52,57,61},45}, Bb={{50,53,58,62},46};
  const LCh *P[24]={&D,&C,&G,&D,&Bm,&G,&Aa,&Aa, &D,&C,&G,&D,&Bm,&G,&Aa,&D, &Bb,&C,&D,&D,&Bb,&C,&Aa,&Aa};
  for(int b=0;b<BARS;b++){ const LCh *c=P[b]; float t=b*BAR; bool big=b>=16;
    for(int k=0;k<16;k++){ if(k%4==1) continue; int m=c->bass+12+(((k/4)&1)?7:0); if(m>57) m-=12;   /* cordes au galop */
      lnote(&s,t+k,0.9f,(float)m,W_SAW,(k%4==0)?0.05f:0.032f,0.004f,1.2f,0.22f,0,0); }
    chord_play(&s,t,5,c,i_brass,big?0.05f:0.04f,0);
    chord_play(&s,t,16,c,i_choir,big?0.04f:0.03f,0);
    i_sub(&s,t,16,(float)c->bass,0.10f);
    static const int TK[6]={0,3,6,8,10,11};                                      /* taikos */
    for(int k=0;k<6;k++) i_tom(&s,t+TK[k],1,(float)((k==0||k==3)?36:41),(k==0||k==3)?0.32f:0.2f);
    song_drum(&s,t+4,1,0.22f); song_drum(&s,t+12,1,0.24f);
    if(b%4==0) i_boom(&s,t,0.55f);
    if(b%4==3){ i_tom(&s,t+12,1,45,0.3f); i_tom(&s,t+13,1,43,0.3f); i_tom(&s,t+14,1,40,0.32f); i_tom(&s,t+15,1,36,0.34f); }
    if(b%8==0) i_crash(&s,t,0.26f);
  }
  i_bell(&s,0,16,38,0.08f); i_bell(&s,BAR*8,16,38,0.08f); i_bell(&s,BAR*16,16,34,0.09f);   /* gong du temple */
  i_swell(&s,BAR*15+4,12,40,0.08f); i_swell(&s,BAR*23+4,12,40,0.09f);
  const char *A="D5:2 D5:2 A5:4 D6:8 | C6:4 B5:2 A5:2 G5:8 | B5:4 A5:2 G5:2 D5:4 G5:4 | A5:12 F#5:4 | B5:2 B5:2 D6:4 F#6:8 | E6:4 D6:2 B5:2 G5:8 | A5:4 B5:2 C#6:2 E6:4 C#6:4 | A5:16";
  lmel(&s,0,A,i_brass,0.09f,0);
  lmel(&s,BAR*8,A,i_brass,0.09f,0); lmel(&s,BAR*8,A,i_brass,0.05f,-12);
  const char *B="D6:8 F6:8 | E6:8 G6:8 | F#6:16 | A6:8 F#6:4 D6:4 | D6:8 Bb5:8 | C6:8 E6:8 | C#6:8 E6:4 A5:4 | A5:16";
  lmel(&s,BAR*16,B,i_brass,0.08f,-12); lmel(&s,BAR*16,B,i_choir,0.05f,-12);
  song_reverb(&s,0.26f,0.82f,0.4f);
  return song_level(&s,0.14f);
}
static Buf build_level_theme(int b){
  switch(b){
    case 0: return build_lvl_catacombes();
    case 1: return build_lvl_foret();
    case 2: return build_lvl_forges();
    case 3: return build_lvl_coeur();
    case 4: return build_lvl_givre();
    case 5: return build_lvl_cite();
    case 6: return build_lvl_ruche();
    case 8: return build_lvl_jardin();
    case 9: return build_lvl_marais();
    case 10: return build_lvl_mines();
    case 11: return build_lvl_sanctuaire();
    default: return build_lvl_neant();
  }
}
/* construction : en tache de fond au demarrage (le niveau choisi d'abord), sinon a la demande */
static atomic_int lvl_state[BIOME_COUNT];   /* 0 a faire, 1 en cours, 2 pret */
static int lvl_first=0;
static void lvl_build(int b){
  int z=0; if(!atomic_compare_exchange_strong(&lvl_state[b],&z,1)) return;
  Buf m=build_level_theme(b);
  MB[MUS_LVL0+b].n=m.n; __atomic_store_n(&MB[MUS_LVL0+b].d,m.d,__ATOMIC_RELEASE);
  atomic_store(&lvl_state[b],2);
}
static void lvl_worker(void *arg){ (void)arg; for(int k=0;k<BIOME_COUNT;k++) lvl_build((lvl_first+k)%BIOME_COUNT); }
static bool lvl_threaded=false;
void music_levels_start(int first){
  lvl_first=clampi(first,0,BIOME_COUNT-1);
  if(plat_thread_start(lvl_worker,NULL)) lvl_threaded=true;
}
int music_level(int biome){
  int b=clampi(biome,0,BIOME_COUNT-1);
  if(atomic_load(&lvl_state[b])==0) lvl_build(b);   /* pas encore commence : on le calcule tout de suite */
  return MUS_LVL0+b;
}
bool music_level_ready(int biome){ return atomic_load(&lvl_state[clampi(biome,0,BIOME_COUNT-1)])==2; }

/* ---------------- ambiances (v9) : nappes stereo precalculees, lues dans « donnees/ambiances » ----------------
   Deux emplacements : l'un joue, l'autre se charge en tache de fond (ou s'eteint) ; le mixeur passe de l'un a
   l'autre en fondu. Le jeu ne touche a un emplacement que lorsque le mixeur l'a rendu muet. */
typedef struct { int16_t *d; int n; char name[40]; } Amb;   /* n : trames stereo */
static Amb amb_slot[2];
static atomic_int amb_target = -1;                 /* jeu -> mixeur : emplacement a faire entendre */
static atomic_int amb_quiet[2] = {1,1};            /* mixeur -> jeu : emplacement muet (remplacable) */
static atomic_int amb_load = 0;                    /* 0 rien, 1 en cours, 2 pret, 3 echec */
static int amb_load_slot = -1;
static float amb_g[2]; static int amb_pos[2];      /* mixeur seulement */
static char amb_want[40], amb_failed[40];
static bool wav_read(const char *path,Amb *a){
  FILE *f=fopen(path,"rb"); if(!f) return false;
  char id[4]; uint32_t sz=0; bool ok= fread(id,1,4,f)==4 && !memcmp(id,"RIFF",4) && fread(&sz,4,1,f)==1 && fread(id,1,4,f)==4 && !memcmp(id,"WAVE",4);
  bool fmt_ok=false;
  while(ok){
    if(fread(id,1,4,f)!=4 || fread(&sz,4,1,f)!=1){ ok=false; break; }
    if(!memcmp(id,"fmt ",4)){ uint16_t fmt=0,ch=0,bits=0; uint32_t rate=0,br=0; uint16_t al=0;
      ok= sz>=16 && fread(&fmt,2,1,f)==1 && fread(&ch,2,1,f)==1 && fread(&rate,4,1,f)==1 && fread(&br,4,1,f)==1 && fread(&al,2,1,f)==1 && fread(&bits,2,1,f)==1;
      fmt_ok= ok && fmt==1 && ch==2 && rate==AUDIO_RATE && bits==16; if(ok && sz>16) fseek(f,(long)(sz-16),SEEK_CUR); }
    else if(!memcmp(id,"data",4)){
      if(!fmt_ok || sz<4){ ok=false; break; }
      int n=(int)(sz/4); a->d=(int16_t*)malloc((size_t)n*4); ok= a->d && fread(a->d,4,(size_t)n,f)==(size_t)n;
      if(ok) a->n=n; else { free(a->d); a->d=NULL; }
      break; }
    else fseek(f,(long)(sz+(sz&1)),SEEK_CUR);
  }
  fclose(f); return ok && a->d;
}
static void amb_worker(void *arg){ (void)arg; int s=amb_load_slot; char p[700];
  snprintf(p,sizeof p,"%s/ambiances/%s.wav",hd_data_dir(),amb_slot[s].name);
  bool ok=wav_read(p,&amb_slot[s]); atomic_store(&amb_load,ok?2:3); }
void amb_play(const char *name){ snprintf(amb_want,sizeof amb_want,"%s",name?name:""); }
static void amb_update(void){
  int at=atomic_load(&amb_target), ld=atomic_load(&amb_load);
  if(ld==1) return;                                    /* chargement en cours : on attend */
  if(ld>=2){ int s=amb_load_slot; atomic_store(&amb_load,0); amb_load_slot=-1;
    if(ld==3){ snprintf(amb_failed,sizeof amb_failed,"%s",amb_slot[s].name); amb_slot[s].name[0]=0; }
    else if(!strcmp(amb_slot[s].name,amb_want)){ atomic_store(&amb_target,s); return; } }
  if(!amb_want[0] || !strcmp(amb_want,amb_failed)){ if(at!=-1) atomic_store(&amb_target,-1); return; }
  if(at>=0 && !strcmp(amb_slot[at].name,amb_want)) return;
  for(int s=0;s<2;s++) if(amb_slot[s].d && !strcmp(amb_slot[s].name,amb_want)){ atomic_store(&amb_target,s); return; }   /* elle s'eteignait : on la rallume */
  int s= at==0 ? 1 : 0; if(at<0 && !atomic_load(&amb_quiet[0])) s=1;
  if(!atomic_load(&amb_quiet[s])) return;              /* on attend la fin du fondu */
  free(amb_slot[s].d); amb_slot[s].d=NULL; amb_slot[s].n=0; amb_pos[s]=0; snprintf(amb_slot[s].name,sizeof amb_slot[s].name,"%s",amb_want);
  amb_load_slot=s; atomic_store(&amb_load,1);
  if(!plat_thread_start(amb_worker,NULL)) amb_worker(NULL);
}
bool amb_playing(char *name,int n){ int at=atomic_load(&amb_target); if(at<0||!amb_slot[at].d){ if(name&&n) name[0]=0; return false; } if(name) snprintf(name,(size_t)n,"%s",amb_slot[at].name); return true; }

/* ---------------- mixeur (thread audio) ---------------- */
typedef struct { bool on; int id; double pos, step; float vl, vr; } Voice;
#define NVOICE 40
static Voice V[NVOICE];
static int mus_cur=-1, mus_req=-1; static double mus_pos=0, mus_rate=1, mus_req_rate=1; static float mus_gain=0; static bool mus_switch=false, mus_ready=false;

static void mixer_cmd(Cmd *c){
  if(c->type==CMD_PLAY){
    int id=c->id; if(id<0||id>=SFX_COUNT||!SB[id].d) return;
    int same=0, oldest=-1; double oldest_pos=-1; int freev=-1, most=-1; double most_t=-1;
    for(int i=0;i<NVOICE;i++){
      if(!V[i].on){ if(freev<0) freev=i; continue; }
      double t=V[i].pos/SB[V[i].id].n;
      if(V[i].id==id){ same++; if(t>oldest_pos){ oldest_pos=t; oldest=i; } }
      if(t>most_t){ most_t=t; most=i; }
    }
    int v = (same>=5 && oldest>=0) ? oldest : (freev>=0 ? freev : most);
    if(v<0) return;
    float pan=clampf(c->pan,-1,1); float a=(pan+1)*PI*0.25f;
    V[v].on=true; V[v].id=id; V[v].pos=0; V[v].step=c->pitch>0.05f?c->pitch:1.0;
    V[v].vl=c->vol*cosf(a)*1.4142f; V[v].vr=c->vol*sinf(a)*1.4142f;
  } else if(c->type==CMD_MUSIC){
    if(c->id==mus_cur && !mus_switch){ mus_rate=c->pitch; return; }
    mus_req=c->id; mus_req_rate=c->pitch; mus_switch=true;
  } else if(c->type==CMD_MUSIC_STOP){
    mus_req=-1; mus_switch=true;
  }
}

void game_audio_mix(int16_t *out,int frames){
  int t=atomic_load(&q_tail);
  while(t!=atomic_load(&q_head)){ mixer_cmd(&cmdq[t]); t=(t+1)&(CMDQ-1); atomic_store(&q_tail,t); }
  float vs=g_vol_sfx, vm=g_vol_music, va=g_vol_amb*0.8f; int at=atomic_load(&amb_target); const float astep=1.0f/(AUDIO_RATE*2.5f);
  for(int i=0;i<frames;i++){
    float l=0,r=0;
    for(int k=0;k<NVOICE;k++){
      Voice *v=&V[k]; if(!v->on) continue;
      Buf *b=&SB[v->id]; int idx=(int)v->pos;
      if(idx>=b->n-1){ v->on=false; continue; }
      float fr=(float)(v->pos-idx); float s=(b->d[idx]+(b->d[idx+1]-b->d[idx])*fr)*(1.0f/32768.0f);
      l+=s*v->vl; r+=s*v->vr; v->pos+=v->step;
    }
    l*=vs; r*=vs;
    /* musique avec fondu */
    if(mus_switch){
      mus_gain-=1.0f/(AUDIO_RATE*0.6f);
      if(mus_gain<=0 || mus_cur<0){ mus_gain=0; mus_cur=mus_req; mus_rate=mus_req_rate; mus_pos=0; mus_switch=false; }
    } else if(mus_cur>=0 && mus_gain<1 && mus_ready){ mus_gain+=1.0f/(AUDIO_RATE*0.8f); if(mus_gain>1) mus_gain=1; }
    const int16_t *md= mus_cur>=0 ? __atomic_load_n(&MB[mus_cur].d,__ATOMIC_ACQUIRE) : NULL;
    mus_ready= md!=NULL;
    if(md){
      int bn=MB[mus_cur].n; int idx=(int)mus_pos; if(idx>=bn){ mus_pos-=bn; idx=(int)mus_pos; if(idx>=bn) idx=0; }
      int nx=idx+1; if(nx>=bn) nx=0;
      float fr=(float)(mus_pos-idx); float s=(md[idx]+(md[nx]-md[idx])*fr)*(1.0f/32768.0f)*mus_gain*vm*0.75f;
      l+=s; r+=s; mus_pos+=mus_rate;
    }
    /* ambiance (v9) : deux nappes en fondu croise */
    for(int s=0;s<2;s++){ float tg= s==at ? 1.0f : 0.0f;
      if(amb_g[s]<tg){ amb_g[s]+=astep; if(amb_g[s]>tg) amb_g[s]=tg; } else if(amb_g[s]>tg){ amb_g[s]-=astep; if(amb_g[s]<tg) amb_g[s]=tg; }
      if(amb_g[s]>0 && amb_slot[s].d && amb_slot[s].n>0){ int p=amb_pos[s]; if(p>=amb_slot[s].n) p=0;
        float g=amb_g[s]*amb_g[s]*va*(1.0f/32768.0f); l+=amb_slot[s].d[p*2]*g; r+=amb_slot[s].d[p*2+1]*g; amb_pos[s]=p+1; } }
    /* saturation douce */
    l = l*(27+l*l)/(27+9*l*l); r = r*(27+r*r)/(27+9*r*r);
    l=clampf(l,-1,1); r=clampf(r,-1,1);
    out[i*2]=(int16_t)(l*32000); out[i*2+1]=(int16_t)(r*32000);
  }
  for(int s=0;s<2;s++) atomic_store(&amb_quiet[s], (amb_g[s]<=0 && s!=at) ? 1 : 0);
}

/* ---------------- cote jeu ---------------- */
static int frame_cnt[SFX_COUNT];
void audio_init(void){
  sin_init();
  build_sfx();
  MB[MUS_TITLE]=build_title();
  MB[MUS_BATTLE]=build_battle();
  MB[MUS_BOSS]=build_boss();
  MB[MUS_CAMP]=build_camp();
}
void audio_frame(void){ memset(frame_cnt,0,sizeof frame_cnt); amb_update(); }
void sfx_play(int id,float vol,float pan,float pitch){
  if(id<0||id>=SFX_COUNT) return;
  int lim = (id==SFX_HIT||id==SFX_BOUNCE||id==SFX_GEM||id==SFX_SHOOT)?2:3;
  if(frame_cnt[id]>=lim) return; frame_cnt[id]++;
  Cmd c={CMD_PLAY,id,vol,pan,pitch}; push_cmd(c);
}
void sfx(int id){ sfx_play(id,1.0f,0,1.0f); }
void sfx_at(int id,float x,float vol){ float pan=(x-SCREEN_W*0.5f)/(SCREEN_W*0.5f)*0.7f; sfx_play(id,vol,pan,frange(0.94f,1.06f)); }
static int cur_music=-2; static float cur_rate=0;
void music_play(int id,float rate){
  if(id==cur_music && fabsf(rate-cur_rate)<0.001f) return;
  cur_music=id; cur_rate=rate;
  Cmd c={CMD_MUSIC,id,1,0,rate}; push_cmd(c);
}
int music_debug_get(int id,const int16_t **d){ if(id<0||id>=MUS_COUNT){ *d=NULL; return 0; } *d=MB[id].d; return MB[id].n; }
void music_stop(void){ cur_music=-1; Cmd c={CMD_MUSIC_STOP,0,0,0,1}; push_cmd(c); }
