/* GOUFFRE - curiosites du decor (v8) : tresors a briser et pieges a declencher avec les billes.
   Chaque niveau a les siens (deux par niveau), sculptes en voxels comme les batiments du campement :
   le sprite garde ses normales et sa lumiere propre, la carte graphique les eclaire comme le reste.
   Repere : x de gauche a droite (0 = axe), y vers le haut (0 = sol), z vers l'arriere (0 = face avant). */
#include "game.h"

#define S_(c) VC(VM_SOLID,c)
#define G_(c) VC(VM_GLOW,c)
#define M_(c) VC(VM_METAL,c)
#define L_(c) VC(VM_LEAF,c)
#define W_(c) VC(VM_WATER,c)

static const char *const NAMES[BIOME_COUNT][2]={
  {"Urne funéraire","Jarre de naphte"},
  {"Coffre moussu","Vesse-de-loup"},
  {"Coffre de forgeron","Baril de poudre"},
  {"Géode du Gouffre","Œil gonflé"},
  {"Coffre gelé","Stalagmite de givre"},
  {"Coffre englouti","Mine marine"},
  {"Rayon royal","Nid de frelons"},
  {"Fragment d'étoile","Faille instable"},
  {"Bourgeon doré","Fleur à lucioles"},
  {"Coffre envasé","Bulle de méthane"},
  {"Filon de cristal","Lampe à grisou"},
  {"Reliquaire","Miroir solaire"},
};
const char *curio_name(int biome,int kind){ return NAMES[clampi(biome,0,BIOME_COUNT-1)][kind?1:0]; }

/* ---------------- petits volumes ---------------- */
static void chest(int hw,int d,int h,uint32_t wood,uint32_t woodd,uint32_t band,uint32_t lock,uint32_t lid){
  vx_planks(-hw,0,0,hw,h-1,d,wood,woodd);
  for(int k=0;k<3;k++) vx_box(-hw,h+k,k,hw,h+k,d-k,lid);              /* couvercle bombe */
  for(int s=-1;s<=1;s+=2){ int x=s*(hw-2); for(int y=0;y<h+3;y++) for(int z=-1;z<=d+1;z++) vx_paint(x,y,z,band); }   /* ferrures */
  for(int y=0;y<h+3;y++){ vx_paint(-hw,y,0,band); vx_paint(hw,y,0,band); }
  vx_box(-1,h-2,-1,1,h,-1,lock); vx_set(0,h-2,-1,S_(0x1a1018));       /* serrure */
}
static void urn(uint32_t c,uint32_t cd,uint32_t band,uint32_t rim,float zc){
  for(int y=0;y<=11;y++){ float r= y<=8 ? 2.0f+2.7f*sinf((y+1.0f)/10.0f*PI) : (y<=10?1.9f:2.6f);
    vx_cyl(0,zc,r,y,y, (y==4||y==6) ? band : (y==11?rim:((vhash(0,y,3)%4==0)?cd:c))); }
  vx_cyl(0,zc,1.5f,11,11,S_(0x1a1018));
  for(int s=-1;s<=1;s+=2){ vx_box(s*5,6,(int)zc,s*5,9,(int)zc,c); vx_set(s*4,9,(int)zc,c); vx_set(s*4,6,(int)zc,c); }
}
static void prism(float x,float z,int y0,int h,float r,float dx,float dz,uint32_t c,uint32_t cd){   /* cristal (eventuellement penche) */
  for(int k=0;k<h;k++){ float rr=r*(1.0f-(float)k/h)+0.45f; vx_cyl(x+dx*k,z+dz*k,rr,y0+k,y0+k,(k%3==2)?cd:c); }
}
static void rock(float x,float y,float z,float rx,float ry,float rz,uint32_t c,uint32_t cd){ vx_blob(x,y,z,rx,ry,rz,c,cd); }

/* ---------------- les vingt-quatre curiosites ---------------- */
static float build(int b,int k){
  const uint32_t IRON=M_(0x5a5466), GOLD=M_(0xd8a83a), BLACK=S_(0x1a1018);
  if(k==0) switch(b){
    case 0: urn(S_(0xb0704a),S_(0x8a5236),S_(0xe8dcc0),S_(0x7a4a30),5); vx_set(0,11,5,M_(0xf9dc5c)); vx_set(1,11,4,M_(0xf9dc5c)); return 5;
    case 1: chest(6,7,5,S_(0x7a5a34),S_(0x4a3420),M_(0x6a6a5a),GOLD,S_(0x8a6a3e));
      for(int x=-6;x<=6;x++) for(int z=0;z<=7;z++) if(vhash(x,9,z)%3) vx_paint(x,7,z,L_(0x4a9a3e));
      vx_set(-4,8,3,L_(0x6ac04a)); vx_set(3,8,4,L_(0x6ac04a)); vx_set(5,8,2,S_(0xf59ac0)); return 4;
    case 2: chest(6,7,5,M_(0x3a3640),M_(0x2a2630),M_(0x7a7484),G_(0xff8a2a),M_(0x4a4652));
      for(int x=-5;x<=5;x++) vx_paint(x,5,0,G_(0xff7a22));
      for(int x=-6;x<=6;x+=3) vx_paint(x,2,0,M_(0x9a94a6)); return 4;
    case 3: rock(0,3.5f,5,6,4,5,S_(0x3a2a4a),S_(0x2a1e36));
      prism(-2,4,4,6,1.6f,0,-0.1f,G_(0xc89aff),G_(0x8a5ad8)); prism(2,5,4,5,1.4f,0.15f,0,G_(0xe0c8ff),G_(0x9a6ae0)); prism(0,3,3,4,1.2f,-0.1f,-0.15f,G_(0xb07aff),G_(0x7a4ac8)); return 5;
    case 4: chest(6,7,5,S_(0x8a9ab8),S_(0x5a6a88),M_(0xd8e8ff),M_(0xa8d8ff),S_(0xc8d8f0));
      for(int x=-6;x<=6;x++) for(int z=0;z<=7;z++) if(vhash(x,8,z)%2) vx_paint(x,7,z,S_(0xf4f8ff));
      prism(-3,4,8,3,1.0f,0,0,M_(0xd8f0ff),M_(0x9ac8f0)); prism(3,3,8,4,1.1f,0,0,M_(0xe8f6ff),M_(0xa8d8ff)); return 4;
    case 5: chest(6,7,5,S_(0x3a6a62),S_(0x24443e),M_(0x6ab0a0),M_(0xd8b040),S_(0x4a7a70));
      for(int i=0;i<10;i++){ uint32_t h=vhash(i,5,7); vx_paint(-6+(int)(h%13),(int)((h>>4)%7),0,S_(0xe8e0d0)); }
      for(int s=0;s<3;s++){ int x=-5+s*5; vx_line((float)x,1,-1,(float)x+1,9,-1,L_(0x2a6a3a)); } return 4;
    case 6: vx_box(-6,0,1,6,7,7,M_(0xc8841c)); vx_box(-5,8,2,5,8,6,S_(0xf0c878));        /* rayon de cire : alveoles sur la face avant */
      for(int y=0;y<=7;y++) for(int x=-6;x<=6;x++){ int cx=((x+6+((y/2)&1)*2)%4), cy=y%2; if(cx==0||cy==1) continue;
        uint32_t h=vhash(x,y,6); vx_set(x,y,1, h%5==0 ? G_(0xffc040) : (h%3==0 ? M_(0xe8a030) : S_(0x5a3008))); }
      for(int k=0;k<4;k++){ int x=-5+k*3+(int)(vhash(k,2,2)%2); for(int y=8-(int)(vhash(k,3,3)%3)-1;y<=8;y++) vx_set(x,y,0,G_(0xffb030)); }
      return 4;
    case 7: rock(0,1.2f,4,4,1.6f,3.5f,S_(0x2a2440),S_(0x1a1630));
      for(int k=0;k<9;k++){ float r=3.2f*(1.0f-fabsf(k-4)/4.5f)+0.3f; vx_cyl(0,4,r,2+k,2+k,(k==4)?G_(0xffffff):G_(k<4?0xd8d0ff:0xb0a0ff)); } return 4;
    case 8: vx_line(0,0,4,0,6,4,L_(0x3a8a3a)); vx_line(0,2,4,-4,4,3,L_(0x4aa84a)); vx_line(0,3,4,4,5,5,L_(0x4aa84a));
      vx_blob(0,9,4,3.2f,3.4f,3.2f,M_(0xf0c040),M_(0xd89a2a)); vx_set(0,13,4,G_(0xfff4b0)); for(int s=0;s<4;s++) vx_set(s<2?-3:3,7+(s&1)*2,4,L_(0x5ab84a)); return 4;
    case 9: chest(6,7,5,S_(0x5a4a2e),S_(0x3a2e1c),M_(0x5a5a48),M_(0xb09a40),S_(0x6a5838));
      vx_blob(0,1,4,8,2.2f,6,S_(0x4a4026),S_(0x3a3420)); for(int i=0;i<6;i++){ uint32_t h=vhash(i,9,9); vx_paint(-6+(int)(h%13),4+(int)((h>>4)%3),0,S_(0x7a8a3a)); } return 4;
    case 10: rock(0,3,5,6.5f,3.6f,5,S_(0x4a4656),S_(0x34303e));
      prism(-3,4,3,6,1.5f,-0.12f,0,G_(0x9af8ff),G_(0x4ab8e0)); prism(2,5,4,7,1.6f,0.1f,0,G_(0xffb8ec),G_(0xd86ac0)); prism(0,2,2,4,1.1f,0,-0.15f,G_(0xc8fcff),G_(0x6ad8f0)); return 5;
    default: vx_box(-5,0,0,5,5,7,GOLD); vx_box(-4,6,1,4,6,6,M_(0xf0c850)); vx_sphere(0,6.5f,3.5f,3.3f,M_(0xf9dc5c));
      vx_box(-1,2,-1,1,4,-1,G_(0xfff0a0)); for(int x=-5;x<=5;x+=2) vx_paint(x,0,0,M_(0x9a6a1a)); vx_set(0,10,3,G_(0xffffff)); return 3.5f;
  }
  switch(b){
    case 0: vx_cyl(0,4,3.6f,0,0,S_(0x2a2028)); for(int y=1;y<=7;y++) vx_cyl(0,4,y<=5?3.8f:2.6f-(y-6)*0.6f,y,y,S_(0x2e2430));
      vx_box(-1,8,3,1,9,5,S_(0x8a6a40)); vx_box(-1,3,0,1,5,0,G_(0xff4a2a)); vx_set(0,6,0,G_(0xff8a2a)); return 4;
    case 1: vx_cyl(0,4,2.2f,0,1,S_(0xd8c8a0)); vx_blob(0,5,4,4.6f,4.2f,4.6f,S_(0xe8dcb8),S_(0xc8b890));
      for(int i=0;i<12;i++){ uint32_t h=vhash(i,3,3); vx_paint(-4+(int)(h%9),3+(int)((h>>4)%6),(int)((h>>8)%9),S_(0xa89068)); } vx_set(0,9,4,G_(0xc8ff8a)); return 4;
    case 2: vx_cyl(0,4,3.6f,0,8,S_(0x8a5a30)); vx_cyl(0,4,3.7f,1,1,IRON); vx_cyl(0,4,3.7f,7,7,IRON); vx_cyl(0,4,3.8f,4,4,IRON); vx_cyl(0,4,2.6f,8,8,S_(0x5c3a24));
      vx_box(-1,3,0,1,5,0,S_(0xe8e0d0)); vx_set(0,4,-1,BLACK); vx_line(1,9,4,2,11,4,S_(0x3a2a1a)); vx_set(2,12,4,VC(VM_FIRE,0xffb040)); return 4;
    case 3: vx_blob(0,1,4,5,1.6f,4,S_(0x7a2a3a),S_(0x5a1a2a)); vx_sphere(0,5.5f,4,4.4f,S_(0xe8e0d8));
      for(int i=0;i<14;i++){ uint32_t h=vhash(i,7,2); vx_paint(-4+(int)(h%9),2+(int)((h>>4)%8),(int)((h>>8)%8),S_(0xc83a4a)); }
      vx_box(-2,4,0,2,7,0,G_(0xa04aff)); vx_box(-1,5,-1,1,6,-1,BLACK); return 4;
    case 4: rock(0,0.5f,4,5,1.2f,4,S_(0x6a7a9a),S_(0x4a5a7a));
      prism(0,4,1,10,2.0f,0,0,G_(0xb8e8ff),G_(0x7ac0f0)); prism(-3,3,1,6,1.3f,-0.12f,-0.05f,G_(0xd8f4ff),G_(0x8ad0f8)); prism(3,5,1,7,1.4f,0.12f,0.05f,G_(0xc8eeff),G_(0x7ac0f0)); return 4;
    case 5: vx_sphere(0,4.5f,4,3.8f,M_(0x3a3a46)); vx_cyl(0,4,1.4f,0,1,M_(0x2a2a34));
      for(int i=0;i<8;i++){ float a=i*TAU/8; vx_line(cosf(a)*3.5f,4.5f+sinf(a)*3.5f,4,cosf(a)*5.6f,4.5f+sinf(a)*5.6f,4,M_(0x6a6a7a)); }
      vx_line(0,8,4,0,10,4,M_(0x6a6a7a)); vx_line(0,4.5f,0,0,4.5f,-1.5f,M_(0x6a6a7a)); vx_set(0,9,3,G_(0xff3a2a)); return 4;
    case 6: vx_blob(0,6,4,3.8f,5.6f,3.8f,S_(0xb8a888),S_(0x9a8a6a)); for(int y=2;y<=10;y+=2) for(int x=-4;x<=4;x++) for(int z=0;z<=8;z++) vx_paint(x,y,z,S_(0x8a7a5a));
      vx_box(-1,4,0,1,5,0,BLACK); vx_set(3,8,1,S_(0xf0c030)); vx_set(4,8,1,BLACK); return 4;
    case 7: rock(0,0.6f,4,5,1.4f,4,S_(0x1e1a30),S_(0x141020));
      prism(-1,4,1,11,1.3f,0.05f,0,G_(0xe8f0ff),G_(0x9ab0ff)); prism(2,3,1,7,1.0f,0.15f,0,G_(0xd0dcff),G_(0x8090ff)); prism(-3,5,1,6,1.0f,-0.15f,0,G_(0xc0c8ff),G_(0x7080f0)); return 4;
    case 8: vx_line(0,0,4,0,6,4,L_(0x2e7a3a)); vx_line(0,1,4,-4,3,4,L_(0x3a9a4a)); vx_line(0,2,4,4,4,4,L_(0x3a9a4a));
      for(int i=0;i<6;i++){ float a=i*TAU/6; vx_line(0,8,4,cosf(a)*4,8+sinf(a)*1.2f,4+sinf(a)*3,S_(0xf59ac0)); } vx_sphere(0,8,4,1.8f,G_(0xeaff7a)); vx_set(0,11,4,G_(0xffffff)); return 4;
    case 9: vx_blob(0,0.8f,4,6,1.4f,5,S_(0x3a3420),S_(0x2a2618)); vx_sphere(0,5,4,4.2f,W_(0x7ab83a)); vx_sphere(-1.5f,6.5f,2.5f,1.2f,G_(0xd8ffa0)); return 4;
    case 10: vx_box(-3,0,2,3,1,6,IRON); vx_box(-2,2,3,2,7,5,G_(0xffc860)); for(int y=2;y<=7;y++){ vx_set(-3,y,2,IRON); vx_set(3,y,2,IRON); vx_set(-3,y,6,IRON); vx_set(3,y,6,IRON); }
      vx_box(-3,8,2,3,8,6,IRON); vx_line(-2,9,4,0,11,4,IRON); vx_line(0,11,4,2,9,4,IRON); vx_set(0,5,1,G_(0xffffff)); return 4;
    default: vx_box(-1,0,4,1,4,5,M_(0x8a6a2a)); vx_box(-3,0,3,3,0,6,M_(0x6a4a1a));
      for(int y=5;y<=15;y++) for(int x=-5;x<=5;x++){ float dy=y-10.0f; float d=x*x+dy*dy; if(d>27) continue; vx_set(x,y,4, d>18 ? GOLD : (d<6?G_(0xfffcf0):G_(0xffe890))); }
      return 4;
  }
}

static Sprite spr[BIOME_COUNT][2]; static int sax[BIOME_COUNT][2], say[BIOME_COUNT][2]; static bool sok[BIOME_COUNT][2];
const Sprite *curio_sprite(int biome,int kind,int *ax,int *ay){
  biome=clampi(biome,0,BIOME_COUNT-1); kind=kind?1:0;
  if(!sok[biome][kind]){
    VoxModel m; memset(&m,0,sizeof m);
    vx_begin(); float zc=build(biome,kind); vx_end(&m,0,0);
    int a,b; vox_camp_sprite(&m,&spr[biome][kind],&a,&b);
    float sx,sy; vox_camp_point(&m,m.px,m.py,m.fz+zc,&sx,&sy); sax[biome][kind]=(int)floorf(sx); say[biome][kind]=(int)floorf(sy);
    vox_free(&m); sok[biome][kind]=true;
  }
  *ax=sax[biome][kind]; *ay=say[biome][kind]; return &spr[biome][kind];
}

/* planche de controle (tests) : les 24 curiosites, agrandies 4 fois */
void curio_sheet(const char *path){
  const int C=40, Z=4, IW=12*C*Z, IH=2*C*Z; uint8_t *img=(uint8_t*)calloc((size_t)IW*IH*3,1);
  for(int i=0;i<IW*IH;i++){ img[i*3]=58; img[i*3+1]=52; img[i*3+2]=66; }
  for(int b=0;b<BIOME_COUNT;b++) for(int k=0;k<2;k++){ int ax,ay; const Sprite *s=curio_sprite(b,k,&ax,&ay);
    int ox=b*C+C/2-ax, oy=k*C+C-6-ay;
    for(int y=0;y<s->h;y++) for(int x=0;x<s->w;x++){ uint32_t c=s->px[y*s->w+x]; if(!(c>>24)) continue;
      for(int v=0;v<Z;v++) for(int u=0;u<Z;u++){ int X=(ox+x)*Z+u, Y=(oy+y)*Z+v; if(X<0||Y<0||X>=IW||Y>=IH) continue; uint8_t *d=&img[((size_t)Y*IW+X)*3]; d[0]=(uint8_t)(c>>16); d[1]=(uint8_t)(c>>8); d[2]=(uint8_t)c; } } }
  FILE *f=fopen(path,"wb"); if(f){ fprintf(f,"P6 %d %d 255\n",IW,IH); fwrite(img,1,(size_t)IW*IH*3,f); fclose(f); }
  free(img);
}
