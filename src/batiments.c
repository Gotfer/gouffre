/* GOUFFRE - batiments : sprites du campement tires des maquettes 3D, ecran « Amélioration »
   (carrousel des batiments, maquette qui tourne sur elle-meme, apercu du niveau suivant, animation de construction) */
#include "game.h"

void camp_world(float dt,float amb);
void camp_set_sel(int b);
void camp_flash(int b);

/* ---------------- sprites du campement ---------------- */
typedef struct { float x,y; int kind; } CampEmit;
static Sprite bspr[BLD_COUNT]; static int bax[BLD_COUNT], bay[BLD_COUNT], blv[BLD_COUNT]; static bool binit=false;
static CampEmit bem[BLD_COUNT][VOX_MAX_EMIT]; static int bnem[BLD_COUNT];
static VoxModel tmpm;

void bld_sprites_refresh(void){
  if(!binit){ for(int b=0;b<BLD_COUNT;b++) blv[b]=-1; binit=true; }
  for(int b=0;b<BLD_COUNT;b++){
    int L=clampi(S.bld[b],0,BUILDINGS[b].max);
    if(blv[b]==L) continue;
    bld_model(b,L,false,&tmpm);
    vox_camp_sprite(&tmpm,&bspr[b],&bax[b],&bay[b]);
    bnem[b]=tmpm.nem;
    for(int k=0;k<tmpm.nem;k++){ float sx,sy; vox_camp_point(&tmpm,tmpm.em[k].x,tmpm.em[k].y,tmpm.em[k].z,&sx,&sy);
      bem[b][k].x=sx-bax[b]; bem[b][k].y=sy-bay[b]; bem[b][k].kind=tmpm.em[k].kind; }
    blv[b]=L;
  }
}
const Sprite *bld_sprite(int b,int *ax,int *ay){ bld_sprites_refresh(); if(ax) *ax=bax[b]; if(ay) *ay=bay[b]; return &bspr[b]; }
/* (x,y) : pied de la facade au campement */
void bld_camp_rect(int b,int x,int y,int *rx,int *ry,int *rw,int *rh){
  bld_sprites_refresh(); *rx=x-bax[b]; *ry=y-bay[b]; *rw=bspr[b].w; *rh=bspr[b].h;
}
void bld_camp_draw(int b,int x,int y,const DrawOpt *o){
  bld_sprites_refresh();
  const Sprite *s=&bspr[b]; DrawOpt d= o ? *o : dopt();
  draw_spr_c(s,(float)(x-bax[b]+s->w/2),(float)(y-bay[b]+s->h/2),&d);
}
static void emit_fx(int kind,float ex,float ey,float sc,int k,bool ui){
  switch(kind){
    case EMI_SMOKE: for(int i=0;i<(ui?6:4);i++){ float ph=fmodf(g_time*0.35f+i*(ui?0.1667f:0.25f)+k*0.13f,1.0f); float px=ex+(ph*6+sinf(ph*6+i)*2)*sc, py=ey-ph*30*sc;
        if(g_gpu && !ui) fx_smoke(px,py,(1.5f+ph*4)*sc,HEX(0x8a8494),0.5f*(1-ph));
        else { int r=(int)((0.8f+ph*3.2f)*sc); int a=(int)(70*(1-ph)*minf(1,ph*6)); gfx_disc_a((int)px,(int)py,r,HEX(0x7a7486),a); if(r>2) gfx_disc_a((int)px-1,(int)py-1,r-2,HEX(0x9a94a6),a/2); } } break;
    case EMI_SPARK: for(int i=0;i<3;i++){ float ph=fmodf(g_time*0.9f+i*0.33f+k*0.21f,1.0f); float px=ex+sinf(ph*9+i*2+k)*2.5f*sc, py=ey-ph*14*sc;
        if(g_gpu && !ui) fx_disc(px,py,0.5f,HEX(0xffa040),1.6f*(1-ph)); else gfx_disc_a((int)px,(int)py,sc>1.5f?1:0,HEX(0xffb050),(int)(255*(1-ph))); } break;
    case EMI_GREEN: for(int i=0;i<3;i++){ float ph=fmodf(g_time*0.4f+i*0.33f+k*0.2f,1.0f); float px=ex+sinf(ph*7+i)*2*sc, py=ey-ph*26*sc;
        if(g_gpu && !ui) fx_disc(px,py,1+ph*2,HEX(0x9af05a),0.7f*(1-ph)); else gfx_disc_a((int)px,(int)py,(int)((1+ph*2)*sc*0.7f),HEX(0x9af05a),(int)(130*(1-ph))); } break;
    default: for(int i=0;i<4;i++){ float ph=fmodf(g_time*0.5f+i*0.25f+k*0.17f,1.0f); float a=i*1.7f+g_time*0.8f; float px=ex+cosf(a)*6*(0.4f+ph)*sc, py=ey+(-ph*10+sinf(a)*2)*sc;
        uint32_t c=(i&1)?HEX(0xc08aff):HEX(0x8ae8ff); float v=sinf(ph*PI);
        if(g_gpu && !ui) fx_star(px,py,1.6f,c,0.8f*v); else { gfx_padd((int)px,(int)py,c,(int)(255*v)); if(sc>1.5f){ gfx_padd((int)px+1,(int)py,c,(int)(120*v)); gfx_padd((int)px-1,(int)py,c,(int)(120*v)); gfx_padd((int)px,(int)py+1,c,(int)(120*v)); gfx_padd((int)px,(int)py-1,c,(int)(120*v)); } } } break;
  }
}
void bld_camp_fx(int b,int x,int y){
  for(int k=0;k<bnem[b];k++) emit_fx(bem[b][k].kind,x+bem[b][k].x,y+bem[b][k].y,1.0f,k,false);
}

/* ---------------- ecran « Amélioration » ---------------- */
#define ST_X 18
#define ST_Y 46
#define ST_W 358
#define ST_H 248
#define IP_X 384
#define IP_Y 46
#define IP_W 238
#define IP_H 248
#define BUILD_T 1.35f
static int up_sel=0, up_ret=GS_CAMP;
static int up_b=-1, up_l=-1;
static VoxModel up_m, up_n, up_old, up_comp;
static bool up_n_ok=false;
static Sprite up_img;
static VoxModel fitm[BLD_COUNT]; static float fit_s[BLD_COUNT]; static bool fit_ok[BLD_COUNT];
static float *fpts[BLD_COUNT]; static int fnp[BLD_COUNT];
static int cy_b=-1; static float cy_pitch=-1, cy_val=0;
static float up_yaw=-0.6f, up_pitch=0.50f, up_manual_t=0, up_slide=0, up_strip=0, up_build=-1, up_flash=0, up_lvltxt=0, up_t=0;
static bool up_preview=false, up_drag=false; static float up_lmx=0, up_lmy=0;
static uint32_t up_done=0;
typedef struct { float x,y,vx,vy,t,life; uint32_t c; int big; } UPart;
static UPart up_p[160]; static int up_np=0;
#define PITCH_LO 0.34f
#define PITCH_HI 0.66f
#define FIT_W (ST_W-16)
#define FIT_H (ST_H-34)

void upgrade_select(int b){ up_sel=clampi(b,0,BLD_COUNT-1); }
void upgrade_open(int b,int ret){ upgrade_select(b); up_ret=ret; goto_state(GS_UPGRADE); }
bool upgrade_busy(void){ return up_build>=0; }
void upgrade_enter(void){
  up_preview=false; up_build=-1; up_slide=0; up_strip=(float)up_sel; up_np=0; up_done=0; up_flash=0; up_lvltxt=0; up_drag=false; up_t=0;
  if(!up_img.px){ up_img=spr_alloc(ST_W,ST_H); up_img.em=(uint32_t*)calloc((size_t)ST_W*ST_H,4); }
}
static void load_fit(int b){
  if(fit_ok[b]) return;
  bld_model(b,BUILDINGS[b].max,true,&tmpm);
  fnp[b]=vox_hull(&tmpm,&fpts[b]);
  fitm[b]=tmpm; fitm[b].c=NULL; fitm[b].n=NULL; fitm[b].blk=NULL; fitm[b].cap=0; fitm[b].bcap=0;
  float s1,s2,cy; vox_fit_pts(&fitm[b],fpts[b],fnp[b],PITCH_LO,FIT_W,FIT_H,3.6f,&s1,&cy); vox_fit_pts(&fitm[b],fpts[b],fnp[b],PITCH_HI,FIT_W,FIT_H,3.6f,&s2,&cy);
  fit_s[b]=minf(s1,s2); fit_ok[b]=true;
}
static void load_model(void){
  int b=up_sel, L=clampi(S.bld[b],0,BUILDINGS[b].max);
  load_fit(b);
  if(up_b==b && up_l==L) return;
  bld_model(b,L,true,&up_m); up_b=b; up_l=L; up_n_ok=false;
}
static void ensure_next(void){
  if(up_n_ok || up_l>=BUILDINGS[up_sel].max) return;
  bld_model(up_sel,up_l+1,true,&up_n); vox_mark_new(&up_n,&up_m); up_n_ok=true;
}
static void part(float x,float y,float vx,float vy,float life,uint32_t c,int big){
  if(up_np>=COUNTOF(up_p)) return; UPart *p=&up_p[up_np++]; p->x=x; p->y=y; p->vx=vx; p->vy=vy; p->t=0; p->life=life; p->c=c; p->big=big;
}
static void parts_update(float dt){
  for(int i=0;i<up_np;){ UPart *p=&up_p[i]; p->t+=dt; if(p->t>=p->life){ up_p[i]=up_p[--up_np]; continue; }
    p->x+=p->vx*dt; p->y+=p->vy*dt; p->vy+=40*dt; p->vx*=1-dt*1.5f; i++; }
}
static void parts_draw(void){
  for(int i=0;i<up_np;i++){ UPart *p=&up_p[i]; float k=1-p->t/p->life; int a=(int)(255*k);
    if(p->big){ gfx_disc_add((int)p->x,(int)p->y,1,p->c,a/2); gfx_padd((int)p->x,(int)p->y,HEX(0xffffff),a); }
    else gfx_padd((int)p->x,(int)p->y,p->c,a); }
}
static void change_sel(int d){
  if(up_build>=0) return;
  up_sel=(up_sel+d+BLD_COUNT)%BLD_COUNT; up_slide=(float)d; up_preview=false; sfx(SFX_HOVER);
}
static bool can_afford(int b,int L){ return L<BUILDINGS[b].max && S.gold>=building_cost(b,L+1) && S.crystals>=building_crystals(b,L+1); }
static void do_upgrade(void){
  int b=up_sel, L=up_l;
  if(L>=BUILDINGS[b].max || up_build>=0) return;
  int gc=building_cost(b,L+1), cc=building_crystals(b,L+1);
  if(S.gold<gc || S.crystals<cc){ sfx(SFX_DENY); toast(S.gold<gc?"Pas assez d'or":"Pas assez de cristaux"); return; }
  S.gold-=gc; S.crystals-=cc; S.bld[b]++; save_write(); sfx(SFX_UPGRADE); meta_check(); rumble(0.35f,0.25f);
  VoxModel t=up_old; up_old=up_m; up_m=t;
  bld_model(b,L+1,true,&up_m); up_l=L+1; up_n_ok=false; up_preview=false;
  up_build=0; up_done|=1u<<b;
}

static void stage_backdrop(void){
  for(int j=0;j<ST_H;j++) gfx_hline(ST_X,ST_X+ST_W-1,ST_Y+j,col_lerp(HEX(0x24162a),HEX(0x0c070e),(float)j/ST_H));
  gfx_glow(ST_X+ST_W*0.5f,ST_Y+ST_H*0.55f,150,HEX(0x6a3a7a),0.30f);
  gfx_glow(ST_X+ST_W*0.5f,ST_Y+ST_H*0.62f,70,HEX(0xffc070),0.10f);
  for(int i=0;i<40;i++){ uint32_t h=(uint32_t)(i*2654435761u); float ph=fmodf(up_t*0.04f*(1+(h&3))+(h>>8)%100/100.0f,1.0f);
    int x=ST_X+4+(int)((h>>12)%(ST_W-8)), y=ST_Y+ST_H-6-(int)(ph*(ST_H-12)); gfx_padd(x,y,HEX(0xc8a0ff),(int)(60*sinf(ph*PI))); }
}
static void stage_fx(const VoxModel *m,const VoxCam *cam){
  int ox0=gT->cx0, oy0=gT->cy0, ox1=gT->cx1, oy1=gT->cy1;
  gfx_clip(ST_X,ST_Y,ST_X+ST_W,ST_Y+ST_H);
  for(int k=0;k<m->nem;k++){ float sx,sy; vox_project(m,cam,m->em[k].x,m->em[k].y,m->em[k].z,&sx,&sy); emit_fx(m->em[k].kind,ST_X+sx,ST_Y+sy,cam->scale*0.75f,k,true); }
  gT->cx0=ox0; gT->cy0=oy0; gT->cx1=ox1; gT->cy1=oy1;
}
static void arrow(int x,int y,int dir,bool hot){
  uint32_t c= hot?HEX(0xffe08a):HEX(0xc89458);
  for(int k=0;k<2;k++){ float o=k?0:1.5f; uint32_t cc=k?c:HEX(0x0a0608);
    if(dir<0) gfx_tri((float)x+8+o,(float)y-10-o,(float)x+8+o,(float)y+10+o,(float)x-4-o,(float)y,cc);
    else gfx_tri((float)x-8-o,(float)y-10-o,(float)x-8-o,(float)y+10+o,(float)x+4+o,(float)y,cc); }
}
static void info_panel(int b,int L){
  char t[200];
  for(int j=0;j<IP_H;j++) gfx_hline(IP_X,IP_X+IP_W-1,IP_Y+j,col_lerp(HEX(0x2a1a22),HEX(0x150c12),(float)j/IP_H));
  orn_frame(IP_X,IP_Y,IP_W,IP_H,0);
  int cx=IP_X+IP_W/2, y=IP_Y+8;
  const BuildingDef *bd=&BUILDINGS[b];
  gothic_title(cx,y,bd->name,gothic_w(bd->name,24)<=IP_W-16?24:12); y+=30;
  if(L>0) snprintf(t,sizeof t,"Niveau %d / %d",L,bd->max); else snprintf(t,sizeof t,"À construire (0 / %d)",bd->max);
  text_draw(cx,y,t,L>=bd->max?HEX(0xffd84a):HEX(0xe8d8c8),1,AL_CENTER,TX_SHADOW); y+=13;
  { int pw=mini(12,(IP_W-24)/bd->max); int x0=cx-(pw*bd->max)/2;
    for(int i=0;i<bd->max;i++){ int x=x0+i*pw; gfx_rect(x,y,pw-2,5,HEX(0x0a0608));
      uint32_t c= i<L ? HEX(0xffd84a) : (i==L && up_preview ? col_lerp(HEX(0x3a3242),HEX(0x8ae8ff),0.5f+0.5f*sinf(up_t*6)) : HEX(0x3a3242));
      gfx_rect(x+1,y+1,pw-4,3,c); } }
  y+=12;
  y+=text_wrap(IP_X+12,y,IP_W-24,bd->desc,HEX(0xc8b8c0),1,0,11)+6;
  gfx_hline(IP_X+12,IP_X+IP_W-13,y,HEX(0x5a3a2a)); y+=6;
  char e1[64],e2[64]; building_effect(b,L,e1,sizeof e1);
  text_draw(IP_X+12,y,"EFFET",HEX(0xc9a24a),1,AL_LEFT,TX_SHADOW); y+=12;
  snprintf(t,sizeof t,"^7Actuel :^0 %s",L>0?e1:"aucun"); text_draw(IP_X+12,y,t,HEX(0xe8e0e8),1,AL_LEFT,0); y+=12;
  if(L<bd->max){ building_effect(b,L+1,e2,sizeof e2); snprintf(t,sizeof t,"^7Niveau %d :^2 %s",L+1,e2); text_draw(IP_X+12,y,t,HEX(0xe8e0e8),1,AL_LEFT,0); }
  y+=16;
  gfx_hline(IP_X+12,IP_X+IP_W-13,y,HEX(0x5a3a2a)); y+=6;
  text_draw(IP_X+12,y,"AU CAMPEMENT",HEX(0xc9a24a),1,AL_LEFT,TX_SHADOW); y+=12;
  if(L<bd->max){ snprintf(t,sizeof t,"^7Niveau %d :^0 %s",L+1,bld_visual(b,L+1)); y+=text_wrap(IP_X+12,y,IP_W-24,t,HEX(0xf0e6d0),1,0,11)+2;
    int bx=IP_X+12; int bw= A.pad ? pad_badge(bx,y,BG_ALT) : text_badge(bx,y,"R");
    text_draw(bx+bw+5,y+2,up_preview?"Masquer l'aperçu":"Voir l'aperçu",up_preview?HEX(0x8ae8ff):HEX(0xc8b8c0),1,AL_LEFT,TX_SHADOW);
    if(A.click && mouse_in(bx-2,y-2,bw+110,16) && up_build<0){ up_preview=!up_preview; sfx(SFX_CLICK); }
    y+=18; }
  else { text_wrap(IP_X+12,y,IP_W-24,"^1Niveau maximum :^0 le bâtiment est au sommet de sa splendeur.",HEX(0xf0e6d0),1,0,11); }
  /* cout + bouton */
  int by=IP_Y+IP_H-36;
  if(L<bd->max){
    int gc=building_cost(b,L+1), cc=building_crystals(b,L+1);
    if(cc>0) snprintf(t,sizeof t,"Coût :  %s"IC_COIN" %d^0   %s"IC_GEM" %d",S.gold>=gc?"^1":"^3",gc,S.crystals>=cc?"^6":"^3",cc);
    else snprintf(t,sizeof t,"Coût :  %s"IC_COIN" %d",S.gold>=gc?"^1":"^3",gc);
    text_draw(cx,by-14,t,HEX(0xe8d8c8),1,AL_CENTER,TX_SHADOW);
    bool ok=can_afford(b,L) && up_build<0;
    bool hot= A.pad || ok;
    if(orn_button(IP_X+18,by,IP_W-36,26,L==0?"   Construire":"   Améliorer",hot && ok,ok) ) do_upgrade();
    else if(!ok && up_build<0 && A.click && mouse_in(IP_X+18,by,IP_W-36,26)) toast(S.gold<gc?"Pas assez d'or":"Pas assez de cristaux");
    pad_badge(IP_X+30,by+6,BG_CONFIRM);
  } else orn_button(IP_X+18,by,IP_W-36,26,"Niveau maximum",false,false);
}
static void strip(float dt){
  float d=(float)up_sel-up_strip; while(d>BLD_COUNT*0.5f) d-=BLD_COUNT; while(d<-BLD_COUNT*0.5f) d+=BLD_COUNT;
  up_strip+=d*minf(1,dt*12);
  int base=(int)floorf(up_strip+0.5f); const int SP=46, CY=322, CX=320;
  for(int o=-7;o<=7;o++){
    int i=((base+o)%BLD_COUNT+BLD_COUNT)%BLD_COUNT; float x=CX+(base+o-up_strip)*SP;
    if(x<ST_X+14 || x>SCREEN_W-ST_X-14) continue;
    float dist=fabsf(x-CX)/(SP*6.0f); bool sel= i==up_sel;
    int w= sel?40:34, h=w; int x0=(int)(x-w/2), y0=CY-h/2;
    for(int j=0;j<h;j++) gfx_hline(x0,x0+w-1,y0+j,col_lerp(sel?HEX(0x4a2a34):HEX(0x2a1820),HEX(0x120a10),(float)j/h));
    orn_frame(x0,y0,w,h,sel?1:0);
    int ax,ay; const Sprite *s=bld_sprite(i,&ax,&ay); (void)ax; (void)ay;
    DrawOpt op=dopt(); float sc=minf((w-6)/(float)s->w,(h-6)/(float)s->h); op.sx=op.sy=sc; op.flags=DS_NOOCC;
    if(dist>0.5f){ op.tint=col_lerp(HEX(0xffffff),HEX(0x6a6070),clampf((dist-0.5f)*2,0,1)); }
    draw_spr_c(s,x,(float)CY,&op);
    int L=S.bld[i];
    if(L>=BUILDINGS[i].max) text_draw(x0+w-3,y0+1,IC_STAR,HEX(0xffd84a),1,AL_RIGHT,TX_OUTLINE);
    else if(can_afford(i,L)) gfx_tri((float)x0+w-9,(float)y0+8,(float)x0+w-3,(float)y0+8,(float)x0+w-6,(float)y0+3,HEX(0x8be05a));
    char t[8]; snprintf(t,sizeof t,"%d",L); text_draw(x0+3,y0+h-11,t,L>0?HEX(0xf0e6d0):HEX(0x8a7a80),1,AL_LEFT,TX_OUTLINE);
    if(A.click && mouse_in(x0,y0,w,h) && !sel && up_build<0){ int dd=i-up_sel; if(dd>BLD_COUNT/2) dd-=BLD_COUNT; if(dd<-BLD_COUNT/2) dd+=BLD_COUNT; up_sel=i; up_slide= dd>0?1.0f:-1.0f; up_preview=false; sfx(SFX_HOVER); }
  }
}

void upgrade_frame(float dt){
  up_t+=dt;
  bld_sprites_refresh();
  /* fond : le campement, flou et assombri */
  layer(LY_SCENE);
  gfx_gradient_v(0,0,SCREEN_W,SCREEN_H,HEX(0x140c16),HEX(0x080508));
  if(g_gpu){ GP.vig=0.4f; layer(LY_UI); }
  /* commandes */
  if(A.back){ sfx(SFX_CLICK); camp_set_sel(up_sel); for(int b=0;b<BLD_COUNT;b++) if(up_done>>b&1) camp_flash(b); goto_state(up_ret); return; }
  if(A.left||A.tab_l) change_sel(-1);
  if(A.right||(A.tab_r && !(g_in&&g_in->key_pressed[KEY_TAB]))) change_sel(1);
  load_model();
  int b=up_sel, L=up_l;
  if(A.reroll && L<BUILDINGS[b].max && up_build<0){ up_preview=!up_preview; sfx(SFX_CLICK); }
  if(A.confirm && up_build<0){ if(L<BUILDINGS[b].max) do_upgrade(); else sfx(SFX_DENY); L=up_l; }
  if(up_preview) ensure_next();
  /* rotation : automatique, stick droit, glisser a la souris */
  float rx= g_in&&g_in->pad_on ? g_in->rx : 0, ry= g_in&&g_in->pad_on ? g_in->ry : 0;
  if(fabsf(rx)<0.2f) rx=0; if(fabsf(ry)<0.2f) ry=0;
  if(rx!=0||ry!=0){ up_yaw+=rx*dt*2.4f; up_pitch=clampf(up_pitch+ry*dt*0.9f,PITCH_LO,PITCH_HI); up_manual_t=2.0f; }
  if(A.mouse_down && (up_drag || (A.click && mouse_in(ST_X,ST_Y,ST_W,ST_H)))){
    if(up_drag){ up_yaw+=(A.mx-up_lmx)*0.014f; up_pitch=clampf(up_pitch+(A.my-up_lmy)*0.006f,PITCH_LO,PITCH_HI); }
    up_drag=true; up_manual_t=2.0f;
  } else up_drag=false;
  up_lmx=A.mx; up_lmy=A.my;
  if(up_manual_t>0) up_manual_t-=dt; else up_yaw+=dt*0.55f;
  up_slide=approachf(up_slide*(1-minf(1,dt*14)),0,dt*0.4f);
  if(up_flash>0) up_flash-=dt*2.2f;
  if(up_lvltxt>0) up_lvltxt-=dt;
  parts_update(dt);
  /* cadre et en-tete */
  orn_panel(8,4,624,354);
  { int hx=160, hy=12, hw=320, hh=28;
    for(int j=0;j<hh;j++) gfx_hline(hx,hx+hw-1,hy+j,col_lerp(HEX(0x4a2a24),HEX(0x24120e),(float)j/hh));
    orn_frame(hx,hy,hw,hh,0);
    gothic_title(320,hy+2,"Améliorations",24); }
  { int bx=34, by=18; text_draw(bx-8,by+1,"<",HEX(0xe8d8c8),1,AL_LEFT,TX_OUTLINE); int w=pad_badge(bx,by,BG_BACK);
    if(A.click && mouse_in(bx-10,by-2,w+14,18)){ sfx(SFX_CLICK); camp_set_sel(up_sel); for(int k=0;k<BLD_COUNT;k++) if(up_done>>k&1) camp_flash(k); goto_state(up_ret); return; } }
  { char t[64]; snprintf(t,sizeof t,"^1"IC_COIN" %d^0   ^6"IC_GEM" %d",S.gold,S.crystals); text_draw(SCREEN_W-22,20,t,HEX(0xf0e6d0),1,AL_RIGHT,TX_OUTLINE); }
  /* scene : la maquette */
  stage_backdrop();
  float sc=fit_s[b];
  if(cy_b!=b || fabsf(cy_pitch-up_pitch)>0.008f){ float s2,c; vox_fit_pts(&fitm[b],fpts[b],fnp[b],up_pitch,FIT_W,FIT_H,3.6f,&s2,&c); cy_val=c/s2; cy_b=b; cy_pitch=up_pitch; }
  float cyo=cy_val*sc+6;
  VoxCam cam={up_yaw,up_pitch,sc,ST_W*0.5f+up_slide*ST_W*0.55f,ST_H*0.5f+cyo};
  const VoxModel *m=&up_m; int flags=0;
  if(up_build>=0){
    up_build+=dt;
    float k=clampf(up_build/BUILD_T,0,1); float e=k*k*(3-2*k);
    float y0=(float)up_m.py-0.5f, y1=(float)maxi(up_m.y0+up_m.ny,up_old.y0+up_old.ny)+1.5f;
    float sw=y0+(y1-y0)*e;
    vox_compose(&up_old,&up_m,sw,&up_comp); m=&up_comp;
    /* etincelles sur la ligne de construction */
    for(int q=0;q<3;q++){ float X=m->x0+rfrand()*m->nx, Z=m->z0+rfrand()*m->nz, sx,sy; vox_project(m,&cam,X,sw,Z,&sx,&sy);
      part(ST_X+sx,ST_Y+sy,rfrange(-14,14),rfrange(-40,-10),rfrange(0.4f,0.9f),rfrand()<0.5f?HEX(0xffd84a):HEX(0xfff0c0),rfrand()<0.3f); }
    if(up_build>=BUILD_T){ up_build=-1; m=&up_m; up_flash=1; up_lvltxt=2.2f; sfx(SFX_EVOLVE);
      for(int q=0;q<70;q++){ float a=rfrand()*TAU, v=rfrange(30,120); part(ST_X+cam.cx+rfrange(-60,60),ST_Y+cam.cy+rfrange(-70,30),cosf(a)*v,sinf(a)*v-40,rfrange(0.6f,1.4f),q%3?HEX(0xffd84a):HEX(0xffffff),q%4==0); } }
  } else if(up_preview && up_n_ok){ m=&up_n; flags=VR_PREVIEW; }
  /* la maquette tourne lentement : une image sur deux suffit, sauf pendant les animations */
  { static int fc=0; static const VoxModel *lastm=NULL; static int lastflags=-1, lastb=-1, lastl=-1;
    bool need = (fc++&1)==0 || up_build>=0 || fabsf(up_slide)>0.002f || m!=lastm || flags!=lastflags || b!=lastb || up_l!=lastl || up_drag;
    if(need){ vox_render(m,up_t,flags,&cam,&up_img); lastm=m; lastflags=flags; lastb=b; lastl=up_l; } }
  vox_blit(&up_img,ST_X,ST_Y,0.9f);
  stage_fx(m,&cam);
  if(up_flash>0) gfx_rect_add(ST_X,ST_Y,ST_W,ST_H,HEX(0xfff0c0),(int)(up_flash*90));
  { int ox0=gT->cx0, oy0=gT->cy0, ox1=gT->cx1, oy1=gT->cy1; gfx_clip(ST_X,ST_Y,ST_X+ST_W,ST_Y+ST_H); parts_draw(); gT->cx0=ox0; gT->cy0=oy0; gT->cx1=ox1; gT->cy1=oy1; }
  orn_frame(ST_X,ST_Y,ST_W,ST_H,0);
  /* etiquettes de la scene */
  { char t[64];
    if(up_preview && up_n_ok && up_build<0){ snprintf(t,sizeof t,"Aperçu : niveau %d",L+1); text_draw(ST_X+10,ST_Y+8,t,col_lerp(HEX(0x8ae8ff),HEX(0xffffff),0.5f+0.5f*sinf(up_t*5)),1,AL_LEFT,TX_OUTLINE); }
    else { if(L>0) snprintf(t,sizeof t,"Niveau %d",L); else snprintf(t,sizeof t,"Chantier"); text_draw(ST_X+10,ST_Y+8,t,HEX(0xe8d8c8),1,AL_LEFT,TX_OUTLINE); }
    snprintf(t,sizeof t,"%d / %d",b+1,BLD_COUNT); text_draw(ST_X+ST_W-10,ST_Y+8,t,HEX(0x9a8a90),1,AL_RIGHT,TX_OUTLINE);
    if(up_lvltxt>0){ float k=clampf(up_lvltxt/2.2f,0,1); int dy=(int)((1-k)*-10); int yy=ST_Y+ST_H-44+dy;
      snprintf(t,sizeof t,L>1?"Niveau %d !":"Construit !",L);
      int tw=maxi(text_w(t,2),text_w(bld_visual(b,L),1))+24; gfx_rect_a(ST_X+ST_W/2-tw/2,yy-4,tw,38,HEX(0x0a0608),(int)(170*minf(1,up_lvltxt*2)));
      text_draw(ST_X+ST_W/2,yy,t,col_lerp(HEX(0xffd84a),HEX(0xffffff),k*0.5f),2,AL_CENTER,TX_OUTLINE);
      text_draw(ST_X+ST_W/2,yy+21,bld_visual(b,L),HEX(0xf0e6d0),1,AL_CENTER,TX_OUTLINE); } }
  /* fleches */
  { int ay=ST_Y+ST_H/2; bool hl=mouse_in(ST_X+2,ay-18,26,36), hr=mouse_in(ST_X+ST_W-28,ay-18,26,36);
    arrow(ST_X+14,ay,-1,hl); arrow(ST_X+ST_W-14,ay,1,hr);
    if(A.pad){ pad_badge(ST_X+6,ay+16,BG_L1); pad_badge(ST_X+ST_W-24,ay+16,BG_R1); }
    if(A.click && hl) change_sel(-1); if(A.click && hr) change_sel(1); }
  info_panel(b,up_l);
  strip(dt);
  /* aide */
  if(A.pad){ char t[200]; snprintf(t,sizeof t,"L1 / R1 : bâtiment   %s : améliorer   %s : aperçu   Stick droit : tourner   %s : retour",btn(B_CONFIRM),btn(B_ALT),btn(B_BACK)); text_draw(320,346,t,HEX(0x8a82a0),1,AL_CENTER,0); }
  else if(A.touch) text_draw(320,346,"Touchez : choisir   Glisser : tourner   Échap : retour",HEX(0x8a82a0),1,AL_CENTER,0);
  else text_draw(320,346,"Flèches : bâtiment   Entrée : améliorer   R : aperçu   Glisser la souris : tourner   Échap : retour",HEX(0x8a82a0),1,AL_CENTER,0);
}
