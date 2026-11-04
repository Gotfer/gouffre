/* GOUFFRE - interface immediate : boutons, panneaux, curseur, logo */
#include "game.h"

Actions A;
static int ui_count=0, ui_focus=0; static bool ui_last_hot=false;

bool mouse_in(int x,int y,int w,int h){ return A.mx>=x && A.my>=y && A.mx<x+w && A.my<y+h; }
void ui_begin(void){ ui_count=0; }
void ui_set_focus(int i){ ui_focus=i; }
int  ui_focus_index(void){ return ui_focus; }
bool ui_hot(void){ return ui_last_hot; }

void ui_panel(int x,int y,int w,int h,uint32_t fill,uint32_t border){
  gfx_rect(x+1,y+1,w-2,h-2,fill);
  gfx_hline(x+1,x+w-2,y,border); gfx_hline(x+1,x+w-2,y+h-1,border);
  gfx_vline(x,y+1,y+h-2,border); gfx_vline(x+w-1,y+1,y+h-2,border);
  gfx_hline(x+1,x+w-2,y+1,col_scale(fill,1.35f));
  gfx_hline(x+1,x+w-2,y+h-2,col_scale(fill,0.7f));
}
void ui_fancy_panel(int x,int y,int w,int h){
  gfx_rect_a(x,y,w,h,HEX(0x0e0a14),238);
  gfx_frame(x,y,w,h,HEX(0x7a5a32)); gfx_frame(x+2,y+2,w-4,h-4,HEX(0x3a2a1a));
  uint32_t g=HEX(0xc9a24a);
  int cx[4]={x,x+w-4,x,x+w-4}, cy[4]={y,y,y+h-4,y+h-4};
  for(int i=0;i<4;i++){ gfx_rect(cx[i],cy[i],4,4,HEX(0x3a2a1a)); gfx_rect(cx[i]+1,cy[i]+1,2,2,g); }
}
void ui_bar(int x,int y,int w,int h,float t,uint32_t fill,uint32_t back){
  gfx_rect(x-1,y-1,w+2,h+2,HEX(0x0a0810));
  gfx_rect(x,y,w,h,back);
  int fw=(int)(w*clampf(t,0,1));
  if(fw>0){ gfx_rect(x,y,fw,h,fill); gfx_rect(x,y,fw,1,col_scale(fill,1.35f)); if(h>3) gfx_rect(x,y+h-1,fw,1,col_scale(fill,0.7f)); }
}

bool ui_button(int x,int y,int w,int h,const char *label,bool enabled){
  int id=ui_count++;
  bool hover=mouse_in(x,y,w,h);
  if(hover && A.mouse_moved && ui_focus!=id){ ui_focus=id; if(enabled) sfx(SFX_HOVER); }
  bool hot = ui_focus==id;
  ui_last_hot=hot;
  uint32_t fill = hot ? HEX(0x3a2a4e) : HEX(0x1e1828);
  uint32_t border = hot ? HEX(0xd8b060) : HEX(0x5a4a6a);
  if(!enabled){ fill=HEX(0x16121c); border=HEX(0x3a3242); }
  ui_panel(x,y,w,h,fill,border);
  if(hot && enabled){
    int ay=y+h/2; float b=sinf(g_time*8)*1.5f;
    gfx_tri((float)x-8+b,(float)ay-3,(float)x-8+b,(float)ay+3,(float)x-4+b,(float)ay,HEX(0xffd84a));
    gfx_tri((float)x+w+7-b,(float)ay-3,(float)x+w+7-b,(float)ay+3,(float)x+w+3-b,(float)ay,HEX(0xffd84a));
  }
  uint32_t tc = !enabled ? HEX(0x5a5466) : (hot ? HEX(0xffffff) : HEX(0xc8c0d4));
  text_draw(x+w/2,y+(h-TXT_H)/2,label,tc,1,AL_CENTER,TX_SHADOW);
  bool act = enabled && ((hot && A.confirm) || (hover && A.click));
  if(act) sfx(SFX_CLICK);
  if(!enabled && hover && A.click) sfx(SFX_DENY);
  return act;
}
void ui_end(void){
  if(ui_count>0){
    if(A.up){ ui_focus=(ui_focus+ui_count-1)%ui_count; sfx(SFX_HOVER); }
    if(A.down){ ui_focus=(ui_focus+1)%ui_count; sfx(SFX_HOVER); }
    if(ui_focus>=ui_count) ui_focus=ui_count-1;
    if(ui_focus<-1) ui_focus=0;   /* -1 : aucun bouton (campement : la manette choisit un batiment) */
  }
}

void ui_cursor(void){
  if(A.pad || A.touch || !g_in || !g_in->mouse_inside) return;   /* au doigt : pas de curseur */
  int x=(int)A.mx, y=(int)A.my;
  bool aiming = g_state==GS_RUN && (R.sub==RS_PLAY||R.sub==RS_INTRO||R.sub==RS_CLEAR) && !R.autoaim;
  if(aiming){
    uint32_t o=HEX(0x0a0810), c=HEX(0xffffff);
    for(int d=-1;d<=1;d++){ gfx_hline(x-6,x-3,y+d,o); gfx_hline(x+3,x+6,y+d,o); gfx_vline(x+d,y-6,y-3,o); gfx_vline(x+d,y+3,y+6,o); }
    gfx_hline(x-5,x-3,y,c); gfx_hline(x+3,x+5,y,c); gfx_vline(x,y-5,y-3,c); gfx_vline(x,y+3,y+5,c);
    gfx_pset(x,y,HEX(0xff6a5a));
  } else {
    static const char *CUR[]={"k.......","kk......","kwk.....","kwwk....","kwwwk...","kwwwwk..","kwwwwwk.","kwwwkkk.","kwkwk...","kk.kwk..","k...kk.."};
    for(int j=0;j<11;j++) for(int i=0;i<8;i++){ char ch=CUR[j][i]; if(ch=='k') gfx_pset(x+i,y+j,HEX(0x0a0810)); else if(ch=='w') gfx_pset(x+i,y+j,HEX(0xf6f2ea)); }
  }
}

void draw_title_logo(int cx,int y,float t){
  const char *T="GOUFFRE"; int sc=6;
  text_draw(cx,y+4,T,HEX(0x0a0608),sc,AL_CENTER,0);
  text_draw(cx,y,T,HEX(0x9a3a12),sc,AL_CENTER,TX_OUTLINE);
  Target *tg=gT; int ox0=tg->cx0, oy0=tg->cy0, ox1=tg->cx1, oy1=tg->cy1;
  gfx_clip(0,y,SCREEN_W,y+7*sc);
  text_draw(cx,y,T,HEX(0xe08a2a),sc,AL_CENTER,0);
  gfx_clip(0,y,SCREEN_W,y+5*sc);
  text_draw(cx,y,T,HEX(0xf8c848),sc,AL_CENTER,0);
  gfx_clip(0,y,SCREEN_W,y+3*sc+2);
  text_draw(cx,y,T,HEX(0xfff4c0),sc,AL_CENTER,0);
  gfx_clip(ox0,oy0,ox1,oy1);
  /* etincelles sur les lettres */
  int w=text_w(T,sc);
  for(int i=0;i<3;i++){
    float ph=fmodf(t*0.9f+i*0.37f,1.0f); int sx=cx-w/2+(int)(fmodf(i*137.0f+floorf(t*0.9f+i*0.37f)*71.0f,(float)w));
    int sy=y+2*sc+(i*7)%(4*sc); int a=(int)(255*sinf(ph*PI));
    gfx_padd(sx,sy,HEX(0xffffff),a); gfx_padd(sx-1,sy,HEX(0xffe0a0),a/2); gfx_padd(sx+1,sy,HEX(0xffe0a0),a/2);
    gfx_padd(sx,sy-1,HEX(0xffe0a0),a/2); gfx_padd(sx,sy+1,HEX(0xffe0a0),a/2);
  }
}
