/* GOUFFRE - entrainement des heros (v6.6)
   - prismes gagnes sur les gardiens, depenses au campement
   - six attributs par heros (Vitalite, Puissance, Cadence, Vitesse, Precision, Savoir), dix niveaux chacun,
     chaque attribut ayant son propre cout qui grandit a chaque niveau
   - ecran « Entraînement » : figurine voxel du heros qui tourne sur elle-meme, attributs a cote          */
#include "game.h"

/* ---------------- donnees ---------------- */
static const int TR_ICON[TR_COUNT]={PS_COEUR,PS_FORCE,PS_CADENCE,PS_BOTTES,PS_LYNX,PS_LANTERNE};
static const uint32_t TR_COL[TR_COUNT]={HEX(0xe8384a),HEX(0xd8dcea),HEX(0xe8b840),HEX(0xb07ad8),HEX(0x6ad8ff),HEX(0xd8b070)};
static const char *const TR_DESC[TR_COUNT]={
  "Points de vie maximum au début de chaque descente.",
  "Dégâts de toutes les billes lancées par le héros.",
  "Vitesse à laquelle le héros lance ses billes.",
  "Vitesse de déplacement au fond du gouffre.",
  "Chances de coup critique (dégâts doublés).",
  "Expérience gagnée : les niveaux viennent plus vite."};

int train_level(int ch,int k){ if(ch<0||ch>=CH_COUNT||k<0||k>=TR_COUNT) return 0; return clampi(S.train[ch][k],0,TRAIN_CAP); }
/* sans fin, mais de plus en plus cher : 1, 2, 4, 6, 9, 12, 16, 20, 25, 30, 36, 42, 49... */
int train_cost(int lvl){ lvl=clampi(lvl,0,TRAIN_CAP); return lvl+1+lvl*lvl/4; }
float train_value(int k,int lvl){
  switch(k){
    case TR_VITA: return 6.0f*lvl;          /* PV */
    case TR_PUIS: return 0.03f*lvl;
    case TR_CADE: return 0.03f*lvl;
    case TR_VITE: return 0.02f*lvl;
    case TR_PREC: return 0.02f*lvl;         /* points de critique */
    default:      return 0.05f*lvl;         /* experience */
  }
}
float train_bonus(int ch,int k){ return train_value(k,train_level(ch,k)); }
static void train_short(int k,int lvl,char *b,int n){
  if(lvl<=0){ snprintf(b,n,"-"); return; }
  if(k==TR_VITA) snprintf(b,n,"+%d PV",(int)(train_value(k,lvl)+0.5f));
  else snprintf(b,n,"+%d %%",(int)(train_value(k,lvl)*100+0.5f));
}
void train_effect(int k,int lvl,char *b,int n){
  int v= k==TR_VITA ? (int)(train_value(k,lvl)+0.5f) : (int)(train_value(k,lvl)*100+0.5f);
  if(lvl<=0){ snprintf(b,n,"aucun"); return; }
  switch(k){
    case TR_VITA: snprintf(b,n,"+%d PV max",v); break;
    case TR_PUIS: snprintf(b,n,"+%d %% de dégâts",v); break;
    case TR_CADE: snprintf(b,n,"+%d %% de cadence",v); break;
    case TR_VITE: snprintf(b,n,"+%d %% de vitesse",v); break;
    case TR_PREC: snprintf(b,n,"+%d %% de critiques",v); break;
    default:      snprintf(b,n,"+%d %% d'expérience",v); break;
  }
}
int train_total(int ch){ int t=0; for(int k=0;k<TR_COUNT;k++) t+=train_level(ch,k); return t; }
bool train_can(int ch,int k){ int L=train_level(ch,k); return ch>=0 && ch<CH_COUNT && S.unlocked[ch] && L<TRAIN_CAP && S.prisms>=train_cost(L); }
static bool hero_can(int ch){ for(int k=0;k<TR_COUNT;k++) if(train_can(ch,k)) return true; return false; }
bool train_any_affordable(void){ for(int c=0;c<CH_COUNT;c++) if(hero_can(c)) return true; return false; }
/* gardien vaincu : plus de prismes en profondeur, dans les Profondeurs et en Difficile */
int train_boss_prisms(void){
  int p;
  if(R.single) p=2+(R.depth-1)/3+(R.lvl>=LEVELS_PER_DEST?1:0);
  else p=1+(R.level==LEVELS_PER_DEST-1?1:0)+(R.dest?1:0);
  if(R.diff_low==DIFF_DIFFICILE) p++;
  else if(R.diff_low==DIFF_FACILE) p=maxi(1,p-1);
  return p;
}

/* ---------------- figurines voxel des heros ----------------
   Le croquis du heros (12 x 15) est agrandi x4 (EPX deux fois, sur les lettres : les matieres restent
   connues), puis chaque rangee est tournee autour de son axe : une section elliptique par segment.
   L'avant porte le croquis, l'arriere un croquis du dos dessine a la main (cheveux, capuches, capes).
   Les accessoires tenus a bout de bras (baton, aimants, flamme) forment leurs propres cylindres. */
static const char *const B_SENTINELLE[15]={
".....Cc.....","....CCc.....","...GWWWWG...","..GWWWWWWG..","..GWWWWWWG..",
"..GWWWWWWG..","..GWWWWWWG..","...GWWWWG...","..CCCCCCCC..",".CCnnnnnnCC.",
".snnnnnnnns.","..nnnnnnnn..","..nnnnnnnn..","...gg..gg...","...GG..GG..."};
static const char *const B_ALCHIMISTE[15]={
".....LL.....","....LLLL....","...LLLLLL...","..LLLLLLLL..","..LLYYYYLL..",
"..LLLLLLLL..","..LLLLLLLL..","...LLLLLL...","..PPLLLLPP..",".PPPPLLPPPP.",
".sPPPPPPPPs.","..PPPPPPPP..","..PPPPPPPP..","..PPPPPPPP..","...HH..HH..."};
static const char *const B_JONGLEUR[15]={
"y..........y","rr........yy",".rrr....yyy.","..rrrryyyy..","..wwwwwwww..",
"..rrrryyyy..","..rrrryyyy..","..rrrryyyy..","...rrryyy...","..ryyrryyr..",
".sryyrryyrs.","..yrryyrry..","..yrryyrry..","...rr..yy...","...kk..kk..."};
static const char *const B_FORGERONNE[15]={
"...rRRRRr...","..rrrrrrrr..",".rrrrRRrrrr.",".rrrRrrRrrr.",".rrRrrrrRrr.",
".rrrrrrrrrr.",".rrRrrrrRrr.",".rr.rRRr.rr.",".r.HHHHHH.r.","..sHhHHhHs..",
".sshhHHhhss.","..HhhHHhhH..","..HdhhhhdH..","...dd..dd...","...KK..KK..."};
static const char *const B_MAGNETISEUR[15]={
"Wg........gW","Wg........gW","rR..uuuu..nC","rR.uuuuuu.nC","rR.uuuuuu.nC",
"rR.uUuuUu.nC","rR.uUUUUu.nC",".rR.uuuu.nC.","..RRuuuunn..",".uuuUUUUuuu.",
".suuUUUUuus.","..uUUUUUUu..","..UUUUUUUU..","...uU..Uu...","...KK..KK..."};
static const char *const B_NECRO[15]={
"...PPPP.....","..PPPPPP..b.",".PPPPPPPPbbb",".PPPPPPPPbbb",".PPPpPPPPbbb",
".PPPpPPPP.h.",".PPPpPPPP.h.","..PPpPPPPsh.","..PPPPPPPPh.",".PPPPPPPPPh.",
"sPPPPPPPPPh.",".PPPPPPPPPh.",".PPPPPPPPPh.","..kk..kk..h.","..KK..KK..H."};
static const char *const B_HORLOGERE[15]={
"....uuuu....","...uyyyyu...","..uyyyyyyu..","..uyyyyyyu..","..CyyyyyyC..",
"..uyyuuyyu..","...yuyyuy...","....uyyu....","..tTTTTTTt..",".tTTtTTtTTt.",
"sTtTYyyYTtTs",".TtTTYYTTtT.","..TtTttTtT..","...TT..TT...","...KK..KK..."};
static const char *const B_PYROMANE[15]={
"....y...y...","..y.oy.yo...","..oyooyooy..",".JoooooooJ..",".JJooooooJJ.",
"..JooooooJ..","..JooooooJ..","...oooooo...","..rrrrrrrr..","y.KKKrrKKK..",
"jsKKKrrKKKs.","J.KKKrrKKK..","..KKKKKKKK..","...KK..KK...","...dd..dd..."};
static const char *const B_DRUIDE[15]={
".h.....h.zz.",".hh...hh.zz.","..hLLLLh..h.","..LLLLLL..h.",".LLLLLLLL.h.",
".LLLlLLLL.h.",".LLLlLLLL.h.","..LLlLLL..h.","..lLLLLl..h.",".lLLLLLLL.h.",
"slLLLLLLLsh.",".LLLLLLLL.h.","..LLLLLL..h.","...hh.hh..h.","...HH.HH..H."};
static const char *const B_MARCHAND[15]={
"........cc..","....KKKKc...","...KKKKKK...","..KYYYYYYK..","KKKKKKKKKKKK",
"...HHHHHH...","...HHHHHH...","...HHHHHH...","....HHHH....","..RRRRRRRR..",
".RRRRRRRRRR.","sRRRRRRRRRRs",".RRRRRRRRRR.","..RRRRRRRR..","...KK..KK..."};
static const char *const B_ASTRONOME[15]={
"....nn......","...nnnn.....","...nnynn....","..nnnnnnn...","..nnnnynnn..",
".CCCCCCCCCC.","...nnnnnn...","...nnnnnn...","...nnnnnn...","..CnnnnnnC..",
".nnnynnnnnc.","snnnnnnnnsci",".nnynnnynn..","..nnnnnnnn..","...KK..KK..."};
static const char *const B_VOLEUSE[15]={
"....PPPP....","...PPPPPP...","..PPPPPPPP..","..PPPPPPPP..","..PPPPPPPP..",
"..PPPPPPPP..","..PPPPPPPP..","...PPppPP...","..KPPPPPPK..",".KKPPPPPPKK.",
"sKKPPPPPPKKs",".KKKPPPPKK.W","..KKKKKKKK.W","...KK..KK...","...dd..dd..."};
static const char *const B_CHASSERESSE[15]={
"....EEEE....","...ELLLLE...","..ELLLLLLE..","..ELLLLLLE..","..ELLLLLLE..",
"..ELLLLLLE..","...ELLLLE...","..hELuuLEh..",".hHLLuuLLHh.","shHLLuuLLHhs",
"...LLLLLL...","...HhhhhH...","...LLLLLL...","...HH..HH...","...dd..dd..."};
static const char *const B_GEOMANCIEN[15]={
"....HHHH...a","...HhhhhH.aj","..HhhhhhhH.a","..HhhhhhhH.a","..HhhhhhhH.a",
"..HhhhhhhH.a","...HhhhhH..a","..UUhhhhUU.a",".UUuuuuuuUUa","sUUuuUUuuUUs",
".UUuuUUuuU.a",".UUUAAAAUU.a","..UUUUUUUU.a","...UU..UU..A","...dd..dd..A"};
/* neck : premiere rangee du corps (au-dessus : la tete, presque ronde ; en dessous : le corps, plus plat) */
typedef struct { const char *const *back; const char *metal, *glow, *fire; int propL, propR, neck; } HeroFig;
static const HeroFig FIG[CH_COUNT]={
  {B_SENTINELLE,"WwGgY","","",-1,99,8},
  {B_ALCHIMISTE,"Y","","",-1,99,8},
  {B_JONGLEUR,"","","",-1,99,9},
  {B_FORGERONNE,"","","",-1,99,8},
  {B_MAGNETISEUR,"Wg","c","",1,10,8},
  {B_NECRO,"","q","",-1,9,8},
  {B_HORLOGERE,"YyC","i","",-1,99,8},
  {B_PYROMANE,"","","yoJj",0,99,8},
  {B_DRUIDE,"","z","",-1,10,8},
  {B_MARCHAND,"Yy","","",-1,99,9},
  {B_ASTRONOME,"","yci","",-1,11,9},
  {B_VOLEUSE,"W","v","",-1,11,8},
  {B_CHASSERESSE,"","","",0,99,8},
  {B_GEOMANCIEN,"aA","j","",-1,11,8},
};
#define A0W 12
#define A0H 15
#define FW (A0W*4)
#define FH (A0H*4)
#define PED 6             /* hauteur du socle */
#define FZC 14            /* axe de rotation (profondeur, repere de travail) */
/* agrandissement x2 (EPX) sur une grille de lettres ('.' = vide) */
static void epx(const char *src,int w,int h,char *dst){
  #define AT(x,y) (((x)<0||(y)<0||(x)>=w||(y)>=h)?'.':src[(y)*w+(x)])
  for(int y=0;y<h;y++) for(int x=0;x<w;x++){
    char P=AT(x,y), A=AT(x,y-1), B=AT(x+1,y), C=AT(x-1,y), D=AT(x,y+1);
    char o0=P,o1=P,o2=P,o3=P;
    if(C==A && C!=D && A!=B) o0=A;
    if(A==B && A!=C && B!=D) o1=B;
    if(D==C && D!=B && C!=A) o2=C;
    if(B==D && B!=A && D!=C) o3=D;
    dst[(2*y)*2*w+2*x]=o0; dst[(2*y)*2*w+2*x+1]=o1; dst[(2*y+1)*2*w+2*x]=o2; dst[(2*y+1)*2*w+2*x+1]=o3;
  }
  #undef AT
}
static uint32_t fig_col(const HeroFig *hf,char c){
  uint32_t col=art_pal(c)&0xFFFFFFu; int mat=VM_SOLID;
  if(strchr(hf->metal,c)) mat=VM_METAL;
  if(strchr(hf->glow,c)) mat=VM_GLOW;
  if(strchr(hf->fire,c)) mat=VM_FIRE;
  return VC(mat,col);
}
static int fig_cls(const HeroFig *hf,int X){ int ox=X/4; return (ox<=hf->propL || ox>=hf->propR) ? 1 : 0; }
static void build_hero(int ch,VoxModel *m){
  const HeroFig *hf=&FIG[ch]; const char *const *fr=hero_art(ch);
  static char f1[A0W*A0H], b1[A0W*A0H], f2[A0W*A0H*4], b2[A0W*A0H*4], F[FW*FH], B[FW*FH];
  for(int y=0;y<A0H;y++) for(int x=0;x<A0W;x++){
    char c=fr[y][x], d=hf->back[y][x];
    f1[y*A0W+x]= (c && c!='.' && art_pal(c)) ? c : '.';
    b1[y*A0W+x]= (d && d!='.' && art_pal(d)) ? d : '.';
  }
  epx(f1,A0W,A0H,f2); epx(f2,A0W*2,A0H*2,F);
  epx(b1,A0W,A0H,b2); epx(b2,A0W*2,A0H*2,B);
  vx_begin();
  /* socle : pierre sombre, bord de bronze, anneau lumineux a la couleur du heros */
  uint32_t hc=CHARS[ch].col;
  vx_cyl(0,FZC,27.5f,0,1,VC(VM_SOLID,0x231c2a));
  vx_cyl_stones(0,FZC,26.0f,2,4,VC(VM_SOLID,0x5a5068),VC(VM_SOLID,0x342e40));
  vx_cyl(0,FZC,26.5f,5,5,VC(VM_METAL,0xb88a48));
  vx_cyl(0,FZC,23.0f,5,5,VC(VM_GLOW,col_lerp(hc|0xFF000000u,HEX(0xffffff),0.3f)));
  vx_cyl(0,FZC,21.5f,5,5,VC(VM_SOLID,0x3e3650));
  /* la figurine, rangee par rangee */
  for(int r=0;r<FH;r++){
    int y=PED+(FH-1-r);
    for(int X=0;X<FW;){
      if(F[r*FW+X]=='.'){ X++; continue; }
      int cls=fig_cls(hf,X), a=X;
      while(X<FW && F[r*FW+X]!='.' && fig_cls(hf,X)==cls) X++;
      int b=X-1;
      float cx=(a+b+1)*0.5f, hw=(b-a+1)*0.5f, dz=maxf(1.0f,hw*(cls?1.0f:(r<hf->neck*4?0.86f:0.64f)));
      for(int x=a;x<=b;x++){
        float u=(x+0.5f-cx)/hw; float ze=dz*sqrtf(maxf(0.0f,1.0f-u*u)); if(ze<0.75f) ze=0.75f;
        int z0=(int)floorf(FZC-ze+0.5f), z1=(int)ceilf(FZC+ze-0.5f)-1; if(z1<z0) z1=z0;
        for(int z=z0;z<=z1;z++){
          char c= z<FZC ? F[r*FW+x] : (B[r*FW+x]!='.' ? B[r*FW+x] : F[r*FW+x]);   /* l'avant regarde vers -z (comme les facades) */
          vx_set(x-FW/2,y,z,fig_col(hf,c));
        }
      }
    }
  }
  vx_end(m,0,(float)FZC);
}
static VoxModel hm[CH_COUNT]; static bool hm_ok[CH_COUNT];
static const VoxModel *hero_model(int ch){ if(!hm_ok[ch]){ build_hero(ch,&hm[ch]); hm_ok[ch]=true; } return &hm[ch]; }
const VoxModel *hero_fig_model(int ch);
const VoxModel *hero_fig_model(int ch){ return hero_model(clampi(ch,0,CH_COUNT-1)); }
/* planche de controle (tests) : les huit figurines sous quatre angles */
void hero_fig_sheet(const char *path){
  int CW=180, CHh=230, W=CW*CH_COUNT, H=CHh*4; uint32_t *img=(uint32_t*)calloc((size_t)W*H,4);
  Sprite sp=spr_alloc(CW,CHh); sp.em=(uint32_t*)calloc((size_t)CW*CHh,4);
  static const float YAW[4]={-0.45f,0.8f,2.3f,3.6f};
  for(int c=0;c<CH_COUNT;c++) for(int a=0;a<4;a++){
    VoxCam cam={YAW[a],0.26f,2.8f,CW*0.5f,CHh-24.0f};
    vox_set_light(-0.45f,0.5f,0.75f); vox_render(hero_model(c),0,0,&cam,&sp); vox_set_light(0,0,0);
    for(int y=0;y<CHh;y++) for(int x=0;x<CW;x++){ uint32_t p=sp.px[y*CW+x]; img[(a*CHh+y)*W+c*CW+x]= (p>>24) ? p : HEX(0x2a2432); }
  }
  FILE *f=fopen(path,"wb"); if(f){ fprintf(f,"P6\n%d %d\n255\n",W,H); for(int i=0;i<W*H;i++){ uint8_t q[3]={(uint8_t)cr(img[i]),(uint8_t)cg(img[i]),(uint8_t)cb(img[i])}; fwrite(q,1,3,f); } fclose(f); }
  free(img); free(sp.px); free(sp.em);
}

/* ---------------- ecran titre : les huit heros en buste autour du titre (composition facon affiche) ----------------
   Les figurines sont rendues une fois en gros plan (camera un peu basse, lumiere venue du centre de l'ecran),
   puis repeintes : bas du buste qui se perd dans le noir, contre-jour a la couleur du heros sur le bord exterieur.
   Ordre d'affichage : du plus lointain (au bord du titre) au plus proche (en bas, coupe par l'ecran). */
typedef struct { int ch; float cx, top, sc, yaw; int side; float bright; } Bust;
/* l'affiche du titre reste celle des huit heros d'origine (les heros ajoutes ensuite n'y figurent pas) */
#define TITLE_BUSTS 8
static const Bust BUSTS[TITLE_BUSTS]={
  {CH_JONGLEUR,   172, 18,2.25f, 0.42f,-1,0.66f},
  {CH_HORLOGERE,  468, 18,2.25f,-0.42f, 1,0.66f},
  {CH_ALCHIMISTE,  66,-10,2.65f, 0.34f,-1,0.82f},
  {CH_NECRO,      574,-10,2.65f,-0.34f, 1,0.82f},
  {CH_FORGERONNE, 186,170,2.85f, 0.30f,-1,0.92f},
  {CH_MAGNETISEUR,454,170,2.85f,-0.30f, 1,0.92f},
  {CH_SENTINELLE,  72,146,3.25f, 0.24f,-1,1.00f},
  {CH_PYROMANE,   568,146,3.25f,-0.24f, 1,1.00f},
};
#define BUST_PITCH (-0.10f)
static Sprite bust_spr[TITLE_BUSTS]; static int bust_done[TITLE_BUSTS]; static bool bust_lock[TITLE_BUSTS]; static int bust_frame=0;
static void bust_render(int i,float t){
  const Bust *b=&BUSTS[i]; int ch=b->ch; const VoxModel *m=hero_model(ch);
  int W=(int)(64*b->sc), H=(int)(50*b->sc+10);
  if(!bust_spr[i].px){ bust_spr[i]=spr_alloc(W,H); bust_spr[i].em=(uint32_t*)calloc((size_t)W*H,4); }
  Sprite *sp=&bust_spr[i];
  float top=(float)(m->y0+m->ny)-m->py;
  VoxCam cam={b->yaw,BUST_PITCH,b->sc,W*0.5f,8+top*b->sc*cosf(BUST_PITCH)};
  /* lumiere venue du centre de l'ecran (devant, un peu en haut) */
  vox_set_light(b->side<0?0.55f:-0.55f,0.42f,0.72f); vox_render(m,t,VR_SMOOTH,&cam,sp); vox_set_light(0,0,0);
  bool lock=false; bust_lock[i]=lock;   /* sur l'affiche, tous les heros paraissent (meme ceux a debloquer) */
  uint32_t rim=CHARS[ch].col|0xFF000000u; rim=col_lerp(rim,HEX(0xffffff),0.25f);
  /* repeint : fondu vers le bas, assombrissement exterieur, contre-jour colore, statue sombre si verrouille */
  for(int y=0;y<H;y++) for(int x=0;x<W;x++){
    int k=y*W+x; uint32_t c=sp->px[k]; if(!(c>>24)) continue;
    float fy=(float)y/H, fx=(float)x/W, outer= b->side<0 ? 1.0f-fx : fx;
    float shade=b->bright*(1.0f-0.6f*clampf((fy-0.40f)/0.5f,0,1))*(1.0f-0.42f*outer*sqrtf(outer));
    uint32_t o= lock ? col_lerp(c|0xFF000000u,HEX(0x0c0812),0.9f) : col_scale(c|0xFF000000u,shade);
    if(!lock){
      /* etalonnage d'affiche : ombres froides (teintees du heros), lumieres chaudes */
      float L=(cr(o)*0.3f+cg(o)*0.59f+cb(o)*0.11f)/255.0f;
      uint32_t cool=col_lerp(HEX(0x141a3a),CHARS[ch].col|0xFF000000u,0.25f);
      o=col_lerp(o,cool,0.38f*(1-L)*(1-L)); o=col_lerp(o,HEX(0xfff0dc),0.12f*L*L);
      o=col_lerp(o,HEX(0x120a1e),0.22f*clampf((fy-0.3f)/0.7f,0,1));
    }
    /* contre-jour : pixels du bord, cote exterieur (et le haut du crane) */
    int dx= b->side<0 ? -1 : 1; int edge=0;
    for(int d=1;d<=3 && !edge;d++){ int xx=x+dx*d; if(xx<0||xx>=W||!(sp->px[y*W+xx]>>24)) edge=d; }
    if(!edge && y>1 && !(sp->px[(y-2)*W+x]>>24) && fy<0.5f) edge=2;
    if(edge) o=col_lerp(o,rim,(lock?0.55f:0.78f)*(1.0f-0.75f*fy)*(edge==1?1.0f:(edge==2?0.7f:0.4f)));
    float a=1.0f-clampf((fy-0.66f)/0.32f,0,1); a=a*a*(3-2*a);
    sp->px[k]=((uint32_t)(int)(a*255)<<24)|(o&0xFFFFFFu);
    if(sp->em && sp->em[k]){ if(lock) sp->em[k]=0; else sp->em[k]=col_scale(sp->em[k]|0xFF000000u,a)&0xFFFFFFu; }
  }
  bust_done[i]=1;
}
/* a appeler chaque image du titre, avant le logo et les boutons */
void title_heroes_draw(float t){
  bust_frame++;
  /* premiere apparition : deux bustes par image (pas d'a-coup) ; heros debloque depuis : on refait le sien */
  int budget=2;
  for(int i=0;i<TITLE_BUSTS && budget>0;i++) if(!bust_done[i]){ bust_render(i,t); budget--; }
  /* les flammes du Pyromane vivent : son buste est refait de temps en temps */
  for(int i=0;i<TITLE_BUSTS;i++) if(BUSTS[i].ch==CH_PYROMANE && bust_done[i] && !bust_lock[i] && bust_frame%8==0) bust_render(i,t);
  int ly=layer_current();
  for(int i=0;i<TITLE_BUSTS;i++){
    if(!bust_done[i]) continue;
    const Bust *b=&BUSTS[i]; const Sprite *sp=&bust_spr[i];
    int x0=(int)(b->cx-sp->w*0.5f), y0=(int)(b->top+sinf(t*0.9f+i*1.7f)*1.2f);
    /* lueurs (yeux, feu) : couche d'emission pour le halo du rendu GPU */
    if(g_gpu && sp->em){ layer(LY_EMIS);
      for(int y=0;y<sp->h;y++) for(int x=0;x<sp->w;x++){ uint32_t e=sp->em[y*sp->w+x]; if(e) gfx_pset(x0+x,y0+y,e); }
      layer(ly); }
    for(int y=0;y<sp->h;y++){ int py=y0+y; if(py<gT->cy0||py>=gT->cy1) continue;
      for(int x=0;x<sp->w;x++){ uint32_t c=sp->px[y*sp->w+x]; int a=(int)(c>>24); if(!a) continue; gfx_pblend(x0+x,py,c&0xFFFFFFu,a); } }
    if(!g_gpu && sp->em){ for(int y=0;y<sp->h;y++) for(int x=0;x<sp->w;x++){ uint32_t e=sp->em[y*sp->w+x]; if(e) gfx_padd(x0+x,y0+y,e,110); } }
  }
}

/* ---------------- ecran « Entraînement » ---------------- */
#define TS_X 18
#define TS_Y 46
#define TS_W 270
#define TS_H 248
#define TP_X 296
#define TP_Y 46
#define TP_W 326
#define TP_H 248
#define FIG_SC 2.8f
#define PITCH_LO 0.08f
#define PITCH_HI 0.42f
#define ROW_H 18
static int tr_ch=0, tr_ret=GS_CAMP, tr_sel=0;
static Sprite tr_img;
static float tr_yaw=-0.45f, tr_pitch=0.26f, tr_manual=0, tr_slide=0, tr_t=0, tr_flash=0, tr_hop=0, tr_spin=0, tr_lbl=0, tr_strip=0;
static float tr_rowfx[TR_COUNT];
static int tr_lbl_k=0, tr_lbl_l=0;
static bool tr_drag=false, tr_dirty=true; static float tr_lmx=0, tr_lmy=0;
typedef struct { float x,y,vx,vy,t,life; uint32_t c; int big; } TPart;
static TPart tp[220]; static int ntp=0;
static const uint32_t RAINBOW[6]={HEX(0xff6a6a),HEX(0xffb84a),HEX(0xf4f06a),HEX(0x7ae88a),HEX(0x6ad8ff),HEX(0xb48aff)};

void training_open(int ch,int ret){ tr_ch=clampi(ch,0,CH_COUNT-1); tr_ret=ret; goto_state(GS_TRAINING); }
bool training_busy(void){ return tr_flash>0; }
void training_enter(void){
  tr_slide=0; tr_flash=0; tr_hop=0; tr_spin=0; tr_lbl=0; ntp=0; tr_drag=false; tr_t=0; tr_manual=0; tr_yaw=-0.45f; tr_pitch=0.26f; tr_dirty=true;
  for(int k=0;k<TR_COUNT;k++) tr_rowfx[k]=0;
  tr_strip=(float)tr_ch;
  if(!tr_img.px){ tr_img=spr_alloc(TS_W,TS_H); tr_img.em=(uint32_t*)calloc((size_t)TS_W*TS_H,4); }
}
static void part(float x,float y,float vx,float vy,float life,uint32_t c,int big){
  if(ntp>=COUNTOF(tp)) return; TPart *p=&tp[ntp++]; p->x=x; p->y=y; p->vx=vx; p->vy=vy; p->t=0; p->life=life; p->c=c; p->big=big;
}
static void parts_update(float dt){
  for(int i=0;i<ntp;){ TPart *p=&tp[i]; p->t+=dt; if(p->t>=p->life){ tp[i]=tp[--ntp]; continue; }
    p->x+=p->vx*dt; p->y+=p->vy*dt; p->vy+=(p->big?-10:26)*dt; p->vx*=1-dt*1.2f; i++; }
}
/* eclats de prisme : petits losanges colores qui scintillent */
static void parts_draw(void){
  for(int i=0;i<ntp;i++){ TPart *p=&tp[i]; float k=1-p->t/p->life; int a=(int)(255*minf(1,k*1.6f)); int x=(int)p->x, y=(int)p->y;
    float tw=0.6f+0.4f*sinf(p->t*18+i);
    if(p->big){ gfx_disc_add(x,y,3,p->c,(int)(a*0.35f)); gfx_padd(x,y,HEX(0xffffff),a); gfx_padd(x-1,y,p->c,(int)(a*tw)); gfx_padd(x+1,y,p->c,(int)(a*tw));
      gfx_padd(x,y-1,p->c,(int)(a*tw)); gfx_padd(x,y+1,p->c,(int)(a*tw)); gfx_padd(x,y-2,p->c,a/3); gfx_padd(x,y+2,p->c,a/3); }
    else { gfx_padd(x,y,p->c,a); gfx_padd(x+1,y,p->c,a/2); gfx_padd(x,y+1,p->c,(int)(a*tw*0.5f)); } }
}
static void change_hero(int d){
  tr_ch=((tr_ch+d)%CH_COUNT+CH_COUNT)%CH_COUNT; tr_slide= d>0?1.0f:-1.0f; sfx(SFX_HOVER);
  tr_lbl=0; tr_hop=0; tr_spin=0; for(int k=0;k<TR_COUNT;k++) tr_rowfx[k]=0;   /* l'annonce du dernier achat concernait l'autre heros */
}
static void go_back(void){ sfx(SFX_CLICK); goto_state(tr_ret); }
static void do_train(void){
  int ch=tr_ch, k=tr_sel, L=train_level(ch,k);
  if(!S.unlocked[ch]){ sfx(SFX_DENY); toast("Ce héros n'a pas encore rejoint le campement"); return; }
  if(L>=TRAIN_CAP){ sfx(SFX_DENY); return; }
  int c=train_cost(L);
  if(S.prisms<c){ sfx(SFX_DENY); toast("Pas assez de prismes : battez des gardiens"); return; }
  S.prisms-=c; S.train[ch][k]=(uint8_t)(L+1); save_write(); meta_check();
  sfx(SFX_UPGRADE); sfx(SFX_CRYSTAL); rumble(0.35f,0.25f);
  tr_flash=1; tr_hop=1; tr_spin=9.0f; tr_lbl=2.2f; tr_lbl_k=k; tr_lbl_l=L+1; tr_rowfx[k]=1;
  /* eclats de prisme qui montent autour de la figurine */
  float cx=TS_X+TS_W*0.5f, cy=TS_Y+TS_H-36.0f;
  for(int q=0;q<90;q++){ float a=rfrand()*TAU, rr=rfrange(18,70);
    part(cx+cosf(a)*rr,cy+sinf(a)*rr*0.25f,-sinf(a)*rfrange(10,40),rfrange(-120,-40),rfrange(0.7f,1.6f),RAINBOW[q%6],q%3==0); }
}

/* fond de la scene : alcove sombre, halo a la couleur du heros, poussiere de prisme */
static void stage_backdrop(int ch){
  for(int j=0;j<TS_H;j++) gfx_hline(TS_X,TS_X+TS_W-1,TS_Y+j,col_lerp(HEX(0x22162a),HEX(0x0b070e),(float)j/TS_H));
  uint32_t hc=CHARS[ch].col|0xFF000000u;
  gfx_glow(TS_X+TS_W*0.5f,TS_Y+TS_H*0.48f,150,col_lerp(HEX(0x6a3a7a),hc,0.35f),0.30f);
  gfx_glow(TS_X+TS_W*0.5f,TS_Y+TS_H*0.80f,80,col_lerp(HEX(0xffc070),hc,0.5f),0.12f);
  /* rayon de lumiere venu d'en haut */
  for(int j=0;j<TS_H-30;j++){ float k=(float)j/(TS_H-30); int hw=(int)(26+k*58); int a=(int)(16*(1-k*0.6f));
    gfx_rect_a(TS_X+TS_W/2-hw,TS_Y+j,hw*2,1,HEX(0xfff0d8),a/2); gfx_rect_a(TS_X+TS_W/2-hw/2,TS_Y+j,hw,1,HEX(0xfff0d8),a/2); }
  for(int i=0;i<36;i++){ uint32_t h=(uint32_t)(i*2654435761u); float ph=fmodf(tr_t*0.05f*(1+(h&3))+(h>>8)%100/100.0f,1.0f);
    int x=TS_X+4+(int)((h>>12)%(TS_W-8)), y=TS_Y+TS_H-6-(int)(ph*(TS_H-12)); gfx_padd(x,y,RAINBOW[i%6],(int)(70*sinf(ph*PI))); }
}
static void arrow(int x,int y,int dir,bool hot){
  uint32_t c= hot?HEX(0xffe08a):HEX(0xc89458);
  for(int k=0;k<2;k++){ float o=k?0:1.5f; uint32_t cc=k?c:HEX(0x0a0608);
    if(dir<0) gfx_tri((float)x+8+o,(float)y-10-o,(float)x+8+o,(float)y+10+o,(float)x-4-o,(float)y,cc);
    else gfx_tri((float)x-8-o,(float)y-10-o,(float)x-8-o,(float)y+10+o,(float)x+4+o,(float)y,cc); }
}
/* figurine d'un heros pas encore venu : statue sombre */
static void silhouette(Sprite *s){
  for(int i=0;i<s->w*s->h;i++){ uint32_t c=s->px[i]; if(!(c>>24)) continue;
    s->px[i]=(c&0xFF000000u)|(col_lerp(c|0xFF000000u,HEX(0x0e0914),0.86f)&0xFFFFFFu); }
  if(s->em) memset(s->em,0,(size_t)s->w*s->h*4);
}
static void stage(float dt){
  int ch=tr_ch; bool un=S.unlocked[ch];
  stage_backdrop(ch);
  const VoxModel *m=hero_model(ch);
  float hop= tr_hop>0 ? sinf((1-tr_hop)*PI)*12 : 0;
  float cy=TS_H-14-28*sinf(tr_pitch)*FIG_SC;
  VoxCam cam={tr_yaw,tr_pitch,FIG_SC,TS_W*0.5f+tr_slide*TS_W*0.6f,cy-hop};
  /* ombre au sol */
  gfx_ellipse_a((int)(TS_X+cam.cx),(int)(TS_Y+cy+3),(int)(84-hop*1.5f),(int)(16-hop*0.3f),HEX(0x000000),110);
  /* la figurine tourne : une image sur deux suffit, sauf pendant les animations */
  { static int fc=0, lastch=-1;
    bool need=(fc++&1)==0 || fabsf(tr_slide)>0.002f || ch!=lastch || tr_drag || tr_hop>0 || tr_spin>0.5f || tr_dirty;
    if(need){ vox_set_light(-0.45f,0.5f,0.75f); vox_render(m,tr_t,0,&cam,&tr_img); vox_set_light(0,0,0); if(!un) silhouette(&tr_img); lastch=ch; tr_dirty=false; } }
  vox_blit(&tr_img,TS_X,TS_Y,0.9f);
  if(tr_flash>0) gfx_rect_add(TS_X,TS_Y,TS_W,TS_H,HEX(0xe8f4ff),(int)(tr_flash*tr_flash*80));
  { int ox0=gT->cx0, oy0=gT->cy0, ox1=gT->cx1, oy1=gT->cy1; gfx_clip(TS_X,TS_Y,TS_X+TS_W,TS_Y+TS_H);
    /* scintillement du prisme autour du socle */
    for(int i=0;i<10;i++){ float a=tr_t*0.9f+i*TAU/10; float px=TS_X+cam.cx+cosf(a)*72, py=TS_Y+cy+sinf(a)*14-4-(i&1)*6;
      if(sinf(a)>-0.2f || !un) continue; gfx_padd((int)px,(int)py,RAINBOW[i%6],(int)(150+100*sinf(tr_t*4+i))); }
    parts_draw();
    gT->cx0=ox0; gT->cy0=oy0; gT->cx1=ox1; gT->cy1=oy1; }
  orn_frame(TS_X,TS_Y,TS_W,TS_H,0);
  /* etiquettes */
  { char t[96];
    if(un){ int need; int xp=hero_rank_xp(ch,&need); snprintf(t,sizeof t,"Rang %d",hero_rank(ch)); text_draw(TS_X+10,TS_Y+8,t,HEX(0xe8d8c8),1,AL_LEFT,TX_OUTLINE);
      if(need>0) ui_bar(TS_X+10,TS_Y+20,56,3,(float)xp/need,HEX(0xffc040),HEX(0x1e1828)); }
    snprintf(t,sizeof t,"%d / %d",ch+1,CH_COUNT); text_draw(TS_X+TS_W-10,TS_Y+8,t,HEX(0x9a8a90),1,AL_RIGHT,TX_OUTLINE);
    if(!un){ int px=TS_X+TS_W/2, py=TS_Y+TS_H/2-30; orn_padlock(px,py);
      text_draw(px,py+14,"Héros verrouillé",HEX(0xe8d8c8),1,AL_CENTER,TX_OUTLINE); }
    if(tr_lbl>0 && un){ float k=clampf(tr_lbl/2.2f,0,1); int dy=(int)((1-k)*-10); int yy=TS_Y+TS_H-52+dy;
      snprintf(t,sizeof t,"%s niveau %d !",ATTR_N[tr_lbl_k],tr_lbl_l);
      char e[64]; train_effect(tr_lbl_k,tr_lbl_l,e,sizeof e);
      int tw=maxi(text_w(t,2),text_w(e,1))+24; gfx_rect_a(TS_X+TS_W/2-tw/2,yy-4,tw,38,HEX(0x0a0608),(int)(170*minf(1,tr_lbl*2)));
      text_draw(TS_X+TS_W/2,yy,t,col_lerp(TR_COL[tr_lbl_k]|0xFF000000u,HEX(0xffffff),0.35f+k*0.3f),2,AL_CENTER,TX_OUTLINE);
      text_draw(TS_X+TS_W/2,yy+21,e,HEX(0xf0e6d0),1,AL_CENTER,TX_OUTLINE); } }
  /* fleches */
  { int ay=TS_Y+TS_H/2; bool hl=mouse_in(TS_X+2,ay-18,26,36), hr=mouse_in(TS_X+TS_W-28,ay-18,26,36);
    arrow(TS_X+14,ay,-1,hl); arrow(TS_X+TS_W-14,ay,1,hr);
    if(A.pad){ pad_badge(TS_X+6,ay+16,BG_L1); pad_badge(TS_X+TS_W-24,ay+16,BG_R1); }
    if(A.click && hl) change_hero(-1); else if(A.click && hr) change_hero(1); }
  (void)dt;
}

/* couleur des pastilles selon le palier (dizaine de niveaux) */
static uint32_t tier_col(int k,int tier,int i){
  if(tier<=0) return TR_COL[k]|0xFF000000u;
  if(tier==1) return HEX(0xffd84a);
  if(tier==2) return HEX(0xd8f4ff);
  return RAINBOW[(i+tier)%6]|0xFF000000u;
}
static void stat_icon(int k,float cx,float cy,float sz,bool dim){
  DrawOpt o=dopt(); if(dim) o.tint=HEX(0x6a6070);
  icon_draw(ICON_PAS(TR_ICON[k]),cx,cy,sz,&o);
}
static void panel(void){
  int ch=tr_ch; bool un=S.unlocked[ch]; const CharDef *cd=&CHARS[ch]; char t[200];
  for(int j=0;j<TP_H;j++) gfx_hline(TP_X,TP_X+TP_W-1,TP_Y+j,col_lerp(HEX(0x2a1a22),HEX(0x150c12),(float)j/TP_H));
  orn_frame(TP_X,TP_Y,TP_W,TP_H,0);
  int cx=TP_X+TP_W/2, y=TP_Y+6;
  gothic_title(cx,y,cd->name,gothic_w(cd->name,24)<=TP_W-20?24:12); y+=29;
  if(un) snprintf(t,sizeof t,"%s   ^7-^0   Rang %d",cd->title,hero_rank(ch)); else snprintf(t,sizeof t,"%s",cd->title);
  text_draw(cx,y,t,HEX(0xd8c8b8),1,AL_CENTER,TX_SHADOW); y+=13;
  gfx_hline(TP_X+12,TP_X+TP_W-13,y,HEX(0x5a3a2a)); y+=5;
  text_draw(TP_X+12,y,"ATTRIBUTS",HEX(0xc9a24a),1,AL_LEFT,TX_SHADOW);
  snprintf(t,sizeof t,"^7Niveaux :^0 %d",train_total(ch)); text_draw(TP_X+150,y,t,HEX(0xd8d0e4),1,AL_LEFT,0);
  text_draw(TP_X+TP_W-12,y,"Coût",HEX(0x9a8a90),1,AL_RIGHT,0);
  y+=12;
  /* les six attributs */
  int rx=TP_X+6, rw=TP_W-12;
  for(int k=0;k<TR_COUNT;k++){
    int ry=y+k*ROW_H; int L=train_level(ch,k); bool sel=k==tr_sel;
    if(A.mouse_moved && mouse_in(rx,ry,rw,ROW_H-1) && tr_sel!=k){ tr_sel=k; sel=true; sfx(SFX_HOVER); }
    if(A.click && mouse_in(rx,ry,rw,ROW_H-1) && tr_sel!=k){ tr_sel=k; sel=true; sfx(SFX_HOVER); }
    if(sel){ for(int j=0;j<ROW_H-1;j++) gfx_hline(rx,rx+rw-1,ry+j,col_lerp(HEX(0x56303a),HEX(0x2e1a22),(float)j/(ROW_H-1)));
      gfx_frame(rx,ry,rw,ROW_H-1,HEX(0xc89458)); gfx_hline(rx+1,rx+rw-2,ry+1,HEX(0x7a4a3a));
      gfx_tri((float)rx-5,(float)ry+4,(float)rx-5,(float)ry+12,(float)rx-1,(float)ry+8,HEX(0xffe08a)); }
    else if(k&1) gfx_rect_a(rx,ry,rw,ROW_H-1,HEX(0x000000),40);
    if(tr_rowfx[k]>0) gfx_rect_add(rx,ry,rw,ROW_H-1,TR_COL[k],(int)(tr_rowfx[k]*120));
    stat_icon(k,(float)rx+13,(float)ry+8.5f,18,!un);
    text_draw(rx+26,ry+5,ATTR_N[k],un?(sel?HEX(0xfff0dc):HEX(0xe8d8c8)):HEX(0x7a6a70),1,AL_LEFT,TX_SHADOW);
    /* niveaux : dix pastilles par palier ; chaque palier de dix change de couleur (or, argent, arc-en-ciel) */
    int px0=rx+86, f= L>0 ? (L-1)%10+1 : 0, tier= L>0 ? (L-1)/10 : 0, nexti=L%10, nextt=L/10;
    for(int i=0;i<10;i++){ int px=px0+i*7; gfx_rect(px,ry+6,6,6,HEX(0x0a0608));
      uint32_t c;
      if(i<f) c=col_lerp(tier_col(k,tier,i),HEX(0xffffff),0.15f);
      else if(tier>0) c=col_lerp(HEX(0x3a3242),tier_col(k,tier-1,i),0.42f);
      else c=HEX(0x3a3242);
      if(i==nexti && sel && un) c=col_lerp(c,tier_col(k,nextt,i),0.3f+0.35f*sinf(tr_t*6));
      gfx_rect(px+1,ry+7,4,4,c); if(i<f) gfx_pset(px+1,ry+7,HEX(0xffffff)); }
    if(L>0){ snprintf(t,sizeof t,"%d",L); text_draw(px0+73,ry+5,t,tier>=1?tier_col(k,tier,0):HEX(0xd8d0e4),1,AL_LEFT,TX_SHADOW); }
    /* bonus actuel */
    train_short(k,L,t,sizeof t); text_draw(rx+rw-58,ry+5,t,L>0?HEX(0x8be05a):HEX(0x6a5a60),1,AL_RIGHT,0);
    /* cout du niveau suivant */
    if(!un) text_draw(rx+rw-6,ry+5,"-",HEX(0x6a5a60),1,AL_RIGHT,0);
    else { int c=train_cost(L); snprintf(t,sizeof t,IC_PRISM" %s%d",S.prisms>=c?"":"^3",c); text_draw(rx+rw-6,ry+5,t,HEX(0xf0f4ff),1,AL_RIGHT,TX_SHADOW); }
  }
  y+=TR_COUNT*ROW_H+3;
  gfx_hline(TP_X+12,TP_X+TP_W-13,y,HEX(0x5a3a2a)); y+=5;
  /* l'attribut choisi */
  { int k=tr_sel, L=train_level(ch,k);
    snprintf(t,sizeof t,"^1%s^0 : %s",ATTR_N[k],TR_DESC[k]); y+=text_wrap(TP_X+12,y,TP_W-24,t,HEX(0xd8d0e4),1,0,11)+2;
    char e1[64], e2[64]; train_effect(k,L,e1,sizeof e1);
    train_effect(k,L+1,e2,sizeof e2); snprintf(t,sizeof t,"^7Actuel :^0 %s   ^7"IC_ARROW" Niveau %d :^2 %s",e1,L+1,e2);
    text_draw(TP_X+12,y,t,HEX(0xe8e0e8),1,AL_LEFT,0);
    /* cout + bouton */
    int by=TP_Y+TP_H-30, bw=150, bx=TP_X+TP_W-14-bw;
    if(!un){
      if(cd->unlock_ach) snprintf(t,sizeof t,"^7Se débloque avec un défi"); else snprintf(t,sizeof t,"^7Se débloque pour ^6"IC_GEM" %d",cd->cost);
      text_draw(TP_X+14,by+7,t,HEX(0xd8d0e4),1,AL_LEFT,0);
      orn_button(bx,by,bw,22,"Verrouillé",false,false);
    } else {
      int c=train_cost(L); bool ok=S.prisms>=c;
      snprintf(t,sizeof t,"Coût :  "IC_PRISM" %s%d^0  prisme%s",ok?"":"^3",c,c>1?"s":""); text_draw(TP_X+14,by+7,t,HEX(0xe8d8c8),1,AL_LEFT,TX_SHADOW);
      if(orn_button(bx,by,bw,22,"   Entraîner",ok,ok)) do_train();
      else if(!ok && A.click && mouse_in(bx,by,bw,22)) toast("Pas assez de prismes : battez des gardiens");
      pad_badge(bx+10,by+5,BG_CONFIRM);
    }
  }
}
static void strip(float dt){
  float d=(float)tr_ch-tr_strip; while(d>CH_COUNT*0.5f) d-=CH_COUNT; while(d<-CH_COUNT*0.5f) d+=CH_COUNT;
  tr_strip+=d*minf(1,dt*12);
  const int SP=44, CY=320; int x0=320-(CH_COUNT*SP)/2+SP/2;
  for(int i=0;i<CH_COUNT;i++){
    bool sel=i==tr_ch, un=S.unlocked[i];
    int w= sel?40:34, h=w; int x=x0+i*SP; int bx=x-w/2, by=CY-h/2;
    for(int j=0;j<h;j++) gfx_hline(bx,bx+w-1,by+j,col_lerp(sel?HEX(0x4a2a34):HEX(0x2a1820),HEX(0x120a10),(float)j/h));
    orn_frame(bx,by,w,h,sel?1:0);
    const Sprite *s=&SPR_LO[CHARS[i].sprite]; DrawOpt op=dopt(); float sc=minf((w-6)/(float)s->w,(h-6)/(float)s->h); op.sx=op.sy=sc; op.flags=DS_NOOCC;
    if(!un) op.tint=HEX(0x302838);
    draw_spr_c(s,(float)x,(float)CY,&op);
    if(un){ char t[8]; int tt=train_total(i); snprintf(t,sizeof t,"%d",tt); text_draw(bx+3,by+h-11,t,tt>0?HEX(0xf0e6d0):HEX(0x8a7a80),1,AL_LEFT,TX_OUTLINE);
      if(hero_can(i)){ float p=0.5f+0.5f*sinf(tr_t*4+i); gfx_tri((float)bx+w-9,(float)by+8,(float)bx+w-3,(float)by+8,(float)bx+w-6,(float)by+3-p,HEX(0x8be05a)); } }
    else { gfx_rect(x-3,CY-2,7,6,HEX(0x2a1608)); gfx_rect(x-2,CY-1,5,4,HEX(0xc88a3a)); gfx_frame(x-2,CY-5,5,4,HEX(0xc88a3a)); }
    if(A.click && mouse_in(bx,by,w,h) && !sel){ int dd=i-tr_ch; change_hero(dd); }
  }
}

void training_frame(float dt){
  tr_t+=dt;
  layer(LY_SCENE);
  gfx_gradient_v(0,0,SCREEN_W,SCREEN_H,HEX(0x140c16),HEX(0x080508));
  if(g_gpu){ GP.vig=0.4f; layer(LY_UI); }
  /* commandes */
  if(A.back){ go_back(); return; }
  if(A.left||A.tab_l) change_hero(-1);
  if(A.right||(A.tab_r && !(g_in&&g_in->key_pressed[KEY_TAB]))) change_hero(1);
  if(A.up){ tr_sel=(tr_sel+TR_COUNT-1)%TR_COUNT; sfx(SFX_HOVER); }
  if(A.down){ tr_sel=(tr_sel+1)%TR_COUNT; sfx(SFX_HOVER); }
  if(A.confirm) do_train();
  /* rotation : automatique, stick droit, glisser a la souris */
  float rx= g_in&&g_in->pad_on ? g_in->rx : 0, ry= g_in&&g_in->pad_on ? g_in->ry : 0;
  if(fabsf(rx)<0.2f) rx=0; if(fabsf(ry)<0.2f) ry=0;
  if(rx!=0||ry!=0){ tr_yaw+=rx*dt*2.6f; tr_pitch=clampf(tr_pitch+ry*dt*0.7f,PITCH_LO,PITCH_HI); tr_manual=2.0f; }
  if(A.mouse_down && (tr_drag || (A.click && mouse_in(TS_X+30,TS_Y,TS_W-60,TS_H)))){
    if(tr_drag){ tr_yaw+=(A.mx-tr_lmx)*0.016f; tr_pitch=clampf(tr_pitch+(A.my-tr_lmy)*0.005f,PITCH_LO,PITCH_HI); }
    tr_drag=true; tr_manual=2.0f;
  } else tr_drag=false;
  tr_lmx=A.mx; tr_lmy=A.my;
  if(tr_manual>0) tr_manual-=dt; else tr_yaw+=dt*0.6f;
  if(tr_spin>0){ tr_yaw+=tr_spin*dt; tr_spin*=expf(-dt*2.8f); if(tr_spin<0.05f) tr_spin=0; }
  tr_slide=approachf(tr_slide*(1-minf(1,dt*14)),0,dt*0.4f);
  if(tr_flash>0) tr_flash-=dt*2.0f;
  if(tr_hop>0) tr_hop-=dt*2.2f; if(tr_hop<0) tr_hop=0;
  if(tr_lbl>0) tr_lbl-=dt;
  for(int k=0;k<TR_COUNT;k++) if(tr_rowfx[k]>0) tr_rowfx[k]-=dt*1.6f;
  parts_update(dt);
  /* cadre et en-tete */
  orn_panel(8,4,624,354);
  { int hx=160, hy=12, hw=320, hh=28;
    for(int j=0;j<hh;j++) gfx_hline(hx,hx+hw-1,hy+j,col_lerp(HEX(0x4a2a24),HEX(0x24120e),(float)j/hh));
    orn_frame(hx,hy,hw,hh,0);
    gothic_title(320,hy+2,"Entraînement",24); }
  { int bx=34, by=18; text_draw(bx-8,by+1,"<",HEX(0xe8d8c8),1,AL_LEFT,TX_OUTLINE); int w=pad_badge(bx,by,BG_BACK);
    if(A.click && mouse_in(bx-10,by-2,w+14,18)){ go_back(); return; } }
  { char t[64]; snprintf(t,sizeof t,IC_PRISM" ^8%d^0  prisme%s",S.prisms,S.prisms>1?"s":""); text_draw(SCREEN_W-22,20,t,HEX(0xc8d8e8),1,AL_RIGHT,TX_OUTLINE); }
  stage(dt);
  panel();
  strip(dt);
  /* aide */
  if(A.pad){ char t[220]; snprintf(t,sizeof t,"L1 / R1 : héros   Haut / Bas : attribut   %s : entraîner   Stick droit : tourner   %s : retour",btn(B_CONFIRM),btn(B_BACK)); text_draw(320,346,t,HEX(0x8a82a0),1,AL_CENTER,0); }
  else text_draw(320,346,"Gauche / Droite : héros   Haut / Bas : attribut   Entrée : entraîner   Glisser la souris : tourner   Échap : retour",HEX(0x8a82a0),1,AL_CENTER,0);
}
