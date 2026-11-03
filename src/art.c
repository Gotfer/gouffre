/* GOUFFRE - graphismes : sprites pixel art (ASCII), boss proceduraux, batiments, billes */
#include "game.h"

Sprite SPR[SP_COUNT];
Sprite SPR_RAW[SP_COUNT];
Sprite SPR_WALK[SP_COUNT][2];
Sprite SPR_LO[SP_COUNT];          /* personnages a l'ancienne taille (petites vignettes de l'interface) */
Sprite SPR_LO_WALK[SP_COUNT][2];
Sprite SPR_BALL[64];
Sprite SPR_BLD[16];
Sprite SPR_HUT[16];

#define ART(name) static const char *const name[]

/* ---------------- personnages « HD » : agrandis x2 (Scale2x) puis repeints ----------------
   Les sprites ASCII d'origine servent de croquis : on double la definition en lissant les diagonales,
   puis on repeint chaque pixel : lumiere en haut a gauche selon le volume, ombres violettes, reflets chauds,
   traits sombres entre les grandes masses, visages des heros dans l'ombre (style « peint »). */
static inline uint32_t pxat(const Sprite *a,int x,int y){ if(x<0||y<0||x>=a->w||y>=a->h) return 0; return a->px[y*a->w+x]; }
static Sprite spr_scale2x(const Sprite *a){
  Sprite s=spr_alloc(a->w*2,a->h*2); if(a->em) s.em=(uint32_t*)calloc((size_t)s.w*s.h,4);
  for(int y=0;y<a->h;y++) for(int x=0;x<a->w;x++){
    uint32_t P=pxat(a,x,y), A=pxat(a,x,y-1), B=pxat(a,x+1,y), C=pxat(a,x-1,y), D=pxat(a,x,y+1);
    int sx[4]={x,x,x,x}, sy[4]={y,y,y,y};
    if(C==A && C!=D && A!=B){ sx[0]=x; sy[0]=y-1; }
    if(A==B && A!=C && B!=D){ sx[1]=x+1; sy[1]=y; }
    if(D==C && D!=B && C!=A){ sx[2]=x-1; sy[2]=y; }
    if(B==D && B!=A && D!=C){ sx[3]=x; sy[3]=y+1; }
    (void)P;
    for(int k=0;k<4;k++){ int ox=2*x+(k&1), oy=2*y+(k>>1); s.px[oy*s.w+ox]=pxat(a,sx[k],sy[k]);
      if(a->em && sx[k]>=0 && sy[k]>=0 && sx[k]<a->w && sy[k]<a->h) s.em[oy*s.w+ox]=a->em[sy[k]*a->w+sx[k]]; }
  }
  return s;
}
static inline float lumc(uint32_t c){ return (cr(c)*0.3f+cg(c)*0.59f+cb(c)*0.11f)/255.0f; }
static uint32_t hd_tone(uint32_t c,float f){
  int g=(int)(lumc(c)*255); c=col_lerp(c,rgb(g,g,g),0.14f);
  if(f<1.0f){ uint32_t d=col_scale(c,0.22f+0.78f*f); return col_lerp(d,HEX(0x1e1030),(1.0f-f)*0.75f); }
  return col_lerp(c,HEX(0xfff1dc),clampf((f-1.0f)*1.8f,0,0.8f));
}
static bool is_skin(uint32_t c){ c&=0xFFFFFFu; return c==0xf2c09au || c==0xc98264u; }
/* repeint un sprite agrandi ; face : couleur des visages (heros) ou 0 */
static void spr_paint(Sprite *s,uint32_t face){
  int w=s->w, h=s->h, n=w*h;
  uint32_t *src=(uint32_t*)malloc((size_t)n*4); memcpy(src,s->px,(size_t)n*4);
  /* visages dans l'ombre : la peau devient sombre et teintee, les yeux disparaissent */
  if(face){
    for(int y=0;y<h;y++) for(int x=0;x<w;x++){ uint32_t c=src[y*w+x]; if(!(c>>24)) continue;
      if(is_skin(c)) src[y*w+x]= (c&0xFFFFFFu)==0xc98264u ? col_scale(face,0.72f) : face; }
    for(int y=0;y<h;y++) for(int x=0;x<w;x++){ uint32_t c=src[y*w+x]; if(!(c>>24) || lumc(c)>0.25f) continue;
      int sk=0; for(int dy=-2;dy<=2;dy++) for(int dx=-2;dx<=2;dx++){ uint32_t q= (x+dx>=0&&y+dy>=0&&x+dx<w&&y+dy<h)?src[(y+dy)*w+x+dx]:0; if((q&0xFFFFFFu)==(face&0xFFFFFFu)) sk++; }
      if(sk>=9) src[y*w+x]=col_scale(face,0.85f); }
  }
  /* volume : distance au bord */
  float *d=(float*)malloc((size_t)n*sizeof(float));
  for(int i=0;i<n;i++) d[i]=(src[i]>>24)?99.0f:0.0f;
  for(int pass=0;pass<2;pass++){
    for(int y=0;y<h;y++) for(int x=0;x<w;x++){ int i=y*w+x; if(d[i]==0) continue; float m=d[i];
      if(x>0) m=fminf(m,d[i-1]+1); if(y>0) m=fminf(m,d[i-w]+1); if(x>0&&y>0) m=fminf(m,d[i-w-1]+1.414f); if(x<w-1&&y>0) m=fminf(m,d[i-w+1]+1.414f); d[i]=m; }
    for(int y=h-1;y>=0;y--) for(int x=w-1;x>=0;x--){ int i=y*w+x; if(d[i]==0) continue; float m=d[i];
      if(x<w-1) m=fminf(m,d[i+1]+1); if(y<h-1) m=fminf(m,d[i+w]+1); if(x<w-1&&y<h-1) m=fminf(m,d[i+w+1]+1.414f); if(x>0&&y<h-1) m=fminf(m,d[i+w-1]+1.414f); d[i]=m; }
  }
  for(int i=0;i<n;i++) d[i]=fminf(d[i],5.0f);
  int top=h, bot=0; for(int y=0;y<h;y++) for(int x=0;x<w;x++) if(src[y*w+x]>>24){ if(y<top) top=y; if(y>bot) bot=y; }
  float span=bot>top?(float)(bot-top):1;
  static const uint8_t B4[4][4]={{0,8,2,10},{12,4,14,6},{3,11,1,9},{15,7,13,5}};
  static const float LV[4]={0.50f,0.76f,1.0f,1.18f};
  for(int y=0;y<h;y++) for(int x=0;x<w;x++){
    int i=y*w+x; uint32_t c=src[i]; if(!(c>>24)) continue;
    float dl=x>0?d[i-1]:0, dr=x<w-1?d[i+1]:0, du=y>0?d[i-w]:0, dd=y<h-1?d[i+w]:0;
    float nx=-(dr-dl), ny=-(dd-du), nz=1.6f; float l=sqrtf(nx*nx+ny*ny+nz*nz); nx/=l; ny/=l; nz/=l;
    float lam=clampf(nx*-0.55f+ny*-0.62f+nz*0.56f,0,1);
    float f=0.50f+0.66f*lam;
    f*=1.07f-0.16f*(y-top)/span;
    /* pli sombre au contact d'une masse voisine plus sombre, en dessous ou a droite */
    uint32_t cd= y<h-1?src[i+w]:0, crr= x<w-1?src[i+1]:0, cu= y>0?src[i-w]:0;
    if((cu>>24) && (cu&0xFFFFFFu)!=(c&0xFFFFFFu) && lumc(cu)>lumc(c)+0.12f) f-=0.18f;     /* sous une masse plus claire : ombre portee */
    if((cd>>24) && (cd&0xFFFFFFu)!=(c&0xFFFFFFu) && lumc(cd)+0.3f<lumc(c)) f-=0.08f;
    (void)crr;
    /* emissif : on garde la couleur vive */
    if(s->em && s->em[i]){ s->px[i]=col_lerp(c,HEX(0xffffff),0.15f); continue; }
    float q=(f-LV[0])/(LV[3]-LV[0])*3.0f + ((B4[y&3][x&3]+0.5f)/16.0f-0.5f)*0.32f;
    int li=clampi((int)floorf(q+0.5f),0,3);
    uint32_t o=hd_tone(c,LV[li]);
    /* contre-jour colore sur le bord droit (facon portraits) */
    if(d[i]<=1.6f && nx>0.25f){ uint32_t rim= face ? col_lerp(face,HEX(0xffffff),0.45f) : HEX(0xb8a8ff); o=col_lerp(o,rim,(face?0.5f:0.32f)*clampf(nx*1.6f,0,1)); }
    s->px[i]=o|0xFF000000u;
  }
  /* traits entre grandes masses de luminance tres differente */
  memcpy(src,s->px,(size_t)n*4);
  for(int y=0;y<h;y++) for(int x=0;x<w;x++){ int i=y*w+x; uint32_t c=src[i]; if(!(c>>24)) continue; if(s->em && s->em[i]) continue;
    uint32_t r= x<w-1?src[i+1]:0, b= y<h-1?src[i+w]:0;
    if(((r>>24) && lumc(r)-lumc(c)>0.34f) || ((b>>24) && lumc(b)-lumc(c)>0.34f)) s->px[i]=col_lerp(c,HEX(0x140e1c),0.45f);
  }
  free(src); free(d);
}
/* sprite HD a partir du croquis ASCII (brut, sans ombrage) ; outline=false : version sans contour (eclats) */
static Sprite spr_hd(const Sprite *raw,uint32_t face,int walk_side,bool outline){
  Sprite r=spr_copy(raw);
  if(walk_side>=0) spr_lift_foot(&r,walk_side);
  Sprite s=spr_scale2x(&r);
  free(r.px); if(r.em) free(r.em); if(r.nm) free(r.nm);
  spr_paint(&s,face);
  if(walk_side>=0){ /* le pied leve monte de 2 px a cette echelle : deja fait par le croquis */ }
  if(outline){ spr_outline_tinted(&s); spr_make_normals(&s,1.0f);
    /* moins de brillance : les couleurs peintes restent lisibles sous les lumieres */
    for(int i=0;i<s.w*s.h;i++){ uint32_t n=s.nm[i]; uint32_t sp=(n>>24)*2/5; s.nm[i]=(n&0x00FFFFFFu)|(sp<<24); } }
  return s;
}

static void mk_sprite(int id,const char *const *rows,int n,bool mir,const char *swap,int walk,float shade){
  Sprite raw=spr_art_raw(rows,n,mir,swap);
  if(shade>=1.0f){
    /* personnage : version HD (x2, repeinte) pour le jeu, ancienne version pour les petites vignettes */
    static const uint32_t FACE[CH_COUNT]={HEX(0xa23a8e),HEX(0xb03a8a),HEX(0xc23a6e),HEX(0x3a7a9a),HEX(0x9a5a3a),HEX(0x7a3aaa),HEX(0x3a72b0),HEX(0x7a3aa0),HEX(0x2a7a62),HEX(0x9a5a2a),HEX(0x3a5aa0),HEX(0x5a2a8a),HEX(0x2a7a4a),HEX(0x8a5a2a)};
    uint32_t face= id<CH_COUNT ? FACE[id] : 0;
    SPR[id]=spr_hd(&raw,face,-1,true);
    SPR_RAW[id]=spr_hd(&raw,face,-1,false);
    if(walk) for(int s=0;s<2;s++) SPR_WALK[id][s]=spr_hd(&raw,face,s,true);
    Sprite lo=spr_copy(&raw); spr_shade(&lo,shade);
    if(walk) for(int s=0;s<2;s++){ Sprite w=spr_copy(&lo); spr_lift_foot(&w,s); spr_outline_tinted(&w); spr_make_normals(&w,1.0f); SPR_LO_WALK[id][s]=w; }
    spr_outline_tinted(&lo); spr_make_normals(&lo,1.0f); SPR_LO[id]=lo;
    free(raw.px); if(raw.em) free(raw.em);
    return;
  }
  spr_shade(&raw,shade);
  SPR_RAW[id]=spr_copy(&raw);
  if(walk) for(int s=0;s<2;s++){ Sprite w=spr_copy(&raw); spr_lift_foot(&w,s); spr_outline_tinted(&w); spr_make_normals(&w,1.0f); SPR_WALK[id][s]=w; }
  spr_outline_tinted(&raw); spr_make_normals(&raw,1.0f); SPR[id]=raw; SPR_LO[id]=raw;
}
#define MK(id,arr,mir,swap)  mk_sprite(id,arr,COUNTOF(arr),mir,swap,0,1.0f)
#define MKW(id,arr,mir,swap) mk_sprite(id,arr,COUNTOF(arr),mir,swap,1,1.0f)
#define MKI(id,arr)          mk_sprite(id,arr,COUNTOF(arr),false,NULL,0,0.55f)

/* ---------------- heros ---------------- */
ART(A_SENTINELLE) = {
".....Cc.....","....CCc.....","...GWWWWG...","..GWWwwWWG..","..GWWWWWWG..",
"..GkkkkkkG..","..GWWkkWWG..","...GWWWWG...","..CCgWWgCC..",".CCCCWWCCCC.",
".sCCCYYCCCs.","..CCCCCCCC..","..nnnnnnnn..","...gg..gg...","...GG..GG..."};
ART(A_ALCHIMISTE) = {
".....LL.....","....LLLL....","...LLLLLL...","..LLEEEELL..","..LYcYYcYL..",
"..LEssssEL..","..LEsSSsEL..","...LLLLLL...","..PPPLLPPP..",".PPPPPPPPPP.",
".sPPPlLPPPs.","..PPPPPPPP..","..PPPPPPPP..","..PPPPPPPP..","...HH..HH..."};
ART(A_JONGLEUR) = {
"y..........y","rr........yy",".rrr....yyy.","..rrrryyyy..","..wwwwwwww..",
"..ssssssss..","..skssssks..","..sssmmsss..","...ssssss...","..ryyrryyr..",
".sryyrryyrs.","..yrryyrry..","..yrryyrry..","...rr..yy...","...kk..kk..."};
ART(A_FORGERONNE) = {
"...rRRRRr...","..rrrrrrrr..",".rrssssssrr.",".rssssssssr.",".rskssssksr.",
".rssssssssr.",".rrsSmmSsrr.",".rr.ssss.rr.",".r.HHHHHH.r.","..sHhhhhHs..",
".sshhhhhhss.","..HhhhhhhH..","..HdhhhhdH..","...dd..dd...","...KK..KK..."};
ART(A_MAGNETISEUR) = {
"Wg........gW","Wg........gW","rR..uuuu..nC","rR.uuuuuu.nC","rR.uecccu.nC",
"rR.uccccu.nC","rR.uUUUUu.nC",".rR.uuuu.nC.","..RRuyyunn..",".uuuuyyuuuu.",
".suuuuuuuus.","..uUuuuuUu..","..UUUUUUUU..","...uU..Uu...","...KK..KK..."};

ART(A_NECRO) = {
"...PPPP.....","..PPppPP..b.",".PPppppPPbbb",".PpkkkkpPbkb",".PkWvvWkPbbb",
".PkqWWqkP.h.",".PPWvvWPP.h.","..PPWWPPPsh.","..PPPPPPPPh.",".PPpPPPPPPh.",
"sPPpPPPPPPh.",".PPpPPPPPPh.",".PPPPPPPPPh.","..kk..kk..h.","..KK..KK..H."};
ART(A_HORLOGERE) = {
"....uuuu....","...uyyyyu...","..uyyyyyyu..","..uyssssyu..","..CiCssCiC..",
"..usssssSu..","...ssSSss...","....ssss....","..tTTYYTTt..",".tTTYyyYTTt.",
"sTtTYyyYTtTs",".TtTTYYTTtT.","..TtTttTtT..","...TT..TT...","...KK..KK..."};
ART(A_PYROMANE) = {
"....y...y...","..y.oy.yo...","..oyooyooy..",".JoooooooJ..",".JJssssssJJ.",
"..JskssksJ..","..JssssssJ..","...ssSSss...","..rrrrrrrr..","y.KKrrrrKK..",
"jsKKKrrKKKs.","J.KKKKKKKK..","..KKKKKKKK..","...KK..KK...","...dd..dd..."};
/* v7 */
ART(A_DRUIDE) = {
".h.....h.zz.",".hh...hh.zz.","..hLLLLh..h.","..LllllL..h.",".LlssssLL.h.",
".LskssksL.h.",".LssssssL.h.","..LsSSsL..h.","..lLssLl..h.",".lLLqqLLL.h.",
"slLLqqLLLsh.",".LLLLLLLL.h.","..LqLLqL..h.","...hh.hh..h.","...HH.HH..H."};
ART(A_MARCHAND) = {
"........cc..","....KKKKc...","...KKKKKK...","..KYYYYYYK..","KKKKKKKKKKKK",
"...ssssss...","...skssks...","...sHHHHs...","....sHHs....","..RRyRRyRR..",
".RRRRyyRRRR.","sRhhRRRRRRRs",".RhhRyyRRRR.","..RRRRRRRR..","...KK..KK..."};
/* v9 */
ART(A_ASTRONOME) = {
"....nn......","...nnnn.....","...nnynn....","..nnnnnnn...","..nnnnynnn..",
".CCCCCCCCCC.","...ssssss...","...skssks...","...ssSSss...","..CnnnnnnC..",
".nnnynnnnnc.","snnnnnnnnsci",".nnynnnynn..","..nnnnnnnn..","...KK..KK..."};
ART(A_VOLEUSE) = {
"....PPPP....","...PPPPPP...","..PPkkkkPP..","..PkkkkkkP..","..PkvkkvkP..",
"..PkkkkkkP..","..PPkkkkPP..","...PPppPP...","..KPPppPPK..",".KKPpppPPKK.",
"sKKPppppPKKs",".KKKPPPPKK.W","..KKKKKKKK.W","...KK..KK...","...dd..dd..."};

/* v10 */
ART(A_CHASSERESSE) = {
"....EEEE....","...ELLLLE...","..ELLLLLLE..","..ELssssLE..","..EskssksE..",
"..ELssssLE..","...ELSSLE...","..hELLLLEh..",".hHLLlLLLHh.","uhHLLlLLLHhs",
"uu.LLLLLL.s.",".u.HhhhhhH..","...LLLLLL...","...HH..HH...","...dd..dd..."};
ART(A_GEOMANCIEN) = {
"....HHHH...a","...HhhhhH.aj","..HhhhhhhH.a","..HhssssH..a","..HsksskH..a",
"..HsbbbbH..a","...bbbbbb..a","..UbbbbbbU.a",".UUubbbbuUUa","sUUuuUUuuUUs",
".UUuuUUuuU.a",".UUUAAAAUU.a","..UUUUUUUU.a","...UU..UU..A","...dd..dd..A"};

/* croquis des heros (figurines de l'ecran d'entrainement) */
const char *const *hero_art(int ch){
  static const char *const *T[CH_COUNT]={A_SENTINELLE,A_ALCHIMISTE,A_JONGLEUR,A_FORGERONNE,A_MAGNETISEUR,A_NECRO,A_HORLOGERE,A_PYROMANE,A_DRUIDE,A_MARCHAND,A_ASTRONOME,A_VOLEUSE,A_CHASSERESSE,A_GEOMANCIEN};
  return T[clampi(ch,0,CH_COUNT-1)];
}

/* ---------------- ennemis ---------------- */
ART(A_FOLLET1) = {
"....i.....","...ici....","..icCci...",".icCCCci..",".cCkCkCc..",
".cCCCCCc..","..cCCCc...","...cCc.c..","....c.c..."};
ART(A_FOLLET2) = {
".....i....","....ici...","..icCCci..",".icCCCCc..",".cCkCkCc..",
".cCCCCCc..","..cCCCc...",".c.cCc....","..c..c...."};
ART(A_MANTE) = {
"..L......L..","...L....L...","...lLllLl...","..llxllxll..","...llllll...",
"q...lLLl...q","Qq..llll..qQ",".Qq.lLLl.qQ.","..QqllllqQ..","....lLLl....",
"...lllLll...","..L.l..l.L..",".L..L..L..L."};
ART(A_CRACHEUR) = {
"....JjjJ....","...JjyyjJ...","...AJjjJA...","..AaaJJaaA..","..AaaaaaaA..",
".AayaaaayaA.",".AaaaaaaaaA.","AaaaJJJJaaaA","AaaaaaaaaaaA",".AAAAAAAAAA."};
ART(A_SENTVIDE) = {
".....vv.....","....vppv....","...vpPPpv...","..vpPPPPpv..",".vpPwwwwPpv.",
"vpPwwkkwwPpv","vpPwkxxkwPpv","vpPwwkkwwPpv",".vpPwwwwPpv.","..vpPPPPpv..",
"...vpPPpv...","....vppv....",".....vv....."};
ART(A_SQUELETTE) = {
"...bbbbbb...","..bbbbbbbb..","..bkkbbkkb..","..bkxbbxkb..","..bbbkkbbb..",
"...bbbbbb...","...bkbkbb...","....BBBB....",".b.bbbbbb.b.",".b.bkbbkb.b.",
".B..bbbb..B.","....bkkb....","....b..b....","...BB..BB..."};
ART(A_CHAUVE1) = {
"P....p..p....P","PP...pppp...PP","PPP.pxppxp.PPP","PPPPppppppPPPP",
"PPPPPppppPPPPP",".PP.PppppP.PP.",".P...pkkp...P.",".....p..p....."};
ART(A_CHAUVE2) = {
".....p..p.....",".....pppp.....","....pxppxp....","..PPppppppPP..",
".PPPPppppPPPP.","PPPPPppppPPPPP","PPP..pkkp..PPP","PP...p..p...PP"};
ART(A_GELEE) = {
"....qqqq....","..qqqqqqqq..",".qqeqqqqqqq.",".qeqqqqqqqq.","qqqqkqqkqqqq",
"qqqqkqqkqqqq","qqqqqqqqqqqq","QqqqqkkqqqqQ","QQqqqqqqqqQQ",".QQQQQQQQQQ."};
ART(A_GELETTE) = { "..qqqq..",".qeqqqq.","qqkqqkqq","qqqqqqqq","QqqqqqqQ",".QQQQQQ." };
ART(A_CRANE) = {
"B..........B","BB.bbbbbb.BB",".BbbbbbbbbB.","..bbbbbbbb..",".bkkkbbkkkb.",
".bkxkbbkxkb.",".bkkkbbkkkb.","..bbbkkbbb..","...bbbbbb...","...kbkbkb...","....bbbb...."};
ART(A_OSSUAIRE) = { /* moitie gauche, miroir */
"........BBBBB","......BBbbbbb",".....Bbbbbbbb","....Bbbbbbbbb","...Bbbbbbbbbb",
"...Bbbkkkkbbb","...Bbkkkkkkbb","...BbkkxxkkBb","...BbkkxxkkBb","...Bbbkkkkbbb",
"....Bbbbbbbkk","....BBbbbbbbk",".....BbbbbbBb","......BkbkbkB","BB.....BbbbbB",
"bBB...BBbbbbb",".bbB.BbbbBBbb","..bbBbbbBbbbb","...bbbbBbbBbb","....BbbbbbbBb",
"....BbBbbBbbb",".....BbbbbbBb","......BB.bbbb",".....BBB..BBB"};
ART(A_CHAMPIGNON) = {
"...ffffff...",".ffwffffwff.","fffffffffwff","ffwffffffffF","FffffffwfffF",
".FFFFFFFFFF.","...bbbbbb...","...bkbbkb...","...bbbbbb...","...bBkkBb...",
"..BbbbbbbB..",".BB.BBBB.BB."};
ART(A_ARAIGNEE) = {
"h..........h",".h..HHHH..h.","..hHHHHHHh..","hhHHxHHxHHhh","..HHHHHHHH..",
".hHHkHHkHHh.","h..HHHHHH..h","..h.HHHH.h..",".h........h."};
ART(A_SCARABEE) = {
"....h..h....",".....hh.....","....tTTt....","..tTTTTTTt..",".tTTtTTtTTt.",
"tTTTTtTTTTTt","tTtTTtTTtTTt","tTTTTtTTTTTt",".tTTTtTTTTt.","..ttTtTTtt..","..h.h..h.h.."};
ART(A_SPORANGE) = {
".......ffffff","....fffffffff","...ffffwwffff","..fffffwwffff",".ffwwffffffff",
".ffwwfffffwwf","ffffffffffwwf","fffffffffffff","FfffffffffffF","FFFFFFFFFFFFF",
".FFkFkFkFkFkF","....bbbbbbbbb","....bbbbbbbbb","....bbzzbbbbb","....bbzkbbbbb",
"....bbbbbbbbb","....bbbbbbkkk","....BbbbbbkRR","....BbbbbbbkR","....BBbbbbbbb",
"...BBbBbbbbbb","..BB.BBBbbBbb",".BB...BB.BBBB","BB.....B...BB"};
ART(A_DIABLOTIN) = {
"r..........r","rr..rrrr..rr",".rrrrrrrrrr.","..rryrryrr..","..rrrrrrrr..",
"...rkkkkr...","..RrrrrrrR..",".RrRrrrrRrR.","R..RrrrrR..R","...RRrrRR...",
"...R.RR.R...","..oo....oo.."};
ART(A_MAGMITE) = {
"....AAAA....","..AAaJJaAA..",".AaJjjjJaaA.",".AJjjjjjjJA.","AaJjkjjkjJaA",
"AAJjjjjjjJAA","AaaJjkkjJaaA",".AAaJJJJaAA.","..AAAaaAAA..","...A....A..."};
ART(A_ARMURE) = {
"....gggg....","...gWwwWg...","..gWwwwwWg..","..gkkkkkkg..","..gkxkkxkg..",
"..gWWWWWWg..","...gggggg...",".gWgWWWWgWg.","gWg.WwwW.gWg","gW..WwwW..Wg",
"....WWWW....","...gW..Wg...","...gW..Wg...","..ggg..ggg.."};
ART(A_GOLEM) = {
".........AAAA","......AAAaaaa",".....Aaaaaaaa","....Aaaaaaaaa","....AaaJJaaaa",
"....AaJjjJaaa","....AaaJJaaaa",".....Aaaaaaaa","..AAAAAaaaaaa",".AaaaaAAaaaaa",
"AaaaaaaAaaJaa","AaaaaaaAaJjJa","AaaaaaaAaaJaa","AaaaaaAAaaaaa",".AaaaaAAaaaaa",
"..AAAA.Aaaaaa",".AaaA..Aaaaaa","AaaaaA.AAaaaa","AaaaaA..AAaaa",".AAAA....Aaaa",
".........Aaaa","........AAaaa","........Aaaaa","........AAAAA"};
ART(A_OEIL) = {
"...wwwwww...","..wwwwwwww..",".wwwwwwRwww.",".wwwppppwww.","wwwpPkkPpwww",
"wRwpkekkpwRw","wwwpkkkkpwww",".wwwppppwww.",".wRwwwwwwWW.","..WwwwwwRW..",
"...MwMMwM...","...M.M.M.M.."};
ART(A_SPECTRE) = {
"....vvvv....","..vvvvvvvv..",".vvvvvvvvvv.",".vvkkvvkkvv.",".vvkkvvkkvv.",
"vvvvvvvvvvvv","vvvvvkkvvvvv","vvvvvkkvvvvv","vvvvvvvvvvvv","vvvvvvvvvvvv",
"vv.vvv.vvv.v","v...v...v..."};
ART(A_CHEVALIER) = {
"...pppppp...","..pvvvvvpp..","..pkkkkkkp..","..pkxkkxkp..","..ppvvvppp..",
".pPPPPPPPPp.","pPvPPPPPPvPp","pvPPPPPPPPvp","..PPPPPPPP..",".WWWWWWWWWW.",
"WggggggggggW","WgggvvgggggW","WggggggggggW",".WWWWWWWWWW."};
ART(A_OBELISQUE) = {
"....vv....","...vvpv...","...vppv...","..vvpPpv..","..vpPPpv..",
"..vpPzPv..","..vpzzPv..","..vpPzPv..","..vpPPpv..","..vvPPpv..",
"..vpPPPv..",".AvpPPpvA.",".AAvPPvAA.","AaAAAAAAaA","AaaaaaaaaA",".AAAAAAAA."};
ART(A_HORREUR) = {
"........PPPPP","......PPpppPP",".....PpppPPPp","....PpwwwPppp","...PpwwkwwPpp",
"...PpwkxkwPpp","...PpwwkwwPPp","..PPpPwwwPppp","..PpppPPPpppp",".PpwwwPppppww",
".PwwkwwPpppwk",".PwkxkwPpppwk",".PwwkwwPpppww","..PwwwPpPpppp","..PPpPppwwwPp",
"...PpppwwkwwP","..PPpppwkxkwP",".PpPpppwwkwwP",".PpPPpppwwwPp","..P.PpppPPPpp",
".....PppPpppp","......PP.PPpp",".......P..PPP","...........P."};

/* ---------------- v4 : les Profondeurs ---------------- */
/* Grottes de Givre */
ART(A_LOUP) = {
".W........W.",".WW......WW.",".WgWWWWWWgW.","..WWcWWcWW..",".WWiWWWWiWW.",
".WWWWWWWWWW.","..WWWkkWWW..","...WwwwwW...","..gWWWWWWg..",".gWWcWWcWWg.",
".gW.gWWg.Wg.",".gg..gg..gg."};
ART(A_ESPRIT) = {
"....i.....","...iWi....","..iWcWi...",".iWcccWi..","iWckckcWi.",
".iWcccWi..","..iWcWi...","...iWi....","..i.i.i...",".i..i..i.."};
ART(A_TORTUE) = {
"....cCCc....","..cCCnnCCc..",".cCnCCCCnCc.",".CCCCnnCCCC.","gCnCCCCCCnCg",
"gCCCnCCnCCCg",".CCCCCCCCCC.","g.CCnCCnCC.g","...CCCCCC...","...WWWWWW...",
"...WkWWkW...","....WWWW...."};
ART(A_HARFANG1) = {
"W....ww....W","WW..wwww..WW","WWW.wjwjw.WW","WgWWwwYwwWgW","WWWWWwwwWWWW",
".WW.WwwwW.WW",".W..WgwgW..W",".....wgw....",".....Y.Y...."};
ART(A_HARFANG2) = {
".....ww.....","....wwww....","....wjwjw...","..WWwwYwwWW.",".WWWWwwwWWWW",
"WWWgWwwwWgWW","WW..WgwgW..W","W....wgw...W",".....Y.Y...."};
ART(A_YETI) = { /* moitie gauche, miroir */
".............","...W.........","...WW........","....WW..WWWWW",".....WWWwwwww",
"......Wwwwwww","......WwCCCCC",".....WwCcCCCC",".....WwCkiCCC",".....WwCCCCCC",
".....WwCCkwkw","....WWwwCCkkk","...WWwwwwwwww","..WwwwwwwwWww",".WwwwWwwwwwww",
".WwwWWwwwwwWw","WwwW.Wwwwwwww","WwwW.WwwwwWww","WgwW.Wwwwwwww","iWiW.WWwwwwww",
".....WwwwWWWW",".....Wwww....","....WWwww....","....Wiwii...."};
/* Cite Engloutie */
ART(A_NOYE) = {
"...TTTTTT...","..TtttttTT..","..TtgggtTT..","..tgzggzgt..","..tggggggt..",
"...gkkkkg...","...gggggg...","..nnnnnnnn..",".gnWnnnnWng.",".g.nnnnnn.g.",
".T.WWWWWW.T.","...nnnnnn...","...nn..nn...","...KK..KK..."};
ART(A_MURENE) = {
"...L....","...LQ...","....LQ..",".....LQ.",".....LQ.","....LQ..","...LQ...",
"..LQ....","..LQ....","...LQ...","...yLQ..","..yLLQQ.","..zLLkQ.","...LLL.."};
ART(A_CRABE) = {
".rr......rr.","rRRr....rRRr","rr.r....r.rr","..r.w..w.r..","..rrkrrkrr..",
".rRrrrrrrRr.","rRrrRrrRrrRr","rrRrrrrrrRrr",".rRRRRRRRRr.","r.r.r..r.r.r"};
ART(A_MEDUSE) = {
"...mmmm...","..mffffm..",".mffwwffm.",".mfffffFm.","mmfkffkfmm",
"MmmmmmmmmM",".M.m.m.M..",".m.M.m.m..",".M.m.M.M..","..m.m.m...",
"..M.M.m...","...m..M..."};
ART(A_SIRENE) = {
"...TTTTT....","..TttttttT..",".TtsssssstT.",".TsksssksT..",".TssssSsstT.",
".TtssmmsstT.","..TtssssT...","..TtCCCCtT..",".s.tsssst.s.","...tttttt...",
"....tTtt....",".....tT.....","....tTTt...."};
ART(A_KRAKEN) = { /* moitie gauche, miroir */
"........PPPPP","......PPppppp",".....Pppppvpp","....Pppppppvp","...Ppppvppppp",
"...Pppppppppp","..Ppppppppppp","..Pppppvppppp","..Ppppppppppp","..PpkkkkPpppp",
"..Ppkxxkkpppp","..PpkkkkPpppp","...Pppppppppp","...PPpppppkkk","....PPpppppPP",
"..PP.Pp.pP.Pp",".Pp..Pp.Pp.Pp","Pp..Pp..Pp.Pp","p...Pp..Pp..P","p..Pp....Pp.P",
".p.Pp.....Pp.","..pP......Pp.","...p.......p.","...........p."};
/* Ruche d'Ambre */
ART(A_OUVRIERE1) = {
"ww...kk...ww","wWw.yyyy.wWw",".wWwykkywWw.","..wwYyyYww..","....kyyk....",
"...yykkyy...","...kkyykk...","....yykk....",".....Yk....."};
ART(A_OUVRIERE2) = {
".....kk.....","....yyyy....","..wwykkyww..",".wWwYyyYwWw.","wWw.kyyk.wWw",
"ww.yykkyy.ww","...kkyykk...","....yykk....",".....Yk....."};
ART(A_SOLDAT) = {
"..k......k..","...k....k...","...YYYYYY...","..YhjhhjhY..","..YhhhhhhY..",
"..Yk.hh.kY..","...YYYYYY...",".hYhYYYYhYh.","h.YYhYYhYY.h","h..YYYYYY..h",
"...YhYYhY...","...hh..hh...","...HH..HH..."};
ART(A_LARVE) = {
"....WWWW....","...WwwwwW...","..WwwWwwwW..","..WwWwwWwW..","..WwwwwwwW..",
"...WwWwwW...","...WkwwkW...","....WRRW...."};
ART(A_LARVULE) = { "..WWWW..",".WwwwwW.",".WwWwwW.",".WkwwkW.","..WRRW.." };
ART(A_FRELON) = {
"ww..k..k..ww","wWw.OOOO.wWw",".wWwOxOxOwW.","..wwOOOOww..","...kOOOOk...",
"...OOkkOO...","...kkOOkk...","....OOkk....",".....Ok.....",".....k......"};
ART(A_GARDIENNE) = {
"....YYYY....","...YjjjjY...","..YjJJJJjY..","..YjJjjJjY..","..YYjjjjYY..",
"...hYYYYh...","..hkhhhhkh..","..hhhkkhhh..",".h.hhhhhh.h.","h..hHhhHh..h",
"...hh..hh...","...HH..HH..."};
ART(A_COCON) = {
"....bb....","....b.....","...bbbb...","..bwwwwb..",".bwwBwwwb.",
".bwBwwBwb.",".bwwjjwwb.",".bwBjjBwb.",".bwwwwwwb.",".bwBwwBwb.",
"..bwwwwb..","..bwBBwb..","...bwwb...","....bb...."};
ART(A_BOURDON) = { /* moitie gauche, miroir */
"..ww.........",".wWWw........","wWWWWw...kk..","wWWWWWw...k..",".wWWWWWw.yyyy",
"..wWWWWWyyyyy","...wWWWykkyyy","....wWykjkyyy",".....yykkkyyy",".....yyyyyykk",
"....kkyyyyyyy","...kkkkkyyyyy","...kkkkkkkkkk","...yyyyyyyyyy","..yyyyyyyyyyy",
"..kkkkkkkkkkk","..yyyyyyyyyyy","...yyyyyyyyyy","...kkkkkkkkkk","....yyyyyyyyy",
".....kkkkkkkk","......kkkkkkk","..........kkk","...........kk"};
/* Neant */
ART(A_OMBRE) = {
"....KKKK....","...KkkkkK...","..KkkkkkkK..","..KkiKKikK..","..KkkkkkkK..",
"...KkkkkK...","..KKkkkkKK..",".KkKkkkkKkK.","Kk.KkkkkK.kK","K..KkkkkK..K",
"...KkkkkK...","...Kk.kKk...","..K.k..k.K..","..k..K...k.."};
ART(A_ECHO) = {
"....wwww....","...wKKKKw...","...wKiiKw...","...wKKKKw...","....wKKw....",
"..wwwKKwww..",".wKKKKKKKKw.","w.wKKKKKKw.w","w..wKKKKw..w","...wKwwKw...",
"...wK..Kw...","...ww..ww..."};
ART(A_ECHO_MINEUR) = { "..wwww..",".wKiiKw.",".wKKKKw.","..wKKw..",".wKKKKw.","w.wKKw.w","..wKKw..","..w..w.." };
ART(A_DEVOREUR) = {
"...nnnnnn...","..nCCCCCCn..",".nCnnCCnnCn.","nCnxnnnnxnCn","nCnnnnnnnnCn",
"nCkwkwkwkwCn","nCkkkkkkkkCn","nCkwkwkwkwCn",".nCnnnnnnCn.","..nCCnnCCn..",
"..n.nnnn.n..","...nn..nn..."};
ART(A_HERAUT) = {
"..iiiiii..",".i......i.","..iKKKKi..","..KkkkkK..",".KkiKKikK.",
".KkkkkkkK.","..KkkkkK..",".PKKvvKKP.","PvPKvvKPvP","P.PKvvKP.P",
"..PKvvKP..","..PKkkKP..","..PKkkKP..","...PkkP...","...P..P...","..P....P.."};
ART(A_COLOSSE_NEANT) = { /* moitie gauche, miroir */
".............","........KKKKK",".......Kkkkkk",".......Kkkiik",".......Kkkkkk",
"........Kkkkk","....KKKKKkkkk","...KkkkkkKkkk","..KkkikkkkKkk","..KkkkikkkKkk",
"..KkkkkkKkkkk","..KkkKkkKkkik","..KkkKKkKkkki","..KkkK.KKkkkk","..KkkK..Kkkkk",
"..KikK..Kkkkk","..KkkK..Kkikk","..KkiK..KKkkk",".KkkkK...Kkkk",".KiiK....Kkkk",
".........Kkk.","........Kkkk.","........Kkik.",".......KKKKK."};

/* ---------------- icones d'objets (10x10) ---------------- */
ART(A_I_FORCE) = {"...gggg...","..gWWWWg..",".gWWWWWWg.",".gWgWgWgg.",".gWWWWWWg.",".gWWWWWWg.","..gWWWWg..","..YYYYYY..","..gggggg..","..GGGGGG.."};
ART(A_I_CADENCE) = {"YYYYYYYYYY",".hyyyyyyh.","..hyyyyh..","...hyyh...","....yy....","....yy....","...h..h...","..h.yy.h..",".hyyyyyyh.","YYYYYYYYYY"};
ART(A_I_BOTTES) = {"...hhhh...","W..hHHh...","WW.hHHh...",".W.hHHh...","...hHHh...","..hhHHh...",".hhHHHhhh.","hhHHHHHHhh","hHHHHHHHHh","dddddddddd"};
ART(A_I_COEUR) = {".rrr..rrr.","rrwrrrrrrr","rwrrrrrrrr","rrrrrrrrrr","rrrrrrrrrR",".rrrrrrrR.","..rrrrrR..","...rrrR...","....rR....",".........."};
ART(A_I_RESSORT) = {"GGGGGGGGGG",".WWWWWWWW.","..g....g..",".WWWWWWWW.","..g....g..",".WWWWWWWW.","..g....g..",".WWWWWWWW.","GGGGGGGGGG",".........."};
ART(A_I_SAC) = {"....hh....","...hHHh...","....hh....","..hhhhhh..",".hhhhhhhh.","hhhcchhhhh","hhhcchhyyh","hhhhhhhyyh",".hhhhhhhh.","..HHHHHH.."};
ART(A_I_LYNX) = {"..........","...yyyy...",".yyyykyyy.","yyyykkyyyy","yyyekkkyyy","yyyykkkyyy","yyyykkyyyy",".yyyykyyy.","...yyyy...",".........."};
ART(A_I_LANTERNE) = {"....gg....","...g..g...","..GGGGGG..","..GyyyyG..","..GyjjyG..","..GyjjyG..","..GyyyyG..","..GGGGGG..","...GGGG...",".........."};
ART(A_I_MOUSSE) = {"..........","...l..l...","..lLl.lLl.","..lLlllLl.","...lLlLl..","....lLl...",".....L....","....hhh...","...hHHHh..","...hhhhh.."};
ART(A_I_FILET) = {"W...W...W.",".W.W.W.W.W","..W...W...",".W.W.W.W.W","W...W...W.",".W.W.W.W.W","..W...W...",".W.W.W.W.W","W...W...W.","hhhhhhhhhh"};
ART(A_I_TREFLE) = {"..LL..LL..",".LllLLllL.",".LllLLllL.","..LLllLL..","..LLllLL..",".LllLLllL.",".LllLLllL.","..LL.hLL..",".....h....","....h....."};
ART(A_I_BOUCLIER) = {".CCCCCCCC.","CcccccccCn","CcciiicCCn","CccciccCCn","CcciiicCCn","CccciccCCn",".CcciccCn.","..CccCCn..","...CCCn...","....nn...."};
ART(A_I_RAGE) = {"..y....y..",".oyo..oyo.",".rrrrrrrr.","rrwrrrrrrR","rwrrkrrkrR","rrrrrrrrrR",".rrkkkkrR.","..rwkwkR..","...rrrR...","....RR...."};
ART(A_I_FIOLE) = {"...hhhh...","....WW....","....WW....","...WWWW...","..WqqllW..",".WlqlllLW.",".WlllllLW.",".WLlllLLW.","..WLLLLW..","...WWWW..."};
ART(A_I_MIROIR) = {"..YYYYYY..",".YiiiiwiY.","YiiiiwiiiY","YiiiwiiiiY","YiiiiiiiiY",".YiiiiiiY.","..YYYYYY..","....hh....","....hh....","....HH...."};
ART(A_I_CAPE) = {"....PP....","...PppP...","..PpkkpP..","..PkxkxP..",".PpkkkkpP.",".PppkkppP.","PpppYppppP","PppPppPppP","Pp.Pp.Pp.P","P..P..P..P"};
ART(A_I_LOUPE) = {"..YYYY....",".YiiiiY...","YiieiiiY..","YieiiiiY..","YiiiiiiY..","YiiiiiiY..",".YiiiiY...","..YYYYhh..","......hhh.",".......hH."};
ART(A_I_ANCRE) = {"....gg....","...g..g...","....gg....","..hhhhhh..","....gg....","....gg....","g...gg...g","gg..gg..gg",".gggggggg.","...gggg..."};
ART(A_I_TOTEM) = {"...hhhh...","..hxhhxh..","w.hhhhhh.w",".whhkkhhw.","..hhhhhh..","w.hHhhHh.w",".whhhhhhw.","..hHHHHh..","..hhhhhh..",".HHHHHHHH."};

/* ---- v7 : le Royaume oublie ---- */
ART(A_LUCIOLE) = {"..ii..ii..",".iiii.iiii","..iikkii..","...kwwk...","...kkkk...","...zzzz...","..zzzzzz..","...zzzz...","....zz...."};
ART(A_FLEUR) = {"..f..ff..f..",".fFf.ff.fFf.",".ffFfFFfFff.","fffkkkkkkfff","FfkwkwwkwkfF",".fkkrrrrkkf.",".FfkrrrrkfF.","..FfkkkkfF..","....lLLl....","..ll.LL.ll..",".lLL.LL.LLl.","..l..LL..l..","....LLLL...."};
ART(A_ESCARGOT) = {"....LlLl......","...lLhhhL..w.w","..LhhHHhhL.q.q","..hHhhhhHh.q.q",".lhHhHHhHhLqqq",".LhHhhhhHhqqkq",".lhhHHHHhhqqq.","QqqqqqqqqqqqQ.",".QQQQQQQQQQQ.."};
ART(A_PAPILLON1) = {"vv........vv","vvv.k..k.vvv","vpvv.kk.vvpv","vPpvvKKvvpPv","vvvvvKKvvvvv",".vvvvKKvvvv.","..vv.KK.vv..",".....KK.....","............"};
ART(A_PAPILLON2) = {"....v..v....","...vvkkvv...","...vpkkpv...","..vvPKKPvv..","..vvvKKvvv..","...vvKKvv...","....vKKv....",".....KK.....","............"};
ART(A_SYLVAIN) = {".......LLLLLL",".....LLlllLLl","...LLlllLLllL","..LlllLLllllL",".LLllLllllLLl",".LlllllLLlllL","LLllLLllllllL","LllllllLLlllL",".LLlLllllLLll","..LLLLlLLLLLL","....hhHhhhHhh","...hhkkhhhhhH","...hkzkhhhhhh","...hhkkhhHhhh","....hhhhhhhhH","....HhhkkkkHh","....hhhhHhhhh","..hh.hhhhhhHh",".hH..Hhhhhhhh","hH...hhhHhhhh",".....hHhhhhHh","....hhh.hhhhH","...hhH...hhHh","..HHH.....HHH"};
ART(A_CRAPAUD) = {"..yy....yy..",".ykyQqqQyky.",".QyyqqqqyyQ.","QqqqLqqLqqqQ","QqLqqqqqqLqQ","QqqkkkkkkqqQ",".QqqbbbbqqQ.","QQqbbbbbbqQQ","q.QQbbbbQQ.q","qq.qq..qq.qq"};
ART(A_SANGSUE) = {"..MMMM..",".MrkkrM.",".MkrrkM.",".MRRRRM.","MRRMMRRM","MRMRRMRM","MRRMMRRM",".MRRRRM.",".MRMMRM.","..MRRM..","..MRRM..","...MM..."};
ART(A_SORCIERE) = {".....EE.....","....EEE.....","...EEEEE....","..EEEEEEE...","EEEEEEEEEEE.","...qqqqq....","...qkqqk....","..WqqLqqW...","..WWqqqWW...",".EEEEEEEEE..","zEEEEEEEEEq.",".EELEEEEEE..","..EEEEEEEE..","..dd....dd.."};
ART(A_MOUSTIQUE1) = {"ii........ii","iii..kk..iii",".iii.kk.iii.","..iiKxxKii..","....KKKK....","....KrrK....","...KKrrKK...","..K..rr..K..",".....gg....."};
ART(A_MOUSTIQUE2) = {"...i....i...","..iii..iii..","..iiikkiii..","...iKxxKi...","....KKKK....","....KrrK....","...KKrrKK...","..K..rr..K..",".....gg....."};
ART(A_HYDRE) = {"...........QQ","..QQ......QqQ",".QqqQ....QqqQ","QqxqqQ...QxqQ","QqqqqQ...QqqQ",".QkwkQ...QkkQ","..QqqQ...QqQ.","...QqQ..QqqQ.","....QqQQqqQ..",".....QqqqQQQQ","....QqqLqqqqq","...QqqLLqqqqq","..QqqLLqqqLLq","..QqLLqqqLLqq",".QqqLqqqLLqqq",".QqLLqqLLqqqq",".QqLqqqLqqbbb","QqqLqqqqqbbbb","QqqqqqqqbbbBb",".QqqqqqqbbBbb","..QQqqqqqbbbb","....QQqqqqqqq","...QQ..QQqqqQ","..QQ....QQ.QQ"};
ART(A_GOLEM_CRISTAL) = {"...v....v...","..vpv..vpv..","..vPAAAAPv..","..AaaaaaaA..","..AaiaaiaA..","..AaaaaaaA..",".vAAaaaaAAv.","vpAaAAAAaApv","vPAaaaaaaAPv","..AaapvaaA..","..AaaPPaaA..","..AAaaaaAA..","..Aa....aA..",".AAA....AAA."};
ART(A_MINEUR) = {"....YYYY....","...YYjjYY...","..YYYYYYYY..","...ciiiic...","...ikiiki...","...iiiiii...","....icci....",".h.cciicc...","hhgciiiicc..",".gciiiiiic..","g.ciiiiiic..","..ciiiiic...","..cic.cic...","..cc...cc..."};
ART(A_VER) = {"..UUUUUU..",".UkwkwkwU.",".UwkkkkwU.",".UUuuuuUU.","..UuuuuU..","..uUUUUu..","..UuuuuU..","..uUUUUu..","..UuuuuU..","..uUUUUu..","...UuuU...","...uUUu...","....UU....","....uu...."};
ART(A_ARAIGNEE_CRISTAL) = {"v..........v",".v..MMMM..v.","..vMmmmmMv..","vvMmiMMimMvv","..MmmmmmmM..",".vMmMmmMmMv.","v..MmmmmM..v","..v.MMMM.v..",".v........v."};
ART(A_QUARTZ) = {"..........mm.",".........mwmm","....m...mmwMm","...mwm..mMwMm","...mMm.vMmmMv","..vmMvvvMMMMv","..vvMvvvvvvvv","...vvaaaaaaaa","....AaaaaaaaA","....AaiiaaaaA","....AaiiaaaaA","...vAaaaaaaaA","..vmvAaaakkkk",".vmMvAaaaaaaa","vmMmvAAaaaaaa","vMmvAAaamMaaa",".vvAAaamwmMaa","...AaaamMmMaa","...Aaaaaaaaaa","...AAaaaaaaaA","....AAaaaaaAA","....Aaa..Aaaa","...AAaa..AAaa","..AAAA...AAAA"};
ART(A_PRETRE) = {".....yy.....","....yYYy....","...yYYYYy...","...YKKKKY.j.","...YKjKjY.jj","...YKKKKY.h.","..WWyyyyWWh.",".WwwYyyYwwh.","sWwwwYYwwwsh",".WwwwYYwwWh.",".WwwwYYwwW..","..WwwYYwwW..","..WWWWWWWW..","...YY..YY..."};
ART(A_MOMIE) = {"...bbBbbB...","..BbbbbBbb..","..bKjbbjKb..","..BbbBbbbB..","...bBbbBb...","..bbbbbbbB..",".bBbbBbbbbb.","bBbbbbbBbbBb","b.bBbbbbbB.b","B.bbbBbbbb.B","..BbbbbbBb..","..bbB.Bbbb..","..Bbb..bBb..","..BBB..BBB.."};
/* v7 : phenix de face, ailes de flammes levees puis rabattues (moitie gauche, miroir) */
ART(A_PHENIX1) = {"J......j","jJ.....J","xjJ...yo",".xjJ..ky","..xjJroY","...xjJro","....xJro",".....Rro","......RJ",".....J.j","....j..J",".......j"};
ART(A_PHENIX2) = {".......j",".......J","......yo","......ky","JJj..roY","xjJJjJro",".xxjJJro","...xxJRo",".....xRJ",".....J.j","....j..J",".......j"};
ART(A_GARDIEN_SOL) = {"....YYYY....","...YyyyyY...","...YKKKKY...","...YKjjKY...","...YyyyyY...","..YYYYYYYY..","yyyYyyyyY...","yjjyYyyyYs..","yjjjyyyyY...","yjjyYYYYY...","yyyYyyyyY...","..YYyyyyY...","..Yy....yY..","..YY....YY.."};
ART(A_SPHINX) = {".........YYYY","........YnYnY",".......YnYnYn","......YnYnYnY","......nYsssss",".....YnYskkss",".....nYssssss",".....YnYsssSs","......nYssSSs",".......YYssss","........YYYYY","......yyyyyyy","....yyyyyyyyy","...yyYyyyyyyy","..yyYyyyyyyyy","..yYyyyyyyyyy",".yyYyyyyyyyyy",".yYyyyyyyyyyy",".yyyyyyyyyyyy","yyYyyyyyyyyyy","yYyyyyyyyyyyy","yyy.yyy..yyyy","YYY.YYY..YYYY","............."};

/* ---------------- billes ---------------- */
void art_draw_ball(float x,float y,float r,int kind,float t,bool glow){
  const BallDef *d=&BALLS[kind];
  int ix=(int)floorf(x+0.5f), iy=(int)floorf(y+0.5f); int ir=(int)(r+0.5f); if(ir<1) ir=1;
  if(glow) gfx_glow(x,y,r*3.2f+2,d->col,0.55f);
  uint32_t c1=d->col, c2=d->col2;
  if(kind==BK_SPECTRE || kind==BK_FAUCHEUSE || kind==BK_GHOST || kind==BK_RAYON){
    if(kind==BK_RAYON && glow){ for(int k=0;k<4;k++){ float a=t*2.5f+k*PI*0.5f; gfx_capsule_add(x,y,x+cosf(a)*(r+5),y+sinf(a)*(r+5),0.6f,HEX(0xffe8a0),150); } }
    gfx_disc_a(ix,iy,ir,c2,150); gfx_disc_a(ix,iy,ir-1,c1,170);
    if(kind==BK_GHOST && ir>=2){ gfx_pset(ix-1,iy,HEX(0x1a3020)); gfx_pset(ix+1,iy,HEX(0x1a3020)); }
    gfx_pset(ix-ir/2,iy-ir/2,HEX(0xffffff));
    return;
  }
  if(kind==BK_CRISTAL || kind==BK_GEODE){
    float rr=r+0.8f;
    gfx_tri(x,y-rr,x-rr,y,x+rr,y,c1); gfx_tri(x-rr,y,x+rr,y,x,y+rr,c2);
    gfx_tri(x,y-rr,x-rr*0.2f,y,x+rr*0.6f,y,col_lerp(c1,HEX(0xffffff),0.5f));
    if(kind==BK_GEODE){ gfx_pset(ix,iy+1,HEX(0x9af05a)); gfx_pset(ix-1,iy+2,HEX(0x9af05a)); gfx_pset(ix+1,iy,HEX(0xc8ff8a)); }
    gfx_pset(ix-1,iy-ir+1,HEX(0xffffff));
    if(IS_EVO(kind)) gfx_ring_a(ix,iy,ir+1,HEX(0xc08aff),110,true);
    return;
  }
  gfx_disc(ix,iy,ir,c2);
  if(ir>=2) gfx_disc(ix-(ir>=3?1:0),iy-(ir>=3?1:0),ir-1,c1);
  if(kind==BK_LUMIERE || kind==BK_AURORE){
    if(glow){ float l=r+3+sinf(t*9)*1.0f; gfx_capsule_add(x-l,y,x+l,y,0.5f,HEX(0xfff4c0),170); gfx_capsule_add(x,y-l,x,y+l,0.5f,HEX(0xfff4c0),170); }
    if(kind==BK_AURORE){ for(int dy=-ir;dy<=ir;dy++) for(int dx=-ir;dx<=ir;dx++){ if(dx*dx+dy*dy>ir*ir) continue; float k=0.5f+0.5f*sinf(t*4+dy*0.9f+dx*0.3f); gfx_pset(ix+dx,iy+dy,col_lerp(HEX(0xff8ad8),HEX(0x7af0e0),k)); } }
  }
  if(kind==BK_FOLLE || kind==BK_KALEIDO){
    float a=t*14;
    if(kind==BK_KALEIDO){
      static const uint32_t KC[6]={HEX(0xff6a6a),HEX(0xffd84a),HEX(0x7af07a),HEX(0x5ab8ff),HEX(0xc08aff),HEX(0xff8ad8)};
      for(int dy=-ir;dy<=ir;dy++) for(int dx=-ir;dx<=ir;dx++){ if(dx*dx+dy*dy>ir*ir) continue; float an=atan2f((float)dy,(float)dx)+a*0.4f; int s=((int)floorf(an/(PI/3))%6+6)%6; gfx_pset(ix+dx,iy+dy,KC[s]); }
    } else {
      for(int k=0;k<2;k++){ float aa=a+k*PI; gfx_pset(ix+(int)roundf(cosf(aa)*(ir-1)),iy+(int)roundf(sinf(aa)*(ir-1)),HEX(0xffe0ff)); }
    }
  }
  if(kind==BK_RAFALE || kind==BK_BLIZZARD || kind==BK_TEMPFEU){
    uint32_t sc = kind==BK_TEMPFEU ? HEX(0xffe080) : HEX(0xffffff);
    float a=-t*16;
    for(int k=0;k<3;k++){ float aa=a+k*TAU/3; gfx_pset(ix+(int)roundf(cosf(aa)*(ir-0.5f)),iy+(int)roundf(sinf(aa)*(ir-0.5f)),sc); gfx_pset(ix+(int)roundf(cosf(aa+0.5f)*(ir*0.5f)),iy+(int)roundf(sinf(aa+0.5f)*(ir*0.5f)),sc); }
    if(kind==BK_BLIZZARD && glow){ for(int k=0;k<6;k++){ float aa=t*1.5f+k*PI/3; gfx_padd(ix+(int)roundf(cosf(aa)*(r+3)),iy+(int)roundf(sinf(aa)*(r+3)),HEX(0xd8f4ff),220); } }
  }
  if(kind==BK_PRISME && ir>=2){
    static const uint32_t RB[5]={HEX(0xff6a6a),HEX(0xffd84a),HEX(0x7af07a),HEX(0x5ab8ff),HEX(0xc08aff)};
    int off=(int)(t*6);
    for(int dy=-ir;dy<=ir;dy++) for(int dx=-ir;dx<=ir;dx++){ if(dx*dx+dy*dy>ir*ir) continue; int band=((dx+dy+2*ir)*5/(4*ir+1)+off)%5; gfx_pset(ix+dx,iy+dy,RB[band]); }
  }
  if(kind==BK_FLEAU){ for(int dy=-ir;dy<=ir;dy++) for(int dx=0;dx<=ir;dx++) if(dx*dx+dy*dy<=ir*ir && dy>dx-1) gfx_pset(ix+dx,iy+dy,HEX(0x8a3ab8)); }
  if(kind==BK_VAPEUR && ir>=3){ gfx_hline(ix-ir+1,ix+ir-2,iy+1,HEX(0xf4fbff)); gfx_hline(ix-ir+2,ix+ir-1,iy-1,HEX(0x8ab0d0)); }
  if(kind==BK_TEMPETE && ir>=3){ gfx_pset(ix,iy-2,HEX(0xfff070)); gfx_pset(ix-1,iy-1,HEX(0xfff070)); gfx_pset(ix,iy,HEX(0xfff070)); gfx_pset(ix+1,iy,HEX(0xfff070)); gfx_pset(ix,iy+1,HEX(0xfff070)); gfx_pset(ix-1,iy+2,HEX(0xfff070)); }
  if(kind==BK_ENCLUME && ir>=3){ gfx_hline(ix-ir+1,ix+ir-1,iy,HEX(0x4a4a6a)); }
  if(kind==BK_METEORE && ir>=3){ gfx_disc(ix+1,iy+1,ir/2,HEX(0xffe080)); }
  if(kind==BK_AIMANT || kind==BK_SINGULARITE){
    /* moitie rouge, moitie bleue */
    for(int dy=-ir;dy<=ir;dy++) for(int dx=-ir;dx<=0;dx++) if(dx*dx+dy*dy<=ir*ir) gfx_pset(ix+dx,iy+dy,kind==BK_SINGULARITE?HEX(0x1a0a2a):HEX(0xe0453a));
    if(kind==BK_SINGULARITE) gfx_ring_a(ix,iy,ir+1,HEX(0xc08aff),200,true);
  }
  if(kind==BK_FOUDRE || kind==BK_TEMPETE || kind==BK_PRISME){
    if(((int)(t*20))&1){ gfx_padd(ix-ir-1,iy,HEX(0xffffa0),200); gfx_padd(ix+ir+1,iy,HEX(0xffffa0),200); }
  }
  /* v7 */
  if(kind==BK_ECHO || kind==BK_TONNERRE){
    if(ir>=3) gfx_ring_a(ix,iy,ir-2,c2,210,false);
    if(glow){ float ph=fmodf(t*2.5f,1.0f); gfx_ring_a(ix,iy,(int)(ir+1+ph*4),c1,(int)(170*(1-ph)),true); }
    if(kind==BK_TONNERRE && ir>=3){ gfx_pset(ix,iy-2,HEX(0xfff070)); gfx_pset(ix-1,iy-1,HEX(0xfff070)); gfx_pset(ix,iy,HEX(0xfff070)); gfx_pset(ix+1,iy,HEX(0xfff070)); gfx_pset(ix,iy+1,HEX(0xfff070)); }
  }
  if(kind==BK_DOREE || kind==BK_MIDAS){
    if(ir>=3) gfx_hline(ix-ir+2,ix+ir-2,iy-1,HEX(0xfff4c0));
    if(glow && (((int)(t*5))%3)==0) gfx_padd(ix+ir-1,iy-ir+1,HEX(0xffffff),255);
    if(kind==BK_MIDAS) gfx_ring_a(ix,iy,ir,HEX(0xfff0a0),130,true);
  }
  if(kind==BK_RACINE || kind==BK_RONCES){
    if(ir>=3){ gfx_pset(ix+1,iy+1,HEX(0x5a3a1c)); gfx_pset(ix-1,iy+2,HEX(0x5a3a1c)); gfx_pset(ix+2,iy-1,HEX(0x7a5a2a)); }
    gfx_pset(ix+ir-1,iy-1,HEX(0x8ac850)); gfx_pset(ix-ir+1,iy+1,HEX(0x6ab040)); gfx_pset(ix,iy-ir+(ir>=3?1:0),HEX(0x8ac850));
    if(kind==BK_RONCES) for(int k=0;k<6;k++){ float aa=k*TAU/6+t*1.5f; gfx_pset(ix+(int)roundf(cosf(aa)*(ir+1)),iy+(int)roundf(sinf(aa)*(ir+1)),HEX(0xd8c8a0)); }
  }
  if(kind==BK_ACIDE || kind==BK_PLUIE){
    for(int k=0;k<3;k++){ float ph=fmodf(t*1.5f+k*0.33f,1.0f); int bx=ix+(k-1)*(ir/2), by=iy+ir-1-(int)(ph*(2*ir-1)); gfx_pset(bx,by,HEX(0xf8ffd0)); }
    if(glow && kind==BK_PLUIE){ float ph=fmodf(t*3,1.0f); gfx_padd(ix,iy+ir+1+(int)(ph*3),HEX(0xd8ff40),(int)(220*(1-ph))); }
  }
  if(IS_EVO(kind) && ir>=3) gfx_ring_a(ix,iy,ir,HEX(0xffffff),90,true);
  if(ir>=2) gfx_pset(ix-ir/2-(ir>=4?1:0),iy-ir/2-(ir>=4?1:0),HEX(0xffffff));
}

void art_ball_icon(Sprite *s,int kind){
  *s=spr_alloc(16,16);
  Target t=spr_target(s); gfx_target(&t);
  art_draw_ball(7.5f,7.5f,kind==BK_BABY?3.f:(kind==BK_PLOMB||kind==BK_SINGULARITE||kind==BK_ENCLUME?6.f:5.f),kind,0,false);
  if(IS_EVO(kind)){ gfx_pset(13,2,HEX(0xffffff)); gfx_pset(12,2,HEX(0xffe080)); gfx_pset(13,3,HEX(0xffe080)); }
  gfx_target(&g_scr);
  spr_outline(s,HEX(0x120e18));
}

/* ---------------- tentacule procedural ---------------- */
void art_draw_tentacle(float bx,float by,float t,float len,uint32_t c1,uint32_t c2,float ang){
  int n=12; float x=bx, y=by; float a=ang;
  float seg=len/n;
  float px[16], py[16];
  for(int i=0;i<=n;i++){
    px[i]=x; py[i]=y;
    a = ang + sinf(t*2.2f + i*0.55f)*0.35f*(float)i/n*2.0f;
    x += cosf(a)*seg; y += sinf(a)*seg;
  }
  for(int i=0;i<n;i++){
    float r0 = 4.5f*(1.0f-(float)i/n)+1.0f;
    gfx_thick_line(px[i],py[i],px[i+1],py[i+1],r0,c2);
  }
  for(int i=0;i<n;i++){
    float r0 = 4.5f*(1.0f-(float)i/n)+1.0f - 1.2f;
    if(r0<0.6f) r0=0.6f;
    gfx_thick_line(px[i]-0.6f,py[i]-0.6f,px[i+1]-0.6f,py[i+1]-0.6f,r0,c1);
  }
  for(int i=1;i<n-2;i+=2) gfx_pset((int)px[i]+1,(int)py[i],col_scale(c1,1.3f));
}

/* ---------------- boss proceduraux ---------------- */
static Sprite boss_canvas, boss_emc; static bool boss_em_on=false;
float g_boss_draw_scale=1.0f;   /* codex : gardiens en miniature */
/* dessine aussi sur la toile de lumiere propre du boss (yeux, lave) */
#define EMIT(stmt) do{ stmt; if(boss_em_on){ Target *_o=gT; Target _t=spr_target(&boss_emc); gfx_target(&_t); stmt; gfx_target(_o); } }while(0)
static void sh_ellipse(int cx,int cy,int rx,int ry,uint32_t light,uint32_t mid,uint32_t dark){
  gfx_ellipse(cx,cy,rx,ry,dark);
  gfx_ellipse(cx-1,cy-2,rx-2,ry-3,mid);
  gfx_ellipse(cx-rx/3,cy-ry/3,rx/3,ry/4,light);
}

static void boss_roi(float t,float anim,float lx,float ly,int phase){
  int cx=56;
  /* cape */
  gfx_ellipse(cx,64,44,26,HEX(0x2a1240));
  for(int i=0;i<9;i++){ float x=cx-40+i*10; gfx_tri(x-5,80,x+5,80,x+(i&1?2:-2),94+((i*7)%5),HEX(0x2a1240)); }
  gfx_ellipse(cx,62,38,20,HEX(0x3d1c5c));
  /* mains */
  for(int s=-1;s<=1;s+=2){
    float hx=cx+s*44, hy=70+sinf(t*2+s)*3 - anim*6;
    gfx_disc((int)hx,(int)hy,7,HEX(0xb3a07e)); gfx_disc((int)hx-1,(int)hy-1,6,HEX(0xeadfc4));
    for(int f=0;f<4;f++){ float fx=hx-6+f*4; gfx_thick_line(fx,hy+4,fx+s*1,hy+12,1.2f,HEX(0xd8c8a4)); }
  }
  /* crane */
  sh_ellipse(cx,44,28,25,HEX(0xfff6dc),HEX(0xe2d4b0),HEX(0xa8946c));
  gfx_ellipse(cx-15,58,6,3,HEX(0xb8a47c)); gfx_ellipse(cx+15,58,6,3,HEX(0xb8a47c));
  /* orbites */
  gfx_ellipse(cx-11,42,8,9,HEX(0x140c18)); gfx_ellipse(cx+11,42,8,9,HEX(0x140c18));
  uint32_t eye = phase ? HEX(0xff8a2a) : HEX(0xff3b3b);
  EMIT(gfx_disc((int)(cx-11+lx*2),(int)(43+ly*2),2,eye)); EMIT(gfx_disc((int)(cx+11+lx*2),(int)(43+ly*2),2,eye));
  EMIT(gfx_pset((int)(cx-12+lx*2),(int)(42+ly*2),HEX(0xffffff))); EMIT(gfx_pset((int)(cx+10+lx*2),(int)(42+ly*2),HEX(0xffffff)));
  /* nez */
  gfx_tri(cx,51,cx-4,58,cx+4,58,HEX(0x2a1c20));
  /* machoire */
  int jy=63+(int)(anim*9);
  gfx_rect(cx-17,jy,34,10,HEX(0xc8b48c)); gfx_rect(cx-15,jy+10,30,3,HEX(0xa8946c));
  gfx_rect(cx-16,jy,32,2,HEX(0xe2d4b0));
  for(int i=0;i<7;i++){ gfx_rect(cx-15+i*4+1,jy+1,3,4,HEX(0xfff6dc)); gfx_rect(cx-15+i*4+1,61,3,3,HEX(0xfff6dc)); }
  if(anim>0.2f) gfx_rect(cx-14,64,28,jy-64,HEX(0x140c18));
  /* couronne */
  uint32_t gold=HEX(0xf2c23a), goldd=HEX(0xa8741c);
  for(int i=0;i<5;i++){ float x=cx-24+i*12; float h=(i==2)?16:11; gfx_tri(x-6,22,x+6,22,x,22-h,goldd); gfx_tri(x-4,22,x+4,22,x,22-h+3,gold); gfx_disc((int)x,(int)(22-h),1,HEX(0xfff0a0)); }
  gfx_rect(cx-28,20,56,8,goldd); gfx_rect(cx-28,20,56,5,gold);
  gfx_disc(cx-14,24,2,HEX(0xd8203a)); gfx_disc(cx,24,2,HEX(0x3b7fdc)); gfx_disc(cx+14,24,2,HEX(0xd8203a));
}

static void half_ellipse(int cx,int cy,int rx,int ry,uint32_t c,bool top){
  for(int dy=-ry;dy<=ry;dy++){
    if(top && dy>0) break;
    if(!top && dy<0) continue;
    float fy=(float)dy/((float)ry+0.5f); float v=1-fy*fy; if(v<0) continue;
    int hw=(int)floorf((float)rx*sqrtf(v)+0.5f); gfx_hline(cx-hw,cx+hw,cy+dy,c);
  }
}

static void boss_mere(float t,float anim,float lx,float ly,int phase){
  int cx=60;
  /* racines */
  for(int i=0;i<7;i++){
    float a=PI*0.15f+i*(PI*0.7f/6); float x0=cx+(i-3)*6, y0=80;
    float x1=x0+cosf(a)*30*(i<3?-1:1)*0.4f+(i-3)*8, y1=y0+12+sinf(t*1.5f+i)*3;
    gfx_thick_line(x0,y0,x1,y1,2.2f,HEX(0x6a2a4a));
  }
  /* tronc / visage */
  gfx_ellipse(cx,70,22,18,HEX(0xb89c86)); gfx_ellipse(cx-1,68,20,16,HEX(0xe8d8c0));
  gfx_ellipse(cx-8,64,5,4,HEX(0xfff4e0));
  uint32_t ec = phase? HEX(0xff6a3a): HEX(0xeaff6a);
  gfx_disc(cx-9,66,5,HEX(0x2a1420)); gfx_disc(cx+9,66,5,HEX(0x2a1420));
  EMIT(gfx_disc((int)(cx-9+lx*1.5f),(int)(66+ly*1.5f),3,ec)); EMIT(gfx_disc((int)(cx+9+lx*1.5f),(int)(66+ly*1.5f),3,ec));
  gfx_pset((int)(cx-10+lx*1.5f),(int)(65+ly*1.5f),HEX(0xffffff)); gfx_pset((int)(cx+8+lx*1.5f),(int)(65+ly*1.5f),HEX(0xffffff));
  gfx_ellipse(cx,78,7,1+(int)(anim*4),HEX(0x5a1028));
  /* chapeau (demi-ellipse) */
  float p=1.0f+sinf(t*2.0f)*0.04f;
  int ry=(int)(40*p);
  half_ellipse(cx,46,52,9,HEX(0x6a2850),false);
  for(int i=-44;i<=44;i+=5) gfx_vline(cx+i,47,51,HEX(0x8a3a68));
  half_ellipse(cx,46,56,ry+2,HEX(0x6a2850),true);
  half_ellipse(cx,46,54,ry,HEX(0x9a3a72),true);
  half_ellipse(cx-2,44,50,ry-5,HEX(0xd85a9a),true);
  gfx_ellipse(cx-18,22,16,7,HEX(0xf59ac0));
  static const int SP[][3]={{-30,26,6},{-8,14,5},{18,22,7},{38,34,4},{-42,38,3},{4,34,4},{30,12,3},{-20,40,3}};
  for(int i=0;i<COUNTOF(SP);i++){ gfx_disc(cx+SP[i][0],SP[i][1],SP[i][2],HEX(0xf6e8f0)); gfx_disc(cx+SP[i][0]-1,SP[i][1]-1,SP[i][2]-1,HEX(0xffffff)); }
}

static void boss_colosse(float t,float anim,float lx,float ly,int phase){
  int cx=62; (void)lx; (void)ly;
  uint32_t st=HEX(0x6e6878), std=HEX(0x48424f), stl=HEX(0x948ea0);
  float pulse=0.6f+0.4f*sinf(t*4);
  uint32_t lava=col_lerp(HEX(0xff6a1a),HEX(0xffe080),pulse);
  if(phase) lava=col_lerp(HEX(0xff3a1a),HEX(0xfff0a0),pulse);
  /* jambes */
  gfx_rect(cx-24,78,16,16,std); gfx_rect(cx+8,78,16,16,std);
  gfx_rect(cx-22,78,12,12,st); gfx_rect(cx+10,78,12,12,st);
  /* corps */
  gfx_rect(cx-34,34,68,48,std); gfx_rect(cx-32,34,62,44,st); gfx_rect(cx-30,36,24,14,stl);
  /* fissures de lave */
  EMIT(gfx_line(cx-20,40,cx-10,56,lava)); EMIT(gfx_line(cx-10,56,cx-14,70,lava));
  EMIT(gfx_line(cx+18,38,cx+12,52,lava)); EMIT(gfx_line(cx+12,52,cx+22,66,lava));
  gfx_disc(cx,58,8,HEX(0x3a1a10)); EMIT(gfx_disc(cx,58,6,lava)); EMIT(gfx_disc(cx-1,57,3,HEX(0xfff4c0)));
  /* tete */
  gfx_rect(cx-15,12,30,24,std); gfx_rect(cx-13,12,26,22,st); gfx_rect(cx-17,18,34,5,stl);
  gfx_rect(cx-10,25,20,4,HEX(0x1a0e10)); EMIT(gfx_rect(cx-9,26,18,2,lava));
  /* bras */
  float drop=anim*16;
  for(int s=-1;s<=1;s+=2){
    int sx=cx+s*44;
    gfx_disc(sx,40,13,std); gfx_disc(sx-1,39,11,st); gfx_disc(sx-4,35,4,stl);
    gfx_rect(sx-6,46,12,(int)(20+drop),std);
    int fy=(int)(66+drop);
    gfx_rect(sx-12,fy,24,20,std); gfx_rect(sx-11,fy,21,17,st); gfx_rect(sx-11,fy,21,4,stl);
    EMIT(gfx_line(sx-6,fy+8,sx+4,fy+14,lava));
  }
}

static void boss_oeil(float t,float anim,float lx,float ly,int phase){
  int cx=64, cy=52;
  uint32_t m1=HEX(0x7a3a9a), m2=HEX(0x4a2266), m3=HEX(0x2a1240);
  if(phase){ m1=HEX(0x9a2a6a); m2=HEX(0x5a1a46); m3=HEX(0x30102a); }
  for(int i=0;i<6;i++){
    float a = PI*0.5f + (i-2.5f)*0.42f;
    art_draw_tentacle(cx+cosf(a)*30,cy+sinf(a)*26,t+i*0.7f,30,m1,m3,a);
  }
  gfx_ellipse(cx,cy,48,40,m3); gfx_ellipse(cx-1,cy-2,46,37,m2);
  static const int B[][3]={{-38,-18,10},{36,-22,11},{-30,22,9},{32,20,10},{0,-34,12},{-12,30,8},{16,30,8}};
  for(int i=0;i<COUNTOF(B);i++){ gfx_disc(cx+B[i][0],cy+B[i][1],B[i][2],m2); gfx_disc(cx+B[i][0]-1,cy+B[i][1]-2,B[i][2]-2,m1); }
  /* petits yeux */
  static const int E[][2]={{-36,-16},{35,-20},{-28,24},{30,22}};
  for(int i=0;i<4;i++){
    float bl=sinf(t*1.3f+i*2.1f);
    if(bl>0.95f) { gfx_hline(cx+E[i][0]-3,cx+E[i][0]+3,cy+E[i][1],m3); continue; }
    gfx_disc(cx+E[i][0],cy+E[i][1],4,HEX(0xf0e8f0)); gfx_disc((int)(cx+E[i][0]+lx*1.5f),(int)(cy+E[i][1]+ly*1.5f),2,HEX(0x1a0a14));
  }
  /* grand oeil */
  int erx=27, ery=21;
  gfx_ellipse(cx,cy,erx+2,ery+2,HEX(0x1a0a14));
  gfx_ellipse(cx,cy,erx,ery,HEX(0xf2e8ee));
  gfx_ellipse(cx+3,cy+4,erx-4,ery-6,HEX(0xd8c4d0));
  for(int i=0;i<8;i++){ float a=i*TAU/8+0.3f; gfx_line((int)(cx+cosf(a)*erx*0.95f),(int)(cy+sinf(a)*ery*0.95f),(int)(cx+cosf(a+0.2f)*erx*0.6f),(int)(cy+sinf(a+0.2f)*ery*0.6f),HEX(0xc83a5a)); }
  int ix=(int)(cx+lx*11), iy=(int)(cy+ly*7);
  uint32_t ic = phase ? HEX(0xff6a1a) : HEX(0xc02848);
  gfx_disc(ix,iy,12,col_scale(ic,0.6f)); EMIT(gfx_disc(ix,iy,11,ic)); EMIT(gfx_ring_a(ix,iy,7,col_scale(ic,1.4f),200,false));
  EMIT(gfx_ellipse(ix,iy,2+(int)(phase?1:0),9,HEX(0x0a0408)));
  gfx_disc(ix-5,iy-5,2,HEX(0xffffff)); gfx_pset(ix+4,iy+4,HEX(0xffd0e0));
  /* paupieres */
  if(anim>0.01f){
    int lid=(int)(anim*(ery+2));
    for(int y=cy-ery-2;y<=cy+ery+2;y++){
      bool top = y < cy-ery-2+lid, bot = y > cy+ery+2-lid;
      if(!top && !bot) continue;
      float fy=(float)(y-cy)/(ery+2.5f); float v=1-fy*fy; if(v<0) continue;
      int hw=(int)((erx+2)*sqrtf(v)+0.5f);
      gfx_hline(cx-hw,cx+hw,y,top?m1:m2);
    }
    if(lid>2) { gfx_hline(cx-erx,cx+erx,cy-ery-2+lid,m3); gfx_hline(cx-erx,cx+erx,cy+ery+2-lid,m3); }
  }
}

/* ---------------- v4 : gardiens des Profondeurs ---------------- */
static void ellipse_bands(int cx,int cy,int rx,int ry,int y0,int step,int th,uint32_t c){
  for(int y=y0;y<=cy+ry;y+=step) for(int k=0;k<th;k++){ int yy=y+k; float fy=(float)(yy-cy)/(ry+0.5f); float v=1-fy*fy; if(v<=0) continue; int hw=(int)(rx*sqrtf(v)); gfx_hline(cx-hw+1,cx+hw-1,yy,c); }
}
static void boss_givre(float t,float anim,float lx,float ly,int phase){
  int cx=64;
  uint32_t ice=HEX(0x9ae8ff), iced=HEX(0x3b7fdc), icel=HEX(0xe8faff), deep=HEX(0x1a3a7a), skin=HEX(0xbfe6ff);
  if(phase){ ice=HEX(0x7ac8ff); iced=HEX(0x2a4ab8); deep=HEX(0x14205a); skin=HEX(0xa8d4ff); }
  /* eclats en orbite (derriere) */
  int n=phase?8:5;
  for(int i=0;i<n;i++){ float a=t*1.2f+i*TAU/n; if(sinf(a)>0) continue; float x=cx+cosf(a)*46, y=60+sinf(a)*14; gfx_tri(x-3,y,x+3,y,x,y-8,iced); gfx_tri(x-3,y,x+3,y,x,y+4,deep); }
  /* robe : facettes de glace */
  gfx_tri(cx-42,102,cx+42,102,cx,40,deep);
  gfx_tri(cx-38,100,cx+38,100,cx,44,iced);
  for(int i=0;i<7;i++){ float x=cx-30+i*10; gfx_tri(x-6,100,x+6,100,x+(i-3)*1.5f,(float)(56+abs(i-3)*5),(i&1)?ice:iced); }
  gfx_tri(cx-11,98,cx+11,98,cx,52,icel);
  for(int i=0;i<10;i++){ float x=cx-38+i*8.4f; gfx_tri(x-4,99,x+4,99,x,(float)(106+((i*5)%4)),i&1?iced:deep); }
  /* buste */
  gfx_ellipse(cx,46,12,10,deep); gfx_ellipse(cx,45,10,8,iced); gfx_ellipse(cx-3,42,4,3,ice);
  /* bras et sceptre */
  float up=anim*10;
  gfx_thick_line(cx-9,40,cx-22,(float)(54-up),2.0f,skin);
  gfx_thick_line(cx+9,40,cx+20,52,2.0f,skin);
  gfx_thick_line(cx-23,(float)(66-up),cx-23,(float)(18-up),1.3f,icel);
  for(int k=0;k<4;k++){ float a=k*PI*0.5f+t*2; gfx_thick_line(cx-23,(float)(16-up),cx-23+cosf(a)*6,(float)(16-up)+sinf(a)*6,0.8f,ice); }
  EMIT(gfx_disc(cx-23,(int)(16-up),4,ice)); EMIT(gfx_disc(cx-23,(int)(16-up),2,HEX(0xffffff)));
  /* tete et chevelure */
  for(int s=-1;s<=1;s+=2) for(int k=0;k<3;k++) gfx_thick_line(cx+s*6,24,cx+s*(9+k*3),(float)(42+k*6)+sinf(t*2+k+s)*2,1.2f,HEX(0xf4fbff));
  gfx_disc(cx,28,9,skin); gfx_disc(cx-1,27,8,HEX(0xd8f0ff));
  uint32_t ec=phase?HEX(0xffffff):HEX(0x63d6f2);
  gfx_rect(cx-6,27,4,3,HEX(0x0a1430)); gfx_rect(cx+2,27,4,3,HEX(0x0a1430));
  EMIT(gfx_rect((int)(cx-5+lx),(int)(28+ly),2,1,ec)); EMIT(gfx_rect((int)(cx+3+lx),(int)(28+ly),2,1,ec));
  /* couronne de cristaux */
  for(int i=0;i<5;i++){ float x=cx-10+i*5; float h=(i==2)?18:((i==1||i==3)?13:9); gfx_tri(x-3,21,x+3,21,x,21-h,iced); gfx_tri(x-2,21,x+2,21,x,23-h,icel); }
  EMIT(gfx_pset(cx,4,HEX(0xffffff)));
  /* eclats en orbite (devant) */
  for(int i=0;i<n;i++){ float a=t*1.2f+i*TAU/n; if(sinf(a)<=0) continue; float x=cx+cosf(a)*46, y=60+sinf(a)*14; gfx_tri(x-4,y,x+4,y,x,y-9,ice); gfx_tri(x-4,y,x+4,y,x,y+5,iced); EMIT(gfx_pset((int)x,(int)y-3,icel)); }
}
static void boss_leviathan(float t,float anim,float lx,float ly,int phase){
  int cx=64, cy=58;
  uint32_t sc=HEX(0x2a8a7a), scd=HEX(0x14504a), scl=HEX(0x5ad0b8), belly=HEX(0xc8e8a0);
  if(phase){ sc=HEX(0x2a6a9a); scd=HEX(0x123a5a); scl=HEX(0x5ab0e0); belly=HEX(0xa8d8e0); }
  /* anneaux du corps de part et d'autre */
  for(int s=-1;s<=1;s+=2){
    for(int k=6;k>=0;k--){
      float x=cx+s*(16+k*7.2f), y=cy+4+sinf(t*2.2f+k*0.8f+s)*8-k*1.5f;
      int r=11-k;
      gfx_disc((int)x,(int)y,r+1,scd); gfx_disc((int)x,(int)y-1,r,sc); gfx_disc((int)x-1,(int)y-r/2,r/3+1,scl);
      if(!(k&1)) gfx_tri(x-3,y-r,x+3,y-r,x+s*2,y-r-6,scl);
    }
  }
  /* tete */
  gfx_ellipse(cx,cy,23,19,scd); gfx_ellipse(cx,cy-2,21,17,sc); gfx_ellipse(cx-6,cy-9,8,4,scl);
  gfx_ellipse(cx,cy+8,14,8,belly);
  for(int i=0;i<5;i++){ float x=cx-12+i*6; gfx_tri(x-3,cy-15,x+3,cy-15,x,(float)(cy-27-(i==2?6:0)),scl); gfx_tri(x-1,cy-15,x+1,cy-15,x,(float)(cy-24-(i==2?6:0)),HEX(0xe8fff4)); }
  /* machoire */
  int jaw=(int)(anim*8);
  if(jaw>1) gfx_rect(cx-11,cy+9,22,jaw+4,HEX(0x0a1a18));
  gfx_ellipse(cx,cy+14+jaw,14,5,scd); gfx_ellipse(cx,cy+13+jaw,12,3,sc);
  for(int i=0;i<6;i++){ float x=(float)(cx-10+i*4); gfx_tri(x,(float)cy+9,x+2,(float)cy+9,x+1,(float)cy+13,HEX(0xf6f2ea)); gfx_tri(x,(float)(cy+12+jaw),x+2,(float)(cy+12+jaw),x+1,(float)(cy+9+jaw),HEX(0xf6f2ea)); }
  /* yeux */
  uint32_t ec=phase?HEX(0xff6a3a):HEX(0xeaff6a);
  for(int s=-1;s<=1;s+=2){ gfx_disc(cx+s*10,cy-4,4,HEX(0x0a1a18)); EMIT(gfx_disc((int)(cx+s*10+lx*1.5f),(int)(cy-4+ly),2,ec)); }
  /* barbillons */
  for(int s=-1;s<=1;s+=2) gfx_thick_line(cx+s*8,cy+12,cx+s*28,(float)(cy+20)+sinf(t*3+s)*4,0.8f,scl);
}
static void boss_ruche(float t,float anim,float lx,float ly,int phase){
  int cx=64;
  uint32_t am=HEX(0xffb030), amd=HEX(0xa86010), aml=HEX(0xffe080), ch=HEX(0x3a2410), chl=HEX(0x6a4420);
  if(phase){ am=HEX(0xff7a20); amd=HEX(0x903010); }
  float p=1.0f+sinf(t*3)*0.05f;
  /* ailes */
  for(int s=-1;s<=1;s+=2){ int fl=(int)(sinf(t*30)*3);
    gfx_ellipse(cx+s*32,30+fl,24,10,HEX(0xa8b8c8)); gfx_ellipse(cx+s*32,30+fl,22,8,HEX(0xe8f4ff));
    gfx_line(cx+s*12,34,cx+s*52,26+fl,HEX(0xa8b8c8)); gfx_line(cx+s*16,36,cx+s*44,36+fl,HEX(0xc8d8e8)); }
  /* abdomen raye (sac a oeufs) */
  int ar=(int)(27*p), ary=(int)(25*p);
  gfx_ellipse(cx,76,ar+2,ary+2,ch); gfx_ellipse(cx,76,ar,ary,am); gfx_ellipse(cx-8,68,ar/3,ary/4,aml);
  ellipse_bands(cx,76,ar,ary,60,10,3,chl);
  EMIT(gfx_ellipse(cx+8,84,5,4,aml)); EMIT(gfx_ellipse(cx-10,90,3,3,aml));
  gfx_tri(cx-5,99,cx+5,99,cx,110,ch);
  /* thorax et pattes */
  for(int s=-1;s<=1;s+=2) for(int k=0;k<3;k++) gfx_thick_line(cx+s*12,(float)(44+k*4),cx+s*(28+k*2),(float)(56+k*6)+sinf(t*4+k)*2,1.0f,ch);
  gfx_ellipse(cx,42,16,12,ch); gfx_ellipse(cx,41,14,10,chl); gfx_ellipse(cx-4,38,5,3,HEX(0x8a5a30));
  /* tete */
  gfx_ellipse(cx,24,13,10,ch); gfx_ellipse(cx,23,11,8,chl);
  uint32_t ec=phase?HEX(0xff3b3b):HEX(0xffd36a);
  for(int s=-1;s<=1;s+=2){ gfx_ellipse(cx+s*7,23,5,6,HEX(0x1a0a04)); EMIT(gfx_ellipse((int)(cx+s*7+lx),(int)(23+ly),3,4,ec)); EMIT(gfx_pset(cx+s*7-1,21,HEX(0xffffff))); }
  float m=anim*4;
  gfx_thick_line(cx-4,31,cx-7-m,38,1.2f,amd); gfx_thick_line(cx+4,31,cx+7+m,38,1.2f,amd);
  /* couronne */
  for(int i=0;i<3;i++){ float x=(float)(cx-6+i*6); gfx_tri(x-3,14,x+3,14,x,(float)(5-(i==1?3:0)),HEX(0xf2c23a)); }
  gfx_rect(cx-9,13,18,3,HEX(0xc99a2e)); EMIT(gfx_pset(cx,9,HEX(0xff4a6a)));
}
static void boss_neant(float t,float anim,float lx,float ly,int phase){
  int cx=64, cy=56;
  float p=1.0f+0.06f*sinf(t*4.5f)+0.04f*sinf(t*9);
  uint32_t body=phase?HEX(0xe8eaf4):HEX(0x0c0a12), body2=phase?HEX(0xb8bccc):HEX(0x221e30), crack=phase?HEX(0x2a1440):HEX(0xe0f0ff);
  /* anneaux */
  for(int k=0;k<3;k++){ float a=t*(0.6f+k*0.3f)*((k&1)?-1:1); int r=36+k*6;
    for(int i=0;i<28;i++){ if(i%4==0) continue; float an=a+i*TAU/28; uint32_t c=k==1?HEX(0x8a7aff):HEX(0x5a5a7a); int x=cx+(int)(cosf(an)*r), y=cy+(int)(sinf(an)*r*0.8f); gfx_pset(x,y,c); if(k==1) EMIT(gfx_pset(x,y,c)); } }
  /* coeur */
  int r=(int)(26*p);
  gfx_disc(cx,cy,r+2,phase?HEX(0x6a6a80):HEX(0x3a3450)); gfx_disc(cx,cy,r,body); gfx_disc(cx-4,cy-5,r-9,body2);
  /* fissures lumineuses */
  for(int i=0;i<7;i++){ float a=i*TAU/7+0.3f+sinf(t*0.5f+i)*0.12f; float x0=cx+cosf(a)*7, y0=cy+sinf(a)*7; float x1=cx+cosf(a+0.25f)*r*0.95f, y1=cy+sinf(a+0.25f)*r*0.95f;
    float xm=(x0+x1)*0.5f+sinf(t+i)*2, ym=(y0+y1)*0.5f+cosf(t*1.3f+i)*2;
    EMIT(gfx_line((int)x0,(int)y0,(int)xm,(int)ym,crack)); EMIT(gfx_line((int)xm,(int)ym,(int)x1,(int)y1,crack)); }
  /* noyau */
  int ix=(int)(cx+lx*6), iy=(int)(cy+ly*5);
  EMIT(gfx_disc(ix,iy,6+(int)(anim*3),crack)); gfx_disc(ix,iy,3,phase?HEX(0xffffff):HEX(0x000000)); EMIT(gfx_pset(ix,iy,phase?HEX(0x000000):HEX(0xffffff)));
  /* fragments en orbite */
  int n=phase?6:4;
  for(int i=0;i<n;i++){ float a=t*1.4f+i*TAU/n; float x=cx+cosf(a)*48, y=cy+sinf(a)*30; gfx_tri(x-5,y,x+5,y,x,y-9,body2); gfx_tri(x-5,y,x+5,y,x,y+6,body); EMIT(gfx_pset((int)x,(int)y-2,crack)); }
}

/* ---------------- v7 : les gardiens du Royaume oublie ---------------- */
static void boss_arbre(float t,float anim,float lx,float ly,int phase){
  int cx=64;
  uint32_t bark=HEX(0x5a3a22), barkd=HEX(0x2e1c10), barkl=HEX(0x8a5a34), leaf=HEX(0x2a6a4a), leafd=HEX(0x163a2a), leafl=HEX(0x4a9a5a), moon=HEX(0xf4f0c8);
  if(phase){ leaf=HEX(0x7a4a22); leafd=HEX(0x3a2214); leafl=HEX(0xc0702a); moon=HEX(0xffc890); }
  /* racines au sol */
  for(int s=-1;s<=1;s+=2) for(int k=0;k<3;k++){ float x0=cx+s*(8+k*5), x1=cx+s*(22+k*14); gfx_thick_line(x0,96,x1,104+k*2+sinf(t*1.5f+k)*1.5f,2.2f-k*0.5f,barkd); }
  /* tronc */
  gfx_tri(cx-18,102,cx+18,102,cx,40,barkd); gfx_rect(cx-14,52,28,48,bark); gfx_tri(cx-14,53,cx+14,53,cx,30,bark);
  for(int i=0;i<5;i++) gfx_thick_line(cx-10+i*5,56,cx-12+i*5+sinf(i*2.0f)*2,98,0.6f,barkd);
  gfx_rect(cx-12,54,3,40,barkl);
  /* branches-bras */
  float up=anim*8;
  for(int s=-1;s<=1;s+=2){ gfx_thick_line(cx+s*12,58,cx+s*32,44-up,3.0f,bark); gfx_thick_line(cx+s*32,44-up,cx+s*46,30-up+sinf(t*2+s)*2,2.0f,bark); gfx_thick_line(cx+s*32,44-up,cx+s*44,54-up,1.4f,bark); }
  /* feuillage en grappes */
  static const float LV[11][3]={{0,18,22},{-24,22,16},{24,22,16},{-36,34,12},{36,34,12},{-14,8,14},{14,8,14},{0,-1,12},{-44,24,9},{44,24,9},{0,30,14}};
  for(int i=0;i<11;i++){ float x=cx+LV[i][0]+sinf(t*0.9f+i)*1.2f, y=LV[i][1]+cosf(t*0.8f+i*1.3f)*1.0f; int r=(int)LV[i][2];
    gfx_disc((int)x,(int)y+2,r,leafd); gfx_disc((int)x-1,(int)y,r-1,leaf); gfx_disc((int)x-r/3,(int)y-r/3,r/3,leafl); }
  /* la lune dans le tronc */
  gfx_disc(cx,64,11,barkd); EMIT(gfx_disc(cx,64,9,moon)); gfx_disc(cx+3,62,3,col_scale(moon,0.85f));
  uint32_t ec=phase?HEX(0xff3a2a):HEX(0x1a2a1a);
  if(phase){ EMIT(gfx_rect((int)(cx-6+lx),(int)(61+ly),4,2,ec)); EMIT(gfx_rect((int)(cx+2+lx),(int)(61+ly),4,2,ec)); }
  else { gfx_rect((int)(cx-6+lx),(int)(61+ly),4,2,ec); gfx_rect((int)(cx+2+lx),(int)(61+ly),4,2,ec); }
  gfx_rect(cx-3,68,6,1+(int)(anim*3),HEX(0x2a1a10));
  /* lucioles en orbite */
  for(int i=0;i<8;i++){ float a=t*1.1f+i*TAU/8; float x=cx+cosf(a)*(50+5*sinf(t*2+i)), y=40+sinf(a)*26; EMIT(gfx_pset((int)x,(int)y,HEX(0xeaff6a))); EMIT(gfx_pset((int)x+1,(int)y,HEX(0xc8f040))); }
}
static void boss_crapaud(float t,float anim,float lx,float ly,int phase){
  int cx=64, cy=66;
  uint32_t sk=HEX(0x6a8a3a), skd=HEX(0x3a5020), skl=HEX(0x9ab858), belly=HEX(0xd8d8a0), wart=HEX(0x4a6a2a);
  if(phase){ sk=HEX(0x8a7a3a); skd=HEX(0x4a3a18); skl=HEX(0xc0a858); }
  float br=1.0f+0.05f*sinf(t*2.0f);
  for(int s=-1;s<=1;s+=2){ gfx_ellipse(cx+s*36,cy+22,16,10,skd); gfx_ellipse(cx+s*35,cy+21,14,8,sk); for(int k=0;k<3;k++) gfx_thick_line(cx+s*(40+k*4),cy+28,cx+s*(46+k*5),cy+34,1.2f,skd); }
  int rx=(int)(40*br), ry=(int)(26*br);
  gfx_ellipse(cx,cy,rx+2,ry+2,skd); gfx_ellipse(cx,cy-1,rx,ry,sk); gfx_ellipse(cx-12,cy-12,rx/3,ry/4,skl);
  gfx_ellipse(cx,cy+12,(int)(26*br),(int)(12*br),belly);
  static const int W[8][2]={{-26,-6},{-18,-14},{22,-10},{30,-2},{-32,4},{14,-18},{-6,-20},{34,8}};
  for(int i=0;i<8;i++){ gfx_disc(cx+W[i][0],cy+W[i][1],2,wart); gfx_pset(cx+W[i][0]-1,cy+W[i][1]-1,skl); }
  for(int s=-1;s<=1;s+=2){ gfx_thick_line(cx+s*20,cy+14,cx+s*26,cy+30,3.0f,skd); for(int k=0;k<3;k++) gfx_thick_line(cx+s*26,cy+30,cx+s*(22+k*5),cy+34,1.0f,skd); }
  int open=(int)(anim*10);
  gfx_thick_line(cx-26,cy+2,cx+26,cy+2,1.0f,HEX(0x1a2410));
  if(open>1){ gfx_ellipse(cx,cy+4,24,open,HEX(0x3a0a14)); gfx_ellipse(cx,cy+4+open/2,10,open/2+1,HEX(0xe05a7a)); }
  for(int s=-1;s<=1;s+=2){ int ex=cx+s*22, ey=cy-24; gfx_disc(ex,ey,10,skd); gfx_disc(ex,ey-1,9,sk);
    uint32_t ic=phase?HEX(0xff6a2a):HEX(0xf9dc5c); EMIT(gfx_disc(ex,ey,6,ic)); gfx_rect((int)(ex-1+lx*2),(int)(ey-4+ly),2,8,HEX(0x0a0a04)); gfx_pset(ex-3,ey-3,HEX(0xffffff)); }
  /* couronne */
  for(int i=0;i<4;i++){ float x=(float)(cx-9+i*6); float h=(i==1||i==2)?11:8; gfx_tri(x-3,cy-26,x+3,cy-26,x,cy-26-h,HEX(0xf2c23a)); }
  gfx_rect(cx-12,cy-28,24,4,HEX(0xc99a2e)); EMIT(gfx_pset(cx,cy-27,HEX(0x5af0d0))); EMIT(gfx_pset(cx-6,cy-27,HEX(0xff4a6a))); EMIT(gfx_pset(cx+6,cy-27,HEX(0xff4a6a)));
}
static void boss_prisme(float t,float anim,float lx,float ly,int phase){
  int cx=64;
  uint32_t c1=HEX(0xff8ad8), c2=HEX(0x9af8ff), c3=HEX(0xc08aff), dk=HEX(0x3a1a4a), lt=HEX(0xffffff), rock=HEX(0x4a4458), rockd=HEX(0x2a2632);
  if(phase){ c1=HEX(0xff5a8a); c3=HEX(0xd0508a); }
  for(int s=-1;s<=1;s+=2){ gfx_rect(cx+s*14-6,82,12,22,rockd); gfx_rect(cx+s*14-5,82,10,20,rock); gfx_tri(cx+s*14-8,104,cx+s*14+8,104,cx+s*14,96,rockd); }
  float sw=anim*6;
  for(int s=-1;s<=1;s+=2){
    gfx_thick_line(cx+s*24,46,cx+s*40,66-sw,5.0f,rockd); gfx_thick_line(cx+s*24,46,cx+s*40,66-sw,4.0f,rock);
    float hx=cx+s*42, hy=74-sw;
    for(int k=0;k<4;k++){ float a=-PI*0.5f+s*(0.2f+k*0.35f); float x1=hx+cosf(a)*14, y1=hy+sinf(a)*14; gfx_tri(hx-3,hy,hx+3,hy,x1,y1,k&1?c2:c1); gfx_tri(hx-1,hy,hx+1,hy,x1,y1,lt); } }
  gfx_tri(cx-28,40,cx+28,40,cx,92,dk);
  gfx_tri(cx-26,42,cx,42,cx-6,88,c3); gfx_tri(cx,42,cx+26,42,cx+6,88,c1);
  gfx_tri(cx-14,44,cx+14,44,cx,84,c2);
  gfx_tri(cx-26,42,cx-34,30,cx-12,42,c1); gfx_tri(cx+26,42,cx+34,30,cx+12,42,c3);
  float p=1.0f+0.15f*sinf(t*5);
  uint32_t core=phase?HEX(0xff4a3a):HEX(0xffffff);
  EMIT(gfx_disc(cx,60,(int)(7*p),phase?HEX(0xff8a6a):HEX(0xd8f8ff))); EMIT(gfx_disc(cx,60,(int)(4*p),core));
  static const uint32_t RB[6]={HEX(0xff6a6a),HEX(0xffd84a),HEX(0x7af07a),HEX(0x5ab8ff),HEX(0xc08aff),HEX(0xff8ad8)};
  for(int k=0;k<6;k++){ float a=t*0.8f+k*TAU/6; EMIT(gfx_line(cx,60,(int)(cx+cosf(a)*14),(int)(60+sinf(a)*14),RB[k])); }
  gfx_tri(cx-12,40,cx+12,40,cx,8,dk); gfx_tri(cx-10,39,cx,39,cx-2,10,c2); gfx_tri(cx,39,cx+10,39,cx+2,10,c1);
  uint32_t ec=phase?HEX(0xff3b3b):HEX(0xffffff);
  EMIT(gfx_rect((int)(cx-6+lx*2),(int)(28+ly),4,2,ec)); EMIT(gfx_rect((int)(cx+2+lx*2),(int)(28+ly),4,2,ec));
  int n=phase?6:4; for(int i=0;i<n;i++){ float a=t*1.3f+i*TAU/n; float x=cx+cosf(a)*52, y=52+sinf(a)*20; gfx_tri(x-4,y,x+4,y,x,y-9,i&1?c1:c2); gfx_tri(x-4,y,x+4,y,x,y+5,c3); EMIT(gfx_pset((int)x,(int)y-3,lt)); }
}
static void boss_soleil(float t,float anim,float lx,float ly,int phase){
  int cx=64, cy=56;
  uint32_t f1=phase?HEX(0xff3a1a):HEX(0xff8a2a), f2=phase?HEX(0xffb040):HEX(0xffd84a);
  int nf=phase?18:14;
  for(int i=0;i<nf;i++){ float a=t*0.5f+i*TAU/nf; float L=(phase?44.0f:40.0f)+6*sinf(t*4+i*1.7f);
    float x0=cx+cosf(a-0.12f)*28, y0=cy+sinf(a-0.12f)*28, x1=cx+cosf(a+0.12f)*28, y1=cy+sinf(a+0.12f)*28, x2=cx+cosf(a)*L, y2=cy+sinf(a)*L;
    EMIT(gfx_tri(x0,y0,x1,y1,x2,y2,(i&1)?f1:f2)); }
  EMIT(gfx_ring(cx,cy,31,f2)); EMIT(gfx_ring(cx,cy,30,f1));
  gfx_disc(cx,cy,29,HEX(0x0a0608)); gfx_disc(cx-3,cy-4,24,HEX(0x1a1018));
  for(int i=0;i<8;i++){ float a=-t*0.4f+i*TAU/8; int x=(int)(cx+cosf(a)*21), y=(int)(cy+sinf(a)*21); EMIT(gfx_pset(x,y,HEX(0xffc040))); EMIT(gfx_pset(x+1,y,HEX(0xc08020))); }
  int ex=(int)(cx+lx*6), ey=(int)(cy+ly*5); float op=0.6f+0.4f*anim;
  gfx_ellipse(cx,cy,16,(int)(9*op)+1,HEX(0x2a1a0a)); EMIT(gfx_ellipse(cx,cy,14,(int)(7*op)+1,phase?HEX(0xff6a2a):HEX(0xffd890)));
  gfx_disc(ex,ey,4,HEX(0x0a0604)); EMIT(gfx_pset(ex-1,ey-1,HEX(0xffffff)));
}

void art_draw_boss(int type,float cx,float cy,float t,float flash,float anim,float lx,float ly,int phase){
  if(!boss_canvas.px) boss_canvas=spr_alloc(128,112);
  if(!boss_emc.px) boss_emc=spr_alloc(128,112);
  spr_clear(&boss_canvas);
  boss_em_on=g_gpu; if(boss_em_on) spr_clear(&boss_emc);
  Target tg=spr_target(&boss_canvas); Target *old=gT; gfx_target(&tg);
  switch(type){
    case BOSS_ROI: boss_roi(t,anim,lx,ly,phase); break;
    case BOSS_MERE: boss_mere(t,anim,lx,ly,phase); break;
    case BOSS_COLOSSE: boss_colosse(t,anim,lx,ly,phase); break;
    case BOSS_GIVRE: boss_givre(t,anim,lx,ly,phase); break;
    case BOSS_LEVIATHAN: boss_leviathan(t,anim,lx,ly,phase); break;
    case BOSS_RUCHE: boss_ruche(t,anim,lx,ly,phase); break;
    case BOSS_NEANT: boss_neant(t,anim,lx,ly,phase); break;
    case BOSS_ARBRE: boss_arbre(t,anim,lx,ly,phase); break;
    case BOSS_CRAPAUD: boss_crapaud(t,anim,lx,ly,phase); break;
    case BOSS_PRISME: boss_prisme(t,anim,lx,ly,phase); break;
    case BOSS_SOLEIL: boss_soleil(t,anim,lx,ly,phase); break;
    default: boss_oeil(t,anim,lx,ly,phase); break;
  }
  gfx_target(old);
  if(boss_em_on){ /* ce qui a ete recouvert ensuite (paupieres...) n'emet plus */
    for(int i=0;i<boss_emc.w*boss_emc.h;i++) if(boss_emc.px[i] && ((boss_emc.px[i]^boss_canvas.px[i])&0xFFFFFFu)) boss_emc.px[i]=0; }
  spr_shade(&boss_canvas,0.45f);
  spr_outline_tinted(&boss_canvas);
  if(g_gpu) spr_make_normals(&boss_canvas,1.0f);
  DrawOpt o=dopt(); if(flash>0){ o.flash=HEX(0xffffff); o.flash_a=(int)(flash*230); }
  o.sx=o.sy=g_boss_draw_scale;
  draw_spr_c(&boss_canvas,cx,cy,&o);
  if(boss_em_on && gT==&g_scr){
    int ly=layer_current(); layer(LY_EMIS);
    DrawOpt e=dopt(); e.sx=e.sy=g_boss_draw_scale; draw_spr_c(&boss_emc,cx,cy,&e);
    layer(ly);
  }
  boss_em_on=false;
}

/* ---------------- batiments du campement ---------------- */
static void roof(int x0,int x1,int ytop,int ybot,uint32_t c,uint32_t cd){
  int mid=(x0+x1)/2;
  gfx_tri((float)x0-2,(float)ybot,(float)x1+2,(float)ybot,(float)mid,(float)ytop,cd);
  gfx_tri((float)x0,(float)ybot-1,(float)x1,(float)ybot-1,(float)mid,(float)ytop+2,c);
  for(int y=ytop+5;y<ybot-1;y+=3){ float f=(float)(y-ytop)/(ybot-ytop); int hw=(int)(f*(x1-x0)/2); gfx_hline(mid-hw+1,mid+hw-1,y,cd); }
}
static void bricks(int x,int y,int w,int h,uint32_t c,uint32_t cd){
  gfx_rect(x,y,w,h,c);
  for(int j=0;j<h;j+=4){ gfx_hline(x,x+w-1,y+j,cd); for(int i=((j/4)&1)*3;i<w;i+=6) gfx_vline(x+i,y+j,y+j+3<y+h-1?y+j+3:y+h-1,cd); }
}
static void planks(int x,int y,int w,int h,uint32_t c,uint32_t cd){
  gfx_rect(x,y,w,h,c); for(int i=0;i<w;i+=4) gfx_vline(x+i,y,y+h-1,cd);
}
static void window(int x,int y,uint32_t lit){ gfx_rect(x,y,5,6,HEX(0x2a1c18)); gfx_rect(x+1,y+1,3,4,lit); gfx_vline(x+2,y+1,y+4,HEX(0x2a1c18)); }

static void draw_building(int b){
  Sprite *s=&SPR_BLD[b]; *s=spr_alloc(52,52);
  Target t=spr_target(s); gfx_target(&t);
  uint32_t lit=HEX(0xffd36a);
  switch(b){
    case BLD_FORGE:
      bricks(6,24,38,24,HEX(0x7a7486),HEX(0x4a4554));
      gfx_rect(34,6,8,18,HEX(0x4a4554)); gfx_rect(35,6,6,17,HEX(0x7a7486));
      roof(4,46,12,26,HEX(0x8f2233),HEX(0x5a1420));
      gfx_rect(18,34,12,14,HEX(0x2a1810)); gfx_rect(20,38,8,10,HEX(0xff8a2a)); gfx_rect(21,40,6,8,HEX(0xffd36a));
      gfx_rect(8,40,8,3,HEX(0x5a5466)); gfx_rect(10,43,4,3,HEX(0x5a5466)); gfx_rect(7,46,10,2,HEX(0x5a5466));
      break;
    case BLD_INFIRMERIE:
      gfx_rect(6,22,40,26,HEX(0xe8e0d0)); gfx_rect(6,22,40,2,HEX(0xc8c0b0));
      roof(3,49,6,24,HEX(0x5a7ac8),HEX(0x34508a));
      gfx_rect(20,34,10,14,HEX(0x6a4a2a));
      gfx_rect(23,25,4,10,HEX(0xe0453a)); gfx_rect(20,28,10,4,HEX(0xe0453a));
      window(9,30,lit); window(37,30,lit);
      break;
    case BLD_ATELIER:
      planks(6,24,40,24,HEX(0x9a6438),HEX(0x5c3a24));
      roof(3,49,10,26,HEX(0x6a6a3a),HEX(0x4a4a24));
      gfx_rect(18,34,14,14,HEX(0x3a2416));
      gfx_ring(39,33,4,HEX(0xc9c4d0)); gfx_disc(39,33,1,HEX(0xc9c4d0));
      for(int i=0;i<8;i++){ float a=i*TAU/8; gfx_pset(39+(int)roundf(cosf(a)*5),33+(int)roundf(sinf(a)*5),HEX(0xc9c4d0)); }
      window(8,30,lit);
      break;
    case BLD_BIBLIO:
      bricks(10,14,32,34,HEX(0x8a7a9a),HEX(0x5a4a6a));
      roof(8,44,2,16,HEX(0x2a3b80),HEX(0x1a2450));
      gfx_disc(26,24,5,HEX(0x2a1c18)); gfx_disc(26,24,4,lit); gfx_hline(22,30,24,HEX(0x2a1c18)); gfx_vline(26,20,28,HEX(0x2a1c18));
      gfx_rect(20,36,12,12,HEX(0x4a2a1a));
      for(int i=0;i<4;i++) gfx_rect(13,34+i*3,5,2,i&1?HEX(0xe0453a):HEX(0x3fa34d));
      break;
    case BLD_TRESOR:
      bricks(6,22,40,26,HEX(0x948ea0),HEX(0x5a5466));
      roof(3,49,8,24,HEX(0xc99a2e),HEX(0x8a6a1a));
      gfx_rect(19,32,14,16,HEX(0x3a2a1a)); gfx_rect(19,32,14,2,HEX(0xc99a2e));
      gfx_disc(26,16,4,HEX(0xf9dc5c)); gfx_vline(26,14,18,HEX(0xc99a2e));
      window(9,30,lit); window(38,30,lit);
      break;
    case BLD_PUITS:
      gfx_vline(12,10,40,HEX(0x5c3a24)); gfx_vline(13,10,40,HEX(0x9a6438));
      gfx_vline(38,10,40,HEX(0x5c3a24)); gfx_vline(39,10,40,HEX(0x9a6438));
      roof(8,44,4,14,HEX(0x8f2233),HEX(0x5a1420));
      gfx_hline(14,37,20,HEX(0x5c3a24)); gfx_vline(26,20,28,HEX(0xc9c4d0)); gfx_rect(24,28,5,4,HEX(0x8a6a4a));
      gfx_ellipse(26,40,16,7,HEX(0x4a4554)); gfx_ellipse(26,38,15,6,HEX(0x7a7486)); gfx_ellipse(26,37,11,3,HEX(0x2a3b80)); gfx_ellipse(26,37,7,2,HEX(0x63d6f2));
      break;
    case BLD_SANCTUAIRE:
      gfx_rect(6,44,40,4,HEX(0x948ea0)); gfx_rect(8,41,36,3,HEX(0xc9c4d0));
      for(int i=0;i<4;i++){ gfx_rect(10+i*9,20,4,21,HEX(0xc9c4d0)); gfx_vline(13+i*9,20,40,HEX(0x8e8898)); }
      gfx_tri(4,20,48,20,26,6,HEX(0x8e8898)); gfx_tri(7,19,45,19,26,8,HEX(0xe8e0f0));
      gfx_disc(26,32,4,HEX(0xa35fe0)); gfx_disc(25,31,2,HEX(0xd2b0f5));
      break;
    case BLD_LABO:
      bricks(8,20,36,28,HEX(0x5a6a6a),HEX(0x3a4a4a));
      gfx_rect(36,3,6,16,HEX(0x3a4a4a)); gfx_rect(37,3,4,15,HEX(0x6a7a7a));
      roof(5,47,6,22,HEX(0x2a6a5a),HEX(0x1a4a3a));
      gfx_rect(19,34,14,14,HEX(0x2a1c18)); gfx_rect(20,35,12,13,HEX(0x14241c));
      gfx_rect(22,40,3,7,HEX(0x9af05a)); gfx_rect(22,39,3,1,HEX(0xd8d0e0)); gfx_rect(27,42,3,5,HEX(0xc08aff)); gfx_rect(27,41,3,1,HEX(0xd8d0e0));
      window(10,28,HEX(0x9af05a)); window(37,28,HEX(0xc08aff));
      gfx_disc(26,14,3,HEX(0x9af05a)); gfx_pset(25,13,HEX(0xe0ffd0));
      break;
    case BLD_AUTEL:
      gfx_rect(8,42,36,6,HEX(0x5a5466)); gfx_rect(10,40,32,3,HEX(0xa8a2b4));
      for(int i=0;i<2;i++){ int x=i?39:9; gfx_rect(x,16,5,24,HEX(0x5a5466)); gfx_rect(x+1,16,3,24,HEX(0x8e8898)); gfx_rect(x-1,13,7,3,HEX(0xa8a2b4)); }
      gfx_rect(16,26,20,14,HEX(0x4a4554)); gfx_rect(17,26,18,12,HEX(0x7a7486)); gfx_rect(16,24,20,3,HEX(0xa8a2b4));
      gfx_rect(18,31,16,2,HEX(0xc99a2e)); gfx_disc(26,35,2,HEX(0xd8203a)); gfx_pset(25,34,HEX(0xffa0a0));
      gfx_tri(19,24,33,24,26,6,HEX(0xc0300a)); gfx_tri(20,24,32,24,26,9,HEX(0xff8a2a)); gfx_tri(22,24,30,24,26,13,HEX(0xffd36a)); gfx_pset(26,19,HEX(0xffffff));
      for(int i=0;i<3;i++){ int x=12+i*12; gfx_pset(x,46,HEX(0xffd36a)); }
      break;
    case BLD_ARMURERIE:
      planks(6,24,40,24,HEX(0x6a4428),HEX(0x3a2416));
      roof(3,49,8,26,HEX(0x5a5466),HEX(0x3a3444));
      gfx_rect(19,34,14,14,HEX(0x2a1810)); gfx_rect(20,35,12,13,HEX(0x3a2416)); gfx_pset(30,41,HEX(0xc99a2e));
      gfx_line(8,28,16,42,HEX(0xd8dcea)); gfx_line(16,28,8,42,HEX(0xd8dcea)); gfx_rect(7,26,3,2,HEX(0xc99a2e)); gfx_rect(15,26,3,2,HEX(0xc99a2e));
      gfx_disc(39,36,6,HEX(0x5a1420)); gfx_disc(39,36,5,HEX(0x8f2233)); gfx_disc(39,36,3,HEX(0xe0453a)); gfx_disc(39,36,1,HEX(0xf9dc5c));
      gfx_rect(22,14,8,6,HEX(0x9a6438)); gfx_line(23,15,29,19,HEX(0xd8dcea)); gfx_line(23,19,29,15,HEX(0xd8dcea));
      window(36,27,HEX(0xffd36a));
      break;
    case BLD_TOUR:
    default:
      bricks(16,10,20,38,HEX(0x7a7486),HEX(0x4a4554));
      for(int i=0;i<4;i++) gfx_rect(14+i*6,5,4,6,HEX(0x7a7486));
      gfx_rect(14,9,24,3,HEX(0x4a4554));
      window(23,18,lit); window(23,30,lit);
      gfx_rect(22,40,8,8,HEX(0x3a2416));
      gfx_vline(26,0,5,HEX(0x5c3a24)); gfx_rect(27,0,6,4,HEX(0xe0453a));
      break;
  }
  gfx_target(&g_scr);
  spr_outline(s,HEX(0x120e18));
  spr_make_normals(s,0.8f);
  static const uint32_t GLOWC[]={HEX(0xffd36a),HEX(0xff8a2a),HEX(0x9af05a),HEX(0xc08aff),HEX(0xd2b0f5),HEX(0xe0ffd0)};
  spr_emis_colors(s,GLOWC,COUNTOF(GLOWC),0.9f);
}

static void draw_hut(int ch){
  Sprite *s=&SPR_HUT[ch]; *s=spr_alloc(30,28);
  /* tente aux couleurs du heros */
  Target t=spr_target(s); gfx_target(&t);
  uint32_t c=CHARS[ch].col; uint32_t cd=col_scale(c,0.6f);
  gfx_tri(2,25,28,25,15,4,cd); gfx_tri(4,24,26,24,15,6,c);
  gfx_tri(11,25,19,25,15,13,HEX(0x2a1810));
  gfx_vline(15,1,5,HEX(0x5c3a24)); gfx_rect(16,1,5,3,HEX(0xf9dc5c));
  gfx_target(&g_scr);
  spr_outline(s,HEX(0x120e18));
  spr_make_normals(s,0.8f);
}

void art_init(void){
  MKW(SP_SENTINELLE,A_SENTINELLE,false,NULL);
  MKW(SP_ALCHIMISTE,A_ALCHIMISTE,false,NULL);
  MKW(SP_JONGLEUR,A_JONGLEUR,false,NULL);
  MKW(SP_FORGERONNE,A_FORGERONNE,false,NULL);
  MKW(SP_MAGNETISEUR,A_MAGNETISEUR,false,NULL);
  MKW(SP_NECRO,A_NECRO,false,NULL);
  MKW(SP_HORLOGERE,A_HORLOGERE,false,NULL);
  MKW(SP_PYROMANE,A_PYROMANE,false,NULL);
  MKW(SP_DRUIDE,A_DRUIDE,false,NULL);
  MKW(SP_MARCHAND,A_MARCHAND,false,NULL);
  MKW(SP_ASTRONOME,A_ASTRONOME,false,NULL);
  MKW(SP_VOLEUSE,A_VOLEUSE,false,NULL);
  MKW(SP_CHASSERESSE,A_CHASSERESSE,false,NULL);
  MKW(SP_GEOMANCIEN,A_GEOMANCIEN,false,NULL);
  MK(SP_FOLLET1,A_FOLLET1,false,NULL);
  MK(SP_FOLLET2,A_FOLLET2,false,NULL);
  MKW(SP_MANTE,A_MANTE,false,NULL);
  MK(SP_CRACHEUR,A_CRACHEUR,false,NULL);
  MK(SP_SENTVIDE,A_SENTVIDE,false,NULL);
  MKW(SP_SQUELETTE,A_SQUELETTE,false,NULL);
  MK(SP_CHAUVE1,A_CHAUVE1,false,NULL);
  MK(SP_CHAUVE2,A_CHAUVE2,false,NULL);
  MK(SP_GELEE,A_GELEE,false,NULL);
  MK(SP_GELETTE,A_GELETTE,false,NULL);
  MK(SP_CRANE,A_CRANE,false,NULL);
  MK(SP_OSSUAIRE,A_OSSUAIRE,true,NULL);
  MKW(SP_CHAMPIGNON,A_CHAMPIGNON,false,NULL);
  MK(SP_ARAIGNEE,A_ARAIGNEE,false,NULL);
  MKW(SP_SCARABEE,A_SCARABEE,false,NULL);
  MK(SP_GELEE_TOX,A_GELEE,false,"qmQMev");
  MK(SP_GELETTE_TOX,A_GELETTE,false,"qmQMev");
  MK(SP_SPORANGE,A_SPORANGE,true,NULL);
  MKW(SP_DIABLOTIN,A_DIABLOTIN,false,NULL);
  MKW(SP_MAGMITE,A_MAGMITE,false,NULL);
  MKW(SP_ARMURE,A_ARMURE,false,NULL);
  MK(SP_CHAUVEF1,A_CHAUVE1,false,"PRpoxy");
  MK(SP_CHAUVEF2,A_CHAUVE2,false,"PRpoxy");
  MK(SP_GOLEM,A_GOLEM,true,NULL);
  MK(SP_OEIL,A_OEIL,false,NULL);
  MK(SP_SPECTRE,A_SPECTRE,false,NULL);
  MK(SP_CHEVALIER,A_CHEVALIER,false,NULL);
  MK(SP_OBELISQUE,A_OBELISQUE,false,NULL);
  MK(SP_HORREUR,A_HORREUR,true,NULL);
  SPR[SP_TENTACULE]=spr_alloc(4,4); SPR_RAW[SP_TENTACULE]=spr_alloc(4,4); SPR_LO[SP_TENTACULE]=SPR[SP_TENTACULE];
  /* v4 : les Profondeurs */
  MKW(SP_LOUP,A_LOUP,false,NULL);
  MK(SP_ESPRIT,A_ESPRIT,false,NULL);
  MK(SP_TORTUE,A_TORTUE,false,NULL);
  MK(SP_GELEE_GIVRE,A_GELEE,false,"qcQC");
  MK(SP_GELETTE_GIVRE,A_GELETTE,false,"qcQC");
  MK(SP_HARFANG1,A_HARFANG1,false,NULL);
  MK(SP_HARFANG2,A_HARFANG2,false,NULL);
  MK(SP_YETI,A_YETI,true,NULL);
  MKW(SP_NOYE,A_NOYE,false,NULL);
  MK(SP_MURENE,A_MURENE,false,NULL);
  MK(SP_CRABE,A_CRABE,false,NULL);
  MK(SP_MEDUSE,A_MEDUSE,false,NULL);
  MK(SP_SIRENE,A_SIRENE,false,NULL);
  MK(SP_KRAKEN,A_KRAKEN,true,NULL);
  MK(SP_OUVRIERE1,A_OUVRIERE1,false,NULL);
  MK(SP_OUVRIERE2,A_OUVRIERE2,false,NULL);
  MKW(SP_SOLDAT,A_SOLDAT,false,NULL);
  MK(SP_LARVE,A_LARVE,false,NULL);
  MK(SP_LARVULE,A_LARVULE,false,NULL);
  MK(SP_FRELON,A_FRELON,false,NULL);
  MKW(SP_GARDIENNE,A_GARDIENNE,false,NULL);
  MK(SP_COCON,A_COCON,false,NULL);
  MK(SP_BOURDON,A_BOURDON,true,NULL);
  MK(SP_OMBRE,A_OMBRE,false,NULL);
  MKW(SP_ECHO,A_ECHO,false,NULL);
  MK(SP_ECHO_MINEUR,A_ECHO_MINEUR,false,NULL);
  MK(SP_OEIL_NEANT,A_OEIL,false,"wKWkRipcPCMn");
  MK(SP_DEVOREUR,A_DEVOREUR,false,NULL);
  MK(SP_HERAUT,A_HERAUT,false,NULL);
  MK(SP_COLOSSE_NEANT,A_COLOSSE_NEANT,true,NULL);
  /* v7 : le Royaume oublie */
  MK(SP_LUCIOLE,A_LUCIOLE,false,NULL);
  MK(SP_FLEUR,A_FLEUR,false,NULL);
  MK(SP_ESCARGOT,A_ESCARGOT,false,NULL);
  MK(SP_PAPILLON1,A_PAPILLON1,false,NULL);
  MK(SP_PAPILLON2,A_PAPILLON2,false,NULL);
  MK(SP_SYLVAIN,A_SYLVAIN,true,NULL);
  MK(SP_CRAPAUD,A_CRAPAUD,false,NULL);
  MK(SP_SANGSUE,A_SANGSUE,false,NULL);
  MKW(SP_SORCIERE,A_SORCIERE,false,NULL);
  MK(SP_GELEE_MARAIS,A_GELEE,false,"qhQd");
  MK(SP_GELETTE_MARAIS,A_GELETTE,false,"qhQd");
  MK(SP_MOUSTIQUE1,A_MOUSTIQUE1,false,NULL);
  MK(SP_MOUSTIQUE2,A_MOUSTIQUE2,false,NULL);
  MK(SP_HYDRE,A_HYDRE,true,NULL);
  MKW(SP_GOLEM_CRISTAL,A_GOLEM_CRISTAL,false,NULL);
  MK(SP_MINEUR,A_MINEUR,false,NULL);
  MK(SP_VER,A_VER,false,NULL);
  MK(SP_ARAIGNEE_CRISTAL,A_ARAIGNEE_CRISTAL,false,NULL);
  MK(SP_CHAUVEC1,A_CHAUVE1,false,"PCpcxi");
  MK(SP_CHAUVEC2,A_CHAUVE2,false,"PCpcxi");
  MK(SP_QUARTZ,A_QUARTZ,true,NULL);
  MK(SP_PRETRE,A_PRETRE,false,NULL);
  MKW(SP_SCARABEE_OR,A_SCARABEE,false,"tyTY");
  MKW(SP_MOMIE,A_MOMIE,false,NULL);
  MK(SP_PHENIX1,A_PHENIX1,true,NULL);
  MK(SP_PHENIX2,A_PHENIX2,true,NULL);
  MKW(SP_GARDIEN_SOL,A_GARDIEN_SOL,false,NULL);
  MK(SP_SPHINX,A_SPHINX,true,NULL);
  MKI(SP_ICON_FORCE,A_I_FORCE);
  MKI(SP_ICON_CADENCE,A_I_CADENCE);
  MKI(SP_ICON_BOTTES,A_I_BOTTES);
  MKI(SP_ICON_COEUR,A_I_COEUR);
  MKI(SP_ICON_RESSORT,A_I_RESSORT);
  MKI(SP_ICON_SAC,A_I_SAC);
  MKI(SP_ICON_LYNX,A_I_LYNX);
  MKI(SP_ICON_LANTERNE,A_I_LANTERNE);
  MKI(SP_ICON_MOUSSE,A_I_MOUSSE);
  MKI(SP_ICON_FILET,A_I_FILET);
  MKI(SP_ICON_TREFLE,A_I_TREFLE);
  MKI(SP_ICON_BOUCLIER,A_I_BOUCLIER);
  MKI(SP_ICON_RAGE,A_I_RAGE);
  MKI(SP_ICON_FIOLE,A_I_FIOLE);
  MKI(SP_ICON_MIROIR,A_I_MIROIR);
  MKI(SP_ICON_CAPE,A_I_CAPE);
  MKI(SP_ICON_LOUPE,A_I_LOUPE);
  MKI(SP_ICON_ANCRE,A_I_ANCRE);
  MKI(SP_ICON_TOTEM,A_I_TOTEM);
  for(int k=0;k<BK_COUNT;k++) art_ball_icon(&SPR_BALL[k],k);
  for(int b=0;b<BLD_COUNT;b++) draw_building(b);
  for(int c=0;c<CH_COUNT;c++) draw_hut(c);
}

/* ---------------- illustrations des evenements ---------------- */
static void ev_ground(int cx,int cy,uint32_t c){ gfx_ellipse(cx,cy+22,46,9,col_scale(c,0.6f)); gfx_ellipse(cx,cy+21,44,7,c); }
void art_draw_event(int ev,int cx,int cy,float t){
  uint32_t stone=HEX(0x7a7486), stoned=HEX(0x4a4554), stonel=HEX(0xa8a2b4);
  switch(ev){
    case 0: /* fontaine */
      ev_ground(cx,cy,HEX(0x2a3a3a));
      gfx_ellipse(cx,cy+12,34,10,stoned); gfx_ellipse(cx,cy+10,32,8,stone); gfx_ellipse(cx,cy+9,27,5,HEX(0x2a5a9a));
      gfx_ellipse(cx,cy+9,22,3,HEX(0x4a9ad8));
      gfx_rect(cx-4,cy-22,8,30,stoned); gfx_rect(cx-3,cy-22,6,30,stone); gfx_ellipse(cx,cy-22,12,4,stoned); gfx_ellipse(cx,cy-23,11,3,stone);
      for(int i=0;i<6;i++){ float ph=fmodf(t*1.3f+i/6.0f,1.0f); float a=i*TAU/6; float px=cx+cosf(a)*ph*16, py=cy-24-sinf(ph*PI)*12+ph*28; gfx_pset((int)px,(int)py,HEX(0x9ae8ff)); gfx_pset((int)px,(int)py+1,HEX(0x4a9ad8)); }
      for(int i=0;i<4;i++){ int sx=cx-18+i*12+(int)(sinf(t*2+i)*2); gfx_padd(sx,cy+9,HEX(0xd8f8ff),(int)(160+90*sinf(t*5+i*1.7f))); }
      break;
    case 1: /* forgeron errant : enclume */
      ev_ground(cx,cy,HEX(0x3a2a24));
      gfx_rect(cx-24,cy+4,48,6,stoned); gfx_rect(cx-14,cy-6,28,12,stone); gfx_rect(cx-30,cy-12,60,8,stone); gfx_rect(cx-30,cy-12,60,2,stonel);
      gfx_tri((float)cx+30,(float)cy-12,(float)cx+30,(float)cy-4,(float)cx+42,(float)cy-10,stone);
      gfx_rect(cx-12,cy+10,24,8,HEX(0x5c3a24));
      { float sw=sinf(t*5); float hx=cx-6+sw*4, hy=cy-24-fabsf(sw)*8; gfx_thick_line(hx,hy,hx+16,hy-14,1.5f,HEX(0x9a6438)); gfx_rect((int)hx-6,(int)hy-4,12,7,stoned); gfx_rect((int)hx-5,(int)hy-4,10,5,stonel); }
      if(sinf(t*5)>0.9f) for(int i=0;i<6;i++) gfx_padd(cx-4+rirand(8)-4,cy-14-rirand(6),HEX(0xffd36a),255);
      break;
    case 2: /* coffre scelle */
      ev_ground(cx,cy,HEX(0x2e2638));
      gfx_rect(cx-26,cy-6,52,26,HEX(0x5c3a24)); gfx_rect(cx-26,cy-6,52,2,HEX(0x9a6438));
      gfx_ellipse(cx,cy-6,26,12,HEX(0x6a4428)); gfx_rect(cx-26,cy-6,52,1,HEX(0x2a1810));
      for(int i=-1;i<=1;i+=2){ gfx_rect(cx+i*18-2,cy-16,4,36,HEX(0xc99a2e)); }
      gfx_rect(cx-5,cy-4,10,10,HEX(0xf9dc5c)); gfx_rect(cx-2,cy,4,4,HEX(0x2a1810));
      { int a=(int)(120+100*sinf(t*3)); for(int i=0;i<5;i++){ float an=t*0.8f+i*TAU/5; gfx_padd(cx+(int)(cosf(an)*30),cy-4+(int)(sinf(an)*10),HEX(0xc08aff),a); } }
      break;
    case 3: /* autel du sang */
      ev_ground(cx,cy,HEX(0x2a1418));
      gfx_rect(cx-24,cy-2,48,22,stoned); gfx_rect(cx-22,cy-2,44,20,stone); gfx_rect(cx-26,cy-6,52,6,stonel);
      gfx_rect(cx-20,cy-6,40,3,HEX(0x8f2233)); for(int i=0;i<5;i++){ int dx=cx-18+i*9; int l=4+(int)(fmodf(t*0.7f+i*0.3f,1.0f)*10); gfx_vline(dx,cy-3,cy-3+l,HEX(0xc0303a)); }
      gfx_disc(cx,cy-16,7,HEX(0x5a0a1a)); gfx_disc(cx,cy-16,5,HEX(0xd8203a)); gfx_disc(cx-2,cy-18,1,HEX(0xffa0a0));
      gfx_glow((float)cx,(float)cy-16,18,HEX(0xff2040),0.5f+0.2f*sinf(t*4));
      break;
    case 4: /* autel de l'avarice */
      ev_ground(cx,cy,HEX(0x2a2418));
      gfx_rect(cx-22,cy-4,44,24,HEX(0x8a6a1a)); gfx_rect(cx-20,cy-4,40,22,HEX(0xc99a2e)); gfx_rect(cx-24,cy-8,48,5,HEX(0xf9dc5c));
      for(int i=0;i<9;i++){ int px=cx-16+(i%5)*8+(i/5)*4, py=cy-12-(i/5)*4; gfx_ellipse(px,py,3,1,HEX(0xc99a2e)); gfx_ellipse(px,py-1,3,1,HEX(0xf9dc5c)); }
      gfx_disc(cx,cy+8,5,HEX(0x5a3a0a)); gfx_disc(cx,cy+8,3,HEX(0x1a0a04));
      if(((int)(t*4))&1) gfx_padd(cx+10,cy-18,HEX(0xffffff),255);
      break;
    case 5: /* pacte des ombres */
      ev_ground(cx,cy,HEX(0x140a1c));
      for(int i=0;i<7;i++){ float a=PI+i*PI/6; art_draw_tentacle(cx+cosf(a)*20,cy+16,t+i,26,HEX(0x3a1a5a),HEX(0x1a0a2a),-PI*0.5f+(i-3)*0.25f); }
      gfx_ellipse(cx,cy-10,16,10,HEX(0x0a0410)); gfx_ellipse(cx,cy-10,10,6,HEX(0xc02848)); gfx_ellipse(cx,cy-10,2,6,HEX(0x0a0410));
      gfx_glow((float)cx,(float)cy-10,26,HEX(0x8a2aff),0.45f+0.15f*sinf(t*3));
      break;
    case 6: /* marchand ambulant */
      ev_ground(cx,cy,HEX(0x2a3022));
      gfx_rect(cx-30,cy-4,50,18,HEX(0x6a4428)); gfx_rect(cx-30,cy-4,50,2,HEX(0x9a6438));
      gfx_disc(cx-20,cy+16,7,HEX(0x3a2416)); gfx_disc(cx-20,cy+16,5,HEX(0x6a4428)); gfx_disc(cx+12,cy+16,7,HEX(0x3a2416)); gfx_disc(cx+12,cy+16,5,HEX(0x6a4428));
      half_ellipse(cx-5,cy-4,28,20,HEX(0xd8c8a4),true); for(int i=-24;i<=20;i+=8) gfx_vline(cx-5+i,cy-18,cy-5,HEX(0xb3a07e));
      gfx_rect(cx+22,cy-10,4,24,HEX(0x5c3a24)); gfx_disc(cx+24,cy-14,4,HEX(0xffd36a));
      gfx_glow((float)cx+24,(float)cy-14,14,HEX(0xffb040),0.6f);
      break;
    case 7: /* statue */
      ev_ground(cx,cy,HEX(0x2a2632));
      gfx_rect(cx-14,cy+6,28,14,stoned); gfx_rect(cx-12,cy+6,24,12,stone);
      { Sprite *s=&SPR[SP_JONGLEUR]; DrawOpt o=dopt(); o.sx=o.sy=1.25f; o.flash=HEX(0x9a94a8); o.flash_a=175; draw_spr_c(s,(float)cx,(float)cy-14,&o); }
      for(int i=0;i<3;i++){ float a=t*2+i*TAU/3; art_draw_ball(cx+cosf(a)*22,cy-34+sinf(a)*6,2.5f,BK_FER+i*3,t,true); }
      break;
    case 8: /* puits sans fond */
      ev_ground(cx,cy,HEX(0x22262a));
      gfx_ellipse(cx,cy+6,28,12,stoned); gfx_ellipse(cx,cy+4,26,10,stone); gfx_ellipse(cx,cy+4,20,7,HEX(0x05030a));
      gfx_rect(cx-26,cy+4,52,12,stone); for(int i=0;i<6;i++) gfx_vline(cx-24+i*10,cy+5,cy+15,stoned);
      gfx_glow((float)cx,(float)cy+4,20,HEX(0x8a5aff),0.35f+0.15f*sinf(t*2));
      break;
    case 9: /* bibliotheque oubliee */
      ev_ground(cx,cy,HEX(0x2a2030));
      gfx_rect(cx-30,cy-26,60,46,HEX(0x4a2a1a)); gfx_rect(cx-28,cy-24,56,42,HEX(0x2a1810));
      for(int sh=0;sh<3;sh++){ int y=cy-24+sh*14; gfx_rect(cx-28,y+12,56,2,HEX(0x6a4428)); for(int i=0;i<12;i++){ static const uint32_t BC[5]={0x8f2233,0x2a3b80,0x3fa34d,0xc99a2e,0x5d2d8c}; int h=8+((i*7+sh*3)%4); gfx_rect(cx-27+i*4+(i>5?2:0),y+12-h,3,h,HEX(BC[(i+sh)%5])); } }
      gfx_glow((float)cx+18,(float)cy-30,14,HEX(0xffe0a0),0.5f+0.2f*sinf(t*3));
      break;
    case 10: /* refugies */
      ev_ground(cx,cy,HEX(0x2a3022));
      gfx_tri((float)cx-36,(float)cy+18,(float)cx-4,(float)cy+18,(float)cx-20,(float)cy-12,HEX(0x8a6a4a)); gfx_tri((float)cx-26,(float)cy+18,(float)cx-14,(float)cy+18,(float)cx-20,(float)cy+2,HEX(0x2a1810));
      gfx_tri((float)cx+4,(float)cy+18,(float)cx+38,(float)cy+18,(float)cx+21,(float)cy-16,HEX(0x6a7a8a)); gfx_tri((float)cx+15,(float)cy+18,(float)cx+27,(float)cy+18,(float)cx+21,(float)cy+2,HEX(0x2a1810));
      for(int i=0;i<3;i++){ float f=sinf(t*12+i*2); gfx_tri((float)cx-4+i*3,(float)cy+20,(float)cx+i*3,(float)cy+20,(float)cx-2+i*3+f,(float)cy+12-f*2,i==1?HEX(0xffd36a):HEX(0xff7a22)); }
      break;
    case 12: /* source glacee */
      ev_ground(cx,cy,HEX(0x2a3a4a));
      gfx_ellipse(cx,cy+8,32,11,HEX(0x5a7a9a)); gfx_ellipse(cx,cy+6,30,9,HEX(0x9ac8e8)); gfx_ellipse(cx,cy+6,24,6,HEX(0x2a6aa8)); gfx_ellipse(cx-6,cy+5,8,2,HEX(0x9ae8ff));
      for(int i=0;i<6;i++){ int x=cx-26+i*10, h=8+((i*7)%9); gfx_tri((float)x-3,(float)cy+4,(float)x+3,(float)cy+4,(float)x,(float)(cy+4-h),i&1?HEX(0x9ae8ff):HEX(0xe8faff)); }
      for(int i=0;i<5;i++){ float ph=fmodf(t*0.6f+i*0.2f,1.0f); gfx_padd(cx-20+i*10,(int)(cy+4-ph*26),HEX(0xd8f4ff),(int)(220*(1-ph))); }
      gfx_glow((float)cx,(float)cy+6,26,HEX(0x6ad8ff),0.3f+0.1f*sinf(t*2));
      break;
    case 13: /* epave engloutie */
      ev_ground(cx,cy,HEX(0x1a3430));
      gfx_tri((float)cx-40,(float)cy+16,(float)cx+30,(float)cy+16,(float)cx-30,(float)cy-6,HEX(0x4a2e1c)); gfx_tri((float)cx-30,(float)cy-6,(float)cx+30,(float)cy+16,(float)cx+38,(float)cy-2,HEX(0x5c3a24));
      for(int i=0;i<6;i++) gfx_line(cx-34+i*12,cy+14,cx-26+i*12,cy-2,HEX(0x2a1810));
      gfx_thick_line((float)cx+4,(float)cy+2,(float)cx+14,(float)cy-34,1.5f,HEX(0x6a4428)); gfx_line(cx+2,cy-24,cx+22,cy-20,HEX(0x6a4428));
      gfx_rect(cx-14,cy+2,10,7,HEX(0x8a6a1a)); gfx_rect(cx-14,cy+2,10,2,HEX(0xc99a2e)); gfx_pset(cx-9,cy+5,HEX(0xf9dc5c));
      for(int i=0;i<6;i++){ float ph=fmodf(t*0.5f+i*0.17f,1.0f); gfx_ring_a(cx-30+i*12,(int)(cy+10-ph*40),1,HEX(0x9af0e0),(int)(200*(1-ph)),true); }
      break;
    case 14: /* miroir du neant */
      ev_ground(cx,cy,HEX(0x12101a));
      gfx_ellipse(cx,cy-6,20,28,HEX(0x8a7aaa)); gfx_ellipse(cx,cy-6,18,26,HEX(0x05040a));
      { Sprite *sp=&SPR[CHARS[R.ch].sprite]; DrawOpt o=dopt(); o.sx=o.sy=1; o.flash=HEX(0xe0f0ff); o.flash_a=(int)(120+60*sinf(t*2)); o.alpha=150; draw_spr_c(sp,(float)cx,(float)cy-6,&o); }
      gfx_line(cx-8,cy-28,cx+2,cy-10,HEX(0xe0f0ff)); gfx_line(cx+2,cy-10,cx-4,cy+6,HEX(0xe0f0ff)); gfx_line(cx+2,cy-10,cx+12,cy-4,HEX(0xa8b8e8));
      gfx_glow((float)cx,(float)cy-6,30,HEX(0x8a7aff),0.25f+0.1f*sinf(t*3));
      break;
    default: /* champignon etrange */
      ev_ground(cx,cy,HEX(0x1e2a22));
      gfx_rect(cx-5,cy-6,10,26,HEX(0xe8dcc4)); gfx_rect(cx-5,cy-6,3,26,HEX(0xffffff));
      half_ellipse(cx,cy-6,28,22,HEX(0x5a1a6a),true); half_ellipse(cx,cy-7,26,20,HEX(0x9a3ac8),true);
      { static const int D[][3]={{-14,-14,4},{6,-20,5},{16,-10,3},{-4,-12,3}}; for(int i=0;i<4;i++) gfx_disc(cx+D[i][0],cy+D[i][1],D[i][2],HEX(0xe0ffd0)); }
      gfx_glow((float)cx,(float)cy-14,34,HEX(0xc06aff),0.35f+0.15f*sinf(t*2.5f));
      break;
  }
}
