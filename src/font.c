/* GOUFFRE - police pixel proportionnelle, UTF-8, accents composes, codes couleur ^0..^9 */
#include "game.h"

/* chaque glyphe : 7 lignes de corps (+2 de jambage), separees par des espaces */
static const char *FONT_SRC[] = {
 "A", ".###. #...# #...# ##### #...# #...# #...#",
 "B", "####. #...# #...# ####. #...# #...# ####.",
 "C", ".###. #...# #.... #.... #.... #...# .###.",
 "D", "####. #...# #...# #...# #...# #...# ####.",
 "E", "##### #.... #.... ####. #.... #.... #####",
 "F", "##### #.... #.... ####. #.... #.... #....",
 "G", ".###. #...# #.... #.### #...# #...# .####",
 "H", "#...# #...# #...# ##### #...# #...# #...#",
 "I", "### .#. .#. .#. .#. .#. ###",
 "J", ".### ...# ...# ...# #..# #..# .##.",
 "K", "#...# #..#. #.#.. ##... #.#.. #..#. #...#",
 "L", "#.... #.... #.... #.... #.... #.... #####",
 "M", "#...# ##.## #.#.# #.#.# #...# #...# #...#",
 "N", "#...# ##..# #.#.# #..## #...# #...# #...#",
 "O", ".###. #...# #...# #...# #...# #...# .###.",
 "P", "####. #...# #...# ####. #.... #.... #....",
 "Q", ".###. #...# #...# #...# #.#.# #..#. .##.#",
 "R", "####. #...# #...# ####. #.#.. #..#. #...#",
 "S", ".#### #.... #.... .###. ....# ....# ####.",
 "T", "##### ..#.. ..#.. ..#.. ..#.. ..#.. ..#..",
 "U", "#...# #...# #...# #...# #...# #...# .###.",
 "V", "#...# #...# #...# #...# .#.#. .#.#. ..#..",
 "W", "#...# #...# #...# #.#.# #.#.# ##.## #...#",
 "X", "#...# #...# .#.#. ..#.. .#.#. #...# #...#",
 "Y", "#...# #...# .#.#. ..#.. ..#.. ..#.. ..#..",
 "Z", "##### ....# ...#. ..#.. .#... #.... #####",
 "a", ".... .... .##. ...# .### #..# .###",
 "b", "#... #... ###. #..# #..# #..# ###.",
 "c", "... ... .## #.. #.. #.. .##",
 "d", "...# ...# .### #..# #..# #..# .###",
 "e", ".... .... .##. #..# #### #... .###",
 "f", "..## .#.. ###. .#.. .#.. .#.. .#..",
 "g", ".... .... .### #..# #..# #..# .### ...# .##.",
 "h", "#... #... ###. #..# #..# #..# #..#",
 "i", "# . # # # # #",
 "\xC4\xB1", ". . # # # # #",
 "j", ".# .. .# .# .# .# .# .# #.",
 "k", "#... #... #..# #.#. ##.. #.#. #..#",
 "l", "#. #. #. #. #. #. .#",
 "m", "..... ..... ##.#. #.#.# #.#.# #.#.# #.#.#",
 "n", ".... .... ###. #..# #..# #..# #..#",
 "o", ".... .... .##. #..# #..# #..# .##.",
 "p", ".... .... ###. #..# #..# #..# ###. #... #...",
 "q", ".... .... .### #..# #..# #..# .### ...# ...#",
 "r", "... ... #.# ##. #.. #.. #..",
 "s", ".... .... .### #... .##. ...# ###.",
 "t", ".#. .#. ### .#. .#. .#. ..#",
 "u", ".... .... #..# #..# #..# #..# .###",
 "v", "..... ..... #...# #...# .#.#. .#.#. ..#..",
 "w", "..... ..... #...# #...# #.#.# #.#.# .#.#.",
 "x", ".... .... #..# #..# .##. #..# #..#",
 "y", ".... .... #..# #..# #..# #..# .### ...# .##.",
 "z", ".... .... #### ...# .##. #... ####",
 "0", ".##. #..# #..# #..# #..# #..# .##.",
 "1", ".#. ##. .#. .#. .#. .#. ###",
 "2", ".##. #..# ...# ..#. .#.. #... ####",
 "3", "###. ...# ...# .##. ...# ...# ###.",
 "4", "#..# #..# #..# #### ...# ...# ...#",
 "5", "#### #... #... ###. ...# ...# ###.",
 "6", ".##. #... #... ###. #..# #..# .##.",
 "7", "#### ...# ...# ..#. .#.. .#.. .#..",
 "8", ".##. #..# #..# .##. #..# #..# .##.",
 "9", ".##. #..# #..# .### ...# ...# .##.",
 ".", ". . . . . . #",
 ",", ".. .. .. .. .. .. .# #.",
 ":", ". . . # . . #",
 ";", ".. .. .. .# .. .. .# #.",
 "!", "# # # # # . #",
 "?", ".##. #..# ...# ..#. .#.. .... .#..",
 "'", "# #",
 "\xE2\x80\x99", "# #",
 "\"", "#.# #.#",
 "-", "... ... ... ###",
 "+", "... ... .#. ### .#.",
 "=", "... ... ### ... ###",
 "/", "..# ..# .#. .#. .#. #.. #..",
 "\\", "#.. #.. .#. .#. .#. ..# ..#",
 "(", ".# #. #. #. #. #. .#",
 ")", "#. .# .# .# .# .# #.",
 "[", "## #. #. #. #. #. ##",
 "]", "## .# .# .# .# .# ##",
 "<", "... ..# .#. #.. .#. ..#",
 ">", "... #.. .#. ..# .#. #..",
 "%", "##... ##..# ...#. ..#.. .#... #..## ...##",
 "*", "... #.# .#. #.#",
 "#", ".#.#. ##### .#.#. .#.#. ##### .#.#.",
 "&", ".##.. #..#. #.#.. .#... #.#.# #..#. .##.#",
 "_", ".... .... .... .... .... .... .... ####",
 "\xC3\x97", "... ... #.# .#. #.#",
 "\xC2\xB0", ".#. #.# .#.",
 "\xE2\x80\xA6", "..... ..... ..... ..... ..... ..... #.#.#",
 "\xC2\xAB", ".... .... .#.# #.#. .#.#",
 "\xC2\xBB", ".... .... #.#. .#.# #.#.",
 "\xC5\x93", "...... ...... .##.#. #..#.# #..### #..#.. .##.##",
 "\xC5\x92", ".##### #..#.. #..#.. #..### #..#.. #..#.. .#####",
 IC_HEART, "..... .#.#. ##### ##### .###. ..#..",
 IC_COIN,  "..... .###. #.### #.### #.### .###.",
 IC_GEM,   "..#.. .###. ##### .###. ..#..",
 IC_SKULL, "..... .###. ##### #.#.# ##### .#.#.",
 IC_STAR,  "..#.. ..#.. ##### .###. .#.#.",
 IC_ARROW, "..... ..#.. ...#. ##### ...#. ..#..",
 IC_CLOCK, "..... .###. #.#.# #.### #...# .###.",
 IC_BALL,  "..... .###. #.### ##### ##### .###.",
 IC_PS_CROSS,  "..... #...# .#.#. ..#.. .#.#. #...#",
 IC_PS_CIRCLE, "..... .###. #...# #...# #...# .###.",
 IC_PS_TRI,    "..... ..#.. .#.#. .#.#. #...# #####",
 IC_PS_SQUARE, "..... ##### #...# #...# #...# #####",
 IC_PRISM, "..... ..#.. .###. .###. ##### #####",
 NULL, NULL
};

typedef struct { uint32_t cp; uint8_t w; uint8_t rows[11]; } Glyph;
static Glyph G[200];
static int nG;
static int16_t gidx[256];

static uint32_t utf8_next(const char **p){
  const unsigned char *s=(const unsigned char*)*p; uint32_t c=s[0];
  if(c<0x80){ *p+=1; return c; }
  if((c&0xE0)==0xC0 && s[1]){ c=((c&0x1F)<<6)|(s[1]&0x3F); *p+=2; return c; }
  if((c&0xF0)==0xE0 && s[1] && s[2]){ c=((c&0x0F)<<12)|((uint32_t)(s[1]&0x3F)<<6)|(s[2]&0x3F); *p+=3; return c; }
  if((c&0xF8)==0xF0 && s[1] && s[2] && s[3]){ c=((c&0x07)<<18)|((uint32_t)(s[1]&0x3F)<<12)|((uint32_t)(s[2]&0x3F)<<6)|(s[3]&0x3F); *p+=4; return c; }
  *p+=1; return '?';
}
static Glyph *find_glyph(uint32_t cp){
  if(cp<256){ int i=gidx[cp]; return i>=0 ? &G[i] : NULL; }
  for(int i=0;i<nG;i++) if(G[i].cp==cp) return &G[i];
  return NULL;
}

enum { AC_ACUTE, AC_GRAVE, AC_CIRC, AC_DIAER, AC_CEDIL };
static const char *ACC[5][2] = { {".#","#."}, {"#.",".#"}, {".#.","#.#"}, {"...","#.#"}, {".#","#."} };

static void add_composite(uint32_t cp,uint32_t base,int acc){
  Glyph *b=find_glyph(base); if(!b || nG>=COUNTOF(G)) return;
  Glyph g=*b; g.cp=cp;
  int aw=(int)strlen(ACC[acc][0]);
  int w=g.w; int shift=0;
  if(aw>w){ shift=(aw-w)/2 + ((aw-w)&1); for(int r=0;r<11;r++) g.rows[r]=(uint8_t)(g.rows[r]<<shift); w=aw; }
  int ax=(w-aw)/2;
  bool cap = (base>='A' && base<='Z');
  int r0 = acc==AC_CEDIL ? 9 : (cap ? 0 : 2);
  for(int r=0;r<2;r++) for(int i=0;i<aw;i++) if(ACC[acc][r][i]=='#') g.rows[r0+r] |= (uint8_t)(1<<(ax+i));
  g.w=(uint8_t)w;
  if(cp<256) gidx[cp]=(int16_t)nG;
  G[nG++]=g;
}

void font_init(void){
  for(int i=0;i<256;i++) gidx[i]=-1;
  nG=0;
  for(int i=0;FONT_SRC[i];i+=2){
    const char *name=FONT_SRC[i]; const char *src=FONT_SRC[i+1];
    uint32_t cp=utf8_next(&name);
    Glyph g; memset(&g,0,sizeof g); g.cp=cp;
    int row=0, col=0, w=0;
    for(const char *p=src; ; p++){
      if(*p==' ' || *p==0){ if(col>w) w=col; row++; col=0; if(*p==0) break; continue; }
      if(*p=='#' && row<9) g.rows[row+2] |= (uint8_t)(1<<col);
      col++;
    }
    g.w=(uint8_t)w;
    if(cp<256) gidx[cp]=(int16_t)nG;
    G[nG++]=g;
  }
  struct { uint32_t cp, base; int acc; } C[] = {
    {0xE9,'e',AC_ACUTE},{0xE8,'e',AC_GRAVE},{0xEA,'e',AC_CIRC},{0xEB,'e',AC_DIAER},
    {0xE0,'a',AC_GRAVE},{0xE2,'a',AC_CIRC},{0xE4,'a',AC_DIAER},
    {0xF9,'u',AC_GRAVE},{0xFB,'u',AC_CIRC},{0xFC,'u',AC_DIAER},
    {0xEE,0x131,AC_CIRC},{0xEF,0x131,AC_DIAER},{0xF4,'o',AC_CIRC},{0xF6,'o',AC_DIAER},{0xE7,'c',AC_CEDIL},
    {0xC9,'E',AC_ACUTE},{0xC8,'E',AC_GRAVE},{0xCA,'E',AC_CIRC},{0xCB,'E',AC_DIAER},
    {0xC0,'A',AC_GRAVE},{0xC2,'A',AC_CIRC},{0xD9,'U',AC_GRAVE},{0xDB,'U',AC_CIRC},
    {0xCE,'I',AC_CIRC},{0xCF,'I',AC_DIAER},{0xD4,'O',AC_CIRC},{0xC7,'C',AC_CEDIL},
  };
  for(int i=0;i<COUNTOF(C);i++) add_composite(C[i].cp,C[i].base,C[i].acc);
}

static const uint32_t TXTCOL[10] = {
  0, HEX(0xffd84a), HEX(0x8be05a), HEX(0xff5a4a), HEX(0x6ad8ff),
  HEX(0xff9a3a), HEX(0xc08aff), HEX(0x9a94a8), HEX(0xffffff), HEX(0xff7ac8)
};

static int glyph_adv(uint32_t cp){
  if(cp==' ') return 3;
  Glyph *g=find_glyph(cp); if(!g) g=find_glyph('?');
  return g ? g->w+1 : 4;
}
/* largeur d'un segment [s,e) sans codes couleur */
static int seg_w(const char *s,const char *e){
  int w=0; const char *p=s;
  while(p<e && *p){
    if(*p=='^' && p[1]>='0' && p[1]<='9'){ p+=2; continue; }
    uint32_t cp=utf8_next(&p); w+=glyph_adv(cp);
  }
  return w>0 ? w-1 : 0;
}
/* lignes de pixels reellement occupees (accents, jambages compris), relatives au y de text_draw */
void text_vext(const char *s,int *top,int *bot){
  int t=99, b=-1; const char *p=s;
  while(*p){
    if(*p=='^' && p[1]>='0' && p[1]<='9'){ p+=2; continue; }
    uint32_t cp=utf8_next(&p); if(cp==' '||cp=='\n') continue;
    Glyph *g=find_glyph(cp); if(!g) g=find_glyph('?'); if(!g) continue;
    for(int r=0;r<11;r++) if(g->rows[r]){ if(r<t) t=r; if(r>b) b=r; }
  }
  if(b<0){ t=2; b=8; }
  *top=t; *bot=b;
}
int text_w(const char *s,int scale){
  int best=0; const char *ls=s;
  for(const char *p=s;;p++){
    if(*p=='\n' || *p==0){ int w=seg_w(ls,p); if(w>best) best=w; if(!*p) break; ls=p+1; }
  }
  return best*scale;
}
static void draw_glyph(int x,int y,Glyph *g,uint32_t col,int scale){
  for(int r=0;r<11;r++){
    uint8_t bits=g->rows[r]; if(!bits) continue;
    for(int c=0;c<g->w;c++) if(bits&(1<<c)){
      if(scale==1) gfx_pset(x+c,y+r,col); else gfx_rect(x+c*scale,y+r*scale,scale,scale,col);
    }
  }
}
/* le prisme : facettes claires en haut, lumiere decomposee en arc-en-ciel vers le bas */
static void draw_prism(int x,int y,Glyph *g,int scale){
  static const uint32_t RB[6]={HEX(0xff6a6a),HEX(0xffb84a),HEX(0xf4f06a),HEX(0x7ae88a),HEX(0x6ad8ff),HEX(0xb48aff)};
  for(int r=0;r<11;r++){
    uint8_t bits=g->rows[r]; if(!bits) continue;
    for(int c=0;c<g->w;c++) if(bits&(1<<c)){
      uint32_t col = r<=3 ? HEX(0xffffff) : (r==4 ? HEX(0xe4faff) : (r==5 ? HEX(0xb8e8ff) : RB[clampi(c+(r-6),0,5)]));
      if(scale==1) gfx_pset(x+c,y+r,col); else gfx_rect(x+c*scale,y+r*scale,scale,scale,col);
    }
  }
}
/* dessine [s,e) ; *cur = couleur courante (codes appliques) ; override!=0 force une couleur */
static void draw_seg(int x,int y,const char *s,const char *e,uint32_t base,uint32_t *cur,int scale,uint32_t override){
  const char *p=s;
  while(p<e && *p){
    if(*p=='^' && p[1]>='0' && p[1]<='9'){ int k=p[1]-'0'; *cur = k==0 ? base : TXTCOL[k]; p+=2; continue; }
    uint32_t cp=utf8_next(&p);
    if(cp==' '){ x+=3*scale; continue; }
    Glyph *g=find_glyph(cp); if(!g) g=find_glyph('?'); if(!g) continue;
    if(cp==0xE00C && !override) draw_prism(x,y,g,scale); else
    draw_glyph(x,y,g,override?override:*cur,scale);
    x+=(g->w+1)*scale;
  }
}
static void draw_seg_fx(int x,int y,const char *s,const char *e,uint32_t col,uint32_t *cur,int scale,int fx){
  uint32_t tmp;
  if(fx&TX_OUTLINE){
    uint32_t oc=HEX(0x0c0910);
    for(int dy=-1;dy<=1;dy++) for(int dx=-1;dx<=1;dx++){ if(!dx&&!dy) continue; tmp=*cur; draw_seg(x+dx*scale,y+dy*scale,s,e,col,&tmp,scale,oc); }
    if(scale>1){ tmp=*cur; draw_seg(x,y+2*scale,s,e,col,&tmp,scale,oc); }
  } else if(fx&TX_SHADOW){
    tmp=*cur; draw_seg(x+(scale>1?scale:1),y+(scale>1?scale:1),s,e,col,&tmp,scale,HEX(0x0c0910));
  }
  draw_seg(x,y,s,e,col,cur,scale,0);
}
int text_draw(int x,int y,const char *s,uint32_t col,int scale,int align,int fx){
  if(!s) return 0;
  uint32_t cur=col; int best=0; int ly=y; const char *ls=s;
  for(const char *p=s;;p++){
    if(*p=='\n' || *p==0){
      int w=seg_w(ls,p)*scale; if(w>best) best=w;
      int lx = align==AL_CENTER ? x-w/2 : (align==AL_RIGHT ? x-w : x);
      draw_seg_fx(lx,ly,ls,p,col,&cur,scale,fx);
      if(!*p) break;
      ls=p+1; ly+=TXT_H*scale;
    }
  }
  return best;
}
/* retour a la ligne automatique ; renvoie la hauteur utilisee */
static int wrap_impl(int x,int y,int w,const char *s,uint32_t col,int scale,int fx,int lh,bool draw){
  uint32_t cur=col; int lines=0;
  const char *p=s;
  while(*p){
    const char *ls=p, *last_break=NULL; const char *q=p;
    while(*q && *q!='\n'){
      const char *nx=q;
      if(*nx=='^' && nx[1]>='0' && nx[1]<='9') nx+=2; else utf8_next(&nx);
      if(*q==' ') { if(seg_w(ls,q)*scale<=w) last_break=q; else break; }
      q=nx;
    }
    const char *le;
    if(!*q || *q=='\n'){
      if(seg_w(ls,q)*scale<=w || !last_break) le=q; else le=last_break;
    } else le = last_break ? last_break : q;
    if(le==ls && *le && *le!='\n'){ /* mot trop long : coupe brutalement */
      const char *t=ls; utf8_next(&t); le=t;
    }
    if(draw) draw_seg_fx(x,y+lines*lh*scale,ls,le,col,&cur,scale,fx);
    lines++;
    p=le; if(*p==' '||*p=='\n') p++;
  }
  return lines*lh*scale;
}
int text_wrap(int x,int y,int w,const char *s,uint32_t col,int scale,int fx,int lh){ return wrap_impl(x,y,w,s,col,scale,fx,lh,true); }
int text_wrap_count(const char *s,int w,int scale){ return wrap_impl(0,0,w,s,0,scale,0,1,false); }

/* ---------------- chiffres gras (degats, compteurs) : 7x10, degrade vertical, contour sombre ---------------- */
static const char *BIGD[12][10]={
  {".#####.","##...##","##...##","##..###","##.#.##","###..##","##...##","##...##","##...##",".#####."},
  {"..##...",".###...","####...","..##...","..##...","..##...","..##...","..##...","..##...","######."},
  {".#####.","##...##",".....##",".....##","....##.","...##..","..##...",".##....","##.....","#######"},
  {".#####.","##...##",".....##",".....##","..####.",".....##",".....##",".....##","##...##",".#####."},
  {"....##.","...###.","..####.",".##.##.","##..##.","##..##.","#######","....##.","....##.","....##."},
  {"#######","##.....","##.....","######.",".....##",".....##",".....##",".....##","##...##",".#####."},
  {".#####.","##...##","##.....","##.....","######.","##...##","##...##","##...##","##...##",".#####."},
  {"#######","##...##","....##.","....##.","...##..","...##..","..##...","..##...","..##...","..##..."},
  {".#####.","##...##","##...##","##...##",".#####.","##...##","##...##","##...##","##...##",".#####."},
  {".#####.","##...##","##...##","##...##","##...##",".######",".....##",".....##","##...##",".#####."},
  {"##","##","##","##","##","##","##","..","##","##"},                              /* ! */
  {".......","...##..","...##..","...##..","#######","#######","...##..","...##..","...##..","......."}, /* + */
};
static int bigd_idx(char c){ if(c>='0'&&c<='9') return c-'0'; if(c=='!') return 10; if(c=='+') return 11; return -1; }
bool text_is_number(const char *s){
  if(!s||!*s) return false; int nd=0;
  for(const char *p=s;*p;p++){ if(*p>='0'&&*p<='9') nd++; else if(*p!='!' && *p!='+' && *p!='-') return false; }
  return nd>0;
}
static int bigd_w(const char *s){ int w=0; for(const char *p=s;*p;p++){ int k=bigd_idx(*p); if(*p=='-'){ w+=6; continue; } if(k<0) continue; w+=(int)strlen(BIGD[k][0])+1; } return w>0?w-1:0; }
int text_big_digits(int x,int y,const char *s,uint32_t col,int scale,int align){
  int w=bigd_w(s)*scale;
  int x0 = align==AL_CENTER ? x-w/2 : (align==AL_RIGHT ? x-w : x);
  uint32_t top=col_lerp(col,HEX(0xffffff),0.35f), bot=col_scale(col,0.72f), oc=HEX(0x120a14);
  for(int pass=0;pass<2;pass++){
    int cx=x0;
    for(const char *p=s;*p;p++){
      if(*p=='-'){ /* tiret */
        for(int j=4;j<6;j++) for(int i=0;i<5;i++){ int px=cx+i*scale, py=y+j*scale;
          if(pass==0){ gfx_rect(px-scale,py-scale,scale*3,scale*3,oc); gfx_rect(px,py+scale,scale,scale*2,oc); }
          else gfx_rect(px,py,scale,scale,col_lerp(top,bot,j/9.0f)); }
        cx+=6*scale; continue; }
      int k=bigd_idx(*p); if(k<0) continue;
      int gw=(int)strlen(BIGD[k][0]);
      for(int j=0;j<10;j++) for(int i=0;i<gw;i++){
        if(BIGD[k][j][i]!='#') continue;
        int px=cx+i*scale, py=y+j*scale;
        if(pass==0){ gfx_rect(px-scale,py-scale,scale*3,scale*3,oc); gfx_rect(px,py+scale,scale,scale*2,oc); }   /* contour + ombre */
        else gfx_rect(px,py,scale,scale,col_lerp(top,bot,j/9.0f));
      }
      cx+=(gw+1)*scale;
    }
  }
  return w;
}
