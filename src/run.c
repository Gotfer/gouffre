/* GOUFFRE - logique d'une descente (run) */
#include "game.h"

Run R;
RunResult LAST;

void meta_run_begin(void);
int  meta_run_achs(int *out,int max);

static void apply_card(Card *c);
bool run_has_mod(int m){ for(int i=0;i<R.nmods;i++) if(R.mods[i]==m) return true; return false; }
/* niveaux joues un par un : base propre a chaque niveau, puis montee en puissance sur toute sa longueur */
/* calibres par simulation : chaque biome a sa propre durete (le Coeur du Gouffre et les Profondeurs mordent plus fort) */
static float LVL_BASE[BIOME_COUNT]={0.9f,1.08f,1.22f,1.3f,2.85f,2.25f,2.8f,2.75f, 2.45f,2.6f,2.75f,2.9f};
static float RD_LVL=0.25f;   /* part du niveau dans les degats de contact / breches */
#define LVL_LONG 2.6f                         /* (avant v7.1) un niveau durait 2,6 fois l'ancien niveau */
#define LVL_SPAWN 790.0f                      /* v7.1 : chaque niveau dure ~15 min (790 s de monstres + gardiens) */
#define LVL_XP_POW 0.6f
#define LVL_DMG_POW 0.8f                       /* l'XP est reduite en partie pour garder la meme courbe de puissance */
static float lvl_prog(void){ return clampf(R.ltime/maxf(1,R.lduration),0,1); }
float run_diff(void){ if(R.single){ return RD_LVL*R.lvl+2.0f*lvl_prog(); } return (float)(R.level + 3*R.cycle + R.dest); }
static float depth_hp(void){ return 1.0f+0.35f*(R.depth-1); }
static float depth_gold(void){ return 1.0f + 0.15f*(R.depth-1); }
static float depth_dmg(void){ return 1.0f + 0.05f*(R.depth-1); }
static float depth_speed(void){ int d=R.depth; return 1.0f+0.06f*mini(d-1,4)+0.03f*maxi(0,d-5); }
static float cycle_hp(void){ return 1.0f + 1.3f*R.cycle + 0.4f*R.cycle*R.cycle; }
static float ramp(void){ if(R.single){ float P=lvl_prog(); return LVL_BASE[R.lvl]*(1.0f+1.5f*P+1.9f*P*P); } return BIOMES[R.biome].hp_mult; }
#define DUO_HP 1.5f    /* v10.1 : avec un compagnon, monstres et gardiens plus resistants */
#define DUO_DMG 1.25f  /* ... et plus dangereux */
static float duo_hp(void){ return R.ch2>=0 ? DUO_HP : 1.0f; }
static float hp_scale(void){ return duo_hp()*ramp()*depth_hp()*cycle_hp()*(R.pact?1.3f:1.0f)*(run_has_mod(MOD_OR)?1.25f:1.0f)*diff_cur()->en_hp; }
float run_hp_scale(void){ return hp_scale(); }
static float heal_mult(void){ if(run_has_mod(MOD_SANS_SOIN)) return 0; return curse_on(7) ? 0.5f : 1.0f; }
static float en_hit_scale(void){ return 1.0f+0.16f*run_diff(); }   /* contact et breches selon l'avancee */
static float xp_need(int L){ float l=(float)(L-1); return 3 + 2.5f*l + 0.45f*l*l; }
static float bdmg(int k,int lvl){ return BALLS[k].dmg + BALLS[k].dmg_lvl*(lvl-1); }
static float tfac(void){ return R.timeslow_t>0 ? 0.33f : 1.0f; }

int run_ball_level(int kind){ for(int i=0;i<R.nballs;i++) if(R.balls[i]==kind) return R.ball_lvl[i]; return 0; }
int run_pas_level(int kind){ for(int i=0;i<R.npas;i++) if(R.pas[i]==kind) return R.pas_lvl[i]; return 0; }
static void shake(float a){ if(S.shake && a>R.shake) R.shake=a; }
static void hitstop(float t){ if(R.hitstop_cd<=0 && t>R.hitstop){ R.hitstop=t; R.hitstop_cd=t+0.35f; } }
static int stat_dur(void){ return R.s_statdur; }

/* ---------------- effets ---------------- */
static Part *part_new(void){ Part *p=&R.part[R.part_next]; R.part_next=(R.part_next+1)%MAX_PART; memset(p,0,sizeof *p); return p; }
extern bool g_norender;   /* simulation sans affichage : pas de particules (purement visuelles) */
void spawn_particles(float x,float y,int n,uint32_t c,float spd,int kind,float life){
  if(g_norender) return;
  for(int i=0;i<n;i++){
    Part *p=part_new(); float a=frand()*TAU, s=spd*(0.25f+0.75f*frand());
    p->x=x; p->y=y; p->vx=cosf(a)*s; p->vy=sinf(a)*s; p->life=p->max=life*(0.6f+0.8f*frand());
    p->c=c; p->kind=(uint8_t)kind; p->size=1; p->drag=1.5f;
    switch(kind){
      case PT_PIX: case PT_SQ: p->grav=140; p->size=kind==PT_SQ?2:1; break;
      case PT_GLOW: p->size=(uint8_t)irange(2,4); p->drag=3; break;
      case PT_SMOKE: p->size=(uint8_t)irange(2,4); p->drag=3; p->grav=-12; break;
      case PT_SPARK: p->drag=4; break;
      case PT_EMBER: p->grav=-40; p->vy=-fabsf(p->vy)*0.6f; p->drag=1; break;
      case PT_RING: p->vx=p->vy=0; p->size=(uint8_t)clampi((int)spd,2,60); break;
      case PT_STREAK: p->drag=2.5f; break;
    }
  }
}
static void ring(float x,float y,float r,uint32_t c,float life){ if(g_norender) return; Part *p=part_new(); p->x=x; p->y=y; p->kind=PT_RING; p->size=(uint8_t)clampf(r,2,80); p->c=c; p->life=p->max=life; }
void add_ftext(float x,float y,const char *s,uint32_t c,int scale){
  static int nx=0; FText *t=NULL;
  for(int i=0;i<MAX_TXT;i++){ if(!R.txt[i].on){ t=&R.txt[i]; break; } }
  if(!t){ t=&R.txt[nx]; nx=(nx+1)%MAX_TXT; }
  t->on=true; t->x=x+frange(-3,3); t->y=y; t->vy=-34; t->life=0.8f; t->age=0; t->c=c; t->scale=scale;
  snprintf(t->s,sizeof t->s,"%s",s);
}
static void flash_light(float x,float y,float r,uint32_t c,float inten,float life){
  for(int i=0;i<MAX_FLASH;i++) if(!R.flashes[i].on){ Flash *f=&R.flashes[i]; f->on=true; f->x=x; f->y=y; f->r=r; f->c=c; f->inten=inten; f->life=f->max=life; return; }
}
static void bolt(float x0,float y0,float x1,float y1,uint32_t c){
  for(int i=0;i<MAX_BOLTS;i++) if(!R.bolts[i].on){ Bolt *b=&R.bolts[i]; b->on=true; b->x0=x0; b->y0=y0; b->x1=x1; b->y1=y1; b->c=c; b->life=0.16f; return; }
}
static Zone *zone_add(int kind,float x,float y,float r,float life){
  for(int i=0;i<MAX_ZONES;i++) if(!R.zones[i].on){ Zone *z=&R.zones[i]; memset(z,0,sizeof *z); z->on=true; z->kind=kind; z->x=x; z->y=y; z->r=r; z->life=z->max=life; return z; }
  return NULL;
}
static void shock_add(float x,float y,float rmax,float life,float str){
  int best=0; float bt=-1;
  for(int i=0;i<MAX_SHOCK;i++){ Shock *s=&R.shocks[i]; if(!s->on){ best=i; break; } if(s->t/s->max>bt){ bt=s->t/s->max; best=i; } }
  Shock *s=&R.shocks[best]; s->on=true; s->x=x; s->y=y; s->t=0; s->max=life; s->rmax=rmax; s->str=str;
}
static void beam_add(float x,float y0,float y1,uint32_t c,float w,float life){
  for(int i=0;i<MAX_BEAM;i++) if(!R.beams[i].on){ Beam *b=&R.beams[i]; b->on=true; b->x=x; b->y0=y0; b->y1=y1; b->c=c; b->w=w; b->life=b->max=life; return; }
}
/* eclatement d'un sprite en pixels */
static void shatter(float x,float y,int sprite,float scale,int maxp,uint32_t fallback){
  if(g_norender) return;
  if(sprite<0){ spawn_particles(x,y,maxp/2,fallback,90,PT_SQ,0.6f); return; }
  const Sprite *s=&SPR_RAW[sprite]; int n=0;
  for(int i=0;i<s->w*s->h;i++) if(s->px[i]>>24) n++;
  if(!n) return;
  int stride=n/maxp+1, k=irand(stride);
  float cx=s->w*0.5f, cy=s->h*0.5f;
  for(int j=0;j<s->h;j++) for(int i=0;i<s->w;i++){
    uint32_t c=s->px[j*s->w+i]; if(!(c>>24)) continue;
    if((k++)%stride) continue;
    Part *p=part_new(); p->kind=PT_SHARD; p->c=c; p->size=(uint8_t)(scale>1.4f||s->w>16?2:1);
    p->x=x+(i-cx)*scale; p->y=y+(j-cy)*scale;
    p->vx=(i-cx)*frange(9,18)+frange(-25,25); p->vy=(j-cy)*frange(6,12)-frange(50,120);
    p->grav=360; p->drag=1.2f; p->life=p->max=frange(0.45f,0.85f);
  }
}

/* ---------------- stats ---------------- */
void run_recalc_stats(void){
  const CharDef *c=&CHARS[R.ch]; const CharDef *c2= R.ch2>=0 ? &CHARS[R.ch2] : NULL;
  if(S.difficulty<R.diff_low) R.diff_low=clampi(S.difficulty,0,DIFF_COUNT-1);   /* recompenses : la difficulte la plus facile choisie compte */
  int lt=run_pas_level(PS_TREFLE);
  /* duo : les talents se cumulent (multiplicateurs des deux heros), on garde la meilleure base */
  float cdmg=1, cfire=1, cspeed=1; int topr=maxi(R.rank,R.rank2);
  for(int k=0;k<2;k++){ int h= k ? R.ch2 : R.ch; if(h<0) continue; const CharDef *cd=&CHARS[h];
    cdmg*= (h==CH_JONGLEUR && ch_rank(h)>=5) ? 1.0f : cd->dmg; cfire*=cd->fire; cspeed*=cd->speed; }
  float rk_dmg=maxf(hero_dmg_bonus(R.ch),R.ch2>=0?hero_dmg_bonus(R.ch2):0); int rk_hp=maxi(hero_hp_bonus(R.ch),R.ch2>=0?hero_hp_bonus(R.ch2):0);
  /* entrainement (prismes) : en duo, le meilleur des deux heros pour chaque attribut */
  float tr[TR_COUNT]; for(int k=0;k<TR_COUNT;k++){ tr[k]=train_bonus(R.ch,k); if(R.ch2>=0) tr[k]=maxf(tr[k],train_bonus(R.ch2,k)); }
  float base_hp=c2?maxf(c->hp,c2->hp):c->hp; int base_babies=c2?maxi(c->babies,c2->babies):c->babies;
  { extern float g_bot_skill; cdmg*=g_bot_skill; }   /* tests de rythme : joueur simule plus habile */
  R.s_dmg = cdmg*(1+rk_dmg)*(1+tr[TR_PUIS])*(1+0.15f*run_pas_level(PS_FORCE))*(1+0.06f*S.bld[BLD_FORGE])*(1+R.ev_dmg)*(run_has_mod(MOD_VERRE)?1.5f:1.0f)*(run_has_mod(MOD_JONGLE)?0.8f:1.0f);
  R.s_fire = 0.19f/(cfire*(1+0.14f*run_pas_level(PS_CADENCE))*(1+0.05f*S.bld[BLD_ATELIER])*(1+tr[TR_CADE]));
  R.s_speed = 108*cspeed*(1+0.12f*run_pas_level(PS_BOTTES))*(1+0.05f*S.bld[BLD_TOUR])*(1+R.ev_speed)*(1+tr[TR_VITE]);
  R.s_bspeed = 295*(1+0.12f*run_pas_level(PS_RESSORT));
  R.s_crit = minf(1.0f,0.05f+0.08f*run_pas_level(PS_LYNX)+0.02f*S.bld[BLD_ARMURERIE]+(topr>=10?0.05f:0.0f)+tr[TR_PREC]+(has_ch(CH_VOLEUSE)?(ch_rank(CH_VOLEUSE)>=5?0.12f:0.08f):0.0f));
  R.s_critm = has_ch(CH_FORGERONNE)?(ch_rank(CH_FORGERONNE)>=5?2.8f:2.5f):2.0f;
  R.s_pick = 34*(1+0.4f*run_pas_level(PS_LANTERNE))*(has_ch(CH_MAGNETISEUR)?2.2f:1.0f);
  R.s_xp = (R.len_k>0?R.len_k:1.0f)*(1+0.1f*run_pas_level(PS_LANTERNE))*(1+tr[TR_SAVO])*(1+0.08f*S.bld[BLD_BIBLIO])*(R.pact?1.6f:1.0f)*(run_has_mod(MOD_RUEE)?1.4f:1.0f)*(run_has_mod(MOD_BROUILLARD)?1.3f:1.0f);
  R.s_gold = (has_ch(CH_MARCHAND)?(ch_rank(CH_MARCHAND)>=5?1.75f:1.5f):1.0f)*(1+(lt>=3?0.6f:0.2f*lt))*(1+0.06f*S.bld[BLD_TRESOR])*depth_gold()*(R.pact?1.6f:1.0f)*(run_has_mod(MOD_OR)?2.0f:1.0f)*(run_has_mod(MOD_CHAMPIONS)?1.5f:1.0f)*DIFFS[R.diff_low].reward;
  R.s_regen = 0.5f*run_pas_level(PS_MOUSSE) + (has_ch(CH_DRUIDE)?(ch_rank(CH_DRUIDE)>=5?1.0f:0.6f):0.0f) + 0.1f*S.bld[BLD_SERRE];
  R.s_net = run_pas_level(PS_FILET)>0 ? 0.10f+0.10f*run_pas_level(PS_FILET) : 0;
  int fl=run_pas_level(PS_FIOLE);
  R.s_status = (has_ch(CH_ALCHIMISTE)?(ch_rank(CH_ALCHIMISTE)>=5?1.8f:1.6f):1.0f)*(1+0.25f*fl);
  R.s_statdur = fl;
  R.s_breach = has_ch(CH_SENTINELLE)?(ch_rank(CH_SENTINELLE)>=5?0.5f:0.65f):1.0f;
  R.s_babies = base_babies + 2*run_pas_level(PS_SAC) + R.ev_babies + (run_has_mod(MOD_JONGLE)?6:0) + (topr>=3?1:0);
  R.s_mirror = run_pas_level(PS_MIROIR)>0 ? 0.04f+0.08f*run_pas_level(PS_MIROIR) : 0;
  R.s_rage = run_pas_level(PS_RAGE)>0 ? 0.01f+0.03f*run_pas_level(PS_RAGE) : 0;
  R.s_dodge = 0.08f*run_pas_level(PS_CAPE);
  R.s_giant = 0.15f*run_pas_level(PS_LOUPE);
  R.s_anchor = 0.20f*run_pas_level(PS_ANCRE);
  R.s_thorns = run_pas_level(PS_TOTEM)>0 ? 3+2*run_pas_level(PS_TOTEM) : 0;
  R.s_slow = 0.06f*run_pas_level(PS_PENDULE);
  R.s_home = 0.55f*run_pas_level(PS_BOUSSOLE);
  R.s_grim = 0.15f*run_pas_level(PS_GRIMOIRE);
  /* v10 : mutateurs (nouveaux) et bonus general de recompenses */
  if(run_has_mod(MOD_HORDE)) R.s_xp*=1.25f;
  if(run_has_mod(MOD_FRAGILE)) R.s_xp*=1.3f;
  if(run_has_mod(MOD_SANS_SOIN)){ R.s_gold*=1.3f; R.s_regen=0; }
  if(run_has_mod(MOD_ELITES)) R.s_gold*=1.4f;
  if(R.muts){ int mb=mut_bonus(R.muts); R.s_gold*=1+mb*0.01f; R.s_xp*=1+mb*0.005f; }
  float old=R.hpmax;
  R.hpmax = (base_hp + 12*S.bld[BLD_INFIRMERIE] + rk_hp + tr[TR_VITA] + 25*run_pas_level(PS_COEUR) + R.ev_hp)*(run_has_mod(MOD_VERRE)?0.6f:1.0f);
  if(R.hpmax<10) R.hpmax=10;
  if(old>0 && R.hpmax>old) R.hp+=R.hpmax-old;
  if(R.hp>R.hpmax) R.hp=R.hpmax;
}

/* ---------------- billes ---------------- */
static int ball_pierce(int kind,int lvl){ return kind==BK_LUMIERE ? lvl : (kind==BK_AURORE ? 3 : (kind==BK_ECLIPSE ? 2 : 0)); }
static Ball *ball_new(void){ for(int i=0;i<MAX_BALLS;i++) if(!R.balls_e[i].on){ Ball *b=&R.balls_e[i]; memset(b,0,sizeof *b); b->on=true; return b; } return NULL; }
static void ball_setup(Ball *b,int kind,int lvl){
  b->kind=kind; b->lvl=lvl; b->r=BALLS[kind].r; if(kind==BK_PLOMB) b->r=4.0f+0.5f*lvl;
}
static void trail_reset(Ball *b){ for(int i=0;i<TRAIL_N;i++){ b->tx[i]=b->x; b->ty[i]=b->y; } }
static void sync_balls(void){
  bool assigned[MAX_BALLS]; memset(assigned,0,sizeof assigned);
  for(int s=0;s<R.nballs;s++){
    int found=-1;
    for(int i=0;i<MAX_BALLS;i++){ Ball *b=&R.balls_e[i]; if(b->on && !b->temp && !assigned[i] && b->kind==R.balls[s] && b->slot==s){ found=i; break; } }
    if(found<0) for(int i=0;i<MAX_BALLS;i++){ Ball *b=&R.balls_e[i]; if(b->on && !b->temp && !assigned[i] && b->kind==R.balls[s]){ found=i; break; } }
    if(found<0){ Ball *b=ball_new(); if(!b) continue; found=(int)(b-R.balls_e); b->state=BS_READY; b->x=R.px; b->y=R.py; }
    Ball *b=&R.balls_e[found]; assigned[found]=true; b->slot=s; ball_setup(b,R.balls[s],R.ball_lvl[s]);
  }
  int babies=0;
  for(int i=0;i<MAX_BALLS;i++){
    Ball *b=&R.balls_e[i]; if(!b->on || b->temp || assigned[i]) continue;
    if(b->kind==BK_BABY && babies<R.s_babies){ babies++; b->slot=-1; continue; }
    b->on=false;
  }
  while(babies<R.s_babies){ Ball *b=ball_new(); if(!b) break; ball_setup(b,BK_BABY,1); b->slot=-1; b->state=BS_READY; b->x=R.px; b->y=R.py; babies++; }
  for(int s=0;s<R.nballs;s++) S.balls_seen|=1ull<<R.balls[s];
  if(R.nballs>=MAX_SLOTS) meta_feat(FEAT_FULL_BALLS);
}
static int temp_count(void){ int n=0; for(int i=0;i<MAX_BALLS;i++) if(R.balls_e[i].on && R.balls_e[i].temp) n++; return n; }
static Ball *temp_ball(int kind,int lvl,float x,float y,float ang,float speed,float life){
  if(temp_count()>=80) return NULL;
  Ball *b=ball_new(); if(!b) return NULL;
  ball_setup(b,kind,lvl); b->temp=true; b->slot=-1; b->state=BS_FLY; b->x=x; b->y=y;
  b->speed=speed; b->vx=cosf(ang)*speed; b->vy=sinf(ang)*speed; b->life=life;
  b->pierce = ball_pierce(kind,lvl);
  trail_reset(b);
  return b;
}
static void ball_launch(Ball *b){
  float ax=R.aimx-R.px, ay=R.aimy-(R.py-8);
  float d=sqrtf(ax*ax+ay*ay); if(d<0.01f){ ax=0; ay=-1; d=1; }
  ax/=d; ay/=d;
  if(ay>-0.26f){ ay=-0.26f; ax=signf(ax)*sqrtf(1-ay*ay); }
  b->state=BS_FLY; b->x=R.px; b->y=R.py-8;
  b->speed=R.s_bspeed*BALLS[b->kind].speed;
  float spread=frange(-0.03f,0.03f); float ca=cosf(spread), sa=sinf(spread);
  float vx=ax*ca-ay*sa, vy=ax*sa+ay*ca;
  b->vx=vx*b->speed; b->vy=vy*b->speed;
  for(int i=0;i<4;i++){ b->hit_id[i]=0; b->hit_t[i]=0; }
  b->relaunch=0; b->cd=0; b->cd2=0; b->bounces=0; b->wall=0; b->hits=0; b->beam_t=0.15f; b->fly_t=0; b->boom=0;
  b->pierce = ball_pierce(b->kind,b->lvl);
  trail_reset(b);
  sfx_play(SFX_SHOOT,0.35f,(R.px-320)/320.0f*0.6f,frange(0.95f,1.08f));
  if(R.s_mirror>0 && b->kind!=BK_BABY && chance(R.s_mirror)){
    float a=atan2f(vy,vx)+frange(-0.35f,0.35f);
    Ball *c=temp_ball(b->kind,b->lvl,b->x,b->y,a,b->speed,2.6f);
    if(c){ c->relaunch=9; spawn_particles(b->x,b->y,6,HEX(0xd8f0ff),40,PT_GLOW,0.3f); }
  }
}
static Ball *shard_new(float x,float y,int elec){
  float a=frange(-PI*0.92f,-PI*0.08f);
  return temp_ball(BK_SHARD,elec?2:1,x,y,a,230,2.2f);
}
static void ghost_new(float x,float y){
  int g=0; for(int i=0;i<MAX_BALLS;i++) if(R.balls_e[i].on && R.balls_e[i].kind==BK_GHOST) g++;
  if(g>=14) return;
  Ball *b=temp_ball(BK_GHOST,1,x,y,frange(-PI*0.85f,-PI*0.15f),210,4.0f);
  if(b){ spawn_particles(x,y,8,HEX(0x8affc0),50,PT_GLOW,0.5f); sfx_at(SFX_GHOST,x,0.3f); }
}

/* ---------------- joueur ---------------- */
static void run_lose(void);
float g_hurt_by[5]; int g_hurt_cat=1; int g_breach_kind[EK_COUNT]; float g_inflow, g_dealt;   /* banc de test : degats subis sur le niveau en cours (breche, tirs, contact, boss, lave) */
static void player_hurt(float dmg,bool breach){
  int cat=breach?0:g_hurt_cat; g_hurt_cat=1;
  if(R.ch2>=0) dmg*=DUO_DMG;
  if(R.sub!=RS_PLAY && R.sub!=RS_CLEAR) return;
  if(breach) R.breaches++;
  if(!breach && R.iframes>0) return;
  if(!breach && R.s_dodge>0 && chance(R.s_dodge)){   /* v7 : cape d'ombre */
    R.iframes=0.35f; add_ftext(R.px,R.py-26,"Esquive !",HEX(0xc8b8ff),1);
    spawn_particles(R.px,R.py-6,10,HEX(0x3a2a5a),40,PT_SMOKE,0.5f); spawn_particles(R.px,R.py-6,6,HEX(0xa898ff),60,PT_GLOW,0.3f);
    sfx_play(SFX_BLINK,0.35f,0,0.8f); return;
  }
  if(!breach && R.shield_up){
    R.shield_up=false; R.shield_cd=30-6*run_pas_level(PS_BOUCLIER); R.iframes=0.6f; R.shield_flash=1;
    ring(R.px,R.py-6,22,HEX(0x5ab8ff),0.35f); spawn_particles(R.px,R.py-6,12,HEX(0x9ad8ff),80,PT_SPARK,0.35f);
    sfx(SFX_BLOCK); rumble(0.3f,0.1f); add_ftext(R.px,R.py-26,"Bloqué !",HEX(0x9ad8ff),1); return;
  }
  if(breach) dmg*=R.s_breach;
  dmg*=depth_dmg()*diff_cur()->en_dmg;
  if(R.single && !R.boss.on && R.dmg_k>0) dmg*=R.dmg_k;
  { extern float g_bot_skill; if(g_bot_skill>1) dmg/=sqrtf(g_bot_skill); }
  R.hp-=dmg; R.hurt_t=0.35f; if(!breach) R.iframes=0.9f; g_hurt_by[cat]+=dmg;
  R.lvl_hits++; R.lvl_hits_seg++; if(R.boss.on||R.mb_on) R.boss_hits++;
  char b[16]; snprintf(b,sizeof b,"-%d",(int)(dmg+0.5f)); add_ftext(R.px,R.py-22,b,HEX(0xff4a4a),1);
  shake(breach?5:4); sfx(SFX_HURT); rumble(breach?0.6f:0.45f,breach?0.25f:0.16f); hitstop(0.05f);
  spawn_particles(R.px,R.py-6,10,HEX(0xff4a4a),70,PT_SQ,0.5f);
  if(R.s_thorns>0 && R.thorn_cd<=0 && R.hp>0){   /* v7 : totem d'epines */
    for(int i=0;i<R.s_thorns;i++){ Ball *t=shard_new(R.px,R.py-10,0); if(t) t->lvl=3; }
    R.thorn_cd=0.6f; ring(R.px,R.py-8,20,HEX(0xd8c8a0),0.3f); sfx_at(SFX_SPLIT,R.px,0.4f);
  }
  if(R.hp<=0) run_lose();
}
static void heal(float v){
  if(R.hp>=R.hpmax) return;
  v*=heal_mult(); if(v<=0) return;
  R.hp=minf(R.hpmax,R.hp+v);
  char b[16]; snprintf(b,sizeof b,"+%d",(int)(v+0.5f)); add_ftext(R.px,R.py-24,b,HEX(0x8be05a),1);
}

/* ---------------- ennemis ---------------- */
static Enemy *enemy_spawn(int kind,float x,float y,float hpmult){
  for(int i=0;i<MAX_EN;i++) if(!R.en[i].on){
    Enemy *e=&R.en[i]; memset(e,0,sizeof *e); const EnemyDef *d=&ENEMIES[kind];
    e->on=true; e->kind=kind; e->id=R.next_id++; e->x=x; e->y=y; e->w=(float)(d->w*EN_SCALE); e->h=(float)(d->h*EN_SCALE); e->scale=1;
    e->hpmax=e->hp=d->hp*hpmult*EN_HP;   /* personnages agrandis : deux fois moins d'ennemis par ligne, deux fois plus resistants */ e->shoot=d->shoot_cd*frange(0.5f,1.2f); e->t=frand()*10; { extern float g_inflow; g_inflow+=e->hpmax; }
    e->vx = (d->flags&EF_DRIFT) ? (chance(0.5f)?18.f:-18.f) : 0; e->ang=x; e->spawn=0;
    e->special=frange(1.5f,4.0f); e->orb=frand()*TAU; e->summon_t=frange(1.5f,3.5f);
    if((d->flags&EF_ELITE) && curse_on(10)){ e->hpmax*=1.4f; e->hp=e->hpmax; }
    if((d->flags&EF_ELITE) && run_has_mod(MOD_ELITES)){ e->hpmax*=1.5f; e->hp=e->hpmax; }
    e->link_t=0; e->link_id=0;
    return e;
  }
  return NULL;
}
static void make_champion(Enemy *e){
  int c=irange(1,CHAMP_COUNT-1); e->champ=c;
  switch(c){
    case CHAMP_RAPIDE: e->hpmax*=1.6f; break;
    case CHAMP_COLOSSE: e->hpmax*=3.2f; e->scale=1.35f; e->w*=1.3f; e->h*=1.3f; break;
    case CHAMP_REGEN: e->hpmax*=2.0f; break;
    case CHAMP_EXPLOSIF: e->hpmax*=1.6f; break;
    case CHAMP_INVOCATEUR: e->hpmax*=2.0f; e->champ_t=2.5f; break;
    case CHAMP_SOIGNEUR: e->hpmax*=1.8f; e->champ_t=1.0f; break;
    case CHAMP_SPECTRAL: e->hpmax*=1.7f; break;
    default: e->hpmax*=1.8f; e->shield=e->hpmax*0.5f; break;
  }
  if(run_has_mod(MOD_ELITES)) e->hpmax*=1.5f;
  if(curse_on(10)){ e->hpmax*=1.4f; e->shield*=1.4f; }
  e->hp=e->hpmax;
}
static void drop_pickup(int kind,float x,float y,int val){
  for(int i=0;i<MAX_PICK;i++) if(!R.pick[i].on){
    Pickup *p=&R.pick[i]; memset(p,0,sizeof *p); p->on=true; p->kind=kind; p->x=x; p->y=y; p->val=val;
    p->vx=frange(-35,35); p->vy=frange(-70,-35); return;
  }
}
static Bullet *enemy_bullet(float x,float y,float ang,float spd,float r,uint32_t col,int kind){
  for(int i=0;i<MAX_BUL;i++) if(!R.bul[i].on){
    Bullet *b=&R.bul[i]; memset(b,0,sizeof *b); if(curse_on(9)) spd*=1.25f; b->on=true; b->kind=kind; b->x=x; b->y=y; b->vx=cosf(ang)*spd; b->vy=sinf(ang)*spd;
    b->r=r; b->col=col; b->dmg=7+1.8f*run_diff(); b->life=8; b->t=0; return b;
  }
  return NULL;
}
static void lob(float x,float y,int kind){
  Bullet *b=enemy_bullet(x,y,0,0,3.5f,kind==4?HEX(0xffb030):(kind==5?HEX(0x9ab040):HEX(0xff8a2a)),kind); if(!b) return;
  b->tx=clampf(R.px+frange(-24,24),PIT_X0+10,PIT_X1-10); b->ty=clampf(R.py+frange(-10,10),ZONE_TOP+6,ZONE_BOT);
  b->tz=0; b->vz=1.25f; b->vx=(b->tx-x)/b->vz; b->vy=(b->ty-y)/b->vz; b->life=b->vz;
}
static void lava_lob(float x,float y){ lob(x,y,2); }
static void amber_lob(float x,float y){ lob(x,y,4); }
static bool submerged(const Enemy *e){ return (ENEMIES[e->kind].flags&EF_DIVE) && e->sstate==1; }
static float aim_at_player(float x,float y){ return atan2f((R.py-6)-y,R.px-x); }

static void boss_hurt(float dmg,uint32_t col,bool crit);
static bool g_quake=false, g_link=false;   /* v10 : degats venus d'une secousse, d'une chaine */
static void explode(float x,float y,float r,float dmg,uint32_t col,int burn,bool big);
static void enemy_kill(Enemy *e,int src);
static void miniboss_defeated(Enemy *e);
static int nblast=0; static float blast_x[64], blast_y[64];
static bool g_curio_kill=false;
static int ndark=0; static float dark_x[64], dark_y[64]; static int dark_k[64];   /* v9 : eclats d'ombre (Eclipse) et de glace (Zero absolu) */   /* v8 : les monstres vaincus par un piege du decor comptent pour les defis */

static void enemy_hurt(Enemy *e,float dmg,uint32_t col,bool from_below,bool crit,bool direct){
  if(!e->on) return;
  const EnemyDef *d=&ENEMIES[e->kind];
  if(!e->mboss && (d->flags&EF_PHASE) && fmodf(e->phase,3.2f)>2.2f) return;
  if(e->champ==CHAMP_SPECTRAL && fmodf(e->t,3.0f)>2.0f) return;   /* v10 : champion spectral, intangible un instant */
  if(!e->mboss && submerged(e)) return;
  if(direct && !e->mboss){
    bool melt=e->acid>=3;   /* v7 : la corrosion fait fondre boucliers, carapaces et armures */
    if((d->flags&EF_SHIELD) && from_below && !melt){   /* le bouclier arrete l'essentiel des coups venus d'en bas */
      spawn_particles(e->x,e->y+e->h/2,3,HEX(0xd2b0f5),60,PT_SPARK,0.25f);
      sfx_play(SFX_SHIELD,0.2f,0,frange(0.9f,1.1f)); dmg*=0.3f;
    }
    if((d->flags&EF_PLATE) && from_below && !melt) dmg*=0.4f;
    if((d->flags&EF_ARMOR) && !melt) dmg=maxf(dmg*0.35f,dmg-4.0f);
    if(run_has_mod(MOD_BLINDES)) dmg=maxf(dmg*0.5f,dmg-3.0f);
    e->kick=2.5f;
  }
  if(e->freeze_t>0) dmg*=1.3f;
  if(e->root_t>0) dmg*=1.25f;
  if(e->acid>0) dmg*=1+0.05f*e->acid;
  if(e->mark_t>0) dmg*=1+e->mark_k;
  if(e->shield>0){ float a=minf(e->shield,dmg); e->shield-=a; dmg-=a; if(e->shield<=0) spawn_particles(e->x,e->y,10,HEX(0x5ab8ff),70,PT_SPARK,0.3f); }
  g_dealt+=maxf(0,minf(dmg,e->hp)); e->hp-=dmg; e->flash=0.1f; e->hpbar_t=2.0f; e->calm_t=0;
  if(S.dmg_numbers && direct){
    char b[16]; snprintf(b,sizeof b,crit?"%d!":"%d",(int)(dmg+0.5f));
    add_ftext(e->x,e->y-e->h/2-2,b,crit?HEX(0xff5a48):col,crit?2:1);
  }
  if(e->link_t>0 && !g_link && dmg>0){   /* v10 : la chaine transmet une part des degats */
    for(int i=0;i<MAX_EN;i++){ Enemy *o=&R.en[i]; if(!o->on || o==e || o->id!=e->link_id) continue;
      g_link=true; enemy_hurt(o,dmg*e->link_k,HEX(0xa8c8ff),false,false,false); g_link=false; break; } }
  if(e->hp<=0) enemy_kill(e,0);
}
static void enemy_dot(Enemy *e,float dmg,uint32_t col){
  if(e->root_t>0) dmg*=1.25f;
  if(e->mark_t>0) dmg*=1+e->mark_k;
  if(e->acid>0) dmg*=1+0.05f*e->acid;
  g_dealt+=maxf(0,minf(dmg,e->hp)); e->hp-=dmg; e->dot_acc+=dmg; e->hpbar_t=2.0f; e->calm_t=0;
  if(e->dot_show<=0 && e->dot_acc>=1){
    if(S.dmg_numbers){ char b[16]; snprintf(b,sizeof b,"%d",(int)(e->dot_acc+0.5f)); add_ftext(e->x,e->y-e->h/2-2,b,col,1); }
    e->dot_acc=0; e->dot_show=0.5f;
  }
  if(e->hp<=0) enemy_kill(e,0);
}
static void enemy_kill(Enemy *e,int src){
  (void)src;
  if(!e->on) return;
  const EnemyDef *d=&ENEMIES[e->kind];
  e->on=false; R.kills++; meta_note_kill(e->kind); if(g_curio_kill) S.curio_kills++;
  if(e->mboss) miniboss_defeated(e);
  if(ball_masterable(e->last_ball) || IS_LEG(e->last_ball)) S.ball_kills[e->last_ball]++;
  if(e->champ){ S.champions++; R.champ_kills++; if(e->champ<16) S.champ_kinds[e->champ]++; }
  if(g_quake) S.quake_kills++; if(g_link) S.link_kills++;
  if(e->mark_t>0) S.mark_kills++;
  bool elite=(d->flags&EF_ELITE)!=0;
  shatter(e->x,e->y,d->sprite2>=0&&(((int)(e->t*6))&1)?d->sprite2:d->sprite,e->scale,elite?70:(e->champ?40:22),d->blood);
  spawn_particles(e->x,e->y,elite?14:5,d->blood,elite?120:70,PT_SQ,0.5f);
  spawn_particles(e->x,e->y,elite?8:3,HEX(0x403848),20,PT_SMOKE,0.8f);
  sfx_at(SFX_KILL,e->x,elite?0.9f:0.5f);
  if(elite){ shake(3); flash_light(e->x,e->y,60,d->blood,1.4f,0.35f); shock_add(e->x,e->y,46,0.4f,1.4f); hitstop(0.045f); R.st_elites++; S.elites++; ring(e->x,e->y,30,HEX(0xffffff),0.3f); }
  if(!e->guard){
    if(d->xp>0){ int v=d->xp*EN_XP; drop_pickup(v>=8?PK_GEM3:(v>=2?PK_GEM2:PK_GEM),e->x,e->y,v); }
    if(e->champ){ drop_pickup(PK_GEM2,e->x,e->y,3); for(int i=0;i<3;i++) drop_pickup(PK_COIN,e->x,e->y,2); }
    float cc=d->coin*EN_COIN; if(e->minion) cc*=0.4f;
    if(elite){ for(int i=0;i<4;i++) drop_pickup(PK_COIN,e->x,e->y,2); }
    else { while(cc>1){ drop_pickup(PK_COIN,e->x,e->y,irange(1,2)); cc-=1; } if(chance(cc)) drop_pickup(PK_COIN,e->x,e->y,irange(1,2)); }
    if(chance(0.012f*EN_COIN*heal_mult())) drop_pickup(PK_HEART,e->x,e->y,10);
    if(e->last_ball==BK_AURORE) drop_pickup(PK_GEM,e->x,e->y,1);
    if(e->last_ball==BK_MIDAS){   /* change en or */
      for(int i=0;i<(elite?5:2);i++) drop_pickup(PK_COIN,e->x,e->y,elite?3:1);
      spawn_particles(e->x,e->y,elite?18:8,HEX(0xffd84a),elite?110:70,PT_SPARK,0.45f); spawn_particles(e->x,e->y,4,HEX(0xfff4c0),30,PT_GLOW,0.5f);
      ring(e->x,e->y,elite?26:14,HEX(0xffe070),0.3f); sfx_at(SFX_GOLD,e->x,0.35f);
    }
  }
  if((d->flags&EF_SPLIT) && d->child>=0){
    for(int s=-1;s<=1;s+=2){ Enemy *c=enemy_spawn(d->child,e->x+s*9,e->y,e->hpmax/d->hp*0.9f/(e->champ?2.0f:1.0f)); if(c){ c->push=-30; c->spawn=1; c->minion=e->minion; } }
  }
  if(e->champ==CHAMP_EXPLOSIF){ for(int k=0;k<8;k++) enemy_bullet(e->x,e->y,k*TAU/8+0.2f,62,3,HEX(0xff7a3a),1); explode(e->x,e->y,26,10,HEX(0xff8a3a),0,false); }
  if(d->flags&EF_EMBERS){
    float a=aim_at_player(e->x,e->y);
    for(int i=-1;i<=1;i++) enemy_bullet(e->x,e->y,a+i*0.35f,70,2.5f,d->blood,1);
  }
  if(e->poison>0 && run_ball_level(BK_FLEAU)>0){
    zone_add(ZN_POISON,e->x,e->y,30,0.6f);
    for(int i=0;i<MAX_EN;i++){ Enemy *o=&R.en[i]; if(!o->on) continue; if(sqrf(o->x-e->x)+sqrf(o->y-e->y)<34*34){ o->poison+=e->poison/2+1; o->poison_t=6; } }
  }
  if(e->last_ball==BK_FAUCHEUSE) heal(1);
  if(e->last_ball==BK_GEODE){ for(int i=0;i<3;i++) shard_new(e->x,e->y,0); spawn_particles(e->x,e->y,8,HEX(0xd090ff),70,PT_SPARK,0.3f); }
  if(has_ch(CH_PYROMANE) && e->burn>0 && nblast<64){ blast_x[nblast]=e->x; blast_y[nblast]=e->y; nblast++; }
  if(e->mark_t>0 && run_ball_level(BK_ECLIPSE)>0 && ndark<64){ dark_x[ndark]=e->x; dark_y[ndark]=e->y; dark_k[ndark]=0; ndark++; }
  if(e->freeze_t>0 && run_ball_level(BK_ZERO)>0 && ndark<64){ dark_x[ndark]=e->x; dark_y[ndark]=e->y; dark_k[ndark]=1; ndark++; }
  if(e->last_ball==BK_OUROBOROS){ heal(1); for(int i=0;i<MAX_EN;i++){ Enemy *o=&R.en[i]; if(!o->on || o==e) continue; if(sqrf(o->x-e->x)+sqrf(o->y-e->y)<36*36){ o->poison+=3; o->poison_t=maxf(o->poison_t,6.0f); } }
    spawn_particles(e->x,e->y,8,HEX(0x9af06a),50,PT_GLOW,0.5f); }
  if(has_ch(CH_NECRO) && !e->guard && chance(ch_rank(CH_NECRO)>=5?0.18f:0.12f)) ghost_new(e->x,e->y);
  R.combo++; R.combo_t=1.2f; if(R.combo>S.best_combo) S.best_combo=R.combo; if(R.combo>R.combo_max) R.combo_max=R.combo;
  if(R.kills>=500) meta_feat(FEAT_KILLS_1000);
}
static int count_enemies(void){ int n=0; for(int i=0;i<MAX_EN;i++) if(R.en[i].on) n++; return n; }

/* ---------------- effets des billes ---------------- */
static void chain_lightning(float x,float y,int first_id,int count,float dmg,float freeze){
  int hit[12]; int nh=0; hit[nh++]=first_id; float cx=x, cy=y;
  for(int c=0;c<count && nh<12;c++){
    Enemy *best=NULL; float bd=66*66;
    for(int i=0;i<MAX_EN;i++){
      Enemy *e=&R.en[i]; if(!e->on || e->y<-4) continue;
      bool skip=false; for(int k=0;k<nh;k++) if(hit[k]==e->id) skip=true; if(skip) continue;
      float d=sqrf(e->x-cx)+sqrf(e->y-cy); if(d<bd){ bd=d; best=e; }
    }
    if(!best) break;
    bolt(cx,cy,best->x,best->y,HEX(0xfff070));
    hit[nh++]=best->id; cx=best->x; cy=best->y;
    enemy_hurt(best,dmg*R.s_dmg,HEX(0xfff070),false,false,false);
    if(freeze>0 && chance(freeze)){ best->freeze_t=1.2f; }
  }
  if(nh>1){ sfx_at(SFX_ZAP,x,0.35f); flash_light(x,y,40,HEX(0xfff070),0.8f,0.15f); }
}
static void explode(float x,float y,float r,float dmg,uint32_t col,int burn,bool big){
  for(int i=0;i<MAX_EN;i++){
    Enemy *e=&R.en[i]; if(!e->on) continue;
    float qx=clampf(x,e->x-e->w/2,e->x+e->w/2), qy=clampf(y,e->y-e->h/2,e->y+e->h/2);
    if(sqrf(qx-x)+sqrf(qy-y)<=r*r){
      if(burn>0){ e->burn=clampi(e->burn+burn,0,14); e->burn_t=3.0f+stat_dur(); }
      enemy_hurt(e,dmg*R.s_dmg,col,false,false,false);
    }
  }
  if(R.boss.on && R.boss.enter>=1 && R.boss.dying<=0){
    Boss *b=&R.boss; float qx=clampf(x,b->x-b->w/2,b->x+b->w/2), qy=clampf(y,b->y-b->h/2,b->y+b->h/2);
    if(sqrf(qx-x)+sqrf(qy-y)<=r*r) boss_hurt(dmg*R.s_dmg*0.8f,col,false);
  }
  ring(x,y,r,col,0.3f);
  spawn_particles(x,y,big?22:9,col,big?140:90,PT_SPARK,0.35f);
  spawn_particles(x,y,big?8:3,HEX(0x50484a),30,PT_SMOKE,0.9f);
  flash_light(x,y,r*2.2f,col,big?1.6f:1.0f,big?0.35f:0.2f);
  if(big) shock_add(x,y,r*1.6f,0.35f,1.2f);
  sfx_play(SFX_EXPLODE,big?0.75f:0.3f,(x-320)/320.0f*0.6f,frange(0.9f,1.15f));
  shake(big?4:1.5f);
}
/* bourrasque : 0 vent, 1 blizzard, 2 tempete de feu */
static void gust(float x,float y,float r,float dmg,int kind){
  uint32_t col = kind==1?HEX(0xd8f4ff):(kind==2?HEX(0xffa050):HEX(0xc8f8d8));
  for(int i=0;i<MAX_EN;i++){
    Enemy *e=&R.en[i]; if(!e->on) continue;
    float qx=clampf(x,e->x-e->w/2,e->x+e->w/2), qy=clampf(y,e->y-e->h/2,e->y+e->h/2);
    if(sqrf(qx-x)+sqrf(qy-y)>r*r) continue;
    if(!e->guard && !(ENEMIES[e->kind].flags&EF_ELITE)) e->push=minf(e->push,kind==1?-105.0f:-72.0f);
    e->slow_t=maxf(e->slow_t,1.3f);
    if(kind==1 && chance(0.3f)) e->freeze_t=1.0f;
    if(kind==2){ e->burn=clampi(e->burn+2,0,14); e->burn_t=3.0f+stat_dur(); }
    enemy_hurt(e,dmg*R.s_dmg,col,false,false,false);
  }
  if(R.boss.on && R.boss.enter>=1 && R.boss.dying<=0 && fabsf(R.boss.x-x)<r+R.boss.w/2 && fabsf(R.boss.y-y)<r+R.boss.h/2) boss_hurt(dmg*R.s_dmg*0.8f,col,false);
  ring(x,y,r,col,0.28f);
  for(int k=0;k<10 && !g_norender;k++){ Part *p=part_new(); float a=frand()*TAU; p->kind=PT_STREAK; p->x=x+cosf(a)*r*0.4f; p->y=y+sinf(a)*r*0.4f; p->vx=-sinf(a)*150+cosf(a)*60; p->vy=cosf(a)*150+sinf(a)*60; p->c=col; p->life=p->max=0.3f; p->drag=3; }
  if(kind==2) zone_add(ZN_FIRE,x,y,22,2.0f);
  sfx_at(SFX_WIND,x,0.3f);
}
static void holy_beam(float x,float y){
  for(int i=0;i<MAX_EN;i++){
    Enemy *e=&R.en[i]; if(!e->on || e->y>y || fabsf(e->x-x)>7+e->w/2) continue;
    e->blind_t=maxf(e->blind_t,2.0f);
    enemy_hurt(e,14*R.s_dmg,HEX(0xfff0a0),false,false,false);
  }
  if(R.boss.on && R.boss.enter>=1 && R.boss.dying<=0 && fabsf(R.boss.x-x)<R.boss.w/2+6 && R.boss.y<y) boss_hurt(14*R.s_dmg*0.8f,HEX(0xfff0a0),false);
  beam_add(x,0,y,HEX(0xffe8a0),5,0.22f);
  sfx_play(SFX_BEAM,0.18f,(x-320)/320.0f*0.6f,frange(0.9f,1.1f));
}

/* ---------------- curiosites du decor (v8) ---------------- */
#define CURIO_CY 9.0f      /* centre du volume au-dessus du point de pose */
static void curio_spawn(void){
  for(int i=0;i<MAX_CURIO;i++) if(!R.cur[i].on){ Curio *c=&R.cur[i]; memset(c,0,sizeof *c);
    c->on=true; c->kind = chance(0.7f) ? (R.curio_last==CU_TRESOR?CU_PIEGE:CU_TRESOR) : R.curio_last;
    R.curio_last=c->kind; R.curio_n++;
    c->x=frange(PIT_X0+30,PIT_X1-30); c->y=-6; c->r= c->kind==CU_TRESOR ? 11 : 10;
    c->hmax=c->hits= c->kind==CU_TRESOR ? 3+mini(3,(R.depth-1)/3) : 1;
    return; }
}
static void curio_burst(const Curio *c,int n){   /* eclats du sprite */
  if(g_norender) return;
  int ax,ay; const Sprite *s=curio_sprite(R.biome,c->kind,&ax,&ay); int tot=0;
  for(int i=0;i<s->w*s->h;i++) if(s->px[i]>>24) tot++;
  if(!tot) return; int stride=tot/n+1, k=irand(stride);
  for(int j=0;j<s->h;j++) for(int i=0;i<s->w;i++){ uint32_t col=s->px[j*s->w+i]; if(!(col>>24) || ((k++)%stride)) continue;
    Part *p=part_new(); p->kind=PT_SHARD; p->c=col; p->size=2; p->x=c->x+(i-ax)*2.0f; p->y=c->y+(j-ay)*2.0f;
    p->vx=(i-ax)*frange(8,16)+frange(-30,30); p->vy=(j-ay)*frange(4,9)-frange(60,130); p->grav=360; p->drag=1.2f; p->life=p->max=frange(0.45f,0.9f); }
}
static void curio_break(Curio *c){
  float x=c->x, y=c->y-CURIO_CY; int b=R.biome;
  c->on=false; S.curios++; R.curios_run++;
  curio_burst(c,60);
  float cab=1+0.12f*S.bld[BLD_CABINET];
  int n=(int)((5+irand(4))*cab); if(b==11) n*=2;
  for(int i=0;i<n;i++) drop_pickup(PK_COIN,x,y,irange(1,2)+(R.depth>=6?1:0));
  drop_pickup(PK_GEM2,x,y,(int)(4*cab)); drop_pickup(PK_GEM2,x,y,(int)(4*cab));
  if(b==3||b==7) drop_pickup(PK_GEM3,x,y,8);
  if(b==10) for(int i=0;i<3;i++) drop_pickup(PK_GEM3,x,y,8);
  if(b==6||b==8){ heal(14); drop_pickup(PK_HEART,x,y,10); spawn_particles(x,y,12,HEX(0xb8ff7a),50,PT_GLOW,0.6f); }
  else if(chance(0.25f*heal_mult())) drop_pickup(PK_HEART,x,y,10);
  ring(x,y,26,HEX(0xffe070),0.35f); spawn_particles(x,y,18,HEX(0xffd84a),110,PT_SPARK,0.45f); spawn_particles(x,y,5,HEX(0xfff4c0),30,PT_GLOW,0.6f);
  flash_light(x,y,70,HEX(0xffd070),1.2f,0.3f); sfx_at(SFX_GOLD,x,0.7f); sfx_at(SFX_KILL,x,0.4f); shake(2);
  add_ftext(x,y-14,"Trésor !",HEX(0xffd84a),1);
}
static void curio_status(float x,float y,float r,int kind,float t){   /* effet secondaire d'un piege sur les monstres proches */
  for(int i=0;i<MAX_EN;i++){ Enemy *e=&R.en[i]; if(!e->on) continue; if(sqrf(e->x-x)+sqrf(e->y-y)>sqrf(r+e->w*0.5f)) continue;
    switch(kind){
      case 0: e->poison=clampi(e->poison+4,0,20); e->poison_t=maxf(e->poison_t,6.0f); break;
      case 1: if(!e->mboss) e->freeze_t=maxf(e->freeze_t,t); break;
      case 2: if(!e->mboss) e->stun_t=maxf(e->stun_t,t); break;
      case 3: e->blind_t=maxf(e->blind_t,t); e->slow_t=maxf(e->slow_t,t); break;
      default: e->blind_t=maxf(e->blind_t,t); break;
    } }
}
static void curio_trigger(Curio *c){
  float x=c->x, y=c->y-CURIO_CY; int b=R.biome;
  c->on=false; S.curios++; R.curios_run++;
  curio_burst(c,40);
  float dmg=12.0f*hp_scale()*EN_HP/maxf(R.s_dmg,0.05f)*(1+0.12f*S.bld[BLD_CABINET]);   /* explode() multiplie par la puissance du heros : on vise ~12 PV de base */
  g_curio_kill=true;
  switch(b){
    case 0: explode(x,y,46,dmg,HEX(0xff7a2a),5,true); { Zone *z=zone_add(ZN_FIRE,x,y,26,3.0f); (void)z; } break;
    case 1: explode(x,y,40,dmg*0.5f,HEX(0xc8e89a),0,false); curio_status(x,y,54,0,0); zone_add(ZN_POISON,x,y,44,1.4f);
      spawn_particles(x,y,26,HEX(0xd8e8a0),70,PT_SMOKE,1.2f); spawn_particles(x,y,14,HEX(0xc8ff8a),60,PT_GLOW,0.8f); break;
    case 2: explode(x,y,60,dmg*1.4f,HEX(0xff8a2a),6,true); zone_add(ZN_FIRE,x,y,34,3.5f); shake(6); break;
    case 3: explode(x,y,52,dmg,HEX(0xb070ff),0,true); curio_status(x,y,60,3,3.0f); break;
    case 4: explode(x,y,48,dmg*0.8f,HEX(0x9ae8ff),0,false); curio_status(x,y,56,1,2.5f); sfx_at(SFX_FREEZE,x,0.7f);
      spawn_particles(x,y,20,HEX(0xe0f6ff),90,PT_SHARD,0.6f); break;
    case 5: explode(x,y,58,dmg*1.3f,HEX(0x5af0d0),0,true); curio_status(x,y,62,2,1.2f); break;
    case 6: explode(x,y,44,dmg*0.7f,HEX(0xffb030),0,false); curio_status(x,y,50,0,0);
      for(int i=0;i<6;i++){ Ball *t=shard_new(x,y,0); if(t) t->lvl=3; } sfx_at(SFX_SPLIT,x,0.6f); break;
    case 7: explode(x,y,36,dmg*0.6f,HEX(0xd8e0ff),0,false); chain_lightning(x,y,-1,8,12.0f*hp_scale()*EN_HP/maxf(R.s_dmg,0.05f)*0.7f,0.25f); sfx_at(SFX_THUNDER,x,0.6f); break;
    case 8: explode(x,y,40,dmg*0.6f,HEX(0xeaff7a),0,false); curio_status(x,y,70,4,4.0f); heal(10); sfx(SFX_HEAL);
      spawn_particles(x,y,24,HEX(0xeaff7a),80,PT_GLOW,0.9f); break;
    case 9: explode(x,y,50,dmg,HEX(0xb8d040),3,true); curio_status(x,y,56,0,0); spawn_particles(x,y,20,HEX(0x9ab040),60,PT_SMOKE,1.0f); break;
    case 10: explode(x,y,64,dmg*1.5f,HEX(0xffc060),4,true); curio_status(x,y,66,2,0.8f); shake(6); break;
    default: {   /* miroir solaire : un eventail de rayons vers le haut */
      float bd=dmg*0.8f*R.s_dmg;
      for(int k=-2;k<=2;k++){ float bx=x+k*14;
        for(int i=0;i<MAX_EN;i++){ Enemy *e=&R.en[i]; if(!e->on || e->y>y+6 || fabsf(e->x-bx)>7+e->w/2) continue; e->blind_t=maxf(e->blind_t,2.5f); e->burn=clampi(e->burn+2,0,14); e->burn_t=maxf(e->burn_t,3.0f); enemy_hurt(e,bd,HEX(0xfff0a0),false,false,false); }
        if(R.boss.on && R.boss.enter>=1 && R.boss.dying<=0 && fabsf(R.boss.x-bx)<R.boss.w/2+6 && R.boss.y<y) boss_hurt(bd*0.5f,HEX(0xfff0a0),false);
        beam_add(bx,0,y,HEX(0xffe8a0),5,0.35f); }
      ring(x,y,30,HEX(0xfff0a0),0.35f); flash_light(x,y,90,HEX(0xffe0a0),1.6f,0.35f); sfx_at(SFX_BEAM,x,0.7f); } break;
  }
  g_curio_kill=false;
  hitstop(0.04f); shake(3);
}
static void curio_hit(Curio *c){
  if(c->kind==CU_PIEGE){ curio_trigger(c); return; }
  c->hits--; c->flash=0.12f; c->wob=1;
  spawn_particles(c->x,c->y-CURIO_CY,5,HEX(0xffd84a),70,PT_SPARK,0.25f); spawn_particles(c->x,c->y-CURIO_CY,2,HEX(0xffe8a0),30,PT_GLOW,0.3f);
  sfx_play(SFX_SHIELD,0.35f,(c->x-320)/320.0f*0.6f,1.2f+0.12f*(c->hmax-c->hits));
  if(c->hits<=0) curio_break(c);
}
static void curio_update(float dt){
  float sp=BIOMES[R.biome].speed*(R.sub==RS_PLAY?1:0.3f)*tfac();
  for(int i=0;i<MAX_CURIO;i++){ Curio *c=&R.cur[i]; if(!c->on) continue;
    c->t+=dt; c->y+=sp*dt; if(c->flash>0) c->flash-=dt; if(c->wob>0) c->wob-=dt*3; if(c->y>34) c->lab+=dt;
    if(c->y>ZONE_TOP-8){ spawn_particles(c->x,c->y-6,8,HEX(0x6a6070),30,PT_SMOKE,0.6f); c->on=false; } }
  if(R.sub!=RS_PLAY || R.boss.on || R.boss_spawned || R.mb_on || R.mb_warned || R.ltime>R.lduration-15) return;
  R.curio_t-=dt; if(R.curio_t<=0){ curio_spawn(); R.curio_t= getenv("GOUFFRE_CURIOS") ? 2.5f : frange(18,30)*(1-0.06f*S.bld[BLD_CABINET]); }
}

/* ---------------- v9 : etoiles filantes, big bang ---------------- */
static void mark_enemy(Enemy *e,float k,float t){ if(!e->on) return; if(e->mark_t<=0) spawn_particles(e->x,e->y-e->h*0.3f,4,HEX(0x8a5aff),30,PT_GLOW,0.4f); e->mark_k=maxf(e->mark_k,k); e->mark_t=maxf(e->mark_t,t); }
static void star_call(float x,float y,int excl,float dmg,int kind){
  int cand[64], nc=0;
  for(int i=0;i<MAX_EN && nc<64;i++){ Enemy *e=&R.en[i]; if(!e->on || e->id==excl || e->y<8 || e->y>ZONE_TOP) continue; cand[nc++]=i; }
  float tx=x+frange(-30,30), ty=y+frange(-20,10);
  if(nc){ Enemy *e=&R.en[cand[irand(nc)]]; tx=e->x; ty=e->y; }
  else if(R.boss.on && R.boss.enter>=1){ tx=R.boss.x+frange(-R.boss.w*0.3f,R.boss.w*0.3f); ty=R.boss.y; }
  tx=clampf(tx,PIT_X0+6,PIT_X1-6); ty=clampf(ty,10,ZONE_TOP);
  Zone *z=zone_add(ZN_STAR,tx,ty,kind==2?22:18,0.32f); if(z){ z->dmg=dmg; z->aux=kind; }
}
static void star_burst(float x,float y,float r,float dmg,int kind){
  if(has_ch(CH_ASTRONOME)) dmg*=1.25f;
  uint32_t col= kind==2 ? HEX(0xffb050) : HEX(0xb8c8ff);
  for(int i=0;i<MAX_EN;i++){ Enemy *e=&R.en[i]; if(!e->on) continue;
    float qx=clampf(x,e->x-e->w/2,e->x+e->w/2), qy=clampf(y,e->y-e->h/2,e->y+e->h/2); if(sqrf(qx-x)+sqrf(qy-y)>r*r) continue;
    if(kind==2){ e->burn=clampi(e->burn+2,0,14); e->burn_t=maxf(e->burn_t,3.0f); }
    enemy_hurt(e,dmg*R.s_dmg,col,false,false,false); }
  if(R.boss.on && R.boss.enter>=1 && R.boss.dying<=0){ Boss *b=&R.boss; float qx=clampf(x,b->x-b->w/2,b->x+b->w/2), qy=clampf(y,b->y-b->h/2,b->y+b->h/2); if(sqrf(qx-x)+sqrf(qy-y)<=r*r) boss_hurt(dmg*R.s_dmg*0.8f,col,false); }
  if(kind==1) for(int k=0;k<3;k++){ Ball *t=shard_new(x,y,0); if(t) t->lvl=2; }
  if(kind==2) zone_add(ZN_FIRE,x,y+2,16,2.5f);
  ring(x,y,r,col,0.3f); spawn_particles(x,y,12,col,100,PT_SPARK,0.35f); spawn_particles(x,y,4,HEX(0xffffff),40,PT_GLOW,0.4f);
  flash_light(x,y,r*2.4f,col,1.2f,0.2f); sfx_play(SFX_CRYSTAL,0.3f,(x-320)/320.0f*0.6f,frange(1.3f,1.6f));
}
static void big_bang(float x,float y){
  explode(x,y,74,46,HEX(0xd8b8ff),2,true);
  for(int k=0;k<4;k++) star_call(x,y,-1,24,2);
  ring(x,y,90,HEX(0xffffff),0.5f); shock_add(x,y,140,0.6f,2.0f); shake(7); hitstop(0.06f);
  spawn_particles(x,y,30,HEX(0xe8d8ff),160,PT_SPARK,0.6f); flash_light(x,y,180,HEX(0xc8a8ff),2.2f,0.45f);
  sfx_play(SFX_EXPLODE,0.9f,(x-320)/320.0f*0.6f,0.7f);
}

static int status_count(Enemy *e){ return (e->burn>0)+(e->poison>0)+(e->bleed>0)+(e->slow_t>0||e->freeze_t>0||e->root_t>0||e->stun_t>0)+(e->blind_t>0)+(e->acid>0); }
/* v7 : racines, corrosion, echos */
static void root_enemy(Enemy *e,float t){
  if(!e->on || e->guard) return;
  if((ENEMIES[e->kind].flags&EF_ELITE) || e->mboss) t*=0.5f;
  if(e->root_t<=0){ spawn_particles(e->x,e->y+e->h/2,5,HEX(0x8ac850),40,PT_PIX,0.35f); spawn_particles(e->x,e->y+e->h/2,3,HEX(0x8a5a2a),30,PT_SQ,0.4f); if(R.hit_sfx_t<=0) sfx_at(SFX_ROOT,e->x,0.22f); }
  e->root_t=maxf(e->root_t,t);
}
static void acid_add(Enemy *e,int n,int mx){
  if(!e->on) return;
  int before=e->acid;
  e->acid=maxi(e->acid,mini(e->acid+n,mx)); e->acid_t=5.0f+stat_dur();
  if(before<3 && e->acid>=3){ spawn_particles(e->x,e->y,6,HEX(0xd8ff40),50,PT_GLOW,0.4f); if(R.hit_sfx_t<=0) sfx_at(SFX_ACID,e->x,0.25f); }
}
static void boss_acid(int n,int mx){ Boss *b=&R.boss; if(!b->on) return; b->acid=maxi(b->acid,mini(b->acid+n,mx)); b->acid_t=5.0f+stat_dur(); }
static void echo_add(float x,float y,float r,float dmg,int aux){
  Zone *z=zone_add(ZN_ECHO,x,y,r,aux?0.5f:0.4f); if(z){ z->dmg=dmg; z->aux=aux; }
}
static void echo_burst(float x,float y,float r,float dmg,int aux){
  uint32_t col = aux ? HEX(0xd8d0ff) : HEX(0xa898ff);
  for(int i=0;i<MAX_EN;i++){
    Enemy *e=&R.en[i]; if(!e->on) continue;
    float qx=clampf(x,e->x-e->w/2,e->x+e->w/2), qy=clampf(y,e->y-e->h/2,e->y+e->h/2);
    if(sqrf(qx-x)+sqrf(qy-y)>r*r) continue;
    if(aux && !e->guard && chance(0.5f)) e->stun_t=maxf(e->stun_t,0.8f);
    enemy_hurt(e,dmg*R.s_dmg,col,false,false,false);
  }
  if(R.boss.on && R.boss.enter>=1 && R.boss.dying<=0){
    Boss *b=&R.boss; float qx=clampf(x,b->x-b->w/2,b->x+b->w/2), qy=clampf(y,b->y-b->h/2,b->y+b->h/2);
    if(sqrf(qx-x)+sqrf(qy-y)<=r*r) boss_hurt(dmg*R.s_dmg*0.8f,col,false);
  }
  ring(x,y,r,col,0.32f); ring(x,y,r*0.62f,col,0.22f);
  spawn_particles(x,y,aux?14:7,col,aux?110:70,PT_SPARK,0.3f);
  shock_add(x,y,r*1.3f,0.3f,aux?1.3f:0.7f);
  flash_light(x,y,r*1.8f,col,aux?1.5f:0.8f,aux?0.25f:0.14f);
  if(aux){ chain_lightning(x,y,-1,4,14,0); shake(2.5f); sfx_play(SFX_THUNDER,0.45f,(x-320)/320.0f*0.6f,frange(0.9f,1.1f)); }
  else sfx_play(SFX_ECHO,0.22f,(x-320)/320.0f*0.6f,frange(0.92f,1.12f));
}
static void acid_splash(float x,float y,float r,float dmg,int n){
  for(int i=0;i<MAX_EN;i++){
    Enemy *e=&R.en[i]; if(!e->on) continue;
    float qx=clampf(x,e->x-e->w/2,e->x+e->w/2), qy=clampf(y,e->y-e->h/2,e->y+e->h/2);
    if(sqrf(qx-x)+sqrf(qy-y)>r*r) continue;
    acid_add(e,n,10); enemy_hurt(e,dmg*R.s_dmg,HEX(0xe8ff70),false,false,false);
  }
  if(R.boss.on && R.boss.enter>=1 && R.boss.dying<=0){
    Boss *b=&R.boss; float qx=clampf(x,b->x-b->w/2,b->x+b->w/2), qy=clampf(y,b->y-b->h/2,b->y+b->h/2);
    if(sqrf(qx-x)+sqrf(qy-y)<=r*r){ boss_acid(n,10); boss_hurt(dmg*R.s_dmg*0.8f,HEX(0xe8ff70),false); }
  }
  ring(x,y,r,HEX(0xd8ff40),0.3f);
  if(!g_norender) for(int k=0;k<14;k++){ Part *p=part_new(); float a=frange(-PI,0); float s=frange(40,120); p->kind=PT_PIX; p->x=x; p->y=y; p->vx=cosf(a)*s; p->vy=sinf(a)*s; p->grav=260; p->drag=1; p->c=k&1?HEX(0xe8ff70):HEX(0x9ad820); p->size=k%3==0?2:1; p->life=p->max=frange(0.35f,0.6f); }
  flash_light(x,y,r*2,HEX(0xc8f040),1.0f,0.2f);
  sfx_play(SFX_ACID,0.3f,(x-320)/320.0f*0.6f,frange(0.9f,1.1f));
}
/* v10 : secousses (bille sismique, faille, Geomancien) et chaines */
static void quake_hit(Enemy *e,float dmg,float stun){
  if(!e->on) return;
  if(stun>0 && !e->guard && chance(stun)) e->stun_t=maxf(e->stun_t,(e->mboss||(ENEMIES[e->kind].flags&EF_ELITE))?0.35f:0.8f);
  g_quake=true; enemy_hurt(e,dmg*R.s_dmg,HEX(0xffc070),false,false,false); g_quake=false;
  if(!g_norender) spawn_particles(e->x,e->y+e->h/2,3,HEX(0xb08a5a),40,PT_SQ,0.4f);
}
/* onde le long du mur le plus proche */
static void quake_wall(float x,float y,float dmg,float stun,float hr){
  float wx = x<(PIT_X0+PIT_X1)*0.5f ? PIT_X0 : PIT_X1;
  for(int i=0;i<MAX_EN;i++){ Enemy *e=&R.en[i]; if(!e->on) continue; if(fabsf(e->x-wx)<44 && fabsf(e->y-y)<hr+30) quake_hit(e,dmg,stun); }
  if(R.boss.on && R.boss.enter>=1 && fabsf(R.boss.x-wx)<R.boss.w*0.5f+44 && fabsf(R.boss.y-y)<R.boss.h*0.5f+30) boss_hurt(dmg*R.s_dmg*0.8f,HEX(0xffc070),false);
  if(!g_norender){ for(int k=0;k<6;k++) spawn_particles(wx+(wx<320?6:-6),y+frange(-26,26),2,HEX(0xc8a070),50,PT_SQ,0.4f); }
  shock_add(wx,y,40,0.3f,0.8f); if(R.hit_sfx_t<=0) sfx_at(SFX_BOUNCE,wx,0.35f);
}
/* faille : toute la largeur du puits */
static void quake_row(float y,float dmg,float stun){
  for(int i=0;i<MAX_EN;i++){ Enemy *e=&R.en[i]; if(!e->on) continue; if(fabsf(e->y-y)<e->h*0.5f+12) quake_hit(e,dmg,stun); }
  if(R.boss.on && R.boss.enter>=1 && fabsf(R.boss.y-y)<R.boss.h*0.5f+12) boss_hurt(dmg*R.s_dmg*0.8f,HEX(0xffc070),false);
  if(!g_norender) for(int k=0;k<18;k++){ float x=PIT_X0+(k+0.5f)*PIT_W/18.0f; spawn_particles(x,y+frange(-2,2),1,k&1?HEX(0xffd080):HEX(0x8a6a4a),45,k&1?PT_SPARK:PT_SQ,0.35f); }
  flash_light((PIT_X0+PIT_X1)*0.5f,y,PIT_W*0.6f,HEX(0xffb050),0.9f,0.15f); shake(2.0f);
  for(int k=0;k<3;k++) shock_add(PIT_X0+(k+0.5f)*PIT_W/3.0f,y,50,0.3f,0.7f);
}
/* chaine : relie la cible a ses voisins (degats partages) */
static void link_enemy(Enemy *e,float k,int n,bool pull){
  for(int j=0;j<n;j++){
    Enemy *best=NULL; float bd=72*72;
    for(int i=0;i<MAX_EN;i++){ Enemy *o=&R.en[i]; if(!o->on || o==e || o->guard) continue;
      if(o->link_t>0 && o->link_id==e->id) continue;
      float d=sqrf(o->x-e->x)+sqrf(o->y-e->y); if(d<bd){ bd=d; best=o; } }
    if(!best) break;
    if(j==0){ e->link_id=best->id; e->link_t=4.0f; e->link_k=k; }
    best->link_id=e->id; best->link_t=4.0f; best->link_k=k;
    if(pull && !best->mboss){ float dx=e->x-best->x, dy=e->y-best->y, dd=sqrtf(dx*dx+dy*dy); if(dd>8){ best->x+=dx/dd*minf(10,dd-8); best->y+=dy/dd*minf(6,dd-8); } best->slow_t=maxf(best->slow_t,2.0f); }
  }
}
static float ball_hit_damage(Ball *b,bool *crit,Enemy *e){
  float base = b->kind==BK_SHARD ? BALLS[BK_SHARD].dmg : (b->kind==BK_BABY||b->kind==BK_GHOST ? BALLS[b->kind].dmg : bdmg(b->kind,b->lvl));
  float d=base*R.s_dmg*frange(0.9f,1.1f);
  if(b->kind==BK_FOLLE) d*=1+(0.09f+0.03f*b->lvl)*mini(b->bounces,15);
  if(b->kind==BK_KALEIDO) d*=1+0.15f*mini(b->bounces,20);
  if(b->kind==BK_RICOCHET) d*=1+(0.12f+0.04f*b->lvl)*mini(b->wall,10);
  if(b->kind==BK_CARAMBOLE) d*=1+0.18f*mini(b->bounces,25);
  if(e && b->kind==BK_CRISTAL) d*=1+(0.20f+0.05f*b->lvl)*status_count(e);
  if(e && b->kind==BK_GEODE) d*=1+0.4f*status_count(e);
  if(b->kind==BK_DOREE) d*=1+minf((30+15*b->lvl)*0.01f,(R.gold/20)*0.01f);
  if(b->kind==BK_MIDAS) d*=1+minf(1.0f,(R.gold/10)*0.01f);
  if(has_ch(CH_MARCHAND)) d*=1+minf(0.40f,(R.gold/25)*0.01f);
  if(R.s_rage>0) d*=1+R.s_rage*floorf((1-clampf(R.hp/R.hpmax,0,1))*10);
  if(R.s_giant>0 && (!e || (ENEMIES[e->kind].flags&EF_ELITE) || e->champ || e->mboss)) d*=1+R.s_giant;   /* loupe (e==NULL : gardien) */
  if(R.s_anchor>0 && e && e->y>(PIT_Y0+FLOOR_Y)*0.5f) d*=1+R.s_anchor;                                    /* ancre */
  d*=1+0.04f*ball_stars(b->kind);
  if(e){ int bt=bestiary_tier(e->kind); if(bt>=2) d*=1+0.05f*(bt-1); }
  else if(R.boss.on){ int bt=boss_tier(R.boss.type); if(bt>=2) d*=1+0.05f*(bt-1); }
  if(S.bld[BLD_OBSERV]>0 && (!e || e->mboss)) d*=1+0.05f*S.bld[BLD_OBSERV];
  if(b->boom>0) d*= b->kind==BK_BOOMERANG ? 1.2f+0.1f*b->lvl : 1.3f;   /* v10 : le retour du boomerang frappe plus fort */
  if(IS_EVO(b->kind)) d*=1+R.s_grim;                                     /* grimoire */
  if(R.hunt>0) d*=1+0.02f*R.hunt;                                        /* Chasseresse */
  if(IS_LEG(b->kind)) d*=1+0.10f*maxi(0,S.bld[BLD_CREUSET]-1);                   /* creuset legendaire */      /* observatoire : gardiens et gardiens mineurs */
  *crit = b->kind==BK_ENCLUME || chance(R.s_crit);
  if(*crit) d*=R.s_critm;
  return d;
}
/* impact d'une bille sur un ennemi */
static void ball_on_enemy(Ball *b,Enemy *e,bool from_below){
  bool crit; float d=ball_hit_damage(b,&crit,e);
  d+=2.0f*e->bleed;
  e->last_ball=b->kind;
  int L=b->lvl; int sd=stat_dur();
  uint32_t col=HEX(0xffffff);
  switch(b->kind){
    case BK_FEU: e->burn=clampi(e->burn+(L>=3?2:1),0,4+2*L); e->burn_t=3.0f+sd; col=HEX(0xffb050); break;
    case BK_GLACE: e->slow_t=2.0f; if(chance(0.08f+0.07f*L)){ e->freeze_t=1.2f; sfx_at(SFX_FREEZE,e->x,0.3f); } col=HEX(0x9ae8ff); break;
    case BK_VENIN: e->poison+= L>=2?2:1; e->poison_t=6.0f+sd; col=HEX(0x9af05a); break;
    case BK_EPINE: e->bleed=clampi(e->bleed+(L>=3?2:1),0,4+2*L); e->bleed_t=5.0f+sd; col=HEX(0xff6a7a); break;
    case BK_PLOMB: e->push=-(25.0f+15.0f*L)*3; break;
    case BK_ENCLUME: e->push=-180; e->freeze_t=maxf(e->freeze_t,0.6f); break;
    case BK_FLEAU: e->poison+=3; e->poison_t=6.0f+sd; e->bleed=clampi(e->bleed+2,0,12); e->bleed_t=5.0f+sd; col=HEX(0xc8ff5a); break;
    case BK_FAUCHEUSE: if(!e->guard && e->hp-d < e->hpmax*0.18f && !(ENEMIES[e->kind].flags&EF_ELITE)) d=e->hp+1; col=HEX(0xff8ab0); break;
    case BK_LUMIERE: case BK_RAYON: e->blind_t=maxf(e->blind_t,2.0f+L); col=HEX(0xfff4c0); break;
    case BK_AURORE: e->blind_t=maxf(e->blind_t,3.0f); col=HEX(0xffb8e8); break;
    case BK_GEODE: e->poison+=3; e->poison_t=6.0f+sd; col=HEX(0xd090ff); break;
    case BK_CRISTAL: col=HEX(0x9af8ff); break;
    case BK_FOLLE: case BK_KALEIDO: col=HEX(0xff9ae0); break;
    case BK_RACINE: if(chance(0.15f+0.10f*L)) root_enemy(e,1.2f+0.3f*L); col=HEX(0xd8b070); break;
    case BK_RONCES: root_enemy(e,1.6f); e->bleed=clampi(e->bleed+2,0,12); e->bleed_t=5.0f+sd; col=HEX(0xa8d060); break;
    case BK_ACIDE: acid_add(e,L>=3?2:1,4+2*L); col=HEX(0xd8ff40); break;
    case BK_OMBRE: mark_enemy(e,0.15f+0.05f*L,4.0f+sd); col=HEX(0xb08aff); break;
    case BK_ECLIPSE: mark_enemy(e,0.40f,4.0f+sd); e->blind_t=maxf(e->blind_t,2.5f); col=HEX(0xfff0c0); break;
    case BK_CAUCHEMAR: mark_enemy(e,0.30f,4.0f+sd); e->slow_t=maxf(e->slow_t,1.5f); if(!e->guard && !e->mboss) e->push=-90; col=HEX(0xc08aff); break;
    case BK_LAVE: case BK_VOLCAN: case BK_SUPERNOVA: e->burn=clampi(e->burn+1,0,maxi(6,e->burn)); e->burn_t=maxf(e->burn_t,3.0f+sd); col=HEX(0xff9a4a); break;
    case BK_ZERO: if(!e->guard) e->freeze_t=maxf(e->freeze_t,(e->mboss||(ENEMIES[e->kind].flags&EF_ELITE))?0.8f:2.0f); col=HEX(0xd8f4ff); break;
    case BK_OUROBOROS: e->poison+=4; e->poison_t=6.0f+sd; e->bleed=clampi(e->bleed+3,0,14); e->bleed_t=5.0f+sd;
      if(!e->guard && !e->mboss && e->hp-d < e->hpmax*0.25f && !(ENEMIES[e->kind].flags&EF_ELITE)) d=e->hp+1; col=HEX(0x9af06a); break;
    case BK_JUGEMENT: e->blind_t=maxf(e->blind_t,2.0f); col=HEX(0xfff8d0); break;
    case BK_ETOILE: case BK_CONSTELLATION: col=HEX(0xc8d8ff); break;
    case BK_RICOCHET: case BK_CARAMBOLE: col=HEX(0xffb070); break;
    case BK_BOOMERANG: col=HEX(0xf0c080); break;
    case BK_CHAKRAM: e->bleed=clampi(e->bleed+2,0,12); e->bleed_t=5.0f+sd; col=HEX(0xffd090); break;
    case BK_SEISME: case BK_FAILLE: case BK_APOCALYPSE: col=HEX(0xffc070); break;
    case BK_CHAINE: link_enemy(e,0.35f+0.10f*L,1,false); col=HEX(0xc8d0e8); break;
    case BK_ENTRAVE: link_enemy(e,0.60f,3,true); col=HEX(0xa8c8ff); break;
    case BK_PLUIE: acid_add(e,2,10); col=HEX(0xe8ff70); break;
    case BK_DOREE: case BK_MIDAS: col=HEX(0xffd860); break;
    case BK_ECHO: case BK_TONNERRE: col=HEX(0xc8c0ff); break;
    default: break;
  }
  if(has_ch(CH_PYROMANE) && b->kind!=BK_FEU){ e->burn=clampi(e->burn+1,0,maxi(6,e->burn)); e->burn_t=maxf(e->burn_t,3.0f+sd); }
  if(has_ch(CH_DRUIDE) && b->kind!=BK_BABY && chance(ch_rank(CH_DRUIDE)>=5?0.30f:0.20f)) root_enemy(e,1.5f);
  if(crit && has_ch(CH_VOLEUSE)) mark_enemy(e,ch_rank(CH_VOLEUSE)>=5?0.40f:0.25f,4.0f);   /* v9 : coup de l'ombre */
  int eid=e->id; float ex=e->x, ey=e->y;
  enemy_hurt(e,d,col,from_below,crit,true);
  b->hits++;
  switch(b->kind){
    case BK_FOUDRE: chain_lightning(ex,ey,eid,1+L,5.0f+3*L,0); break;
    case BK_TEMPETE: chain_lightning(ex,ey,eid,5,22,0.35f); break;
    case BK_BOMBE: explode(b->x,b->y,16.0f+4*L,6.0f+4*L,HEX(0xff8a3a),0,false); break;
    case BK_METEORE: explode(b->x,b->y,38,30,HEX(0xffa030),3,true); break;
    case BK_SANG: if(chance(0.12f+0.08f*L)){ heal(1); spawn_particles(b->x,b->y,4,HEX(0xe84a7a),40,PT_GLOW,0.4f); } break;
    case BK_AURORE: if(chance(0.25f)){ heal(1); spawn_particles(b->x,b->y,4,HEX(0xffb8e8),40,PT_GLOW,0.4f); } break;
    case BK_ECLAT: if(b->cd<=0){ for(int i=0;i<L;i++) shard_new(b->x,b->y,0); b->cd=0.25f; sfx_at(SFX_SPLIT,b->x,0.3f); } break;
    case BK_PRISME: if(b->cd<=0){ for(int i=0;i<3;i++) shard_new(b->x,b->y,1); b->cd=0.25f; sfx_at(SFX_SPLIT,b->x,0.35f); } break;
    case BK_KALEIDO: if(b->cd<=0){ shard_new(b->x,b->y,0); b->cd=0.12f; } break;
    case BK_VAPEUR: if(b->cd<=0){ zone_add(ZN_STEAM,b->x,b->y,26,2.5f); b->cd=0.3f; } break;
    case BK_RAFALE: if(b->cd<=0){ gust(b->x,b->y,22.0f+4*L,3.0f+2*L,0); b->cd=0.15f; } break;
    case BK_BLIZZARD: if(b->cd<=0){ gust(b->x,b->y,40,10,1); b->cd=0.15f; } break;
    case BK_TEMPFEU: if(b->cd<=0){ gust(b->x,b->y,36,9,2); b->cd=0.2f; } break;
    case BK_CRISTAL: case BK_GEODE: if(R.hit_sfx_t<=0) sfx_at(SFX_CRYSTAL,ex,0.18f); break;
    case BK_SINGULARITE:
      for(int i=0;i<MAX_EN;i++){ Enemy *o=&R.en[i]; if(!o->on || o->guard) continue; float dx=b->x-o->x, dy=b->y-o->y; float dd=sqrtf(dx*dx+dy*dy); if(dd<44 && dd>1){ o->x+=dx/dd*minf(8,dd*0.3f); o->y+=dy/dd*minf(8,dd*0.3f); enemy_hurt(o,18*R.s_dmg,HEX(0x8a5aff),false,false,false); } }
      ring(b->x,b->y,40,HEX(0x8a5aff),0.35f); spawn_particles(b->x,b->y,10,HEX(0xc08aff),60,PT_GLOW,0.4f); shock_add(b->x,b->y,40,0.3f,0.8f);
      break;
    case BK_SHARD: if(b->lvl==2) chain_lightning(ex,ey,eid,2,10,0); else if(b->lvl==3 && e->on){ e->bleed=clampi(e->bleed+1,0,8); e->bleed_t=maxf(e->bleed_t,4.0f); } break;
    case BK_ECHO: if(b->cd<=0){ echo_add(b->x,b->y,20.0f+4*L,4.0f+3*L,0); b->cd=0.18f; } break;
    case BK_TONNERRE: chain_lightning(ex,ey,eid,3,12,0); if(b->cd<=0){ echo_add(b->x,b->y,38,26,1); b->cd=0.24f; } break;
    case BK_DOREE: if(chance(0.06f+0.03f*L)){ drop_pickup(PK_COIN,ex,ey,1); spawn_particles(ex,ey,4,HEX(0xffd84a),50,PT_SPARK,0.3f); } break;
    case BK_MIDAS: if(chance(0.15f)){ drop_pickup(PK_COIN,ex,ey,1); spawn_particles(ex,ey,5,HEX(0xffe070),60,PT_SPARK,0.3f); } break;
    case BK_RONCES: if(b->cd<=0){ Zone *z=zone_add(ZN_RONCES,b->x,b->y+4,22,3.0f); if(z) z->dmg=4; b->cd=0.35f; } break;
    case BK_PLUIE: acid_splash(b->x,b->y,34,12,2); if(b->cd<=0){ Zone *z=zone_add(ZN_ACIDE,b->x,b->y+4,20,2.5f); if(z) z->dmg=3; b->cd=0.3f; } break;
    case BK_LAVE: if(b->cd<=0){ zone_add(ZN_FIRE,b->x,b->y+3,12.0f+3*L,2.0f+L); b->cd=0.35f; } break;
    case BK_VOLCAN: if(b->cd<=0){ explode(b->x,b->y,34,22,HEX(0xff8a2a),2,true); zone_add(ZN_FIRE,b->x,b->y+3,24,3.5f); b->cd=0.3f; } break;
    case BK_ETOILE: if(chance(0.20f+0.08f*L)) star_call(ex,ey,eid,10.0f+6*L,0); break;
    case BK_CONSTELLATION: if(b->cd2<=0){ star_call(ex,ey,eid,22,1); b->cd2=0.15f; } break;
    case BK_SUPERNOVA: if(b->cd2<=0 && chance(0.35f)){ for(int k=0;k<3;k++) star_call(ex,ey,eid,24,2); b->cd2=0.4f; } break;
    case BK_BIGBANG:
      for(int i=0;i<MAX_EN;i++){ Enemy *o=&R.en[i]; if(!o->on || o->guard) continue; float dx=b->x-o->x, dy=b->y-o->y; float dd=sqrtf(dx*dx+dy*dy); if(dd<52 && dd>1){ o->x+=dx/dd*minf(9,dd*0.3f); o->y+=dy/dd*minf(9,dd*0.3f); enemy_hurt(o,24*R.s_dmg,HEX(0xb08aff),false,false,false); } }
      ring(b->x,b->y,48,HEX(0xb08aff),0.3f); if(b->cd2<=0 && chance(0.25f)){ big_bang(b->x,b->y); b->cd2=1.0f; } break;
    case BK_JUGEMENT: chain_lightning(ex,ey,eid,6,20,0.2f); if(b->cd<=0){ holy_beam(b->x,b->y+12); b->cd=0.22f; } break;
    case BK_ZERO: if(b->cd<=0){ gust(b->x,b->y,42,12,1); b->cd=0.2f; } break;
    case BK_SEISME: if(b->cd<=0){ quake_wall(b->x,b->y,3.0f+2*L,0.10f+0.05f*L,18); b->cd=0.2f; } break;
    case BK_FAILLE: if(b->cd<=0){ quake_row(b->y,16,0.8f); b->cd=0.25f; } break;
    case BK_APOCALYPSE: if(b->cd<=0){ explode(b->x,b->y,34,24,HEX(0xff7a2a),2,true); zone_add(ZN_FIRE,b->x,b->y+3,22,3.5f); quake_row(b->y,22,0.8f); b->cd=0.3f; } break;
    default: break;
  }
  if(R.hit_sfx_t<=0){ sfx_play(SFX_HIT,0.32f,(ex-320)/320.0f*0.6f,frange(0.85f,1.2f)); R.hit_sfx_t=0.035f; }
  spawn_particles(b->x,b->y,crit?7:3,crit?HEX(0xffe080):BALLS[b->kind].col,crit?90:60,crit?PT_SPARK:PT_PIX,0.3f);
  if(crit) flash_light(b->x,b->y,20,HEX(0xffe0a0),0.8f,0.08f);
}
static void ball_on_boss(Ball *b){
  bool crit; float d=ball_hit_damage(b,&crit,NULL);
  boss_hurt(d,HEX(0xffffff),crit);
  b->hits++;
  switch(b->kind){
    case BK_FOUDRE: chain_lightning(b->x,b->y,-1,1+b->lvl,5.0f+3*b->lvl,0); break;
    case BK_TEMPETE: chain_lightning(b->x,b->y,-1,5,22,0.35f); break;
    case BK_BOMBE: explode(b->x,b->y,16.0f+4*b->lvl,6.0f+4*b->lvl,HEX(0xff8a3a),0,false); break;
    case BK_METEORE: explode(b->x,b->y,38,30,HEX(0xffa030),3,true); break;
    case BK_SANG: if(chance(0.12f+0.08f*b->lvl)) heal(1); break;
    case BK_AURORE: if(chance(0.25f)) heal(1); break;
    case BK_ECLAT: if(b->cd<=0){ for(int i=0;i<b->lvl;i++) shard_new(b->x,b->y,0); b->cd=0.25f; } break;
    case BK_PRISME: if(b->cd<=0){ for(int i=0;i<3;i++) shard_new(b->x,b->y,1); b->cd=0.25f; } break;
    case BK_KALEIDO: if(b->cd<=0){ shard_new(b->x,b->y,0); b->cd=0.12f; } break;
    case BK_VAPEUR: if(b->cd<=0){ zone_add(ZN_STEAM,b->x,b->y,26,2.5f); b->cd=0.3f; } break;
    case BK_RAFALE: if(b->cd<=0){ gust(b->x,b->y,22.0f+4*b->lvl,3.0f+2*b->lvl,0); b->cd=0.15f; } break;
    case BK_BLIZZARD: if(b->cd<=0){ gust(b->x,b->y,40,10,1); b->cd=0.15f; } break;
    case BK_TEMPFEU: if(b->cd<=0){ gust(b->x,b->y,36,9,2); b->cd=0.2f; } break;
    case BK_ECHO: if(b->cd<=0){ echo_add(b->x,b->y,20.0f+4*b->lvl,4.0f+3*b->lvl,0); b->cd=0.18f; } break;
    case BK_TONNERRE: chain_lightning(b->x,b->y,-1,3,12,0); if(b->cd<=0){ echo_add(b->x,b->y,38,26,1); b->cd=0.24f; } break;
    case BK_ACIDE: boss_acid(b->lvl>=3?2:1,4+2*b->lvl); break;
    case BK_PLUIE: if(b->cd<=0){ acid_splash(b->x,b->y,34,12,2); Zone *z=zone_add(ZN_ACIDE,b->x,b->y+4,20,2.5f); if(z) z->dmg=3; b->cd=0.3f; } else boss_acid(1,10); break;
    case BK_DOREE: if(chance(0.03f)) drop_pickup(PK_COIN,b->x,b->y,1); break;
    case BK_MIDAS: if(chance(0.07f)) drop_pickup(PK_COIN,b->x,b->y,1); break;
    case BK_RONCES: if(b->cd<=0){ Zone *z=zone_add(ZN_RONCES,b->x,b->y+4,22,3.0f); if(z) z->dmg=4; b->cd=0.35f; } break;
    case BK_OMBRE: { Boss *bo=&R.boss; bo->mark_k=maxf(bo->mark_k,0.15f+0.05f*b->lvl); bo->mark_t=4.0f; } break;
    case BK_ECLIPSE: case BK_CAUCHEMAR: { Boss *bo=&R.boss; bo->mark_k=maxf(bo->mark_k,b->kind==BK_ECLIPSE?0.40f:0.30f); bo->mark_t=4.0f; } break;
    case BK_LAVE: if(b->cd<=0){ zone_add(ZN_FIRE,b->x,b->y+3,12.0f+3*b->lvl,2.0f+b->lvl); b->cd=0.35f; } break;
    case BK_VOLCAN: if(b->cd<=0){ explode(b->x,b->y,34,22,HEX(0xff8a2a),2,true); zone_add(ZN_FIRE,b->x,b->y+3,24,3.5f); b->cd=0.3f; } break;
    case BK_ETOILE: if(chance(0.20f+0.08f*b->lvl)) star_call(b->x,b->y,-1,10.0f+6*b->lvl,0); break;
    case BK_CONSTELLATION: if(b->cd2<=0){ star_call(b->x,b->y,-1,22,1); b->cd2=0.15f; } break;
    case BK_SUPERNOVA: if(b->cd2<=0 && chance(0.35f)){ for(int k=0;k<3;k++) star_call(b->x,b->y,-1,24,2); b->cd2=0.4f; } break;
    case BK_BIGBANG: if(b->cd2<=0 && chance(0.25f)){ big_bang(b->x,b->y); b->cd2=1.0f; } break;
    case BK_JUGEMENT: chain_lightning(b->x,b->y,-1,6,20,0.2f); if(b->cd<=0){ holy_beam(b->x,b->y+12); b->cd=0.22f; } break;
    case BK_ZERO: if(b->cd<=0){ gust(b->x,b->y,42,12,1); b->cd=0.2f; } break;
    case BK_SEISME: if(b->cd<=0){ quake_wall(b->x,b->y,3.0f+2*b->lvl,0,18); b->cd=0.2f; } break;
    case BK_FAILLE: if(b->cd<=0){ quake_row(b->y,16,0.8f); b->cd=0.25f; } break;
    case BK_APOCALYPSE: if(b->cd<=0){ explode(b->x,b->y,34,24,HEX(0xff7a2a),2,true); zone_add(ZN_FIRE,b->x,b->y+3,22,3.5f); quake_row(b->y,22,0.8f); b->cd=0.3f; } break;
    default: break;
  }
  if(R.hit_sfx_t<=0){ sfx_play(SFX_HIT,0.35f,(b->x-320)/320.0f*0.6f,frange(0.7f,0.9f)); R.hit_sfx_t=0.035f; }
  spawn_particles(b->x,b->y,crit?7:3,crit?HEX(0xffe080):BALLS[b->kind].col,60,crit?PT_SPARK:PT_PIX,0.3f);
}

/* collision cercle / boite ; renvoie la normale */
static bool hit_box(float x,float y,float r,float ex,float ey,float hw,float hh,float *nx,float *ny,float *px,float *py){
  float qx=clampf(x,ex-hw,ex+hw), qy=clampf(y,ey-hh,ey+hh);
  float dx=x-qx, dy=y-qy, d2=dx*dx+dy*dy;
  if(d2>r*r) return false;
  if(d2>1e-6f){ float d=sqrtf(d2); *nx=dx/d; *ny=dy/d; *px=qx+*nx*(r+0.3f); *py=qy+*ny*(r+0.3f); }
  else {
    float l=x-(ex-hw), rr=(ex+hw)-x, t=y-(ey-hh), bb=(ey+hh)-y; float m=l; *nx=-1; *ny=0;
    if(rr<m){ m=rr; *nx=1; *ny=0; } if(t<m){ m=t; *nx=0; *ny=-1; } if(bb<m){ *nx=0; *ny=1; }
    *px = *nx<0 ? ex-hw-r-0.3f : (*nx>0 ? ex+hw+r+0.3f : x);
    *py = *ny<0 ? ey-hh-r-0.3f : (*ny>0 ? ey+hh+r+0.3f : y);
  }
  return true;
}
static bool recently_hit(Ball *b,int id){ for(int i=0;i<4;i++) if(b->hit_id[i]==id && b->hit_t[i]>0) return true; return false; }
static void mark_hit(Ball *b,int id,float t){
  int k=0; float mn=1e9f; for(int i=0;i<4;i++){ if(b->hit_t[i]<mn){ mn=b->hit_t[i]; k=i; } }
  b->hit_id[k]=id; b->hit_t[k]=t;
}
static void reflect(Ball *b,float nx,float ny){
  float dot=b->vx*nx+b->vy*ny;
  if(dot<0){ b->vx-=2*dot*nx; b->vy-=2*dot*ny; }
  float a=frange(-0.05f,0.05f), ca=cosf(a), sa=sinf(a);
  float vx=b->vx*ca-b->vy*sa, vy=b->vx*sa+b->vy*ca; b->vx=vx; b->vy=vy;
}
static void fix_velocity(Ball *b){
  float sp=sqrtf(b->vx*b->vx+b->vy*b->vy); if(sp<1){ b->vx=0; b->vy=-b->speed; return; }
  float vx=b->vx/sp, vy=b->vy/sp;
  if(fabsf(vy)<0.24f){ vy = (vy<0?-0.24f:0.24f); vx=signf(vx)*sqrtf(1-vy*vy); }
  b->vx=vx*b->speed; b->vy=vy*b->speed;
}
static void crazy(Ball *b){
  b->bounces++;
  if(b->kind==BK_FOLLE || b->kind==BK_KALEIDO){ float a=frange(-0.3f,0.3f), ca=cosf(a), sa=sinf(a); float vx=b->vx*ca-b->vy*sa, vy=b->vx*sa+b->vy*ca; b->vx=vx; b->vy=vy; }
  if(b->kind==BK_KALEIDO && b->cd<=0){ shard_new(b->x,b->y,0); b->cd=0.12f; }
  if(b->kind==BK_CARAMBOLE){ float a=frange(-0.32f,0.32f), ca=cosf(a), sa=sinf(a); float vx=b->vx*ca-b->vy*sa, vy=b->vx*sa+b->vy*ca; b->vx=vx; b->vy=vy;
    if(!b->temp && b->bounces==6){ Ball *c=temp_ball(BK_CARAMBOLE,1,b->x,b->y,atan2f(b->vy,b->vx)+frange(0.5f,0.9f)*(chance(0.5f)?1:-1),b->speed,2.4f);
      if(c){ c->bounces=6; spawn_particles(b->x,b->y,8,HEX(0xff8a7a),70,PT_SPARK,0.3f); sfx_at(SFX_SPLIT,b->x,0.35f); } } }
}
static void wall_quake(Ball *b){   /* v10 : bille sismique et faille */
  if(b->kind==BK_SEISME && b->cd2<=0){ quake_wall(b->x,b->y,6.0f+4*b->lvl,0.15f+0.05f*b->lvl,24); b->cd2=0.15f; }
  else if(b->kind==BK_FAILLE && b->cd2<=0){ quake_row(b->y,18,0.8f); b->cd2=0.2f; }
}
static Enemy *nearest_enemy(float x,float y,float maxd){
  Enemy *best=NULL; float bd=maxd*maxd;
  for(int i=0;i<MAX_EN;i++){ Enemy *e=&R.en[i]; if(!e->on || e->y<0) continue; float d=sqrf(e->x-x)+sqrf(e->y-y); if(d<bd){ bd=d; best=e; } }
  return best;
}

static void ball_update(Ball *b,float dt){
  for(int i=0;i<4;i++) if(b->hit_t[i]>0) b->hit_t[i]-=dt;
  if(b->cd>0) b->cd-=dt; if(b->cd2>0) b->cd2-=dt;
  if(b->state==BS_READY){ b->x=R.px; b->y=R.py-8; return; }
  if(b->state==BS_RETURN){
    float dx=R.px-b->x, dy=(R.py-8)-b->y, d=sqrtf(dx*dx+dy*dy);
    float sp=560*dt; if(d<=sp+6){ b->state=BS_READY; b->x=R.px; b->y=R.py-8;
      if(has_ch(CH_CHASSERESSE) && !b->temp){ int mx=ch_rank(CH_CHASSERESSE)>=5?15:10; R.hunt=mini(mx,R.hunt+1); R.hunt_t=6.0f; }   /* v10 : retour gagnant */
      return; }
    b->x+=dx/d*sp; b->y+=dy/d*sp;
    for(int i=TRAIL_N-1;i>0;i--){ b->tx[i]=b->tx[i-1]; b->ty[i]=b->ty[i-1]; } b->tx[0]=b->x; b->ty[0]=b->y;
    return;
  }
  if(b->temp){ b->life-=dt; if(b->life<=0){ spawn_particles(b->x,b->y,3,BALLS[b->kind].col,30,PT_GLOW,0.3f); b->on=false; return; } }
  /* traine */
  b->trail_t-=dt; if(b->trail_t<=0){ for(int i=TRAIL_N-1;i>0;i--){ b->tx[i]=b->tx[i-1]; b->ty[i]=b->ty[i-1]; } b->tx[0]=b->x; b->ty[0]=b->y; b->trail_t=0.0125f; }
  /* tete chercheuse */
  if(b->kind==BK_AIMANT || b->kind==BK_SINGULARITE || b->kind==BK_GHOST){
    float tx=0,ty=0; bool has=false;
    Enemy *e=nearest_enemy(b->x,b->y,150); if(e){ tx=e->x; ty=e->y; has=true; }
    if(R.boss.on && R.boss.enter>=1){ float db=sqrf(R.boss.x-b->x)+sqrf(R.boss.y-b->y); if(!has || db<sqrf(e->x-b->x)+sqrf(e->y-b->y)){ tx=R.boss.x; ty=R.boss.y; has=true; } }
    if(has){
      float turn=(b->kind==BK_SINGULARITE?3.2f:(b->kind==BK_GHOST?2.2f:(1.2f+0.8f*b->lvl)))*dt;
      float cur=atan2f(b->vy,b->vx), want=atan2f(ty-b->y,tx-b->x);
      float diff=want-cur; while(diff>PI) diff-=TAU; while(diff<-PI) diff+=TAU;
      cur+=clampf(diff,-turn,turn); b->vx=cosf(cur)*b->speed; b->vy=sinf(cur)*b->speed;
    }
  }
  if(b->kind==BK_RAYON){ b->beam_t-=dt; if(b->beam_t<=0){ holy_beam(b->x,b->y); b->beam_t=0.45f; } }
  /* v10 : boussole (toutes les billes s'orientent un peu) */
  if(R.s_home>0 && b->kind!=BK_AIMANT && b->kind!=BK_SINGULARITE && b->kind!=BK_GHOST){
    Enemy *e=nearest_enemy(b->x,b->y,110);
    if(e){ float cur=atan2f(b->vy,b->vx), want=atan2f(e->y-b->y,e->x-b->x); float diff=want-cur; while(diff>PI) diff-=TAU; while(diff<-PI) diff+=TAU;
      float turn=R.s_home*dt; cur+=clampf(diff,-turn,turn); b->vx=cosf(cur)*b->speed; b->vy=sinf(cur)*b->speed; } }
  /* v10 : boomerang et chakram font demi-tour vers le heros */
  b->fly_t+=dt;
  if((b->kind==BK_BOOMERANG && b->boom<1 && b->fly_t>0.65f) || (b->kind==BK_CHAKRAM && b->boom<2 && b->fly_t>0.55f*(b->boom+1))){
    b->boom++; float dx=R.px-b->x, dy=(R.py-8)-b->y, dd=sqrtf(dx*dx+dy*dy); if(dd<1) dd=1;
    if(b->boom==1 || b->kind==BK_CHAKRAM){ b->vx=dx/dd*b->speed; b->vy=dy/dd*b->speed; if(b->boom==2){ b->vx=-b->vx; b->vy=-fabsf(b->vy); } }
    for(int i=0;i<4;i++){ b->hit_id[i]=0; b->hit_t[i]=0; }
    if(!g_norender){ ring(b->x,b->y,8,BALLS[b->kind].col,0.2f); spawn_particles(b->x,b->y,4,BALLS[b->kind].col,40,PT_SPARK,0.25f); }
  }
  float dist=b->speed*dt; int steps=(int)(dist/2.0f)+1; float sdt=dt/steps;
  bool pierce_all = b->kind==BK_SPECTRE || b->kind==BK_FAUCHEUSE || b->kind==BK_RAYON || b->kind==BK_GHOST || b->kind==BK_CAUCHEMAR || b->kind==BK_JUGEMENT || b->kind==BK_BOOMERANG || b->kind==BK_CHAKRAM;
  for(int s=0;s<steps;s++){
    b->x+=b->vx*sdt; b->y+=b->vy*sdt;
    if(b->x-b->r<PIT_X0){ b->x=PIT_X0+b->r; b->vx=fabsf(b->vx); b->wall++; crazy(b); fix_velocity(b); wall_quake(b); if(R.bounce_sfx_t<=0){ sfx_play(SFX_BOUNCE,0.15f,-0.5f,frange(0.9f,1.1f)); R.bounce_sfx_t=0.05f; } }
    if(b->x+b->r>PIT_X1){ b->x=PIT_X1-b->r; b->vx=-fabsf(b->vx); b->wall++; crazy(b); fix_velocity(b); wall_quake(b); if(R.bounce_sfx_t<=0){ sfx_play(SFX_BOUNCE,0.15f,0.5f,frange(0.9f,1.1f)); R.bounce_sfx_t=0.05f; } }
    if(b->y-b->r<PIT_Y0){ b->y=PIT_Y0+b->r; b->vy=fabsf(b->vy); b->wall++; crazy(b); fix_velocity(b); }
    if(b->y+b->r>=FLOOR_Y && b->vy>0){
      if(b->temp){ if(b->kind==BK_GHOST){ b->vy=-fabsf(b->vy); b->y=FLOOR_Y-b->r-1; continue; } b->on=false; return; }
      if(has_ch(CH_MAGNETISEUR) && fabsf(b->x-R.px)<mag_radius() && R.sub==RS_PLAY){
        float ax=R.aimx-b->x, ay=R.aimy-b->y; float d=sqrtf(ax*ax+ay*ay); if(d<1){ ax=0; ay=-1; d=1; } ax/=d; ay/=d; if(ay>-0.3f){ ay=-0.3f; ax=signf(ax)*sqrtf(1-ay*ay); }
        b->vx=ax*b->speed; b->vy=ay*b->speed; b->y=FLOOR_Y-b->r-1; b->relaunch++;
        b->pierce = ball_pierce(b->kind,b->lvl); b->wall=0;
        spawn_particles(b->x,b->y,5,chance(0.5f)?HEX(0xe0453a):HEX(0x5a9aff),50,PT_GLOW,0.3f);
        sfx_play(SFX_RETURN,0.25f,0,1.5f); continue;
      }
      if(R.s_net>0 && b->relaunch<1 && chance(R.s_net)){
        b->vy=-fabsf(b->vy); b->y=FLOOR_Y-b->r-1; b->relaunch++; fix_velocity(b);
        spawn_particles(b->x,FLOOR_Y,4,HEX(0xc9c4d0),40,PT_PIX,0.3f); continue;
      }
      b->state=BS_RETURN; return;
    }
    /* ennemis */
    for(int i=0;i<MAX_EN;i++){
      Enemy *e=&R.en[i]; if(!e->on) continue;
      if(ENEMIES[e->kind].flags&EF_ORBS){
        for(int k=0;k<2;k++){
          float ox=e->x+cosf(e->orb+k*PI)*13, oy=e->y+sinf(e->orb+k*PI)*9;
          float dx=b->x-ox, dy=b->y-oy, dd=dx*dx+dy*dy, rr=b->r+3.5f;
          if(dd<rr*rr && dd>0.01f && !recently_hit(b,-100-e->id*2-k)){
            float d=sqrtf(dd); float nx=dx/d, ny=dy/d; b->x=ox+nx*(rr+0.3f); b->y=oy+ny*(rr+0.3f);
            reflect(b,nx,ny); fix_velocity(b); mark_hit(b,-100-e->id*2-k,0.1f); crazy(b);
            spawn_particles(ox,oy,4,HEX(0xd2b0f5),60,PT_SPARK,0.25f); if(R.bounce_sfx_t<=0){ sfx_at(SFX_SHIELD,ox,0.25f); R.bounce_sfx_t=0.05f; }
          }
        }
      }
      float nx,ny,px,py;
      if(!hit_box(b->x,b->y,b->r,e->x,e->y,e->w*0.5f,e->h*0.5f,&nx,&ny,&px,&py)) continue;
      if((ENEMIES[e->kind].flags&EF_PHASE) && fmodf(e->phase,3.2f)>2.2f) continue;
      if(e->champ==CHAMP_SPECTRAL && fmodf(e->t,3.0f)>2.0f) continue;
      if(submerged(e)) continue;
      if(pierce_all || b->pierce>0){
        if(!recently_hit(b,e->id)){ mark_hit(b,e->id,0.4f); if(!pierce_all) b->pierce--; ball_on_enemy(b,e,ny>0.5f); }
        continue;
      }
      bool again=recently_hit(b,e->id);
      b->x=px; b->y=py; reflect(b,nx,ny); fix_velocity(b);
      if(!again){ mark_hit(b,e->id,0.07f); crazy(b); ball_on_enemy(b,e,ny>0.5f); }
      break;
    }
    if(!b->on) return;
    /* curiosites du decor */
    for(int i=0;i<MAX_CURIO;i++){ Curio *c=&R.cur[i]; if(!c->on) continue;
      float cy=c->y-CURIO_CY, dx=b->x-c->x, dy=b->y-cy, dd=dx*dx+dy*dy, rr=b->r+c->r;
      if(dd>=rr*rr) continue;
      int hid=-300-i; bool again=recently_hit(b,hid); bool solid=!pierce_all && b->pierce<=0;
      if(solid){ float d=sqrtf(maxf(dd,1e-4f)); float nx=dx/d, ny=dy/d; b->x=c->x+nx*(rr+0.3f); b->y=cy+ny*(rr+0.3f); reflect(b,nx,ny); fix_velocity(b); }
      if(!again){ mark_hit(b,hid,0.12f); if(solid) crazy(b); curio_hit(c); }
    }
    /* boss */
    if(R.boss.on && R.boss.enter>=1 && R.boss.dying<=0 && !R.boss.dive){
      Boss *bo=&R.boss; float nx,ny,px,py;
      if(hit_box(b->x,b->y,b->r,bo->x,bo->y,bo->w*0.5f,bo->h*0.5f,&nx,&ny,&px,&py)){
        if(pierce_all || b->pierce>0){ if(!recently_hit(b,-1)){ mark_hit(b,-1,0.25f); if(!pierce_all) b->pierce--; ball_on_boss(b); } }
        else { bool again=recently_hit(b,-1); b->x=px; b->y=py; reflect(b,nx,ny); fix_velocity(b); if(!again){ mark_hit(b,-1,0.07f); crazy(b); ball_on_boss(b); } }
      }
    }
  }
  /* les billes detruisent les projectiles */
  for(int i=0;i<MAX_BUL;i++){
    Bullet *u=&R.bul[i]; if(!u->on) continue;
    float uy = BUL_LOB(u) ? u->y-u->tz : u->y;
    if(sqrf(u->x-b->x)+sqrf(uy-b->y)<sqrf(u->r+b->r+1)){ u->on=false; spawn_particles(u->x,uy,4,u->col,50,PT_SPARK,0.2f); }
  }
}

/* ---------------- boss ---------------- */
static void boss_defeated(void);
static void boss_hurt(float dmg,uint32_t col,bool crit){
  Boss *b=&R.boss; if(!b->on || b->enter<1 || b->dying>0 || b->dive) return;
  { float arm= b->acid>=3 ? 0 : BOSSES[b->type].armor; dmg=maxf(dmg-arm,dmg*0.3f); }
  if(b->acid>0) dmg*=1+0.03f*b->acid;
  if(b->mark_t>0) dmg*=1+b->mark_k*0.6f;
  if(b->guards>0) dmg*=0.35f;
  b->hp-=dmg; if(b->flash<-0.16f) b->flash=0.04f;
  if(S.dmg_numbers){ char s[16]; snprintf(s,sizeof s,crit?"%d!":"%d",(int)(dmg+0.5f)); add_ftext(b->x+frange(-b->w/3,b->w/3),b->y+b->h/2,s,crit?HEX(0xff5a48):col,crit?2:1); }
  if(b->phase==0 && b->hp<b->hpmax*0.5f){
    b->phase=1; b->roar=1.2f; sfx(SFX_BOSS_ROAR); shake(6); hitstop(0.12f); shock_add(b->x,b->y,90,0.6f,1.6f);
    snprintf(R.banner,sizeof R.banner,"%s s'enrage !",BOSSES[b->type].name); R.banner2[0]=0; R.banner_t=2.2f;
  }
  if(b->hp<=0){ b->hp=0; b->dying=0.001f; sfx(SFX_BOSS_DIE); shake(8); hitstop(0.2f); }
}
static void spawn_boss(void){
  if(R.single && hd_get_variant()!=3){   /* v9 : le sol se change en arene du gardien */
    hd_set_variant(3); bg_build(R.biome); hd_gpu_select(R.biome);
    R.levelup_flash=1.2f; shake(5); flash_light((PIT_X0+PIT_X1)*0.5f,ZONE_TOP,260,HEX(0xfff0d8),2.0f,0.6f); }
  Boss *b=&R.boss; memset(b,0,sizeof *b);
  b->on=true; b->type=BIOMES[R.biome].boss; b->w=BOSSES[b->type].w; b->h=BOSSES[b->type].h;
  { static const float SINGLE_HP[BOSS_COUNT]={2600,3300,4300,5200,5600,6800,8000,9500,5800,7000,8200,10000};   /* gardien au bout d'un long niveau */
    float base = (R.single ? SINGLE_HP[b->type]*(R.mode==MODE_DAILY?0.8f:1.0f) : BOSSES[b->type].hp)*duo_hp();
    b->hpmax=b->hp=base*depth_hp()*cycle_hp()*(R.pact?1.3f:1.0f)*(curse_on(6)?1.25f:1.0f)*diff_cur()->boss_hp; }
  b->tx=(PIT_X0+PIT_X1)*0.5f;
  b->x=(PIT_X0+PIT_X1)*0.5f; b->y=-70; b->enter=0; b->atk1=2.0f; b->atk2=5.0f; b->atk3=4.0f; b->roar=1.5f;
  if(run_has_mod(MOD_RAGE)) b->phase=1;
  R.boss_spawned=true; R.boss_t=0; R.boss_hits=0; R.boss_trail=1;
  /* les monstres encore presents s'enfuient devant le gardien : le duel peut commencer */
  for(int i=0;i<MAX_EN;i++){ Enemy *e=&R.en[i]; if(e->on && !e->guard) e->flee=true; }
  music_play(MUS_BOSS,1.0f); sfx(SFX_BOSS_ROAR); shake(6); rumble(0.5f,0.7f);
  snprintf(R.banner,sizeof R.banner,"%s",BOSSES[b->type].name); snprintf(R.banner2,sizeof R.banner2,R.mode==MODE_ENDLESS?"Gardien de l'étage %d":"Gardien du niveau %d",R.single?R.lvl+1:R.stage+1); R.banner_t=3.0f;
}
/* ---------------- gardiens mineurs : deux par niveau, au tiers et aux deux tiers ---------------- */
static const float MB_HP[2]={900,1700};
static Enemy *mb_find(void){ for(int i=0;i<MAX_EN;i++){ Enemy *e=&R.en[i]; if(e->on && e->mboss) return e; } return NULL; }
static void spawn_miniboss(void){
  int kind=lvl_mb_kind(R.lvl,R.seg); const EnemyDef *d=&ENEMIES[kind];
  float cx=(PIT_X0+PIT_X1)*0.5f;
  Enemy *e=enemy_spawn(kind,cx,-44,1.0f); if(!e) return;
  float hp=MB_HP[R.seg]*LVL_BASE[R.lvl]*depth_hp()*(R.pact?1.3f:1.0f)*(curse_on(6)?1.25f:1.0f)*diff_cur()->boss_hp*(R.mode==MODE_DAILY?0.8f:1.0f)*(run_has_mod(MOD_ELITES)?1.5f:1.0f)*duo_hp();
  e->mboss=R.seg+1; e->hpmax=e->hp=hp; e->spawn=1; e->mb_atk=2.2f; e->mb_step=0; e->mb_cx=cx; e->ang=0; e->vx=0;
  float target = R.seg ? 70.0f : 58.0f; float base=(float)(d->w*EN_SCALE);
  e->scale=clampf(target/maxf(8,base),1.3f,2.4f); e->w=d->w*EN_SCALE*e->scale; e->h=d->h*EN_SCALE*e->scale;
  R.mb_on=true; R.mb_id=e->id; R.mb_t=0; R.mb_hpmax=hp; R.boss_t=0; R.boss_hits=0; R.boss_trail=1;
  for(int i=0;i<MAX_EN;i++){ Enemy *o=&R.en[i]; if(o->on && o!=e && !o->guard) o->flee=true; }
  music_play(MUS_BOSS,1.05f); sfx(SFX_BOSS_ROAR); shake(5); rumble(0.4f,0.5f);
  snprintf(R.banner,sizeof R.banner,"%s",lvl_mb_name(R.lvl,R.seg)); snprintf(R.banner2,sizeof R.banner2,"Gardien mineur %d / 2",R.seg+1); R.banner_t=2.6f;
}
static void mb_bullets(Enemy *e,int n,float spread,float spd,int kind){
  float a=aim_at_player(e->x,e->y+e->h*0.3f); uint32_t c=ENEMIES[e->kind].blood;
  for(int k=0;k<n;k++) enemy_bullet(e->x,e->y+e->h*0.3f,a+(k-(n-1)*0.5f)*spread,spd,3,col_lerp(c,HEX(0xffffff),0.25f),kind);
}
static void mboss_update(Enemy *e,float dt,bool frozen){
  R.mb_t+=dt;
  if(e->freeze_t>0.7f) e->freeze_t=0.7f;            /* le gel ne l'arrete qu'un instant */
  e->push=0;
  bool rage=e->hp<e->hpmax*0.5f;
  if(rage && e->sstate==0){ e->sstate=1; sfx(SFX_BOSS_ROAR); shake(5); shock_add(e->x,e->y,80,0.5f,1.4f);
    snprintf(R.banner,sizeof R.banner,"%s s'enrage !",lvl_mb_name(R.lvl,e->mboss-1)); R.banner2[0]=0; R.banner_t=1.8f; }
  float ty=78;
  if(e->y<ty-1){ e->y=approachf(e->y,ty,dt*55); return; }
  if(!frozen){
    e->ang+=dt*(rage?0.8f:0.55f)*(e->slow_t>0?0.5f:1.0f);
    float amp=maxf(10,(PIT_W-e->w)*0.5f-8);
    e->x=e->mb_cx+sinf(e->ang)*amp; e->y=ty+sinf(e->t*1.3f)*5;
  }
  if(frozen || e->blind_t>0 || R.sub!=RS_PLAY) return;
  e->mb_atk-=dt*(rage?1.35f:1.0f)*(e->slow_t>0?0.5f:1.0f);
  if(e->mb_atk>0) return;
  int big=e->mboss-1; int pat=e->mb_step%4; e->mb_step++;
  bool lobber = R.biome==2 || R.biome==6 || R.biome==9;
  switch(pat){
    case 0: mb_bullets(e,3+2*big+(rage?2:0),0.22f,78+6*R.lvl,R.biome==4?3:0); break;
    case 1: { const BiomeDef *bi=&BIOMES[R.biome]; int n=2+big;
      if(count_enemies()<MAX_EN-40) for(int k=0;k<n;k++){ Enemy *m=enemy_spawn(bi->pool[irand(bi->pool_n)],clampf(e->x+frange(-30,30),PIT_X0+12,PIT_X1-12),e->y+e->h*0.4f,hp_scale()*0.7f); if(m){ m->minion=true; m->spawn=0; } }
      ring(e->x,e->y,e->w*0.6f,ENEMIES[e->kind].blood,0.35f); sfx_at(SFX_SPLIT,e->x,0.4f); } break;
    case 2: { int n=10+2*big+(rage?4:0); uint32_t c=col_lerp(ENEMIES[e->kind].blood,HEX(0xffffff),0.25f);
      for(int k=0;k<n;k++) enemy_bullet(e->x,e->y,k*TAU/n+e->t,52+4*R.lvl,3,c,R.biome==4?3:0); } break;
    default:
      if(lobber){ for(int k=0;k<2+big;k++) lob(e->x+frange(-12,12),e->y,R.biome==6?4:(R.biome==9?5:2)); }
      else { mb_bullets(e,2,0.10f,96+6*R.lvl,0); }
      break;
  }
  e->kick=3.0f; e->flash=0.06f; sfx_at(SFX_ENEMY_SHOT,e->x,0.6f);
  e->mb_atk=(2.7f-0.35f*big)*(R.mode==MODE_DAILY?1.1f:1.0f);
}
static void miniboss_defeated(Enemy *e){
  hd_gpu_prefetch(R.biome+16*mini(R.seg+1,2));   /* v9 : decor de la partie suivante, prepare en tache de fond */
  float x=e->x, y=e->y;
  R.mb_on=false; R.mb_warned=false; R.mb_kills++; S.mb_kills++;
  int g=(int)((30+12*R.lvl)*R.s_gold*(R.pact?1.6f:1.0f)); R.gold+=g;
  int cr= e->mboss>=2 ? 1 : 0; R.crystals+=cr;
  spawn_particles(x,y,50,HEX(0xffd36a),160,PT_SPARK,0.7f); spawn_particles(x,y,24,HEX(0xffffff),110,PT_GLOW,0.7f);
  ring(x,y,60,HEX(0xffe0a0),0.5f); flash_light(x,y,170,HEX(0xffe0c0),1.8f,0.5f); shock_add(x,y,130,0.7f,1.8f);
  for(int i=0;i<8;i++) drop_pickup(PK_COIN,x+frange(-16,16),y,2);
  for(int i=0;i<4;i++) drop_pickup(PK_GEM2,x+frange(-14,14),y,3);
  for(int i=0;i<MAX_EN;i++) if(R.en[i].on && &R.en[i]!=e) enemy_kill(&R.en[i],0);
  for(int i=0;i<MAX_BUL;i++) R.bul[i].on=false;
  { float hv=R.hpmax*diff_cur()->boss_heal*0.5f; if(hv>0 && R.hp<R.hpmax){ heal(hv); spawn_particles(R.px,R.py-6,12,HEX(0x8be05a),50,PT_GLOW,0.6f); } }
  snprintf(R.banner,sizeof R.banner,"%s est vaincu !",lvl_mb_name(R.lvl,e->mboss-1));
  if(cr) snprintf(R.banner2,sizeof R.banner2,"+%d "IC_COIN"   +%d "IC_GEM,g,cr); else snprintf(R.banner2,sizeof R.banner2,"+%d "IC_COIN,g);
  R.banner_t=2.6f;
  R.sub=RS_CLEAR; R.clear_t=0; R.mb_reward=true;
  music_play(MUS_TITLE,1.0f); sfx(SFX_BOSS_DIE); shake(7); hitstop(0.18f); rumble(0.8f,0.5f);
  if(R.lvl_hits_seg==0) meta_feat(FEAT_NOHIT_LEVEL);
  if(R.pact) meta_feat(FEAT_PACT);
  R.lvl_hits_seg=0;
  meta_check();
}
static void boss_minion(int kind,float x,float y){
  Enemy *e=enemy_spawn(kind,x,y,hp_scale()*0.8f);
  if(e){ e->minion=true; e->spawn=0; spawn_particles(x,y,8,HEX(0x8a7aa0),40,PT_SMOKE,0.6f); }
}
static void boss_update(float dt){
  Boss *b=&R.boss; if(!b->on) return;
  b->t+=dt; if(b->flash>-1) b->flash-=dt; if(b->roar>0) b->roar-=dt;
  if(b->mark_t>0){ b->mark_t-=dt; if(b->mark_t<=0) b->mark_k=0; }
  if(b->acid>0){ b->acid_t-=dt; if(b->acid_t<=0) b->acid=0; else if(irand(10)==0) spawn_particles(b->x+frange(-b->w/2,b->w/2),b->y+b->h/2-2,1,HEX(0xd8ff40),12,PT_PIX,0.5f); }
  float cx=(PIT_X0+PIT_X1)*0.5f;
  if(b->dying>0){
    b->dying+=dt;
    if(irand(4)==0){ float x=b->x+frange(-b->w/2,b->w/2), y=b->y+frange(-b->h/2,b->h/2);
      spawn_particles(x,y,10,chance(0.5f)?HEX(0xffd36a):HEX(0xff7a22),110,PT_SPARK,0.4f); ring(x,y,frange(10,24),HEX(0xffe0a0),0.3f);
      flash_light(x,y,60,HEX(0xffc080),1.2f,0.2f); if(irand(3)==0) sfx_play(SFX_EXPLODE,0.4f,0,frange(0.8f,1.2f)); shake(3); if(irand(3)==0) shock_add(x,y,30,0.3f,1.0f); }
    if(b->dying>2.3f) boss_defeated();
    return;
  }
  if(b->enter<1){
    b->enter+=dt/2.6f; float t=clampf(b->enter,0,1); t=1-(1-t)*(1-t);
    b->y=lerpf(-70,70,t); b->x=cx;
    if(b->enter>=1){ b->enter=1; shock_add(b->x,b->y,80,0.5f,1.2f); }
    return;
  }
  float px=R.px;
  float ak=(1.0f+0.15f*R.cycle)*(curse_on(6)?1.15f:1.0f);
  b->atk1-=dt*ak; b->atk2-=dt*ak; b->atk3-=dt*ak;
  if(b->anim>0) b->anim=approachf(b->anim,0,dt*1.5f);
  switch(b->type){
    case BOSS_ROI: {
      float sp=b->phase?0.75f:0.5f, amp=b->phase?92:70;
      b->x=cx+sinf(b->t*sp)*amp; b->y=70+sinf(b->t*1.3f)*4;
      if(b->atk1<=0){
        int n=b->phase?7:5; float a=aim_at_player(b->x,b->y+20);
        for(int i=0;i<n;i++) enemy_bullet(b->x,b->y+22,a+(i-(n-1)*0.5f)*0.24f,85,3,HEX(0xeadfc4),0);
        b->anim=1; b->atk1=b->phase?2.0f:2.6f; sfx_at(SFX_ENEMY_SHOT,b->x,0.6f);
      }
      if(b->atk2<=0){
        int n=b->phase?5:4;
        for(int i=0;i<n;i++){ float x=PIT_X0+16+irand(PIT_W/32)*32; boss_minion(EK_SQUELETTE,x,b->y+b->h/2+18+irand(3)*30); }
        b->atk2=7.5f; sfx(SFX_WARN);
      }
      if(b->phase && b->atk3<=0){
        for(int i=0;i<14;i++) enemy_bullet(b->x,b->y,i*TAU/14+b->t,60,3,HEX(0xff6a4a),0);
        b->atk3=5.0f; b->anim=1;
      }
    } break;
    case BOSS_MERE: {
      b->x=cx+sinf(b->t*0.3f)*34; b->y=66+sinf(b->t*1.6f)*3;
      float cyc=fmodf(b->t,5.0f);
      if(cyc<2.4f){ if(b->atk1<=0){ float a=b->t*2.6f; for(int k=0;k<(b->phase?3:2);k++) enemy_bullet(b->x,b->y+16,a+k*TAU/(b->phase?3:2),55,3,HEX(0xf59ac0),0); b->atk1=0.16f; b->anim=0.6f; } }
      if(b->atk2<=0){ for(int i=0;i<3;i++) boss_minion(EK_CHAMPIGNON,PIT_X0+24+frand()*(PIT_W-48),b->y+b->h/2+16); b->atk2=8.5f; sfx(SFX_WARN); }
      if(b->phase && b->atk3<=0){ enemy_bullet(PIT_X0+8+frand()*(PIT_W-16),4,PI*0.5f,62,3,HEX(0xa8f07a),0); b->atk3=0.28f; }
    } break;
    case BOSS_COLOSSE: {
      b->x=cx+sinf(b->t*0.35f)*60; b->y=72;
      if(b->atk1<=0){ float a=aim_at_player(b->x,b->y); enemy_bullet(b->x,b->y+10,a,110,5,HEX(0xff7a22),0); b->atk1=b->phase?2.0f:3.0f; sfx_at(SFX_ENEMY_SHOT,b->x,0.7f); }
      if(b->slam_t>0){
        b->slam_t-=dt; b->anim=clampf(1-b->slam_t/0.9f,0,1);
        if(b->slam_t<=0){
          for(int i=0;i<b->slam_n;i++) for(int k=0;k<4;k++) enemy_bullet(b->slam_x[i],b->y+30+k*16,PI*0.5f,215,4,HEX(0xffb040),1);
          sfx(SFX_SLAM); shake(6); b->anim=0; rumble(0.6f,0.25f); shock_add(b->x,b->y+40,70,0.4f,1.3f);
        }
      } else if(b->atk2<=0){
        b->slam_n=b->phase?5:3; b->slam_t=0.9f;
        for(int i=0;i<b->slam_n;i++){ b->slam_x[i]=(i==0)?clampf(px,PIT_X0+10,PIT_X1-10):PIT_X0+10+frand()*(PIT_W-20); zone_add(ZN_WARN,b->slam_x[i],0,10,0.9f); }
        b->atk2=b->phase?3.8f:5.0f; sfx(SFX_WARN);
      }
      if(b->phase && b->atk3<=0){ int g=0; for(int i=0;i<MAX_EN;i++) if(R.en[i].on && R.en[i].kind==EK_GOLEM) g++; if(g==0) boss_minion(EK_GOLEM,PIT_X0+40+frand()*(PIT_W-80),b->y+70); b->atk3=12; }
    } break;
    case BOSS_GIVRE: {
      b->x=cx+sinf(b->t*0.45f)*70; b->y=68+sinf(b->t*1.1f)*4;
      if(b->atk1<=0){ int n=b->phase?7:5; float a=aim_at_player(b->x,b->y+20);
        for(int i=0;i<n;i++) enemy_bullet(b->x,b->y+24,a+(i-(n-1)*0.5f)*0.2f,90,3,HEX(0x9ae8ff),3);
        b->anim=1; b->atk1=b->phase?1.9f:2.5f; sfx_at(SFX_FREEZE,b->x,0.6f); }
      if(b->slam_t>0){ b->slam_t-=dt; b->anim=maxf(b->anim,0.5f);
        if(b->slam_t<=0){ for(int i=0;i<b->slam_n;i++) for(int k=0;k<3;k++) enemy_bullet(b->slam_x[i],2.0f-k*12,PI*0.5f,230,3.5f,HEX(0xd4f4ff),3); sfx(SFX_FREEZE); shake(3); } }
      else if(b->atk2<=0){ b->slam_n=b->phase?6:4; b->slam_t=0.95f;
        for(int i=0;i<b->slam_n;i++){ b->slam_x[i]=(i==0)?clampf(px,PIT_X0+10,PIT_X1-10):PIT_X0+10+frand()*(PIT_W-20); zone_add(ZN_WARN,b->slam_x[i],0,10,0.95f); }
        b->atk2=b->phase?4.2f:5.6f; sfx(SFX_WARN); }
      if(b->phase && b->atk3<=0){ for(int i=0;i<16;i++) enemy_bullet(b->x,b->y,i*TAU/16+b->t,52,3,HEX(0xe8faff),3); b->atk3=6.0f; b->anim=1; }
      b->spin+=dt; if(b->spin>9.0f){ b->spin=0; for(int i=0;i<2;i++) boss_minion(EK_LOUP,PIT_X0+30+frand()*(PIT_W-60),b->y+b->h/2+14); sfx(SFX_WARN); }
    } break;
    case BOSS_LEVIATHAN: {
      if(b->dive){
        b->dive_t-=dt; b->x=approachf(b->x,b->tx,150*dt);
        if(irand(2)==0) spawn_particles(b->x+frange(-34,34),b->y+frange(-4,12),1,HEX(0x9af0e0),30,PT_GLOW,0.5f);
        if(b->dive_t<=0){ b->dive=0; shock_add(b->x,b->y,90,0.5f,1.4f); sfx(SFX_SLAM); shake(5);
          int n=b->phase?16:12; for(int i=0;i<n;i++) enemy_bullet(b->x,b->y,i*TAU/n,70,3,HEX(0x5af0d0),0);
          spawn_particles(b->x,b->y,30,HEX(0x9af0e0),140,PT_SPARK,0.5f); }
        break;
      }
      b->x=lerpf(b->x,b->tx+sinf(b->t*0.8f)*60,minf(1,dt*2)); b->x=clampf(b->x,PIT_X0+50,PIT_X1-50); b->y=72+sinf(b->t*1.6f)*8;
      if(b->atk1<=0){ float a=aim_at_player(b->x,b->y+14); for(int k=-1;k<=1;k++) enemy_bullet(b->x,b->y+18,a+k*0.3f,64,4.5f,HEX(0x9af0e0),0); b->atk1=b->phase?1.8f:2.4f; b->anim=1; sfx_at(SFX_ENEMY_SHOT,b->x,0.6f); }
      if(b->slam_t>0){ b->slam_t-=dt;
        if(b->slam_t<=0){ for(int i=0;i<b->slam_n;i++){ beam_add(b->slam_x[i],0,FLOOR_Y,HEX(0x7af0e0),7,0.45f); if(fabsf(R.px-b->slam_x[i])<11){ g_hurt_cat=3; player_hurt(16+3.0f*R.depth+5.0f*R.cycle,false); } spawn_particles(b->slam_x[i],ZONE_BOT,10,HEX(0xc8fff4),120,PT_SPARK,0.4f); } sfx(SFX_BEAM); shake(3); } }
      else if(b->atk2<=0){ b->slam_n=b->phase?5:3; b->slam_t=1.0f;
        for(int i=0;i<b->slam_n;i++){ b->slam_x[i]=(i==0)?clampf(px,PIT_X0+10,PIT_X1-10):PIT_X0+10+frand()*(PIT_W-20); zone_add(ZN_WARN,b->slam_x[i],0,10,1.0f); }
        b->atk2=b->phase?4.0f:5.2f; sfx(SFX_WARN); }
      if(b->atk3<=0){ b->dive=1; b->dive_t=2.2f; b->tx=PIT_X0+60+frand()*(PIT_W-120); b->atk3=b->phase?7.5f:9.5f; sfx(SFX_GHOST); spawn_particles(b->x,b->y,24,HEX(0x9af0e0),110,PT_SPARK,0.5f);
        if(b->phase) for(int i=0;i<3;i++) boss_minion(EK_MURENE,PIT_X0+24+frand()*(PIT_W-48),b->y+30); }
    } break;
    case BOSS_RUCHE: {
      b->x=cx+sinf(b->t*0.3f)*30; b->y=64+sinf(b->t*1.4f)*3;
      if(b->atk1<=0){ int w=0; for(int i=0;i<MAX_EN;i++) if(R.en[i].on && R.en[i].kind==EK_OUVRIERE) w++;
        if(w<12) for(int i=0;i<3;i++) boss_minion(EK_OUVRIERE,b->x+frange(-30,30),b->y+b->h/2+6);
        b->atk1=b->phase?5.0f:6.5f; }
      if(b->atk2<=0){ for(int k=0;k<(b->phase?4:3);k++) amber_lob(b->x+frange(-20,20),b->y+20); b->atk2=b->phase?3.4f:4.4f; b->anim=1; sfx_at(SFX_ENEMY_SHOT,b->x,0.6f); }
      if(b->atk3<=0){ float a=aim_at_player(b->x,b->y+40); int n=b->phase?11:7; for(int i=0;i<n;i++) enemy_bullet(b->x,b->y+44,a+(i-(n-1)*0.5f)*0.14f,105,2.5f,HEX(0xf9dc5c),0); b->atk3=b->phase?2.6f:3.4f; b->anim=1; }
      if(b->phase){ int g=0; for(int i=0;i<MAX_EN;i++) if(R.en[i].on && R.en[i].guard) g++; b->guards=g;
        if(g==0){ b->guard_cd-=dt; if(b->guard_cd<=0){ for(int i=0;i<3;i++){ Enemy *e=enemy_spawn(EK_COCON,PIT_X0+50+i*(PIT_W-100)/2.0f,b->y+72,hp_scale()*1.3f); if(e){ e->guard=true; e->minion=true; } }
          b->guard_cd=15; sfx(SFX_BOSS_ROAR); snprintf(R.banner,sizeof R.banner,"Détruisez les cocons !"); R.banner2[0]=0; R.banner_t=2.0f; } } }
    } break;
    case BOSS_NEANT: {
      if(b->dive){ b->dive_t-=dt;
        if(b->dive_t<=0){ b->dive=0; b->x=b->tx; int n=b->phase?24:18; for(int i=0;i<n;i++) enemy_bullet(b->x,b->y,i*TAU/n+b->t,60,3,HEX(0xe0f0ff),0);
          shock_add(b->x,b->y,80,0.5f,1.5f); sfx(SFX_BLINK); spawn_particles(b->x,b->y,30,HEX(0xe0f0ff),120,PT_SPARK,0.4f); }
        break; }
      b->x=clampf(b->tx+sinf(b->t*0.7f)*26,PIT_X0+44,PIT_X1-44); b->y=74+sinf(b->t*1.1f)*10;
      { float cyc=fmodf(b->t,5.0f);
        if(cyc<2.6f && b->atk1<=0){ int arms=b->phase?3:2; float a=b->t*2.4f; for(int k=0;k<arms;k++) enemy_bullet(b->x,b->y,a+k*TAU/arms,58,2.8f,(k&1)?HEX(0x8a7aff):HEX(0xe0f0ff),0); b->atk1=0.13f; b->anim=0.5f; } }
      if(b->slam_t>0){ b->slam_t-=dt;
        if(b->slam_t<=0){ for(int i=0;i<b->slam_n;i++){ beam_add(b->slam_x[i],0,FLOOR_Y,HEX(0xd0c8ff),6,0.4f); if(fabsf(R.px-b->slam_x[i])<10){ g_hurt_cat=3; player_hurt(20+4.0f*R.depth+6.0f*R.cycle,false); } } sfx(SFX_LASER); shake(4); rumble(0.35f,0.3f); } }
      else if(b->atk2<=0){ b->slam_n=b->phase?4:3; b->slam_t=1.0f;
        for(int i=0;i<b->slam_n;i++){ b->slam_x[i]=(i==0)?clampf(px,PIT_X0+10,PIT_X1-10):PIT_X0+10+frand()*(PIT_W-20); zone_add(ZN_WARN,b->slam_x[i],0,10,1.0f); }
        b->atk2=b->phase?3.8f:5.0f; sfx(SFX_LASER_CHARGE); }
      if(b->atk3<=0){ b->dive=1; b->dive_t=0.45f; b->tx=PIT_X0+60+frand()*(PIT_W-120); b->atk3=b->phase?6.0f:8.0f;
        spawn_particles(b->x,b->y,30,HEX(0xe0f0ff),120,PT_SPARK,0.4f); sfx(SFX_BLINK);
        if(b->phase) for(int i=0;i<2;i++) boss_minion(EK_ECHO,PIT_X0+30+frand()*(PIT_W-60),b->y+50); }
    } break;
    /* ---------------- v7 : les gardiens du Royaume oublie ---------------- */
    case BOSS_ARBRE: {
      b->x=cx+sinf(b->t*0.35f)*48; b->y=68+sinf(b->t*1.2f)*3;
      if(b->atk1<=0){ int n=b->phase?7:5; float a=aim_at_player(b->x,b->y+24);   /* graines en eventail */
        for(int i=0;i<n;i++) enemy_bullet(b->x,b->y+26,a+(i-(n-1)*0.5f)*0.22f,80,3,HEX(0xc8ff8a),0);
        b->anim=1; b->atk1=b->phase?1.9f:2.5f; sfx_at(SFX_ENEMY_SHOT,b->x,0.6f); }
      if(b->slam_t>0){ b->slam_t-=dt;   /* racines qui jaillissent du sol */
        if(b->slam_t<=0){ for(int i=0;i<b->slam_n;i++){ beam_add(b->slam_x[i],ZONE_TOP-30,FLOOR_Y,HEX(0x8ac850),7,0.5f); spawn_particles(b->slam_x[i],ZONE_BOT,12,HEX(0x6a4a24),110,PT_SQ,0.5f);
            if(fabsf(R.px-b->slam_x[i])<11){ g_hurt_cat=3; player_hurt(14+3.0f*R.depth+5.0f*R.cycle,false); } }
          sfx(SFX_SLAM); sfx_play(SFX_ROOT,0.6f,0,0.7f); shake(4); rumble(0.4f,0.25f); } }
      else if(b->atk2<=0){ b->slam_n=b->phase?5:3; b->slam_t=1.0f;
        for(int i=0;i<b->slam_n;i++){ b->slam_x[i]=(i==0)?clampf(px,PIT_X0+10,PIT_X1-10):PIT_X0+10+frand()*(PIT_W-20); zone_add(ZN_WARN,b->slam_x[i],0,10,1.0f); }
        b->atk2=b->phase?4.2f:5.6f; sfx(SFX_WARN); }
      if(b->atk3<=0){ int w=0; for(int i=0;i<MAX_EN;i++) if(R.en[i].on && R.en[i].kind==EK_LUCIOLE) w++;   /* essaim de lucioles */
        if(w<10) for(int i=0;i<(b->phase?5:3);i++) boss_minion(EK_LUCIOLE,b->x+frange(-40,40),b->y+b->h/2+8);
        b->atk3=b->phase?7.0f:9.0f; }
      if(b->phase){ b->spin+=dt; if(b->spin>5.5f){ b->spin=0; for(int i=0;i<16;i++) enemy_bullet(b->x,b->y,i*TAU/16+b->t,56,3,HEX(0xffb8e8),0); b->anim=1; } }
    } break;
    case BOSS_CRAPAUD: {
      if(b->dive){   /* le grand bond : il retombe la ou vous etiez */
        b->dive_t-=dt;
        if(b->dive_t<=0){ b->dive=0; b->x=b->tx; b->y=72; shock_add(b->x,b->y+20,100,0.5f,1.6f); sfx(SFX_SLAM); shake(6); rumble(0.6f,0.3f);
          int n=b->phase?16:12; for(int i=0;i<n;i++) enemy_bullet(b->x,b->y+20,i*TAU/n,68,3,HEX(0xb8d040),0);
          for(int k=0;k<(b->phase?3:2);k++){ Zone *z=zone_add(ZN_AMBER,clampf(b->x+frange(-50,50),PIT_X0+16,PIT_X1-16),frange(ZONE_TOP+10,ZONE_BOT-6),17,4.0f); if(z) z->aux=1; }
          spawn_particles(b->x,b->y+20,30,HEX(0x8aa040),140,PT_SQ,0.5f); }
        break; }
      b->x=approachf(b->x,b->tx,60*dt); b->y=72-fabsf(sinf(b->t*2.2f))*4;
      if(fabsf(b->x-b->tx)<2 && chance(dt*0.5f)) b->tx=PIT_X0+60+frand()*(PIT_W-120);
      if(b->slam_t>0){ b->slam_t-=dt; b->anim=maxf(b->anim,0.6f);   /* coup de langue */
        if(b->slam_t<=0){ for(int i=0;i<b->slam_n;i++){ beam_add(b->slam_x[i],b->y+18,FLOOR_Y,HEX(0xff7aa8),5,0.35f); if(fabsf(R.px-b->slam_x[i])<10){ g_hurt_cat=3; player_hurt(15+3.0f*R.depth+5.0f*R.cycle,false); } }
          sfx(SFX_DASH); shake(3); b->anim=1; } }
      else if(b->atk1<=0){ b->slam_n=b->phase?2:1; b->slam_t=0.7f;
        for(int i=0;i<b->slam_n;i++){ b->slam_x[i]= i==0 ? clampf(px,PIT_X0+10,PIT_X1-10) : clampf(px+frange(-60,60),PIT_X0+10,PIT_X1-10); zone_add(ZN_WARN,b->slam_x[i],0,10,0.7f); }
        b->atk1=b->phase?2.6f:3.4f; sfx(SFX_WARN); }
      if(b->atk2<=0){ for(int k=0;k<(b->phase?4:3);k++) lob(b->x+frange(-20,20),b->y+16,5); b->atk2=b->phase?3.6f:4.8f; b->anim=1; sfx_at(SFX_ENEMY_SHOT,b->x,0.6f); }   /* crachats de vase */
      if(b->atk3<=0){ b->dive=1; b->dive_t=1.3f; b->tx=clampf(px,PIT_X0+60,PIT_X1-60); b->atk3=b->phase?7.0f:9.0f; sfx(SFX_DASH); spawn_particles(b->x,b->y+20,20,HEX(0x8aa040),100,PT_SQ,0.5f);
        if(b->phase) for(int i=0;i<2;i++) boss_minion(EK_CRAPAUD,PIT_X0+30+frand()*(PIT_W-60),b->y+40); }
    } break;
    case BOSS_PRISME: {
      b->x=cx+sinf(b->t*0.4f)*56; b->y=70+sinf(b->t*1.5f)*3;
      if(b->atk1<=0){ int n=b->phase?16:12; float off=frand()*TAU;   /* anneau d'eclats */
        for(int i=0;i<n;i++) enemy_bullet(b->x,b->y,off+i*TAU/n,60,3,(i&1)?HEX(0xff8ad8):HEX(0x9af8ff),0);
        b->anim=1; b->atk1=b->phase?2.4f:3.0f; sfx_at(SFX_CRYSTAL,b->x,0.6f); }
      if(b->slam_t>0){ b->slam_t-=dt;   /* rayons du prisme */
        if(b->slam_t<=0){ static const uint32_t RB[5]={HEX(0xff6a8a),HEX(0xffd84a),HEX(0x7af07a),HEX(0x5ab8ff),HEX(0xc08aff)};
          for(int i=0;i<b->slam_n;i++){ beam_add(b->slam_x[i],0,FLOOR_Y,RB[i%5],6,0.45f); if(fabsf(R.px-b->slam_x[i])<10){ g_hurt_cat=3; player_hurt(18+3.0f*R.depth+5.0f*R.cycle,false); } }
          sfx(SFX_LASER); shake(3); rumble(0.3f,0.25f); } }
      else if(b->atk2<=0){ b->slam_n=b->phase?4:3; b->slam_t=1.0f;
        for(int i=0;i<b->slam_n;i++){ b->slam_x[i]=(i==0)?clampf(px,PIT_X0+10,PIT_X1-10):PIT_X0+10+frand()*(PIT_W-20); zone_add(ZN_WARN,b->slam_x[i],0,10,1.0f); }
        b->atk2=b->phase?4.0f:5.2f; sfx(SFX_LASER_CHARGE); }
      if(b->phase){
        if(b->atk3<=0){ float a=aim_at_player(b->x,b->y+24); for(int k=-1;k<=1;k++) enemy_bullet(b->x,b->y+26,a+k*0.18f,110,3,HEX(0xffffff),0); b->atk3=1.6f; }
        int g=0; for(int i=0;i<MAX_EN;i++) if(R.en[i].on && R.en[i].guard) g++; b->guards=g;
        if(g==0){ b->guard_cd-=dt; if(b->guard_cd<=0){ for(int i=0;i<3;i++){ Enemy *e=enemy_spawn(EK_GOLEM_CRISTAL,PIT_X0+50+i*(PIT_W-100)/2.0f,b->y+74,hp_scale()*1.2f); if(e){ e->guard=true; e->minion=true; } }
          b->guard_cd=16; sfx(SFX_BOSS_ROAR); snprintf(R.banner,sizeof R.banner,"Brisez les golems !"); R.banner2[0]=0; R.banner_t=2.0f; } }
      } else if(b->atk3<=0){ for(int i=0;i<2;i++) boss_minion(EK_ARAIGNEE_CRISTAL,PIT_X0+30+frand()*(PIT_W-60),b->y+44); b->atk3=8.0f; }
    } break;
    case BOSS_SOLEIL: {
      if(b->dive){ b->dive_t-=dt;   /* eclipse : il reparait dans un anneau de flammes */
        if(b->dive_t<=0){ b->dive=0; b->x=b->tx; int n=b->phase?26:20; for(int i=0;i<n;i++) enemy_bullet(b->x,b->y,i*TAU/n+b->t,62,3,(i&1)?HEX(0xffc040):HEX(0xff6a2a),0);
          shock_add(b->x,b->y,90,0.5f,1.6f); sfx(SFX_EXPLODE); spawn_particles(b->x,b->y,30,HEX(0xffd080),130,PT_SPARK,0.5f); }
        break; }
      b->x=clampf(b->tx+sinf(b->t*0.6f)*40,PIT_X0+46,PIT_X1-46); b->y=72+sinf(b->t*1.2f)*8;
      { float cyc=fmodf(b->t,5.5f);   /* spirale solaire */
        if(cyc<2.8f && b->atk1<=0){ int arms=b->phase?4:3; float a=b->t*2.1f; for(int k=0;k<arms;k++) enemy_bullet(b->x,b->y,a+k*TAU/arms,60,2.8f,(k&1)?HEX(0xff6a2a):HEX(0xffd84a),0); b->atk1=0.15f; b->anim=0.6f; } }
      if(b->slam_t>0){ b->slam_t-=dt;   /* eruptions venues du ciel */
        if(b->slam_t<=0){ for(int i=0;i<b->slam_n;i++){ beam_add(b->slam_x[i],0,FLOOR_Y,HEX(0xffa030),7,0.45f); if(fabsf(R.px-b->slam_x[i])<11){ g_hurt_cat=3; player_hurt(20+4.0f*R.depth+6.0f*R.cycle,false); } spawn_particles(b->slam_x[i],ZONE_BOT,10,HEX(0xffd080),110,PT_EMBER,0.5f); }
          sfx(SFX_BEAM); shake(4); rumble(0.35f,0.3f); } }
      else if(b->atk2<=0){ b->slam_n=b->phase?5:3; b->slam_t=1.0f;
        for(int i=0;i<b->slam_n;i++){ b->slam_x[i]=(i==0)?clampf(px,PIT_X0+10,PIT_X1-10):PIT_X0+10+frand()*(PIT_W-20); zone_add(ZN_WARN,b->slam_x[i],0,10,1.0f); }
        b->atk2=b->phase?3.8f:5.0f; sfx(SFX_LASER_CHARGE); }
      if(b->atk3<=0){ b->dive=1; b->dive_t=0.6f; b->tx=PIT_X0+60+frand()*(PIT_W-120); b->atk3=b->phase?6.5f:8.5f; spawn_particles(b->x,b->y,30,HEX(0xffc040),120,PT_SPARK,0.4f); sfx(SFX_BLINK);
        if(b->phase) for(int i=0;i<2;i++) boss_minion(EK_PHENIX,PIT_X0+30+frand()*(PIT_W-60),b->y+50); }
    } break;
    default: {
      b->x=cx+sinf(b->t*0.6f)*80; b->y=72+sinf(b->t*1.2f)*14;
      if(b->atk1<=0){ int n=b->phase?20:16; float off=frand()*TAU; for(int i=0;i<n;i++) enemy_bullet(b->x,b->y,off+i*TAU/n,62,3,HEX(0xc08aff),0); b->atk1=b->phase?2.8f:3.5f; sfx_at(SFX_ENEMY_SHOT,b->x,0.6f); }
      if(b->laser_st==0 && b->atk2<=0){ b->laser_st=1; b->laser_t=0; b->laser_x=px; sfx(SFX_LASER_CHARGE); }
      if(b->laser_st==1){
        b->laser_t+=dt; if(b->laser_t<0.6f) b->laser_x=approachf(b->laser_x,px,160*dt);
        b->anim=0;
        if(b->laser_t>=1.1f){ b->laser_st=2; b->laser_t=0; b->laser_hit=false; sfx(SFX_LASER); shake(4); rumble(0.35f,0.6f); }
      } else if(b->laser_st==2){
        b->laser_t+=dt;
        if(!b->laser_hit && fabsf(R.px-b->laser_x)<10){ b->laser_hit=true; g_hurt_cat=3; player_hurt(18+3.0f*R.depth+6.0f*R.cycle,false); }
        if(b->laser_t>=0.9f){ b->laser_st=0; b->atk2=b->phase?5.5f:7.0f; }
      }
      if(b->phase){
        int g=0; for(int i=0;i<MAX_EN;i++) if(R.en[i].on && R.en[i].guard) g++;
        b->guards=g;
        if(g==0){ b->guard_cd-=dt; b->anim=approachf(b->anim,0,dt*2); }
        else b->anim=approachf(b->anim,0.55f,dt*2);
        if(g==0 && b->guard_cd<=0){
          for(int i=0;i<4;i++){ Enemy *e=enemy_spawn(EK_TENTACULE,PIT_X0+40+i*(PIT_W-80)/3.0f,b->y+62,hp_scale()*1.1f); if(e){ e->guard=true; e->minion=true; } }
          b->guard_cd=18; sfx(SFX_BOSS_ROAR);
          snprintf(R.banner,sizeof R.banner,"Détruisez les tentacules !"); R.banner2[0]=0; R.banner_t=2.0f;
        }
      }
    } break;
  }
}

/* ---------------- niveaux ---------------- */
static void clear_world(void){
  memset(R.en,0,sizeof R.en); memset(R.bul,0,sizeof R.bul); memset(R.zones,0,sizeof R.zones);
  memset(R.bolts,0,sizeof R.bolts); memset(R.flashes,0,sizeof R.flashes); memset(R.txt,0,sizeof R.txt);
  memset(R.shocks,0,sizeof R.shocks); memset(R.beams,0,sizeof R.beams); nblast=0; ndark=0;
  memset(R.cur,0,sizeof R.cur); R.curio_t=frange(6,10); if(getenv("GOUFFRE_CURIOS")) R.curio_t=0.2f;   /* tests : curiosites en rafale */
  for(int i=0;i<MAX_BALLS;i++){ Ball *b=&R.balls_e[i]; if(!b->on) continue; if(b->temp){ b->on=false; continue; } b->state=BS_READY; b->x=R.px; b->y=R.py-8; }
}
static void level_begin(int stage){
  R.stage=stage;
  if(R.single){ R.level=R.lvl%LEVELS_PER_DEST; R.cycle=0; R.dest=R.lvl/LEVELS_PER_DEST; }
  else {
    R.level=stage%LEVELS_PER_DEST; R.cycle=stage/LEVELS_PER_DEST;
    /* infini : une fois les autres destinations ouvertes, chaque cycle en change */
    { int nd=1; while(nd<DEST_COUNT && dest_unlocked(nd)) nd++;
      R.dest = R.mode==MODE_ENDLESS ? (R.dest0+R.cycle)%nd : R.dest0; }
  }
  R.biome = R.dest*LEVELS_PER_DEST + R.level;
  R.seg=0; R.mb_on=false; R.mb_warned=false; R.mb_warn=0; R.mb_reward=false; R.lvl_hits_seg=0;
  { extern float g_hurt_by[5]; memset(g_hurt_by,0,sizeof g_hurt_by); memset(g_breach_kind,0,sizeof g_breach_kind); g_inflow=g_dealt=0; }
  R.ltime=0; R.lduration=R.single?LVL_SPAWN:BIOMES[R.biome].duration;
  R.len_k=R.single?powf(BIOMES[R.biome].duration*LVL_LONG/LVL_SPAWN,LVL_XP_POW):1.0f;
  R.dmg_k=R.single?powf(BIOMES[R.biome].duration*LVL_LONG/LVL_SPAWN,LVL_DMG_POW):1.0f;   /* niveau plus long : on encaisse moins par coup */ R.spawn_t=1.0f; R.rows=0; R.boss_spawned=false; R.boss_warn=0;
  R.pact=R.pact_next; R.pact_next=false;
  if(R.mode==MODE_RUSH){ R.ltime=R.lduration; R.pending_levels+= stage==0 ? 6 : 3; }   /* v9 : rush, le gardien d'emblee */
  memset(&R.boss,0,sizeof R.boss);
  clear_world();
  memset(R.pick,0,sizeof R.pick);
  R.px=(PIT_X0+PIT_X1)*0.5f; R.py=ZONE_BOT-16;
  R.sub=RS_INTRO; R.sub_t=0; R.lvl_hits=0;
  R.champ_rate=0.015f*(R.depth-1)+0.05f*R.cycle+0.008f*R.level+0.01f*R.dest+(run_has_mod(MOD_CHAMPIONS)?0.15f:0.0f)+(run_has_mod(MOD_ELITES)?0.10f:0.0f)+(curse_on(10)?0.08f:0.0f);
  if(R.mode==MODE_ENDLESS){ snprintf(R.banner,sizeof R.banner,"Étage %d",stage+1); if(stage+1>S.endless_best) S.endless_best=stage+1; }
  else if(R.single) snprintf(R.banner,sizeof R.banner,"Niveau %d",R.lvl+1);
  else if(R.mode==MODE_RUSH) snprintf(R.banner,sizeof R.banner,"Rush : gardien %d / %d",R.level+1,LEVELS_PER_DEST);
  else snprintf(R.banner,sizeof R.banner,R.dest==2?"Royaume %d":(R.dest?"Profondeurs %d":"Niveau %d"),R.level+1);
  snprintf(R.banner2,sizeof R.banner2,"%s%s",BIOMES[R.biome].name,R.pact?"  ^3(pacte)":""); R.banner_t=2.6f;
  music_play(music_level(R.biome),R.cycle>0?1.04f:1.0f);   /* v6.6 : chaque niveau a son theme */
  hd_set_variant(R.mode==MODE_RUSH ? 3 : 0); { const char *pv=getenv("GOUFFRE_PARTIE"); if(pv && *pv) hd_set_variant(atoi(pv)); }   /* tests : partie forcee */   /* v9 : premiere partie du niveau (le rush se joue dans les arenes) */
  bg_build(R.biome);
  hd_gpu_select(R.biome);   /* v8 : textures HD du niveau pour la carte graphique */
  run_recalc_stats();
  if(R.single){ R.max_level_reached=R.lvl+1; }
  else { if(R.level+1>R.max_level_reached) R.max_level_reached=R.level+1; if(R.max_level_reached>S.best_level) S.best_level=R.max_level_reached; }
}
/* reprise du niveau apres un gardien mineur (et l'evenement qui suit) */
static void level_resume(void){
  R.seg=mini(R.seg+1,2); R.mb_on=false; R.mb_warned=false; R.mb_warn=0; R.mb_reward=false;
  if(R.pact_next){ R.pact=true; R.pact_next=false; }
  clear_world(); memset(R.pick,0,sizeof R.pick);
  hd_set_variant(R.seg); bg_build(R.biome); hd_gpu_select(R.biome);   /* v9 : le decor change a chaque partie du niveau */
  R.px=(PIT_X0+PIT_X1)*0.5f; R.py=ZONE_BOT-16; R.comp_x=R.px-14; R.comp_y=R.py+2;
  R.sub=RS_INTRO; R.sub_t=0; R.spawn_t=1.0f; R.lvl_hits_seg=0;
  snprintf(R.banner,sizeof R.banner,"%s",BIOMES[R.biome].name);
  snprintf(R.banner2,sizeof R.banner2,"%s%s",R.seg>=2?"Dernière partie du niveau":"Deuxième partie du niveau",R.pact?"  ^3(pacte)":""); R.banner_t=2.4f;
  music_play(music_level(R.biome),R.cycle>0?1.04f:1.0f);
  run_recalc_stats();
}

static void spawn_row(float prog){
  if(count_enemies()>MAX_EN-30) return;
  const BiomeDef *bi=&BIOMES[R.biome];
  float hpm=hp_scale()*(R.single?1.0f:(1+0.75f*prog));
  int cols=PIT_W/(16*EN_SCALE); const int CW=16*EN_SCALE;
  float dens= R.single ? 0.18f+0.34f*prog+0.015f*R.lvl+0.03f*mini(R.depth-1,6)
                       : 0.18f+0.22f*prog+0.04f*R.level+0.03f*mini(R.depth-1,6)+0.04f*R.cycle;
  if(dens>0.7f) dens=0.7f;
  if(curse_on(8)) dens=minf(0.8f,dens*1.2f);
  if(run_has_mod(MOD_HORDE)) dens=minf(0.85f,dens*1.35f);
  int pat=irand(12);
  if(R.rows<2) pat=0;
  int type_main=bi->pool[irand(bi->pool_n)];
  if(pat==11 && prog>0.25f){
    int c=irange(0,cols-2); Enemy *e=enemy_spawn(bi->elite,PIT_X0+c*CW+CW,-30,hpm); if(e) e->spawn=0;
    for(int k=0;k<3;k++){ int cc=irand(cols); if(cc>=c-1 && cc<=c+2) continue; enemy_spawn(bi->pool[irand(bi->pool_n)],PIT_X0+cc*CW+CW/2,-16,hpm); }
  } else {
    bool used[32]={0};
    for(int c=0;c<cols;c++){
      bool put=false;
      if(pat<=6) put=chance(dens);
      else if(pat<=8) put=((c+R.rows)&1)==0 && chance(dens*1.6f);
      else if(pat==9){ int c1=(R.rows*4)%cols; put=(abs(c-c1)<=1)||(abs(c-(c1+5)%cols)<=1); }
      else put=chance(dens*1.25f) && (c%4)!=1;
      if(!put || used[c]) continue;
      int k = chance(0.55f)?type_main:bi->pool[irand(bi->pool_n)];
      Enemy *e=enemy_spawn(k,PIT_X0+c*CW+CW/2,-16,hpm); used[c]=true;
      if(e && R.champ_rate>0 && chance(R.champ_rate)) make_champion(e);
    }
  }
  R.rows++;
}

static void card_hint(int k,char *buf,int n);
void run_debug_jump(int lvl){
  static const int give[4]={BK_FEU,BK_FOUDRE,BK_ECLAT,BK_GLACE};
  for(int i=0;i<4 && R.nballs<MAX_SLOTS;i++){ if(run_ball_level(give[i])) continue; R.balls[R.nballs]=give[i]; R.ball_lvl[R.nballs]=mini(3,1+lvl); R.nballs++; }
  for(int i=0;i<R.nballs;i++) R.ball_lvl[i]=mini(3,R.ball_lvl[i]+lvl);
  static const int pas[3]={PS_FORCE,PS_CADENCE,PS_SAC};
  for(int i=0;i<3;i++){ R.pas[R.npas]=pas[i]; R.pas_lvl[R.npas]=mini(3,lvl); R.npas++; }
  R.plevel=4+lvl*5; R.xpnext=xp_need(R.plevel);
  run_recalc_stats(); R.hp=R.hpmax; sync_balls();
  level_begin(lvl);
}
/* tests : un kit de fin de niveau, puis directement le gardien */
void run_debug_jump_end(void){
  static const int give[4]={BK_FEU,BK_FOUDRE,BK_ECLAT,BK_GLACE};
  for(int i=0;i<4 && R.nballs<MAX_SLOTS;i++){ if(run_ball_level(give[i])) continue; R.balls[R.nballs]=give[i]; R.ball_lvl[R.nballs]=3; R.nballs++; }
  for(int i=0;i<R.nballs;i++) R.ball_lvl[i]=3;
  static const int pas[3]={PS_FORCE,PS_CADENCE,PS_SAC};
  for(int i=0;i<3 && R.npas<MAX_SLOTS;i++){ R.pas[R.npas]=pas[i]; R.pas_lvl[R.npas]=3; R.npas++; }
  R.plevel=19; R.xpnext=xp_need(R.plevel);
  run_recalc_stats(); R.hp=R.hpmax; sync_balls();
  if(R.single){ R.seg=2; } R.ltime=R.lduration-0.5f;
}
void run_debug_balls(int set){
  static const int SETS[11][5]={{BK_LUMIERE,BK_FOLLE,BK_CRISTAL,BK_RAFALE,BK_FEU},{BK_RAYON,BK_KALEIDO,BK_GEODE,BK_BLIZZARD,BK_TEMPFEU},{BK_AURORE,BK_METEORE,BK_PRISME,BK_SINGULARITE,BK_FLEAU},{BK_VAPEUR,BK_TEMPETE,BK_FAUCHEUSE,BK_ENCLUME,BK_GHOST},
    {BK_ECHO,BK_DOREE,BK_RACINE,BK_ACIDE,BK_FER},{BK_TONNERRE,BK_MIDAS,BK_RONCES,BK_PLUIE,BK_ECLAT},
    {BK_OMBRE,BK_RICOCHET,BK_LAVE,BK_ETOILE,BK_FER},{BK_ECLIPSE,BK_CAUCHEMAR,BK_CARAMBOLE,BK_VOLCAN,BK_CONSTELLATION},{BK_SUPERNOVA,BK_BIGBANG,BK_JUGEMENT,BK_ZERO,BK_OUROBOROS},
    {BK_BOOMERANG,BK_SEISME,BK_CHAINE,BK_FER,BK_FEU},{BK_CHAKRAM,BK_FAILLE,BK_ENTRAVE,BK_APOCALYPSE,BK_BOOMERANG}};
  set=clampi(set,0,10); R.nballs=0;
  for(int i=0;i<5;i++){ if(SETS[set][i]==BK_GHOST) continue; R.balls[R.nballs]=SETS[set][i]; R.ball_lvl[R.nballs]=IS_EVO(SETS[set][i])?1:3; R.nballs++; }
  static const int pas[5]={PS_BOUCLIER,PS_RAGE,PS_FIOLE,PS_MIROIR,PS_FORCE};
  R.npas=0; for(int i=0;i<5;i++){ R.pas[R.npas]=pas[i]; R.pas_lvl[R.npas]=3; R.npas++; }
  R.plevel=15; R.xpnext=xp_need(R.plevel); R.shield_up=true;
  if(set>=4) R.gold=600;   /* billes dorees : de quoi frapper fort */
  run_recalc_stats(); R.hp=R.hpmax*0.5f; sync_balls();
  level_begin(1);
}
static int bk_start[BK_COUNT];
static int rs_ch, rs_ch2=-1, rs_depth=1, rs_mode=MODE_NORMAL, rs_level=0;   /* pour le redemarrage rapide */
void run_start(int ch,int ch2,int depth,int mode,int level){
  { static bool once=false; if(!once){ once=true; const char *e=getenv("GOUFFRE_LB"); if(e){ for(int i=0;i<BIOME_COUNT && *e;i++){ LVL_BASE[i]=(float)atof(e); const char *c=strchr(e,','); if(!c) break; e=c+1; } }
      const char *r=getenv("GOUFFRE_RD"); if(r) RD_LVL=(float)atof(r); } }
  memset(&R,0,sizeof R);
  rs_ch=ch; rs_ch2=ch2; rs_depth=depth; rs_mode=mode; rs_level=level;
  LAST.new_depth=false; LAST.new_checks=0; LAST.next_unlocked=false; LAST.new_dest=false; LAST.check_gold=0; LAST.record_time=false; LAST.obj_met=LAST.obj_new=0; LAST.record=false;
  R.ch=clampi(ch,0,CH_COUNT-1); R.ch2=(ch2>=0 && ch2<CH_COUNT && ch2!=R.ch)?ch2:-1;
  R.depth=clampi(depth,1,DEPTH_MAX); R.mode=mode; R.diff_low=clampi(S.difficulty,0,DIFF_COUNT-1);
  R.single = mode==MODE_NORMAL || mode==MODE_DAILY;
  if(mode==MODE_DAILY){ int h; daily_setup(daily_seed_today(),&h,R.mods); R.nmods=2; R.lvl=daily_level_today(); R.ch2=-1; }
  else R.lvl=clampi(level,0,BIOME_COUNT-1);
  { const char *mt=getenv("GOUFFRE_MUTS"); if(mt && mode==MODE_NORMAL){ S.mut_sel=(uint32_t)atoi(mt); S.ach[ach_for_reward(RW_MODE,MUT_UNLOCK)]=true; } }   /* tests */
  if(mode==MODE_NORMAL && mut_unlocked() && S.mut_sel){ R.muts=S.mut_sel&((1u<<MOD_COUNT)-1); R.nmods=0; for(int m=0;m<MOD_COUNT;m++) if((R.muts>>m)&1) R.mods[R.nmods++]=m; }   /* v10 : mutateurs */
  R.dest0=R.dest= (R.single||mode==MODE_RUSH) ? R.lvl/LEVELS_PER_DEST : 0;
  R.rank=hero_rank(R.ch); R.rank2= R.ch2>=0 ? hero_rank(R.ch2) : 0;
  R.curses=R.mode==MODE_DAILY?0:maxi(0,R.depth-5);
  memcpy(bk_start,S.ball_kills,sizeof bk_start);
  R.px=(PIT_X0+PIT_X1)*0.5f; R.py=ZONE_BOT-16; R.comp_x=R.px-14; R.comp_y=R.py+2; R.comp_face=1;
  R.plevel=1; R.xpnext=xp_need(1);
  R.balls[0]=CHARS[R.ch].start_ball; R.ball_lvl[0]=1; R.nballs=1;
  /* duo : le compagnon apporte sa propre bille */
  if(R.ch2>=0 && CHARS[R.ch2].start_ball!=R.balls[0]){ R.balls[1]=CHARS[R.ch2].start_ball; R.ball_lvl[1]=1; R.nballs=2; }
  /* laboratoire */
  if(S.bld[BLD_LABO]>0 && R.nballs<MAX_SLOTS){
    int cand[BK_COUNT], nc=0; for(int k=BK_FIRST_BASE;k<=BK_LAST_BASE;k++) if(run_ball_level(k)==0 && ball_unlocked(k)) cand[nc++]=k;
    if(nc){ R.balls[R.nballs]=cand[irand(nc)]; R.ball_lvl[R.nballs]=S.bld[BLD_LABO]>=4?3:(S.bld[BLD_LABO]>=2?2:1); R.nballs++; }
    if(S.bld[BLD_LABO]>=3){ int pc[PS_COUNT], np=0; for(int k=0;k<PS_COUNT;k++) if(pas_unlocked(k)) pc[np++]=k; if(np){ R.pas[0]=pc[irand(np)]; R.pas_lvl[0]=S.bld[BLD_LABO]>=4?2:1; R.npas=1; } }
  }
  R.rerolls=S.bld[BLD_PUITS]+(maxi(R.rank,R.rank2)>=7?1:0); R.revives=S.bld[BLD_SANCTUAIRE]>=3?2:(S.bld[BLD_SANCTUAIRE]>0?1:0);
  R.autoaim=S.autoaim; R.next_id=1; R.aimx=R.px; R.aimy=0; R.aim_ang=-PI*0.5f;
  R.timeslow_cd=ch_rank(CH_HORLOGERE)>=5?11:14; R.shield_cd=0; R.hp_trail=1;
  run_recalc_stats(); R.hp=R.hpmax;
  if(run_pas_level(PS_BOUCLIER)>0) R.shield_up=true;
  sync_balls();
  R.tutorial_t= S.runs<3 ? 10 : 0;
  meta_run_begin();
  level_begin(0);
}

/* ---------------- choix de montee de niveau ---------------- */
static void gen_cards(bool boss){
  Card pool[64]; float w[64]; int np=0;
  int n = 3 + (run_pas_level(PS_TREFLE)>=3?1:0);
  R.ncards=0;
  if(S.bld[BLD_CREUSET]>0) for(int k=BK_FIRST_LEG;k<=BK_LAST_LEG;k++){   /* v9 : fusions legendaires (deux evolutions) */
    int a=BALLS[k].evo_a, b=BALLS[k].evo_b;
    if(run_ball_level(a)>0 && run_ball_level(b)>0 && R.ncards<n){ Card c={CARD_LEG,k,a,b}; R.cards[R.ncards++]=c; }
  }
  for(int k=BK_FIRST_EVO;k<=BK_LAST_EVO;k++){
    int a=BALLS[k].evo_a, b=BALLS[k].evo_b;
    if(run_ball_level(a)>=3 && run_ball_level(b)>=3 && R.ncards<n){ Card c={CARD_EVO,k,a,b}; R.cards[R.ncards++]=c; }
  }
  for(int i=0;i<R.nballs;i++) if(!IS_EVO(R.balls[i]) && R.ball_lvl[i]<3){ Card c={CARD_UPBALL,R.balls[i],0,0}; pool[np]=c; w[np++]=boss?14:10; }
  if(R.nballs<MAX_SLOTS) for(int k=BK_FIRST_BASE;k<=BK_LAST_BASE;k++){
    if(!ball_unlocked(k)) continue;
    bool own=run_ball_level(k)>0;
    for(int e=BK_FIRST_EVO;e<=BK_LAST_EVO;e++) if(run_ball_level(e)>0 && (BALLS[e].evo_a==k||BALLS[e].evo_b==k)) own=true;
    if(!own && np<60){ Card c={CARD_NEWBALL,k,0,0}; pool[np]=c; w[np++]=6; }
  }
  for(int i=0;i<R.npas;i++) if(R.pas_lvl[i]<3 && np<60){ Card c={CARD_UPPAS,R.pas[i],0,0}; pool[np]=c; w[np++]=8; }
  if(R.npas<MAX_SLOTS) for(int k=0;k<PS_COUNT;k++) if(run_pas_level(k)==0 && pas_unlocked(k) && np<60){ Card c={CARD_NEWPAS,k,0,0}; pool[np]=c; w[np++]=5; }
  while(R.ncards<n && np>0){
    float tot=0; for(int i=0;i<np;i++) tot+=w[i];
    float r=frand()*tot; int pick=np-1; for(int i=0;i<np;i++){ r-=w[i]; if(r<=0){ pick=i; break; } }
    R.cards[R.ncards++]=pool[pick]; pool[pick]=pool[np-1]; w[pick]=w[np-1]; np--;
  }
  if(R.ncards<n){ Card c={CARD_HEAL,0,0,0}; R.cards[R.ncards++]=c; }
  if(R.ncards<n){ Card c={CARD_GOLD,0,0,0}; R.cards[R.ncards++]=c; }
  R.card_t=0; R.card_sel=0;
}
static void open_levelup(bool boss){
  R.boss_reward=boss; gen_cards(boss);
  R.lu_from=R.sub; R.sub=RS_LEVELUP;
  if(!boss) sfx(SFX_LEVELUP);
  rumble(0.25f,0.12f);
}
static int slot_of(int k){ for(int i=0;i<R.nballs;i++) if(R.balls[i]==k) return i; return -1; }
static void apply_card(Card *c){
  switch(c->type){
    case CARD_NEWBALL: if(R.nballs<MAX_SLOTS){ R.balls[R.nballs]=c->kind; R.ball_lvl[R.nballs]=1; R.nballs++; R.picked_balls++; } sfx(SFX_SELECT); break;
    case CARD_UPBALL: { int s=slot_of(c->kind); if(s>=0 && R.ball_lvl[s]<3) R.ball_lvl[s]++; sfx(SFX_UPGRADE); } break;
    case CARD_NEWPAS: if(R.npas<MAX_SLOTS){ R.pas[R.npas]=c->kind; R.pas_lvl[R.npas]=1; R.npas++; } if(c->kind==PS_COEUR) R.hp+=25; if(c->kind==PS_BOUCLIER) R.shield_up=true; sfx(SFX_SELECT); break;
    case CARD_UPPAS: for(int i=0;i<R.npas;i++) if(R.pas[i]==c->kind && R.pas_lvl[i]<3) R.pas_lvl[i]++; if(c->kind==PS_COEUR) R.hp+=25; sfx(SFX_UPGRADE); break;
    case CARD_EVO: {
      int sa=slot_of(c->a), sb=slot_of(c->b); if(sa<0||sb<0) break;
      int keep=sa<sb?sa:sb, drop=sa<sb?sb:sa;
      R.balls[keep]=c->kind; R.ball_lvl[keep]=1;
      for(int i=drop;i<R.nballs-1;i++){ R.balls[i]=R.balls[i+1]; R.ball_lvl[i]=R.ball_lvl[i+1]; }
      R.nballs--;
      S.evo_seen|=1u<<(c->kind-BK_FIRST_EVO); R.evolutions_done++; S.evolutions++;
      if(R.evolutions_done>=3) meta_feat(FEAT_3EVOS);
      if(R.evolutions_done>=4) meta_feat(FEAT_4EVOS);
      sfx(SFX_EVOLVE); R.levelup_flash=1; shake(4); rumble(0.5f,0.3f); shock_add(R.px,R.py-8,120,0.6f,1.5f);
      snprintf(R.banner,sizeof R.banner,"Évolution : %s !",BALLS[c->kind].name); R.banner2[0]=0; R.banner_t=2.2f;
    } break;
    case CARD_LEG: {
      int sa=slot_of(c->a), sb=slot_of(c->b); if(sa<0||sb<0) break;
      int keep=sa<sb?sa:sb, drop=sa<sb?sb:sa;
      R.balls[keep]=c->kind; R.ball_lvl[keep]=1;
      for(int i=drop;i<R.nballs-1;i++){ R.balls[i]=R.balls[i+1]; R.ball_lvl[i]=R.ball_lvl[i+1]; }
      R.nballs--;
      S.evo_seen|=1u<<(c->kind-BK_FIRST_EVO); S.legendaries++; R.legendaries++;
      sfx(SFX_EVOLVE); sfx_play(SFX_EXPLODE,0.6f,0,0.6f); R.levelup_flash=1.5f; shake(7); rumble(0.8f,0.5f); shock_add(R.px,R.py-8,200,0.8f,2.2f); hitstop(0.1f);
      spawn_particles(R.px,R.py-10,40,BALLS[c->kind].col,160,PT_SPARK,0.8f); ring(R.px,R.py-10,60,HEX(0xffffff),0.5f);
      snprintf(R.banner,sizeof R.banner,"Fusion légendaire : %s !",BALLS[c->kind].name); R.banner2[0]=0; R.banner_t=2.8f;
    } break;
    case CARD_HEAL: R.hp=minf(R.hpmax,R.hp+R.hpmax*0.3f*heal_mult()); sfx(SFX_HEAL); break;
    case CARD_GOLD: R.gold+=(int)(25*R.s_gold); sfx(SFX_COIN); break;
  }
  run_recalc_stats();
  sync_balls();
  meta_check();
}
static void win_bookkeeping(void){
  if(R.won) return;
  R.won=true; S.wins++; S.hero_wins|=1u<<R.ch; if(R.ch2>=0) S.hero_wins|=1u<<R.ch2;
  if(R.mode==MODE_NORMAL && R.single){
    int L=R.lvl, d=R.depth;
    bool first=lvl_maxwon(L)==0; int dmax0=lvl_depth_max(L);
    unsigned add=(1u<<R.ch)|(R.ch2>=0?(1u<<R.ch2):0u);
    int df=clampi(mini(R.diff_low,S.difficulty),0,DIFF_COUNT-1);   /* coches de la difficulte jouee */
    LAST.new_checks = add & ~(unsigned)S.lvl_done_d[df][L][d-1];
    if(R.muts){ int nm=mut_count(R.muts); R.crystals+=nm; S.mut_wins++; if(nm>S.mut_best) S.mut_best=nm; }   /* v10 : mutateurs : un cristal par mutateur */
    for(int k=0;k<3;k++) if(obj_met(L,k)){ LAST.obj_met|=1<<k; if(!((S.obj_done[L]>>k)&1)){ S.obj_done[L]|=(uint8_t)(1<<k); LAST.obj_new|=1<<k; R.crystals+=2; } }   /* v9 : objectifs */
    S.lvl_done_d[df][L][d-1]|=(uint16_t)add;
    S.lvl_done[L][d-1]|=(uint16_t)add;
    /* chaque nouvelle coche rapporte de l'or (moitie en Facile, moitie en plus en Difficile) */
    { int n=0; for(int c=0;c<CH_COUNT;c++) n+=(LAST.new_checks>>c)&1; LAST.check_gold=n*(40+20*L)*(df==DIFF_FACILE?1:(df==DIFF_DIFFICILE?3:2))/2; R.gold+=LAST.check_gold; }
    LAST.next_unlocked = first && L+1<BIOME_COUNT && lvl_prereq(L+1)==L;
    LAST.new_dest = first && L==LEVELS_PER_DEST-1;   /* v7 : le Coeur du Gouffre ouvre aussi le Royaume oublie */
    LAST.new_depth = lvl_depth_max(L)>dmax0;
    S.levels_won++; if(R.dest==1) S.prof_wins++; if(R.dest==2) S.roy_wins++;
    if(S.lvl_best[L]==0 || (int)R.total_time<S.lvl_best[L]){ LAST.record_time = S.lvl_best[L]!=0; S.lvl_best[L]=(int)R.total_time; }
    lvl_derive();
  }
  if(R.mode==MODE_DAILY) S.daily_wins++;
  if(S.difficulty<R.diff_low) R.diff_low=clampi(S.difficulty,0,DIFF_COUNT-1);
  S.diff_wins[R.diff_low]++;
  if(R.single && R.boss_time<80) meta_feat(FEAT_SPEED);
  if(R.single && R.dest==1 && R.boss_time<100) meta_feat(FEAT_SPEED_PROF);
  if(R.single && R.dest==2 && R.boss_time<100) meta_feat(FEAT_SPEED_ROY);
  if(R.single && R.lvl>=3 && R.evolutions_done==0) meta_feat(FEAT_NO_EVO_WIN);
  if(R.single && R.lvl>=3 && R.nballs<=2) meta_feat(FEAT_FEW_BALLS);
  if(R.revived) meta_feat(FEAT_REVIVE_WIN);
  if(R.ch2>=0) meta_feat(FEAT_DUO);
  meta_check();
}
static void after_choice(void){
  if(R.boss_reward){
    R.boss_reward=false;
    if(R.single){
      if(R.boss_spawned){ win_bookkeeping(); R.sub=RS_WIN; R.sub_t=0; R.banner_t=0; R.levelup_flash=0; music_play(MUS_TITLE,1.0f); sfx(SFX_REVIVE); return; }
      R.sub=RS_CLEAR; R.clear_t=10; return;     /* gardien mineur : un evenement, puis la suite du niveau */
    }
    if(R.level>=LEVELS_PER_DEST-1 && R.cycle==0 && R.mode==MODE_ENDLESS){ snprintf(R.banner,sizeof R.banner,"Le gouffre continue…"); snprintf(R.banner2,sizeof R.banner2,"Mode infini : la descente reprend, plus dure"); R.banner_t=3.0f; }
    R.sub=RS_CLEAR; R.clear_t=10; /* -> transition */
    return;
  }
  R.pending_levels--;
  if(R.pending_levels>0) gen_cards(false);
  else R.sub = R.lu_from==RS_CLEAR ? RS_CLEAR : RS_PLAY;
}
static void choose_card(int i){
  if(i<0 || i>=R.ncards) return;
  apply_card(&R.cards[i]);
  after_choice();
}
void run_choose_card(int i){ if(R.sub==RS_LEVELUP) choose_card(i); }
void run_skip_card(void){ if(R.sub!=RS_LEVELUP) return; int g=(int)(8*R.s_gold+0.5f); R.gold+=g; sfx(SFX_COIN); after_choice(); }
void run_reroll_cards(void){
  if(R.sub!=RS_LEVELUP) return;
  if(R.rerolls>0){ R.rerolls--; R.used_reroll=true; gen_cards(R.boss_reward); sfx(SFX_SELECT); } else sfx(SFX_DENY);
}

static void boss_defeated(void){
  Boss *b=&R.boss;
  b->on=false;
  int g= R.single ? (int)((70+30*R.lvl)*R.s_gold) : (int)((60+45*R.level+60*R.cycle+80*R.dest)*R.s_gold); R.gold+=g;
  int cr= R.single ? 1+(R.lvl>=3?1:0)+(run_has_mod(MOD_RAGE)?1:0)+(curse_on(6)?1:0)
                   : 1+(R.level==LEVELS_PER_DEST-1?1:0)+(run_has_mod(MOD_RAGE)?1:0)+(curse_on(6)?1:0)+(R.dest&&R.level>=2?1:0);
  R.crystals+=cr;
  int pr=train_boss_prisms(); R.prisms+=pr;   /* v6.6 : prismes pour l'entrainement des heros */
  spawn_particles(b->x,b->y,60,HEX(0xffd36a),180,PT_SPARK,0.8f);
  for(int i=0;i<24;i++){ static const uint32_t RB[6]={HEX(0xff6a6a),HEX(0xffb84a),HEX(0xf4f06a),HEX(0x7ae88a),HEX(0x6ad8ff),HEX(0xb48aff)}; spawn_particles(b->x,b->y,1,RB[i%6],140,PT_GLOW,1.0f); }
  spawn_particles(b->x,b->y,30,HEX(0xffffff),120,PT_GLOW,0.8f);
  ring(b->x,b->y,70,HEX(0xffe0a0),0.6f); flash_light(b->x,b->y,200,HEX(0xffe0c0),2.0f,0.6f); shock_add(b->x,b->y,160,0.8f,2.0f);
  for(int i=0;i<12;i++) drop_pickup(PK_COIN,b->x+frange(-20,20),b->y,3);
  for(int i=0;i<MAX_EN;i++) if(R.en[i].on) enemy_kill(&R.en[i],0);
  for(int i=0;i<MAX_BUL;i++) R.bul[i].on=false;
  /* le gardien tombe : on reprend son souffle avant de descendre plus bas */
  { float hv=R.hpmax*(diff_cur()->boss_heal+(R.mode==MODE_RUSH?0.4f:0.0f)); if(hv>0 && R.hp<R.hpmax){ heal(hv); sfx(SFX_HEAL); spawn_particles(R.px,R.py-6,16,HEX(0x8be05a),60,PT_GLOW,0.7f); } }
  snprintf(R.banner,sizeof R.banner,"%s est vaincu !",BOSSES[b->type].name);
  snprintf(R.banner2,sizeof R.banner2,"+%d "IC_COIN"   +%d "IC_GEM"   +%d "IC_PRISM,g,cr,pr); R.banner_t=3.0f;
  R.sub=RS_CLEAR; R.clear_t=0;
  music_play(MUS_TITLE,1.0f); rumble(1.0f,0.7f);
  /* statistiques et exploits */
  S.boss_kills[b->type]++; S.bosses_total++; R.st_bosses++;
  if(R.lvl_hits_seg==0) meta_feat(FEAT_NOHIT_LEVEL);
  R.lvl_hits_seg=0;
  if(R.boss_hits==0) meta_feat(FEAT_BOSS_NOHIT);
  if(R.boss_t<40 && b->type!=BOSS_ROI) meta_feat(FEAT_FAST_BOSS);
  if(R.hp<R.hpmax*0.1f) meta_feat(FEAT_LOW_HP_BOSS);
  if(b->type==BOSS_ROI && R.picked_balls==0) meta_feat(FEAT_SOLO);
  if(R.pact) meta_feat(FEAT_PACT);
  if(R.dest==1 && R.boss_hits==0) meta_feat(FEAT_NOHIT_PROF);
  if(R.dest==2 && R.boss_hits==0) meta_feat(FEAT_NOHIT_ROY);
  if(b->type==BOSS_NEANT && R.boss_t<60) meta_feat(FEAT_FAST_VOID);
  meta_check();
}

static void run_lose(void){
  if(R.revives>0){
    R.revives--; R.revived=true; R.hp=R.hpmax*(S.bld[BLD_SANCTUAIRE]>=2?1.0f:0.5f); R.iframes=3.0f;
    for(int i=0;i<MAX_BUL;i++) R.bul[i].on=false;
    for(int i=0;i<MAX_EN;i++){ Enemy *e=&R.en[i]; if(!e->on) continue; if(e->y>ZONE_TOP-60){ e->push=-160; enemy_hurt(e,60*hp_scale(),HEX(0xd2b0f5),false,false,false); } }
    ring(R.px,R.py,120,HEX(0xd2b0f5),0.7f); flash_light(R.px,R.py,160,HEX(0xd2b0f5),2,0.6f); shock_add(R.px,R.py-8,140,0.7f,1.8f);
    sfx(SFX_REVIVE); shake(6); rumble(0.7f,0.4f);
    snprintf(R.banner,sizeof R.banner,"Résurrection !"); snprintf(R.banner2,sizeof R.banner2,"Le sanctuaire vous protège"); R.banner_t=2.0f;
    return;
  }
  R.hp=0; R.sub=RS_DEAD; R.dead_t=0; music_stop(); rumble(0.9f,0.6f);
  shatter(R.px,R.py-SPR[CHARS[R.ch].sprite].h/2,CHARS[R.ch].sprite,1,80,HEX(0xff4a4a));
  spawn_particles(R.px,R.py-6,20,HEX(0xff4a4a),100,PT_SQ,0.9f);
  sfx(SFX_BOSS_DIE); shake(8); hitstop(0.25f); shock_add(R.px,R.py-8,100,0.6f,1.6f);
}

static void end_bookkeeping(bool won){
  bool nd=LAST.new_depth; unsigned nc=LAST.new_checks; bool nu=LAST.next_unlocked, nds=LAST.new_dest; int cg=LAST.check_gold; bool rt=LAST.record_time;
  memset(&LAST,0,sizeof LAST);
  LAST.new_depth=nd; LAST.new_checks=nc; LAST.next_unlocked=nu; LAST.new_dest=nds; LAST.check_gold=cg; LAST.record_time=rt;
  LAST.ch2=R.ch2; LAST.lvl=R.single?R.lvl:-1; LAST.mb_kills=R.mb_kills;
  LAST.won=won; LAST.level=R.max_level_reached; LAST.kills=R.kills; LAST.gold=R.gold; LAST.crystals=R.crystals;
  LAST.time=R.total_time; LAST.plevel=R.plevel; LAST.ch=R.ch; LAST.nballs=R.nballs; LAST.mode=R.mode; LAST.stage=R.stage+1; LAST.depth=R.depth;
  if(S.difficulty<R.diff_low) R.diff_low=clampi(S.difficulty,0,DIFF_COUNT-1);
  LAST.dest=R.dest0; LAST.biome=R.biome; LAST.diff=R.diff_low;
  for(int i=0;i<R.nballs;i++){ LAST.balls[i]=R.balls[i]; LAST.ball_lvl[i]=R.ball_lvl[i]; }
  if(won) win_bookkeeping();
  S.gold+=R.gold; S.gold_total+=R.gold; S.crystals+=R.crystals; S.crystals_total+=R.crystals; S.runs++;
  S.prisms+=R.prisms; S.prisms_total+=R.prisms; LAST.prisms=R.prisms;
  S.play_time+=(int)R.total_time; if(R.dest0==1) S.prof_runs++; if(R.dest0==2) S.roy_runs++;
  if(R.plevel>S.best_plevel) S.best_plevel=R.plevel;
  if(R.gold>=600) meta_feat(FEAT_RICH);
  if(R.gold>=3000) meta_feat(FEAT_GOLD_3000);
  if(R.kills>=1000) meta_feat(FEAT_KILLS_2000);
  /* experience permanente des heros (le compagnon en recoit les trois quarts) */
  { LAST.rank0=hero_rank(R.ch);
    float hx = R.single ? (R.kills*EN_XP+120.0f*R.st_bosses+60.0f*R.mb_kills+40.0f*(R.lvl+1)+(won?400.0f+60.0f*R.lvl:0.0f))
                        : (R.kills*EN_XP+120.0f*R.st_bosses+40.0f*R.max_level_reached+(won?400.0f:0.0f));
    hx*=(1+0.1f*(R.depth-1))*(R.dest0?1.25f:1.0f)*(1+0.1f*S.bld[BLD_AUTEL]);
    if(R.mode==MODE_DAILY) hx*=1.2f;
    hx*=DIFFS[R.diff_low].reward;
    LAST.hxp=(int)hx; S.hero_xp[R.ch]=mini(S.hero_xp[R.ch]+(int)hx,100000000);
    LAST.rank1=hero_rank(R.ch);
    if(R.ch2>=0){ LAST.rank0b=hero_rank(R.ch2); LAST.hxp2=(int)(hx*0.75f); S.hero_xp[R.ch2]=mini(S.hero_xp[R.ch2]+LAST.hxp2,100000000); LAST.rank1b=hero_rank(R.ch2); } }
  /* etoiles de maitrise gagnees */
  LAST.nstars=0;
  for(int k=0;k<BK_COUNT && LAST.nstars<6;k++){ if(!ball_masterable(k)) continue;
    int s0=0, s1=ball_stars(k); for(int st=1;st<=3;st++) if(bk_start[k]>=ball_star_need(k,st)) s0=st;
    if(s1>s0){ LAST.stars_k[LAST.nstars]=k; LAST.stars_n[LAST.nstars]=s1; LAST.nstars++; } }
  if(R.mode==MODE_ENDLESS){ LAST.record = R.stage+1>=S.endless_best; }
  if(R.mode==MODE_DAILY){
    int today=daily_seed_today();
    int score=R.kills*EN_XP+500*R.st_bosses+250*R.mb_kills+(R.won?3000+maxi(0,1200-(int)R.total_time):0);
    LAST.score=score;
    if(S.daily_date!=today){ S.daily_date=today; S.daily_best=0; S.daily_done++; S.crystals+=R.won?3:1; LAST.crystals+=R.won?3:1; }
    if(score>S.daily_best){ S.daily_best=score; LAST.record=true; }
  }
  meta_check();
  LAST.nachs=meta_run_achs(LAST.achs,16);
  save_write();
}
void run_end_to_results(bool won){
  if(R.ended) return;
  R.ended=true;
  end_bookkeeping(won);
  /* premiere victoire sur le dernier niveau d'une destination : la cinematique de fin, puis le bilan */
  if(won && R.single && R.mode==MODE_NORMAL && R.lvl%LEVELS_PER_DEST==LEVELS_PER_DEST-1){
    int id= CINE_FIN+R.lvl/LEVELS_PER_DEST;
    if(!(S.ends_seen&(1<<(id-1)))){ cine_request(id,GS_RESULTS); return; }
  }
  goto_state(GS_RESULTS);
}
/* redemarrage rapide : la descente en cours compte comme perdue, on repart aussitot */
void run_quick_restart(void){
  if(!R.ended){ R.ended=true; end_bookkeeping(false); }
  start_run_request(rs_ch,rs_ch2,rs_depth,rs_mode,rs_level);
}
void run_abandon(void){ R.sub=RS_DEAD; R.dead_t=2.0f; music_stop(); }

/* ---------------- evenements entre les niveaux ---------------- */
static const char *EV_TITLE[EV_COUNT]={"Fontaine de jouvence","Forgeron errant","Coffre scellé","Autel du sang","Autel de l'avarice","Pacte des ombres",
  "Marchand ambulant","Statue du Jongleur","Puits sans fond","Bibliothèque oubliée","Camp de réfugiés","Champignon étrange",
  "Source glacée","Épave engloutie","Miroir du néant"};
static const char *EV_DESC[EV_COUNT]={
  "Une eau claire jaillit d'une vasque de pierre, intacte malgré les siècles.",
  "Un vieux forgeron a installé son enclume au bord du gouffre. Il accepte l'or.",
  "Un coffre couvert de runes. Quelque chose cliquette à l'intérieur… ou quelqu'un.",
  "Une pierre gravée réclame du sang. En échange, elle promet la force.",
  "Des pièces d'or tapissent un autel. Une voix murmure : « donne, et reçois ».",
  "Une ombre vous propose un marché : plus de danger, plus de richesses.",
  "Un marchand a poussé sa carriole jusqu'ici. Ses prix sont… raisonnables.",
  "La statue d'un jongleur légendaire. Des billes de pierre tournent autour d'elle.",
  "Un puits dont on n'entend jamais le fond. On dit qu'il exauce les vœux.",
  "Des rayonnages poussiéreux remplis de traités sur l'art du lancer.",
  "Des voyageurs épuisés se sont abrités ici. Ils n'ont presque plus rien.",
  "Un champignon violet luit doucement. Il sent étonnamment bon.",
  "Une source à demi gelée murmure dans la roche. Ce qui y plonge en ressort changé.",
  "La coque d'un navire englouti, échouée au fond de la cité. Des coffres flottent encore dans la cale.",
  "Un éclat de néant reflète une autre version de vous-même. Elle vous sourit."};
void run_event_text(const char **title,const char **desc){ *title=EV_TITLE[R.ev_id]; *desc=EV_DESC[R.ev_id]; }
static int upgradable_ball(void){ int c[MAX_SLOTS], n=0; for(int i=0;i<R.nballs;i++) if(!IS_EVO(R.balls[i]) && R.ball_lvl[i]<3) c[n++]=i; return n?c[irand(n)]:-1; }
int run_event_options(char lab[4][96],bool en[4]){
  int n=0; int g=R.gold;
  #define OPT(cond,...) do{ snprintf(lab[n],96,__VA_ARGS__); en[n]=(cond); n++; }while(0)
  switch(R.ev_id){
    case 0: OPT(R.hp<R.hpmax,"Boire : soigne tous vos PV"); OPT(1,"Remplir une fiole : ^3+15 PV max"); break;
    case 1: OPT(g>=40 && upgradable_ball()>=0,"Payer ^140 "IC_COIN"^0 : +1 niveau à une bille"); OPT(g>=60,"Payer ^160 "IC_COIN"^0 : ^1+10%% de dégâts"); break;
    case 2: OPT(1,"Ouvrir le coffre (risqué)"); break;
    case 3: OPT(R.hpmax>40,"Sacrifier ^325%% PV max^0 : ^1+25%% de dégâts"); break;
    case 4: OPT(g>=50,"Offrir ^150 "IC_COIN"^0 : un objet au hasard"); OPT(g>=120,"Offrir ^1120 "IC_COIN"^0 : ^6+1 cristal"); break;
    case 5: OPT(1,"Accepter : monstres ^3+30%% PV^0, or et XP ^1+60%%"); break;
    case 6: OPT(g>=30 && R.hp<R.hpmax,"Potion (^130 "IC_COIN"^0) : soigne 50%%"); OPT(g>=25,"Dés pipés (^125 "IC_COIN"^0) : ^1+2 relances"); OPT(g>=120 && R.revives==0,"Pierre d'âme (^1120 "IC_COIN"^0) : résurrection"); break;
    case 7: OPT(1,"Prier : ^1+3 petites billes"); break;
    case 8: OPT(g>=10,"Jeter une pièce (^110 "IC_COIN"^0) : ^1+1 relance"); OPT(1,"Se pencher au-dessus du vide…"); break;
    case 9: OPT(1,"Étudier : un niveau de héros gratuit"); break;
    case 10: OPT(g>=30,"Partager (^130 "IC_COIN"^0) : ils vous offrent ^6un cristal"); break;
    case 12: OPT(R.hpmax>40 && upgradable_ball()>=0,"Plonger une bille : ^1+1 niveau^0, ^3-15 PV max"); OPT(1,"Boire : soigne ^340%%^0, ^1+10%% de vitesse"); break;
    case 13: OPT(1,"Fouiller la cale (risqué)"); OPT(1,"Récupérer une ancre : ^1+12%% de dégâts^0, ^3-8%% de vitesse"); break;
    case 14: OPT(1,"Toucher le reflet : ^1billes +1 niveau^0, mais ^3pacte"); OPT(R.hp>R.hpmax*0.3f,"Briser le miroir : ^6+2 cristaux^0, ^3-25%% PV"); break;
    default: OPT(1,"Le manger…"); break;
  }
  OPT(1,R.ev_id==5?"Refuser":"Continuer sa route");
  #undef OPT
  return n;
}
void run_event_choose(int i){
  char lab[4][96]; bool en[4]; int n=run_event_options(lab,en);
  if(i<0||i>=n||!en[i]||R.ev_result>=0) { sfx(SFX_DENY); return; }
  R.ev_result=i; R.ev_t=0; S.events_done++;
  if(i==n-1){ snprintf(R.ev_msg,sizeof R.ev_msg,R.ev_id==5?"L'ombre s'efface en ricanant.":"Vous reprenez votre descente."); sfx(SFX_CLICK); meta_check(); return; }
  char *m=R.ev_msg; int mn=(int)sizeof R.ev_msg; bool good=true;
  switch(R.ev_id){
    case 0: if(i==0){ R.hp=R.hpmax; snprintf(m,mn,"L'eau fraîche referme toutes vos blessures."); } else { R.ev_hp+=15; snprintf(m,mn,"Vous gardez une fiole précieuse : ^3+15 PV max^0."); } break;
    case 1: if(i==0){ R.gold-=40; int s=upgradable_ball(); if(s>=0){ R.ball_lvl[s]++; snprintf(m,mn,"Le forgeron retrempe votre %s : niveau ^1%d^0 !",BALLS[R.balls[s]].name,R.ball_lvl[s]); } }
            else { R.gold-=60; R.ev_dmg+=0.10f; snprintf(m,mn,"Toutes vos billes sont affûtées : ^1+10%% de dégâts^0."); } break;
    case 2: if(chance(0.7f)){ int g=(int)(frange(60,120)*R.s_gold); R.gold+=g; snprintf(m,mn,"Le coffre regorge d'or : ^1+%d "IC_COIN"^0 !",g); }
            else { float d=R.hpmax*0.2f; R.hp=maxf(1,R.hp-d); good=false; snprintf(m,mn,"Un piège ! Des aiguilles empoisonnées : ^3-%d PV^0.",(int)d); } break;
    case 3: { float d=R.hpmax*0.25f; R.ev_hp-=d/(run_has_mod(MOD_VERRE)?0.6f:1.0f); R.ev_dmg+=0.25f; snprintf(m,mn,"La pierre boit votre sang. Vous vous sentez ^1bien plus fort^0."); } break;
    case 4: if(i==0){ R.gold-=50;
              int c[PS_COUNT], k=0; for(int p=0;p<PS_COUNT;p++){ int l=run_pas_level(p); if((l==0 && R.npas<MAX_SLOTS && pas_unlocked(p)) || (l>0 && l<3)) c[k++]=p; }
              if(k){ int p=c[irand(k)]; Card cd={run_pas_level(p)?CARD_UPPAS:CARD_NEWPAS,p,0,0}; apply_card(&cd); snprintf(m,mn,"L'autel vous accorde : ^2%s^0.",PASSIVES[p].name); }
              else snprintf(m,mn,"L'autel reste muet… mais garde votre or.");
            } else { R.gold-=120; R.crystals++; snprintf(m,mn,"Un ^6cristal^0 se matérialise dans votre main."); } break;
    case 5: R.pact_next=true; snprintf(m,mn,R.single?"Le pacte est scellé. La suite du niveau sera ^3plus dure^0, mais ^1plus généreuse^0.":"Le pacte est scellé. Le prochain niveau sera ^3plus dur^0, mais ^1plus généreux^0."); break;
    case 6: if(i==0){ R.gold-=30; R.hp=minf(R.hpmax,R.hp+R.hpmax*0.5f); snprintf(m,mn,"Une potion au goût de fraise. Vous vous sentez mieux."); }
            else if(i==1){ R.gold-=25; R.rerolls+=2; snprintf(m,mn,"Deux dés qui retombent toujours du bon côté : ^1+2 relances^0."); }
            else { R.gold-=120; R.revives++; snprintf(m,mn,"La pierre d'âme pulse doucement : vous ^6reviendrez^0 une fois."); } break;
    case 7: R.ev_babies+=3; snprintf(m,mn,"Trois billes de pierre se détachent et vous suivent : ^1+3 petites billes^0."); break;
    case 8: if(i==0){ R.gold-=10; R.rerolls++; snprintf(m,mn,"Plouf… un vœu exaucé : ^1+1 relance^0."); }
            else if(chance(0.5f)){ R.crystals++; snprintf(m,mn,"Vous apercevez un éclat au fond… ^6+1 cristal^0 !"); }
            else { float d=R.hpmax*0.15f; R.hp=maxf(1,R.hp-d); good=false; snprintf(m,mn,"Quelque chose vous griffe depuis le noir : ^3-%d PV^0.",(int)d); } break;
    case 9: R.pending_levels++; snprintf(m,mn,R.single?"Vous apprenez une nouvelle technique : ^1un niveau de plus^0 dès la reprise.":"Vous apprenez une nouvelle technique : ^1un niveau de plus^0 au début du prochain niveau."); break;
    case 10: R.gold-=30; R.crystals++; snprintf(m,mn,"Les réfugiés vous remercient avec un ^6cristal^0 trouvé dans les profondeurs."); break;
    case 12: if(i==0){ int sl=upgradable_ball(); if(sl>=0){ R.ball_lvl[sl]++; R.ev_hp-=15; snprintf(m,mn,"La glace trempe votre %s : niveau ^1%d^0. Le froid vous mord : ^3-15 PV max^0.",BALLS[R.balls[sl]].name,R.ball_lvl[sl]); } }
             else { R.hp=minf(R.hpmax,R.hp+R.hpmax*0.4f*heal_mult()); R.ev_speed+=0.10f; snprintf(m,mn,"L'eau glacée vous fouette le sang : ^1+10%% de vitesse^0."); } break;
    case 13: if(i==0){ if(chance(0.6f)){ int gg=(int)(frange(150,250)*R.s_gold); R.gold+=gg; snprintf(m,mn,"Un coffre plein de doublons : ^1+%d "IC_COIN"^0 !",gg); }
               else { float dd=R.hpmax*0.2f; R.hp=maxf(1,R.hp-dd); good=false; snprintf(m,mn,"Une murène jaillit de la cale : ^3-%d PV^0.",(int)dd); } }
             else { R.ev_dmg+=0.12f; R.ev_speed-=0.08f; snprintf(m,mn,"L'ancre pèse lourd, mais quels coups ! ^1+12%% de dégâts^0, ^3-8%% de vitesse^0."); } break;
    case 14: if(i==0){ int up=0; for(int k=0;k<R.nballs;k++) if(!IS_EVO(R.balls[k]) && R.ball_lvl[k]<3){ R.ball_lvl[k]++; up++; } R.pact_next=true;
               snprintf(m,mn,R.single?"Votre reflet vous cède sa force : ^1%d bille%s^0 gagne%s un niveau. La suite du niveau sera ^3plus dure^0.":"Votre reflet vous cède sa force : ^1%d bille%s^0 gagne%s un niveau. Le prochain niveau sera ^3plus dur^0.",up,up>1?"s":"",up>1?"nt":""); }
             else { float dd=R.hpmax*0.25f; R.hp=maxf(1,R.hp-dd); R.crystals+=2; good=false; snprintf(m,mn,"Le miroir éclate en mille morceaux : ^6+2 cristaux^0, ^3-%d PV^0.",(int)dd); } break;
    default: if(chance(0.5f)){ R.ev_hp+=20; snprintf(m,mn,"Délicieux ! ^3+20 PV max^0."); }
             else { R.ev_hp-=10; R.ev_speed+=0.12f; good=false; snprintf(m,mn,"Tout tourne… ^3-10 PV max^0, mais vous courez ^1plus vite^0."); } break;
  }
  run_recalc_stats(); sync_balls();
  sfx(good?SFX_UPGRADE:SFX_HURT);
  meta_check();
}
static void start_event(void);
/* v9 : fin du rush des gardiens */
static void rush_win(void){
  int d=clampi(R.dest0,0,DEST_COUNT-1), t=(int)R.total_time;
  R.won=true; S.rush_wins++; S.rush_dest|=1<<d;
  if(S.rush_best[d]==0 || t<S.rush_best[d]){ S.rush_best[d]=t; LAST.record=true; }
  if(t<360) meta_feat(FEAT_RUSH_FAST);
  R.sub=RS_WIN; R.sub_t=0; R.banner_t=0; R.levelup_flash=0; R.fade=0; music_play(MUS_TITLE,1.0f); sfx(SFX_REVIVE);
  meta_check();
}
void run_debug_event(int ev){ start_event(); R.ev_id=clampi(ev,0,EV_COUNT-1); { char lab[4][96]; bool en[4]; int n=run_event_options(lab,en); int f=0; while(f<n-1 && !en[f]) f++; ui_set_focus(f); } }
static void start_event(void){
  #define EV_OK(e) ((e)<12 || R.dest==1)
  int c[EV_COUNT], n=0; for(int e=0;e<EV_COUNT;e++) if(EV_OK(e) && !((R.ev_seen>>e)&1)) c[n++]=e;
  if(!n){ R.ev_seen=0; for(int e=0;e<EV_COUNT;e++) if(EV_OK(e)) c[n++]=e; }
  R.ev_id=c[irand(n)];
  if(R.dest==1){ int pc[3], np=0; for(int e=12;e<EV_COUNT;e++) if(!((R.ev_seen>>e)&1)) pc[np++]=e; if(np && chance(0.5f)) R.ev_id=pc[irand(np)]; }
  R.ev_seen|=1u<<R.ev_id;
  #undef EV_OK
  R.sub=RS_EVENT; R.ev_sel=0; R.ev_t=0; R.ev_result=-1; R.ev_msg[0]=0; R.ev_pending=false;
  { char lab[4][96]; bool en[4]; int n=run_event_options(lab,en); int f=0; while(f<n-1 && !en[f]) f++; ui_set_focus(f); }
  music_play(MUS_CAMP,1.0f); sfx(SFX_EVENT);
}

/* ---------------- mise a jour ---------------- */
static void update_pickups(float dt,bool vacuum){
  for(int i=0;i<MAX_PICK;i++){
    Pickup *p=&R.pick[i]; if(!p->on) continue;
    p->t+=dt;
    float dx=R.px-p->x, dy=(R.py-6)-p->y, d=sqrtf(dx*dx+dy*dy);
    if(vacuum || d<R.s_pick) p->magnet=true;
    if(p->magnet){
      float sp=180+minf(p->t,4.0f)*260; float st=sp*dt;
      if(d>st+7){ p->vx=dx/d*sp; p->vy=dy/d*sp; p->x+=p->vx*dt; p->y+=p->vy*dt; }
      else {
        p->on=false;
        switch(p->kind){
          case PK_GEM: case PK_GEM2: case PK_GEM3:
            R.xp+=p->val*R.s_xp; sfx_play(SFX_GEM,0.3f,0,1.0f+minf(R.combo,20)*0.03f);
            spawn_particles(R.px,R.py-10,2,p->kind==PK_GEM3?HEX(0xd090ff):(p->kind==PK_GEM2?HEX(0x9af05a):HEX(0x6ad8ff)),30,PT_GLOW,0.25f); break;
          case PK_COIN: R.gold_frac+=p->val*R.s_gold; { int g=(int)R.gold_frac; R.gold+=g; R.gold_frac-=g; } sfx_play(SFX_COIN,0.25f,0,frange(0.95f,1.1f)); break;
          case PK_HEART: heal(10); sfx(SFX_HEAL); break;
        }
      }
      continue;
    }
    p->vx*=1-2*dt; p->vy+=140*dt; if(p->vy>50) p->vy=50;
    p->x+=p->vx*dt; p->y+=p->vy*dt;
    if(p->x<PIT_X0+3){ p->x=PIT_X0+3; p->vx=fabsf(p->vx); } if(p->x>PIT_X1-3){ p->x=PIT_X1-3; p->vx=-fabsf(p->vx); }
    if(p->y>FLOOR_Y-3){ p->y=FLOOR_Y-3; p->vy=0; p->vx=0; if(p->t>14) p->on=false; }
  }
  while(R.xp>=R.xpnext){ R.xp-=R.xpnext; R.plevel++; R.xpnext=xp_need(R.plevel); R.pending_levels++; R.levelup_flash=1; if(R.plevel>S.best_plevel) S.best_plevel=R.plevel; }
}

static void update_enemies(float dt){
  float prog=clampf(R.ltime/R.lduration,0,1);
  float descend=BIOMES[R.biome].speed*diff_cur()->speed*(1+0.35f*prog)*depth_speed()*(1+0.08f*R.cycle)*(run_has_mod(MOD_RUEE)?1.25f:1.0f);
  for(int i=0;i<MAX_EN;i++){
    Enemy *e=&R.en[i]; if(!e->on) continue;
    const EnemyDef *d=&ENEMIES[e->kind];
    e->t+=dt; if(e->flash>0) e->flash-=dt; if(e->spawn<1) e->spawn=minf(1,e->spawn+dt*3);
    if(e->hpbar_t>0) e->hpbar_t-=dt; if(e->dot_show>0) e->dot_show-=dt;
    if(e->freeze_t>0) e->freeze_t-=dt; if(e->slow_t>0) e->slow_t-=dt; if(e->blind_t>0) e->blind_t-=dt;
    if(e->root_t>0) e->root_t-=dt; if(e->stun_t>0) e->stun_t-=dt;
    if(e->mark_t>0){ e->mark_t-=dt; if(e->mark_t<=0) e->mark_k=0; }
    if(e->acid>0){ e->acid_t-=dt; if(e->acid_t<=0) e->acid=0; else if(irand(16)==0) spawn_particles(e->x+frange(-4,4),e->y+e->h/2-2,1,HEX(0xd8ff40),10,PT_PIX,0.5f); }
    if(e->kick>0) e->kick=approachf(e->kick,0,dt*30);
    bool frozen=e->freeze_t>0 || e->stun_t>0;
    bool held=frozen || e->root_t>0;     /* enracine : ne bouge plus, mais peut encore tirer */
    /* statuts */
    if(e->burn>0){ enemy_dot(e,e->burn*3.0f*R.s_status*dt,HEX(0xff9a3a)); e->burn_t-=dt; if(e->burn_t<=0) e->burn=0; if(!e->on) continue; if(irand(8)==0) spawn_particles(e->x+frange(-4,4),e->y,1,HEX(0xff8a2a),15,PT_EMBER,0.5f); }
    if(e->poison>0){ enemy_dot(e,e->poison*1.5f*R.s_status*dt,HEX(0x9af05a)); e->poison_t-=dt; if(e->poison_t<=0) e->poison=0; if(!e->on) continue; if(irand(14)==0) spawn_particles(e->x+frange(-4,4),e->y,1,HEX(0x9af05a),10,PT_GLOW,0.5f); }
    if(e->bleed>0){ e->bleed_t-=dt; if(e->bleed_t<=0) e->bleed=0; if(irand(20)==0) spawn_particles(e->x,e->y+3,1,HEX(0xd8203a),8,PT_PIX,0.4f); }
    /* regeneration : seulement apres 1,5 s sans subir de degats (brulure et poison l'empechent) */
    e->calm_t+=dt;
    if((d->flags&EF_REGEN) && e->calm_t>1.5f){ e->hp=minf(e->hpmax,e->hp+e->hpmax*0.035f*dt); }
    if(e->champ==CHAMP_REGEN && e->calm_t>1.5f){ e->hp=minf(e->hpmax,e->hp+e->hpmax*0.03f*dt); }
    if(e->link_t>0) e->link_t-=dt;
    if(e->champ==CHAMP_SOIGNEUR && !frozen){ e->champ_t-=dt; if(e->champ_t<=0){ e->champ_t=1.0f; bool any=false;
        for(int k=0;k<MAX_EN;k++){ Enemy *o=&R.en[k]; if(!o->on||o==e) continue; if(sqrf(o->x-e->x)+sqrf(o->y-e->y)<56*56 && o->hp<o->hpmax){ o->hp=minf(o->hpmax,o->hp+o->hpmax*0.05f); any=true; if(!g_norender) spawn_particles(o->x,o->y,2,HEX(0x8be05a),20,PT_GLOW,0.5f); } }
        if(any) ring(e->x,e->y,52,HEX(0x8be05a),0.35f); } }
    if(e->champ==CHAMP_INVOCATEUR && !frozen && e->y>4 && e->y<ZONE_TOP-30 && R.sub==RS_PLAY){ e->champ_t-=dt;
      if(e->champ_t<=0){ e->champ_t=5.0f;
        if(count_enemies()<MAX_EN-40 && e->summoned<8){ const BiomeDef *bi=&BIOMES[R.biome];
          for(int k=0;k<2;k++){ Enemy *c=enemy_spawn(bi->pool[irand(bi->pool_n)],clampf(e->x+(k?12:-12),PIT_X0+8,PIT_X1-8),e->y+e->h/2+4,hp_scale()*0.45f); if(c){ c->minion=true; c->spawn=0; } }
          e->summoned+=2; ring(e->x,e->y,e->w,HEX(0xc08aff),0.3f); spawn_particles(e->x,e->y,6,HEX(0xc08aff),40,PT_GLOW,0.4f); sfx_at(SFX_SPLIT,e->x,0.25f); } } }
    /* invocateurs */
    if(e->mboss){ mboss_update(e,dt,frozen); continue; }
    if((d->flags&EF_SUMMON) && !frozen && !e->flee && d->child>=0 && e->y>4 && e->y<ZONE_TOP-30 && R.sub==RS_PLAY){
      e->summon_t-=dt;
      if(e->summon_t<=0){
        bool el=(d->flags&EF_ELITE)!=0; e->summon_t=el?4.5f:6.0f;
        if(count_enemies()<MAX_EN-40 && e->summoned<10){
          int n=el?2:1;
          for(int k=0;k<n;k++){ Enemy *c=enemy_spawn(d->child,clampf(e->x+frange(-12,12),PIT_X0+8,PIT_X1-8),e->y+e->h/2+4,hp_scale()*0.7f); if(c){ c->minion=true; c->spawn=0; } }
          e->summoned+=n; ring(e->x,e->y,e->w,d->blood,0.3f); spawn_particles(e->x,e->y+e->h/2,6,d->blood,40,PT_GLOW,0.4f); sfx_at(SFX_SPLIT,e->x,0.25f);
        }
      }
    }
    if(d->flags&EF_PHASE) e->phase+=dt;
    if(d->flags&EF_ORBS) e->orb+=2.2f*dt;
    if((d->flags&EF_HEAL) && !frozen){
      e->shoot-=dt;
      if(e->shoot<=0){ e->shoot=1.5f; bool any=false;
        for(int k=0;k<MAX_EN;k++){ Enemy *o=&R.en[k]; if(!o->on||o==e) continue; if(sqrf(o->x-e->x)+sqrf(o->y-e->y)<44*44 && o->hp<o->hpmax){ o->hp=minf(o->hpmax,o->hp+o->hpmax*0.06f); any=true; spawn_particles(o->x,o->y,3,HEX(0x8be05a),20,PT_GLOW,0.5f); } }
        if(any) ring(e->x,e->y,40,HEX(0x8be05a),0.4f);
      }
    }
    /* fuite devant le gardien : remonte et disparait, sans tirer ni percer */
    if(e->flee){
      e->y-=80*dt; e->push=0;
      if(irand(10)==0) spawn_particles(e->x,e->y+e->h/2,1,HEX(0x8a7aa0),20,PT_SMOKE,0.5f);
      if(e->y<-e->h){ e->on=false; }
      continue;
    }
    /* deplacement */
    float sp = e->guard ? 0 : descend*d->speed*(e->champ==CHAMP_RAPIDE?1.6f:1.0f)*(1-R.s_slow);
    if(held) sp=0; else if(e->slow_t>0) sp*=0.5f;
    /* plongee : intouchable et plus rapide pendant un instant */
    if((d->flags&EF_DIVE) && !held){
      e->special-=dt;
      if(e->sstate==0 && e->special<=0 && e->y>8 && e->y<ZONE_TOP-46){ e->sstate=1; e->special=1.5f; ring(e->x,e->y,10,HEX(0x9af0e0),0.3f); spawn_particles(e->x,e->y,6,HEX(0xc8fff4),40,PT_GLOW,0.35f); sfx_at(SFX_GHOST,e->x,0.18f); }
      else if(e->sstate==1){ sp*=1.9f; if(irand(3)==0) spawn_particles(e->x+frange(-4,4),e->y,1,HEX(0x9af0e0),12,PT_GLOW,0.4f);
        if(e->special<=0 || e->y>ZONE_TOP-34){ e->sstate=0; e->special=frange(2.2f,3.6f); ring(e->x,e->y,12,HEX(0xc8fff4),0.3f); spawn_particles(e->x,e->y,8,HEX(0xc8fff4),50,PT_SPARK,0.3f); } }
      else if(e->sstate==0 && e->special<=0) e->special=1.0f;
    }
    if((d->flags&EF_DASH) && !held && e->spawn>=1){
      e->special-=dt;
      if(e->sstate==0 && e->special<=0 && e->y>10 && e->y<ZONE_TOP-50){ e->sstate=1; e->special=0.55f; }
      else if(e->sstate==1){ sp=0; if(e->special<=0){ e->sstate=2; e->special=0.32f; sfx_at(SFX_DASH,e->x,0.3f); } }
      else if(e->sstate==2){ sp=descend*9; if(irand(2)==0) spawn_particles(e->x,e->y-e->h/2,1,HEX(0x9be05a),10,PT_GLOW,0.3f); if(e->special<=0){ e->sstate=0; e->special=frange(2.5f,4.5f); } }
      else if(e->sstate==0 && e->special<=0) e->special=1.0f;
    }
    if((d->flags&EF_BLINK) && !held){
      e->special-=dt;
      if(e->special<=0 && e->y>10){
        spawn_particles(e->x,e->y,8,HEX(0x63d6f2),50,PT_GLOW,0.35f);
        e->ang=clampf(e->ang+frange(-70,70),PIT_X0+10,PIT_X1-10); e->x=e->ang; e->y+=frange(-6,14);
        spawn_particles(e->x,e->y,8,HEX(0xd4f4ff),50,PT_GLOW,0.35f); sfx_at(SFX_BLINK,e->x,0.2f);
        e->special=frange(2.2f,3.8f);
      }
    }
    e->y += sp*dt + e->push*dt;
    e->push=approachf(e->push,0,320*dt);
    if((d->flags&EF_ZIGZAG) && !held){ e->x=e->ang+sinf(e->t*2.3f)*18; e->x=clampf(e->x,PIT_X0+e->w/2,PIT_X1-e->w/2); }
    if((d->flags&EF_DRIFT) && !held){ e->x+=e->vx*dt; if(e->x<PIT_X0+e->w/2){ e->x=PIT_X0+e->w/2; e->vx=fabsf(e->vx); } if(e->x>PIT_X1-e->w/2){ e->x=PIT_X1-e->w/2; e->vx=-fabsf(e->vx); } }
    if((d->flags&EF_BLINK) && !held) e->x=e->ang+sinf(e->t*1.7f)*6;
    if(e->kind==EK_ARAIGNEE && !held) e->x+=sinf(e->t*5)*8*dt;
    e->x=clampf(e->x,PIT_X0+e->w/2,PIT_X1-e->w/2);
    /* tirs */
    if((d->flags&(EF_SHOOT|EF_LOB|EF_FROST)) && !frozen && e->blind_t<=0 && e->y>6 && e->y<ZONE_TOP-20 && R.sub==RS_PLAY){
      e->shoot-=dt*(e->slow_t>0?0.5f:1);
      if(e->shoot<=0){
        e->shoot=d->shoot_cd*frange(0.85f,1.25f);
        float a=aim_at_player(e->x,e->y);
        switch(e->kind){
          case EK_CRANE: enemy_bullet(e->x,e->y+4,a,82,2.5f,HEX(0xeadfc4),0); break;
          case EK_CHAMPIGNON: for(int k=-1;k<=1;k++) enemy_bullet(e->x,e->y+4,a+k*0.33f,58,2.5f,HEX(0xf59ac0),0); break;
          case EK_SPORANGE: for(int k=0;k<8;k++) enemy_bullet(e->x,e->y,k*TAU/8+e->t,52,3,HEX(0xf59ac0),0); break;
          case EK_DIABLOTIN: enemy_bullet(e->x,e->y+4,a,100,3,HEX(0xff7a22),0); break;
          case EK_OEIL: for(int k=0;k<3;k++) enemy_bullet(e->x,e->y+4,a,88+k*16,2.5f,HEX(0xc83a5a),0); break;
          case EK_HORREUR: for(int k=-2;k<=2;k++) enemy_bullet(e->x,e->y+8,a+k*0.26f,70,3,HEX(0xc08aff),0); break;
          case EK_CRACHEUR: lava_lob(e->x,e->y-4); e->flash=0.08f; spawn_particles(e->x,e->y-6,6,HEX(0xff8a2a),50,PT_EMBER,0.4f); break;
          case EK_GARDIENNE: amber_lob(e->x,e->y-4); e->flash=0.08f; spawn_particles(e->x,e->y-6,6,HEX(0xffc040),50,PT_GLOW,0.4f); break;
          case EK_SORCIERE: lob(e->x,e->y-4,5); e->flash=0.08f; spawn_particles(e->x,e->y-6,6,HEX(0xb8d060),50,PT_GLOW,0.4f); break;
          case EK_FLEUR: for(int k=-1;k<=1;k++) enemy_bullet(e->x,e->y+4,a+k*0.3f,62,2.5f,HEX(0xc8ff8a),0); break;
          case EK_HYDRE: for(int k=-1;k<=1;k++) enemy_bullet(e->x+k*14,e->y+6,a+k*0.18f,76,3,HEX(0xb8d040),0); break;
          case EK_MINEUR: enemy_bullet(e->x,e->y+4,a,104,3,HEX(0x9ad8ff),0); break;
          case EK_PRETRE: for(int k=0;k<2;k++) enemy_bullet(e->x,e->y+4,a,84+k*18,2.5f,HEX(0xffd84a),0); break;
          case EK_SPHINX: for(int k=-2;k<=2;k++) enemy_bullet(e->x,e->y+8,a+k*0.22f,80,3,HEX(0xffc040),0); break;
          case EK_ESPRIT: enemy_bullet(e->x,e->y+4,a,78,2.5f,HEX(0x9ae8ff),3); break;
          case EK_YETI: for(int k=-1;k<=1;k++) enemy_bullet(e->x,e->y+8,a+k*0.28f,66,4,HEX(0xd4f4ff),3); break;
          case EK_OEIL_NEANT: for(int k=0;k<4;k++) enemy_bullet(e->x,e->y+4,a,84+k*14,2.5f,HEX(0xe0f0ff),0); break;
          case EK_BOURDON: for(int k=-2;k<=2;k++) enemy_bullet(e->x,e->y+8,a+k*0.22f,92,2.5f,HEX(0xffb030),0); break;
          case EK_COLOSSE_NEANT: for(int k=0;k<10;k++) enemy_bullet(e->x,e->y,k*TAU/10+e->t*0.7f,58,3,HEX(0xe0f0ff),0); break;
          default: enemy_bullet(e->x,e->y,a,80,2.5f,HEX(0xffffff),0); break;
        }
        sfx_at(SFX_ENEMY_SHOT,e->x,0.35f);
      }
    }
    /* contact joueur */
    if(e->y+e->h/2>ZONE_TOP-8){
      float hw=e->w*0.36f, hh=e->h*0.36f;   /* le corps peint est plus petit que la case de l'ennemi agrandi */
      float qx=clampf(R.px,e->x-hw,e->x+hw), qy=clampf(R.py-6,e->y-hh,e->y+hh);
      if(sqrf(qx-R.px)+sqrf(qy-(R.py-6))<5.5f*5.5f){ g_hurt_cat=2; player_hurt(d->contact*en_hit_scale()*(e->champ?1.5f:1.0f),false); }
    }
    /* breche */
    if(e->y+e->h/2>=FLOOR_Y){
      e->on=false;
      float dm=d->breach*en_hit_scale()*(e->champ?1.5f:1.0f)*(run_has_mod(MOD_FRAGILE)?2.0f:1.0f); if(e->guard) dm=0;
      if(dm>0){ player_hurt(dm,true); g_breach_kind[e->kind]++; }
      spawn_particles(e->x,FLOOR_Y,14,HEX(0xff4a4a),90,PT_SQ,0.5f);
      flash_light(e->x,FLOOR_Y,70,HEX(0xff3030),1.4f,0.4f);
      sfx_at(SFX_BREACH,e->x,0.6f); R.danger=1;
    }
  }
  /* explosions en chaine du pyromane */
  int n=nblast; float bx[64], by[64]; memcpy(bx,blast_x,sizeof(float)*n); memcpy(by,blast_y,sizeof(float)*n); nblast=0;
  for(int i=0;i<n;i++) explode(bx[i],by[i],20,(6+1.5f*run_diff())*(ch_rank(CH_PYROMANE)>=5?1.5f:1.0f),HEX(0xff7a2a),1,false);
  /* v9 : eclats d'ombre et de glace */
  n=ndark; float dx2[64], dy2[64]; int dk2[64]; memcpy(dx2,dark_x,sizeof(float)*n); memcpy(dy2,dark_y,sizeof(float)*n); memcpy(dk2,dark_k,sizeof(int)*n); ndark=0;
  for(int i=0;i<n;i++){
    if(dk2[i]==0){ explode(dx2[i],dy2[i],30,18,HEX(0x8a5aff),0,false); spawn_particles(dx2[i],dy2[i],10,HEX(0x2a1a3a),40,PT_SMOKE,0.7f); }
    else { for(int k=0;k<MAX_EN;k++){ Enemy *o=&R.en[k]; if(!o->on||o->guard) continue; if(sqrf(o->x-dx2[i])+sqrf(o->y-dy2[i])<30*30) o->freeze_t=maxf(o->freeze_t,(o->mboss||(ENEMIES[o->kind].flags&EF_ELITE))?0.5f:1.2f); }
      for(int k=0;k<2;k++) shard_new(dx2[i],dy2[i],0); ring(dx2[i],dy2[i],28,HEX(0xc8f0ff),0.3f); spawn_particles(dx2[i],dy2[i],12,HEX(0xe8f8ff),80,PT_SHARD,0.5f); }
  }
}

static void update_bullets(float dt){
  for(int i=0;i<MAX_BUL;i++){
    Bullet *b=&R.bul[i]; if(!b->on) continue;
    b->t+=dt; b->life-=dt; b->x+=b->vx*dt; b->y+=b->vy*dt;
    if(BUL_LOB(b)){
      bool amber=b->kind==4, vase=b->kind==5;
      float p=clampf(b->t/b->vz,0,1); b->tz=sinf(p*PI)*46;
      if(irand(3)==0) spawn_particles(b->x,b->y-b->tz,1,vase?HEX(0x9ab040):(amber?HEX(0xffc040):HEX(0xff8a2a)),12,(amber||vase)?PT_GLOW:PT_EMBER,0.4f);
      if(b->life<=0){ b->on=false;
        if(amber||vase){ Zone *z=zone_add(ZN_AMBER,b->tx,b->ty,17,4.5f); if(z) z->aux=vase; spawn_particles(b->tx,b->ty,12,vase?HEX(0x9ab040):HEX(0xffc040),70,PT_SPARK,0.35f); flash_light(b->tx,b->ty,36,vase?HEX(0x8aa030):HEX(0xffb030),0.9f,0.3f); sfx_at(SFX_SPLIT,b->tx,0.3f); }
        else { Zone *z=zone_add(ZN_LAVA,b->tx,b->ty,15,3.5f); if(z) z->tick=0.3f;
          spawn_particles(b->tx,b->ty,12,HEX(0xffa040),80,PT_SPARK,0.35f); flash_light(b->tx,b->ty,40,HEX(0xff7a22),1.0f,0.3f); sfx_at(SFX_EXPLODE,b->tx,0.25f); } }
      continue;
    }
    if(b->life<=0 || b->x<PIT_X0-10 || b->x>PIT_X1+10 || b->y<-30 || b->y>SCREEN_H+10){ b->on=false; continue; }
    if(sqrf(b->x-R.px)+sqrf(b->y-(R.py-6))<sqrf(b->r+3.5f)){ bool vuln=R.iframes<=0 && !R.shield_up; b->on=false; player_hurt(b->dmg,false); spawn_particles(b->x,b->y,6,b->col,60,PT_SPARK,0.3f);
      if(b->kind==3 && vuln){ R.chill_t=2.0f; spawn_particles(R.px,R.py-8,8,HEX(0xd4f4ff),40,PT_GLOW,0.4f); sfx_at(SFX_FREEZE,R.px,0.3f); } }
  }
}

static void update_zones(float dt){
  for(int i=0;i<MAX_ZONES;i++){
    Zone *z=&R.zones[i]; if(!z->on) continue;
    z->life-=dt; if(z->life<=0){ z->on=false; if(z->kind==ZN_ECHO) echo_burst(z->x,z->y,z->r,z->dmg,z->aux); if(z->kind==ZN_STAR) star_burst(z->x,z->y,z->r,z->dmg,z->aux); continue; }
    if(z->kind==ZN_RONCES || z->kind==ZN_ACIDE){
      bool ac=z->kind==ZN_ACIDE;
      z->tick-=dt; if(z->tick<=0){ z->tick=ac?0.5f:0.4f; uint32_t col=ac?HEX(0xe8ff70):HEX(0xa8d060);
        for(int k=0;k<MAX_EN;k++){ Enemy *e=&R.en[k]; if(!e->on) continue; if(sqrf(e->x-z->x)+sqrf((e->y-z->y)*1.4f)>sqrf(z->r+e->w/2)) continue;
          if(ac) acid_add(e,1,10); else { root_enemy(e,0.6f); e->bleed=clampi(e->bleed+1,0,10); e->bleed_t=maxf(e->bleed_t,3.0f); }
          enemy_dot(e,z->dmg*R.s_dmg,col); }
        if(ac && R.boss.on && R.boss.enter>=1 && sqrf(R.boss.x-z->x)+sqrf(R.boss.y-z->y)<sqrf(z->r+R.boss.w/2)) boss_acid(1,10);
      }
      if(irand(ac?6:9)==0) spawn_particles(z->x+frange(-z->r,z->r)*0.7f,z->y+frange(-3,3),1,ac?HEX(0xd8ff40):HEX(0x8ac850),10,ac?PT_GLOW:PT_PIX,0.5f);
      continue;
    }
    if(z->kind==ZN_STEAM || z->kind==ZN_FIRE){
      z->tick-=dt; if(z->tick<=0){ z->tick=0.25f;
        float dmg = z->kind==ZN_STEAM ? 3.5f : 3.0f; uint32_t col = z->kind==ZN_STEAM ? HEX(0xd0e0ff) : HEX(0xffa050);
        for(int k=0;k<MAX_EN;k++){ Enemy *e=&R.en[k]; if(!e->on) continue; if(sqrf(e->x-z->x)+sqrf(e->y-z->y)<sqrf(z->r+e->w/2)){ if(z->kind==ZN_STEAM) e->slow_t=0.5f; else { e->burn=clampi(e->burn+1,0,14); e->burn_t=maxf(e->burn_t,2.0f); } enemy_dot(e,dmg*R.s_dmg,col); } }
        if(R.boss.on && R.boss.enter>=1 && sqrf(R.boss.x-z->x)+sqrf(R.boss.y-z->y)<sqrf(z->r+R.boss.w/2)) boss_hurt(dmg*R.s_dmg*0.8f,col,false);
      }
      if(irand(3)==0) spawn_particles(z->x+frange(-z->r,z->r)*0.7f,z->y+frange(-z->r,z->r)*0.5f,1,z->kind==ZN_STEAM?HEX(0xe0e8f0):HEX(0xff8a2a),8,z->kind==ZN_STEAM?PT_SMOKE:PT_EMBER,0.7f);
    } else if(z->kind==ZN_AMBER){
      if(sqrf(R.px-z->x)+sqrf((R.py-4)-z->y)*2.2f<sqrf(z->r+3)) R.amber_t=0.15f;
      if(irand(9)==0) spawn_particles(z->x+frange(-z->r,z->r)*0.6f,z->y+frange(-3,3),1,z->aux?HEX(0xb8d060):HEX(0xffd060),8,PT_GLOW,0.6f);
    } else if(z->kind==ZN_LAVA){
      z->tick-=dt;
      if(z->tick<=0 && z->life<z->max-0.2f){ z->tick=0.45f; if(sqrf(R.px-z->x)+sqrf((R.py-4)-z->y)<sqrf(z->r+3)){ g_hurt_cat=4; player_hurt(4+1.1f*run_diff(),false); } }
      if(irand(5)==0) spawn_particles(z->x+frange(-z->r,z->r)*0.6f,z->y+frange(-3,3),1,HEX(0xffa040),14,PT_EMBER,0.5f);
    }
  }
}

static void update_player(float dt){
  float mx=A.movex, my=A.movey; float ml=sqrtf(mx*mx+my*my); if(ml>1){ mx/=ml; my/=ml; }
  float spd=R.s_speed*(R.chill_t>0?0.6f:1.0f)*(R.amber_t>0?0.55f:1.0f);
  if(R.chill_t>0) R.chill_t-=dt; if(R.amber_t>0) R.amber_t-=dt;
  R.px+=mx*spd*dt; R.py+=my*spd*dt;
  R.px=clampf(R.px,PIT_X0+12,PIT_X1-12); R.py=clampf(R.py,ZONE_TOP,ZONE_BOT);
  if(mx<-0.1f) R.facing=-1; else if(mx>0.1f) R.facing=1;
  /* compagnon : suit le heros a quelques pas */
  if(R.ch2>=0){ float tx=clampf(R.px-16*R.facing,PIT_X0+10,PIT_X1-10), ty=clampf(R.py+3,ZONE_TOP,ZONE_BOT+2); float dx=tx-R.comp_x, dy=ty-R.comp_y, d=sqrtf(dx*dx+dy*dy);
    if(d>3){ float sp=minf(d*5.0f,R.s_speed*1.5f); R.comp_x+=dx/d*sp*dt; R.comp_y+=dy/d*sp*dt; R.comp_walk+=dt; if(fabsf(dx)>2) R.comp_face=dx<0?-1:1; } }
  R.walk+=ml*dt*10; R.anim+=dt; if(ml>0.1f) R.walk_t+=dt*ml;
  if(R.iframes>0) R.iframes-=dt; if(R.hurt_t>0) R.hurt_t-=dt;
  if(R.shield_flash>0) R.shield_flash-=dt*3;
  if(R.thorn_cd>0) R.thorn_cd-=dt;
  if(R.s_regen>0 && R.hp<R.hpmax){ R.hp=minf(R.hpmax,R.hp+R.s_regen*heal_mult()*dt); }
  /* bouclier runique */
  if(run_pas_level(PS_BOUCLIER)>0 && !R.shield_up){ R.shield_cd-=dt; if(R.shield_cd<=0){ R.shield_up=true; sfx_play(SFX_SHIELD,0.4f,0,0.8f); ring(R.px,R.py-6,18,HEX(0x5ab8ff),0.3f); } }
  /* horlogere */
  if(has_ch(CH_HORLOGERE)){
    if(R.timeslow_t>0){ R.timeslow_t-=dt; }
    else { R.timeslow_cd-=dt; if(R.timeslow_cd<=0 && R.sub==RS_PLAY){ R.timeslow_t=3.0f; R.timeslow_cd=ch_rank(CH_HORLOGERE)>=5?11:14; sfx(SFX_TIME); shock_add(R.px,R.py-8,200,0.7f,1.0f); } }
  }
  /* visee : stick droit > auto (si active) > derniere direction (manette) / souris */
  if(A.aim_stick){
    R.aim_ang=atan2f(A.aimy,A.aimx); R.aim_hold=0.35f;
    R.aimx=R.px+A.aimx*120; R.aimy=(R.py-8)+A.aimy*120;
  } else if((A.pad||A.touch) && R.aim_hold>0){
    R.aim_hold-=dt; R.aimx=R.px+cosf(R.aim_ang)*120; R.aimy=(R.py-8)+sinf(R.aim_ang)*120;
  } else if(A.pad ? S.pad_autoaim : R.autoaim){
    float tx=R.px, ty=R.py-200; float best=-1e9f;
    if(R.boss.on && R.boss.enter>=1 && R.boss.dying<=0){ tx=R.boss.x; ty=R.boss.y; best=1e9f; }
    for(int i=0;i<MAX_EN && best<1e8f;i++){ Enemy *e=&R.en[i]; if(!e->on || e->y<4) continue; float sc=e->y - fabsf(e->x-R.px)*0.35f; if(sc>best){ best=sc; tx=e->x; ty=e->y; } }
    R.aimx=lerpf(R.aimx,tx,minf(1,dt*10)); R.aimy=lerpf(R.aimy,ty,minf(1,dt*10));
  } else if(A.pad||A.touch){ R.aimx=R.px+cosf(R.aim_ang)*120; R.aimy=(R.py-8)+sinf(R.aim_ang)*120; }
  else { p25_inv(A.mx,A.my,&R.aimx,&R.aimy); }   /* la souris vise dans la vue 2.5D */
  /* tir */
  R.fire_cd-=dt;
  if(R.fire_cd<=0){
    for(int k=0;k<MAX_BALLS;k++){
      int idx=(R.fire_idx+k)%MAX_BALLS; Ball *b=&R.balls_e[idx];
      if(b->on && !b->temp && b->state==BS_READY){ ball_launch(b); R.fire_idx=idx+1; R.fire_cd=R.s_fire; break; }
    }
    if(R.fire_cd<=0) R.fire_cd=0.02f;
  }
}

/* particules d'ambiance par biome */
static void update_ambient(float dt){
  if(g_norender) return;
  static float acc=0; acc+=dt*((R.biome==2||R.biome==4||R.biome==11)?26:(R.biome==8?12:16));
  while(acc>=1){
    acc-=1;
    Part *p=part_new(); p->kind=PT_MOTE; p->size=1; p->drag=0.2f;
    p->x=frange(PIT_X0+2,PIT_X1-2); p->life=p->max=frange(2.5f,5.0f);
    switch(R.biome){
      case 4: p->y=frange(-4,SCREEN_H); p->vx=frange(-10,4); p->vy=frange(14,30); p->c=chance(0.7f)?HEX(0xe8f4ff):HEX(0x9ae8ff); p->size=chance(0.3f)?2:1; break;
      case 5: p->y=frange(60,SCREEN_H+4); p->vx=frange(-3,3); p->vy=frange(-26,-10); p->c=chance(0.6f)?HEX(0x9af0e0):HEX(0x5ad0c8); p->size=chance(0.35f)?2:1; break;
      case 6: p->y=frange(0,SCREEN_H); p->vx=frange(-6,6); p->vy=frange(-6,6); p->c=chance(0.6f)?HEX(0xffd060):HEX(0xffa030); p->size=chance(0.25f)?2:1; break;
      case 7: p->y=frange(0,SCREEN_H); p->vx=frange(-4,4); p->vy=frange(-18,-6); p->c=chance(0.5f)?HEX(0xe0f0ff):HEX(0x8a7aff); p->size=chance(0.2f)?2:1; break;
      case 0: p->y=frange(0,SCREEN_H); p->vx=frange(-6,6); p->vy=frange(-4,6); p->c=HEX(0x8a84a0); break;
      case 1: p->y=frange(40,SCREEN_H); p->vx=frange(-5,5); p->vy=frange(-14,-4); p->c=chance(0.5f)?HEX(0xf59ac0):HEX(0x9af05a); p->size=chance(0.3f)?2:1; break;
      case 2: p->y=frange(200,SCREEN_H+4); p->vx=frange(-10,10); p->vy=frange(-60,-25); p->c=chance(0.7f)?HEX(0xff8a2a):HEX(0xffd36a); p->life=p->max=frange(1.5f,3.5f); break;
      case 8: p->y=frange(0,SCREEN_H); p->vx=frange(-6,6); p->vy=frange(-8,3); p->c=chance(0.7f)?HEX(0xeaff7a):HEX(0x9af8e8); p->size=chance(0.35f)?2:1; p->life=p->max=frange(3.0f,6.0f); break;
      case 9: p->y=frange(80,SCREEN_H+4); p->vx=frange(-4,4); p->vy=frange(-12,-4); p->c=chance(0.6f)?HEX(0xa8c060):HEX(0x6a8a3a); p->size=chance(0.3f)?2:1; break;
      case 10: p->y=frange(-4,SCREEN_H); p->vx=frange(-3,3); p->vy=frange(6,16); { int k=irand(3); p->c= k==0?HEX(0xff9ae0):(k==1?HEX(0x9af8ff):HEX(0xf0e8ff)); } p->size=chance(0.2f)?2:1; break;
      case 11: p->x=frange(PIT_X0-20,PIT_X1-30); p->y=frange(0,SCREEN_H); p->vx=frange(14,30); p->vy=frange(-3,4); p->c=chance(0.6f)?HEX(0xe8cc88):HEX(0xffd860); p->size=chance(0.25f)?2:1; break;
      default: p->y=frange(0,SCREEN_H); p->vx=frange(-8,8); p->vy=frange(-8,8); p->c=chance(0.6f)?HEX(0xa060ff):HEX(0x6a5aff); p->size=chance(0.25f)?2:1; break;
    }
  }
}

static void update_fx(float dt){
  for(int i=0;i<MAX_PART;i++){
    Part *p=&R.part[i]; if(p->life<=0) continue;
    p->life-=dt; p->vy+=p->grav*dt; float dr=1-p->drag*dt; if(dr<0) dr=0; p->vx*=dr; p->vy*=dr;
    if(p->kind==PT_MOTE) p->vx+=sinf(p->life*2.3f+p->y*0.05f)*12*dt;
    p->x+=p->vx*dt; p->y+=p->vy*dt;
    if(p->kind==PT_SHARD && p->y>FLOOR_Y+4 && p->vy>0){ p->vy*=-0.35f; p->vx*=0.6f; }
  }
  for(int i=0;i<MAX_TXT;i++){ FText *t=&R.txt[i]; if(!t->on) continue; t->life-=dt; t->age+=dt; t->y+=t->vy*dt; t->vy*=1-3*dt; if(t->life<=0) t->on=false; }
  for(int i=0;i<MAX_BOLTS;i++){ Bolt *b=&R.bolts[i]; if(!b->on) continue; b->life-=dt; if(b->life<=0) b->on=false; }
  for(int i=0;i<MAX_FLASH;i++){ Flash *f=&R.flashes[i]; if(!f->on) continue; f->life-=dt; if(f->life<=0) f->on=false; }
  for(int i=0;i<MAX_SHOCK;i++){ Shock *s=&R.shocks[i]; if(!s->on) continue; s->t+=dt; if(s->t>=s->max) s->on=false; }
  for(int i=0;i<MAX_BEAM;i++){ Beam *b=&R.beams[i]; if(!b->on) continue; b->life-=dt; if(b->life<=0) b->on=false; }
  if(R.shake>0) R.shake=approachf(R.shake,0,dt*18);
  if(R.banner_t>0) R.banner_t-=dt;
  if(R.levelup_flash>0) R.levelup_flash-=dt*2;
  if(R.danger>0) R.danger-=dt*1.5f;
  if(R.combo_t>0){ R.combo_t-=dt; if(R.combo_t<=0) R.combo=0; }
  if(R.hit_sfx_t>0) R.hit_sfx_t-=dt; if(R.bounce_sfx_t>0) R.bounce_sfx_t-=dt;
  if(R.tutorial_t>0 && R.sub==RS_PLAY) R.tutorial_t-=dt;
  if(R.hitstop_cd>0) R.hitstop_cd-=dt;
  /* barres "trainantes" */
  float hpf=clampf(R.hp/R.hpmax,0,1); if(R.hp_trail<hpf) R.hp_trail=hpf; else R.hp_trail=approachf(R.hp_trail,hpf,dt*0.35f);
  if(R.boss.on){ float bf=clampf(R.boss.hp/R.boss.hpmax,0,1); if(R.boss_trail<bf) R.boss_trail=bf; else R.boss_trail=approachf(R.boss_trail,bf,dt*0.25f); }
  else if(R.mb_on){ Enemy *e=mb_find(); if(e){ float bf=clampf(e->hp/e->hpmax,0,1); if(R.boss_trail<bf) R.boss_trail=bf; else R.boss_trail=approachf(R.boss_trail,bf,dt*0.25f); } }
  float xf=clampf(R.xp/R.xpnext,0,1); R.xp_show = xf<R.xp_show ? xf : approachf(R.xp_show,xf,dt*1.5f);
  if(R.hp<R.hpmax*0.3f) R.low_hp_t+=dt; else R.low_hp_t=0;
}

void run_pause(void){ if(R.sub==RS_PAUSE||R.sub==RS_EVOLIST) return; R.prev_sub=R.sub; R.sub=RS_PAUSE; R.pause_sel=0; pause_reset(); ui_set_focus(0); sfx(SFX_CLICK); }
void run_resume(void){ R.sub=R.prev_sub; if(R.sub==RS_PAUSE||R.sub==RS_EVOLIST) R.sub=RS_PLAY; sfx(SFX_CLICK); }

static void update_levelup(float dt){
  R.card_t+=dt;
  levelup_input(dt);
}

static void run_step(float dt);
/* vitesse de jeu : la simulation avance par pas identiques (x1.5 = un pas puis deux, x2 = deux pas) */
static const float SPEEDS[3]={1.0f,1.5f,2.0f};
float game_speed_mul(int k){ return SPEEDS[clampi(k,0,2)]; }
void speed_buttons_rect(int i,int *x,int *y,int *w,int *h){ *w=18; *h=18; *x=SCREEN_W-6-(3-i)*20; *y=4; }
float g_speed_msg=0;
static bool speed_state(void){ return R.sub==RS_PLAY || R.sub==RS_CLEAR || R.sub==RS_INTRO; }
void run_frame(float dt){
  if(dt>0.05f) dt=0.05f;
  if(g_speed_msg>0) g_speed_msg-=dt;
  if(speed_state()){
    int ns=S.game_speed;
    if(g_in && (g_in->pad_pressed[PAD_RB] || g_in->key_pressed[KEY_R])) ns=(S.game_speed+1)%3;
    if(A.click) for(int i=0;i<3;i++){ int x,y,w,h; speed_buttons_rect(i,&x,&y,&w,&h); if(mouse_in(x-1,y-1,w+2,h+2)) ns=i; }
    if(ns!=S.game_speed){ S.game_speed=ns; g_speed_msg=1.2f; sfx(SFX_CLICK); save_write(); }
  }
  static float acc=0;
  float sp = speed_state() ? SPEEDS[clampi(S.game_speed,0,2)] : 1.0f;
  if(sp<=1.0f){ acc=0; run_step(dt); return; }
  acc+=sp; int n=(int)acc; acc-=n; if(n<1) n=1;
  Actions saved=A;
  for(int i=0;i<n;i++){
    if(i>0){ /* les appuis ne comptent qu'une fois */
      A.confirm=A.back=A.pause=A.reroll=A.toggle_aim=false; for(int k=0;k<4;k++) A.n[k]=false;
      A.click=A.rclick=false; A.tab_l=A.tab_r=A.trig_l=A.trig_r=false; A.wheel=0; A.up=A.down=A.left=A.right=false; }
    int before=R.sub;
    run_step(dt);
    if(g_state!=GS_RUN || R.sub!=before || !speed_state()) break;
  }
  A=saved;
}
static void run_step(float dt){
  if(R.hitstop>0 && (R.sub==RS_PLAY||R.sub==RS_CLEAR||R.sub==RS_DEAD)){ R.hitstop-=dt; if(A.pause && R.sub==RS_PLAY) run_pause(); return; }
  float tf=tfac();
  R.scroll+=dt*BIOMES[R.biome].speed*(R.sub==RS_PLAY?1:0.3f)*tf;
  if(R.sub==RS_PLAY||R.sub==RS_INTRO||R.sub==RS_CLEAR) update_ambient(dt);
  switch(R.sub){
    case RS_INTRO:
      R.sub_t+=dt; update_player(dt); update_fx(dt);
      for(int i=0;i<MAX_BALLS;i++) if(R.balls_e[i].on) ball_update(&R.balls_e[i],dt);
      if(R.sub_t>1.6f) R.sub=RS_PLAY;
      if(A.pause) run_pause();
      break;
    case RS_PLAY: {
      R.total_time+=dt;
      if(!(R.single && (R.mb_on || R.mb_warned))) R.ltime+=dt;   /* le temps du niveau s'arrete face a un gardien mineur */
      if(R.boss.on || R.mb_on) R.boss_time+=dt;
      if(A.pause){ run_pause(); break; }
      if(A.toggle_aim){
        if(A.pad){ S.pad_autoaim=!S.pad_autoaim; add_ftext(R.px,R.py-30,S.pad_autoaim?"Visée auto":"Visée au stick",HEX(0xffd84a),1); }
        else { R.autoaim=!R.autoaim; S.autoaim=R.autoaim; add_ftext(R.px,R.py-30,R.autoaim?"Visée auto":(A.touch?"Visée au doigt":"Visée souris"),HEX(0xffd84a),1); }
        sfx(SFX_CLICK);
      }
      if(!R.boss_spawned){
        R.spawn_t-=dt*tf;
        float prog=R.ltime/R.lduration;
        /* gardiens mineurs : au tiers, puis aux deux tiers du niveau */
        if(R.single && R.seg<2 && !R.mb_on && R.ltime>=R.lduration*(R.seg+1)/3.0f){
          R.ltime=R.lduration*(R.seg+1)/3.0f;
          if(!R.mb_warned){ R.mb_warned=true; R.mb_warn=0; snprintf(R.banner,sizeof R.banner,"Un gardien mineur approche…"); R.banner2[0]=0; R.banner_t=2.2f; sfx(SFX_WARN); }
          R.mb_warn+=dt; if(R.mb_warn>2.4f && !mb_find()) spawn_miniboss();
        }
        bool hold= R.single && (R.mb_on || R.mb_warned);
        if(R.spawn_t<=0 && R.ltime<R.lduration && !hold){ spawn_row(prog);
          R.spawn_t = R.single ? lerpf(3.6f,2.5f,clampf(prog,0,1))*(1.0f-0.015f*R.lvl)*(curse_on(8)?0.9f:1.0f)
                               : lerpf(3.6f,2.6f,clampf(prog,0,1))*(1.0f-0.03f*R.level)*(R.cycle>0?0.9f:1.0f)*(curse_on(8)?0.9f:1.0f); }
        if(R.ltime>=R.lduration && !hold){
          if(R.boss_warn==0){ snprintf(R.banner,sizeof R.banner,"Le gardien approche…"); R.banner2[0]=0; R.banner_t=2.5f; sfx(SFX_WARN); if(R.single) hd_gpu_prefetch(R.biome+48); }
          R.boss_warn+=dt; if(R.boss_warn>2.6f) spawn_boss();
        }
      } else if(R.boss.on) R.boss_t+=dt;
      update_player(dt);
      for(int i=0;i<MAX_BALLS;i++) if(R.balls_e[i].on) ball_update(&R.balls_e[i],dt);
      update_enemies(dt*tf); if(R.sub!=RS_PLAY) break;
      boss_update(dt*tf); if(R.sub!=RS_PLAY) break;
      update_bullets(dt*tf); if(R.sub!=RS_PLAY) break;
      update_zones(dt); update_pickups(dt,false); curio_update(dt); update_fx(dt);
      if(has_ch(CH_ASTRONOME)){ R.astro_t-=dt*tf; if(R.astro_t<=0){ R.astro_t= ch_rank(CH_ASTRONOME)>=5 ? 5.0f : 7.0f;   /* v9 : pluie d'etoiles */
          Enemy *best=NULL; for(int i=0;i<MAX_EN;i++){ Enemy *e=&R.en[i]; if(!e->on || e->y<8 || e->y>ZONE_TOP) continue; if(!best || e->hp>best->hp) best=e; }
          if(best){ Zone *z=zone_add(ZN_STAR,best->x,best->y,20,0.32f); if(z){ z->dmg=16+4*run_diff(); z->aux=0; } }
          else if(R.boss.on && R.boss.enter>=1){ Zone *z=zone_add(ZN_STAR,R.boss.x,R.boss.y,20,0.32f); if(z){ z->dmg=16+4*run_diff(); z->aux=0; } } } }
      if(R.hunt>0){ R.hunt_t-=dt*tf; if(R.hunt_t<=0) R.hunt=0; }
      if(has_ch(CH_GEOMANCIEN)){ R.geo_t-=dt*tf; if(R.geo_t<=0){ R.geo_t= ch_rank(CH_GEOMANCIEN)>=5 ? 7.0f : 10.0f;   /* v10 : secousse */
          float dm=12+3*run_diff();
          for(int i=0;i<MAX_EN;i++){ Enemy *e=&R.en[i]; if(!e->on || e->y<0) continue; quake_hit(e,dm,1.0f); }
          if(R.boss.on && R.boss.enter>=1) boss_hurt(dm*2*R.s_dmg,HEX(0xffc070),false);
          shake(5); for(int k=0;k<4;k++) shock_add(PIT_X0+(k+0.5f)*PIT_W/4.0f,ZONE_TOP*0.5f,90,0.4f,1.2f);
          flash_light((PIT_X0+PIT_X1)*0.5f,ZONE_TOP*0.5f,PIT_W,HEX(0xffb050),1.0f,0.25f); sfx(SFX_BREACH); } }
      { static float mc=0; mc+=dt; if(mc>0.5f){ mc=0; meta_check(); } }
      if(R.pending_levels>0 && R.sub==RS_PLAY) open_levelup(false);
    } break;
    case RS_LEVELUP: update_levelup(dt); break;
    case RS_PAUSE: break;
    case RS_EVOLIST: if(A.back||A.pause||A.confirm||A.click){ R.sub=RS_PAUSE; sfx(SFX_CLICK); } break;
    case RS_EVENT:
      R.ev_t+=dt;
      if(R.ev_result>=0 && R.ev_t>0.4f && (A.confirm||A.click||A.back)){ R.fade=1; if(R.single) level_resume(); else level_begin(R.stage+1); sfx(SFX_CLICK); }
      break;
    case RS_CLEAR: {
      R.clear_t+=dt;
      if(A.pause && R.clear_t<9){ run_pause(); break; }
      update_player(dt);
      for(int i=0;i<MAX_BALLS;i++) if(R.balls_e[i].on) ball_update(&R.balls_e[i],dt);
      update_enemies(dt); boss_update(dt); update_zones(dt); update_pickups(dt,R.clear_t>0.8f); curio_update(dt); update_fx(dt);
      if(R.pending_levels>0 && R.clear_t>1.0f && R.clear_t<9){ open_levelup(false); break; }
      if(R.clear_t>2.6f && R.clear_t<9 && R.boss_spawned && !R.boss.on){ open_levelup(true); R.clear_t=5; break; }
      if(R.clear_t>2.6f && R.clear_t<9 && R.mb_reward){ R.mb_reward=false; open_levelup(true); R.clear_t=5; break; }
      if(R.clear_t>=10){ R.fade+=dt*2.5f; if(R.fade>=1){ R.fade=1; if(R.mode==MODE_RUSH){ if(R.level>=LEVELS_PER_DEST-1) rush_win(); else level_begin(R.stage+1); } else start_event(); } }
    } break;
    case RS_DEAD:
      R.dead_t+=dt; update_fx(dt);
      if(R.dead_t>2.6f) run_end_to_results(false);
      break;
    case RS_WIN:
      R.sub_t+=dt; update_fx(dt);
      if(irand(5)==0){ float x=frange(PIT_X0+20,PIT_X1-20), y=frange(40,200); spawn_particles(x,y,16,col_lerp(HEX(0xffd36a),HEX(0x9ae8ff),frand()),110,PT_SPARK,0.8f); ring(x,y,20,HEX(0xffffff),0.4f); }
      if(R.sub_t>4.5f || (R.sub_t>1.5f && (A.confirm||A.click))) run_end_to_results(true);
      break;
  }
  if(R.fade>0 && R.sub!=RS_CLEAR){ R.fade-=dt*2.5f; if(R.fade<0) R.fade=0; }
  (void)card_hint;
}

static void card_hint(int k,char *buf,int n){
  buf[0]=0;
  for(int e=BK_FIRST_EVO;e<=BK_LAST_EVO;e++){
    if(BALLS[e].evo_a==k || BALLS[e].evo_b==k){
      int o = BALLS[e].evo_a==k ? BALLS[e].evo_b : BALLS[e].evo_a;
      size_t l=strlen(buf); snprintf(buf+l,n-(int)l,"%s^6%s^0 + %s",l?"\n":"",BALLS[e].name,BALLS[o].name);
    }
  }
}
void run_card_text(Card *c,char *title,int tn,char *sub,int sn,char *desc,int dn,char *hint,int hn){
  hint[0]=0;
  switch(c->type){
    case CARD_NEWBALL: snprintf(title,tn,"%s",BALLS[c->kind].name); snprintf(sub,sn,"Nouvelle bille"); ball_desc(c->kind,1,desc,dn); card_hint(c->kind,hint,hn); break;
    case CARD_UPBALL: { int l=run_ball_level(c->kind); snprintf(title,tn,"%s",BALLS[c->kind].name); snprintf(sub,sn,"Niveau %d "IC_ARROW" %d",l,l+1); ball_desc(c->kind,l+1,desc,dn); if(l+1>=3) card_hint(c->kind,hint,hn); } break;
    case CARD_NEWPAS: snprintf(title,tn,"%s",PASSIVES[c->kind].name); snprintf(sub,sn,"Nouvel objet"); passive_desc(c->kind,1,desc,dn); break;
    case CARD_UPPAS: { int l=run_pas_level(c->kind); snprintf(title,tn,"%s",PASSIVES[c->kind].name); snprintf(sub,sn,"Niveau %d "IC_ARROW" %d",l,l+1); passive_desc(c->kind,l+1,desc,dn); } break;
    case CARD_EVO: snprintf(title,tn,"%s",BALLS[c->kind].name); snprintf(sub,sn,"ÉVOLUTION"); { char d2[200]; ball_desc(c->kind,1,d2,sizeof d2); snprintf(desc,dn,"Fusionne %s et %s.\n%s",BALLS[c->a].name,BALLS[c->b].name,d2); } break;
    case CARD_LEG: snprintf(title,tn,"%s",BALLS[c->kind].name); snprintf(sub,sn,"FUSION LÉGENDAIRE"); { char d2[260]; ball_desc(c->kind,1,d2,sizeof d2); snprintf(desc,dn,"Fusionne %s et %s.\n%s",BALLS[c->a].name,BALLS[c->b].name,d2); } break;
    case CARD_HEAL: snprintf(title,tn,"Repos"); snprintf(sub,sn,"Soin"); snprintf(desc,dn,"Récupère ^3%d%%^0 de vos PV.",curse_on(7)?15:30); break;
    case CARD_GOLD: snprintf(title,tn,"Bourse"); snprintf(sub,sn,"Or"); snprintf(desc,dn,"Gagne ^1%d "IC_COIN"^0.",(int)(25*R.s_gold)); break;
  }
}

/* fiche detaillee d'une carte (panneau de droite du choix d'amelioration) */
void run_card_detail(Card *c,char *head,int hn,char *name,int nn,char *dmg,int dn,char *desc,int ddn,int *rec,int *nrec){
  head[0]=name[0]=dmg[0]=desc[0]=0; *nrec=0;
  int k=c->kind;
  switch(c->type){
    case CARD_NEWBALL: case CARD_UPBALL: case CARD_EVO: case CARD_LEG: {
      int l0=run_ball_level(k), l= c->type==CARD_UPBALL ? l0+1 : 1;
      if(c->type==CARD_NEWBALL) snprintf(head,hn,"Nouvelle bille !");
      else if(c->type==CARD_UPBALL) snprintf(head,hn,"Niveau %d "IC_ARROW" %d",l0,l);
      else snprintf(head,hn,c->type==CARD_LEG?"Fusion légendaire !":"Évolution !");
      snprintf(name,nn,"%s",BALLS[k].name);
      float d=bdmg(k,l)*R.s_dmg; int d0=(int)(d+0.5f), d1=(int)(d*R.s_critm+0.5f);
      snprintf(dmg,dn,"%d-%d dégâts à l'impact",maxi(1,d0),maxi(1,d1));
      ball_desc(k,l,desc,ddn);
      { char *p=strstr(desc," Dégâts ^1"); if(p) *p=0; }   /* deja indique au-dessus */
      if(c->type==CARD_EVO||c->type==CARD_LEG){ rec[0]=c->a; rec[1]=c->b; rec[2]=k; *nrec=1; }
      else for(int e=BK_FIRST_EVO;e<=BK_LAST_EVO && *nrec<3;e++) if(BALLS[e].evo_a==k||BALLS[e].evo_b==k){ int o=BALLS[e].evo_a==k?BALLS[e].evo_b:BALLS[e].evo_a; rec[*nrec*3]=k; rec[*nrec*3+1]=o; rec[*nrec*3+2]=e; (*nrec)++; }
    } break;
    case CARD_NEWPAS: case CARD_UPPAS: {
      int l0=run_pas_level(k), l= c->type==CARD_UPPAS ? l0+1 : 1;
      if(c->type==CARD_NEWPAS) snprintf(head,hn,"Nouvel objet !"); else snprintf(head,hn,"Niveau %d "IC_ARROW" %d",l0,l);
      snprintf(name,nn,"%s",PASSIVES[k].name); passive_desc(k,l,desc,ddn);
    } break;
    case CARD_HEAL: snprintf(head,hn,"Un instant de répit"); snprintf(name,nn,"Repos"); snprintf(desc,ddn,"Récupère ^3%d%%^0 de vos PV.",curse_on(7)?15:30); break;
    default: snprintf(head,hn,"Une bourse oubliée"); snprintf(name,nn,"Bourse"); snprintf(desc,ddn,"Gagne ^1%d "IC_COIN"^0.",(int)(25*R.s_gold)); break;
  }
}
