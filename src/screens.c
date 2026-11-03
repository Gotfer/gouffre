/* GOUFFRE - liste des niveaux : bannieres peintes, coches des heros, profondeur, modes de jeu */
#include "game.h"

void camp_world(float dt,float amb);
void bg_banner(int biome,Sprite *out,int y0);
bool mode_unlocked(int m);
int  ach_for_reward(int rw,int val);
bool dest_unlocked(int d);

#define LS_X   112
#define LS_W   416
#define LS_TOP 50
#define LS_BOT 360
#define BAN_H  80          /* banniere repliee */
#define SEL_H  128         /* banniere du niveau choisi */
#define BAND   30          /* le bandeau d'informations commence ici */
#define NPAGE1 3           /* page 2 : mode infini, defi du jour, rush des gardiens (v9) */
static int page1_mode(int i){ return i==0?MODE_ENDLESS:(i==1?MODE_DAILY:MODE_RUSH); }
static const char *page1_name(int i){ return i==0?"MODE INFINI":(i==1?"DÉFI DU JOUR":"RUSH DES GARDIENS"); }
static const char *DEST_NAMES[3]={"le Gouffre","les Profondeurs","le Royaume oublié"};

static Sprite ban[BIOME_COUNT+NPAGE1];
static bool ban_ok=false;
static float ls_scroll=0, ls_h[BIOME_COUNT]; static int ls_ret=GS_TITLE; static float ls_t=0;
static int ls_last_page=-1;

static int page_n(int pg){ return pg ? NPAGE1 : BIOME_COUNT; }

void levels_build_banners(void){
  if(ban_ok) return;
  for(int b=0;b<BIOME_COUNT;b++){ if(!ban[b].px) ban[b]=spr_alloc(LS_W,SEL_H); bg_banner(b,&ban[b],40+b*37); }
  /* mode infini : une tranche de chaque niveau, cote a cote */
  if(!ban[BIOME_COUNT].px) ban[BIOME_COUNT]=spr_alloc(LS_W,SEL_H);
  for(int y=0;y<SEL_H;y++) for(int x=0;x<LS_W;x++){ int k=(x*BIOME_COUNT)/LS_W; uint32_t c=ban[k].px[y*LS_W+x];
    if((x*BIOME_COUNT)%LS_W<BIOME_COUNT) c=HEX(0x0a0608); ban[BIOME_COUNT].px[y*LS_W+x]=col_lerp(c,HEX(0x3a1a5a),0.25f); }
  /* v9 : rush des gardiens : les niveaux en biais, teinte de braise */
  if(!ban[BIOME_COUNT+2].px) ban[BIOME_COUNT+2]=spr_alloc(LS_W,SEL_H);
  for(int y=0;y<SEL_H;y++) for(int x=0;x<LS_W;x++){ int xs=x+(SEL_H-y)/2; int k=((xs*BIOME_COUNT)/(LS_W+SEL_H/2))%BIOME_COUNT; uint32_t c=ban[k].px[y*LS_W+x];
    if(((xs*BIOME_COUNT)%(LS_W+SEL_H/2))<BIOME_COUNT+1) c=HEX(0x0a0608); ban[BIOME_COUNT+2].px[y*LS_W+x]=col_lerp(c,HEX(0x5a1010),0.32f)|0xFF000000u; }
  ban_ok=true;
}
/* defi du jour : le niveau du jour, teinte doree (refaite a chaque visite : le niveau change chaque jour) */
static void daily_banner(void){
  if(!ban[BIOME_COUNT+1].px) ban[BIOME_COUNT+1]=spr_alloc(LS_W,SEL_H);
  int dl=daily_level_today();
  for(int i=0;i<LS_W*SEL_H;i++) ban[BIOME_COUNT+1].px[i]=col_lerp(ban[dl].px[i],HEX(0xffc040),0.18f)|0xFF000000u;
}

void levels_enter(void){
  levels_build_banners(); daily_banner();
  if(g_prev_state==GS_TITLE) ls_ret=GS_TITLE; else if(g_prev_state==GS_CAMP||g_prev_state==GS_RESULTS) ls_ret=GS_CAMP;
  S.lvl_page=clampi(S.lvl_page,0,1); if(S.lvl_page==1 && !mode_unlocked(MODE_ENDLESS) && !mode_unlocked(MODE_DAILY) && !mode_unlocked(MODE_RUSH)) S.lvl_page=0;
  S.lvl_sel=clampi(S.lvl_sel,0,page_n(S.lvl_page)-1);
  for(int i=0;i<BIOME_COUNT;i++) ls_h[i]=BAN_H;
  ls_last_page=-1; ls_t=0;
}

/* bord dechire en haut de chaque banniere */
static void torn_edge(int x,int y,int w,int seed){
  for(int i=0;i<w;i++){ uint32_t h=(uint32_t)(i/3+seed*977)*2654435761u; h^=h>>15; int d=(int)(h%4);
    gfx_vline(x+i,y,y+d,HEX(0x0a0608)); if(d<3) gfx_pset(x+i,y+d+1,HEX(0x5a3a2a)); }
}
static void medallion(int cx,int cy,int kind,bool lit){
  gfx_disc(cx,cy,19,HEX(0x0a0608)); gfx_disc(cx,cy,18,lit?HEX(0x8a5a2e):HEX(0x4a3a40)); gfx_disc(cx,cy,16,HEX(0x2a1a24));
  gfx_ring(cx,cy,17,lit?HEX(0xc89458):HEX(0x6a5a60));
  const Sprite *s=&SPR_LO[ENEMIES[kind].sprite]; DrawOpt o=dopt(); o.flags=DS_NOOCC;
  float sc=s->w>26?1.0f:(s->w>14?1.4f:2.0f); o.sx=o.sy=sc; if(!lit){ o.flash=HEX(0x1a1018); o.flash_a=200; }
  draw_spr_c(s,(float)cx,(float)cy+1,&o);
}
static const char *mode_lock_text(int m){ static char b[96]; int a=ach_for_reward(RW_MODE,m); snprintf(b,sizeof b,"Défi « %s » requis",a>=0?ACHS[a].name:"?"); return b; }

/* bandeau du niveau choisi : coches des heros, profondeur, duree, bouton Jouer */
static bool level_band(int i,int x,int y,int w,int h,int page){
  bool play=false; char b[160];
  gfx_rect_a(x,y,w,h,HEX(0x08050a),190); gfx_hline(x,x+w-1,y,HEX(0x5a3a2a));
  bool locked = page==0 ? !lvl_unlocked(i) : !mode_unlocked(page1_mode(i));
  const char *nm = page==0 ? lvl_short(i) : page1_name(i);
  text_draw(x+w/2,y+6,nm,HEX(0xfff4ec),2,AL_CENTER,TX_OUTLINE);
  if(locked){
    orn_padlock(x+w/2,y+48);
    if(page==0) snprintf(b,sizeof b,"Terminez « %s » pour ouvrir ce niveau.",BIOMES[maxi(0,lvl_prereq(i))].name); else snprintf(b,sizeof b,"%s",mode_lock_text(page1_mode(i)));
    text_draw(x+w/2,y+66,b,HEX(0xc8b8c0),1,AL_CENTER,TX_SHADOW);
    return false;
  }
  /* profondeur (L1 / R1) */
  int *dsp=NULL, dmax=1;
  if(page==0){ dsp=&S.lvl_depth[i]; dmax=lvl_depth_max(i); }
  else if(i==0){ dsp=&S.depth_sel; dmax=maxi(1,S.depth_max); }
  if(dsp){ *dsp=clampi(*dsp,1,dmax);
    int cy=y+30, cx=x+w/2+6;
    snprintf(b,sizeof b,"Profondeur %d",*dsp); text_draw(cx,cy+2,b,HEX(0xf6eae0),1,AL_CENTER,TX_SHADOW);
    int tw=text_w(b,1);
    if(dmax>1){ int lx=cx-tw/2-30, rx=cx+tw/2+12;
      pad_badge(lx,cy,BG_L1); pad_badge(rx,cy,BG_R1);
      if(A.click && mouse_in(lx,cy,22,14)){ *dsp=(*dsp+dmax-2)%dmax+1; sfx(SFX_HOVER); }
      if(A.click && mouse_in(rx,cy,22,14)){ *dsp=*dsp%dmax+1; sfx(SFX_HOVER); } }
    int d=*dsp; int ly=cy+16;
    if(page==0){ float est=790.0f+110; (void)i; snprintf(b,sizeof b,"Durée : %d min environ",(int)(est/60+0.5f)); text_draw(cx,ly,b,HEX(0xb8a8b0),1,AL_CENTER,0); ly+=11; }
    if(d>1){ snprintf(b,sizeof b,"Ennemis ^3+%d %%^0 PV   or ^1+%d %%",(int)(35*(d-1)),15*(d-1)); text_draw(cx,ly,b,HEX(0x9a8a98),1,AL_CENTER,0); ly+=11; }
    if(d>=6){ snprintf(b,sizeof b,"^3%d malédiction%s",d-5,d>6?"s":""); text_draw(cx,ly,b,HEX(0x9a8a98),1,AL_CENTER,0); ly+=11; }
    if(page==0){ int nd=0; for(int k=0;k<3;k++) nd+=(S.obj_done[i]>>k)&1; snprintf(b,sizeof b,"Objectifs %s%s%s^0  %d/3",(S.obj_done[i]&1)?"^1"IC_STAR:"^7"IC_STAR,(S.obj_done[i]&2)?"^1"IC_STAR:"^7"IC_STAR,(S.obj_done[i]&4)?"^1"IC_STAR:"^7"IC_STAR,nd); if(ly<y+h-8){ text_draw(cx,ly,b,HEX(0xb8a8b0),1,AL_CENTER,0); ly+=11; } }
    if(page==0 && S.lvl_best[i]>0 && ly<y+h-8){ snprintf(b,sizeof b,"Record : %d:%02d",S.lvl_best[i]/60,S.lvl_best[i]%60); text_draw(cx,ly,b,HEX(0x8a7a88),1,AL_CENTER,0); }
    else if(page==1 && ly<y+h-8){ snprintf(b,sizeof b,"Record : étage %d",S.endless_best); text_draw(cx,ly,b,HEX(0x8a7a88),1,AL_CENTER,0); }
  }
  /* coches par heros (page des niveaux) */
  if(page==0){
    unsigned done=lvl_heroes_diff(i,dsp?*dsp:1,S.difficulty);
    for(int c=0;c<CH_COUNT;c++){ int gx=x+4+(c%7)*17, gy=y+30+(c/7)*30;
      const Sprite *sp=&SPR_LO[CHARS[c].sprite]; DrawOpt o=dopt(); o.flags=DS_NOOCC;
      if(!S.unlocked[c]){ o.flash=HEX(0x1a1018); o.flash_a=235; }
      else if(!((done>>c)&1)){ o.tint=HEX(0xb8b0b8); }
      gfx_ellipse_a(gx+9,gy+20,7,2,HEX(0x000000),120);
      draw_spr_c(sp,gx+9.0f,gy+20.0f-sp->h*0.5f,&o);
      if((done>>c)&1) draw_check(gx+12,gy+1,HEX(0x8be05a)); }
  } else if(i==1){
    int dh=0, dm[2]={0,1}; daily_setup(daily_seed_today(),&dh,dm);
    const Sprite *p=portrait(dh,false); int px=x+14, py=y+30;
    for(int j=0;j<52;j++) gfx_hline(px,px+51,py+j,col_lerp(HEX(0x4a2a3a),HEX(0x140a12),j/52.0f));
    { int ox0=gT->cx0, oy0=gT->cy0, ox1=gT->cx1, oy1=gT->cy1; gfx_clip(maxi(ox0,px),maxi(oy0,py),mini(ox1,px+52),mini(oy1,py+52)); draw_spr(p,px-6,py-8,DS_NOOCC); gfx_clip(ox0,oy0,ox1,oy1); }
    orn_frame(px,py,52,52,0);
    { int dl=daily_level_today();
      snprintf(b,sizeof b,"^1%s^0  -  %s",CHARS[dh].name,BIOMES[dl].name); text_draw(x+w/2+6,y+30,b,HEX(0xf6eae0),1,AL_CENTER,TX_SHADOW); }
    for(int k=0;k<2;k++){ snprintf(b,sizeof b,"^6"IC_STAR"^0 %s",MODS[dm[k]].name); text_draw(x+w/2+6,y+44+k*11,b,HEX(0xd8c8d0),1,AL_CENTER,0); }
    if(S.daily_date==daily_seed_today()) snprintf(b,sizeof b,"Meilleur score du jour : %d",S.daily_best); else snprintf(b,sizeof b,"Profondeur 2, héros imposé");
    text_draw(x+w/2+6,y+70,b,HEX(0x8a7a88),1,AL_CENTER,0);
  } else if(i==2){   /* v9 : rush des gardiens */
    int nd=1; while(nd<DEST_COUNT && dest_unlocked(nd)) nd++; S.rush_sel=clampi(S.rush_sel,0,nd-1);
    int cy=y+30, cx=x+w/2+6;
    snprintf(b,sizeof b,"Destination : %s",DEST_NAMES[S.rush_sel]); text_draw(cx,cy+2,b,HEX(0xf6eae0),1,AL_CENTER,TX_SHADOW);
    int tw=text_w(b,1);
    if(nd>1){ int lx=cx-tw/2-30, rx=cx+tw/2+12; pad_badge(lx,cy,BG_L1); pad_badge(rx,cy,BG_R1);
      if(A.click && mouse_in(lx,cy,22,14)){ S.rush_sel=(S.rush_sel+nd-1)%nd; sfx(SFX_HOVER); }
      if(A.click && mouse_in(rx,cy,22,14)){ S.rush_sel=(S.rush_sel+1)%nd; sfx(SFX_HOVER); } }
    text_draw(cx,cy+16,"Les 4 gardiens à la suite, sans monstres entre eux.",HEX(0xb8a8b0),1,AL_CENTER,0);
    int rb=S.rush_best[S.rush_sel];
    if(rb>0) snprintf(b,sizeof b,"Record : %d:%02d",rb/60,rb%60); else snprintf(b,sizeof b,"Pas encore terminé");
    text_draw(cx,cy+28,b,HEX(0x8a7a88),1,AL_CENTER,0);
  } else {
    text_wrap(x+14,y+32,110,"Les niveaux s'enchaînent sans fin, toujours plus durs.",HEX(0xb8a8b0),1,0,11);
  }
  /* bouton Jouer */
  { int bw=62, bh=56, bx=x+w-bw-12, by=y+28; bool hov=mouse_in(bx,by,bw,bh);
    for(int j=0;j<bh;j++) gfx_hline(bx,bx+bw-1,by+j,col_lerp(hov?HEX(0x5a3036):HEX(0x3a2026),HEX(0x1a0e12),(float)j/bh));
    orn_frame(bx,by,bw,bh,1);
    text_draw(bx+bw/2,by+12,"Jouer",HEX(0xfff0e4),1,AL_CENTER,TX_SHADOW);
    int bwid= A.pad ? 13 : text_w("Entrée",1)+8; pad_badge(bx+bw/2-bwid/2,by+30,BG_CONFIRM);
    if(hov && A.click) play=true; }
  return play;
}

static void start_from_pick(int page,int i){
  if(page==0){
    if(!lvl_unlocked(i)){ sfx(SFX_DENY); return; }
    PICK.level=i; PICK.depth=clampi(S.lvl_depth[i],1,lvl_depth_max(i)); PICK.mode=MODE_NORMAL; hd_gpu_prefetch_level(i,0);
    save_write(); sfx(SFX_SELECT); goto_state(GS_CHARSEL); return;
  }
  int m= page1_mode(i);
  if(!mode_unlocked(m)){ sfx(SFX_DENY); toast(mode_lock_text(m)); return; }
  if(m==MODE_RUSH){ PICK.level=clampi(S.rush_sel,0,DEST_COUNT-1)*LEVELS_PER_DEST; PICK.depth=1; PICK.mode=MODE_RUSH; hd_gpu_prefetch_level(PICK.level,3); save_write(); sfx(SFX_SELECT); goto_state(GS_CHARSEL); return; }
  if(m==MODE_ENDLESS){ PICK.level=0; PICK.depth=clampi(S.depth_sel,1,maxi(1,S.depth_max)); PICK.mode=MODE_ENDLESS; hd_gpu_prefetch_level(0,0); save_write(); sfx(SFX_SELECT); goto_state(GS_CHARSEL); return; }
  /* defi du jour : heros impose, on part directement */
  int dh=0, dm[2]; daily_setup(daily_seed_today(),&dh,dm);
  PICK.level=0; PICK.depth=2; PICK.mode=MODE_DAILY; save_write(); sfx(SFX_SELECT);
  start_run_request(dh,-1,2,MODE_DAILY,0);
}

/* v10 : mutateurs (regles a cocher avant un niveau, contre plus de recompenses) */
static bool mut_open=false; static int mut_cur=0;
static void mutators_panel(void){
  int px=110, py=40, pw=420, ph=300; char b[160];
  gfx_darken(0,0,SCREEN_W,SCREEN_H,120);
  orn_panel(px,py,pw,ph);
  gothic_title(SCREEN_W/2,py+8,"Mutateurs",24);
  text_draw(SCREEN_W/2,py+38,"Des règles en plus pour les niveaux : plus durs, mais mieux récompensés.",HEX(0xd8c8d0),1,AL_CENTER,TX_SHADOW);
  if(A.up){ mut_cur=(mut_cur+MOD_COUNT-1)%MOD_COUNT; sfx(SFX_HOVER); }
  if(A.down){ mut_cur=(mut_cur+1)%MOD_COUNT; sfx(SFX_HOVER); }
  if(A.left||A.right){ mut_cur=(mut_cur+MOD_COUNT/2)%MOD_COUNT; sfx(SFX_HOVER); }
  int act=-1;
  for(int m=0;m<MOD_COUNT;m++){ int col=m/(MOD_COUNT/2), row=m%(MOD_COUNT/2); int x=px+14+col*200, y=py+54+row*30, w=192, h=26;
    bool on=(S.mut_sel>>m)&1, hov=mouse_in(x,y,w,h);
    if(hov && A.mouse_moved && mut_cur!=m){ mut_cur=m; sfx(SFX_HOVER); }
    for(int j=0;j<h;j++) gfx_hline(x,x+w-1,y+j,col_lerp(on?HEX(0x4a2a3a):HEX(0x24161c),HEX(0x140a10),(float)j/h));
    orn_frame(x,y,w,h,m==mut_cur?1:0);
    gfx_rect(x+5,y+8,9,9,HEX(0x0a0608)); gfx_rect(x+6,y+9,7,7,on?HEX(0xffd84a):HEX(0x3a2a30));
    text_draw(x+19,y+3,MODS[m].name,on?HEX(0xfff0dc):HEX(0xc8b8c0),1,AL_LEFT,TX_SHADOW);
    snprintf(b,sizeof b,"^1+%d %%",mut_bonus(1u<<m)); text_draw(x+w-6,y+3,b,HEX(0xffffff),1,AL_RIGHT,TX_SHADOW);
    if(hov && A.click) act=m; }
  if(A.confirm) act=mut_cur;
  if(act>=0){ S.mut_sel^=1u<<act; sfx(SFX_CLICK); }
  /* description du mutateur choisi */
  text_wrap(px+18,py+238,pw-36,MODS[mut_cur].desc,HEX(0xe8dce4),1,TX_SHADOW,11);
  int n=mut_count(S.mut_sel);
  if(n) snprintf(b,sizeof b,"%d mutateur%s : or ^1+%d %%^0, expérience ^4+%d %%^0, ^6+%d "IC_GEM"^0 à la victoire",n,n>1?"s":"",mut_bonus(S.mut_sel),mut_bonus(S.mut_sel)/2,n);
  else snprintf(b,sizeof b,"Aucun mutateur : le niveau se joue normalement.");
  text_draw(SCREEN_W/2,py+262,b,HEX(0xd8d0e0),1,AL_CENTER,TX_SHADOW);
  if(A.pad) snprintf(b,sizeof b,"%s : cocher   %s : fermer",btn(B_CONFIRM),btn(B_BACK)); else snprintf(b,sizeof b,"Clic ou Entrée : cocher   Échap : fermer");
  text_draw(SCREEN_W/2,py+ph-18,b,HEX(0x8a82a0),1,AL_CENTER,0);
  if(A.back || A.pause || (g_in && (g_in->key_pressed[KEY_M]||g_in->pad_pressed[PAD_X]))){ mut_open=false; save_write(); sfx(SFX_CLICK); }
}
void levels_frame(float dt){
  ls_t+=dt;
  layer(LY_SCENE);
  camp_world(dt,1.1f);
  light_apply();
  if(g_gpu){ GP.blur_all=0.5f; layer(LY_UI); }
  gfx_darken(0,0,SCREEN_W,SCREEN_H,70);
  if(mut_open){ int pg=S.lvl_page; (void)pg; orn_panel(96,6,448,360); mutators_panel(); return; }
  int page=S.lvl_page, n=page_n(page);
  bool pages_ok = mode_unlocked(MODE_ENDLESS)||mode_unlocked(MODE_DAILY)||mode_unlocked(MODE_RUSH);
  if(page!=ls_last_page){ ls_last_page=page; for(int k=0;k<BIOME_COUNT;k++) ls_h[k]=BAN_H; ls_scroll=0; }
  /* commandes */
  int sel=S.lvl_sel;
  if(A.up && sel>0){ sel--; sfx(SFX_HOVER); }
  if(A.down && sel<n-1){ sel++; sfx(SFX_HOVER); }
  if(A.wheel<0 && sel<n-1){ sel++; sfx(SFX_HOVER); } if(A.wheel>0 && sel>0){ sel--; sfx(SFX_HOVER); }
  S.lvl_sel=sel;
  { int *dsp=NULL, dmax=1;
    if(page==0 && lvl_unlocked(sel)){ dsp=&S.lvl_depth[sel]; dmax=lvl_depth_max(sel); }
    else if(page==1 && sel==0 && mode_unlocked(MODE_ENDLESS)){ dsp=&S.depth_sel; dmax=maxi(1,S.depth_max); }
    else if(page==1 && sel==2 && mode_unlocked(MODE_RUSH)){ int nd=1; while(nd<DEST_COUNT && dest_unlocked(nd)) nd++; dsp=&S.rush_sel; dmax=nd; S.rush_sel++; }
    int dd=(A.left||A.tab_l?-1:0)+(A.right||(A.tab_r && !(g_in&&g_in->key_pressed[KEY_TAB]))?1:0);
    if(dsp && dmax>1 && dd){ *dsp=((*dsp-1+dd+dmax)%dmax)+1; sfx(SFX_HOVER); }
    if(page==1 && sel==2 && mode_unlocked(MODE_RUSH)) S.rush_sel--; }
  bool page_sw = A.trig_l || A.trig_r || (g_in && g_in->key_pressed[KEY_TAB]);
  if(page_sw){ if(pages_ok){ S.lvl_page=!page; S.lvl_sel=0; sfx(SFX_CLICK); } else { sfx(SFX_DENY); toast("Modes de jeu : défi « Habitué », « Purificateur » ou « Gladiateur » requis"); } }
  if(A.reroll){ S.difficulty=(S.difficulty+1)%DIFF_COUNT; save_write(); sfx(SFX_HOVER); }
  /* cadre */
  orn_panel(96,6,448,360);
  /* en-tete */
  { int hx=LS_X, hy=16, hw=LS_W, hh=28;
    for(int j=0;j<hh;j++) gfx_hline(hx,hx+hw-1,hy+j,col_lerp(HEX(0x4a2a24),HEX(0x24120e),(float)j/hh));
    orn_frame(hx,hy,hw,hh,0);
    gothic_title(320,hy+2,page?"Modes de jeu":"Niveaux du Gouffre",24);
    if(pages_ok){
      if(page==0){ int bx=hx+hw-50; pad_badge(bx,hy+8,BG_R2); if(A.click && mouse_in(bx-4,hy,40,hh)){ S.lvl_page=1; S.lvl_sel=0; sfx(SFX_CLICK); } }
      else { int bx=hx+24; pad_badge(bx,hy+8,BG_L2); if(A.click && mouse_in(bx-4,hy,40,hh)){ S.lvl_page=0; S.lvl_sel=0; sfx(SFX_CLICK); } }
    } }
  /* retour */
  { int bx=72, by=20; text_draw(bx-8,by+1,"<",HEX(0xe8d8c8),1,AL_LEFT,TX_OUTLINE); pad_badge(bx,by,BG_BACK);
    if(A.click && mouse_in(bx-10,by-2,40,18)){ sfx(SFX_CLICK); goto_state(ls_ret); return; } }
  if(A.back){ sfx(SFX_CLICK); goto_state(ls_ret); return; }
  /* ressources */
  { char b[80]; snprintf(b,sizeof b,"^1"IC_COIN" %d^0   ^6"IC_GEM" %d^0   "IC_PRISM" %d",S.gold,S.crystals,S.prisms); text_draw(SCREEN_W-8,6,b,HEX(0xf0e6d0),1,AL_RIGHT,TX_OUTLINE); }
  /* liste */
  page=S.lvl_page; n=page_n(page); sel=clampi(S.lvl_sel,0,n-1);
  float total=0, ysel=0;
  for(int i=0;i<n;i++){ float th= i==sel ? SEL_H : BAN_H; ls_h[i]=approachf(ls_h[i],th,dt*520); if(i==sel) ysel=total; total+=ls_h[i]+4; }
  float view=LS_BOT-LS_TOP;
  float target=clampf(ysel-22,0,maxf(0,total-view));
  ls_scroll+= (target-ls_scroll)*minf(1,dt*12);
  int ox0=gT->cx0, oy0=gT->cy0, ox1=gT->cx1, oy1=gT->cy1;
  gfx_clip(LS_X,LS_TOP,LS_X+LS_W,LS_BOT);
  float yy=LS_TOP-ls_scroll; bool play=false; int clicked=-1;
  for(int i=0;i<n;i++){
    int h=(int)(ls_h[i]+0.5f), y=(int)yy; yy+=ls_h[i]+4;
    if(y>LS_BOT || y+h<LS_TOP) continue;
    int bi= page ? BIOME_COUNT+i : i;
    bool locked = page==0 ? !lvl_unlocked(i) : !mode_unlocked(page1_mode(i));
    /* image : le milieu de la banniere */
    { int sy0=(SEL_H-h)/2; gfx_clip(LS_X,maxi(LS_TOP,y),LS_X+LS_W,mini(LS_BOT,y+h));
      draw_spr(&ban[bi],LS_X,y-sy0,DS_NOOCC);
      if(locked) gfx_darken(LS_X,y,LS_W,h,150);
      gfx_clip(LS_X,LS_TOP,LS_X+LS_W,LS_BOT); }
    torn_edge(LS_X,y,LS_W,i+page*11);
    gfx_hline(LS_X,LS_X+LS_W-1,y+h-1,HEX(0x0a0608));
    if(i==sel && h>=SEL_H-6){
      if(level_band(i,LS_X,y+BAND,LS_W,h-BAND,page)) play=true;
    } else {
      const char *nm = page==0 ? (locked?"???":lvl_short(i)) : page1_name(i);
      text_draw(LS_X+LS_W/2,y+h/2-10,nm,locked?HEX(0x9a8a90):HEX(0xfff4ec),2,AL_CENTER,TX_OUTLINE);
      if(page==0){ medallion(LS_X+LS_W-36,y+h/2,lvl_mb_kind(i,1),!locked);
        if(!locked){ unsigned dn=lvl_heroes_diff(i,0,S.difficulty); int nc=0; for(int c=0;c<CH_COUNT;c++) nc+=(dn>>c)&1;
          if(nc){ char b[16]; snprintf(b,sizeof b,"%d/%d",nc,CH_COUNT); draw_check(LS_X+14,y+h/2-4,HEX(0x8be05a)); text_draw(LS_X+27,y+h/2-4,b,HEX(0xe8f0d8),1,AL_LEFT,TX_OUTLINE); } }
        else orn_padlock(LS_X+40,y+h/2+2); }
      if(A.click && mouse_in(LS_X,maxi(LS_TOP,y),LS_W,mini(h,LS_BOT-y))) clicked=i;
    }
  }
  gfx_clip(ox0,oy0,ox1,oy1);
  if(clicked>=0){ S.lvl_sel=clicked; sfx(SFX_HOVER); }
  if(A.confirm || play){ start_from_pick(S.lvl_page,S.lvl_sel); }
  /* difficulte (facon « Modificateurs ») */
  { char b[96]; int bx=10, by=336; snprintf(b,sizeof b,"Difficulté : %s",diff_cur()->name);
    gfx_rect_a(bx-4,by-3,text_w(b,1)+30,19,HEX(0x0a0608),190);
    int bw=pad_badge(bx,by,BG_ALT);
    text_draw(bx+bw+5,by+2,"Difficulté :",HEX(0xf0e4ec),1,AL_LEFT,TX_OUTLINE);
    text_draw(bx+bw+5+text_w("Difficulté : ",1),by+2,diff_cur()->name,diff_cur()->col,1,AL_LEFT,TX_OUTLINE);
    if(A.click && mouse_in(bx,by-2,140,16)){ S.difficulty=(S.difficulty+1)%DIFF_COUNT; save_write(); sfx(SFX_HOVER); } }
  /* v10 : mutateurs (niveaux seulement) */
  if(S.lvl_page==0){ char b[96]; int n=mut_count(S.mut_sel);
    if(mut_unlocked()){ if(n) snprintf(b,sizeof b,"Mutateurs : %d (^1+%d %%^0)",n,mut_bonus(S.mut_sel)); else snprintf(b,sizeof b,"Mutateurs : aucun");
      int tw=text_w(b,1), bx=SCREEN_W-14-tw-24, by=336;
      gfx_rect_a(bx-4,by-3,tw+32,19,HEX(0x0a0608),190);
      int bw=A.pad?pad_badge(bx,by,BG_X):text_badge(bx,by,"M");
      text_draw(bx+bw+5,by+2,b,n?HEX(0xffd0a0):HEX(0xf0e4ec),1,AL_LEFT,TX_OUTLINE);
      if((A.click && mouse_in(bx-4,by-3,tw+32,19)) || (g_in && (g_in->key_pressed[KEY_M]||g_in->pad_pressed[PAD_X]))){ mut_open=true; mut_cur=0; sfx(SFX_CLICK); } }
    else { snprintf(b,sizeof b,"Mutateurs : défi « Apprenti sorcier »"); text_draw(SCREEN_W-14,338,b,HEX(0x7a6a78),1,AL_RIGHT,TX_OUTLINE); } }
}
