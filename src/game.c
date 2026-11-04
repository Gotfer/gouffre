/* GOUFFRE - etats du jeu, commandes, sauvegarde, pilote automatique */
#include "game.h"
#include <time.h>

int g_state=GS_TITLE; float g_time=0; bool g_quit=false; int g_fs_request=-1;
uint32_t g_seed_override=0;   /* tests : graine fixe */
int g_dbg=0;                  /* tests : desactive des effets */
bool g_norender=false;        /* tests : simulation sans affichage */
bool g_skip_intro=false;      /* tests : pas d'introduction au demarrage */
float g_bot_skill=1.0f;       /* tests de rythme : multiplicateur d'habilete du pilote */
bool g_bot_endless=false;     /* tests : le pilote ne joue qu'en mode infini */
int g_force_hero=-1;          /* tests : heros impose (-1 : celui de la sauvegarde ; 99 : au hasard) */
bool g_bot_smart=false;       /* tests : le pilote choisit ses cartes comme un joueur raisonnable */
static int bot_card_score(const Card *c){
  int sc=0;
  switch(c->type){
    case CARD_EVO: sc=100; break;
    case CARD_LEG: sc=150; break;
    case CARD_UPBALL: sc=60; break;
    case CARD_NEWBALL: { sc=R.nballs<5?50:0;
      for(int e=BK_FIRST_EVO;e<=BK_LAST_EVO;e++){ int a=BALLS[e].evo_a, b=BALLS[e].evo_b; if((a==c->kind && run_ball_level(b)>0)||(b==c->kind && run_ball_level(a)>0)) sc+=15; } } break;
    case CARD_UPPAS: case CARD_NEWPAS: { int k=c->kind;
      bool good = k==PS_FORCE||k==PS_CADENCE||k==PS_LYNX||k==PS_COEUR||k==PS_RAGE||k==PS_FIOLE||k==PS_MIROIR||k==PS_SAC||k==PS_BOUCLIER||k==PS_LOUPE||k==PS_CAPE;
      sc=(good?40:22)-(c->type==CARD_NEWPAS?5:0); } break;
    case CARD_HEAL: sc=R.hp<R.hpmax*0.5f?70:5; break;
    default: sc=5; break;
  }
  return sc+irand(26);
}
Input *g_in=NULL;
Save S;

void menus_reset_background(void);
void charsel_enter(void);
void camp_enter(void);
Pick PICK={0,1,MODE_NORMAL};

/* ---------------- sauvegarde ---------------- */
void save_defaults(void){
  memset(&S,0,sizeof S);
  S.unlocked[0]=true; S.depth_max=1; S.depth_sel=1; S.last_char=0; S.depth_max2=1; S.depth_sel2=1; S.depth_max3=1; S.depth_sel3=1;
  S.vol_music=6; S.vol_sfx=8; S.vol_amb=6; S.pixel_perfect=true; S.shake=true; S.dmg_numbers=true; S.autoaim=false; S.fullscreen=false;
  S.pad_autoaim=true; S.rumble=true; S.bloom=1; S.quality=3; S.difficulty=DIFF_NORMALE; S.crt=true; S.view25=true;
  S.duo_last=-1; for(int L=0;L<BIOME_COUNT;L++) S.lvl_depth[L]=1;
}
void save_write(void){
  const char *p=plat_save_path(); if(!p) return;
  char tmp[600]; snprintf(tmp,sizeof tmp,"%s.tmp",p);
  FILE *f=fopen(tmp,"w"); if(!f) return;
  fprintf(f,"gouffre_sauvegarde=1\n");
  fprintf(f,"gold=%d\ncrystals=%d\n",S.gold,S.crystals);
  for(int b=0;b<BLD_COUNT;b++) fprintf(f,"bld%d=%d\n",b,S.bld[b]);
  for(int c=0;c<CH_COUNT;c++) fprintf(f,"hero%d=%d\n",c,S.unlocked[c]?1:0);
  fprintf(f,"depth_max=%d\ndepth_sel=%d\nlast_char=%d\nruns=%d\nwins=%d\nbest_level=%d\ntotal_kills=%d\n",
    S.depth_max,S.depth_sel,S.last_char,S.runs,S.wins,S.best_level,S.total_kills);
  fprintf(f,"vol_ambiances=%d\n",S.vol_amb);
  fprintf(f,"vol_music=%d\nvol_sfx=%d\nfullscreen=%d\npixel_perfect=%d\nshake=%d\ndmg_numbers=%d\nautoaim=%d\nevo_seen=%u\n",
    S.vol_music,S.vol_sfx,S.fullscreen,S.pixel_perfect,S.shake,S.dmg_numbers,S.autoaim,S.evo_seen);
  fprintf(f,"pad_autoaim=%d\nrumble=%d\nvitesse=%d\n",S.pad_autoaim,S.rumble,S.game_speed);
  fprintf(f,"version=11\n");
  for(int k=0;k<ACH_MAX/32;k++){ unsigned int m=0; for(int i=0;i<32;i++) if(S.ach[k*32+i]) m|=1u<<i; fprintf(f,"ach%d=%u\n",k,m); }
  fprintf(f,"balls_unlocked=%llu\npas_unlocked=%u\nballs_seen=%llu\nfeats=%u\nhero_wins=%u\n",(unsigned long long)S.balls_unlocked,S.pas_unlocked,(unsigned long long)S.balls_seen,S.feats,S.hero_wins);
  for(int k=0;k<EK_COUNT;k++) fprintf(f,"kills_en%d=%d\n",k,S.kills_en[k]);
  for(int k=0;k<BOSS_COUNT;k++) fprintf(f,"boss_kills%d=%d\n",k,S.boss_kills[k]);
  fprintf(f,"elites=%d\nevolutions=%d\nbest_combo=%d\nbest_plevel=%d\ngold_total=%d\nbosses_total=%d\n",S.elites,S.evolutions,S.best_combo,S.best_plevel,S.gold_total,S.bosses_total);
  fprintf(f,"events_done=%d\nendless_best=%d\ndaily_date=%d\ndaily_best=%d\ndaily_done=%d\ndaily_wins=%d\nmode_sel=%d\nbloom=%d\n",S.events_done,S.endless_best,S.daily_date,S.daily_best,S.daily_done,S.daily_wins,S.mode_sel,S.bloom);
  fprintf(f,"graphismes=%d\n",S.quality);
  /* v4 */
  fprintf(f,"depth_max2=%d\ndepth_sel2=%d\ndest_sel=%d\ndepth_won0=%d\ndepth_won1=%d\nprof_runs=%d\nprof_wins=%d\ncrystals_total=%d\nplay_time=%d\nchampions=%d\n",
    S.depth_max2,S.depth_sel2,S.dest_sel,S.depth_won[0],S.depth_won[1],S.prof_runs,S.prof_wins,S.crystals_total,S.play_time,S.champions);
  fprintf(f,"depth_max3=%d\ndepth_sel3=%d\ndepth_won2=%d\nroyaume_gagnes=%d\nroyaume_descentes=%d\n",S.depth_max3,S.depth_sel3,S.depth_won[2],S.roy_wins,S.roy_runs);
  fprintf(f,"curiosites=%d\ncuriosites_victimes=%d\nlegendaires=%d\n",S.curios,S.curio_kills,S.legendaries);
  for(int L=0;L<BIOME_COUNT;L++) if(S.obj_done[L]) fprintf(f,"objectifs%d=%d\n",L,S.obj_done[L]);
  fprintf(f,"rush_victoires=%d\nrush_destinations=%d\nrush_choix=%d\nmarques_vaincus=%d\n",S.rush_wins,S.rush_dest,S.rush_sel,S.mark_kills);
  for(int d=0;d<DEST_COUNT;d++) if(S.rush_best[d]) fprintf(f,"rush_record%d=%d\n",d,S.rush_best[d]);
  fprintf(f,"mutateurs=%u\nmutateurs_victoires=%d\nmutateurs_record=%d\nsecousses_victimes=%d\nchaines_victimes=%d\n",S.mut_sel,S.mut_wins,S.mut_best,S.quake_kills,S.link_kills);
  for(int c=1;c<CHAMP_COUNT;c++) if(S.champ_kinds[c]) fprintf(f,"champions_pouvoir%d=%d\n",c,S.champ_kinds[c]);
  for(int c=0;c<CH_COUNT;c++) fprintf(f,"hero_xp%d=%d\n",c,S.hero_xp[c]);
  for(int k=0;k<BK_COUNT;k++) if(S.ball_kills[k]) fprintf(f,"ball_kills%d=%d\n",k,S.ball_kills[k]);
  /* v4.1 */
  fprintf(f,"difficulte=%d\n",S.difficulty);
  for(int d=0;d<DIFF_COUNT;d++) fprintf(f,"victoires_diff%d=%d\n",d,S.diff_wins[d]);
  fprintf(f,"cathodique=%d\n",S.crt?1:0);
  fprintf(f,"vue25=%d\n",S.view25?1:0);
  fprintf(f,"intro_vue=%d\nfins_vues=%d\n",S.intro_seen?1:0,S.ends_seen);
  /* v6 */
  for(int L=0;L<BIOME_COUNT;L++){ for(int d=0;d<DEPTH_MAX;d++) if(S.lvl_done[L][d]) fprintf(f,"niv%d_%d=%u\n",L,d+1,(unsigned)S.lvl_done[L][d]);
    for(int df=0;df<DIFF_COUNT;df++) for(int d=0;d<DEPTH_MAX;d++) if(S.lvl_done_d[df][L][d]) fprintf(f,"nivd%d_%d_%d=%u\n",df,L,d+1,(unsigned)S.lvl_done_d[df][L][d]);
    fprintf(f,"niv_prof%d=%d\n",L,S.lvl_depth[L]); if(S.lvl_best[L]) fprintf(f,"niv_record%d=%d\n",L,S.lvl_best[L]); }
  fprintf(f,"niv_sel=%d\nniv_page=%d\nduo=%d\nniveaux_gagnes=%d\nmini_gardiens=%d\n",S.lvl_sel,S.lvl_page,S.duo_last,S.levels_won,S.mb_kills);
  /* v6.6 : prismes et entrainement des heros */
  fprintf(f,"prismes=%d\nprismes_total=%d\n",S.prisms,S.prisms_total);
  for(int c=0;c<CH_COUNT;c++){ int any=0; for(int k=0;k<TR_COUNT;k++) any|=S.train[c][k];
    if(any){ fprintf(f,"entr%d=",c); for(int k=0;k<TR_COUNT;k++) fprintf(f,k?",%d":"%d",(int)S.train[c][k]); fprintf(f,"\n"); } }
  fclose(f);
  remove(p);
  if(rename(tmp,p)!=0){ /* repli : ecriture directe */ FILE *g=fopen(p,"w"); if(g){ FILE *r=fopen(tmp,"r"); if(r){ int ch; while((ch=fgetc(r))!=EOF) fputc(ch,g); fclose(r);} fclose(g);} remove(tmp); }
}
void save_load(void){
  save_defaults();
  const char *p=plat_save_path(); if(!p) return;
  FILE *f=fopen(p,"r"); if(!f) return;
  char line[160]; int version=1; bool had_dw=false;
  int bk_raw[64]; memset(bk_raw,0,sizeof bk_raw);   /* maitrise des billes : numeros convertis apres lecture */
  while(fgets(line,sizeof line,f)){
    char *eq=strchr(line,'='); if(!eq || eq==line || eq-line>=63) continue;
    char key[64]; memcpy(key,line,(size_t)(eq-line)); key[eq-line]=0;
    unsigned long v=strtoul(eq+1,NULL,10);
    int iv=(int)strtol(eq+1,NULL,10); uint32_t uv=(uint32_t)v; uint64_t u64=(uint64_t)strtoull(eq+1,NULL,10);
    if(!strcmp(key,"gold")) S.gold=iv; else if(!strcmp(key,"crystals")) S.crystals=iv;
    else if(!strncmp(key,"bld",3)){ int b=atoi(key+3); if(b>=0&&b<BLD_COUNT) S.bld[b]=iv; }
    else if(!strcmp(key,"hero_wins")) S.hero_wins=uv;
    else if(!strncmp(key,"hero_xp",7)){ int c=atoi(key+7); if(c>=0&&c<CH_COUNT) S.hero_xp[c]=maxi(0,iv); }
    else if(!strncmp(key,"hero",4) && key[4]>='0' && key[4]<='9'){ int c=atoi(key+4); if(c>=0&&c<CH_COUNT) S.unlocked[c]=iv!=0; }
    else if(!strcmp(key,"depth_max")) S.depth_max=iv; else if(!strcmp(key,"depth_sel")) S.depth_sel=iv;
    else if(!strcmp(key,"last_char")) S.last_char=iv; else if(!strcmp(key,"runs")) S.runs=iv;
    else if(!strcmp(key,"wins")) S.wins=iv; else if(!strcmp(key,"best_level")) S.best_level=iv;
    else if(!strcmp(key,"total_kills")) S.total_kills=iv;
    else if(!strcmp(key,"vol_music")) S.vol_music=iv; else if(!strcmp(key,"vol_sfx")) S.vol_sfx=iv; else if(!strcmp(key,"vol_ambiances")) S.vol_amb=iv;
    else if(!strcmp(key,"fullscreen")) S.fullscreen=iv!=0; else if(!strcmp(key,"pixel_perfect")) S.pixel_perfect=iv!=0;
    else if(!strcmp(key,"shake")) S.shake=iv!=0; else if(!strcmp(key,"dmg_numbers")) S.dmg_numbers=iv!=0;
    else if(!strcmp(key,"autoaim")) S.autoaim=iv!=0; else if(!strcmp(key,"evo_seen")) S.evo_seen=uv;
    else if(!strcmp(key,"pad_autoaim")) S.pad_autoaim=iv!=0; else if(!strcmp(key,"rumble")) S.rumble=iv!=0;
    else if(!strcmp(key,"vitesse")) S.game_speed=clampi(iv,0,2);
    else if(!strcmp(key,"version")) version=iv;
    else if(!strncmp(key,"ach",3) && key[3]>='0' && key[3]<='9' && key[4]==0){ int k=key[3]-'0'; if(k<ACH_MAX/32) for(int i=0;i<32;i++) S.ach[k*32+i]=(uv>>i)&1; }
    else if(!strcmp(key,"balls_unlocked")) S.balls_unlocked=u64; else if(!strcmp(key,"pas_unlocked")) S.pas_unlocked=uv;
    else if(!strcmp(key,"balls_seen")) S.balls_seen=u64; else if(!strcmp(key,"feats")) S.feats=uv; else if(!strcmp(key,"hero_wins")) S.hero_wins=uv;
    else if(!strncmp(key,"kills_en",8)){ int k=atoi(key+8); if(k>=0&&k<EK_COUNT) S.kills_en[k]=iv; }
    else if(!strncmp(key,"boss_kills",10)){ int k=atoi(key+10); if(k>=0&&k<BOSS_COUNT) S.boss_kills[k]=iv; }
    else if(!strcmp(key,"elites")) S.elites=iv; else if(!strcmp(key,"evolutions")) S.evolutions=iv;
    else if(!strcmp(key,"best_combo")) S.best_combo=iv; else if(!strcmp(key,"best_plevel")) S.best_plevel=iv;
    else if(!strcmp(key,"gold_total")) S.gold_total=iv; else if(!strcmp(key,"bosses_total")) S.bosses_total=iv;
    else if(!strcmp(key,"events_done")) S.events_done=iv; else if(!strcmp(key,"endless_best")) S.endless_best=iv;
    else if(!strcmp(key,"daily_date")) S.daily_date=iv; else if(!strcmp(key,"daily_best")) S.daily_best=iv;
    else if(!strcmp(key,"daily_done")) S.daily_done=iv; else if(!strcmp(key,"daily_wins")) S.daily_wins=iv;
    else if(!strcmp(key,"mode_sel")) S.mode_sel=iv; else if(!strcmp(key,"bloom")) S.bloom=iv;
    else if(!strcmp(key,"graphismes")) S.quality=clampi(iv,0,3);
    else if(!strcmp(key,"depth_max2")) S.depth_max2=iv; else if(!strcmp(key,"depth_sel2")) S.depth_sel2=iv;
    else if(!strcmp(key,"dest_sel")) S.dest_sel=iv; else if(!strcmp(key,"depth_won0")) S.depth_won[0]=iv; else if(!strcmp(key,"depth_won1")) S.depth_won[1]=iv;
    else if(!strcmp(key,"depth_won2")) S.depth_won[2]=iv; else if(!strcmp(key,"depth_max3")) S.depth_max3=iv; else if(!strcmp(key,"depth_sel3")) S.depth_sel3=iv;
    else if(!strcmp(key,"royaume_gagnes")) S.roy_wins=maxi(0,iv); else if(!strcmp(key,"royaume_descentes")) S.roy_runs=maxi(0,iv);
    else if(!strcmp(key,"curiosites")) S.curios=maxi(0,iv); else if(!strcmp(key,"curiosites_victimes")) S.curio_kills=maxi(0,iv);
    else if(!strcmp(key,"legendaires")) S.legendaries=maxi(0,iv);
    else if(!strncmp(key,"objectifs",9)){ int L=atoi(key+9); if(L>=0&&L<BIOME_COUNT) S.obj_done[L]=(uint8_t)(iv&7); }
    else if(!strcmp(key,"rush_victoires")) S.rush_wins=maxi(0,iv); else if(!strcmp(key,"rush_destinations")) S.rush_dest=iv&7; else if(!strcmp(key,"rush_choix")) S.rush_sel=clampi(iv,0,DEST_COUNT-1);
    else if(!strcmp(key,"marques_vaincus")) S.mark_kills=maxi(0,iv);
    else if(!strcmp(key,"mutateurs")) S.mut_sel=uv&((1u<<MOD_COUNT)-1); else if(!strcmp(key,"mutateurs_victoires")) S.mut_wins=maxi(0,iv); else if(!strcmp(key,"mutateurs_record")) S.mut_best=maxi(0,iv);
    else if(!strcmp(key,"secousses_victimes")) S.quake_kills=maxi(0,iv); else if(!strcmp(key,"chaines_victimes")) S.link_kills=maxi(0,iv);
    else if(!strncmp(key,"champions_pouvoir",17)){ int c=atoi(key+17); if(c>0&&c<CHAMP_COUNT) S.champ_kinds[c]=maxi(0,iv); }
    else if(!strncmp(key,"rush_record",11)){ int d=atoi(key+11); if(d>=0&&d<DEST_COUNT) S.rush_best[d]=maxi(0,iv); }
    else if(!strcmp(key,"prof_runs")) S.prof_runs=iv; else if(!strcmp(key,"prof_wins")) S.prof_wins=iv;
    else if(!strcmp(key,"crystals_total")) S.crystals_total=iv; else if(!strcmp(key,"play_time")) S.play_time=iv; else if(!strcmp(key,"champions")) S.champions=iv;
    else if(!strncmp(key,"ball_kills",10)){ int k=atoi(key+10); if(k>=0&&k<64) bk_raw[k]=maxi(0,iv); }
    else if(!strcmp(key,"difficulte")) S.difficulty=clampi(iv,0,DIFF_COUNT-1);
    else if(!strcmp(key,"cathodique")) S.crt=iv!=0;
    else if(!strcmp(key,"vue25")) S.view25=iv!=0;
    else if(!strcmp(key,"intro_vue")) S.intro_seen=iv!=0; else if(!strcmp(key,"fins_vues")) S.ends_seen=iv&7;
    else if(!strncmp(key,"niv_prof",8)){ int L=atoi(key+8); if(L>=0&&L<BIOME_COUNT) S.lvl_depth[L]=clampi(iv,1,DEPTH_MAX); }
    else if(!strncmp(key,"niv_record",10)){ int L=atoi(key+10); if(L>=0&&L<BIOME_COUNT) S.lvl_best[L]=maxi(0,iv); }
    else if(!strcmp(key,"niv_sel")) S.lvl_sel=clampi(iv,0,BIOME_COUNT-1); else if(!strcmp(key,"niv_page")) S.lvl_page=clampi(iv,0,1);
    else if(!strcmp(key,"duo")) S.duo_last=clampi(iv,-1,CH_COUNT-1);
    else if(!strcmp(key,"niveaux_gagnes")) S.levels_won=maxi(0,iv); else if(!strcmp(key,"mini_gardiens")) S.mb_kills=maxi(0,iv);
    else if(!strncmp(key,"nivd",4) && key[4]>='0' && key[4]<='9'){ int df=atoi(key+4); const char *u=strchr(key,'_'); int L=u?atoi(u+1):-1; const char *u2=u?strchr(u+1,'_'):NULL; int d=u2?atoi(u2+1):0;
      if(df>=0&&df<DIFF_COUNT&&L>=0&&L<BIOME_COUNT&&d>=1&&d<=DEPTH_MAX) S.lvl_done_d[df][L][d-1]=(uint16_t)(uv&0xFFFF); }
    else if(!strncmp(key,"niv",3) && key[3]>='0' && key[3]<='9'){ int L=atoi(key+3); const char *u=strchr(key,'_'); int d=u?atoi(u+1):0; if(L>=0&&L<BIOME_COUNT&&d>=1&&d<=DEPTH_MAX) S.lvl_done[L][d-1]=(uint16_t)(uv&0xFFFF); }
    else if(!strcmp(key,"prismes")) S.prisms=maxi(0,iv); else if(!strcmp(key,"prismes_total")) S.prisms_total=maxi(0,iv);
    else if(!strncmp(key,"entr",4) && key[4]>='0' && key[4]<='9'){ int c=atoi(key+4); if(c>=0&&c<CH_COUNT){ const char *q=eq+1;
        for(int k=0;k<TR_COUNT && *q;k++){ S.train[c][k]=(uint16_t)clampi((int)strtol(q,NULL,10),0,TRAIN_CAP); q=strchr(q,','); if(!q) break; q++; } } }
    else if(!strncmp(key,"victoires_diff",14)){ int d=atoi(key+14); if(d>=0&&d<DIFF_COUNT){ S.diff_wins[d]=maxi(0,iv); had_dw=true; } }
  }
  if(version<2){ /* sauvegarde v1 : on deduit ce qu'on peut */
    for(int i=0;i<3;i++) if(S.best_level>i+1 && !S.boss_kills[i]) S.boss_kills[i]=1;
    if(S.wins>0 && !S.boss_kills[3]) S.boss_kills[3]=S.wins;
    S.bosses_total=0; for(int i=0;i<BOSS_COUNT;i++) S.bosses_total+=S.boss_kills[i];
  }
  if(version<4 && S.runs>0){ /* sauvegarde v3 : on offre aux heros une experience de depart selon le passe */
    int n=0; for(int c=0;c<CH_COUNT;c++) n+=S.unlocked[c];
    int share=mini(11400,(S.total_kills/2)/maxi(1,n));
    for(int c=0;c<CH_COUNT;c++) if(S.unlocked[c] && S.hero_xp[c]<share) S.hero_xp[c]=share;
    if(S.wins>0 && S.depth_won[0]<S.depth_max-1) S.depth_won[0]=maxi(1,S.depth_max-1);
  }
  if(version<5){ /* avant la v5.1 : deux fois plus d'ennemis, deux fois moins resistants -> compteurs convertis (meme progression) */
    S.total_kills/=2;
    for(int k=0;k<EK_COUNT;k++) if(!(ENEMIES[k].flags&EF_ELITE) && S.kills_en[k]>1) S.kills_en[k]=(S.kills_en[k]+1)/2;
    for(int k=0;k<64;k++) bk_raw[k]/=2;
  }
  if(version<6){ /* avant la v6 : les niveaux deja vaincus sont coches (heros gagnants, sinon le dernier joue) */
    int lc=clampi(S.last_char,0,CH_COUNT-1);
    uint16_t heroes=(uint16_t)(S.hero_wins&0xFFu); if(!heroes) heroes=(uint16_t)(1u<<lc);
    for(int L=0;L<BIOME_COUNT;L++){
      if(S.boss_kills[BIOMES[L].boss]<=0) continue;
      int dw=S.depth_won[L/LEVELS_PER_DEST];
      uint16_t m= dw>0 ? heroes : (uint16_t)(1u<<lc);
      for(int k=0;k<maxi(1,dw) && k<DEPTH_MAX;k++) S.lvl_done[L][k]|=m;
    }
    S.lvl_sel=0; for(int L=0;L<BIOME_COUNT;L++) if(lvl_unlocked(L)) S.lvl_sel=L;
    for(int L=0;L<BIOME_COUNT;L++) if(lvl_maxwon(L)>0) S.levels_won++;
  }
  if(version<7){ /* avant la v6.4 : une seule coche pour les trois difficultes -> on la range en Normale */
    memcpy(S.lvl_done_d[DIFF_NORMALE],S.lvl_done,sizeof S.lvl_done);
  }
  if(version<8){ /* avant la v6.6 : un prisme pour chaque gardien deja vaincu */
    S.prisms+=maxi(0,S.bosses_total); S.prisms_total+=maxi(0,S.bosses_total);
  }
  /* v7 : quatre billes et quatre evolutions de plus -> les numeros des anciennes ont bouge */
  if(version<9){
    uint64_t bu=0, bs=0; int nk[64]; memset(nk,0,sizeof nk);
    for(int k=0;k<33;k++){ int n=bk_old_to_new(k); if(n<0) continue;
      if((S.balls_unlocked>>k)&1) bu|=1ull<<n; if((S.balls_seen>>k)&1) bs|=1ull<<n; nk[n]+=bk_raw[k]; }
    S.balls_unlocked=bu; S.balls_seen=bs; memcpy(bk_raw,nk,sizeof nk);
  }
  /* v9 : quatre billes de base de plus, inserees avant les evolutions */
  if(version<10){
    uint64_t bu=0, bs=0; int nk[64]; memset(nk,0,sizeof nk);
    for(int k=0;k<41;k++){ int n=bk_v9_to_v10(k); if(n<0) continue;
      if((S.balls_unlocked>>k)&1) bu|=1ull<<n; if((S.balls_seen>>k)&1) bs|=1ull<<n; nk[n]+=bk_raw[k]; }
    S.balls_unlocked=bu; S.balls_seen=bs; memcpy(bk_raw,nk,sizeof nk);
  }
  /* v10 : trois billes de base, trois evolutions et une fusion de plus */
  if(version<11){
    uint64_t bu=0, bs=0; int nk[64]; memset(nk,0,sizeof nk);
    for(int k=0;k<55;k++){ int n=bk_v10_to_v11(k); if(n<0) continue;
      if((S.balls_unlocked>>k)&1) bu|=1ull<<n; if((S.balls_seen>>k)&1) bs|=1ull<<n; nk[n]+=bk_raw[k]; }
    S.balls_unlocked=bu; S.balls_seen=bs; memcpy(bk_raw,nk,sizeof nk);
    S.evo_seen=evo_seen_v10_to_v11(S.evo_seen);
  }
  for(int k=0;k<BK_COUNT;k++) S.ball_kills[k]=bk_raw[k];
  /* la progression (niveaux et profondeurs debloques) reste commune aux trois difficultes */
  for(int df=0;df<DIFF_COUNT;df++) for(int L=0;L<BIOME_COUNT;L++) for(int d=0;d<DEPTH_MAX;d++) S.lvl_done[L][d]|=S.lvl_done_d[df][L][d];
  fclose(f);
  if(!had_dw) S.diff_wins[DIFF_NORMALE]=S.wins;   /* avant la v4.1 : toutes les victoires etaient en Normale */
  S.unlocked[0]=true;
  for(int b=0;b<BLD_COUNT;b++) S.bld[b]=clampi(S.bld[b],0,BUILDINGS[b].max);
  S.mode_sel=clampi(S.mode_sel,0,MODE_COUNT-1);
  S.depth_max=clampi(S.depth_max,1,DEPTH_MAX); S.depth_sel=clampi(S.depth_sel,1,S.depth_max);
  S.depth_max2=clampi(S.depth_max2,1,DEPTH_MAX); S.depth_sel2=clampi(S.depth_sel2,1,S.depth_max2); S.dest_sel=clampi(S.dest_sel,0,DEST_COUNT-1);
  S.depth_max3=clampi(S.depth_max3,1,DEPTH_MAX); S.depth_sel3=clampi(S.depth_sel3,1,S.depth_max3);
  for(int d=0;d<DEST_COUNT;d++) S.depth_won[d]=clampi(S.depth_won[d],0,DEPTH_MAX);
  S.last_char=clampi(S.last_char,0,CH_COUNT-1); if(!S.unlocked[S.last_char]) S.last_char=0;
  S.vol_music=clampi(S.vol_music,0,10); S.vol_sfx=clampi(S.vol_sfx,0,10); S.vol_amb=clampi(S.vol_amb,0,10);
  if(S.gold<0) S.gold=0; if(S.crystals<0) S.crystals=0; if(S.prisms<0) S.prisms=0; if(S.prisms_total<S.prisms) S.prisms_total=S.prisms;
  for(int L=0;L<BIOME_COUNT;L++) S.lvl_depth[L]=clampi(S.lvl_depth[L],1,lvl_depth_max(L));
  if(S.duo_last>=0 && !S.unlocked[S.duo_last]) S.duo_last=-1;
  lvl_derive();
}

/* ---------------- commandes ---------------- */
static float rep[4]; static bool held_prev[4]; static bool pad_mode=false;
static bool nav(int i,bool held,bool fresh,float dt){
  if(fresh){ held_prev[i]=true; rep[i]=0.36f; return true; }   /* appui tres bref : compte quand meme */
  if(!held){ held_prev[i]=false; return false; }
  if(!held_prev[i]){ held_prev[i]=true; rep[i]=0.36f; return true; }
  rep[i]-=dt; if(rep[i]<=0){ rep[i]=0.085f; return true; }
  return false;
}
static float dz(float v){ if(fabsf(v)<0.22f) return 0; return (v-signf(v)*0.22f)/0.78f; }
static void compute_actions(Input *in,float dt){
  Actions a; memset(&a,0,sizeof a);
  bool padact=false;
  if(in->pad_on){ for(int i=0;i<PAD_COUNT;i++) if(in->pad_pressed[i]) padact=true; if(fabsf(in->lx)>0.5f||fabsf(in->ly)>0.5f||fabsf(in->rx)>0.5f||fabsf(in->ry)>0.5f) padact=true; }
  bool kbact=in->mouse_moved||in->mb_pressed[MOUSE_LEFT]; for(int i=0;i<KEY_COUNT;i++) if(in->key_pressed[i]) kbact=true;
  { static bool was_on=false; if(in->pad_on && !was_on) pad_mode=true; was_on=in->pad_on; }   /* manette branchee : on passe en mode manette */
  if(!in->pad_on) pad_mode=false;
  if(padact) pad_mode=true; else if(kbact) pad_mode=false;
  a.pad=pad_mode; a.pad_type=in->pad_type;
  bool up=in->key[KEY_UP]||in->pad[PAD_UP]||in->ly<-0.6f;
  bool down=in->key[KEY_DOWN]||in->pad[PAD_DOWN]||in->ly>0.6f;
  bool left=in->key[KEY_LEFT]||in->pad[PAD_LEFT]||in->lx<-0.6f;
  bool right=in->key[KEY_RIGHT]||in->pad[PAD_RIGHT]||in->lx>0.6f;
  a.up=nav(0,up,in->key_pressed[KEY_UP]||in->pad_pressed[PAD_UP],dt);
  a.down=nav(1,down,in->key_pressed[KEY_DOWN]||in->pad_pressed[PAD_DOWN],dt);
  a.left=nav(2,left,in->key_pressed[KEY_LEFT]||in->pad_pressed[PAD_LEFT],dt);
  a.right=nav(3,right,in->key_pressed[KEY_RIGHT]||in->pad_pressed[PAD_RIGHT],dt);
  a.confirm=in->key_pressed[KEY_ENTER]||in->key_pressed[KEY_SPACE]||in->pad_pressed[PAD_A];
  a.back=in->key_pressed[KEY_ESCAPE]||in->key_pressed[KEY_BACKSPACE]||in->pad_pressed[PAD_B];
  a.pause=in->key_pressed[KEY_ESCAPE]||in->key_pressed[KEY_P]||in->pad_pressed[PAD_START];
  a.reroll=in->key_pressed[KEY_R]||in->pad_pressed[PAD_Y];
  a.toggle_aim=in->key_pressed[KEY_SPACE]||in->key_pressed[KEY_TAB]||in->pad_pressed[PAD_Y]||in->pad_pressed[PAD_BACK];
  for(int i=0;i<4;i++) a.n[i]=in->key_pressed[KEY_1+i];
  a.wheel=in->wheel; a.tab_l=in->pad_pressed[PAD_LB]; a.tab_r=in->pad_pressed[PAD_RB]||in->key_pressed[KEY_TAB];
  { static bool pl=false, pr=false; bool l=in->pad_on && in->lt>0.6f, r=in->pad_on && in->rt>0.6f; a.trig_l=l&&!pl; a.trig_r=r&&!pr; pl=in->pad_on && in->lt>0.35f; pr=in->pad_on && in->rt>0.35f; }
  a.click=in->mb_pressed[MOUSE_LEFT]; a.rclick=in->mb_pressed[MOUSE_RIGHT]; a.mouse_down=in->mb[MOUSE_LEFT];
  a.mouse_moved=in->mouse_moved; a.mx=in->mx; a.my=in->my;
  a.movex=(right?1.f:0.f)-(left?1.f:0.f); a.movey=(down?1.f:0.f)-(up?1.f:0.f);
  if(in->key[KEY_LEFT]||in->key[KEY_RIGHT]||in->key[KEY_UP]||in->key[KEY_DOWN]||in->pad[PAD_LEFT]||in->pad[PAD_RIGHT]||in->pad[PAD_UP]||in->pad[PAD_DOWN]){
    a.movex=(float)((in->key[KEY_RIGHT]||in->pad[PAD_RIGHT])-(in->key[KEY_LEFT]||in->pad[PAD_LEFT]));
    a.movey=(float)((in->key[KEY_DOWN]||in->pad[PAD_DOWN])-(in->key[KEY_UP]||in->pad[PAD_UP]));
  } else if(in->pad_on){ a.movex=dz(in->lx); a.movey=dz(in->ly); }
  if(in->pad_on){ float m=sqrtf(in->rx*in->rx+in->ry*in->ry); if(m>0.35f){ a.aim_stick=true; a.aimx=in->rx/m; a.aimy=in->ry/m; } }
  /* au doigt : sticks virtuels analogiques */
  a.touch = in->touch && !pad_mode;
  if(a.touch){
    if(fabsf(in->tmx)>0.01f || fabsf(in->tmy)>0.01f){ a.movex=in->tmx; a.movey=in->tmy; }
    float m=sqrtf(in->tax*in->tax+in->tay*in->tay); if(in->taim && m>0.01f){ a.aim_stick=true; a.aimx=in->tax/m; a.aimy=in->tay/m; }
  }
  A=a;
}

/* ---------------- manette : noms des boutons, vibrations, messages ---------------- */
const char *btn(int b){
  if(A.pad_type==PADT_PS){
    switch(b){ case B_CONFIRM: return "^4"IC_PS_CROSS"^0"; case B_BACK: return "^3"IC_PS_CIRCLE"^0";
      case B_ALT: return "^2"IC_PS_TRI"^0"; case B_X: return "^9"IC_PS_SQUARE"^0"; case B_START: return "Options"; default: return "Créer"; }
  }
  switch(b){ case B_CONFIRM: return "^2A^0"; case B_BACK: return "^3B^0"; case B_ALT: return "^1Y^0"; case B_X: return "^4X^0";
    case B_START: return A.pad_type==PADT_NINTENDO?"+":"Start"; default: return A.pad_type==PADT_NINTENDO?"-":"Back"; }
}
void rumble(float strength,float seconds){ if(S.rumble && g_in && g_in->pad_on) plat_rumble(strength,seconds); }
static char g_toast[96]; static float g_toast_t=0;
void toast(const char *msg){ snprintf(g_toast,sizeof g_toast,"%s",msg); g_toast_t=2.8f; }
static void draw_toast(float dt){
  if(g_toast_t<=0) return; g_toast_t-=dt;
  int w=text_w(g_toast,1)+24, x=SCREEN_W/2-w/2;
  gfx_rect_a(x,4,w,18,HEX(0x0a0810),215); gfx_frame(x,4,w,18,HEX(0xc9a24a));
  text_draw(SCREEN_W/2,7,g_toast,HEX(0xf0e6d0),1,AL_CENTER,0);
}

/* ---------------- planche de sprites (tests) ---------------- */
static int g_sheet=0;
static void draw_sheet(void){
  gfx_clear(HEX(0x3a3444));
  if(g_sheet==1){
    int x=6, y=6;
    for(int id=0;id<SP_COUNT;id++){
      const Sprite *s=&SPR[id]; int cw=s->w*2+6, nfr=SPR_WALK[id][0].px?3:1;
      if(x+cw*nfr>SCREEN_W-4){ x=6; y+=40; }
      for(int f=0;f<nfr;f++){ const Sprite *ss= f==0?s:&SPR_WALK[id][f-1]; draw_spr_scaled(ss,x,y,2,0xFFFFFFFFu); x+=cw; }
      x+=4;
    }
  } else if(g_sheet==2){
    for(int k=0;k<BK_COUNT;k++){ int x=10+(k%12)*52, y=10+(k/12)*40; draw_spr_scaled(&SPR_BALL[k],x,y,2,0xFFFFFFFFu); art_draw_ball(x+44,y+16,BALLS[k].r,k,g_time,true); }
    for(int b=0;b<BLD_COUNT;b++) draw_spr(&SPR_BLD[b],10+b*62,140,0);
    for(int c=0;c<CH_COUNT;c++) draw_spr(&SPR_HUT[c],10+c*40,200,0);
  } else if(g_sheet==4){   /* v7 : le Royaume oublie (monstres puis gardiens, phase 1 et 2) */
    int x=6, y=6;
    for(int id=SP_LUCIOLE;id<=SP_SPHINX;id++){ const Sprite *s=&SPR[id]; if(x+s->w>SCREEN_W-4){ x=6; y+=58; } draw_spr(s,x,y,0); x+=s->w+6; }
    for(int b=0;b<4;b++) art_draw_boss(BOSS_ARBRE+b,80+b*160,290,g_time,0,0.5f,0,0,(int)(g_time*0.5f)&1);
  } else {
    for(int e=0;e<EV_COUNT;e++) art_draw_event(e,60+(e%6)*104,80+(e/6)*130,g_time);
  }
}

/* ---------------- etats ---------------- */
static int trans_target=-1, trans_dir=0; static float trans=0;
static bool pending_run=false; static int pend_ch=0, pend_ch2=-1, pend_depth=1, pend_mode=0, pend_level=0;
void start_run_request(int ch,int ch2,int depth,int mode,int level){ pending_run=true; pend_ch=ch; pend_ch2=ch2; pend_depth=depth; pend_mode=mode; pend_level=level; goto_state(GS_RUN); }
void goto_state(int s){ trans_target=s; if(trans_dir!=1) trans_dir=1; }
int g_prev_state=GS_TITLE;
static void enter_state(int s){
  g_prev_state=g_state; g_state=s; ui_set_focus(0);
  switch(s){
    case GS_TITLE: music_play(MUS_TITLE,1.0f); break;
    case GS_CAMP: camp_enter(); music_play(MUS_CAMP,1.0f); break;
    case GS_META: music_play(MUS_CAMP,1.0f); break;
    case GS_CHARSEL: charsel_enter(); music_play(MUS_TITLE,1.0f); break;
    case GS_LEVELS: levels_enter(); music_play(MUS_TITLE,1.0f); break;
    case GS_UPGRADE: upgrade_enter(); music_play(MUS_CAMP,1.0f); break;
    case GS_TRAINING: training_enter(); music_play(MUS_CAMP,1.0f); break;
    case GS_RESULTS: music_play(MUS_TITLE,0.85f); break;
    case GS_CINE: cine_enter(); break;
    case GS_RUN:
      if(pending_run){ pending_run=false; rng_seed(g_seed_override?g_seed_override:((uint32_t)time(NULL)^(uint32_t)(g_time*1000)^((uint32_t)S.runs*7919u))); run_start(pend_ch,pend_ch2,pend_depth,pend_mode,pend_level); }
      else if(g_prev_state==GS_META){   /* retour de l'encyclopedie / des succes : la musique de la descente reprend */
        if(R.sub==RS_EVENT) music_play(MUS_CAMP,1.0f);
        else if(R.boss.on || R.mb_on) music_play(MUS_BOSS,1.0f);
        else music_play(music_level(R.biome),R.cycle>0?1.04f:1.0f); }
      break;
    default: break;
  }
}

void game_init(void){
  gfx_init(); font_init();
  rng_seed(g_seed_override?g_seed_override:(uint32_t)time(NULL));
  art_init(); portraits_init(); audio_init(); save_load(); meta_check();
  music_levels_start(S.lvl_sel);   /* themes des niveaux, en tache de fond : le dernier niveau choisi d'abord */
  bld_sprites_refresh();   /* batiments du campement (maquettes) */
  icons_init();            /* icones realistes des billes et des objets (tache de fond) */
  g_state=GS_TITLE;
  if(!S.intro_seen && !g_skip_intro){ g_state=GS_CINE; cine_start(CINE_INTRO,GS_TITLE); }   /* premier lancement : la legende du Gouffre */
  else music_play(MUS_TITLE,1.0f);
  trans=1; trans_dir=-1;
}
const uint32_t *game_framebuffer(void){ return g_screen; }
bool game_should_quit(void){ return g_quit; }
int game_fullscreen_request(void){ int r=g_fs_request; g_fs_request=-1; return r; }
void game_fullscreen_changed(bool fs){ if(S.fullscreen!=fs){ S.fullscreen=fs; save_write(); } }
bool game_pixel_perfect(void){ return S.pixel_perfect; }
bool game_uses_gpu(void){ return g_gpu; }
bool game_wants_fullscreen_at_start(void){ return S.fullscreen; }
void game_focus_lost(void){ if(g_state==GS_RUN && (R.sub==RS_PLAY||R.sub==RS_INTRO)) run_pause(); }
void game_shutdown(void){ save_write(); }

void game_frame(Input *in,float dt){
  if(dt>0.1f) dt=0.1f; if(dt<0) dt=0;
  g_in=in; g_time+=dt; audio_frame();
  g_gpu = g_gpu_ok && S.quality>0; g_gpu_quality = S.quality-1;
  gpu_frame_begin();
  icons_frame();
  compute_actions(in,dt);
  if(trans_dir!=0){ Actions k=A; memset(&A,0,sizeof A); A.mx=k.mx; A.my=k.my; A.pad=k.pad; A.movex=k.movex; A.movey=k.movey; }
  if(in->key_pressed[KEY_F11]) g_fs_request = S.fullscreen?0:1;
  g_vol_music=S.vol_music/10.0f; g_vol_sfx=S.vol_sfx/10.0f; g_vol_amb=S.vol_amb/10.0f;
  gfx_target(&g_scr); gfx_noclip();
  if(g_sheet){ draw_sheet(); return; }
  switch(g_state){
    case GS_TITLE: title_frame(dt); break;
    case GS_CHARSEL: charsel_frame(dt); break;
    case GS_LEVELS: levels_frame(dt); break;
    case GS_UPGRADE: upgrade_frame(dt); break;
    case GS_TRAINING: training_frame(dt); break;
    case GS_CAMP: camp_frame(dt); break;
    case GS_RUN: run_frame(dt); gfx_noclip(); if(!g_norender || R.sub==RS_EVENT || R.sub==RS_PAUSE || R.sub==RS_EVOLIST) run_draw(); break;
    case GS_RESULTS: results_frame(dt); break;
    case GS_OPTIONS: options_frame(dt); break;
    case GS_META: meta_frame(dt); break;
    case GS_CINE: cine_frame(dt); break;
    case GS_HELP: help_frame(dt); break;
  }
  { char nm[40];   /* v9 : nappe d'ambiance selon l'ecran (le campement selon la saison, le niveau ou l'arene du gardien) */
    switch(g_state){
      case GS_CAMP: case GS_UPGRADE: case GS_TRAINING: case GS_META: snprintf(nm,sizeof nm,"campement_%d",hd_season()); amb_play(nm); break;
      case GS_RUN: snprintf(nm,sizeof nm,"niveau_%02d%s",clampi(R.biome,0,BIOME_COUNT-1)+1,hd_get_variant()==3?"_arene":""); amb_play(nm); break;
      case GS_OPTIONS: case GS_HELP: break;
      default: amb_play(NULL); break;
    } }
  gfx_target(&g_scr); gfx_noclip(); layer(LY_UI); gfx_noclip();
  { static bool prev_pad=false;
    if(in->pad_on && !prev_pad){ char m[96]; snprintf(m,sizeof m,"Manette connectée : %s",in->pad_name[0]?in->pad_name:"manette"); toast(m); }
    else if(!in->pad_on && prev_pad){ toast("Manette déconnectée"); if(g_state==GS_RUN && (R.sub==RS_PLAY||R.sub==RS_INTRO)) run_pause(); }
    prev_pad=in->pad_on; }
  draw_toast(dt);
  draw_meta_toast(dt);
  if(trans_dir==1){ trans+=dt*5; if(trans>=1){ trans=1; enter_state(trans_target); trans_dir=-1; } }
  else if(trans_dir==-1){ trans-=dt*4; if(trans<=0){ trans=0; trans_dir=0; } }
  if(trans>0){ if(g_gpu) GP.blur_all=maxf(GP.blur_all,clampf(trans*1.4f,0,1)*0.9f); gfx_darken(0,0,SCREEN_W,SCREEN_H,(int)(clampf(trans,0,1)*255)); }
  if(g_state!=GS_CINE) ui_cursor();
}

/* ---------------- scenes de test ---------------- */
void game_debug_scene(const char *name,int arg){
  trans=0; trans_dir=0;
  if(!strcmp(name,"charsel")){ S.crystals=3; S.unlocked[1]=true; S.depth_max=3;
    if(arg>=10){ S.boss_kills[BOSS_OEIL]=1; S.depth_max2=8; S.depth_sel2=8; S.dest_sel=1; S.hero_xp[0]=30000; S.hero_xp[1]=70000; arg-=10; } if(arg>0){ S.runs=3; S.wins=1; meta_check(); S.mode_sel=arg; } enter_state(GS_CHARSEL); }
  else if(!strcmp(name,"camp")){ S.gold=1234; S.crystals=2; S.bld[0]=3; S.bld[1]=2; S.bld[4]=1; S.bld[6]=1; S.unlocked[1]=S.unlocked[4]=true;
    if(arg>=1) for(int b=0;b<BLD_COUNT;b++) S.bld[b]= arg==1 ? (BUILDINGS[b].max+1)/2 : BUILDINGS[b].max;
    if(arg>=3) for(int c=0;c<CH_COUNT;c++) S.unlocked[c]=true;
    enter_state(GS_CAMP); }
  else if(!strcmp(name,"amelio")){ int b=arg%100, mode=arg/100; S.gold=60000; S.crystals=60;
    for(int q=0;q<BLD_COUNT;q++) S.bld[q]= mode==0 ? 0 : (mode==1 ? (BUILDINGS[q].max+1)/2 : (mode==2 ? BUILDINGS[q].max-1 : BUILDINGS[q].max));
    if(mode==4){ S.gold=300; S.crystals=0; for(int q=0;q<BLD_COUNT;q++) S.bld[q]=1; }
    upgrade_select(b); enter_state(GS_UPGRADE); }
  else if(!strcmp(name,"entrainement")){ /* arg : heros + 100 x mode (0 debut, 1 en cours, 2 tres entraine, 3 sans prisme, 4 heros verrouille) */
    int ch=arg%100, mode=arg/100;
    for(int c=0;c<CH_COUNT;c++) S.unlocked[c]= c!=CH_HORLOGERE && c!=CH_PYROMANE;
    if(mode!=4) S.unlocked[clampi(ch,0,CH_COUNT-1)]=true;   /* 4 : le heros choisi reste verrouille s'il l'etait */
    S.hero_xp[CH_SENTINELLE]=26000; S.hero_xp[CH_FORGERONNE]=9000;
    S.prisms= mode==3 ? 0 : (mode==0 ? 12 : 140); S.prisms_total=S.prisms+40; S.crystals=9;
    if(mode==1||mode==4){ static const uint8_t T[CH_COUNT][TR_COUNT]={{4,3,1,0,2,0},{0,2,2,0,0,1},{1,1,3,0,0,0},{2,5,0,0,3,0},{0,0,0,2,0,4},{3,0,0,0,0,0},{0,0,0,0,0,0},{0,0,0,0,0,0},{2,0,1,0,0,2},{0,1,0,3,1,0}};
      memcpy(S.train,T,sizeof T); }
    if(mode==2) for(int c=0;c<CH_COUNT;c++) for(int k=0;k<TR_COUNT;k++) S.train[c][k]=(uint16_t)(8+k*5+c);   /* paliers d'or, d'argent, arc-en-ciel */
    training_open(ch,GS_CAMP); trans=0; trans_dir=0; trans_target=-1; enter_state(GS_TRAINING); }
  else if(!strcmp(name,"options")){ options_open(GS_TITLE); trans=0; trans_dir=0; enter_state(GS_OPTIONS); }
  else if(!strcmp(name,"defis")){ S.total_kills=740; S.runs=4; meta_check(); meta_open(META_ACH,GS_TITLE); trans=0; trans_dir=0; enter_state(GS_META); }
  else if(!strcmp(name,"codex")){ for(int k=0;k<EK_COUNT;k+=2) S.kills_en[k]=3+k*20; for(int q=0;q<BOSS_COUNT;q++) S.boss_kills[q]=1+q*3; S.ball_kills[BK_FEU]=2600; S.ball_kills[BK_METEORE]=5200; S.evo_seen=0x55; meta_open(META_CODEX,GS_TITLE); trans=0; trans_dir=0; enter_state(GS_META); extern void meta_debug_tab(int); meta_debug_tab(arg); }
  else if(!strcmp(name,"journal")){ S.runs=40; S.wins=6; S.total_kills=60000; S.hero_xp[0]=20000; S.hero_xp[3]=70000; S.ball_kills[BK_FEU]=3000; S.ball_kills[BK_FER]=9000; S.depth_won[0]=4; S.depth_max=5; S.bld[0]=9; S.endless_best=7; for(int k=0;k<EK_COUNT;k+=3) S.kills_en[k]=200; meta_check(); meta_open(META_JOURNAL,GS_TITLE); trans=0; trans_dir=0; enter_state(GS_META); }
  else if(!strcmp(name,"niveaux")){ S.crystals=5; S.unlocked[1]=S.unlocked[3]=S.unlocked[7]=true;
    if(arg>=1){ S.lvl_done[0][0]=0x03; S.lvl_done[0][1]=0x01; S.lvl_done[1][0]=0x8A; S.lvl_done[2][0]=0x01; }
    if(arg>=2){ for(int L=0;L<6;L++) S.lvl_done[L][0]|=0x0B; S.boss_kills[BOSS_OEIL]=1; S.runs=5; S.wins=3; meta_check(); }
    if(arg>=1){ memcpy(S.lvl_done_d[DIFF_NORMALE],S.lvl_done,sizeof S.lvl_done); S.lvl_done_d[DIFF_FACILE][0][0]=0x01; }
    lvl_derive(); S.lvl_sel=clampi(arg>=2?4:1,0,BIOME_COUNT-1); S.lvl_page=arg>=3?1:0; enter_state(GS_LEVELS); }
  else if(!strcmp(name,"duo")){ for(int c=0;c<CH_COUNT;c++) S.unlocked[c]=true; S.duo_last=CH_PYROMANE; PICK.level=2; PICK.depth=1; PICK.mode=MODE_NORMAL; enter_state(GS_CHARSEL); extern void charsel_debug_duo(int); charsel_debug_duo(arg); }
  else if(!strcmp(name,"rush")){ pending_run=true; pend_ch=clampi(arg/10,0,CH_COUNT-1); pend_ch2=-1; pend_depth=1; pend_mode=MODE_RUSH; pend_level=clampi(arg%10,0,DEST_COUNT-1)*LEVELS_PER_DEST; S.unlocked[pend_ch]=true; enter_state(GS_RUN); }
  else if(!strcmp(name,"event")){ pending_run=true; pend_ch=0; pend_ch2=-1; pend_depth=1; pend_mode=0; pend_level=0; enter_state(GS_RUN); R.gold=150; R.ev_seen=0; extern void run_debug_event(int); run_debug_event(arg); }
  else if(!strcmp(name,"title")){ enter_state(GS_TITLE); }
  else if(!strcmp(name,"cine")){ cine_request(arg,GS_TITLE); trans_target=-1; trans_dir=0; trans=0; enter_state(GS_CINE); }
  else if(!strcmp(name,"sheet")){ g_sheet=1+arg; }
  else if(!strcmp(name,"padtest")){ extern void options_open_test(void); enter_state(GS_OPTIONS); options_open_test(); }
  else if(!strcmp(name,"results")){
    memset(&LAST,0,sizeof LAST); LAST.diff=S.difficulty; LAST.won=arg>0; LAST.level=3; LAST.kills=742; LAST.gold=388; LAST.crystals=2; LAST.time=812; LAST.plevel=17; LAST.ch=4;
    LAST.hxp=4321; LAST.rank0=3; LAST.rank1=arg>0?4:3; S.hero_xp[4]=26000; LAST.nstars=2; LAST.stars_k[0]=BK_FEU; LAST.stars_n[0]=2; LAST.stars_k[1]=BK_SINGULARITE; LAST.stars_n[1]=1; LAST.dest=1; LAST.biome=6; LAST.depth=7;
    LAST.nballs=4; LAST.balls[0]=BK_SINGULARITE; LAST.ball_lvl[0]=1; LAST.balls[1]=BK_FEU; LAST.ball_lvl[1]=3; LAST.balls[2]=BK_FOUDRE; LAST.ball_lvl[2]=2; LAST.balls[3]=BK_ECLAT; LAST.ball_lvl[3]=1;
    LAST.ch2=-1; LAST.lvl=-1; LAST.prisms=arg>0?3:0; S.prisms=17;
    if(arg>=2){ /* duo, niveau 3 termine : coches, niveau suivant debloque */
      LAST.ch2=CH_PYROMANE; LAST.hxp2=3240; LAST.rank0b=2; LAST.rank1b=2; S.hero_xp[CH_PYROMANE]=9000;
      LAST.lvl=2; LAST.biome=2; LAST.depth=1; LAST.dest=0; LAST.mode=MODE_NORMAL; LAST.mb_kills=2; LAST.time=468; LAST.kills=531; LAST.gold=246; LAST.crystals=1; LAST.nstars=0;
      LAST.new_checks=(1u<<4)|(1u<<CH_PYROMANE); LAST.check_gold=2*(40+20*2); LAST.next_unlocked=1; LAST.record_time=1; S.gold=1180; S.crystals=4; }
    enter_state(GS_RESULTS);
  }
  else if(!strcmp(name,"balls")){ pending_run=true; pend_ch=CH_PYROMANE; pend_ch2=-1; pend_depth=1; pend_mode=0; pend_level=0; for(int c=0;c<CH_COUNT;c++) S.unlocked[c]=true; enter_state(GS_RUN); extern void run_debug_balls(int); run_debug_balls(arg); }
  else if(!strncmp(name,"run",3) || !strcmp(name,"levelup") || !strcmp(name,"boss") || !strcmp(name,"prof") || !strcmp(name,"boss_prof") || !strcmp(name,"boss_roy") || !strcmp(name,"niveau") || !strcmp(name,"mboss")){
    bool prof=strstr(name,"prof")!=NULL, roy=strstr(name,"roy")!=NULL, boss=!strncmp(name,"boss",4);
    int lvl = arg; if(lvl<0) lvl=0; if(lvl>3 && strcmp(name,"run_endless") && strcmp(name,"niveau") && strcmp(name,"mboss")) lvl=3; if(lvl>11) lvl=11;
    if(prof||roy){ S.boss_kills[BOSS_OEIL]=maxi(1,S.boss_kills[BOSS_OEIL]); }
    pending_run=true; pend_ch= boss ? CH_MAGNETISEUR : (lvl%CH_COUNT); pend_ch2=-1; pend_depth=1; pend_mode=MODE_NORMAL; pend_level=(prof?LEVELS_PER_DEST:(roy?2*LEVELS_PER_DEST:0))+lvl;
    { const char *c2=getenv("GOUFFRE_DUO"); if(c2) pend_ch2=clampi(atoi(c2),-1,CH_COUNT-1); }
    { const char *hh=getenv("GOUFFRE_HERO"); if(hh) pend_ch=clampi(atoi(hh),0,CH_COUNT-1); }
    /* tests d'equilibrage : etat de progression type (batiments, rangs, billes) */
    { const char *mp=getenv("GOUFFRE_META"); if(mp){ int k=clampi(atoi(mp),0,4);
        static const int BL[5][BLD_COUNT]={{0},{3,3,2,2,2,1,0,1,0,0,1,1,1},{6,6,5,4,4,2,1,3,1,1,3,2,2},{9,9,8,7,6,3,2,5,2,2,5,4,3},{12,12,10,9,8,5,3,7,3,4,8,6,5}};
        static const int XP[5]={0,3000,20000,60000,140000};
        for(int b=0;b<BLD_COUNT;b++) S.bld[b]=mini(BL[k][b],BUILDINGS[b].max);
        for(int c=0;c<CH_COUNT;c++) S.hero_xp[c]=XP[k];
        if(k>=1){ S.balls_unlocked|=(1ull<<BK_SANG)|(1ull<<BK_CRISTAL)|(1ull<<BK_ECLAT); S.pas_unlocked|=(1u<<PS_MOUSSE)|(1u<<PS_FILET); }
        if(k>=2){ for(int b=BK_FIRST_BASE;b<=BK_LAST_BASE;b++) S.balls_unlocked|=1ull<<b; for(int q=0;q<PS_COUNT;q++) S.pas_unlocked|=1u<<q; }
        S.runs=50; } }
    if(!strcmp(name,"niveau") || !strcmp(name,"mboss")){ pend_level=clampi(arg,0,BIOME_COUNT-1); pend_ch=CH_SENTINELLE; { const char *hh=getenv("GOUFFRE_HERO"); if(hh) pend_ch=clampi(atoi(hh),0,CH_COUNT-1); } lvl=0; }
    { const char *dd=getenv("GOUFFRE_DEPTH"); if(dd) pend_depth=clampi(atoi(dd),1,DEPTH_MAX); }
    if(!strcmp(name,"run_endless")){ pend_mode=MODE_ENDLESS; pend_level=0; }
    if(!strcmp(name,"run_daily")) pend_mode=MODE_DAILY;
    if(!strncmp(name,"run_hero",8)){ pend_ch=lvl%CH_COUNT; pend_level=0; lvl=0; }
    for(int c=0;c<CH_COUNT;c++) S.unlocked[c]=true;
    enter_state(GS_RUN);
    if(lvl>0 && R.mode==MODE_ENDLESS){
      extern void run_debug_jump(int lvl); run_debug_jump(lvl);
    }
    if(!strcmp(name,"levelup")){ R.xp=R.xpnext+0.5f; }
    if(!strcmp(name,"mboss")){ R.ltime=R.lduration/3.0f-0.5f; }
    if(boss){ extern void run_debug_jump_end(void); run_debug_jump_end(); }
  }
}

/* ---------------- pilote automatique (tests) ---------------- */
static float bot_t=0, bot_act=0; static int bot_step=0;
bool g_bot_pad=false;
static void bot_press(Input *in,int key,int pb){
  if(g_bot_pad && pb>=0){ in->pad_pressed[pb]=true; in->pad[pb]=true; }
  else if(key>=0){ in->key_pressed[key]=true; in->key[key]=true; }
}
static void bot_move(Input *in){
  float tx=R.px, ty=ZONE_BOT-8; float push=0; bool threat=false;
  for(int i=0;i<MAX_BUL;i++){ Bullet *b=&R.bul[i]; if(!b->on) continue; float dx=b->x-R.px, dy=b->y-(R.py-6);
    if(BUL_LOB(b)) continue;
    if(dy<10 && dy>-90 && fabsf(dx)<22 && b->vy>0){ push+=(dx>0?-1.f:1.f)*(1.2f-fabsf(dx)/22); threat=true; } }
  for(int i=0;i<MAX_EN;i++){ Enemy *e=&R.en[i]; if(!e->on) continue; if(e->y+e->h/2>R.py-40 && fabsf(e->x-R.px)<16){ push+=(e->x>R.px?-1.f:1.f); threat=true; } }
  if(R.boss.on && R.boss.laser_st>=1 && fabsf(R.boss.laser_x-R.px)<18){ push+=(R.boss.laser_x>R.px?-2.f:2.f); threat=true; }
  for(int i=0;i<MAX_ZONES;i++){ Zone *z=&R.zones[i]; if(z->on && z->kind==ZN_WARN && fabsf(z->x-R.px)<16){ push+=(z->x>R.px?-2.f:2.f); threat=true; } }
  for(int i=0;i<MAX_ZONES;i++){ Zone *z=&R.zones[i]; if(z->on && (z->kind==ZN_LAVA||z->kind==ZN_AMBER) && fabsf(z->x-R.px)<z->r+8 && fabsf(z->y-R.py)<z->r+8){ push+=(z->x>R.px?-1.5f:1.5f); threat=true; } }
  for(int i=0;i<MAX_BUL;i++){ Bullet *b=&R.bul[i]; if(b->on && BUL_LOB(b) && fabsf(b->tx-R.px)<20 && fabsf(b->ty-R.py)<20){ push+=(b->tx>R.px?-1.f:1.f); threat=true; } }
  if(threat){ tx=R.px+push*40; if(tx<PIT_X0+12) tx=PIT_X0+40; if(tx>PIT_X1-12) tx=PIT_X1-40; }
  else {
    float bd=1e9f;
    for(int i=0;i<MAX_PICK;i++){ Pickup *p=&R.pick[i]; if(!p->on||p->magnet) continue; float d=fabsf(p->x-R.px)+fabsf(p->y-R.py)*0.5f; if(p->y>ZONE_TOP-60 && d<bd){ bd=d; tx=p->x; ty=clampf(p->y,ZONE_TOP,ZONE_BOT); } }
    if(bd>1e8f) tx=(PIT_X0+PIT_X1)*0.5f+sinf(bot_t*0.4f)*90;
    if(R.ch==CH_MAGNETISEUR){ float best=1e9f; for(int i=0;i<MAX_BALLS;i++){ Ball *b=&R.balls_e[i]; if(!b->on||b->state!=BS_FLY||b->temp||b->vy<=0||b->y<180) continue; float d=FLOOR_Y-b->y; if(d<best){ best=d; tx=b->x; ty=ZONE_BOT-6; } } }
  }
  if(g_bot_pad){
    float dx=tx-R.px, dy=ty-R.py;
    in->lx = fabsf(dx)>3 ? clampf(dx/20,-1,1) : 0; in->ly = fabsf(dy)>3 ? clampf(dy/20,-1,1) : 0;
    /* une moitie du temps : visee au stick droit vers l'ennemi le plus bas */
    if(fmodf(bot_t,8.0f)<4.0f){
      float ex=R.px, ey=R.py-200, best=-1e9f;
      for(int i=0;i<MAX_EN;i++){ Enemy *e=&R.en[i]; if(!e->on||e->y<4) continue; if(e->y>best){ best=e->y; ex=e->x; ey=e->y; } }
      float ax=ex-R.px, ay=ey-(R.py-8), m=sqrtf(ax*ax+ay*ay); if(m>1){ in->rx=ax/m; in->ry=ay/m; }
    }
    return;
  }
  if(tx<R.px-3) in->key[KEY_LEFT]=true; if(tx>R.px+3) in->key[KEY_RIGHT]=true;
  if(ty<R.py-3) in->key[KEY_UP]=true; if(ty>R.py+3) in->key[KEY_DOWN]=true;
}
static bool bot_upg=false, bot_tr=false;
void game_bot_input(Input *in,float dt){
  bot_t+=dt; bot_act-=dt;
  in->mouse_inside=false;
  switch(g_state){
    case GS_TITLE: if(bot_act<=0){
        if(g_force_hero>=0){ int h=g_force_hero==99?irand(CH_COUNT):g_force_hero%CH_COUNT; if(S.unlocked[h]) S.last_char=h; }
        ui_set_focus(0); bot_press(in,KEY_ENTER,PAD_A); bot_act=1.0f; } break;
    case GS_CHARSEL: if(bot_act<=0){ bot_press(in,KEY_ENTER,PAD_A); bot_act=1.0f; } break;
    case GS_LEVELS: if(bot_act<=0){
        /* comme un joueur : le niveau le plus profond ouvert, parfois un niveau deja vu ; la profondeur la plus forte */
        extern bool mode_unlocked(int m);
        if(g_bot_endless && mode_unlocked(MODE_ENDLESS)){ S.lvl_page=1; S.lvl_sel=0; S.depth_sel=1; }
        else { int n=lvl_count_unlocked(); int L=n-1; if(n>1 && chance(0.35f)) L=irand(n); S.lvl_page=0; S.lvl_sel=L; S.lvl_depth[L]=lvl_depth_max(L);
          if(mode_unlocked(MODE_ENDLESS) && chance(0.08f)){ S.lvl_page=1; S.lvl_sel=0; } }
        bot_press(in,KEY_ENTER,PAD_A); bot_act=1.0f; } break;
    case GS_RUN:
      if(!g_bot_pad) R.autoaim=true;
      if(R.sub==RS_LEVELUP){
        static int want=-1;
        if(R.card_t>0.45f && bot_act<=0){
          if(g_bot_smart && (want<0 || want>=R.ncards)){ int best=-1; for(int i=0;i<R.ncards;i++){ int sc=bot_card_score(&R.cards[i]); if(sc>best){ best=sc; want=i; } } }
          if(want<0 || want>=R.ncards){
            want=irand(R.ncards);
            for(int i=0;i<R.ncards;i++) if(R.cards[i].type==CARD_EVO||R.cards[i].type==CARD_LEG) want=i;
            if(R.cards[want].type==CARD_NEWPAS && R.nballs<3) for(int i=0;i<R.ncards;i++) if(R.cards[i].type==CARD_NEWBALL||R.cards[i].type==CARD_UPBALL) want=i;
          }
          if(g_bot_pad){
            if(R.card_sel<want) bot_press(in,-1,PAD_RIGHT); else if(R.card_sel>want) bot_press(in,-1,PAD_LEFT); else { bot_press(in,-1,PAD_A); want=-1; }
            bot_act=0.12f;
          } else { in->key_pressed[KEY_1+want]=true; want=-1; bot_act=0.2f; }
        }
      } else if(R.sub==RS_PLAY||R.sub==RS_CLEAR||R.sub==RS_INTRO) bot_move(in);
      else if(R.sub==RS_EVENT && bot_act<=0){
        if(R.ev_result<0){ char lab[4][96]; bool en[4]; int n=run_event_options(lab,en); int want=0; for(int i=0;i<n;i++) if(en[i]){ want=i; if(chance(0.6f)) break; }
          if(ui_focus_index()<want) bot_press(in,KEY_DOWN,PAD_DOWN); else if(ui_focus_index()>want) bot_press(in,KEY_UP,PAD_UP); else bot_press(in,KEY_ENTER,PAD_A); bot_act=0.25f; }
        else { bot_press(in,KEY_ENTER,PAD_A); bot_act=0.6f; }
      }
      else if(R.sub==RS_WIN && R.sub_t>2 && bot_act<=0){ bot_press(in,KEY_ENTER,PAD_A); bot_act=1; }
      else if(R.sub==RS_PAUSE && bot_act<=0){ bot_press(in,KEY_ESCAPE,PAD_B); bot_act=0.5f; }
      break;
    case GS_RESULTS: if(bot_act<=0){ bot_press(in,KEY_ENTER,PAD_A); bot_act=1.5f; bot_step=0; } break;
    case GS_UPGRADE:   /* comme avant au campement : un essai d'amelioration par batiment, puis retour */
      if(bot_act<=0 && !upgrade_busy()){
        bot_step++; bot_act=0.12f;
        if(bot_step<24){ if(bot_step&1) bot_press(in,KEY_RIGHT,PAD_RIGHT); else bot_press(in,KEY_ENTER,PAD_A); }
        else { bot_press(in,KEY_ESCAPE,PAD_B); bot_step=0; bot_act=0.6f; }
      }
      break;
    case GS_TRAINING:   /* entrainement : quelques essais sur les attributs (et un autre heros), puis retour */
      if(bot_act<=0 && !training_busy()){
        bot_step++; bot_act=0.12f;
        if(bot_step<36){ int r=bot_step%3; if(r==1) bot_press(in,KEY_ENTER,PAD_A); else if(r==2) bot_press(in,KEY_DOWN,PAD_DOWN); else if(bot_step%12==0) bot_press(in,KEY_RIGHT,PAD_RIGHT); }
        else { bot_press(in,KEY_ESCAPE,PAD_B); bot_step=0; bot_act=0.6f; }
      }
      break;
    case GS_CAMP:
      for(int c=0;c<CH_COUNT;c++) if(!S.unlocked[c] && !CHARS[c].unlock_ach && S.crystals>=CHARS[c].cost){ S.crystals-=CHARS[c].cost; S.unlocked[c]=true; }
      if(bot_act<=0){
        bot_act=0.12f;
        if(!bot_upg){ bot_upg=true; bot_step=0; bot_press(in,KEY_R,PAD_Y); bot_act=0.6f; }
        else if(!bot_tr){ bot_tr=true; bot_step=0; bot_press(in,KEY_T,PAD_X); bot_act=0.6f; }
        else { bot_upg=false; bot_tr=false; int n=0, list[CH_COUNT]; for(int c=0;c<CH_COUNT;c++) if(S.unlocked[c]) list[n++]=c; S.last_char=list[irand(n)];
          S.duo_last=-1; if(n>1 && chance(0.6f)){ int o=list[irand(n)]; if(o!=S.last_char) S.duo_last=o; }
          /* comme un joueur : la profondeur la plus basse debloquee, parfois le mode infini */
          S.dest_sel = dest_unlocked(1) && chance(0.5f) ? 1 : 0;
          *depth_sel_of(S.dest_sel)=g_bot_endless?1:depth_max_of(S.dest_sel); S.mode_sel=MODE_NORMAL;
          extern bool mode_unlocked(int m); if(mode_unlocked(MODE_ENDLESS) && (g_bot_endless || chance(0.15f))) S.mode_sel=MODE_ENDLESS;
          bot_press(in,KEY_E,PAD_START); bot_step=0; bot_act=1.0f; }
      }
      break;
    case GS_OPTIONS: case GS_META: case GS_HELP: if(bot_act<=0){ bot_press(in,KEY_ESCAPE,PAD_B); bot_act=0.5f; } break;
    case GS_CINE: in->key[KEY_ESCAPE]=true; break;   /* maintenir pour passer */
  }
}
