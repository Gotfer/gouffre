/* GOUFFRE - ecrans : titre, choix du heros, campement, resultats, options */
#include "game.h"

bool mode_unlocked(int m);
int  ach_for_reward(int rw,int val);
void reward_text(int i,char *b,int n);

/* ---------------- fond : le puits vu d'en haut (tunnel) ---------------- */
#define TW 256
#define TH 64
static uint16_t *t_ang, *t_dep; static uint8_t *t_sh; static uint32_t t_tex[TW*TH]; static uint8_t t_em[TW*TH];
static void tunnel_init(void){
  t_ang=(uint16_t*)malloc(SCREEN_W*SCREEN_H*2); t_dep=(uint16_t*)malloc(SCREEN_W*SCREEN_H*2); t_sh=(uint8_t*)malloc(SCREEN_W*SCREEN_H);
  float cx=SCREEN_W*0.5f, cy=SCREEN_H*0.56f;
  for(int y=0;y<SCREEN_H;y++) for(int x=0;x<SCREEN_W;x++){
    float dx=(x+0.5f-cx)*0.9f, dy=(y+0.5f-cy)*1.25f; float d=sqrtf(dx*dx+dy*dy)+0.001f;
    float a=atan2f(dy,dx); int i=y*SCREEN_W+x;
    t_ang[i]=(uint16_t)((int)((a/TAU+0.5f)*TW*8)&(TW*4-1));
    t_dep[i]=(uint16_t)((int)(4200.0f/d)&0xFFFF);
    float s=clampf((d-16)/330.0f,0,1); s=powf(s,1.35f);
    t_sh[i]=(uint8_t)(s*255);
  }
  /* texture de briques avec torches */
  for(int y=0;y<TH;y++) for(int x=0;x<TW;x++){
    int row=y/8; int off=(row&1)*8; bool mortar=(y%8)==7 || ((x+off)%16)==15;
    float n=fbm(x*0.2f,y*0.2f,5);
    uint32_t c=mortar?HEX(0x141018):col_scale(((x+off)/16+row)%3==0?HEX(0x4a4352):HEX(0x3a3442),0.8f+0.35f*n);
    if(!mortar && (y%8)==0) c=col_scale(c,1.2f);
    t_tex[y*TW+x]=c;
  }
  for(int k=0;k<8;k++){ int tx=k*32+8, ty=(k&1)?20:44;
    for(int j=0;j<4;j++){ t_tex[(ty+j)*TW+tx]=HEX(0x4a3a2a); t_tex[(ty+j)*TW+tx+1]=HEX(0x5c3a24); }
    for(int j=1;j<5;j++) for(int i=-1;i<=2;i++){ if(abs(i)+j>5) continue; t_tex[(ty-j)*TW+tx+i]=j>3?HEX(0xffe080):HEX(0xff8a22); t_em[(ty-j)*TW+tx+i]=(uint8_t)(1+k*20); } }
}
static void tunnel_draw(float t,float speed,int bright){
  if(!t_ang) tunnel_init();
  int rot=(int)(t*14)&(TW*4-1), adv=(int)(t*speed);
  for(int i=0;i<SCREEN_W*SCREEN_H;i++){
    int u=((t_ang[i]+rot)>>2)&(TW-1), v=(t_dep[i]+adv)&(TH-1);
    int sh=t_sh[i]*bright>>8;
    uint32_t c=t_tex[v*TW+u];
    if(t_em[v*TW+u]){ float f=0.75f+0.25f*sinf(t*13+t_em[v*TW+u]); c=col_lerp(HEX(0xff7a22),HEX(0xffe8a0),f-0.5f); sh=mini(255,sh*2+60);
      uint32_t e=0xFF000000u|((uint32_t)((cr(c)*sh)>>8)<<16)|((uint32_t)((cg(c)*sh)>>8)<<8)|(uint32_t)((cb(c)*sh)>>8);
      if(g_gpu){ g_screen[i]=e; g_emis[i]=col_scale(e,0.3f); } else g_screen[i]=e; continue; }
    int r=(cr(c)*sh)>>8, g=(cg(c)*sh*220)>>16, bb=(cb(c)*sh*200)>>16;
    g_screen[i]=0xFF000000u|((uint32_t)r<<16)|((uint32_t)g<<8)|(uint32_t)bb;
  }
  float cx=SCREEN_W*0.5f, cy=SCREEN_H*0.56f;
  if(g_gpu){ fx_glow(cx,cy,90,HEX(0x8a3aff),0.32f+0.1f*sinf(t*1.3f)); fx_glow(cx,cy,26,HEX(0xd090ff),0.7f); fx_glow(cx,cy,8,HEX(0xffe8ff),0.9f); return; }
  gfx_glow(cx,cy,60,HEX(0x8a3aff),0.35f+0.1f*sinf(t*1.3f));
  gfx_glow(cx,cy,18,HEX(0xd090ff),0.4f);
}
/* braises qui montent du puits et billes qui y tombent */
typedef struct { float a,d,v,s; uint32_t c; int kind; } TFly;
static TFly fly[60]; static bool fly_ok=false;
static void fly_update(float dt){
  float cx=SCREEN_W*0.5f, cy=SCREEN_H*0.56f;
  if(!fly_ok){ for(int i=0;i<60;i++){ fly[i].d=rfrange(10,400); fly[i].a=rfrand()*TAU; fly[i].v=rfrange(30,90); fly[i].kind=i<10; fly[i].c=fly[i].kind?BALLS[BK_FER+rirand(BK_LAST_BASE-BK_FER+1)].col:((rfrand()<0.5f)?HEX(0xff8a2a):HEX(0xffd36a)); } fly_ok=true; }
  for(int i=0;i<60;i++){
    TFly *f=&fly[i];
    if(f->kind){ f->d-=dt*f->d*0.55f; f->a+=dt*0.5f; if(f->d<8){ f->d=rfrange(300,420); f->a=rfrand()*TAU; f->c=BALLS[BK_FER+rirand(BK_LAST_BASE-BK_FER+1)].col; } }
    else { f->d+=dt*(f->v+f->d*0.6f); f->a+=dt*0.2f; if(f->d>420){ f->d=rfrange(8,30); f->a=rfrand()*TAU; } }
    float x=cx+cosf(f->a)*f->d/0.9f, y=cy+sinf(f->a)*f->d/1.25f;
    if(f->kind){ float r=1.0f+f->d/90.0f; if(g_gpu) fx_ball(x,y,r,BK_FER+(i%12),1); else art_draw_ball(x,y,r,BK_FER+(i%12),g_time,true); }
    else { int a=(int)(clampf(f->d/120,0,1)*230); if(g_gpu){ fx_disc(x,y,0.45f+f->d/400.0f,f->c,a/255.0f*1.4f); continue; } gfx_padd((int)x,(int)y,f->c,a); if(f->d>150) gfx_padd((int)x+1,(int)y,f->c,a/2); }
  }
}
void menu_backdrop(float dt,int amb);
static void menu_background(float dt,int amb){
  layer(LY_SCENE);
  tunnel_draw(g_time,14,clampi(amb*2,50,200));
  fly_update(dt);
  for(int y=0;y<SCREEN_H;y++){ float v=fabsf((float)(y-180))/180.0f; gfx_rect_a(0,y,SCREEN_W,1,HEX(0x05040a),(int)(v*v*120)); }
  if(g_gpu){
    GP.bloom=S.bloom?0.85f:0; GP.bloom_thr=0.62f; GP.sat=1.1f; GP.con=1.06f; GP.vig=0.7f; GP.chroma=S.bloom?0.15f:0; GP.grain=S.bloom?0.02f:0;
    GP.sh_tint[0]=0.16f; GP.sh_tint[1]=0.08f; GP.sh_tint[2]=0.3f; GP.tint_amt=0.14f; GP.emis_gain=1.0f;
    GP.hi_tint[0]=1.04f; GP.hi_tint[1]=0.99f; GP.hi_tint[2]=0.96f;
    /* le puits : tout est eclaire par une lueur violette qui monte du fond */
    light_begin(0,0,SCREEN_W,SCREEN_H,170,160,190);
    light_add(SCREEN_W*0.5f,SCREEN_H*0.56f,150,HEX(0x9a5aff),0.6f+0.1f*sinf(g_time*1.3f));
    GP.fog[0]=0.6f; GP.fog[1]=0.45f; GP.fog[2]=0.9f; GP.fog[3]=0.18f; GP.fog_speed=2.0f;
  }
}
void menu_bg_public(float dt,int amb){ menu_background(dt,amb); }
void menu_tunnel(float t,float speed,int bright){ tunnel_draw(t,speed,bright); }   /* cinematiques */
void menus_reset_background(void){}

/* ---------------- titre ---------------- */
void title_frame(float dt){
  menu_background(dt,90);
  if(g_gpu) fx_glow(SCREEN_W/2,62,200,HEX(0xff7a2a),0.08f);
  /* les heros en buste tout autour du titre, comme sur une affiche */
  if(g_gpu) layer(LY_UI);
  for(int y=0;y<SCREEN_H;y++){ float v=fabsf((float)y-170)/190.0f; gfx_rect_a(0,y,SCREEN_W,1,HEX(0x05030a),(int)(70+60*v)); }
  title_heroes_draw(g_time);
  /* au centre, un voile sombre garde le titre et les boutons lisibles */
  for(int y=0;y<SCREEN_H;y++){ int a=(int)(130*(0.55f+0.45f*sinf(PI*clampf((float)y/SCREEN_H,0,1)))); for(int k=0;k<3;k++) gfx_rect_a(222+k*20,y,196-k*40,1,HEX(0x05030a),a/4); }
  if(g_gpu){ layer(LY_EMIS); draw_title_logo(SCREEN_W/2,30,g_time); layer(LY_UI); }
  draw_title_logo(SCREEN_W/2,30,g_time);
  if(S.bloom && !g_gpu) post_bloom(SCREEN_W/2-200,20,SCREEN_W/2+200,104,170,0.55f,1.0f);
  text_draw(SCREEN_W/2,108,"Des billes. Des monstres. Un gouffre sans fond.",HEX(0xd8c8b0),1,AL_CENTER,TX_SHADOW);
  static bool t_cine=false;
  ui_begin();
  char b[128];
  if(t_cine){
    /* revoir les cinematiques */
    text_draw(SCREEN_W/2,128,"CINÉMATIQUES",HEX(0xffd84a),1,AL_CENTER,TX_OUTLINE);
    int y=146, w=240, x=SCREEN_W/2-w/2;
    static const char *NM[4]={"La légende du Gouffre","Fin : le Gouffre purifié","Fin : les Profondeurs vaincues","Fin : le Royaume oublié"};
    static const char *LK[4]={"","^7??? (vaincre l'Œil du Gouffre)","^7??? (vaincre le Cœur du Néant)","^7??? (vaincre le Soleil Noir)"};
    for(int i=0;i<4;i++){ bool un=cine_unlocked(i); if(ui_button(x,y,w,20,un?NM[i]:LK[i],un)){ t_cine=false; cine_request(i,GS_TITLE); } y+=24; }
    if(ui_button(x,y+6,w,20,"Retour",true) || A.back){ t_cine=false; ui_set_focus(6); }
  } else {
    int y=126, w=160, x=SCREEN_W/2-w/2;
    if(ui_button(x,y,w,20,"Jouer",true)) goto_state(GS_LEVELS); y+=21;
    if(ui_button(x,y,w,20,"Campement",true)) goto_state(GS_CAMP); y+=21;
    snprintf(b,sizeof b,"Défis  ^1%d/%d",meta_new_count(),ACH_COUNT);
    if(ui_button(x,y,w,20,b,true)) meta_open(META_ACH,GS_TITLE); y+=21;
    { char pb[64]; snprintf(pb,sizeof pb,"Journal  ^1%d %%",(int)completion_pct()); if(ui_button(x,y,w,20,pb,true)) meta_open(META_JOURNAL,GS_TITLE); } y+=21;
    if(ui_button(x,y,w,20,"Codex",true)) meta_open(META_CODEX,GS_TITLE); y+=21;
    if(ui_button(x,y,w,20,"Cinématiques",true)){ t_cine=true; ui_set_focus(0); } y+=21;
    if(ui_button(x,y,w,20,"Options",true)) options_open(GS_TITLE); y+=21;
    if(ui_button(x,y,w,20,"Aide",true)) help_open(GS_TITLE); y+=21;
    if(ui_button(x,y,w,20,"Quitter",true)) g_quit=true;
  }
  ui_end();
  snprintf(b,sizeof b,"^1"IC_COIN" %d^0    ^6"IC_GEM" %d^0    Descentes : %d    Victoires : %d",S.gold,S.crystals,S.runs,S.wins);
  text_draw(SCREEN_W/2,324,b,HEX(0xb0a8c0),1,AL_CENTER,TX_SHADOW);
  text_draw(8,SCREEN_H-14,g_gpu?"v10.2":"v10.2 (rendu logiciel)",HEX(0x6a6478),1,AL_LEFT,0);
  if(A.pad){ snprintf(b,sizeof b,"%s : valider   %s : retour",btn(B_CONFIRM),btn(B_BACK)); text_draw(SCREEN_W-8,SCREEN_H-14,b,HEX(0x8a82a0),1,AL_RIGHT,0); }
  else text_draw(SCREEN_W-8,SCREEN_H-14,A.touch?"Touchez pour choisir":"Souris ou Flèches + Entrée   F11 : plein écran",HEX(0x6a6478),1,AL_RIGHT,0);
  if(g_in && g_in->pad_on){ snprintf(b,sizeof b,"Manette : %s",g_in->pad_name); text_draw(8,SCREEN_H-28,b,HEX(0x6a6478),1,AL_LEFT,0); }
}

/* ---------------- choix du heros ---------------- */
static int cs_sel=-1;
static int duo_ch=0, duo_sel=-1;   /* ecran du duo : heros choisi, compagnon (-1 : aucun) */
void charsel_enter(void){ cs_sel=-1; }

/* ---------------- choix du heros : grille de portraits a gauche, fiche detaillee a droite ---------------- */
static int cs_over=0;          /* 0 : fiche ; 1 : statistiques ; 2 : preparation de la descente */
static int cs_row=0;           /* ligne active de la fenetre de depart */
#define CS_RANDOM CH_COUNT     /* la derniere case : un heros au hasard */
#define CS_COLS 4
static const char *FLAVOR[CH_COUNT]={
  "Elle garde le bord du Gouffre depuis la première nuit, et n'a jamais reculé d'un pas.",
  "Ses fioles changent chaque rebond en brûlure, en poison ou en plaie ouverte.",
  "Il n'a jamais su lancer une seule bille à la fois. Il n'a jamais essayé.",
  "Elle forge ses billes dans le bronze des cloches de la vallée. Elles frappent fort.",
  "Ses aimants rappellent les billes qui tombent et attirent tout ce qui brille.",
  "Chaque monstre qui tombe lui laisse un peu de son âme.",
  "Elle a réglé toutes les horloges de la vallée. Désormais, elle règle le temps.",
  "On l'a chassé du village pour avoir incendié la forge. Le Gouffre, lui, l'a accueilli.",
  "Elle parle aux racines qui percent les parois du Gouffre. Elles lui répondent, et retiennent les monstres.",
  "Il a tout vendu, même sa propre ombre. Il descend racheter ce que le Gouffre a englouti.",
  "Elle a cartographié chaque étoile du ciel. Celles qui tombent dans le Gouffre lui obéissent encore.",
  "Elle a volé des trésors à des rois. Le Gouffre en cache de bien plus grands.",
  "Ce qu'elle lance finit toujours par revenir. Les monstres, eux, ne reviennent jamais.",
  "Il écoute la roche depuis cent ans. Quand il frappe du pied, le Gouffre entier tremble."};
static const char *TALENT[CH_COUNT]={"les brèches font 50 % de dégâts en moins","altérations +80 % au lieu de +60 %","plus aucun malus de dégâts",
  "critiques x2,8 au lieu de x2,5","champ magnétique plus large","18 % de chances de libérer une âme","le temps se fige toutes les 11 s","explosions 50 % plus fortes",
  "régénère 1 PV/s et enracine 30 % du temps","+75 % d'or au lieu de +50 %",
  "une étoile filante toutes les 5 s","marque à +40 % et +12 % de critiques",
  "jusqu'à +30 % de dégâts","une secousse toutes les 7 s"};
/* cadre de bronze orne (sel : dore et lumineux) */
void orn_frame(int x,int y,int w,int h,int sel){
  uint32_t lt= sel?HEX(0xffe08a):HEX(0xc89458), md= sel?HEX(0xd8a040):HEX(0x8a5a2e), dk= sel?HEX(0x7a4a14):HEX(0x4a2c16);
  gfx_frame(x-3,y-3,w+6,h+6,HEX(0x0a0608));
  gfx_hline(x-2,x+w+1,y-2,lt); gfx_vline(x-2,y-2,y+h+1,lt); gfx_hline(x-2,x+w+1,y+h+1,dk); gfx_vline(x+w+1,y-2,y+h+1,dk);
  gfx_frame(x-1,y-1,w+2,h+2,md); gfx_frame(x,y,w,h,HEX(0x1a0e10));
  for(int k=0;k<4;k++){ int cx=(k&1)?x+w:x-1, cy=(k&2)?y+h:y-1;
    gfx_rect(cx-2,cy-2,4,4,HEX(0x0a0608)); gfx_pset(cx-1,cy-1,lt); gfx_pset(cx,cy-1,md); gfx_pset(cx-1,cy,md); gfx_pset(cx,cy,dk);
    gfx_pset(cx+((k&1)?3:-4),cy,md); gfx_pset(cx,cy+((k&2)?3:-4),md); }
  if(sel){ int mx=x+w/2; gfx_tri((float)mx-3,(float)y-3,(float)mx+3,(float)y-3,(float)mx,(float)y+1,lt); gfx_tri((float)mx-3,(float)(y+h+2),(float)mx+3,(float)(y+h+2),(float)mx,(float)(y+h-2),lt); }
}
/* grand panneau : fond sombre tisse, double bordure, fleurons aux coins */
void orn_panel(int x,int y,int w,int h){
  for(int j=0;j<h;j++){ float k=(float)j/h; uint32_t c=col_lerp(HEX(0x2a1a24),HEX(0x1a1018),k); for(int i=0;i<w;i++){ uint32_t cc=c; if(((i+j*3)%7)==0 || ((i*5+j)%11)==0) cc=col_scale(c,0.88f); gfx_pset(x+i,y+j,cc); } }
  gfx_frame(x,y,w,h,HEX(0x0a0608)); gfx_frame(x+1,y+1,w-2,h-2,HEX(0x8a5a2e)); gfx_frame(x+2,y+2,w-4,h-4,HEX(0xc89458)); gfx_frame(x+3,y+3,w-6,h-6,HEX(0x4a2c16)); gfx_frame(x+5,y+5,w-10,h-10,HEX(0x3a2418));
  for(int k=0;k<4;k++){ int cx=(k&1)?x+w-9:x+8, cy=(k&2)?y+h-9:y+8; int sx=(k&1)?-1:1, sy=(k&2)?-1:1;
    gfx_rect(cx-3,cy-3,7,7,HEX(0x0a0608)); gfx_rect(cx-2,cy-2,5,5,HEX(0x8a5a2e)); gfx_rect(cx-1,cy-1,3,3,HEX(0xe8b860)); gfx_pset(cx,cy,HEX(0xfff0c0));
    for(int i=1;i<=7;i++){ gfx_pset(cx+sx*(3+i),cy-sy*1,i&1?HEX(0xc89458):HEX(0x8a5a2e)); gfx_pset(cx-sx*1,cy+sy*(3+i),i&1?HEX(0xc89458):HEX(0x8a5a2e)); } }
}
bool orn_button(int x,int y,int w,int h,const char *label,bool hot,bool enabled){
  bool hover=mouse_in(x,y,w,h);
  bool on=(hot||hover)&&enabled;
  for(int j=0;j<h;j++){ uint32_t c=col_lerp(on?HEX(0x4a2a30):HEX(0x2a1820),on?HEX(0x2a161c):HEX(0x1a0e14),(float)j/h); gfx_hline(x,x+w-1,y+j,c); }
  orn_frame(x,y,w,h,on?1:0);
  text_draw(x+w/2,y+(h-TXT_H)/2+1,label,!enabled?HEX(0x6a5a60):(on?HEX(0xfff0dc):HEX(0xe8d8c8)),1,AL_CENTER,TX_SHADOW);
  bool act=enabled && hover && A.click; if(act) sfx(SFX_CLICK);
  if(!enabled && hover && A.click) sfx(SFX_DENY);
  return act;
}
void orn_padlock(int cx,int cy){
  for(int k=0;k<2;k++){ uint32_t c=k?HEX(0xd89a48):HEX(0x2a1608); int r=6-k;
    for(int a=0;a<=12;a++){ float an=PI+a*PI/12; gfx_pset(cx+(int)roundf(cosf(an)*r),cy-6+(int)roundf(sinf(an)*r),c); gfx_pset(cx+(int)roundf(cosf(an)*(r-1)),cy-6+(int)roundf(sinf(an)*(r-1)),c); } }
  gfx_rect(cx-8,cy-6,17,14,HEX(0x2a1608)); gfx_rect(cx-7,cy-5,15,12,HEX(0xc88a3a)); gfx_rect(cx-7,cy-5,15,2,HEX(0xf0c070)); gfx_rect(cx-7,cy+5,15,2,HEX(0x8a5420));
  gfx_disc(cx,cy-1,2,HEX(0x2a1608)); gfx_rect(cx-1,cy,2,4,HEX(0x2a1608));
}
void orn_emblem(int cx,int cy){
  for(int s=-1;s<=1;s+=2){ for(int i=0;i<16;i++){ int x=cx+s*(10+i), y=cy+(int)(sinf(i*0.5f)*2.5f); gfx_pset(x,y,i&1?HEX(0xc89458):HEX(0x8a5a2e)); gfx_pset(x,y+1,HEX(0x4a2c16)); }
    gfx_tri((float)cx+s*26,(float)cy-3,(float)cx+s*26,(float)cy+4,(float)cx+s*31,(float)cy,HEX(0xc89458)); }
  gfx_disc(cx,cy,10,HEX(0x0a0608)); gfx_disc(cx,cy,9,HEX(0x8a5a2e)); gfx_disc(cx,cy,8,HEX(0xd8a050)); gfx_disc(cx,cy,6,HEX(0x2a1418));
  if(g_gpu) fx_ball(cx,cy,3.5f,BK_LUMIERE,1); else art_draw_ball(cx,cy,3.5f,BK_LUMIERE,g_time,true);
}
/* titre en degrade (ivoire en haut, cuivre en bas), facon enluminure */
void title_text(int cx,int y,const char *T,int sc){
  text_draw(cx,y+sc,T,HEX(0x0a0608),sc,AL_CENTER,0);
  text_draw(cx,y,T,HEX(0x8a4a2a),sc,AL_CENTER,TX_OUTLINE);
  Target *tg=gT; int ox0=tg->cx0, oy0=tg->cy0, ox1=tg->cx1, oy1=tg->cy1;
  gfx_clip(ox0,maxi(oy0,y),ox1,mini(oy1,y+6*sc)); text_draw(cx,y,T,HEX(0xd89a6a),sc,AL_CENTER,0);
  gfx_clip(ox0,maxi(oy0,y),ox1,mini(oy1,y+4*sc)); text_draw(cx,y,T,HEX(0xf6e2c8),sc,AL_CENTER,0);
  gfx_clip(ox0,maxi(oy0,y),ox1,mini(oy1,y+2*sc)); text_draw(cx,y,T,HEX(0xfff8ee),sc,AL_CENTER,0);
  gfx_clip(ox0,oy0,ox1,oy1);
}
/* pastille de bouton de manette (ou touche du clavier) facon Ball x Pit : contour arrondi clair */
int pad_badge(int x,int y,int kind){
  const char *t=""; bool round=false;
  int pt=A.pad?A.pad_type:-1;
  switch(kind){
    case BG_L1: t= pt<0 ? "<" : (pt==PADT_PS?"L1":(pt==PADT_NINTENDO?"L":"LB")); break;
    case BG_R1: t= pt<0 ? ">" : (pt==PADT_PS?"R1":(pt==PADT_NINTENDO?"R":"RB")); break;
    case BG_L2: t= pt<0 ? "Tab" : (pt==PADT_PS?"L2":(pt==PADT_NINTENDO?"ZL":"LT")); break;
    case BG_R2: t= pt<0 ? "Tab" : (pt==PADT_PS?"R2":(pt==PADT_NINTENDO?"ZR":"RT")); break;
    case BG_CONFIRM: if(pt<0) t="Entrée"; else { t=btn(B_CONFIRM); round=true; } break;
    case BG_BACK: if(pt<0) t="Échap"; else { t=btn(B_BACK); round=true; } break;
    case BG_ALT: if(pt<0) t="R"; else { t=btn(B_ALT); round=true; } break;
    default: if(pt<0) t="E"; else { t=btn(B_X); round=true; } break;
  }
  if(round){ uint32_t c=HEX(0xf0e4ec), d=HEX(0x0a0608);
    gfx_disc(x+6,y+6,7,d); gfx_ring(x+6,y+6,6,c); gfx_disc(x+6,y+6,5,HEX(0x2a1a24));
    /* symbole centre au pixel pres dans le rond (5x5 pour les symboles PlayStation) */
    int tp,bt; text_vext(t,&tp,&bt); int tw=text_w(t,1);
    (void)tw; text_draw(x+6,y+6-(tp+bt+1)/2,t,c,1,AL_CENTER,0); return 13; }
  return text_badge(x,y,t);
}
/* touche de clavier ou gachette : cadre qui s'agrandit si le texte a un accent ou un jambage */
int text_badge(int x,int y,const char *t){
  int tw=text_w(t,1); int tp,bt; text_vext(t,&tp,&bt); int th=bt-tp+1;
  int w=tw+8, h=maxi(13,th+4), y0=y+(13-h)/2;
  uint32_t c=HEX(0xf0e4ec), d=HEX(0x0a0608);
  gfx_rect(x,y0,w,h,d); gfx_rect(x+1,y0+1,w-2,h-2,HEX(0x2a1a24));
  gfx_hline(x+2,x+w-3,y0+1,c); gfx_hline(x+2,x+w-3,y0+h-2,c); gfx_vline(x+1,y0+2,y0+h-3,c); gfx_vline(x+w-2,y0+2,y0+h-3,c);
  text_draw(x+w/2,y0+2+((h-4)-th)/2-tp,t,c,1,AL_CENTER,0);
  return w;
}
void draw_check(int x,int y,uint32_t c){
  static const char *M[8]={"......##.",".....###.","....###..","##.###...","#####....",".###.....","..#......","........."};
  for(int j=0;j<8;j++) for(int i=0;i<9;i++) if(M[j][i]=='#'){ gfx_pset(x+i+1,y+j+1,HEX(0x0a0608)); }
  for(int j=0;j<8;j++) for(int i=0;i<9;i++) if(M[j][i]=='#') gfx_pset(x+i,y+j, j<3?col_lerp(c,HEX(0xffffff),0.35f):c);
}
/* deux portraits superposes : le compagnon en retrait a droite, le heros devant */
void duo_portrait(int x,int y,int w,int h,int ch,int ch2,int scale){
  for(int j=0;j<h;j++) gfx_hline(x,x+w-1,y+j,col_lerp(HEX(0x4a2a3a),HEX(0x140a12),(float)j/h));
  int ox0=gT->cx0, oy0=gT->cy0, ox1=gT->cx1, oy1=gT->cy1; gfx_clip(maxi(ox0,x),maxi(oy0,y),mini(ox1,x+w),mini(oy1,y+h));
  if(ch2>=0){ const Sprite *p=portrait(ch2,false); DrawOpt o=dopt(); o.sx=o.sy=(float)scale; o.flags=DS_NOOCC; o.tint=HEX(0xb8a8b8);
    draw_spr_c(p,x+w*0.5f+w*0.24f,y+h-p->h*scale*0.5f+2,&o); }
  { const Sprite *p=portrait(ch,false); DrawOpt o=dopt(); o.sx=o.sy=(float)scale; o.flags=DS_NOOCC;
    draw_spr_c(p,x+w*0.5f-(ch2>=0?w*0.14f:0),y+h-p->h*scale*0.5f+(ch2>=0?4:0),&o); }
  gfx_clip(ox0,oy0,ox1,oy1);
}
const char *hero_pair_name(int ch,int ch2,char *b,int n){
  if(ch2>=0) snprintf(b,n,"%s & %s",CHARS[ch].name,CHARS[ch2].name); else snprintf(b,n,"%s",CHARS[ch].name);
  return b;
}
static bool cs_unlocked(int c){ return c==CS_RANDOM || S.unlocked[c]; }
static void cs_card(int i,int x,int y,int w,int h,bool sel){
  for(int j=0;j<h;j++){ uint32_t c=col_lerp(HEX(0x3a2230),HEX(0x160c14),(float)j/h); gfx_hline(x,x+w-1,y+j,c); }
  if(sel && g_gpu) fx_glow(x+w*0.5f,y+h*0.45f,40,i<CH_COUNT?CHARS[i].col:HEX(0xffd84a),0.18f);
  if(i==CS_RANDOM){
    text_draw(x+w/2,y+h/2-16,"?",HEX(0xe8c070),4,AL_CENTER,TX_OUTLINE);
    text_draw(x+w/2,y+h-14,"Au hasard",HEX(0xd8c8b8),1,AL_CENTER,TX_SHADOW);
  } else {
    bool un=S.unlocked[i];
    const Sprite *p=portrait(i,!un);
    int ox0=gT->cx0, oy0=gT->cy0, ox1=gT->cx1, oy1=gT->cy1; gfx_clip(x,y,x+w,y+h);
    draw_spr(p,x+(w-p->w)/2,y+h-p->h,DS_NOOCC);
    gfx_clip(ox0,oy0,ox1,oy1);
    if(!un){ orn_padlock(x+w/2,y+h/2-2);
      if(!CHARS[i].unlock_ach){ char b[24]; snprintf(b,sizeof b,"^6"IC_GEM"^0 %d",CHARS[i].cost); gfx_rect_a(x+w/2-16,y+h-14,32,11,HEX(0x0a0608),200); text_draw(x+w/2,y+h-13,b,HEX(0xf0e6d0),1,AL_CENTER,0); } }
    else if(S.hero_xp[i]==0 && i!=CH_SENTINELLE){ int bw=text_w("Nouveau",1)+8; gfx_rect(x+w-bw-2,y+2,bw,11,HEX(0x0a0608)); gfx_rect(x+w-bw-1,y+3,bw-2,9,HEX(0x8a5a2e)); text_draw(x+w-bw/2-2,y+4,"Nouveau",HEX(0xfff0c0),1,AL_CENTER,0); }
    else { int rk=hero_rank(i); if(rk>1){ char rb[16]; snprintf(rb,sizeof rb,rk>=RANK_MAX?"^1"IC_STAR"10":"^1%d",rk); text_draw(x+w-4,y+3,rb,HEX(0xffffff),1,AL_RIGHT,TX_OUTLINE); } }
  }
  orn_frame(x,y,w,h,sel?1:0);
}
/* ---------------- ecran du duo : « Emmener un 2e perso ? » ---------------- */
static void duo_cycle(int dir){
  int order[CH_COUNT+1], n=0; order[n++]=-1; for(int c=0;c<CH_COUNT;c++) if(S.unlocked[c] && c!=duo_ch) order[n++]=c;
  int cur=0; for(int i=0;i<n;i++) if(order[i]==duo_sel) cur=i;
  duo_sel=order[(cur+dir+n)%n]; sfx(SFX_HOVER);
}
static void duo_hero(int cx,int ch,bool companion){
  char b[96];
  int fw=132, fx=cx-fw/2, fy=78;
  text_draw(cx,62,ch>=0?CHARS[ch].name:"Aucun",HEX(0xf6e8e0),1,AL_CENTER,TX_SHADOW);
  for(int j=0;j<fw;j++) gfx_hline(fx,fx+fw-1,fy+j,col_lerp(HEX(0x4a2a3a),HEX(0x140a12),(float)j/fw));
  if(ch>=0){ if(g_gpu) fx_glow(fx+fw*0.5f,fy+fw*0.4f,80,CHARS[ch].col,0.14f);
    const Sprite *p=portrait(ch,false); DrawOpt o=dopt(); o.sx=o.sy=2; o.flags=DS_NOOCC; draw_spr_c(p,fx+fw*0.5f,fy+fw-p->h+0.0f,&o); }
  else text_draw(cx,fy+fw/2-28,"?",HEX(0x8a6a5a),8,AL_CENTER,TX_OUTLINE);
  orn_frame(fx,fy,fw,fw,companion?1:0);
  int by=fy+fw+12;
  if(ch>=0){ int k=CHARS[ch].start_ball; gfx_disc(cx,by+24,25,HEX(0x0a0608)); gfx_disc(cx,by+24,24,HEX(0x24141c));
    if(g_gpu) fx_glow(cx,by+24,34,BALLS[k].col,0.22f);
    icon_draw(k,(float)cx,(float)by+24,50,NULL);
    text_draw(cx,by+54,BALLS[k].name,HEX(0xf0e0d8),1,AL_CENTER,TX_SHADOW);
    snprintf(b,sizeof b,"^1%s",CHARS[ch].trait); text_draw(cx,by+66,b,HEX(0xffffff),1,AL_CENTER,TX_SHADOW); }
  else text_draw(cx,by+20,"Descendre seul",HEX(0x9a8a90),1,AL_CENTER,TX_SHADOW);
  if(companion){
    for(int s2=-1;s2<=1;s2+=2){ int ax=cx+s2*(fw/2+14), ay=fy+fw/2; bool hov=mouse_in(ax-8,ay-10,16,20);
      uint32_t c=hov?HEX(0xfff0c0):HEX(0xd89a48);
      gfx_tri((float)ax-s2*6,(float)ay-8,(float)ax-s2*6,(float)ay+8,(float)ax+s2*6,(float)ay,HEX(0x0a0608));
      gfx_tri((float)ax-s2*5,(float)ay-6,(float)ax-s2*5,(float)ay+6,(float)ax+s2*4,(float)ay,c);
      if(hov && A.click) duo_cycle(s2); }
    if(mouse_in(fx,fy,fw,fw) && A.click) duo_cycle(1);
  }
}
void charsel_debug_duo(int arg){ cs_sel=CH_SENTINELLE; duo_ch=CH_SENTINELLE; duo_sel= arg>0 ? CH_PYROMANE : -1; cs_over=2; }
static void duo_screen(void){
  int x=104, y=10, w=432, h=338;
  orn_panel(x,y,w,h);
  /* retour */
  { int bx=x+12, by=y+12; text_draw(bx,by+1,"<",HEX(0xe8d8c8),1,AL_LEFT,TX_SHADOW); pad_badge(bx+8,by,BG_BACK);
    if(mouse_in(bx-2,by-2,30,16) && A.click){ cs_over=0; sfx(SFX_CLICK); return; } }
  gothic_title(320,y+14,"Emmener un 2e perso ?",24);
  duo_hero(214,duo_ch,false);
  duo_hero(426,duo_sel,true);
  /* la croix entre les deux */
  for(int k=-1;k<=1;k++){ gfx_line(311+k,135,329+k,153,HEX(0xd8c8c0)); gfx_line(329+k,135,311+k,153,HEX(0xd8c8c0)); }
  if(A.left) duo_cycle(-1); if(A.right) duo_cycle(1);
  if(duo_sel>=0) text_draw(320,298,"^3À deux : monstres et gardiens +50 % de PV, +25 % de dégâts",HEX(0xffffff),1,AL_CENTER,TX_SHADOW);
  bool go=orn_button(230,312,180,24,"Continuer",true,true) || A.confirm;
  if(A.back){ cs_over=0; sfx(SFX_CLICK); return; }
  if(go){
    S.last_char=duo_ch; S.duo_last=duo_sel; save_write(); sfx(SFX_SELECT);
    start_run_request(duo_ch,duo_sel,PICK.depth,PICK.mode,PICK.level); cs_sel=-1; cs_over=0; return;
  }
}
void charsel_frame(float dt){
  if(cs_sel<0){ cs_sel=S.last_char; if(cs_sel<0||cs_sel>=CH_COUNT) cs_sel=0; cs_over=0; cs_row=4; }
  menu_background(dt,40);
  gfx_darken(0,0,SCREEN_W,SCREEN_H,150);
  if(g_gpu){ GP.blur_all=0.6f; layer(LY_UI); }
  char b[256];
  /* ---- grille ---- */
  const int GX=12, GY=56, CWd=63, CHt=64, GAP=5;   /* v9 : 12 heros + le hasard sur 4 rangees */
  orn_panel(4,4,282,352);
  { int pw=170, px=4+141-pw/2; for(int j=0;j<18;j++){ uint32_t c=col_lerp(HEX(0x4a2c1c),HEX(0x24140e),(float)j/18); gfx_hline(px,px+pw-1,8+j,c); } orn_frame(px,8,pw,18,0);
    text_draw(4+141,12,"Choisir un héros",HEX(0xf0e0c8),1,AL_CENTER,TX_SHADOW); }
  orn_emblem(4+141,38);
  /* fermer */
  { int x=14, y=30; bool hov=mouse_in(x,y,18,18); for(int j=0;j<18;j++) gfx_hline(x,x+17,y+j,col_lerp(HEX(0x3a2026),HEX(0x1a0e12),(float)j/18)); orn_frame(x,y,18,18,hov?1:0);
    uint32_t xc=hov?HEX(0xfff0dc):HEX(0xd8c8b8); for(int k=0;k<10;k++){ gfx_pset(x+4+k,y+4+k,xc); gfx_pset(x+5+k,y+4+k,xc); gfx_pset(x+13-k,y+4+k,xc); gfx_pset(x+14-k,y+4+k,xc); }
    if(hov && A.click && cs_over==0){ sfx(SFX_CLICK); cs_sel=-1; goto_state(GS_LEVELS); return; } }
  for(int i=0;i<=CS_RANDOM;i++){
    int x=GX+(i%CS_COLS)*(CWd+GAP), y=GY+(i/CS_COLS)*(CHt+GAP);
    if(cs_over==0 && mouse_in(x,y,CWd,CHt) && (A.click || (A.mouse_moved && false))){ if(cs_sel!=i){ cs_sel=i; sfx(SFX_HOVER); } else if(A.click && cs_unlocked(i)){ cs_over=3; sfx(SFX_CLICK); } }
    cs_card(i,x,y,CWd,CHt,i==cs_sel);
    /* coche : ce heros a deja termine le niveau choisi a cette profondeur */
    if(i<CH_COUNT && PICK.mode==MODE_NORMAL && ((lvl_heroes_diff(PICK.level,PICK.depth,S.difficulty)>>i)&1)){ gfx_rect(x+3,y+3,13,12,HEX(0x0a0608)); draw_check(x+5,y+5,HEX(0x8be05a)); }
  }
  if(cs_over==0){
    const int NR=CS_RANDOM/CS_COLS+1;
    int c=cs_sel%CS_COLS, r=cs_sel/CS_COLS;
    if(A.left){ c=(c+CS_COLS-1)%CS_COLS; } if(A.right){ c=(c+1)%CS_COLS; } if(A.up){ r=(r+NR-1)%NR; } if(A.down){ r=(r+1)%NR; }
    int ns=r*CS_COLS+c; if(ns>CS_RANDOM) ns=CS_RANDOM; if(ns!=cs_sel){ cs_sel=ns; sfx(SFX_HOVER); }
  }
  /* ---- fiche ---- */
  orn_panel(292,4,344,352);
  int sel=cs_sel==CS_RANDOM ? -1 : cs_sel;
  const CharDef *cd= sel>=0 ? &CHARS[sel] : NULL;
  bool un= sel<0 || S.unlocked[sel];
  /* grand portrait */
  { int fx=306, fy=20, fw=132, fh=132;
    for(int j=0;j<fh;j++) gfx_hline(fx,fx+fw-1,fy+j,col_lerp(HEX(0x4a2a3a),HEX(0x140a12),(float)j/fh));
    if(g_gpu && sel>=0 && un) fx_glow(fx+fw*0.5f,fy+fh*0.4f,90,cd->col,0.15f);
    if(sel>=0){ const Sprite *p=portrait(sel,!un); DrawOpt o=dopt(); o.sx=o.sy=2; o.flags=DS_NOOCC; draw_spr_c(p,fx+fw*0.5f,fy+fh-p->h+0.0f,&o); if(!un) orn_padlock(fx+fw/2,fy+fh/2); }
    else text_draw(fx+fw/2,fy+fh/2-28,"?",HEX(0xe8c070),8,AL_CENTER,TX_OUTLINE);
    orn_frame(fx,fy,fw,fh,1);
    for(int k=0;k<2;k++){ int ex=k?fx+fw+3:fx-8, ey=fy+fh/2; gfx_rect(ex,ey-6,5,12,HEX(0x0a0608)); gfx_rect(ex+1,ey-5,3,10,HEX(0xc89458)); gfx_pset(ex+2,ey,HEX(0xfff0c0)); } }
  int rx=452, rw=176, rcx=rx+rw/2;
  if(sel>=0){
    const char *nm=cd->name; int nw=text_w(nm,2);
    if(nw<=rw) title_text(rcx,24,nm,2);
    else { const char *sp=strchr(nm,' '); if(sp){ char a1[32]; int l=(int)(sp-nm); memcpy(a1,nm,(size_t)l); a1[l]=0; text_draw(rcx,14,a1,HEX(0xc8b8a8),1,AL_CENTER,TX_SHADOW); if(text_w(sp+1,2)<=rw) title_text(rcx,26,sp+1,2); else text_draw(rcx,28,sp+1,HEX(0xf6e8d8),1,AL_CENTER,TX_OUTLINE); } else text_draw(rcx,24,nm,HEX(0xf6e8d8),1,AL_CENTER,TX_OUTLINE); }
    if(un){
      int rk=hero_rank(sel), need; int xp=hero_rank_xp(sel,&need);
      snprintf(b,sizeof b,"Rang %d",rk); text_draw(rcx,46,b,HEX(0xe0d0c0),1,AL_CENTER,TX_SHADOW);
      int bx=rx+6, by=60, bw=rw-12, bh=8;
      gfx_rect(bx-3,by-3,bw+6,bh+6,HEX(0x0a0608)); gfx_rect(bx-2,by-2,bw+4,bh+4,HEX(0x8a5a2e)); gfx_rect(bx-1,by-1,bw+2,bh+2,HEX(0x2a140c)); gfx_rect(bx,by,bw,bh,HEX(0x2a0e14));
      float f= need>0 ? (float)xp/need : 1.0f; int fw2=(int)(bw*clampf(f,0,1));
      for(int j=0;j<bh;j++) gfx_hline(bx,bx+fw2-1,by+j,col_lerp(HEX(0xd8384a),HEX(0x6a1420),(float)j/bh));
      if(fw2>2) gfx_vline(bx+fw2-1,by,by+bh-1,HEX(0xff9aa0));
      for(int k=-1;k<=1;k+=2){ int ex= k<0?bx-6:bx+bw+3; gfx_disc(ex+1,by+bh/2,3,HEX(0x0a0608)); gfx_disc(ex+1,by+bh/2,2,HEX(0xc89458)); }
      if(need>0) snprintf(b,sizeof b,"%d / %d",xp,need); else snprintf(b,sizeof b,"Rang maximum");
      text_draw(rcx,74,b,HEX(0xe0d0c0),1,AL_CENTER,TX_SHADOW);
    } else {
      if(cd->unlock_ach){ int a=ach_for_reward(RW_HERO,sel); snprintf(b,sizeof b,"^3Verrouillé^0 : défi ^1%s",a>=0?ACHS[a].name:"?"); }
      else snprintf(b,sizeof b,"^3Verrouillé^0 : ^6"IC_GEM" %d^0 cristaux",cd->cost);
      text_wrap(rx+4,46,rw-8,b,HEX(0xe0d0c0),1,TX_SHADOW,11);
    }
    /* bille de depart */
    { int ix=rx+22, iy=98; gfx_disc(ix+16,iy+16,17,HEX(0x0a0608)); gfx_disc(ix+16,iy+16,16,HEX(0x2a1418));
      icon_draw(cd->start_ball,ix+16.0f,iy+16.0f,34,NULL);
      if(g_gpu) fx_glow(ix+16,iy+16,22,BALLS[cd->start_ball].col,0.2f);
      text_draw(ix+46,iy+12,BALLS[cd->start_ball].name,HEX(0xe8d8c8),1,AL_LEFT,TX_SHADOW); }
    /* textes */
    text_wrap(306,166,316,FLAVOR[sel],HEX(0xe8c8d0),1,TX_SHADOW,11);
    snprintf(b,sizeof b,"^1%s :^0 %s",cd->trait,cd->desc); text_wrap(306,206,316,b,HEX(0xe8d8e0),1,TX_SHADOW,11);
    if(un && hero_rank(sel)>=5){ snprintf(b,sizeof b,"^2Talent renforcé^0 : %s",TALENT[sel]); text_wrap(306,240,316,b,HEX(0xb0a8c0),1,0,11); }
    /* petits insignes */
    { bool won=(S.hero_wins>>sel)&1, mx=un && hero_rank(sel)>=RANK_MAX; int iy=262;
      for(int k=0;k<2;k++){ int ix=rcx-60+k*20-108; bool lit= k==0?won:mx; gfx_rect(ix,iy,16,16,HEX(0x0a0608)); gfx_rect(ix+1,iy+1,14,14,lit?HEX(0x4a2a1a):HEX(0x1e1416));
        text_draw(ix+8,iy+3,k==0?IC_SKULL:IC_STAR,lit?HEX(0xffd84a):HEX(0x4a3a40),1,AL_CENTER,0);
        if(mouse_in(ix,iy,16,16)){ const char *tt= k==0?(won?"A déjà terminé un niveau":"N'a encore terminé aucun niveau"):(mx?"Rang 10 : tenue dorée":"Rang 10 : tenue dorée (pas encore)"); int tw=text_w(tt,1)+10; gfx_rect_a(ix,iy-15,tw,13,HEX(0x0a0810),235); text_draw(ix+5,iy-13,tt,HEX(0xf0e6d0),1,AL_LEFT,0); } } }
  } else {
    title_text(rcx,30,"Au hasard",2);
    text_wrap(rx,56,rw,"Un héros débloqué, tiré au sort au moment de descendre.",HEX(0xe0d0c0),1,TX_SHADOW,11);
    text_wrap(306,166,316,"Laissez le Gouffre décider. Chaque héros gagne son expérience comme d'habitude.",HEX(0xe8c8d0),1,TX_SHADOW,11);
  }
  /* boutons */
  bool can_go = sel<0 || un;
  /* rappel du niveau choisi */
  { char lb[96]; if(PICK.mode==MODE_ENDLESS) snprintf(lb,sizeof lb,"^6Mode infini^0   profondeur %d",PICK.depth);
    else if(PICK.mode==MODE_RUSH) snprintf(lb,sizeof lb,"^6Rush des gardiens^0 : %s",PICK.level>=2*LEVELS_PER_DEST?"Royaume oublié":(PICK.level>=LEVELS_PER_DEST?"Profondeurs":"Gouffre"));
    else snprintf(lb,sizeof lb,"^1Niveau %d^0 : %s   prof. %d",PICK.level+1,BIOMES[PICK.level].name,PICK.depth);
    text_draw(4+141,336,lb,HEX(0xd8c8b8),1,AL_CENTER,TX_SHADOW); }
  bool buy = sel>=0 && !un && !cd->unlock_ach;
  if(orn_button(306,300,128,22,"Plus de stats…",false,sel>=0) && cs_over==0){ cs_over=1; }
  { int ix=446, iy=305; for(int k=0;k<3;k++){ int h2=4+k*3; gfx_rect(ix+k*4,iy+12-h2,3,h2,HEX(0xc89458)); } }
  if(buy){ snprintf(b,sizeof b,"Débloquer ^6"IC_GEM"^0 %d",cd->cost);
    if(orn_button(486,296,128,30,b,cs_over==0,S.crystals>=cd->cost) && cs_over==0){ S.crystals-=cd->cost; S.unlocked[sel]=true; save_write(); sfx(SFX_EVOLVE); meta_check(); } }
  else if(orn_button(486,296,128,30,"Choisir",cs_over==0,can_go) && cs_over==0){ cs_over=3; }
  if(cs_over==0){
    if(A.confirm){ if(buy){ if(S.crystals>=cd->cost){ S.crystals-=cd->cost; S.unlocked[sel]=true; save_write(); sfx(SFX_EVOLVE); meta_check(); } else sfx(SFX_DENY); }
      else if(can_go){ cs_over=3; sfx(SFX_CLICK); } else sfx(SFX_DENY); }
    else if((A.reroll||A.tab_r) && sel>=0){ cs_over=1; sfx(SFX_CLICK); }
    else if(A.back){ cs_sel=-1; goto_state(GS_LEVELS); return; }
    if(A.pad) snprintf(b,sizeof b,"%s : choisir   %s : plus de stats   %s : retour",btn(B_CONFIRM),btn(B_ALT),btn(B_BACK));
    else if(A.touch) snprintf(b,sizeof b,"Touchez un héros, puis Choisir   Échap : retour");
    else snprintf(b,sizeof b,"Flèches : héros   Entrée : choisir   R : plus de stats   Échap : retour");
    text_draw(464,340,b,HEX(0x8a7a80),1,AL_CENTER,0);
    return;
  }
  /* « Choisir » : tirage du heros au hasard, puis l'ecran du duo (s'il y a un compagnon possible) */
  if(cs_over==3){
    bool rnd= sel<0;
    int ch=sel; if(ch<0){ int list[CH_COUNT], n=0; for(int c=0;c<CH_COUNT;c++) if(S.unlocked[c]) list[n++]=c; ch=list[rirand(n)]; }
    int nun=0; for(int c=0;c<CH_COUNT;c++) nun+=S.unlocked[c];
    /* v10.2 : « Au hasard » tire deux heros differents a chaque fois (le compagnon aussi) */
    duo_ch=ch; duo_sel=(S.duo_last>=0 && S.duo_last!=ch && S.unlocked[S.duo_last]) ? S.duo_last : -1;
    if(rnd && nun>=2){ int list[CH_COUNT], n=0; for(int c=0;c<CH_COUNT;c++) if(S.unlocked[c] && c!=ch) list[n++]=c; duo_sel=list[rirand(n)]; }
    if(nun<2){ S.last_char=ch; save_write(); start_run_request(ch,-1,PICK.depth,PICK.mode,PICK.level); cs_sel=-1; cs_over=0; return; }
    cs_over=2;
  }
  /* ---- fenetres par-dessus la fiche ---- */
  gfx_rect_a(0,0,SCREEN_W,SCREEN_H,HEX(0x05030a),150);
  if(cs_over==1){
    int x=150, y=40, w=340, h=280; orn_panel(x,y,w,h);
    title_text(x+w/2,y+14,"Statistiques",2);
    int hp=(int)cd->hp+12*S.bld[BLD_INFIRMERIE]+hero_hp_bonus(sel)+(int)train_bonus(sel,TR_VITA);
    const char *L[8]; char l0[96],l1[96],l2[96],l3[96],l4[96],l5[96],l6[96],l7[96];
    snprintf(l0,sizeof l0,"Points de vie : ^1%d",hp);
    snprintf(l1,sizeof l1,"Vitesse : ^1%d %%",(int)(cd->speed*(1+train_bonus(sel,TR_VITE))*100+0.5f));
    if(train_level(sel,TR_PUIS)>0) snprintf(l2,sizeof l2,"Dégâts : ^1%d %%^0   (rang : ^2+%d %%^0, entraînement : ^2+%d %%^0)",(int)(cd->dmg*100+0.5f),(int)(hero_dmg_bonus(sel)*100+0.5f),(int)(train_bonus(sel,TR_PUIS)*100+0.5f));
    else snprintf(l2,sizeof l2,"Dégâts : ^1%d %%^0   (rang : ^2+%d %%^0)",(int)(cd->dmg*100+0.5f),(int)(hero_dmg_bonus(sel)*100+0.5f));
    snprintf(l3,sizeof l3,"Cadence : ^1%d %%",(int)(cd->fire*(1+train_bonus(sel,TR_CADE))*100+0.5f));
    snprintf(l4,sizeof l4,"Petites billes : ^1%d",cd->babies+(hero_rank(sel)>=3?1:0));
    snprintf(l5,sizeof l5,"Bille de départ : ^1%s",BALLS[cd->start_ball].name);
    snprintf(l6,sizeof l6,"Expérience totale : ^1%d",S.hero_xp[sel]);
    snprintf(l7,sizeof l7,"%s : %s",hero_rank(sel)>=5?"^2Talent renforcé^0":"^7Talent du rang 5^0",TALENT[sel]);
    L[0]=l0; L[1]=l1; L[2]=l2; L[3]=l3; L[4]=l4; L[5]=l5; L[6]=l6; L[7]=l7;
    for(int i=0;i<7;i++) text_draw(x+24,y+46+i*18,L[i],HEX(0xe0d0c0),1,AL_LEFT,TX_SHADOW);
    text_wrap(x+24,y+46+7*18,w-48,L[7],HEX(0xc0b0b8),1,0,11);
    bool cl=orn_button(x+w/2-50,y+h-34,100,22,"Fermer",true,true);
    if(cl || A.back || A.confirm || A.reroll){ cs_over=0; sfx(SFX_CLICK); }
    return;
  }
  /* « Emmener un 2e perso ? » */
  duo_screen();
}

/* ---------------- campement ---------------- */
static int camp_sel=0; static float camp_fx[BLD_COUNT];
void camp_enter(void){ ui_set_focus(-1); bld_sprites_refresh(); }
void camp_set_sel(int b){ camp_sel=clampi(b,0,BLD_COUNT-1); }
void camp_flash(int b){ if(b>=0&&b<BLD_COUNT) camp_fx[b]=0.6f; }
static const int HUTS[CH_COUNT][2]={{276,196},{364,196},{220,224},{420,224},{320,262},{190,198},{450,198},{184,240},{212,170},{428,170},{124,200},{516,200},{264,176},{376,176}};
typedef struct { float x,y,vx,vy,t; } Firefly;
static Firefly ff[24]; static bool ff_ok=false;
/* fond du campement precalcule (dossier « donnees ») : ciel, collines, sol, palissade, foyer */
static uint32_t *cb_px=NULL; static uint8_t *cb_em=NULL; static int cb_state=0;
static bool camp_backdrop(void){
  if(cb_state==0){ size_t n=(size_t)SCREEN_W*SCREEN_H; cb_state=-1;
    if(hd_available()){ cb_px=(uint32_t*)malloc(n*4); cb_em=(uint8_t*)malloc(n);
      if(cb_px && cb_em && hd_region_low(HD_CAMP+hd_season(),5,cb_px,cb_em,NULL,SCREEN_W,SCREEN_H)) cb_state=1;
      else { free(cb_px); free(cb_em); cb_px=NULL; cb_em=NULL; } } }
  return cb_state==1;
}
static void camp_scene(float dt){
  bool hdb=camp_backdrop(), hdg=false;
  if(hdb){
    if(g_gpu){ hd_gpu_select(HD_CAMP+hd_season()); hd_frame(0,0); hdg=hd_gpu_active(HD_CAMP+hd_season()); }
    for(int i=0;i<SCREEN_W*SCREEN_H;i++){ uint32_t c=cb_px[i]&0xFFFFFFu; g_screen[i]= hdg ? c|((cb_em[i]==7?7u:6u)<<24) : c|0xFF000000u; }
  } else gfx_gradient_v(0,0,SCREEN_W,140,HEX(0x07071a),HEX(0x29223f));
  for(int i=0;i<90;i++){ uint32_t h=(uint32_t)i*2654435761u; int x=(int)(h%SCREEN_W), y=(int)((h>>12)%120); int a=(int)(120+120*sinf(g_time*(1+(h&7)*0.3f)+i));
    if(hdb){ if(g_gpu && (h>>3)%5==0 && cb_em[y*SCREEN_W+x]==7) fx_star(x+0.5f,y+0.5f,2.0f,HEX(0xd8d8ff),a/255.0f*0.4f); continue; }
    if(g_gpu){ int ly=layer_current(); layer(LY_EMIS); gfx_pset(x,y,col_scale(HEX(0xd8d8ff),a/255.0f)); layer(ly); if((h>>3)%9==0) fx_star(x+0.5f,y+0.5f,2.2f,HEX(0xd8d8ff),a/255.0f*0.5f); }
    else gfx_padd(x,y,HEX(0xd8d8ff),a); }
  { float ph=fmodf(g_time*0.13f,1.0f); if(ph<0.08f){ float k=ph/0.08f; float x=120+k*260, y=20+k*50; if(g_gpu) fx_caps(x,y,x-18,y-4,0.6f,HEX(0xffffff),1.4f*(1-k)); else gfx_capsule_add(x,y,x-14,y-3,0.6f,HEX(0xffffff),(int)(220*(1-k))); } }
  if(g_gpu){ fx_glow(548,40,60,HEX(0xb8b4ff),hdb?0.16f:0.22f); }
  else gfx_glow(548,40,30,HEX(0xd8d4ff),hdb?0.12f:0.25f);
  if(!hdb){
    gfx_disc(548,40,13,HEX(0xd8d4c0)); gfx_disc(545,37,11,HEX(0xf4f0dc)); gfx_disc(550,44,2,HEX(0xd8d4c0)); gfx_disc(543,35,1,HEX(0xd8d4c0));
    for(int x=0;x<SCREEN_W;x++){ float h=118+sinf(x*0.013f)*10+sinf(x*0.031f+1)*6; gfx_vline(x,(int)h,140,HEX(0x15122a)); float h2=126+sinf(x*0.021f+2)*6; gfx_vline(x,(int)h2,140,HEX(0x1c1830)); }
    gfx_gradient_v(0,138,SCREEN_W,SCREEN_H-138,HEX(0x2a3022),HEX(0x161a12));
    for(int i=0;i<500;i++){ uint32_t h=(uint32_t)(i+7)*2246822519u; int x=(int)(h%SCREEN_W), y=140+(int)((h>>11)%220); gfx_pset(x,y,(h>>5)&1?HEX(0x3a4a2a):HEX(0x222a1a)); if((h>>9)%9==0){ gfx_pset(x,y-1,HEX(0x4a6a32)); } }
    /* le gouffre au loin */
    gfx_ellipse(320,152,48,10,HEX(0x2a1a3a)); gfx_ellipse(320,152,42,8,HEX(0x05030a));
  }
  if(g_gpu) fx_glow(320,150,64,HEX(0x8a3aff),0.3f+0.1f*sinf(g_time*1.2f)); else
  gfx_glow(320,152,50,HEX(0x8a3aff),0.22f+0.08f*sinf(g_time*1.2f));
  if(!hdb){
    /* palissade */
    for(int x=-4;x<SCREEN_W;x+=9){ if(x>266&&x<374) continue; gfx_rect(x,128,7,22,HEX(0x4a3020)); gfx_rect(x+1,128,5,20,HEX(0x6a4428)); gfx_tri((float)x,128,(float)x+7,128,(float)x+3.5f,122,HEX(0x6a4428)); }
    /* chemins */
    for(int b=0;b<BLD_COUNT;b++){ float x0=320,y0=226,x1=(float)BUILDINGS[b].x,y1=(float)BUILDINGS[b].y-2; for(int k=0;k<=20;k++){ float t=k/20.0f; gfx_ellipse_a((int)lerpf(x0,x1,t),(int)lerpf(y0,y1,t),6,2,HEX(0x4a3a28),120); } }
    /* feu de camp */
    gfx_ellipse(320,228,16,5,HEX(0x2a2420));
    for(int i=0;i<8;i++){ float a=i*TAU/8; gfx_disc((int)(320+cosf(a)*14),(int)(228+sinf(a)*4),2,HEX(0x6a6472)); }
    gfx_thick_line(310,229,330,225,1.5f,HEX(0x5c3a24)); gfx_thick_line(310,225,330,229,1.5f,HEX(0x4a2e1c));
  }
  { int ly=layer_current(); if(g_gpu) layer(LY_EMIS);
  for(int i=0;i<3;i++){ float f=sinf(g_time*12+i*2.1f); int h=10+(int)(f*3)-i*2; gfx_tri(313.0f+i*5,226,323.0f+i*5,226,318.0f+i*5+f,(float)(226-h),i==1?HEX(0xffb040):HEX(0xff7a22)); }
  gfx_tri(316,226,324,226,320+sinf(g_time*9)*1.5f,215,HEX(0xffe080));
  if(g_gpu){ layer(ly); fx_glow(320,220,40,HEX(0xff8a3a),0.35f); fx_glow(320,221,12,HEX(0xffe0a0),0.6f); } }
  (void)dt;
}
static void hut_draw(int ch){
  if(!S.unlocked[ch]) return;
  int hx=HUTS[ch][0], hy=HUTS[ch][1];
  draw_spr(&SPR_HUT[ch],hx-SPR_HUT[ch].w/2,hy-SPR_HUT[ch].h,0);
  if(hero_rank(ch)>=RANK_MAX){ int ly=layer_current(); if(g_gpu) layer(LY_EMIS); text_draw(hx,hy-SPR_HUT[ch].h-12+(int)(sinf(g_time*3+ch)*1.5f),IC_STAR,HEX(0xffd84a),1,AL_CENTER,TX_OUTLINE); if(g_gpu) layer(ly); }
  /* le heros devant sa tente */
  int spr=CHARS[ch].sprite; const Sprite *s=&SPR_LO[spr];
  float ph=g_time*0.6f+ch*1.3f; float wx=sinf(ph)*7; bool walking=fabsf(cosf(ph))>0.25f;
  if(walking && SPR_LO_WALK[spr][0].px){ int f=((int)(g_time*7+ch))&3; if(f==1) s=&SPR_LO_WALK[spr][0]; else if(f==3) s=&SPR_LO_WALK[spr][1]; }
  DrawOpt o=dopt(); if(cosf(ph)<0) o.flags=DS_FLIPX;
  int fx=hx+16+(int)wx, fy=hy+2;
  gfx_ellipse_a(fx,fy,5,1,HEX(0x000000),110);
  draw_spr_c(s,(float)fx,(float)fy-s->h/2,&o);
}
/* le campement sans interface (fete de la cinematique de fin) ; les lumieres sont appliquees par l'appelant */
void camp_world(float dt,float amb){
  camp_scene(dt);
  for(int pass=0;pass<2;pass++){
    for(int b=0;b<BLD_COUNT;b++){
      if((BUILDINGS[b].y>200)!=(pass==1)) continue;
      int X=BUILDINGS[b].x, Y=BUILDINGS[b].y, rx,ry,rw,rh; bld_camp_rect(b,X,Y,&rx,&ry,&rw,&rh);
      gfx_ellipse_a(rx+rw/2,Y,rw/2-2,4,HEX(0x000000),100);
      bld_camp_draw(b,X,Y,NULL);
      bld_camp_fx(b,X,Y);
    }
    if(pass==0) for(int c=0;c<CH_COUNT;c++) if(HUTS[c][1]<=200) hut_draw(c);
    if(pass==1) for(int c=0;c<CH_COUNT;c++) if(HUTS[c][1]>200) hut_draw(c);
  }
  light_begin(0,0,SCREEN_W,SCREEN_H,(int)((g_gpu?78:96)*amb),(int)((g_gpu?82:100)*amb),(int)((g_gpu?132:150)*amb));
  float f=0.85f+0.15f*sinf(g_time*13)+0.05f*sinf(g_time*31);
  light_add(320,220,180,HEX(0xffa050),1.4f*f);
  light_add(320,152,70,HEX(0x9a5aff),0.4f);
  for(int b=0;b<BLD_COUNT;b++) if(S.bld[b]>0){ int rx,ry,rw,rh; bld_camp_rect(b,BUILDINGS[b].x,BUILDINGS[b].y,&rx,&ry,&rw,&rh); light_add((float)BUILDINGS[b].x,(float)BUILDINGS[b].y-rh*0.35f,52,HEX(0xffd080),0.45f); }
  if(g_gpu){ light_add(548,40,220,HEX(0x8a90d0),0.25f); for(int c=0;c<CH_COUNT;c++) if(S.unlocked[c]) light_add((float)HUTS[c][0],(float)HUTS[c][1]-10,30,CHARS[c].col,0.25f); }
  for(int i=0;i<12;i++){ float ph=fmodf(g_time*0.8f+i*0.083f,1.0f); float x=320+sinf(i*7.0f+g_time)*7, y=214-ph*64; fx_disc(x,y,0.6f,HEX(0xffb040),1.6f*(1-ph)); }
  if(g_gpu){
    GP.bloom=S.bloom?0.8f:0; GP.bloom_thr=0.7f; GP.sat=1.1f; GP.con=1.05f; GP.vig=0.6f;
    GP.sh_tint[0]=0.06f; GP.sh_tint[1]=0.06f; GP.sh_tint[2]=0.19f; GP.tint_amt=0.14f; GP.hi_tint[0]=1.05f; GP.hi_tint[1]=1.0f; GP.hi_tint[2]=0.94f;
    GP.fog[0]=0.55f; GP.fog[1]=0.6f; GP.fog[2]=0.9f; GP.fog[3]=0.12f; GP.fog_speed=0.6f;
  }
}
void camp_frame(float dt){
  camp_scene(dt);
  int hovered=-1;
  bld_sprites_refresh();
  for(int b=0;b<BLD_COUNT;b++){
    int rx,ry,rw,rh; bld_camp_rect(b,BUILDINGS[b].x,BUILDINGS[b].y,&rx,&ry,&rw,&rh);
    if(mouse_in(rx+4,ry+6,rw-8,rh-6)) hovered=b;
  }
  if(hovered>=0 && A.mouse_moved && camp_sel!=hovered){ camp_sel=hovered; sfx(SFX_HOVER); }
  for(int pass=0;pass<2;pass++){
    for(int b=0;b<BLD_COUNT;b++){
      if((BUILDINGS[b].y>200)!=(pass==1)) continue;
      int X=BUILDINGS[b].x, Y=BUILDINGS[b].y, rx,ry,rw,rh; bld_camp_rect(b,X,Y,&rx,&ry,&rw,&rh);
      float cx=(float)X, by=(float)Y;
      gfx_ellipse_a(rx+rw/2,Y,rw/2-2,4,HEX(0x000000),100);
      if(b==camp_sel){ int a=(int)(120+80*sinf(g_time*5)); gfx_ellipse_a(rx+rw/2,Y,rw/2+2,6,HEX(0xffd84a),a/3); }
      DrawOpt o=dopt();
      if(camp_fx[b]>0){ o.flash=HEX(0xfff0c0); o.flash_a=(int)(camp_fx[b]*200); camp_fx[b]-=dt; }
      bld_camp_draw(b,X,Y,&o);
      bld_camp_fx(b,X,Y);
      int mx=BUILDINGS[b].max; int px=(int)cx-(mx*5)/2;
      for(int i=0;i<mx;i++){ gfx_rect(px+i*5,(int)by+3,4,3,HEX(0x0a0810)); gfx_rect(px+i*5,(int)by+3,3,2,i<S.bld[b]?HEX(0xffd84a):HEX(0x3a3242)); }
    }
    if(pass==0) for(int c=0;c<CH_COUNT;c++) if(HUTS[c][1]<=200) hut_draw(c);
    if(pass==1) for(int c=0;c<CH_COUNT;c++) if(HUTS[c][1]>200) hut_draw(c);
  }
  { int rx,ry,rw,rh; bld_camp_rect(camp_sel,BUILDINGS[camp_sel].x,BUILDINGS[camp_sel].y,&rx,&ry,&rw,&rh); int cx=BUILDINGS[camp_sel].x, ty=ry-6+(int)(sinf(g_time*6)*2);
    int ly=layer_current(); if(g_gpu) layer(LY_UI);
    gfx_tri((float)cx-4,(float)ty-4,(float)cx+4,(float)ty-4,(float)cx,(float)ty+1,HEX(0xffd84a)); layer(ly); }
  /* lumieres */
  light_begin(0,0,SCREEN_W,SCREEN_H,g_gpu?70:92,g_gpu?76:98,g_gpu?128:150);
  float f=0.85f+0.15f*sinf(g_time*13)+0.05f*sinf(g_time*31);
  light_add(320,220,160,HEX(0xffa050),1.25f*f);
  light_add(320,152,70,HEX(0x9a5aff),0.55f);
  for(int b=0;b<BLD_COUNT;b++) if(S.bld[b]>0) light_add((float)BUILDINGS[b].x,(float)BUILDINGS[b].y-18,48,b==BLD_LABO?HEX(0xa0f080):(b==BLD_AUTEL?HEX(0xff9a4a):HEX(0xffd080)),0.35f+0.025f*S.bld[b]);
  light_add((float)BUILDINGS[camp_sel].x,(float)BUILDINGS[camp_sel].y-20,52,HEX(0xfff0c0),0.35f);
  if(g_gpu){ light_add(548,40,220,HEX(0x8a90d0),0.25f); for(int c=0;c<CH_COUNT;c++) if(S.unlocked[c]) light_add((float)HUTS[c][0],(float)HUTS[c][1]-10,30,CHARS[c].col,0.25f); }
  light_apply();
  /* lucioles et etincelles */
  if(!ff_ok){ for(int i=0;i<24;i++){ ff[i].x=rfrange(20,620); ff[i].y=rfrange(150,330); ff[i].t=rfrand()*10; } ff_ok=true; }
  for(int i=0;i<24;i++){ Firefly *q=&ff[i]; q->t+=dt; q->x+=sinf(q->t*0.7f+i)*12*dt; q->y+=cosf(q->t*0.9f+i*2)*8*dt; int a=(int)(140+110*sinf(q->t*3+i));
    if(g_gpu){ fx_glow(q->x,q->y,4,HEX(0xc8ff6a),a/255.0f*0.6f); fx_disc(q->x,q->y,0.6f,HEX(0xe8ffa0),a/255.0f*1.3f); if(a>200) light_add(q->x,q->y,14,HEX(0xb8ff5a),0.2f); }
    else { gfx_padd((int)q->x,(int)q->y,HEX(0xd8ff7a),a); gfx_padd((int)q->x+1,(int)q->y,HEX(0x9af05a),a/3); } }
  for(int i=0;i<8;i++){ float ph=fmodf(g_time*0.8f+i*0.13f,1.0f); float x=320+sinf(i*7.0f+g_time)*6, y=214-ph*56; if(g_gpu) fx_disc(x,y,0.55f,HEX(0xffb040),1.6f*(1-ph)); else gfx_padd((int)x,(int)y,HEX(0xffb040),(int)(255*(1-ph))); }
  if(S.bloom && !g_gpu){ post_bloom(0,0,SCREEN_W,SCREEN_H,150,0.6f,0.8f); post_grade(0,0,SCREEN_W,SCREEN_H,1.08f,1.04f,HEX(0x101030),0.12f,0.6f); }
  if(g_gpu){
    GP.bloom=S.bloom?0.8f:0; GP.bloom_thr=0.7f; GP.sat=1.1f; GP.con=1.05f; GP.vig=0.6f; GP.chroma=S.bloom?0.1f:0; GP.grain=S.bloom?0.02f:0;
    GP.sh_tint[0]=0.06f; GP.sh_tint[1]=0.06f; GP.sh_tint[2]=0.19f; GP.tint_amt=0.14f; GP.hi_tint[0]=1.05f; GP.hi_tint[1]=1.0f; GP.hi_tint[2]=0.94f;
    GP.fog[0]=0.55f; GP.fog[1]=0.6f; GP.fog[2]=0.9f; GP.fog[3]=0.14f; GP.fog_speed=0.6f; GP.emis_gain=1.0f;
    layer(LY_UI);
  }
  /* barre du haut */
  gfx_rect_a(0,0,SCREEN_W,30,HEX(0x0a0810),190); gfx_hline(0,SCREEN_W,30,HEX(0x3a2a1a));
  text_draw(12,6,"CAMPEMENT DE FORT-REBOND",HEX(0xffd84a),1,AL_LEFT,TX_OUTLINE);
  char b[160];
  snprintf(b,sizeof b,"^1"IC_COIN" %d^0     ^6"IC_GEM" %d^0     "IC_PRISM" %d",S.gold,S.crystals,S.prisms); text_draw(12,18,b,HEX(0xf0e6d0),1,AL_LEFT,TX_SHADOW);
  ui_begin();
  { char jb[32], db[32]; const char *lab[8];
    snprintf(jb,sizeof jb,"Journal %d%%",(int)completion_pct()); snprintf(db,sizeof db,"Défis %d/%d",meta_new_count(),ACH_COUNT);
    lab[0]=A.pad?"     Amélioration":"Amélioration"; lab[1]=A.pad?"     Entraînement":"Entraînement"; lab[2]=jb; lab[3]=db; lab[4]="Codex"; lab[5]="Menu"; lab[6]="Expédition "IC_ARROW; lab[7]="?";
    int w[8], tot=0; for(int i=0;i<8;i++){ w[i]=text_w(lab[i],1)+(i<2?24:13); tot+=w[i]+3; }
    int x=SCREEN_W-3-tot; int act=-1;
    for(int i=0;i<8;i++){ if(ui_button(x,5,w[i],20,lab[i],true)) act=i;
      bool any=false;
      if(i==0) for(int q=0;q<BLD_COUNT;q++) if(S.bld[q]<BUILDINGS[q].max && S.gold>=building_cost(q,S.bld[q]+1) && S.crystals>=building_crystals(q,S.bld[q]+1)) any=true;
      if(i==1) any=train_any_affordable();
      if(any){ float p=0.5f+0.5f*sinf(g_time*4+i); gfx_tri((float)x+w[i]-11,(float)17,(float)x+w[i]-4,(float)17,(float)x+w[i]-7.5f,(float)11-p,HEX(0x8be05a)); }
      if(i<2 && A.pad) pad_badge(x+5,9,i==0?BG_ALT:BG_X);
      x+=w[i]+3; }
    if(act==7 || (g_in&&g_in->key_pressed[KEY_F1])) help_open(GS_CAMP);
    else if(act==6 || (g_in&&(g_in->key_pressed[KEY_E]||g_in->pad_pressed[PAD_START]))){ goto_state(GS_LEVELS); }
    else if(act==5 || A.back){ goto_state(GS_TITLE); }
    else if(act==4) meta_open(META_CODEX,GS_CAMP);
    else if(act==3) meta_open(META_ACH,GS_CAMP);
    else if(act==2) meta_open(META_JOURNAL,GS_CAMP);
    else if(act==1 || (g_in&&(g_in->key_pressed[KEY_T]||g_in->pad_pressed[PAD_X]))){ sfx(SFX_CLICK); training_open(S.last_char,GS_CAMP); }
    else if(act==0 || A.reroll){ sfx(SFX_CLICK); upgrade_open(camp_sel,GS_CAMP); } }
  /* panneau d'information */
  int bsel=camp_sel; const BuildingDef *bd=&BUILDINGS[bsel]; int lvl=S.bld[bsel];
  ui_fancy_panel(8,292,624,64);
  snprintf(b,sizeof b,"%s",bd->name); text_draw(18,297,b,HEX(0xffd84a),1,AL_LEFT,TX_OUTLINE);
  snprintf(b,sizeof b,"Niveau %d / %d",lvl,bd->max); text_draw(18+text_w(bd->name,1)+10,297,b,HEX(0x9a94a8),1,AL_LEFT,0);
  text_draw(18,310,bd->desc,HEX(0xd8d0e4),1,AL_LEFT,0);
  char e1[64], e2[64]; building_effect(bsel,lvl,e1,sizeof e1);
  if(lvl<bd->max){ building_effect(bsel,lvl+1,e2,sizeof e2); snprintf(b,sizeof b,"^7Actuel :^0 %s   ^7"IC_ARROW" Suivant :^2 %s",e1,e2); }
  else snprintf(b,sizeof b,"^7Actuel :^0 %s   ^1(niveau maximum)",e1);
  text_draw(18,323,b,HEX(0xd8d0e4),1,AL_LEFT,0);
  if(A.pad){ char hb[200]; snprintf(hb,sizeof hb,"Flèches : choisir   %s : Amélioration   %s : Entraînement   %s : expédition   %s : menu",btn(B_ALT),btn(B_X),btn(B_START),btn(B_BACK)); text_draw(18,339,hb,HEX(0x8a82a0),1,AL_LEFT,0); }
  else if(A.touch) text_draw(18,339,"Touchez un bâtiment ou une tente   Échap : menu",HEX(0x6a6478),1,AL_LEFT,0);
  else text_draw(18,339,"Flèches ou clic : choisir   R : Amélioration   T : Entraînement   E : expédition   Échap : menu",HEX(0x6a6478),1,AL_LEFT,0);
  /* le campement se regarde seulement : on ameliore dans le menu « Amélioration » */
  { int rx=620;
    if(lvl>=bd->max){ text_draw(rx,304,"Niveau maximum",HEX(0xffd84a),1,AL_RIGHT,TX_SHADOW); }
    else { int gc=building_cost(bsel,lvl+1), cc=building_crystals(bsel,lvl+1);
      text_draw(rx,300,lvl==0?"Construction":"Niveau suivant",HEX(0x9a94a8),1,AL_RIGHT,0);
      if(cc>0) snprintf(b,sizeof b,"%s"IC_COIN" %d   %s"IC_GEM" %d",S.gold>=gc?"^1":"^3",gc,S.crystals>=cc?"^6":"^3",cc);
      else snprintf(b,sizeof b,"%s"IC_COIN" %d",S.gold>=gc?"^1":"^3",gc);
      text_draw(rx,312,b,HEX(0xffffff),1,AL_RIGHT,TX_SHADOW);
      text_draw(rx,324,"dans le menu Amélioration",HEX(0x7a7488),1,AL_RIGHT,0); } }
  ui_end();
  /* les tentes des heros : un clic ouvre leur entrainement */
  if(hovered<0 && A.my>30 && A.my<290){
    for(int c=0;c<CH_COUNT;c++){ if(!S.unlocked[c]) continue; int hx=HUTS[c][0], hy=HUTS[c][1], hw=SPR_HUT[c].w, hh=SPR_HUT[c].h;
      if(!mouse_in(hx-hw/2-2,hy-hh-2,hw+28,hh+6)) continue;
      int ly=layer_current(); if(g_gpu) layer(LY_UI);
      char lb[96]; snprintf(lb,sizeof lb,"%s  ^7- entraînement "IC_PRISM,CHARS[c].name);
      int tw=text_w(lb,1)+10; int ty=hy-hh-16; gfx_rect_a(hx-tw/2,ty-2,tw,12,HEX(0x0a0810),200); gfx_frame(hx-tw/2,ty-2,tw,12,HEX(0xc9a24a));
      text_draw(hx,ty,lb,HEX(0xffd84a),1,AL_CENTER,0); layer(ly);
      if(A.click){ sfx(SFX_CLICK); training_open(c,GS_CAMP); }
      break; } }
  if(hovered>=0 && A.click && A.my<290){ camp_sel=hovered; sfx(SFX_HOVER); }
  if(A.left){ camp_sel=(camp_sel+BLD_COUNT-1)%BLD_COUNT; sfx(SFX_HOVER); ui_set_focus(-1); }
  if(A.right){ camp_sel=(camp_sel+1)%BLD_COUNT; sfx(SFX_HOVER); ui_set_focus(-1); }
  if(A.up||A.down){ camp_sel=(camp_sel+4)%BLD_COUNT; sfx(SFX_HOVER); ui_set_focus(-1); }
}

/* ---------------- resultats ---------------- */
void results_frame(float dt){
  menu_background(dt,40);
  gfx_darken(0,0,SCREEN_W,SCREEN_H,150);
  if(g_gpu){ GP.blur_all=0.6f; layer(LY_UI); }
  bool single = LAST.lvl>=0 && LAST.mode!=MODE_ENDLESS && LAST.mode!=MODE_RUSH;
  const char *title = LAST.mode==MODE_RUSH ? (LAST.won?"Rush terminé !":"Fin du rush") : (LAST.won ? (single?"Niveau terminé !":"Victoire !") : (LAST.mode==MODE_ENDLESS ? "Fin de l'ascension" : "Fin de la descente"));
  if(LAST.won) gfx_glow(SCREEN_W/2,30,140,HEX(0xffc040),0.2f);
  if(LAST.won) gothic_draw(SCREEN_W/2,14,title,HEX(0xfff6c8),HEX(0xe8a040),24,AL_CENTER,TX_OUTLINE,HEX(0x1a0a08));
  else gothic_draw(SCREEN_W/2,14,title,HEX(0xffe0d8),HEX(0xd87060),24,AL_CENTER,TX_OUTLINE,HEX(0x1a0a08));
  orn_panel(116,48,408,250);
  char b[200]; int y=60;
  /* heros (et compagnon) */
  { int fx=128, fy=60, fw=56, fh=56; duo_portrait(fx,fy,fw,fh,LAST.ch,LAST.ch2,1); orn_frame(fx,fy,fw,fh,LAST.won?1:0); }
  { char nb[96]; hero_pair_name(LAST.ch,LAST.ch2,nb,sizeof nb); text_draw(194,y+1,nb,HEX(0xffffff),1,AL_LEFT,TX_SHADOW); }
  snprintf(b,sizeof b,"Niveau du héros : ^1%d",LAST.plevel); text_draw(194,y+14,b,HEX(0xb0a8c0),1,AL_LEFT,0);
  if(LAST.mode==MODE_ENDLESS) snprintf(b,sizeof b,"Étage atteint : ^1%d%s",LAST.stage,LAST.record?"   ^6Record !":"");
  else if(LAST.mode==MODE_RUSH) snprintf(b,sizeof b,"Rush des gardiens : ^1%d^0 / %d gardiens%s",LAST.won?LEVELS_PER_DEST:maxi(0,LAST.stage-1),LEVELS_PER_DEST,LAST.won&&LAST.record?"   ^6Record !":"");
  else snprintf(b,sizeof b,"^1Niveau %d^0 : %s   prof. %d%s",clampi(LAST.biome,0,BIOME_COUNT-1)+1,BIOMES[clampi(LAST.biome,0,BIOME_COUNT-1)].name,LAST.depth,LAST.diff==DIFF_FACILE?"   ^2(Facile)":(LAST.diff==DIFF_DIFFICILE?"   ^3(Difficile)":""));
  text_draw(194,y+26,b,HEX(0xb0a8c0),1,AL_LEFT,0);
  if(LAST.mode==MODE_DAILY){ snprintf(b,sizeof b,"Défi du jour : ^1%d points%s",LAST.score,LAST.record?"   ^6Record du jour !":""); text_draw(194,y+38,b,HEX(0xb0a8c0),1,AL_LEFT,0); }
  else if(single && !LAST.won) { snprintf(b,sizeof b,"Gardiens mineurs vaincus : ^1%d^0 / 2",LAST.mb_kills); text_draw(194,y+38,b,HEX(0xb0a8c0),1,AL_LEFT,0); }
  /* rangs des heros */
  for(int k=0;k<2;k++){ int ch= k?LAST.ch2:LAST.ch; if(ch<0) continue;
    int r0=k?LAST.rank0b:LAST.rank0, r1=k?LAST.rank1b:LAST.rank1, hx=k?LAST.hxp2:LAST.hxp; int need; int xp=hero_rank_xp(ch,&need);
    int ry=y+2+k*26;
    if(r1>r0) snprintf(b,sizeof b,"^1Rang %d "IC_ARROW" %d !",r0,r1); else snprintf(b,sizeof b,"Rang %d",r1);
    text_draw(512,ry,b,r1>r0?HEX(0xffd84a):HEX(0xd8d0e4),1,AL_RIGHT,TX_SHADOW);
    snprintf(b,sizeof b,"^1+%d^0 XP",hx); text_draw(512,ry+11,b,HEX(0xb0a8c0),1,AL_RIGHT,0);
    if(need>0) ui_bar(432,ry+21,80,3,(float)xp/need,HEX(0xffc040),HEX(0x1e1828)); }
  y=124;
  int s2=(int)LAST.time;
  text_draw(136,y,IC_CLOCK"  Temps",HEX(0x9a94a8),1,AL_LEFT,0); snprintf(b,sizeof b,"%d:%02d%s",s2/60,s2%60,LAST.record_time?"  ^6record":""); text_draw(306,y,b,HEX(0xf0e6d0),1,AL_RIGHT,0);
  text_draw(334,y,IC_SKULL"  Monstres",HEX(0x9a94a8),1,AL_LEFT,0); snprintf(b,sizeof b,"%d",LAST.kills); text_draw(504,y,b,HEX(0xf0e6d0),1,AL_RIGHT,0); y+=14;
  text_draw(136,y,"^1"IC_COIN"^0  Or rapporté",HEX(0x9a94a8),1,AL_LEFT,0); snprintf(b,sizeof b,"^1+%d",LAST.gold); text_draw(306,y,b,HEX(0xf0e6d0),1,AL_RIGHT,0);
  text_draw(334,y,"^6"IC_GEM"^0  Cristaux",HEX(0x9a94a8),1,AL_LEFT,0); snprintf(b,sizeof b,"^6+%d",LAST.crystals); text_draw(504,y,b,HEX(0xf0e6d0),1,AL_RIGHT,0); y+=14;
  text_draw(136,y,IC_PRISM"  Prismes",HEX(0x9a94a8),1,AL_LEFT,0); snprintf(b,sizeof b,"^4+%d",LAST.prisms); text_draw(306,y,b,HEX(0xf0e6d0),1,AL_RIGHT,0);
  if(LAST.prisms>0) text_draw(334,y,"^7à dépenser à l'Entraînement",HEX(0x9a94a8),1,AL_LEFT,0);
  y+=16;
  gfx_hline(136,504,y,HEX(0x3a2a1a)); y+=6;
  for(int i=0;i<LAST.nballs;i++){ icon_draw(LAST.balls[i],144.0f+i*74,y+7.0f,19,NULL); snprintf(b,sizeof b,IS_EVO(LAST.balls[i])?"^6"IC_STAR:"%d",LAST.ball_lvl[i]); text_draw(156+i*74,y+3,b,HEX(0xd8d0e4),1,AL_LEFT,0); }
  y+=22;
  /* v9 : objectifs du niveau */
  if(single && LAST.won && LAST.lvl>=0){
    int x=136; text_draw(x,y+1,"Objectifs :",HEX(0xc9a24a),1,AL_LEFT,TX_SHADOW); x+=text_w("Objectifs : ",1);
    for(int k=0;k<3;k++){ char t[80]; obj_text(LAST.lvl,k,t,sizeof t); bool ok=(LAST.obj_met>>k)&1, nw=(LAST.obj_new>>k)&1;
      snprintf(b,sizeof b,"%s",ok?(nw?"^1"IC_STAR:"^2"IC_STAR):"^7"IC_STAR); text_draw(x+k*14,y+1,b,HEX(0xffffff),1,AL_LEFT,TX_OUTLINE);
      if(mouse_in(x+k*14-2,y-2,14,14)){ int tw=text_w(t,1)+10; gfx_rect_a(x+k*14,y-15,tw,13,HEX(0x0a0810),235); text_draw(x+k*14+5,y-13,t,HEX(0xf0e6d0),1,AL_LEFT,0); } }
    int nn=0; for(int k=0;k<3;k++) nn+=(LAST.obj_new>>k)&1;
    if(nn){ snprintf(b,sizeof b,"^6+%d "IC_GEM"^0 (%d nouveau%s)",2*nn,nn,nn>1?"x":""); text_draw(x+50,y+1,b,HEX(0xffffff),1,AL_LEFT,TX_SHADOW); }
    y+=14; }
  /* coches gagnees sur ce niveau */
  if(LAST.new_checks){
    text_draw(136,y+3,"Nouvelle coche :",HEX(0xc9a24a),1,AL_LEFT,TX_SHADOW);
    int cx=232; for(int c=0;c<CH_COUNT;c++) if((LAST.new_checks>>c)&1){ const Sprite *sp=&SPR_LO[CHARS[c].sprite]; DrawOpt o=dopt(); o.flags=DS_NOOCC; draw_spr_c(sp,cx+8.0f,y+6.0f,&o); draw_check(cx+12,y-2,HEX(0x8be05a)); cx+=26; }
    snprintf(b,sizeof b,"^1+%d "IC_COIN,LAST.check_gold); text_draw(cx+6,y+3,b,HEX(0xffffff),1,AL_LEFT,TX_SHADOW); y+=18;
  }
  if(LAST.nstars>0){
    char sb2[160]; sb2[0]=0;
    for(int i=0;i<LAST.nstars && i<3;i++){ size_t l=strlen(sb2); snprintf(sb2+l,sizeof sb2-l,"%s%s ^1%d"IC_STAR"^0",l?"   ":"",BALLS[LAST.stars_k[i]].name,LAST.stars_n[i]); }
    snprintf(b,sizeof b,"^6Maîtrise^0 : %s",sb2); text_draw(136,y,b,HEX(0xd8d0e4),1,AL_LEFT,0); y+=13;
  }
  if(LAST.nachs>0){
    int maxl = mini(LAST.nachs, maxi(1,(250-y)/12-1));
    text_draw(136,y,"DÉFIS ACCOMPLIS",HEX(0xc9a24a),1,AL_LEFT,TX_SHADOW); y+=12;
    for(int i=0;i<LAST.nachs && i<maxl;i++){ char rb[96]; reward_text(LAST.achs[i],rb,sizeof rb); snprintf(b,sizeof b,"^1"IC_STAR"^0 %s",ACHS[LAST.achs[i]].name); text_draw(142,y,b,HEX(0xf0e6d0),1,AL_LEFT,0); text_draw(504,y,rb,HEX(0xf0e6d0),1,AL_RIGHT,0); y+=11; }
    if(LAST.nachs>maxl){ snprintf(b,sizeof b,"… et %d autres",LAST.nachs-maxl); text_draw(142,y,b,HEX(0x8a82a0),1,AL_LEFT,0); y+=11; }
  }
  /* nouvelles ouvertures */
  { int ny=258;
    if(LAST.next_unlocked && LAST.lvl+1<BIOME_COUNT){ snprintf(b,sizeof b,"^4Niveau %d débloqué : %s !",LAST.lvl+2,BIOMES[LAST.lvl+1].name); text_draw(SCREEN_W/2,ny,b,HEX(0xffffff),1,AL_CENTER,TX_SHADOW); ny+=12; }
    if(LAST.new_dest){ snprintf(b,sizeof b,"^5Nouvelle destination : le Royaume oublié (niveau %d, %s) !",2*LEVELS_PER_DEST+1,BIOMES[2*LEVELS_PER_DEST].name); text_draw(SCREEN_W/2,ny,b,HEX(0xffffff),1,AL_CENTER,TX_SHADOW); ny+=12; }
    if(LAST.new_depth && single){ snprintf(b,sizeof b,"^6Profondeur %d débloquée pour ce niveau !",lvl_depth_max(LAST.lvl)); text_draw(SCREEN_W/2,ny,b,HEX(0xffffff),1,AL_CENTER,TX_SHADOW); ny+=12; }
    else if(!LAST.won && LAST.mode==MODE_NORMAL && S.levels_won==0 && S.runs>=3 && S.difficulty!=DIFF_FACILE && LAST.nachs<=3 && ny<270){
      text_draw(SCREEN_W/2,ny,"Bloqué ? La difficulté ^2Facile^0 se règle dans la liste des niveaux",HEX(0xd8d0e4),1,AL_CENTER,TX_SHADOW); ny+=12; }
    snprintf(b,sizeof b,"Total : ^1"IC_COIN" %d^0   ^6"IC_GEM" %d^0   "IC_PRISM" %d",S.gold,S.crystals,S.prisms); text_draw(SCREEN_W/2,282,b,HEX(0xb0a8c0),1,AL_CENTER,0); }
  ui_begin();
  if(ui_button(150,306,160,24,"Retour au campement",true)) goto_state(GS_CAMP);
  if(ui_button(330,306,160,24,"Choisir un niveau",true)) goto_state(GS_LEVELS);
  ui_end();
  if(A.left){ ui_set_focus(0); sfx(SFX_HOVER); } if(A.right){ ui_set_focus(1); sfx(SFX_HOVER); }
}

/* ---------------- options ---------------- */
static int opt_ret=GS_TITLE; static int reset_confirm=0; static bool opt_test=false; static float test_hold=0;
void options_open(int ret){ opt_ret=ret; reset_confirm=0; opt_test=false; ui_set_focus(0); goto_state(GS_OPTIONS); }
void options_open_test(void){ opt_ret=GS_TITLE; opt_test=true; test_hold=0; }

/* ---------- ecran de test de la manette ---------- */
static void face_btn(int x,int y,bool on,const char *label){
  gfx_disc(x,y,8,HEX(0x0e0a14)); gfx_disc(x,y,7,on?HEX(0xffd84a):HEX(0x3a3048));
  text_draw(x,y-6,label,on?HEX(0x1a1420):HEX(0xf0e6d0),1,AL_CENTER,0);
}
static void stick_view(int x,int y,float sx,float sy,const char *name){
  gfx_disc(x,y,20,HEX(0x0e0a14)); gfx_ring(x,y,20,HEX(0x5a4a6a)); gfx_ring(x,y,4,HEX(0x2e2638));
  bool act=sqrtf(sx*sx+sy*sy)>0.25f;
  gfx_disc(x+(int)(sx*15),y+(int)(sy*15),6,act?HEX(0x8be05a):HEX(0x9a94a8));
  char b[64]; snprintf(b,sizeof b,"%s  %+.2f  %+.2f",name,sx,sy); text_draw(x,y+25,b,HEX(0x9a94a8),1,AL_CENTER,0);
}
static void small_btn(int x,int y,int w,bool on,const char *label){
  ui_panel(x,y,w,13,on?HEX(0xffd84a):HEX(0x2a2236),HEX(0x5a4a6a));
  text_draw(x+w/2,y+1,label,on?HEX(0x1a1420):HEX(0xd8d0e4),1,AL_CENTER,0);
}
static void pad_test_frame(float dt){
  Input *in=g_in; bool on=in && in->pad_on;
  ui_fancy_panel(70,10,500,342);
  text_draw(SCREEN_W/2,18,"TEST DE LA MANETTE",HEX(0xffd84a),2,AL_CENTER,TX_OUTLINE);
  char b[200];
  if(on) snprintf(b,sizeof b,"^2%s^0   (%s)",in->pad_name,in->pad_info);
  else snprintf(b,sizeof b,"^3Aucune manette détectée^0 : branchez-la et attendez quelques secondes");
  text_draw(SCREEN_W/2,46,b,HEX(0xd8d0e4),1,AL_CENTER,0);
  int cx=SCREEN_W/2, cy=150;
  gfx_ellipse(cx-118,cy+34,54,58,HEX(0x4a3e58)); gfx_ellipse(cx+118,cy+34,54,58,HEX(0x4a3e58));
  gfx_rect(cx-130,cy-44,260,86,HEX(0x4a3e58));
  gfx_ellipse(cx-118,cy+34,52,56,HEX(0x231c2e)); gfx_ellipse(cx+118,cy+34,52,56,HEX(0x231c2e));
  gfx_rect(cx-128,cy-42,256,82,HEX(0x231c2e));
  bool P[PAD_COUNT]; for(int i=0;i<PAD_COUNT;i++) P[i]= on && in->pad[i];
  bool ps = on && in->pad_type==PADT_PS;
  ui_bar(cx-170,cy-74,64,6,on?in->lt:0,HEX(0x6ad8ff),HEX(0x1e1828)); ui_bar(cx+106,cy-74,64,6,on?in->rt:0,HEX(0x6ad8ff),HEX(0x1e1828));
  text_draw(cx-138,cy-86,ps?"L2":"LT",HEX(0x9a94a8),1,AL_CENTER,0); text_draw(cx+138,cy-86,ps?"R2":"RT",HEX(0x9a94a8),1,AL_CENTER,0);
  small_btn(cx-170,cy-62,64,P[PAD_LB],ps?"L1":"LB"); small_btn(cx+106,cy-62,64,P[PAD_RB],ps?"R1":"RB");
  int dx=cx-118, dy=cy-6;
  gfx_rect(dx-5,dy-17,11,11,P[PAD_UP]?HEX(0xffd84a):HEX(0x3a3048)); gfx_rect(dx-5,dy+7,11,11,P[PAD_DOWN]?HEX(0xffd84a):HEX(0x3a3048));
  gfx_rect(dx-17,dy-5,11,11,P[PAD_LEFT]?HEX(0xffd84a):HEX(0x3a3048)); gfx_rect(dx+7,dy-5,11,11,P[PAD_RIGHT]?HEX(0xffd84a):HEX(0x3a3048));
  gfx_rect(dx-5,dy-5,11,11,HEX(0x2e2638));
  int fx=cx+118, fy=cy-6;
  face_btn(fx,fy-15,P[PAD_Y],ps?IC_PS_TRI:"Y"); face_btn(fx+15,fy,P[PAD_B],ps?IC_PS_CIRCLE:"B");
  face_btn(fx,fy+15,P[PAD_A],ps?IC_PS_CROSS:"A"); face_btn(fx-15,fy,P[PAD_X],ps?IC_PS_SQUARE:"X");
  small_btn(cx-56,cy-34,44,P[PAD_BACK],ps?"Créer":"Back"); small_btn(cx+12,cy-34,44,P[PAD_START],ps?"Options":"Start");
  stick_view(cx-52,cy+34,on?in->lx:0,on?in->ly:0,"G"); stick_view(cx+52,cy+34,on?in->rx:0,on?in->ry:0,"D");
  text_wrap(90,cy+86,460,"Appuyez sur les boutons et bougez les sticks : ils s'allument ici. Si rien ne bouge, envoyez-moi le fichier ^1manettes.txt^0 qui se trouve dans le dossier ^1%APPDATA%\\Gouffre^0.",HEX(0xb0a8c0),1,0,11);
  if(P[PAD_B]) test_hold+=dt; else test_hold=0;
  if(test_hold>0) ui_bar(cx-60,330,120,4,test_hold/1.0f,HEX(0xffd84a),HEX(0x1e1828));
  snprintf(b,sizeof b,"Échap, clic, ou maintenir %s : retour",ps?"^3"IC_PS_CIRCLE"^0":"^3B^0");
  text_draw(SCREEN_W/2,336,b,HEX(0x6a6478),1,AL_CENTER,0);
  bool kb_out = g_in && (g_in->key_pressed[KEY_ESCAPE]||g_in->key_pressed[KEY_BACKSPACE]);
  if(kb_out || A.click || test_hold>=1.0f){ opt_test=false; test_hold=0; sfx(SFX_CLICK); ui_set_focus(15); }
}
static void opt_slider(int *v,int dir){ int n=clampi(*v+dir,0,10); if(n!=*v){ *v=n; sfx(SFX_HOVER); } }
void options_frame(float dt){
  if(opt_ret==GS_RUN){ run_draw(); layer(LY_UI); gfx_darken(0,0,SCREEN_W,SCREEN_H,150); if(g_gpu){ GP.blur_all=0.85f; GP.desat=0.3f; } }
  else { menu_background(dt,40); if(g_gpu){ GP.blur_all=0.6f; layer(LY_UI); } }
  if(opt_test){ pad_test_frame(dt); return; }
  ui_fancy_panel(150,4,340,352);
  text_draw(SCREEN_W/2,10,"OPTIONS",HEX(0xffd84a),2,AL_CENTER,TX_OUTLINE);
  char b[160]; int y=30, x=SCREEN_W/2-130, w=260, h=15, st=16;
  bool changed=false, diff_hot=false;
  ui_begin();
  { int d0=S.difficulty;
    snprintf(b,sizeof b,"Difficulté : < %s >",diff_cur()->name);
    if(ui_button(x,y,w,h,b,true)) S.difficulty=(S.difficulty+1)%DIFF_COUNT;
    if(ui_hot()){ diff_hot=true; if(A.left){ S.difficulty=(S.difficulty+DIFF_COUNT-1)%DIFF_COUNT; sfx(SFX_HOVER); } if(A.right){ S.difficulty=(S.difficulty+1)%DIFF_COUNT; sfx(SFX_HOVER); } }
    if(S.difficulty!=d0){ changed=true; if(opt_ret==GS_RUN) run_recalc_stats(); }
    y+=st; }
  snprintf(b,sizeof b,"Musique : < %d >",S.vol_music);
  if(ui_button(x,y,w,h,b,true)){ opt_slider(&S.vol_music,S.vol_music>=10?-10:1); changed=true; }
  if(ui_hot()){ if(A.left){ opt_slider(&S.vol_music,-1); changed=true; } if(A.right){ opt_slider(&S.vol_music,1); changed=true; } } y+=st;
  snprintf(b,sizeof b,"Effets sonores : < %d >",S.vol_sfx);
  if(ui_button(x,y,w,h,b,true)){ opt_slider(&S.vol_sfx,S.vol_sfx>=10?-10:1); changed=true; }
  if(ui_hot()){ if(A.left){ opt_slider(&S.vol_sfx,-1); changed=true; } if(A.right){ opt_slider(&S.vol_sfx,1); changed=true; } } y+=st;
  snprintf(b,sizeof b,"Ambiances : < %d >",S.vol_amb);
  if(ui_button(x,y,w,h,b,true)){ opt_slider(&S.vol_amb,S.vol_amb>=10?-10:1); changed=true; }
  if(ui_hot()){ if(A.left){ opt_slider(&S.vol_amb,-1); changed=true; } if(A.right){ opt_slider(&S.vol_amb,1); changed=true; } } y+=st;
  snprintf(b,sizeof b,"Plein écran : %s",S.fullscreen?"Oui":"Non");
  if(ui_button(x,y,w,h,b,true)){ g_fs_request=S.fullscreen?0:1; } y+=st;
  snprintf(b,sizeof b,"Pixels parfaits : %s",S.pixel_perfect?"Oui":"Non");
  if(ui_button(x,y,w,h,b,true)){ S.pixel_perfect=!S.pixel_perfect; changed=true; } y+=st;
  { static const char *QN[4]={"Rétro (logiciel)","Performance","Élevée","Ultra"};
    snprintf(b,sizeof b,"Graphismes : < %s >",QN[clampi(S.quality,0,3)]);
    if(ui_button(x,y,w,h,b,true)){ S.quality=(S.quality+1)%4; changed=true; }
    if(ui_hot()){ if(A.left){ S.quality=(S.quality+3)%4; changed=true; sfx(SFX_HOVER); } if(A.right){ S.quality=(S.quality+1)%4; changed=true; sfx(SFX_HOVER); } } y+=st; }
  snprintf(b,sizeof b,"Lueurs et étalonnage : %s",S.bloom?"Oui":"Non");
  if(ui_button(x,y,w,h,b,true)){ S.bloom=!S.bloom; changed=true; } y+=st;
  snprintf(b,sizeof b,"Effet écran cathodique : %s",S.crt?"Oui":"Non");
  if(ui_button(x,y,w,h,b,true)){ S.crt=!S.crt; changed=true; } y+=st;
  snprintf(b,sizeof b,"Vue du puits : %s",S.view25?"2.5D (inclinée)":"De dessus");
  if(ui_button(x,y,w,h,b,true)){ S.view25=!S.view25; changed=true; } y+=st;
  snprintf(b,sizeof b,"Tremblements de l'écran : %s",S.shake?"Oui":"Non");
  if(ui_button(x,y,w,h,b,true)){ S.shake=!S.shake; changed=true; } y+=st;
  snprintf(b,sizeof b,"Chiffres de dégâts : %s",S.dmg_numbers?"Oui":"Non");
  if(ui_button(x,y,w,h,b,true)){ S.dmg_numbers=!S.dmg_numbers; changed=true; } y+=st;
  snprintf(b,sizeof b,"Visée auto (souris) : %s",S.autoaim?"Oui":"Non");
  if(ui_button(x,y,w,h,b,true)){ S.autoaim=!S.autoaim; if(opt_ret==GS_RUN) R.autoaim=S.autoaim; changed=true; } y+=st;
  snprintf(b,sizeof b,"Visée auto (manette) : %s",S.pad_autoaim?"Oui":"Non");
  if(ui_button(x,y,w,h,b,true)){ S.pad_autoaim=!S.pad_autoaim; changed=true; } y+=st;
  snprintf(b,sizeof b,"Vibrations de la manette : %s",S.rumble?"Oui":"Non");
  if(ui_button(x,y,w,h,b,true)){ S.rumble=!S.rumble; changed=true; if(S.rumble) rumble(0.6f,0.25f); } y+=st;
  if(ui_button(x,y,w,h,"Tester la manette",true)){ opt_test=true; test_hold=0; } y+=st;
  if(opt_ret!=GS_RUN){
    const char *lab = reset_confirm==0 ? "Effacer la progression" : (reset_confirm==1 ? "Vraiment tout effacer ?" : "Dernière chance : confirmer");
    if(ui_button(x,y,w,h,lab,true)){
      reset_confirm++;
      if(reset_confirm>=3){ int vm=S.vol_music, vs=S.vol_sfx, va=S.vol_amb; bool fs=S.fullscreen, crt=S.crt, v25=S.view25; int bl=S.bloom, q=S.quality, df=S.difficulty; save_defaults(); S.vol_music=vm; S.vol_sfx=vs; S.vol_amb=va; S.fullscreen=fs; S.bloom=bl; S.quality=q; S.difficulty=df; S.crt=crt; S.view25=v25; save_write(); reset_confirm=0; sfx(SFX_BREACH); }
    }
    y+=st;
  }
  y+=4;
  if(ui_button(x,y,w,h,"Retour",true) || A.back){ save_write(); goto_state(opt_ret); }
  ui_end();
  if(diff_hot){ const char *dd=diff_cur()->desc;
    if(text_w(dd,1)<=320) text_draw(SCREEN_W/2,330,dd,HEX(0xd8d0e4),1,AL_CENTER,0);
    else text_wrap(SCREEN_W/2-160,324,320,dd,HEX(0xd8d0e4),1,0,11); }
  else {
    if(g_in && g_in->pad_on) snprintf(b,sizeof b,"Manette détectée : ^2%s^0 (%s)",g_in->pad_name,g_in->pad_info); else snprintf(b,sizeof b,"Aucune manette détectée");
    text_draw(SCREEN_W/2,326,b,HEX(0xb0a8c0),1,AL_CENTER,0);
    if(S.quality>0 && !g_gpu_ok) text_draw(SCREEN_W/2,339,"^3Carte graphique indisponible : rendu logiciel",HEX(0x8a82a0),1,AL_CENTER,0);
    else text_draw(SCREEN_W/2,339,"Gauche / Droite pour régler",HEX(0x5a5466),1,AL_CENTER,0);
  }
  if(changed) save_write();
}
void menu_backdrop(float dt,int amb){ menu_background(dt,amb); }
