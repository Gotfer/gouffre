/* GOUFFRE - police gothique des titres (bitmap genere depuis Jacquard 24 / Jacquard 12, licence OFL) */
#include "game.h"
#include "gothic_data.h"

static uint32_t g_utf8(const char **p){
  const unsigned char *s=(const unsigned char*)*p; uint32_t c=s[0];
  if(c<0x80){ *p+=1; return c; }
  if((c&0xE0)==0xC0 && s[1]){ *p+=2; return ((c&0x1F)<<6)|(s[1]&0x3F); }
  if((c&0xF0)==0xE0 && s[1] && s[2]){ *p+=3; return ((c&0x0F)<<12)|((s[1]&0x3F)<<6)|(s[2]&0x3F); }
  if(s[1]&&s[2]&&s[3]){ *p+=4; return ((c&0x07)<<18)|((s[1]&0x3F)<<12)|((s[2]&0x3F)<<6)|(s[3]&0x3F); }
  *p+=1; return '?';
}
static const GGlyph *g_find(uint32_t cp,int size){
  const GGlyph *t= size>=24 ? G24_G : G12_G; int n= size>=24 ? COUNTOF(G24_G) : COUNTOF(G12_G);
  if(cp=='\'') cp=0x2019;
  for(int i=0;i<n;i++) if(t[i].cp==cp) return &t[i];
  /* lettres absentes : on retire l'accent */
  static const char *FB[][2]={{"ÁÃÅ","A"},{"áãå","a"},{"ÌÍ","I"},{"ìí","i"},{"ÒÓÕ","O"},{"òóõ","o"},{"Ú","U"},{"ú","u"},{"Ñ","N"},{"ñ","n"}};
  for(int k=0;k<COUNTOF(FB);k++){ const char *q=FB[k][0]; while(*q){ uint32_t c=g_utf8(&q); if(c==cp) return g_find((uint32_t)FB[k][1][0],size); } }
  return g_find('?',size);
}
int gothic_w(const char *s,int size){
  int w=0; const char *p=s;
  while(*p){ uint32_t cp=g_utf8(&p); const GGlyph *g=g_find(cp,size); w+=g->adv; }
  return w;
}
int gothic_h(int size){ return size>=24 ? G24_ASC+G24_DESC : G12_ASC+G12_DESC; }
int gothic_asc(int size){ return size>=24 ? G24_ASC : G12_ASC; }

/* passe 0 : contour ; passe 1 : ombre ; passe 2 : lettres (degrade vertical) */
static void g_glyph(const GGlyph *g,int x,int base,int size,int pass,uint32_t ctop,uint32_t cbot,uint32_t oc,int ytop,int hh){
  const uint8_t *B= size>=24 ? G24_B : G12_B;
  int bpr=(g->w+7)/8;
  for(int j=0;j<g->h;j++) for(int i=0;i<g->w;i++){
    if(!(B[g->off+j*bpr+i/8]&(0x80>>(i&7)))) continue;
    int px=x+g->ox+i, py=base+g->oy+j;
    if(pass==0){ for(int dy=-1;dy<=1;dy++) for(int dx=-1;dx<=1;dx++) if(dx||dy) gfx_pset(px+dx,py+dy,oc); gfx_pset(px,py+2,oc); gfx_pset(px+1,py+2,oc); }
    else if(pass==1) gfx_pset(px+1,py+1,oc);
    else { float t=hh>1 ? clampf((float)(py-ytop)/(hh-1),0,1) : 0; gfx_pset(px,py,col_lerp(ctop,cbot,t)); }
  }
}
/* y : haut de la ligne. Renvoie la largeur. GT_OUTLINE : contour 1 px + epaisseur dessous ; GT_SHADOW : ombre portee */
int gothic_draw(int x,int y,const char *s,uint32_t ctop,uint32_t cbot,int size,int align,int fx,uint32_t oc){
  int w=gothic_w(s,size);
  if(align==AL_CENTER) x-=w/2; else if(align==AL_RIGHT) x-=w;
  int base=y+gothic_asc(size);
  int hh= size>=24 ? 16 : 8;            /* hauteur des capitales : etendue du degrade */
  int ytop=base-hh;
  for(int pass=0;pass<3;pass++){
    if(pass==0 && !(fx&TX_OUTLINE)) continue;
    if(pass==1 && !(fx&TX_SHADOW)) continue;
    int cx=x; const char *p=s;
    while(*p){ uint32_t cp=g_utf8(&p); const GGlyph *g=g_find(cp,size); if(g->h) g_glyph(g,cx,base,size,pass,ctop,cbot,oc,ytop,hh); cx+=g->adv; }
  }
  return w;
}
/* titre facon enluminure : ivoire rose en haut, vieux rose en bas, contour brun sombre */
int gothic_title(int cx,int y,const char *s,int size){
  return gothic_draw(cx,y,s,HEX(0xfff2ea),HEX(0xe8b8c0),size,AL_CENTER,TX_OUTLINE,HEX(0x1a0a10));
}
