/* GOUFFRE - menus de la descente : choix d'amelioration (billes et objets) et pause a trois onglets */
#include "game.h"

/* ---------------- petites icones d'attributs ---------------- */
static const char *ICO[6][8]={
  {".##.##..","########","########",".######.","..####..","...##...",NULL},                                   /* vitalite */
  {"......##",".....###","....###.","#..###..",".####...","..##....",".#.##...","#...#..."},                   /* puissance */
  {"#######",".#...#.","..#.#..","...#...","..#.#..",".#.#.#.","#######",NULL},                              /* cadence */
  {".###....",".###....",".###....",".####...",".######.",".#######",NULL},                                   /* vitesse */
  {"..####..",".#....#.","#..##..#","#..##..#",".#....#.","..####..",NULL},                                   /* precision */
  {".######.","#.....##","#.###.##","#.....##","#.###.##","########",NULL},                                   /* savoir */
};
static const uint32_t ICOC[6]={HEX(0xe8384a),HEX(0xd8dcea),HEX(0xe8b840),HEX(0xb07ad8),HEX(0x6ad8ff),HEX(0xd8b070)};
void attr_icon(int k,int x,int y,int sc){
  for(int j=0;j<8 && ICO[k][j];j++) for(int i=0;ICO[k][j][i];i++) if(ICO[k][j][i]=='#') gfx_rect(x+i*sc+1,y+j*sc+1,sc,sc,HEX(0x0a0608));
  for(int j=0;j<8 && ICO[k][j];j++) for(int i=0;ICO[k][j][i];i++) if(ICO[k][j][i]=='#') gfx_rect(x+i*sc,y+j*sc,sc,sc,j<2?col_lerp(ICOC[k],HEX(0xffffff),0.3f):ICOC[k]);
}
const char *const ATTR_N[6]={"Vitalité","Puissance","Cadence","Vitesse","Précision","Savoir"};
static void attr_val(int k,char *b,int n){
  switch(k){
    case 0: snprintf(b,n,"%d",(int)(R.hpmax+0.5f)); break;
    case 1: snprintf(b,n,"%d %%",(int)(R.s_dmg*100+0.5f)); break;
    case 2: snprintf(b,n,"%.1f /s",1.0f/maxf(0.02f,R.s_fire)); break;
    case 3: snprintf(b,n,"%d %%",(int)(R.s_speed/108.0f*100+0.5f)); break;
    case 4: snprintf(b,n,"%d %%",(int)(R.s_crit*100+0.5f)); break;
    default: snprintf(b,n,"+%d %%",(int)((R.s_xp-1)*100+0.5f)); break;
  }
}
static void attributes(int x,int y,int w,int sc,int lh){
  char b[32];
  for(int k=0;k<6;k++){ int yy=y+k*lh;
    attr_icon(k,x,yy+(sc>1?2:1),sc>1?2:1);
    text_draw(x+(sc>1?22:12),yy,ATTR_N[k],HEX(0xf0e0e8),sc,AL_LEFT,TX_SHADOW);
    attr_val(k,b,sizeof b); text_draw(x+w,yy,b,HEX(0xf0e0e8),sc,AL_RIGHT,TX_SHADOW); }
}
static int detailed_lines(char L[20][96]){
  int n=0;
  #define LN(...) snprintf(L[n++],96,__VA_ARGS__)
  LN("Points de vie : ^1%d / %d",(int)R.hp,(int)R.hpmax);
  LN("Régénération : ^1%.1f PV/s",R.s_regen);
  LN("Dégâts : ^1%d %%",(int)(R.s_dmg*100+0.5f));
  LN("Coups critiques : ^1%d %%^0   x^1%.1f",(int)(R.s_crit*100+0.5f),R.s_critm);
  LN("Cadence : ^1%.1f^0 lancers/s",1.0f/maxf(0.02f,R.s_fire));
  LN("Vitesse des billes : ^1%d %%",(int)(R.s_bspeed/295.0f*100+0.5f));
  LN("Vitesse : ^1%d %%",(int)(R.s_speed/108.0f*100+0.5f));
  LN("Petites billes : ^1%d",R.s_babies);
  LN("Altérations : ^1%d %%",(int)(R.s_status*100+0.5f));
  LN("Ramassage : ^1%d px",(int)R.s_pick);
  LN("Or : ^1+%d %%^0   expérience : ^1+%d %%",(int)((R.s_gold-1)*100+0.5f),(int)((R.s_xp-1)*100+0.5f));
  if(R.s_breach<1) LN("Brèches : ^2-%d %%^0 de dégâts",(int)((1-R.s_breach)*100+0.5f));
  LN("Relances : ^1%d^0   résurrections : ^1%d",R.rerolls,R.revives);
  if(R.s_net>0) LN("Filet : ^1%d %%",(int)(R.s_net*100+0.5f));
  if(R.s_mirror>0) LN("Miroir : ^1%d %%",(int)(R.s_mirror*100+0.5f));
  if(R.s_dodge>0) LN("Esquive : ^1%d %%",(int)(R.s_dodge*100+0.5f));
  if(R.s_giant>0||R.s_anchor>0) LN("Élites et gardiens : ^1+%d %%^0   fond : ^1+%d %%",(int)(R.s_giant*100+0.5f),(int)(R.s_anchor*100+0.5f));
  if(R.s_thorns>0) LN("Épines : ^1%d^0 par blessure",R.s_thorns);
  #undef LN
  return n;
}

/* ---------------- choix d'amelioration ---------------- */
static int lu_tab=0;           /* 0 : fiche de la carte ; 1 : details de la descente */
static int lu_focus=0;         /* 0 : cartes ; 1 : Relancer ; 2 : Passer */
void card_rect(int i,int n,int *x,int *y,int *w,int *h){
  int cw=70, gap=6, total=n*cw+(n-1)*gap;
  *x=156-total/2+i*(cw+gap); *y=184; *w=cw; *h=110;
}
#define RR_X 12
#define RR_Y 304
#define PS_X 170
void levelup_input(float dt){
  (void)dt;
  if(R.card_t<0.35f) return;
  if(A.trig_r || (g_in && g_in->key_pressed[KEY_TAB])){ lu_tab=!lu_tab; sfx(SFX_CLICK); }
  if(A.trig_l || (g_in && g_in->key_pressed[KEY_E])){ sfx(SFX_CLICK); meta_open(META_CODEX,GS_RUN); return; }
  if(A.left){ if(lu_focus==2) lu_focus=1; else if(lu_focus==0){ R.card_sel=(R.card_sel+R.ncards-1)%R.ncards; } sfx(SFX_HOVER); }
  if(A.right){ if(lu_focus==1) lu_focus=2; else if(lu_focus==0){ R.card_sel=(R.card_sel+1)%R.ncards; } sfx(SFX_HOVER); }
  if(A.down && lu_focus==0){ lu_focus=2; sfx(SFX_HOVER); }
  if(A.up && lu_focus!=0){ lu_focus=0; sfx(SFX_HOVER); }
  for(int i=0;i<R.ncards;i++){
    int x,y,w,h; card_rect(i,R.ncards,&x,&y,&w,&h);
    if(mouse_in(x,y,w,h)){ if(A.mouse_moved && (R.card_sel!=i||lu_focus)){ R.card_sel=i; lu_focus=0; sfx(SFX_HOVER); } if(A.click){ lu_focus=0; run_choose_card(i); return; } }
    if(i<4 && A.n[i]){ run_choose_card(i); return; }
  }
  bool rr=mouse_in(RR_X,RR_Y,140,24), ps=mouse_in(PS_X,RR_Y,130,24);
  if(A.mouse_moved){ if(rr) lu_focus=1; else if(ps) lu_focus=2; }
  bool skip = (ps && A.click) || (g_in && (g_in->key_pressed[KEY_BACKSPACE] || g_in->pad_pressed[PAD_X])) || (A.confirm && lu_focus==2);
  bool reroll = (rr && A.click) || A.reroll || (A.confirm && lu_focus==1);
  if(skip){ lu_focus=0; run_skip_card(); return; }
  if(reroll){ run_reroll_cards(); return; }
  if(A.confirm && lu_focus==0){ run_choose_card(R.card_sel); return; }
}
static void slot_box(int x,int y,int w,int h,bool filled,uint32_t acc){
  gfx_rect(x-1,y-1,w+2,h+2,HEX(0x0a0608));
  for(int j=0;j<h;j++) gfx_hline(x,x+w-1,y+j,col_lerp(filled?HEX(0x34202a):HEX(0x1c1218),HEX(0x140c12),(float)j/h));
  gfx_frame(x,y,w,h,filled?col_lerp(HEX(0x8a5a2e),acc,0.35f):HEX(0x4a3028));
  gfx_pset(x,y,HEX(0xc89458)); gfx_pset(x+w-1,y,HEX(0xc89458)); gfx_pset(x,y+h-1,HEX(0x8a5a2e)); gfx_pset(x+w-1,y+h-1,HEX(0x8a5a2e));
}
static void item_icon(int kind,bool ball,int cx,int cy,int size){
  if(ball){ if(g_gpu) fx_glow((float)cx,(float)cy,size*0.55f,BALLS[kind].col,0.12f); icon_draw(kind,(float)cx,(float)cy,(float)size,NULL); }
  else icon_draw(ICON_PAS(kind),(float)cx,(float)cy,(float)size,NULL);
}
static void inventory_slots(int x0,int y0){
  char b[8];
  for(int r=0;r<2;r++) for(int i=0;i<MAX_SLOTS;i++){
    int x=x0+i*32, y=y0+r*54; bool ball=r==0;
    bool f= ball ? i<R.nballs : i<R.npas;
    int k= f ? (ball?R.balls[i]:R.pas[i]) : -1;
    uint32_t acc= !f ? HEX(0x3a2a30) : (ball ? (IS_EVO(k)?HEX(0xc08aff):BALLS[k].col) : HEX(0x8be05a));
    slot_box(x,y,28,48,f,acc);
    if(f){ item_icon(k,ball,x+14,y+30,24);
      if(ball && IS_EVO(k)) text_draw(x+14,y+5,IC_STAR,HEX(0xffd84a),1,AL_CENTER,TX_OUTLINE);
      else { snprintf(b,sizeof b,"%d",ball?R.ball_lvl[i]:R.pas_lvl[i]); bool mx=(ball?R.ball_lvl[i]:R.pas_lvl[i])>=3; text_big_digits(x+14,y+3,b,mx?HEX(0xffd84a):HEX(0xf0e4ec),1,AL_CENTER); } }
  }
}
static void left_backdrop(int w){
  for(int x=0;x<w;x++){ int a=x<w-40?220:220-(x-(w-40))*3; gfx_rect_a(x,0,1,SCREEN_H,HEX(0x140a10),clampi(a,0,255)); }
  for(int y=0;y<SCREEN_H;y+=2){ uint32_t h=(uint32_t)y*2654435761u; gfx_rect_a((int)(h%(uint32_t)w),y,(int)(h>>27)+2,1,HEX(0x2a1820),70); }
  /* liseré de bronze a droite */
  gfx_vline(w-2,0,SCREEN_H,HEX(0x0a0608)); gfx_vline(w-1,0,SCREEN_H,HEX(0x8a5a2e)); gfx_vline(w,0,SCREEN_H,HEX(0x4a2c16));
  for(int k=0;k<5;k++){ int cy=36+k*72; gfx_rect(w-3,cy-3,7,7,HEX(0x0a0608)); gfx_rect(w-2,cy-2,5,5,HEX(0x8a5a2e)); gfx_rect(w-1,cy-1,3,3,HEX(0xe8b860)); }
}
static void card_label(Card *c,char *b,int n){
  switch(c->type){
    case CARD_NEWBALL: case CARD_NEWPAS: snprintf(b,n,"Nouv."); break;
    case CARD_UPBALL: snprintf(b,n,"Niv. %d",run_ball_level(c->kind)+1); break;
    case CARD_UPPAS: snprintf(b,n,"Niv. %d",run_pas_level(c->kind)+1); break;
    case CARD_EVO: snprintf(b,n,"Évol."); break;
    case CARD_LEG: snprintf(b,n,"Légende"); break;
    case CARD_HEAL: snprintf(b,n,"Soin"); break;
    default: snprintf(b,n,"Or"); break;
  }
}
static void right_card_panel(Card *c){
  int x=322, y=24, w=310, h=314;
  orn_panel(x,y,w,h);
  char head[64], name[64], dmg[64], desc[320]; int rec[9], nrec=0;
  run_card_detail(c,head,sizeof head,name,sizeof name,dmg,sizeof dmg,desc,sizeof desc,rec,&nrec);
  int cx=x+w/2;
  text_draw(cx,y+22,head,HEX(0xf0d8e0),1,AL_CENTER,TX_SHADOW);
  int gs= gothic_w(name,24)<=w-30 ? 24 : 12;
  gothic_title(cx,y+38,name,gs);
  int yy=y+(gs==24?74:66);
  if(dmg[0]){ text_draw(cx,yy,dmg,HEX(0xf6e0e4),1,AL_CENTER,TX_SHADOW); yy+=18; }
  int nl=text_wrap(x+22,yy,w-44,desc,HEX(0xe8d4dc),1,TX_SHADOW,12); yy+=nl>0?nl:12; yy+=14;
  if(nrec>0){
    if(yy<y+170) yy=y+170;
    text_draw(cx,yy,"Synergies d'équipement",HEX(0xf0d8e0),1,AL_CENTER,TX_SHADOW); yy+=18;
    for(int r=0;r<nrec && yy<y+h-40;r++){
      int ix=cx-76;
      for(int k=0;k<3;k++){ int bk=rec[r*3+k]; int px=ix+k*60;
        gfx_disc(px+16,yy+16,17,HEX(0x0a0608)); gfx_disc(px+16,yy+16,16,HEX(0x24141c));
        if(k<2 || ((S.evo_seen>>(bk-BK_FIRST_EVO))&1)) item_icon(bk,true,px+16,yy+16,32);
        else text_draw(px+16,yy+9,"?",HEX(0xc8a8d0),2,AL_CENTER,TX_OUTLINE);
        if(k<2) text_draw(px+40,yy+11,k==0?"×":"=",HEX(0xe8d8e0),1,AL_CENTER,TX_SHADOW); }
      yy+=40; }
  }
}
static void right_details_panel(void){
  int x=322, y=24, w=310, h=314; orn_panel(x,y,w,h);
  text_draw(x+w/2,y+22,"Votre descente",HEX(0xf0d8e0),1,AL_CENTER,TX_SHADOW);
  gothic_title(x+w/2,y+36,"Attributs",24);
  attributes(x+30,y+76,w-60,1,16);
  char L[20][96]; int n=detailed_lines(L);
  for(int i=3;i<n && i<14;i++) text_draw(x+30,y+180+(i-3)*12,L[i],HEX(0xc8b8c0),1,AL_LEFT,0);
}
void levelup_draw(void){
  gfx_darken(0,0,SCREEN_W,SCREEN_H,110);
  left_backdrop(314);
  char b[96];
  /* onglets */
  { int x=22, y=7; int bw= A.pad ? pad_badge(x,y,BG_L2) : text_badge(x,y,"E"); text_draw(x+bw+5,y+2,"Encyclopédie",HEX(0xc8b8c0),1,AL_LEFT,TX_SHADOW);
    if(A.click && mouse_in(x,y-2,bw+80,16)){ sfx(SFX_CLICK); meta_open(META_CODEX,GS_RUN); }
    int x2=176; bw= A.pad ? pad_badge(x2,y,BG_R2) : text_badge(x2,y,"Tab"); text_draw(x2+bw+5,y+2,"Détails",lu_tab?HEX(0xffffff):HEX(0xc8b8c0),1,AL_LEFT,TX_SHADOW);
    if(A.click && mouse_in(x2,y-2,bw+50,16)){ lu_tab=!lu_tab; sfx(SFX_CLICK); } }
  /* portrait du (des) heros */
  duo_portrait(8,26,128,128,R.ch,R.ch2,2); orn_frame(8,26,128,128,1);
  inventory_slots(146,30);
  { snprintf(b,sizeof b,"^1"IC_COIN"^0 %d",R.gold); text_draw(226,140,b,HEX(0xf0e6d0),1,AL_CENTER,TX_SHADOW); }
  text_draw(156,166,R.boss_reward?"Trésor du gardien : choisissez":"Choisissez une amélioration",HEX(0xf0e0e8),1,AL_CENTER,TX_SHADOW);
  float t=R.card_t;
  for(int i=0;i<R.ncards;i++){
    int x,y,w,h; card_rect(i,R.ncards,&x,&y,&w,&h);
    Card *c=&R.cards[i]; bool sel= i==R.card_sel && lu_focus==0;
    float ap=clampf((t-i*0.06f)*5,0,1); y+=(int)((1-ap)*24); if(sel) y-=3;
    uint32_t acc=HEX(0x6ad8ff);
    if(c->type==CARD_NEWPAS||c->type==CARD_UPPAS) acc=HEX(0x8be05a); else if(c->type==CARD_EVO) acc=HEX(0xd090ff); else if(c->type==CARD_LEG) acc=HEX(0xffd84a);
    else if(c->type==CARD_HEAL) acc=HEX(0xff6a6a); else if(c->type==CARD_GOLD) acc=HEX(0xffd84a); else acc=col_lerp(HEX(0x6ad8ff),BALLS[c->kind].col,0.5f);
    for(int j=0;j<h;j++) gfx_hline(x,x+w-1,y+j,col_lerp(sel?HEX(0x4a2632):HEX(0x2e1820),HEX(0x160c12),(float)j/h));
    if(sel && g_gpu) fx_glow(x+w*0.5f,y+h*0.4f,40,acc,0.16f);
    orn_frame(x,y,w,h,sel?1:0);
    /* vignette */
    int bx=x+9, by=y+9, bw=w-18;
    slot_box(bx,by,bw,bw,true,acc);
    int icx=bx+bw/2, icy=by+bw/2;
    if(c->type==CARD_NEWBALL||c->type==CARD_UPBALL||c->type==CARD_EVO||c->type==CARD_LEG) item_icon(c->kind,true,icx,icy,44);
    else if(c->type==CARD_NEWPAS||c->type==CARD_UPPAS) item_icon(c->kind,false,icx,icy,44);
    else text_draw(icx,icy-11,c->type==CARD_HEAL?IC_HEART:IC_COIN,acc,2,AL_CENTER,TX_OUTLINE);
    if(c->type==CARD_EVO||c->type==CARD_LEG) for(int k=0;k<(c->type==CARD_LEG?8:4);k++){ float a=g_time*1.5f+k*TAU/(c->type==CARD_LEG?8:4); gfx_padd(icx+(int)(cosf(a)*19),icy+(int)(sinf(a)*19),c->type==CARD_LEG?HEX(0xffe080):HEX(0xe0c0ff),220); }
    card_label(c,b,sizeof b); text_draw(x+w/2,y+h-20,b,sel?HEX(0xffffff):HEX(0xe8d8e0),1,AL_CENTER,TX_SHADOW);
    snprintf(b,sizeof b,"%d",i+1); if(!A.pad) text_draw(x+5,y+4,b,HEX(0x6a5a64),1,AL_LEFT,0);
  }
  /* Relancer / Passer */
  { snprintf(b,sizeof b,"Relancer (%d)",R.rerolls);
    orn_button(RR_X,RR_Y,140,24,b,lu_focus==1,R.rerolls>0);
    if(A.pad) pad_badge(RR_X+4,RR_Y+5,BG_ALT);
    orn_button(PS_X,RR_Y,130,24,"Passer",lu_focus==2,true);
    if(A.pad) pad_badge(PS_X+4,RR_Y+5,BG_X); }
  if(lu_tab) right_details_panel(); else if(R.ncards>0) right_card_panel(&R.cards[clampi(R.card_sel,0,R.ncards-1)]);
}

/* ---------------- pause : Personnage / Equipement / Systeme ---------------- */
static int pz_tab=0, pz_eq=0, pz_sys=0; static bool pz_stats=false; static float pz_confirm=0; static int pz_confirm_id=-1;
void pause_reset(void){ pz_tab=0; pz_eq=0; pz_sys=0; pz_stats=false; pz_confirm=0; pz_confirm_id=-1; }
static int eq_count(void){ return R.nballs+R.npas; }
static void eq_item(int i,int *kind,bool *ball,int *lvl){ if(i<R.nballs){ *kind=R.balls[i]; *ball=true; *lvl=R.ball_lvl[i]; } else { int j=i-R.nballs; *kind=R.pas[j]; *ball=false; *lvl=R.pas_lvl[j]; } }
static void pause_tabs(int x,int y){
  static const char *T[4]={"Personnage","Équipement","Objectifs","Système"};
  if(A.pad) pad_badge(x-34,y+7,BG_L1);
  for(int k=0;k<4;k++){ int bx=x+k*92, bw=86, bh=26; bool on=k==pz_tab, hov=mouse_in(bx,y,bw,bh);
    for(int j=0;j<bh;j++) gfx_hline(bx,bx+bw-1,y+j,col_lerp(on?HEX(0x5a3440):(hov?HEX(0x3a2630):HEX(0x2a1820)),HEX(0x160c12),(float)j/bh));
    orn_frame(bx,y,bw,bh,on?1:0);
    text_draw(bx+bw/2,y+8,T[k],on?HEX(0xfff4ec):HEX(0xc8b8c0),1,AL_CENTER,TX_SHADOW);
    if(hov && A.click && !on){ pz_tab=k; sfx(SFX_CLICK); } }
  if(A.pad) pad_badge(x+4*92-2,y+7,BG_R1); else text_badge(x+4*92-2,y+7,"Tab");
}
static void pause_character(void){
  int bx=98, by=74, bw=220, bh=200;
  for(int j=0;j<bh;j++) gfx_hline(bx,bx+bw-1,by+j,col_lerp(HEX(0x2a1a22),HEX(0x1a1016),(float)j/bh));
  orn_frame(bx,by,bw,bh,0);
  int px=bx+(bw-128)/2, py=by+12;
  duo_portrait(px,py,128,128,R.ch,R.ch2,2); orn_frame(px,py,128,128,1);
  const char *nm=CHARS[R.ch].name; int gs= gothic_w(nm,24)<=bw-12 ? 24 : 12;
  gothic_title(bx+bw/2,py+136,nm,gs);
  if(R.ch2>=0){ char b[64]; snprintf(b,sizeof b,"& %s",CHARS[R.ch2].name); text_draw(bx+bw/2,py+136+(gs==24?26:16),b,HEX(0xe8d0d8),1,AL_CENTER,TX_SHADOW); }
  gothic_title(442,80,"Attributs",24);
  attributes(342,112,200,2,24);
  { int x=246, y=292, w=150, h=24; bool hov=mouse_in(x,y,w,h);
    orn_button(x,y,w,h,"   Stats détaillées…",false,true); pad_badge(x+8,y+5,BG_ALT);
    if((hov && A.click) || A.reroll){ pz_stats=true; sfx(SFX_CLICK); } }
}
static void pause_equipment(void){
  int lx=98, ly=74, lw=136, lh=258;
  for(int j=0;j<lh;j++) gfx_hline(lx,lx+lw-1,ly+j,col_lerp(HEX(0x2a1a22),HEX(0x1a1016),(float)j/lh));
  orn_frame(lx,ly,lw,lh,0);
  int n=eq_count();
  if(n==0){ text_draw(lx+lw/2,ly+lh/2,"Rien encore",HEX(0x9a8a90),1,AL_CENTER,0); return; }
  pz_eq=clampi(pz_eq,0,n-1);
  if(A.left && pz_eq%2==1){ pz_eq--; sfx(SFX_HOVER); } if(A.right && pz_eq%2==0 && pz_eq+1<n){ pz_eq++; sfx(SFX_HOVER); }
  if(A.up && pz_eq>=2){ pz_eq-=2; sfx(SFX_HOVER); } if(A.down && pz_eq+2<n){ pz_eq+=2; sfx(SFX_HOVER); }
  for(int i=0;i<n;i++){ int kind,lvl; bool ball; eq_item(i,&kind,&ball,&lvl);
    int x=lx+10+(i%2)*62, y=ly+8+(i/2)*49, w=54, h=44; bool sel=i==pz_eq;
    if(mouse_in(x,y,w,h) && (A.mouse_moved||A.click) && pz_eq!=i){ pz_eq=i; sfx(SFX_HOVER); }
    uint32_t acc= ball ? (IS_EVO(kind)?HEX(0xc08aff):BALLS[kind].col) : HEX(0x8be05a);
    for(int j=0;j<h;j++) gfx_hline(x,x+w-1,y+j,col_lerp(sel?HEX(0x4a2632):HEX(0x2a1820),HEX(0x140a10),(float)j/h));
    orn_frame(x,y,w,h,sel?1:0);
    item_icon(kind,ball,x+w/2,y+h/2,38);
    (void)acc; }
  /* fiche */
  int kind,lvl; bool ball; eq_item(pz_eq,&kind,&ball,&lvl);
  int fx=252, fy=80; char b[320];
  const char *nm= ball ? BALLS[kind].name : PASSIVES[kind].name;
  gothic_draw(fx,fy,nm,HEX(0xfff2ea),HEX(0xe8b8c0),gothic_w(nm,24)<=290?24:12,AL_LEFT,TX_OUTLINE,HEX(0x1a0a10));
  { int y=fy+30; gfx_hline(fx,fx+270,y,HEX(0x8a5a2e)); gfx_hline(fx,fx+270,y+1,HEX(0x2a160c));
    for(int k=0;k<2;k++){ int ex=k?fx+270:fx; gfx_rect(ex-2,y-2,5,5,HEX(0x0a0608)); gfx_rect(ex-1,y-1,3,3,HEX(0xc89458)); } }
  int y=fy+40;
  if(ball && IS_EVO(kind)) snprintf(b,sizeof b,IS_LEG(kind)?"Fusion légendaire (%s + %s)":"Évolution (%s + %s)",BALLS[BALLS[kind].evo_a].name,BALLS[BALLS[kind].evo_b].name);
  else snprintf(b,sizeof b,"Niveau %d",lvl);
  text_draw(fx,y,b,HEX(0xf0e0e8),1,AL_LEFT,TX_SHADOW); y+=16;
  if(ball){ Card c={IS_LEG(kind)?CARD_LEG:(IS_EVO(kind)?CARD_EVO:CARD_UPBALL),kind,0,0}; char head[64], name[64], dmg[64], desc[320]; int rec[9], nrec;
    /* fiche au niveau actuel (et non au suivant) */
    float d=(BALLS[kind].dmg+BALLS[kind].dmg_lvl*(lvl-1))*R.s_dmg; snprintf(dmg,sizeof dmg,"%d-%d dégâts à l'impact",maxi(1,(int)(d+0.5f)),maxi(1,(int)(d*R.s_critm+0.5f)));
    text_draw(fx,y,dmg,HEX(0xf6e0e4),1,AL_LEFT,TX_SHADOW); y+=18;
    ball_desc(kind,lvl,desc,sizeof desc); { char *p=strstr(desc," Dégâts ^1"); if(p) *p=0; }
    text_wrap(fx,y,282,desc,HEX(0xe8d4dc),1,TX_SHADOW,12);
    (void)c; (void)head; (void)name; (void)rec; (void)nrec;
  } else { passive_desc(kind,lvl,b,sizeof b); text_wrap(fx,y,282,b,HEX(0xe8d4dc),1,TX_SHADOW,12); }
}
/* v9.1 : objectifs du niveau, en direct */
static void pause_objectives(void){
  int bx=98, by=74, bw=444, bh=258; char b[200], t[96];
  for(int j=0;j<bh;j++) gfx_hline(bx,bx+bw-1,by+j,col_lerp(HEX(0x2a1a22),HEX(0x1a1016),(float)j/bh));
  orn_frame(bx,by,bw,bh,0);
  if(!(R.mode==MODE_NORMAL && R.single)){
    gothic_title(bx+bw/2,by+16,"Objectifs",24);
    text_wrap(bx+24,by+70,bw-48,R.mode==MODE_RUSH?"Le Rush des gardiens n'a pas d'objectifs de niveau : battez votre record de temps !":"Ce mode de jeu n'a pas d'objectifs de niveau. Ils se jouent dans les niveaux de la liste (mode normal).",HEX(0xe0d0d8),1,TX_SHADOW,12);
    return; }
  int L=R.lvl;
  snprintf(b,sizeof b,"%s",BIOMES[R.biome].name); gothic_title(bx+bw/2,by+12,b,gothic_w(b,24)<=bw-20?24:12);
  int got=0; for(int k=0;k<3;k++) got+=(S.obj_done[L]>>k)&1;
  snprintf(b,sizeof b,"Étoiles de ce niveau : ^1%d / 3^0      Toutes : ^1%d / %d",got,obj_total(),BIOME_COUNT*3);
  text_draw(bx+bw/2,by+42,b,HEX(0xd8c8d0),1,AL_CENTER,TX_SHADOW);
  for(int k=0;k<3;k++){
    int y=by+62+k*62, x=bx+14, w=bw-28, h=54;
    for(int j=0;j<h;j++) gfx_hline(x,x+w-1,y+j,col_lerp(HEX(0x24161c),HEX(0x140a10),(float)j/h));
    orn_frame(x,y,w,h,0);
    bool done=(S.obj_done[L]>>k)&1; int ty=obj_kind(L,k), tgt; int v=obj_value(L,k,&tgt);
    text_draw(x+16,y+18,IC_STAR,done?HEX(0xffd84a):HEX(0x5a4a50),2,AL_CENTER,TX_OUTLINE);
    obj_text(L,k,t,sizeof t); text_draw(x+34,y+7,t,HEX(0xfff0e8),1,AL_LEFT,TX_SHADOW);
    uint32_t ok=HEX(0x8be05a), bad=HEX(0xe85a4a), mid=HEX(0xffd84a);
    float fr=0; uint32_t col=mid; const char *st="";
    if(ty==OB_BOSS){
      if(R.boss.on){ fr=clampf((float)v/tgt,0,1); col= v<tgt?ok:bad; snprintf(b,sizeof b,"Gardien final : %d:%02d / %d:%02d",v/60,v%60,tgt/60,tgt%60); st= v<tgt?"^2dans les temps":"^3trop tard pour cette fois"; }
      else { snprintf(b,sizeof b,"Le chrono démarre quand le gardien final arrive (limite %d:%02d).",tgt/60,tgt%60); st=""; fr=0; } }
    else if(ty==OB_BREACH){ fr=clampf((float)v/maxi(1,tgt),0,1); col= v<=tgt?ok:bad; snprintf(b,sizeof b,"Monstres passés : %d / %d au plus",v,tgt); st= v<=tgt?"^2tenu pour l'instant":"^3raté pour cette descente"; }
    else { fr=clampf((float)v/maxi(1,tgt),0,1); col= v>=tgt?ok:mid; snprintf(b,sizeof b,"Progression : %d / %d",mini(v,tgt),tgt); st= v>=tgt?"^2réussi (validé à la victoire)":""; }
    text_draw(x+34,y+21,b,HEX(0xd8ccd4),1,AL_LEFT,TX_SHADOW);
    if(!(ty==OB_BOSS && !R.boss.on)) ui_bar(x+34,y+35,200,6,fr,col,HEX(0x2a1a20));
    text_draw(x+244,y+33,st,HEX(0xe0d0d8),1,AL_LEFT,TX_SHADOW);
    if(done) text_draw(x+w-8,y+7,"^1déjà obtenu",HEX(0xffffff),1,AL_RIGHT,TX_SHADOW);
  }
  text_draw(bx+bw/2,by+bh-14,"Chaque nouvel objectif rempli à la victoire : une étoile et 2 cristaux.",HEX(0x9a8a98),1,AL_CENTER,0);
}
static void pause_system(void){
  static const char *L[6]={"Encyclopédie","Paramètres","Aide","Succès","Redémarrage rapide","Abandonner"};
  if(pz_confirm>0) pz_confirm-=1.0f/60; else pz_confirm_id=-1;
  if(A.up){ pz_sys=(pz_sys+5)%6; sfx(SFX_HOVER); } if(A.down){ pz_sys=(pz_sys+1)%6; sfx(SFX_HOVER); }
  int act=-1;
  for(int i=0;i<6;i++){ int x=246, y=92+i*38, w=148, h=28; bool hov=mouse_in(x,y,w,h);
    if(hov && A.mouse_moved && pz_sys!=i){ pz_sys=i; sfx(SFX_HOVER); }
    const char *lab=L[i]; if(pz_confirm_id==i) lab= i==5 ? "Vraiment abandonner ?" : "Recommencer ?";
    orn_button(x-(pz_confirm_id==i?16:0),y,w+(pz_confirm_id==i?32:0),h,lab,pz_sys==i,true);
    if(hov && A.click) act=i; }
  if(A.confirm) act=pz_sys;
  if(act<0) return;
  sfx(SFX_CLICK);
  switch(act){
    case 0: meta_open(META_CODEX,GS_RUN); break;
    case 1: options_open(GS_RUN); break;
    case 2: help_open(GS_RUN); break;
    case 3: meta_open(META_ACH,GS_RUN); break;
    case 4: if(pz_confirm_id==4){ pause_reset(); run_quick_restart(); } else { pz_confirm_id=4; pz_confirm=3; } break;
    default: if(pz_confirm_id==5){ pause_reset(); run_abandon(); } else { pz_confirm_id=5; pz_confirm=3; } break;
  }
}
void pause_ui(void){
  gfx_darken(0,0,SCREEN_W,SCREEN_H,150);
  orn_panel(70,20,500,320);
  /* retour */
  { int bx=102, by=34; text_draw(bx-8,by+1,"<",HEX(0xe8d8c8),1,AL_LEFT,TX_SHADOW); int w=pad_badge(bx,by,BG_BACK);
    if(A.click && mouse_in(bx-10,by-2,w+14,18)){ run_resume(); return; } }
  if(R.pause_sel<10) R.pause_sel++;
  if(pz_stats){
    char L[20][96]; int n=detailed_lines(L);
    int x=150, y=40, w=340, h=280; orn_panel(x,y,w,h);
    gothic_title(x+w/2,y+10,"Stats détaillées",24);
    for(int i=0;i<n && i<17;i++) text_draw(x+24,y+44+i*13,L[i],HEX(0xe0d0d8),1,AL_LEFT,TX_SHADOW);
    if(orn_button(x+w/2-50,y+h-30,100,22,"Fermer",true,true) || ((A.back||A.pause||A.confirm||A.reroll) && R.pause_sel>2)){ pz_stats=false; sfx(SFX_CLICK); }
    return;
  }
  if(A.tab_l){ pz_tab=(pz_tab+3)%4; sfx(SFX_HOVER); }
  if(A.tab_r){ pz_tab=(pz_tab+1)%4; sfx(SFX_HOVER); }
  pause_tabs(140,36);
  switch(pz_tab){ case 0: pause_character(); break; case 1: pause_equipment(); break; case 2: pause_objectives(); break; default: pause_system(); break; }
  if(g_state!=GS_RUN || R.sub!=RS_PAUSE) return;
  if((A.back||A.pause) && R.pause_sel>2) run_resume();
}
