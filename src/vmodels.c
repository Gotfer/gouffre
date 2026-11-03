/* GOUFFRE - maquettes voxel des batiments du campement
   Repere local : x de gauche a droite (0 = axe du batiment), y vers le haut (0 = sol), z vers l'arriere (0 = facade).
   Niveau 0 : chantier. Chaque niveau ajoute quelque chose de visible (bld_visual le decrit). */
#include "game.h"

#define S_(c) VC(VM_SOLID,c)
#define F_(c) VC(VM_FLAT,c)
#define G_(c) VC(VM_GLOW,c)
#define M_(c) VC(VM_METAL,c)
#define L_(c) VC(VM_LEAF,c)
#define W_(c) VC(VM_WATER,c)
static const uint32_t
  STONE=S_(0x7a7486), STONE_D=S_(0x4a4554), STONE_L=S_(0x9a94a6), MORTAR=S_(0x3a3444),
  WOOD=S_(0x9a6438), WOOD_D=S_(0x5c3a24), WOOD_DD=S_(0x3a2416), WOOD_L=S_(0xb07a48),
  PLASTER=S_(0xe8e0d0), PLASTER_D=S_(0xc8c0b0),
  LIT=G_(0xffd36a), FRAME=S_(0x2a1c18),
  GOLD=M_(0xd8a83a), GOLD_L=M_(0xf9dc5c), STEEL=M_(0xc9c4d0), IRON=M_(0x5a5466), COPPER=M_(0xc87a3a), BRONZE=M_(0xb08038),
  FIRE=VC(VM_FIRE,0xff8a2a), EMBER=G_(0xff8a2a),
  MARBLE=S_(0xd8d2e0), MARBLE_D=S_(0x8e8898), MARBLE_L=S_(0xece6f2),
  LEAF=L_(0x3fa34d), LEAF_D=L_(0x2a7a3a), DIRT=S_(0x4a3a28),
  RED=F_(0xe0453a), CLOTH_R=S_(0xa8283a), CLOTH_RD=S_(0x6a1424);

/* ---------------- petits elements ---------------- */
static void lantern(int x,int y,int z,uint32_t glow){        /* lanterne murale ; z = surface du mur */
  vx_set(x,y+4,z-1,IRON); vx_box(x,y+4,z-2,x+1,y+4,z-3,IRON);
  vx_box(x,y+1,z-3,x+1,y+3,z-2,glow);
  vx_box(x,y,z-3,x+1,y,z-2,IRON);
}
static void barrel(int cx,int cz,int h,bool water){
  vx_cyl((float)cx,(float)cz,2.05f,0,h-1,WOOD);
  vx_cyl((float)cx,(float)cz,2.05f,1,1,IRON); vx_cyl((float)cx,(float)cz,2.05f,h-2,h-2,IRON);
  vx_cyl((float)cx,(float)cz,1.2f,h-1,h-1,water?W_(0x3a7ab8):WOOD_D);
}
static void crate(int x,int y,int z,int s,uint32_t c,uint32_t cd){
  for(int k=0;k<s;k++) for(int j=0;j<s;j++) for(int i=0;i<s;i++){
    int e=(i==0||i==s-1)+(j==0||j==s-1)+(k==0||k==s-1);
    vx_set(x+i,y+k,z+j,e>=2?cd:c); }
}
static void bench(int x0,int x1,int z){
  vx_box(x0,2,z-1,x1,2,z,WOOD);
  vx_box(x0+1,0,z-1,x0+1,1,z,WOOD_D); vx_box(x1-1,0,z-1,x1-1,1,z,WOOD_D);
}
static void flowers(int x0,int x1,int z0,int z1,int seed){
  static const uint32_t FC[5]={0xf59ac0,0xf9dc5c,0xffffff,0xa35fe0,0xe0453a};
  for(int z=z0;z<=z1;z++) for(int x=x0;x<=x1;x++){
    uint32_t h=vhash(x,seed,z);
    if(vx_get(x,0,z)) continue;
    if(h%4==0){ vx_set(x,0,z,LEAF_D); if((h>>4)%2==0) vx_set(x,1,z,S_(FC[(h>>8)%5])); }
  }
}
static void banner(int x,int ytop,int len,int z,int w,uint32_t c,uint32_t cd,uint32_t emb){   /* banniere pendue a une hampe (en facade, z) */
  vx_box(x-1,ytop,z,x+w,ytop,z,IRON);
  vx_box(x,ytop-len,z,x+w-1,ytop-1,z,c);
  for(int y=ytop-len;y<ytop;y++){ vx_set(x,y,z,cd); vx_set(x+w-1,y,z,cd); }
  vx_set(x+w/2,ytop-len-1,z,c);
  vx_box(x+(w-1)/2,ytop-len/2-1,z-1,x+(w-1)/2,ytop-len/2,z-1,emb);
}
static void flagpole(int x,int z,int h,int fw,int fh,uint32_t c,uint32_t emb){
  vx_box(x,0,z,x,h,z,WOOD_D);
  vx_set(x,h+1,z,GOLD);
  vx_box(x+1,h-fh+1,z,x+fw,h,z,c);
  if(emb) vx_box(x+fw/2,h-fh/2,z,x+fw/2+1,h-fh/2+1,z,emb);
}
static void torch(int x,int y,int z){
  vx_box(x,y,z,x,y+2,z,WOOD_D); vx_set(x,y+3,z,FIRE); vx_set(x,y+4,z,FIRE);
  vx_emit((float)x,(float)y+5,(float)z,EMI_SPARK);
}
static void brazier(int x,int y,int z){    /* coupe de 3x3 sur trepied, y = base */
  vx_set(x-1,y,z-1,IRON); vx_set(x+1,y,z-1,IRON); vx_set(x,y,z+1,IRON);
  vx_box(x,y+1,z,x,y+2,z,IRON);
  vx_box(x-1,y+3,z-1,x+1,y+3,z+1,IRON); vx_box(x-2,y+4,z-2,x+2,y+4,z+2,IRON);
  vx_box(x-1,y+4,z-1,x+1,y+5,z+1,FIRE); vx_set(x,y+6,z,FIRE);
  vx_emit((float)x,(float)y+7,(float)z,EMI_SPARK);
}
static void statue(int x,int z,uint32_t c,uint32_t c2,bool sword){
  vx_box(x-2,0,z-2,x+2,2,z+2,STONE_L); vx_box(x-2,3,z-2,x+2,3,z+2,STONE);
  vx_box(x-1,4,z,x-1,6,z,c); vx_box(x+1,4,z,x+1,6,z,c);
  vx_box(x-1,7,z-1,x+1,10,z+1,c); vx_box(x-2,9,z,x+2,10,z,c);
  vx_box(x,11,z,x,12,z,c2); vx_set(x,13,z,c);
  if(sword){ vx_box(x+2,8,z-1,x+2,15,z-1,STEEL); vx_box(x+1,9,z-1,x+3,9,z-1,GOLD); }
}
static void disc_xy(float cx,float cy,int z,float r,uint32_t c){   /* disque vertical face a l'avant */
  for(int y=(int)floorf(cy-r);y<=(int)ceilf(cy+r);y++) for(int x=(int)floorf(cx-r);x<=(int)ceilf(cx+r);x++){
    float dx=x+0.5f-cx, dy=y+0.5f-cy; if(dx*dx+dy*dy<=r*r) vx_set(x,y,z,c); }
}
static void ring_xy(float cx,float cy,int z,float r0,float r1,uint32_t c){
  for(int y=(int)floorf(cy-r1);y<=(int)ceilf(cy+r1);y++) for(int x=(int)floorf(cx-r1);x<=(int)ceilf(cx+r1);x++){
    float dx=x+0.5f-cx, dy=y+0.5f-cy; float d=dx*dx+dy*dy; if(d<=r1*r1 && d>=r0*r0) vx_set(x,y,z,c); }
}
static void gear(float cx,float cy,int z,float r,uint32_t c,uint32_t hub){
  ring_xy(cx,cy,z,r-1.6f,r,c);
  for(int k=0;k<8;k++){ float a=k*TAU/8; vx_set((int)floorf(cx+cosf(a)*(r+0.8f)),(int)floorf(cy+sinf(a)*(r+0.8f)),z,c); }
  for(int k=0;k<4;k++){ float a=k*TAU/4+0.4f; vx_line(cx-0.5f,cy-0.5f,(float)z,cx-0.5f+cosf(a)*(r-1.5f),cy-0.5f+sinf(a)*(r-1.5f),(float)z,c); }
  disc_xy(cx,cy,z-1,1.5f,hub);
}
/* soubassement de pierre en saillie, ouvert devant les portes */
static void plinth(int x0,int x1,int z0,int z1,int h,int gx0,int gx1){
  vx_stones(x0-1,0,z0-1,x1+1,h,z1+1,STONE_L,STONE_D);
  for(int y=1;y<=h;y++) for(int x=gx0;x<=gx1;x++) vx_set(x,y,z0-1,0);
}
static void paint_layer(int y,int x0,int x1,int z0,int z1,uint32_t c){
  for(int z=z0;z<=z1;z++) for(int x=x0;x<=x1;x++) vx_paint(x,y,z,c);
}
/* chantier : fondations, murs commences, echafaudage, materiaux, panneau */
static float site(int x0,int x1,int D,int H,uint32_t sign){
  vx_stones(x0,0,0,x1,1,D-1,STONE_L,STONE_D);
  vx_bricks(x0,2,D-3,x1,6,D-1,STONE,STONE_D);
  vx_bricks(x0,2,0,x0+2,4,D-1,STONE,STONE_D);
  int px[2]={x0-1,x1+1}, pz[2]={-1,D};
  for(int i=0;i<2;i++) for(int j=0;j<2;j++) vx_box(px[i],0,pz[j],px[i],H,pz[j],WOOD_D);
  vx_box(x0-1,H,-1,x1+1,H,-1,WOOD); vx_box(x0-1,H,D,x1+1,H,D,WOOD); vx_box(x0-1,H,-1,x0-1,H,D,WOOD); vx_box(x1+1,H,-1,x1+1,H,D,WOOD);
  vx_box(x0-1,H/2,-3,x1+1,H/2,-2,WOOD);
  vx_box(x0-1,0,-3,x0-1,H/2-1,-3,WOOD_D); vx_box(x1+1,0,-3,x1+1,H/2-1,-3,WOOD_D);
  vx_line((float)x0,1,-1,(float)(x0+x1)/2,(float)H-1,-1,WOOD_D);
  vx_line((float)x1,1,-1,(float)(x0+x1)/2+1,(float)H-1,-1,WOOD_D);
  for(int y=0;y<=H/2+3;y++){ vx_set(x1-4,y,-4,WOOD_D); vx_set(x1-1,y,-4,WOOD_D); if(y%2==0){ vx_set(x1-3,y,-4,WOOD); vx_set(x1-2,y,-4,WOOD); } }
  vx_box(x0+2,0,-8,x0+10,0,-6,WOOD); vx_box(x0+3,1,-8,x0+9,1,-6,WOOD_L); vx_box(x0+4,2,-7,x0+8,2,-7,WOOD);
  vx_box(x1-10,0,-8,x1-7,1,-6,STONE_L); vx_box(x1-9,2,-7,x1-8,2,-7,STONE);
  vx_box(0,0,-6,0,5,-6,WOOD_D); vx_box(-3,4,-7,3,8,-7,WOOD); vx_box(-2,5,-8,2,7,-8,sign);
  return D*0.5f;
}

/* ---------------- 0 : forge ---------------- */
static float m_forge(int L){
  const int x0=-19,x1=18,D=22,H=19;
  if(L==0) return site(x0,x1,D,22,S_(0x8f2233));
  uint32_t rc= L>=10?S_(0xb42c3c):S_(0x8f2233), rd= L>=10?S_(0x6a1424):S_(0x5a1420);
  VBox w={x0,0,0,x1,H,D-1};
  vx_bricks(x0,0,0,x1,H,D-1,STONE,STONE_D);
  if(L>=7) plinth(x0,x1,0,D-1,2,-6,5);
  vx_roof(x0,x1,H+1,0,D-1,15,1,rc,rd,2);
  if(L>=10) paint_layer(H+15,x0-3,x1+3,-3,D+2,GOLD);
  if(L>=15) paint_layer(H+1,x0-3,x1+3,-3,D+2,GOLD);
  /* cheminee */
  vx_bricks(11,0,13,15,38,17,STONE_D,MORTAR); vx_box(10,39,12,16,39,18,STONE_L); vx_box(11,39,13,15,39,17,L>=15?EMBER:FRAME);
  vx_emit(13,41,15,EMI_SMOKE); if(L>=15) vx_emit(13,40,15,EMI_SPARK);
  /* four */
  vx_face_rect(&w,F_FRONT,12,0,25,14,0,L>=15?GOLD:S_(0x2a1810));
  vx_face_rect(&w,F_FRONT,13,0,24,13,0,0); vx_face_rect(&w,F_FRONT,13,0,24,13,1,0);
  vx_face(&w,F_FRONT,13,13,0,S_(0x2a1810)); vx_face(&w,F_FRONT,24,13,0,S_(0x2a1810));
  vx_face_rect(&w,F_FRONT,13,0,24,13,2,S_(0x2a1810));
  vx_face_rect(&w,F_FRONT,15,0,22,8,2,FIRE); vx_face_rect(&w,F_FRONT,16,0,21,4,2,G_(0xffd36a));
  vx_window(&w,F_FRONT,4,8,5,6,LIT,FRAME);
  vx_window(&w,F_LEFT,8,8,5,6,LIT,FRAME); vx_window(&w,F_RIGHT,5,8,5,6,LIT,FRAME); vx_window(&w,F_BACK,22,8,5,6,LIT,FRAME);
  vx_door(&w,F_BACK,8,0,6,11,WOOD,WOOD_D);
  if(L>=2){ vx_cyl(-14,-4,2.05f,0,2,WOOD_D); vx_box(-15,3,-5,-14,3,-4,IRON); vx_box(-16,4,-5,-13,4,-4,IRON); vx_box(-17,5,-5,-12,5,-4,STEEL); vx_set(-18,5,-5,STEEL); }
  if(L>=3) barrel(10,-3,6,true);
  if(L>=4){ lantern(-9,8,0,LIT); lantern(7,8,0,LIT); }
  if(L>=5){ for(int x=-28;x<=-20;x++){ int y=12+(x+28)/2; vx_box(x,y,-2,x,y,16,(x&1)?WOOD:WOOD_L); }
    vx_box(-27,0,-1,-27,11,-1,WOOD_D); vx_box(-27,0,15,-27,11,15,WOOD_D);
    vx_box(-24,3,2,-21,3,12,WOOD); vx_box(-24,0,3,-24,2,3,WOOD_D); vx_box(-24,0,11,-24,2,11,WOOD_D);
    vx_box(-23,4,5,-23,4,8,STEEL); vx_set(-22,4,10,IRON); vx_set(-22,5,10,IRON); }
  if(L>=6){ vx_box(-10,1,-3,-7,1,-1,WOOD_D); vx_box(-10,2,-3,-7,2,-1,S_(0x8a5a3a)); vx_box(-9,3,-3,-7,3,-1,WOOD_D); vx_set(-6,2,-1,IRON); vx_set(-11,2,-2,WOOD_D); }
  if(L>=8){ vx_bricks(-16,0,14,-12,36,18,STONE_D,MORTAR); vx_box(-17,37,13,-11,37,19,STONE_L); vx_box(-16,37,14,-12,37,18,L>=15?EMBER:FRAME); vx_emit(-14,39,16,EMI_SMOKE); }
  if(L>=9){ vx_box(10,5,-1,17,5,-1,WOOD_D); vx_box(10,15,-1,17,15,-1,WOOD_D); vx_box(10,5,-1,10,15,-1,WOOD_D); vx_box(17,5,-1,17,15,-1,WOOD_D);
    for(int k=0;k<3;k++){ int x=11+k*3; vx_box(x,7,-2,x,14,-2,STEEL); vx_box(x-1,8,-2,x+1,8,-2,GOLD); vx_set(x,7,-2,S_(0x3a2416)); } }
  if(L>=11){ vx_bricks(19,0,3,30,11,16,STONE_D,MORTAR);
    for(int x=19;x<=31;x++){ int y=12-(x-19)/4; vx_box(x,y,2,x,y,17,(x&1)?IRON:STONE); }
    VBox a={19,0,3,30,11,16};
    vx_face_rect(&a,F_FRONT,3,0,8,7,0,0); vx_face_rect(&a,F_FRONT,3,0,8,7,1,0); vx_face_rect(&a,F_FRONT,3,0,8,7,2,S_(0x2a1810));
    vx_box(23,0,4,26,2,6,IRON); vx_box(24,2,4,25,2,5,G_(0xffb030));
    vx_cyl(28,12,1.2f,11,24,IRON); vx_emit(28,26,12,EMI_SPARK); vx_emit(28,26,12,EMI_SMOKE); }
  if(L>=12){ vx_box(20,2,-8,26,5,-3,WOOD); vx_box(21,3,-7,25,5,-4,0); vx_box(21,3,-7,25,4,-4,WOOD_D);
    vx_box(20,0,-8,21,1,-7,IRON); vx_box(25,0,-8,26,1,-7,IRON); vx_box(20,0,-4,21,1,-3,IRON); vx_box(25,0,-4,26,1,-3,IRON);
    for(int z=-7;z<=-4;z++) for(int x=21;x<=25;x++){ uint32_t h=vhash(x,7,z); vx_set(x,5,z,(h%3==0)?G_(0xff8a2a):((h%3==1)?STONE:G_(0x63d6f2))); if(h%4==0) vx_set(x,6,z,STONE_D); } }
  if(L>=13){ flagpole(-24,-4,26,5,10,CLOTH_R,GOLD); flagpole(23,-9,26,5,10,CLOTH_R,GOLD); }
  if(L>=14){ vx_box(-2,22,-3,-1,29,-3,WOOD_D); vx_box(-5,29,-4,2,32,-3,STEEL); vx_box(-5,29,-4,-5,32,-3,IRON); vx_box(2,29,-4,2,32,-3,IRON); }
  return D*0.5f;
}

/* ---------------- 1 : infirmerie ---------------- */
static float m_infirmerie(int L){
  const int x0=-20,x1=19,D=22,H=21;
  if(L==0) return site(x0,x1,D,24,S_(0xe0453a));
  VBox w={x0,0,0,x1,H,D-1};
  vx_box(x0,0,0,x1,H,D-1,PLASTER);
  for(int x=x0;x<=x1;x++){ vx_set(x,H,0,PLASTER_D); vx_set(x,H,D-1,PLASTER_D); } for(int z=0;z<D;z++){ vx_set(x0,H,z,PLASTER_D); vx_set(x1,H,z,PLASTER_D); }
  for(int y=0;y<=H;y++){ vx_set(x0,y,0,PLASTER_D); vx_set(x1,y,0,PLASTER_D); vx_set(x0,y,D-1,PLASTER_D); vx_set(x1,y,D-1,PLASTER_D); }
  if(L>=7) plinth(x0,x1,0,D-1,2,-5,4);
  vx_roof(x0,x1,H+1,0,D-1,16,1,S_(0x5a7ac8),S_(0x34508a),2);
  if(L>=15){ paint_layer(H+1,x0-3,x1+3,-3,D+2,GOLD); vx_sphere(0,(float)H+18,-1,1.6f,GOLD_L); }
  vx_door(&w,F_FRONT,16,0,8,11,S_(0x6a4a2a),S_(0x4a2a1a));
  uint32_t cross = L>=15 ? G_(0xffe8a0) : (L>=8 ? G_(0xff5a4a) : RED);
  vx_box(-1,12,-1,0,20,-1,cross); vx_box(-4,15,-1,3,17,-1,cross);
  if(L>=15){ vx_box(-2,11,-1,1,11,-1,GOLD); }
  uint32_t glass = LIT;
  if(L>=13){ vx_window(&w,F_FRONT,4,8,5,6,G_(0x6ac8ff),FRAME); vx_window(&w,F_FRONT,31,8,5,6,G_(0xff7aa0),FRAME);
    vx_face(&w,F_FRONT,5,9,1,G_(0xf9dc5c)); vx_face(&w,F_FRONT,33,11,1,G_(0x9af05a)); vx_face(&w,F_FRONT,7,12,1,G_(0xff7aa0)); vx_face(&w,F_FRONT,32,9,1,G_(0x6ac8ff)); }
  else { vx_window(&w,F_FRONT,4,8,5,6,glass,FRAME); vx_window(&w,F_FRONT,31,8,5,6,glass,FRAME); }
  vx_window(&w,F_LEFT,8,8,5,6,glass,FRAME); vx_window(&w,F_RIGHT,8,8,5,6,glass,FRAME); vx_window(&w,F_BACK,6,8,5,6,glass,FRAME); vx_window(&w,F_BACK,29,8,5,6,glass,FRAME);
  if(L>=2) bench(-17,-11,-4);
  if(L>=3){ for(int k=0;k<2;k++){ int u= k?31:4; int x=x0+u; vx_box(x-1,4,-2,x+5,5,-1,WOOD_D); for(int i=0;i<7;i++) vx_set(x-1+i,6,-2+(i&1),(i%3==0)?S_(0xf59ac0):LEAF); } }
  if(L>=4) lantern(-7,8,0,LIT);
  if(L>=5){ vx_box(-31,0,1,-23,0,17,DIRT);
    for(int z=2;z<=16;z+=3) for(int x=-30;x<=-24;x++){ uint32_t h=vhash(x,5,z); vx_set(x,1,z,LEAF); if(h%3==0) vx_set(x,2,z,(h>>5)%2?S_(0xf9dc5c):LEAF_D); }
    for(int z=0;z<=18;z+=3) vx_box(-32,0,z,-32,2,z,WOOD_D); vx_box(-32,2,0,-32,2,18,WOOD); }
  if(L>=6){ for(int k=0;k<2;k++){ int u= k?31:4; int x=x0+u; vx_box(x-2,8,-1,x-1,13,-1,S_(0x3b5fb8)); vx_box(x+5,8,-1,x+6,13,-1,S_(0x3b5fb8)); } }
  if(L>=9){ VBox a={20,0,3,33,13,18}; vx_box(20,0,3,33,13,18,PLASTER); for(int y=0;y<=13;y++){ vx_set(33,y,3,PLASTER_D); vx_set(33,y,18,PLASTER_D); }
    vx_roof(20,33,14,3,18,9,0,S_(0x5a7ac8),S_(0x34508a),1);
    vx_window(&a,F_FRONT,4,5,5,6,glass,FRAME); vx_window(&a,F_RIGHT,5,5,5,6,glass,FRAME); }
  if(L>=10){ vx_box(22,0,-7,22,11,-7,WOOD_D); vx_box(34,0,-7,34,11,-7,WOOD_D); vx_box(23,11,-7,33,11,-7,S_(0xd8d0c0));
    vx_box(24,5,-7,27,10,-7,PLASTER); vx_box(29,6,-7,32,10,-7,S_(0xb8d8f0)); }
  if(L>=11){ vx_cyl_stones(10,-6,3.6f,0,2,STONE_L,STONE); vx_cyl(10,-6,2.4f,2,2,W_(0x4ad8e8)); vx_box(9,2,-7,10,5,-6,STONE_L); vx_set(9,6,-7,G_(0x9af0ff)); vx_set(10,6,-6,G_(0x9af0ff)); vx_emit(10,7,-6,EMI_MAGIC); }
  if(L>=12){ vx_box(-3,36,13,2,37,18,WOOD_D); for(int k=0;k<4;k++){ int x= (k&1)?2:-3, z=(k&2)?18:13; vx_box(x,38,z,x,43,z,WOOD_D); }
    vx_roof_hip(-4,3,44,12,19,4,S_(0x34508a),S_(0x2a3b80),1); vx_box(-1,40,15,0,42,16,GOLD); vx_set(-1,39,15,GOLD); }
  if(L>=14){ for(int k=0;k<2;k++){ int x= k?21:-22; vx_box(x,22,-3,x,30,-3,WOOD_D); vx_box(x+(k?1:-3),27,-3,x+(k?3:-1),30,-3,PLASTER); vx_set(x+(k?2:-2),28,-4,RED); vx_set(x+(k?2:-2),29,-4,RED); } }
  return D*0.5f;
}

/* ---------------- 2 : atelier ---------------- */
static float m_atelier(int L){
  const int x0=-20,x1=19,D=22,H=19;
  if(L==0) return site(x0,x1,D,22,S_(0xc9c4d0));
  VBox w={x0,0,0,x1,H,D-1};
  vx_planks(x0,0,0,x1,H,D-1,WOOD,WOOD_D);
  for(int y=0;y<=H;y++){ vx_set(x0,y,0,WOOD_DD); vx_set(x1,y,0,WOOD_DD); vx_set(x0,y,D-1,WOOD_DD); vx_set(x1,y,D-1,WOOD_DD); }
  if(L>=7) plinth(x0,x1,0,D-1,2,-8,7);
  vx_roof(x0,x1,H+1,0,D-1,15,1,S_(0x6a6a3a),S_(0x4a4a24),2);
  if(L>=12) paint_layer(H+1,x0-3,x1+3,-3,D+2,GOLD);
  /* grande porte double */
  vx_face_rect(&w,F_FRONT,12,0,27,13,0,WOOD_DD);
  vx_face_rect(&w,F_FRONT,13,0,26,12,0,0);
  for(int y=0;y<=12;y++) for(int u=13;u<=26;u++) vx_face(&w,F_FRONT,u,y,1,(u==19||u==20)?WOOD_DD:((u%3==0)?WOOD_D:S_(0x7a4a2a)));
  vx_face(&w,F_FRONT,18,6,0,IRON); vx_face(&w,F_FRONT,21,6,0,IRON);
  vx_window(&w,F_FRONT,3,7,5,6,LIT,FRAME);
  vx_window(&w,F_LEFT,8,8,5,6,LIT,FRAME); vx_window(&w,F_RIGHT,8,8,5,6,LIT,FRAME); vx_window(&w,F_BACK,17,8,5,6,LIT,FRAME);
  if(L>=3){ gear(13.5f,12.5f,-1,5.2f,BRONZE,GOLD); gear(16.5f,5.0f,-1,2.6f,BRONZE,IRON); } else gear(13.5f,12.5f,-1,4.2f,STEEL,IRON);
  if(L>=2){ vx_box(-19,4,-6,-11,4,-3,WOOD); vx_box(-19,0,-6,-19,3,-6,WOOD_D); vx_box(-11,0,-6,-11,3,-6,WOOD_D); vx_box(-19,0,-3,-19,3,-3,WOOD_D); vx_box(-11,0,-3,-11,3,-3,WOOD_D);
    vx_box(-17,5,-5,-14,5,-5,STEEL); vx_box(-13,5,-4,-13,6,-4,WOOD_D); vx_set(-13,7,-4,IRON); }
  if(L>=4){ crate(9,0,-7,4,WOOD_L,WOOD_D); crate(14,0,-6,3,WOOD_L,WOOD_D); crate(10,4,-6,3,WOOD,WOOD_D); }
  if(L>=5){ vx_cyl(-12,16,1.3f,20,40,IRON); vx_cyl(-12,16,1.9f,40,41,IRON); vx_emit(-12,43,16,EMI_SMOKE); }
  if(L>=6){ vx_box(-1,33,-10,0,33,-3,WOOD_D); vx_box(-1,31,-3,0,33,-3,WOOD_D); vx_line(-0.5f,32,-9,-0.5f,25,-9,S_(0xb8a070)); crate(-2,22,-10,3,WOOD_L,WOOD_D); }
  if(L>=8){ vx_box(x0,19,-1,x1,19,-1,COPPER); vx_box(x0+1,10,-1,x0+1,19,-1,COPPER); vx_box(x0,10,-2,x0+2,11,-2,IRON); vx_set(x0+1,12,-2,S_(0xe0453a)); }
  if(L>=9){ disc_xy(0,26.5f,-3,3.6f,GOLD); disc_xy(0,26.5f,-4,2.8f,F_(0xf6f2ea)); vx_box(0,26,-5,0,29,-5,FRAME); vx_box(0,26,-5,2,26,-5,FRAME); }
  if(L>=10){ vx_box(-9,28,18,-8,40,19,WOOD_D); for(int k=0;k<4;k++){ float a=k*TAU/4+0.6f; vx_line(-8.5f,40,17,-8.5f+cosf(a)*8,40+sinf(a)*8,17,k&1?PLASTER:WOOD_L); } vx_box(-9,39,16,-8,41,16,IRON); }
  if(L>=11){ vx_box(-12,0,-2,-10,2,-1,IRON); vx_box(-12,3,-2,-10,7,-1,STEEL); vx_box(-12,8,-2,-10,10,-1,IRON); vx_set(-12,9,-3,G_(0x63d6f2)); vx_set(-10,9,-3,G_(0x63d6f2)); vx_box(-14,5,-2,-13,7,-2,IRON); vx_box(-9,5,-2,-8,7,-2,IRON); }
  if(L>=12){ lantern(-21,15,0,G_(0x8ae8ff)); lantern(19,15,0,G_(0x8ae8ff)); vx_emit(13,13,-2,EMI_MAGIC); }
  return D*0.5f;
}

/* ---------------- 3 : bibliotheque ---------------- */
static float m_biblio(int L){
  const int x0=-16,x1=15,D=20,H=31;
  if(L==0) return site(x0,x1,D,30,S_(0x2a3b80));
  uint32_t BR=S_(0x8a7a9a), BM=S_(0x5a4a6a);
  VBox w={x0,0,0,x1,H,D-1};
  vx_bricks(x0,0,0,x1,H,D-1,BR,BM);
  if(L>=6) plinth(x0,x1,0,D-1,2,-6,5);
  vx_roof(x0,x1,H+1,0,D-1,14,1,S_(0x2a3b80),S_(0x1a2450),2);
  if(L>=12) paint_layer(H+1,x0-3,x1+3,-3,D+2,GOLD);
  /* rosace */
  ring_xy(0,22.5f,0,3.2f,4.6f,FRAME);
  for(int y=18;y<=27;y++) for(int x=-5;x<=4;x++){ float dx=x+0.5f, dy=y+0.5f-22.5f; if(dx*dx+dy*dy<3.2f*3.2f){ vx_set(x,y,0,0);
    uint32_t g=LIT; if(L>=9){ static const uint32_t SG[4]={0xff7aa0,0x6ac8ff,0xf9dc5c,0x9af05a}; g=G_(SG[(vhash(x,y,3)>>3)%4]); }
    vx_set(x,y,1,(x==0||y==22)?FRAME:g); } }
  /* porte en arc */
  vx_door(&w,F_FRONT,11,0,10,11,S_(0x4a2a1a),S_(0x2a1810));
  vx_face(&w,F_FRONT,11,10,1,S_(0x2a1810)); vx_face(&w,F_FRONT,20,10,1,S_(0x2a1810));
  /* vitrine de livres */
  vx_face_rect(&w,F_FRONT,1,2,6,13,0,FRAME); vx_face_rect(&w,F_FRONT,2,3,5,12,0,0);
  for(int y=3;y<=12;y++) for(int u=2;u<=5;u++){ static const uint32_t BK[5]={0xe0453a,0x3fa34d,0x3b7fdc,0xc99a2e,0xa35fe0}; vx_face(&w,F_FRONT,u,y,1,(y%4==2)?WOOD_D:S_(BK[vhash(u,y,1)%5])); }
  vx_window(&w,F_FRONT,24,6,5,8,LIT,FRAME);
  vx_window(&w,F_LEFT,7,8,5,8,LIT,FRAME); vx_window(&w,F_LEFT,7,20,5,7,LIT,FRAME);
  vx_window(&w,F_RIGHT,7,8,5,8,LIT,FRAME); vx_window(&w,F_RIGHT,7,20,5,7,LIT,FRAME); vx_window(&w,F_BACK,13,14,5,8,LIT,FRAME);
  if(L>=2){ for(int k=0;k<3;k++){ static const uint32_t BK[4]={0xe0453a,0x3b7fdc,0xc99a2e,0x3fa34d}; int h=3+k; for(int y=0;y<h;y++) vx_box(8+k*2,y,-3,9+k*2,y,-2,S_(BK[(k+y)%4])); } }
  if(L>=3){ vx_box(-12,0,-4,-12,4,-4,WOOD_D); vx_box(-14,5,-5,-10,5,-4,WOOD); vx_box(-14,6,-5,-13,6,-4,F_(0xf6f2ea)); vx_box(-11,6,-5,-10,6,-4,F_(0xf6f2ea)); vx_set(-12,6,-5,WOOD_D); }
  if(L>=4){ lantern(-9,8,0,G_(0xa8d8ff)); lantern(6,8,0,G_(0xa8d8ff)); }
  if(L>=5) bench(6,13,-6);
  if(L>=7){ vx_cyl_stones(-20,10,5.2f,0,40,BR,BM); vx_cone(-20,10,6.6f,41,12,S_(0x2a3b80),S_(0x1a2450));
    vx_set(-21,30,5,FRAME); vx_set(-20,30,5,LIT); vx_set(-20,31,5,LIT); vx_set(-21,31,5,FRAME);
    vx_set(-25,22,9,LIT); vx_set(-25,23,9,LIT); if(L>=12) vx_box(-20,53,10,-20,56,10,GOLD_L); }
  if(L>=8){ vx_line(8,39,9,16,47,5,COPPER); vx_line(9,39,9,17,47,5,COPPER); vx_box(16,46,4,18,48,6,BRONZE); vx_line(8,39,9,6,33,12,WOOD_D); vx_line(9,39,9,12,33,12,WOOD_D); }
  if(L>=10){ vx_box(-19,0,-7,-17,4,-5,STONE_L); vx_sphere(-17.5f,7.5f,-5.5f,2.6f,S_(0x3b7fdc)); ring_xy(-17.5f,7.5f,-6,2.8f,3.6f,GOLD); vx_set(-18,8,-8,S_(0x3fa34d)); vx_set(-17,6,-8,S_(0x3fa34d)); }
  if(L>=11){ vx_box(0,46,-2,0,52,-2,GOLD); vx_box(-1,50,-2,1,50,-2,GOLD); vx_set(0,53,-2,G_(0xfff0a0)); }
  if(L>=12){ for(int x=x0+2;x<=x1-2;x+=3) vx_set(x,29,-1,G_(0x8ab8ff)); for(int z=2;z<D-2;z+=3){ vx_set(x0-1,29,z,G_(0x8ab8ff)); vx_set(x1+1,29,z,G_(0x8ab8ff)); } vx_emit(0,24,-2,EMI_MAGIC); }
  return D*0.5f;
}

/* ---------------- 4 : tresorerie ---------------- */
static float m_tresor(int L){
  const int x0=-20,x1=19,D=22,H=19;
  if(L==0) return site(x0,x1,D,22,M_(0xc99a2e));
  VBox w={x0,0,0,x1,H,D-1};
  vx_bricks(x0,0,0,x1,H,D-1,S_(0x948ea0),S_(0x5a5466));
  if(L>=6){ vx_stones(x0-1,0,-1,x1+1,2,D,MARBLE,MARBLE_D); for(int y=1;y<=2;y++) for(int x=-7;x<=6;x++) vx_set(x,y,-1,0); }
  vx_roof(x0,x1,H+1,0,D-1,16,1,M_(0xc99a2e),M_(0x8a6a1a),2);
  if(L>=12) paint_layer(H+16,x0-3,x1+3,-3,D+2,GOLD_L);
  /* porte blindee */
  vx_door(&w,F_FRONT,13,0,14,13,WOOD_D,IRON);
  for(int u=13;u<=26;u++){ vx_face(&w,F_FRONT,u,4,1,IRON); vx_face(&w,F_FRONT,u,9,1,IRON); }
  vx_face_rect(&w,F_FRONT,12,14,27,14,-1,GOLD);
  /* piece d'or au fronton */
  disc_xy(0,27.5f,-3,4.2f,L>=12?G_(0xf9dc5c):GOLD_L); ring_xy(0,27.5f,-4,3.0f,4.2f,GOLD); vx_box(0,25,-4,0,30,-4,GOLD);
  for(int k=0;k<2;k++){ int u= k?31:4; vx_window(&w,F_FRONT,u,8,5,6,LIT,FRAME); for(int y=9;y<=12;y++){ vx_face(&w,F_FRONT,u+1,y,0,IRON); vx_face(&w,F_FRONT,u+3,y,0,IRON); } }
  vx_window(&w,F_LEFT,8,8,5,6,LIT,FRAME); vx_window(&w,F_RIGHT,8,8,5,6,LIT,FRAME);
  if(L>=2){ vx_box(13,0,-4,17,2,-2,WOOD_D); vx_box(13,3,-4,17,3,-2,S_(0x7a4a2a)); vx_box(13,0,-4,13,3,-2,GOLD); vx_box(17,0,-4,17,3,-2,GOLD); vx_set(15,2,-5,GOLD_L); }
  if(L>=3){ for(int k=0;k<3;k++){ int x=-19+k*2; vx_box(x,0,-3,x+1,2-(k&1),-2,S_(0xb39060)); vx_set(x,3-(k&1),-3,S_(0x8a6a40)); vx_set(x+1,3-(k&1),-2,GOLD_L); } }
  if(L>=4){ lantern(-21,12,0,G_(0xffe08a)); lantern(19,12,0,G_(0xffe08a)); }
  if(L>=5){ for(int x=x0-2;x<=x1+2;x++){ if(x>=-8&&x<=7) continue; vx_set(x,4,-9,IRON); vx_set(x,1,-9,IRON); if(x%3==0) vx_box(x,0,-9,x,5,-9,IRON); if(x%3==0) vx_set(x,6,-9,GOLD); } }
  if(L>=7){ for(int k=0;k<2;k++){ int x= k?9:-11; vx_cyl((float)x+1,-2.0f,1.6f,3,17,MARBLE); vx_box(x-1,2,-4,x+2,2,0,MARBLE_D); vx_box(x-1,18,-4,x+2,18,0,MARBLE_D); } vx_box(-12,19,-4,11,20,-1,MARBLE_L); }
  if(L>=8) statue(-17,-6,STEEL,IRON,true);
  if(L>=9){ for(int k=0;k<6;k++){ int x=13+(k%3)*2, y=k/3, z=-7+(k&1); vx_box(x,y,z,x+1,y,z,GOLD_L); } }
  if(L>=10){ vx_cyl(0,11,4.5f,36,37,MARBLE); for(int y=0;y<6;y++){ float r=4.6f*sqrtf(1-(y/6.0f)*(y/6.0f)); vx_cyl(0,11,maxf(r,0.8f),38+y,38+y,GOLD); } vx_set(0,44,11,GOLD_L); vx_set(0,45,11,G_(0xfff0a0)); }
  if(L>=11){ banner(-19,19,10,-1,3,S_(0x6a2a8a),S_(0x4a1a6a),GOLD); banner(16,19,10,-1,3,S_(0x6a2a8a),S_(0x4a1a6a),GOLD); }
  if(L>=12){ vx_emit(0,28,-4,EMI_MAGIC); vx_emit(15,3,-6,EMI_MAGIC); }
  return D*0.5f;
}

/* ---------------- 5 : puits aux souhaits ---------------- */
static float m_puits(int L){
  if(L==0) return site(-10,9,16,14,S_(0x63d6f2));
  float cz=7.0f;
  int rimh = L>=3 ? 5 : 4;
  vx_cyl_stones(0,cz,8.6f,0,rimh,L>=3?STONE_L:STONE,STONE_D);
  vx_cyl(0,cz,6.2f,1,rimh,0);
  if(L>=3){ for(int z=-3;z<=17;z++) for(int x=-10;x<=10;x++){ float dx=x+0.5f, dz=z+0.5f-cz; float d=dx*dx+dz*dz; if(d<=8.6f*8.6f && d>6.2f*6.2f) vx_set(x,rimh,z,MARBLE_L); } }
  uint32_t water = L>=6 ? G_(0x4ad8f8) : W_(0x2a5aa8);
  vx_cyl(0,cz,6.2f,1,1,water);
  if(L>=2) for(int k=0;k<7;k++){ uint32_t h=vhash(k,2,9); vx_set((int)(h%9)-4,1,(int)cz-4+(int)((h>>8)%9),G_(0xf9dc5c)); }
  uint32_t post=WOOD_D;
  vx_box(-9,rimh+1,6,-8,21,7,post); vx_box(8,rimh+1,6,9,21,7,post);
  vx_box(-7,16,6,7,16,7,WOOD); vx_box(10,15,6,10,17,6,IRON); vx_box(10,17,6,11,17,6,IRON);
  vx_box(0,9,6,0,15,6,S_(0xb8a070));
  vx_box(-1,6,5,1,8,7,WOOD); vx_box(-1,8,5,1,8,7,IRON); vx_set(0,8,6,water);
  uint32_t rc= L>=7?M_(0xc99a2e):S_(0x8f2233), rd= L>=7?M_(0x8a6a1a):S_(0x5a1420);
  vx_roof(-10,9,22,3,10,9,1,rc,rd,1);
  if(L>=4){ flowers(-12,12,-4,-2,11); flowers(-12,-10,-1,16,12); flowers(10,12,-1,16,13); flowers(-12,12,17,18,14); }
  if(L>=5){ vx_box(0,24,6,0,25,7,IRON); vx_box(-1,21,6,0,23,7,G_(0xffd36a)); vx_box(-1,20,6,0,20,7,IRON); }
  if(L>=6) vx_emit(0,3,cz,EMI_MAGIC);
  if(L>=7){ for(int k=0;k<10;k++){ float a=k*TAU/10; vx_set((int)floorf(cosf(a)*8.0f),rimh-1,(int)floorf(cz+sinf(a)*8.0f),G_(0x8ab8ff)); } vx_emit(0,26,cz,EMI_MAGIC); vx_set(0,32,7,G_(0xfff0a0)); }
  return cz;
}

/* ---------------- 6 : sanctuaire ---------------- */
static float m_sanctuaire(int L){
  if(L==0) return site(-20,19,20,22,S_(0xa35fe0));
  vx_stones(-21,0,-2,20,2,20,STONE_L,STONE_D);
  vx_box(-19,3,0,18,3,18,MARBLE_D);
  vx_box(-12,4,6,11,22,15,MARBLE);
  for(int y=4;y<=22;y+=4) for(int x=-12;x<=11;x++) vx_set(x,y,6,MARBLE_D);
  for(int k=0;k<4;k++){ int x=-15+k*9; for(int z=0;z<2;z++){ int zz= z?17:1;
    vx_cyl((float)x+1,(float)zz+1,1.7f,4,22,MARBLE_L); vx_box(x-1,4,zz-1,x+2,4,zz+2,MARBLE_D); vx_box(x-1,22,zz-1,x+2,22,zz+2,MARBLE_D); } }
  vx_box(-20,23,-1,19,24,19,MARBLE_D); vx_box(-19,23,-1,18,23,-1,MARBLE);
  vx_roof(-20,19,25,-1,19,10,1,MARBLE_L,MARBLE_D,1);
  if(L>=3){ paint_layer(25,-22,21,-3,21,GOLD); vx_box(0,35,-2,0,37,-2,GOLD_L); vx_box(-21,25,-2,-21,27,-2,GOLD_L); vx_box(20,25,-2,20,27,-2,GOLD_L); }
  /* l'orbe */
  vx_box(-2,4,2,1,7,4,MARBLE_D); vx_box(-1,8,2,0,8,4,GOLD);
  float r= L>=3?4.4f:3.2f, oy= L>=3?14.0f:12.0f;
  vx_sphere(0,oy,3.5f,r,G_(0xa35fe0)); vx_sphere(-0.6f,oy+0.6f,2.8f,r*0.5f,G_(0xe0c8ff));
  if(L>=3){ for(int k=0;k<14;k++){ float a=k*TAU/14; vx_set((int)floorf(cosf(a)*7.2f),(int)oy,(int)floorf(3.5f+sinf(a)*7.2f*0.6f),GOLD_L); } vx_emit(0,oy+2,3.5f,EMI_MAGIC); }
  if(L>=2){ brazier(-17,3,-1); brazier(16,3,-1); }
  return 9.5f;
}

/* ---------------- 7 : tour de guet ---------------- */
static float m_tour(int L){
  if(L==0) return site(-10,9,18,26,S_(0xe0453a));
  int H=37+(L>=4?8:0)+(L>=7?8:0);
  VBox w={-10,0,0,9,H,17};
  vx_bricks(-10,0,0,9,H,17,STONE,STONE_D);
  vx_box(-11,H+1,-1,10,H+2,18,STONE_D);
  for(int x=-11;x<=10;x++) for(int z=-1;z<=18;z++){ bool edge=(x==-11||x==10||z==-1||z==18); if(!edge) continue; int t=(x+z+40)%4; if(t<2) vx_box(x,H+3,z,x,H+5,z,STONE); }
  vx_box(-9,H+2,0,8,H+2,16,WOOD_D);
  vx_door(&w,F_FRONT,7,0,6,9,WOOD,WOOD_D);
  for(int f=0;f<4;f++) for(int y=12;y<H-4;y+=12) vx_window(&w,f,7,y,5,6,LIT,FRAME);
  int fh= L>=2 ? 7 : 4, fw= L>=2 ? 9 : 6;
  flagpole(0,8,H+16,fw,fh,S_(0xe0453a),L>=2?GOLD:0);
  if(L>=3){ torch(-11,H+6,-1); torch(10,H+6,-1); torch(-11,H+6,18); torch(10,H+6,18); }
  if(L>=5){ int y=H/2; vx_box(-12,y,-5,11,y,-1,WOOD); for(int x=-12;x<=11;x+=3) vx_box(x,y+1,-5,x,y+3,-5,WOOD_D); vx_box(-12,y+3,-5,11,y+3,-5,WOOD_L); vx_box(-12,y+1,-5,-12,y+3,-1,WOOD_D); vx_box(11,y+1,-5,11,y+3,-1,WOOD_D);
    vx_line(-11,y-1,-1,-11,y-5,-1,WOOD_D); vx_line(10,y-1,-1,10,y-5,-1,WOOD_D); }
  if(L>=6){ vx_box(-7,H+3,12,-7,H+9,12,WOOD_D); vx_box(-3,H+3,12,-3,H+9,12,WOOD_D); vx_box(-8,H+10,11,-2,H+10,13,WOOD_D); vx_box(-6,H+6,12,-4,H+8,12,GOLD); vx_set(-5,H+5,12,GOLD); }
  if(L>=8){ vx_box(-4,H+3,3,3,H+4,10,IRON); vx_box(-3,H+5,4,2,H+7,9,FIRE); vx_box(-2,H+8,5,1,H+9,8,FIRE); vx_emit(0,H+11,6,EMI_SPARK); vx_emit(0,H+12,6,EMI_SMOKE); }
  return 8.5f;
}

/* ---------------- 8 : laboratoire ---------------- */
static float m_labo(int L){
  const int x0=-18,x1=17,D=20,H=21;
  if(L==0) return site(x0,x1,D,24,S_(0x9af05a));
  VBox w={x0,0,0,x1,H,D-1};
  vx_bricks(x0,0,0,x1,H,D-1,S_(0x5a6a6a),S_(0x3a4a4a));
  vx_roof(x0,x1,H+1,0,D-1,14,1,S_(0x2a6a5a),S_(0x1a4a3a),2);
  vx_bricks(10,0,13,14,38,17,S_(0x4a5a5a),S_(0x2a3a3a)); vx_box(9,39,12,15,39,18,S_(0x6a7a7a)); vx_box(10,39,13,14,39,17,G_(0x5ac83a));
  vx_emit(12,41,15,EMI_GREEN);
  /* porte : etageres de fioles */
  vx_face_rect(&w,F_FRONT,10,0,25,13,0,FRAME); vx_face_rect(&w,F_FRONT,11,0,24,12,0,0); vx_face_rect(&w,F_FRONT,11,0,24,12,1,0);
  vx_face_rect(&w,F_FRONT,11,0,24,12,2,S_(0x14241c));
  for(int y=2;y<=11;y+=4){ vx_face_rect(&w,F_FRONT,11,y,24,y,1,WOOD_D);
    for(int u=12;u<=23;u+=2){ static const uint32_t P[3]={0x9af05a,0xc08aff,0xff7a5a}; vx_face(&w,F_FRONT,u,y+1,1,G_(P[(u+y)%3])); } }
  disc_xy(0,28.5f,-3,2.6f,G_(0x9af05a)); ring_xy(0,28.5f,-3,2.6f,3.6f,IRON);
  vx_window(&w,F_FRONT,3,9,5,6,G_(0x9af05a),FRAME); vx_window(&w,F_FRONT,28,9,5,6,G_(0xc08aff),FRAME);
  vx_window(&w,F_LEFT,7,9,5,6,G_(0x9af05a),FRAME); vx_window(&w,F_RIGHT,7,9,5,6,G_(0xc08aff),FRAME);
  if(L>=2){ vx_cyl(14,-5,3.05f,0,4,COPPER); vx_cyl(14,-5,2.0f,5,6,COPPER); vx_box(14,7,-5,14,9,-5,COPPER); vx_line(14,9,-5,19,6,-5,COPPER);
    vx_box(19,0,-6,21,3,-4,S_(0x3a4a4a)); vx_box(19,4,-6,21,5,-4,G_(0x9af05a)); vx_set(20,6,-5,G_(0xc8ffa0)); vx_emit(20,7,-5,EMI_GREEN); }
  if(L>=3){ for(int y=0;y<=12;y++) for(int z=2;z<=16;z++) for(int x=-31;x<=-19;x++){ bool fr=(x==-31||x==-19||z==2||z==16)&&((x%4==0)||(z%4==0)||y==0||y==12); bool sh=(x==-31||x==-19||z==2||z==16||y==12);
      if(!sh) continue; vx_set(x,y,z,fr?WOOD_D:G_(0x5ab878)); }
    for(int x=-31;x<=-19;x++){ int y=13+mini(x+31,-19-x)/2; vx_box(x,y,2,x,y,16,(x%4==0)?WOOD_D:G_(0x6ac888)); }
    vx_box(-29,1,4,-21,1,14,LEAF); }
  if(L>=4){ for(int k=0;k<2;k++){ int x= k?x1+1:x0-1; vx_box(x,2,-1,x,18,-1,G_(k?0xc08aff:0x9af05a)); }
    vx_box(x0-2,0,-2,x0,4,0,G_(0xa35fe0)); vx_set(x0-1,5,-1,G_(0xe0c8ff)); vx_box(x1,0,-2,x1+2,3,0,G_(0xa35fe0)); vx_set(x1+1,4,-1,G_(0xe0c8ff));
    vx_emit(0,30,-3,EMI_MAGIC); }
  return D*0.5f;
}

/* ---------------- 9 : autel des heros ---------------- */
static float m_autel(int L){
  if(L==0) return site(-18,17,16,20,S_(0xff8a2a));
  vx_stones(-18,0,0,17,1,15,STONE_L,STONE_D);
  vx_box(-16,2,1,15,2,14,S_(0xa8a2b4));
  for(int k=0;k<2;k++){ int x= k?12:-16; vx_box(x,3,5,x+3,24,8,S_(0x8e8898)); vx_box(x,3,5,x,24,5,STONE_D); vx_box(x-1,25,4,x+4,26,9,S_(0xa8a2b4)); vx_box(x-1,3,4,x+4,3,9,S_(0xa8a2b4)); }
  vx_box(-10,3,4,9,15,11,STONE); vx_box(-11,15,3,10,16,12,S_(0xa8a2b4));
  vx_box(-10,10,3,9,10,3,L>=5?GOLD_L:GOLD); vx_box(-1,6,3,0,7,3,G_(0xd8203a));
  int fh= L>=4 ? 22 : 15;
  for(int y=0;y<fh;y++){ float r=4.2f*(1.0f-(float)y/fh)+0.4f; if(y<3) r=3.0f+y*0.4f; vx_cyl(0,7.5f,r,17+y,17+y,y<fh*0.35f?G_(0xffd36a):FIRE); }
  vx_emit(0,17+fh,7.5f,EMI_SPARK);
  if(L>=2){ statue(-14,-3,STONE_L,STONE,true); statue(13,-3,STONE_L,STONE,false); }
  if(L>=3){ for(int k=0;k<2;k++){ int x= k?13:-15; banner(x,24,12,4,3,CLOTH_R,CLOTH_RD,GOLD); } }
  if(L>=4){ brazier(-22,0,4); brazier(21,0,4); }
  if(L>=5){ for(int k=0;k<3;k++){ float a=k*TAU/3+0.5f; vx_sphere(cosf(a)*9.0f,30.0f+k*2,7.5f+sinf(a)*5.0f,1.3f,G_(k==0?0xffd84a:(k==1?0x8ab8ff:0xff8ab0))); } vx_emit(0,32,7.5f,EMI_MAGIC); }
  return 7.5f;
}

/* ---------------- 10 : armurerie ---------------- */
static float m_armurerie(int L){
  const int x0=-20,x1=19,D=22,H=19;
  if(L==0) return site(x0,x1,D,22,S_(0x5a5466));
  VBox w={x0,0,0,x1,H,D-1};
  vx_planks(x0,0,0,x1,H,D-1,S_(0x6a4428),S_(0x3a2416));
  for(int y=0;y<=H;y++){ vx_set(x0,y,0,WOOD_DD); vx_set(x1,y,0,WOOD_DD); vx_set(x0,y,D-1,WOOD_DD); vx_set(x1,y,D-1,WOOD_DD); }
  if(L>=5) plinth(x0,x1,0,D-1,2,-8,7);
  vx_roof(x0,x1,H+1,0,D-1,16,1,S_(0x5a5466),S_(0x3a3444),2);
  if(L>=10) paint_layer(H+1,x0-3,x1+3,-3,D+2,GOLD);
  vx_door(&w,F_FRONT,13,0,14,12,S_(0x3a2416),S_(0x2a1810)); vx_face(&w,F_FRONT,24,6,0,GOLD);
  vx_line(-18,4,-1,-10,16,-1,STEEL); vx_line(-10,4,-1,-18,16,-1,STEEL);
  vx_box(-19,5,-2,-17,5,-2,GOLD); vx_box(-11,5,-2,-9,5,-2,GOLD);
  disc_xy(13.5f,10.5f,-1,5.6f,S_(0x8f2233)); disc_xy(13.5f,10.5f,-2,3.6f,S_(0xe0453a)); ring_xy(13.5f,10.5f,-2,4.6f,5.6f,GOLD); disc_xy(13.5f,10.5f,-3,1.4f,GOLD_L);
  vx_box(-4,23,-3,3,28,-3,WOOD); vx_line(-3,24,-4,2,27,-4,STEEL); vx_line(-3,27,-4,2,24,-4,STEEL);
  if(L>=10){ vx_box(-4,23,-3,3,28,-3,GOLD); vx_line(-3,24,-4,2,27,-4,G_(0xfff0a0)); vx_line(-3,27,-4,2,24,-4,G_(0xfff0a0)); }
  vx_window(&w,F_LEFT,8,8,5,6,LIT,FRAME); vx_window(&w,F_RIGHT,6,8,5,6,LIT,FRAME); vx_window(&w,F_BACK,17,8,5,6,LIT,FRAME);
  if(L>=2){ vx_box(-16,0,-7,-16,8,-7,WOOD_D); vx_box(-17,4,-8,-15,8,-6,S_(0xc8a050)); vx_box(-19,7,-7,-13,7,-7,WOOD_D); vx_box(-16,9,-7,-16,10,-7,S_(0xc8a050)); vx_set(-16,6,-9,S_(0xe0453a)); }
  if(L>=3){ vx_box(20,3,3,20,3,14,WOOD_D); vx_box(20,10,3,20,10,14,WOOD_D); for(int z=4;z<=13;z+=3){ vx_box(21,1,z,21,15,z,WOOD); vx_set(21,16,z,STEEL); vx_set(21,17,z,STEEL); } }
  if(L>=4){ for(int k=0;k<2;k++){ int z=5+k*9; for(int y=7;y<=13;y++) for(int zz=z-3;zz<=z+3;zz++){ float dy=y-10.0f, dz=(float)(zz-z); if(dy*dy+dz*dz<=10.5f) vx_set(x0-1,y,zz,(dy*dy+dz*dz>6.0f)?GOLD:S_(k?0x3b5fb8:0x3fa34d)); } } }
  if(L>=6){ for(int y=0;y<=6;y++) for(int z=-8;z<=-4;z++){ float dy=y-3.0f, dz=z+6.0f; if(dy*dy+dz*dz<=7.0f) vx_set(13,y+1,z,S_(0x9a94a6)); } vx_box(12,0,-9,12,4,-9,WOOD_D); vx_box(12,0,-3,12,4,-3,WOOD_D); vx_box(11,4,-6,15,4,-6,WOOD_D); vx_set(15,5,-6,WOOD_D); }
  if(L>=7){ vx_box(-11,0,-3,-11,2,-3,WOOD_D); vx_box(-12,3,-4,-10,7,-2,STEEL); vx_box(-13,6,-3,-9,7,-3,STEEL); vx_box(-11,8,-3,-11,9,-3,STEEL); vx_set(-11,9,-4,F_(0x2a1c18)); vx_set(-11,10,-3,S_(0xe0453a)); }
  if(L>=8){ banner(-6,19,9,-1,3,CLOTH_RD,S_(0x3a0c14),GOLD); banner(4,19,9,-1,3,CLOTH_RD,S_(0x3a0c14),GOLD); }
  if(L>=9){ vx_planks(21,0,10,28,30,17,S_(0x6a4428),S_(0x3a2416)); vx_roof_hip(21,28,31,10,17,6,S_(0x5a5466),S_(0x3a3444),1); vx_window(&(VBox){21,0,10,28,30,17},F_FRONT,2,22,4,5,LIT,FRAME); vx_box(21,26,9,28,26,9,WOOD_D); }
  if(L>=10){ torch(-21,0,-2); torch(20,0,-2); }
  return D*0.5f;
}

/* ---------------- 11 : serre (v7) ---------------- */
static float m_serre(int L){
  const int x0=-17,x1=16,D=18,H=9;
  if(L==0) return site(x0,x1,D,18,S_(0x3fa34d));
  uint32_t GL=M_(0xa8dcd0), GLD=M_(0x7ab8b0), GLP=M_(0x6ab88a), RIB= L>=8?GOLD:IRON;
  VBox w={x0,0,0,x1,H,D-1};
  vx_stones(x0,0,0,x1,2,D-1,STONE_L,STONE_D);                                       /* muret */
  for(int y=3;y<=H;y++) for(int z=0;z<D;z++) for(int x=x0;x<=x1;x++){
    if(!(x==x0||x==x1||z==0||z==D-1)) continue;
    bool rib = y==H || ((x==x0||x==x1) ? (z%4==0||z==D-1) : ((x-x0)%4==0||x==x1));
    uint32_t h=vhash(x,y,z);
    vx_set(x,y,z, rib?RIB:(y<=5 && h%3==0 ? GLP : (h%5==0?GLD:GL))); }              /* verre, plantes derriere */
  vx_roof(x0,x1,H+1,0,D-1,8,1,GL,GLD,1);
  for(int x=x0-1;x<=x1+1;x++) if((x-x0)%4==0||x==x0-1||x==x1+1) for(int y=H+1;y<=H+10;y++) for(int z=-2;z<=D+1;z++) if(vx_get(x,y,z)) vx_set(x,y,z,RIB);
  vx_door(&w,F_FRONT,15,0,4,8,WOOD_L,WOOD_D);
  if(L>=2){ flowers(x0-2,x1+2,-4,-2,21); }
  if(L>=3){ static const int PX[3]={-6,7,10}; for(int k=0;k<3;k++){ vx_cyl((float)PX[k],-2.0f,1.3f,0,1,S_(0xc06a3a)); vx_sphere((float)PX[k],3.0f,-2.0f,1.6f,k==1?L_(0x5ab84a):LEAF); }
    vx_box(-3,0,-3,-2,1,-2,STEEL); vx_set(-1,1,-3,STEEL); }
  if(L>=4){ lantern(-11,7,0,G_(0xc8ff8a)); lantern(10,7,0,G_(0xc8ff8a)); }
  if(L>=5){ vx_box(-23,0,6,-22,9,7,WOOD_D); vx_blob(-22.5f,12.0f,6.5f,5.0f,3.8f,4.6f,LEAF,LEAF_D);
    for(int k=0;k<7;k++){ uint32_t h=vhash(k,5,11); vx_set(-26+(int)(h%8),10+(int)((h>>4)%5),3+(int)((h>>8)%3),S_(0xe0453a)); } }
  if(L>=6){ VBox a={17,0,4,25,7,14}; vx_stones(17,0,4,25,1,14,STONE_L,STONE_D);
    for(int y=2;y<=7;y++) for(int z=4;z<=14;z++) for(int x=17;x<=25;x++){ if(!(x==25||z==4||z==14)) continue; bool rb=y==7||(x-17)%4==0||z%4==0; vx_set(x,y,z,rb?RIB:(vhash(x,y,z)%3==0?GLP:GL)); }
    vx_roof(17,25,8,4,14,5,0,GL,GLD,1); (void)a; }
  if(L>=7){ vx_cyl_stones(-9,-8,3.2f,0,1,STONE_L,STONE); vx_cyl(-9,-8,2.2f,1,1,W_(0x4ab8d8)); vx_box(-9,2,-8,-9,4,-8,STONE_L); vx_set(-9,5,-8,W_(0x8ae8ff)); }
  if(L>=8){ for(int k=0;k<9;k++){ uint32_t h=vhash(k,8,3); static const uint32_t GC[3]={0xf59ac0,0xc8ff6a,0x8ae8ff}; vx_set(x0-2+(int)(h%(x1-x0+4)),1,-3-(int)((h>>8)%2),G_(GC[(h>>12)%3])); }
    vx_emit(0,H+10,8,EMI_MAGIC); vx_emit(-22,15,6,EMI_GREEN); }
  return D*0.5f;
}

/* ---------------- 12 : observatoire (v7) ---------------- */
static void front_paint(int x,int y,float zmax,uint32_t c,int depth){   /* peint la premiere surface rencontree depuis l'avant */
  for(int z=-4;z<=(int)zmax;z++) if(vx_get(x,y,z)){ for(int d=0;d<depth;d++) vx_set(x,y,z+d,c); return; }
}
static float m_observ(int L){
  if(L==0) return site(-11,10,18,26,S_(0x2a3b80));
  const float cz=8.5f; int H=20+(L>=5?8:0);
  vx_cyl_stones(0,cz,8.8f,0,2,STONE_L,STONE_D);
  vx_cyl_stones(0,cz,7.2f,3,H,S_(0x8a8296),S_(0x5a5466));
  vx_cyl(0,cz,7.9f,H+1,H+1,STONE_L);
  uint32_t DM= L>=7?GOLD:M_(0x5a7ac8), DMD= L>=7?M_(0xb08a2a):M_(0x34508a);
  for(int y=0;y<=7;y++){ float r=7.5f*sqrtf(fmaxf(0,1-(y/7.8f)*(y/7.8f))); vx_cyl(0,cz,fmaxf(r,0.9f),H+2+y,H+2+y,(y%3==2)?DMD:DM); }
  for(int y=H+2;y<=H+8;y++) for(int z=-2;z<=(int)cz;z++){ int nn=0; for(int x=-1;x<=0;x++) if(vx_get(x,y,z)){ vx_set(x,y,z,FRAME); nn++; } if(nn) break; }   /* fente */
  for(int x=-2;x<=1;x++) for(int y=3;y<=9;y++) front_paint(x,y,cz,(y==9||x==-2||x==1)?WOOD_D:WOOD,2);           /* porte */
  for(int x=-1;x<=0;x++) for(int y=13;y<=16;y++) front_paint(x,y,cz,(y==16||y==13)?FRAME:LIT,1);                /* fenetre */
  if(L>=5) for(int x=-1;x<=0;x++) for(int y=21;y<=24;y++) front_paint(x,y,cz,(y==24||y==21)?FRAME:LIT,1);
  { float tl= L>=8?13.0f:8.0f; uint32_t TC= L>=8?GOLD:BRONZE;                                                     /* lunette */
    if(L>=2){ float ex=2.0f+tl*0.75f, ey=(float)H+5+tl*0.7f, ez=3.0f-tl*0.55f;
      for(int k=0;k<2;k++) for(int j=0;j<2;j++) vx_line(1.0f,(float)H+5+j,3.0f+k,ex,ey+j,ez+k,TC);
      vx_box((int)ex,(int)ey,(int)ez,(int)ex+1,(int)ey+1,(int)ez+1,G_(0x9ad8ff)); vx_box(0,H+3,2,1,H+5,3,IRON); } }
  if(L>=3){ lantern(-5,8,4,G_(0x8ab8ff)); lantern(4,8,4,G_(0x8ab8ff)); }
  if(L>=4){ vx_box(10,0,-5,12,3,-3,STONE_L); vx_box(11,4,-4,11,5,-4,BRONZE);
    ring_xy(11.5f,8.5f,-4,2.4f,3.2f,BRONZE); for(int k=0;k<16;k++){ float a=k*TAU/16; vx_set((int)floorf(11.5f+cosf(a)*3.0f),8,(int)floorf(-3.5f+sinf(a)*3.0f),GOLD); }
    vx_sphere(11.5f,8.5f,-3.5f,1.2f,G_(0x8ab8ff)); }
  if(L>=6){ int yb=H-7; for(int z=-2;z<=19;z++) for(int x=-10;x<=10;x++){ float dx=x+0.5f, dz=z+0.5f-cz; float d=dx*dx+dz*dz; if(d<=9.4f*9.4f && d>7.2f*7.2f){ vx_set(x,yb,z,WOOD); float a=atan2f(dz,dx); if(fabsf(fmodf(a*8/TAU+64,1.0f)-0.5f)>0.42f || d>8.9f*8.9f) vx_set(x,yb+1,z,WOOD_D); } }
    flagpole(-9,4,yb+9,5,4,S_(0x2a3b80),G_(0xfff0a0)); }
  if(L>=8){ for(int k=0;k<6;k++){ float a=k*TAU/6+0.3f; vx_set((int)floorf(cosf(a)*10.5f),H+4+(k%3)*3,(int)floorf(cz+sinf(a)*10.5f),G_(0xfff0a0)); } vx_emit(0,(float)H+12,cz,EMI_MAGIC); }
  return cz;
}


/* ---------------- 13 : creuset legendaire (v9) : fourneau de pierre et creuset de metal en fusion ---------------- */
static float m_creuset(int L){
  if(L==0) return site(-10,10,16,22,S_(0xd8a83a));
  const float cz=7.5f;
  uint32_t MOLT=G_(0xff9a3a), MOLT2=G_(0xd870ff), RUNE=G_(0xb08aff);
  vx_cyl_stones(0,cz,9.2f,0,1,STONE_L,STONE_D);                                    /* socle */
  vx_cyl_stones(0,cz,7.4f,2,11,S_(0x6a6274),S_(0x443e50));                         /* fourneau */
  for(int k=0;k<6;k++){ float a=k*TAU/6+0.25f; int x=(int)floorf(cosf(a)*7.6f), z=(int)floorf(cz+sinf(a)*7.6f); vx_box(x,2,z,x,11,z,S_(0x3a3444)); }
  for(int x=-2;x<=1;x++) for(int y=3;y<=6;y++) front_paint(x,y,cz,(y==6||x==-2||x==1)?S_(0x2a2232):G_(0xff7a2a),1);   /* bouche du foyer */
  for(int y=12;y<=17;y++){ float r=4.6f+1.6f*sinf((y-12)/5.0f*PI*0.5f); vx_cyl(0,cz,r,y,y,(y==17)?(L>=5?GOLD:IRON):M_(0x4a4452)); }   /* creuset */
  vx_cyl(0,cz,5.0f,17,17,MOLT); vx_cyl(0,cz,3.0f,17,17,(L>=6)?MOLT2:G_(0xffd060));
  if(L>=2){ for(int k=0;k<4;k++){ float a=k*TAU/4+PI/4; int x=(int)floorf(cosf(a)*11.5f), z=(int)floorf(cz+sinf(a)*11.5f);
      vx_box(x,0,z,x+1,9,z+1,STONE_L); vx_box(x,10,z,x+1,10,z+1,STONE); vx_set(x,6,z-1,RUNE); vx_set(x+1,4,z-1,RUNE); } }
  if(L>=3){ vx_box(9,0,1,13,2,4,WOOD_D); vx_box(10,3,2,12,3,3,S_(0x7a4a2a)); for(int k=0;k<6;k++){ uint32_t h=vhash(k,3,13); vx_set(-12+(int)(h%4),0,1+(int)((h>>4)%3),S_(0x1a1418)); vx_set(-12+(int)(h%3),1,2,S_(0x2a2228)); } }
  if(L>=4){ vx_cyl(5,cz+4,1.4f,12,22,S_(0x5a5466)); vx_cyl(5,cz+4,1.8f,22,22,S_(0x3a3444)); vx_emit(5,23,cz+4,EMI_SPARK); vx_emit(5,24,cz+4,EMI_SMOKE); }
  if(L>=6){ for(int k=0;k<5;k++){ float a=k*TAU/5; vx_box((int)floorf(cosf(a)*7.0f),20+(k%2)*2,(int)floorf(cz+sinf(a)*7.0f),(int)floorf(cosf(a)*7.0f),21+(k%2)*2,(int)floorf(cz+sinf(a)*7.0f),G_(k&1?0x9af8ff:0xd8a8ff)); } }
  if(L>=7){ banner(-9,14,7,-2,3,S_(0x3a1a5a),S_(0x1a0a2a),GOLD); banner(7,14,7,-2,3,S_(0x3a1a5a),S_(0x1a0a2a),GOLD); }
  if(L>=8){ for(int k=0;k<12;k++){ float a=k*TAU/12; vx_set((int)floorf(cosf(a)*9.4f),2,(int)floorf(cz+sinf(a)*9.4f),G_(0xffe890)); } vx_emit(0,22,cz,EMI_MAGIC); vx_emit(0,19,cz,EMI_SPARK); }
  return cz;
}

/* ---------------- 14 : cabinet des curiosites (v9) : boutique de bois, vitrine pleine d'objets etranges ---------------- */
static float m_cabinet(int L){
  const int x0=-11,x1=11,D=14,H=12;
  if(L==0) return site(x0,x1,D,16,S_(0x8a5ab8));
  VBox w={x0,0,0,x1,H,D-1};
  vx_stones(x0-1,0,-1,x1+1,0,D,STONE_L,STONE_D);
  vx_planks(x0,1,0,x1,H,D-1,S_(0x6a3a5a),S_(0x3a1a30));
  vx_roof(x0,x1,H+1,0,D-1,7,0,S_(0x3a2a4a),S_(0x22182e),1);
  vx_door(&w,F_FRONT,3,1,4,8,WOOD,WOOD_D);
  vx_window(&w,F_FRONT,10,3,10,6,G_(0xffe0a0),FRAME);                              /* vitrine */
  { static const uint32_t OC[6]={0xe0453a,0x9af8ff,0xf9dc5c,0xd8a8ff,0x8be05a,0xffffff};
    for(int k=0;k<8;k++){ uint32_t h=vhash(k,7,3); vx_set(x0+11+(int)(h%8),4+(int)((h>>4)%4),-1,S_(OC[(h>>8)%6])); } }
  vx_box(x0+10,10,-2,x0+19,11,-2,WOOD_D); vx_box(x0+12,10,-3,x0+17,11,-3,S_(0xc8a050));  /* enseigne */
  if(L>=2){ vx_cyl(x0-2,-3,1.4f,0,3,S_(0xb0704a)); vx_cyl(x0-2,-3,1.0f,4,4,S_(0x7a4a30)); vx_set(x0-2,4,-3,M_(0xf9dc5c)); }
  if(L>=3){ crate(x1+2,0,-2,3,S_(0x7a5a34),S_(0x4a3420)); vx_box(x1+3,3,-1,x1+3,3,-1,M_(0xd8a83a)); }
  if(L>=4){ lantern(-4,9,0,G_(0xd8a8ff)); lantern(x1-2,9,0,G_(0xd8a8ff)); }
  if(L>=5){ vx_sphere(-3,H+3,-1,1.6f,S_(0xe8dcc0)); vx_set(-4,H+3,-3,S_(0x1a1018)); vx_set(-2,H+3,-3,S_(0x1a1018)); vx_box(-6,H+2,-1,-5,H+2,-1,S_(0xe8dcc0)); vx_box(-1,H+2,-1,0,H+2,-1,S_(0xe8dcc0)); }
  if(L>=6){ vx_cyl(x1+3,6,1.6f,0,1,WOOD_D); for(int y=2;y<=5;y++) vx_cyl(x1+3,6,1.6f,y,y,M_(0xb8e8f0)); vx_set(x1+3,3,5,G_(0xffffff)); vx_set(x1+3,4,5,G_(0xd8d0ff)); }
  if(L>=7){ vx_planks(-4,H+1,4,4,H+7,10,S_(0x6a3a5a),S_(0x3a1a30)); vx_roof_hip(-4,4,H+8,4,10,4,S_(0x3a2a4a),S_(0x22182e),1); vx_window(&(VBox){-4,H+1,4,4,H+7,10},F_FRONT,3,H+3,3,3,G_(0xffe0a0),FRAME); }
  if(L>=8){ vx_box(x0+12,10,-3,x0+17,11,-3,GOLD); flagpole(x1,D-2,H+10,4,3,S_(0x6a3a8a),G_(0xfff0a0)); vx_emit(0,H+12,6,EMI_MAGIC); }
  return D*0.5f;
}

/* ---------------- socle de maquette ---------------- */
static int tile[BLD_COUNT][4]; static bool tile_ok[BLD_COUNT];
static float build(int b,int L){
  switch(b){
    case BLD_FORGE: return m_forge(L);
    case BLD_INFIRMERIE: return m_infirmerie(L);
    case BLD_ATELIER: return m_atelier(L);
    case BLD_BIBLIO: return m_biblio(L);
    case BLD_TRESOR: return m_tresor(L);
    case BLD_PUITS: return m_puits(L);
    case BLD_SANCTUAIRE: return m_sanctuaire(L);
    case BLD_TOUR: return m_tour(L);
    case BLD_LABO: return m_labo(L);
    case BLD_AUTEL: return m_autel(L);
    case BLD_SERRE: return m_serre(L);
    case BLD_OBSERV: return m_observ(L);
    case BLD_CREUSET: return m_creuset(L);
    case BLD_CABINET: return m_cabinet(L);
    default: return m_armurerie(L);
  }
}
static void socle(int b){
  int x0=tile[b][0]-3, x1=tile[b][1]+3, z0=tile[b][2]-3, z1=tile[b][3]+3;
  for(int y=-4;y<=-1;y++) for(int z=z0;z<=z1;z++) for(int x=x0;x<=x1;x++){
    uint32_t h=vhash(x,y,z); uint32_t c;
    bool edge=(x==x0||x==x1||z==z0||z==z1);
    if(y==-1){ bool path = abs(x)<=3 && z<0;
      if(path) c= (h%5==0)?S_(0x6a6070):((h%2)?S_(0x9a90a0):S_(0x847a8c));
      else c= L_(h%3==0?0x4a7a38:(h%3==1?0x3f6a32:0x36602c)); if(edge && !path) c=L_(0x2e5a2a); }
    else if(y==-2) c= S_(h%4==0?0x6a4a30:0x5a3e28);
    else if(y==-3) c= (h%7==0)?S_(0x6a6070):S_(0x4a3220);
    else c=S_(0x2a1c14);
    vx_set(x,y,z,c);
  }
  for(int z=z0+1;z<z1;z++) for(int x=x0+1;x<x1;x++){
    if(vx_get(x,0,z) || (abs(x)<=4 && z<0)) continue;
    uint32_t h=vhash(x,21,z);
    if(h%23==0){ vx_set(x,0,z,L_(0x2a7a3a)); if((h>>6)%2) vx_set(x,1,z,S_((h>>9)%3==0?0xf59ac0:((h>>9)%3==1?0xf9dc5c:0xffffff))); }
    else if(h%31==0) vx_set(x,0,z,S_(0x7a7486));
  }
}
void bld_model(int b,int lvl,bool with_socle,VoxModel *m){
  b=clampi(b,0,BLD_COUNT-1); lvl=clampi(lvl,0,BUILDINGS[b].max);
  if(with_socle && !tile_ok[b]){
    vx_begin(); build(b,BUILDINGS[b].max);
    int x0,y0,z0,x1,y1,z1;
    if(vx_bounds(&x0,&y0,&z0,&x1,&y1,&z1)){ tile[b][0]=x0; tile[b][1]=x1; tile[b][2]=z0; tile[b][3]=z1; }
    else { tile[b][0]=-20; tile[b][1]=20; tile[b][2]=-6; tile[b][3]=22; }
    tile_ok[b]=true;
  }
  vx_begin();
  float pz=build(b,lvl);
  if(with_socle) socle(b);
  vx_end(m,0,pz);
}

/* ---------------- ce que chaque niveau ajoute ---------------- */
static const char *const VIS[BLD_COUNT][16]={
  {"Un chantier","La forge","Une enclume sur sa souche","Un tonneau de trempe","Deux lanternes à l'entrée","Un auvent et son établi","Un soufflet de forge","Un socle de pierre taillée",
   "Une seconde cheminée","Un râtelier d'épées","Des tuiles neuves au faîte doré","Une fonderie annexe","Un chariot de minerai","Les bannières de la guilde","Le grand marteau du fronton","Braises éternelles et toit doré"},
  {"Un chantier","L'infirmerie","Un banc pour les blessés","Des jardinières d'herbes","Une lanterne à la porte","Un potager de simples","Des volets bleus","Un socle de pierre",
   "Une croix lumineuse","Une aile de convalescence","Du linge qui sèche","Une fontaine de soins","Un clocheton","Des vitraux","Des fanions sur le toit","Faîte doré et croix de lumière"},
  {"Un chantier","L'atelier","Un établi dehors","Des engrenages de bronze","Des caisses de pièces","Une cheminée en tôle","Une poulie de levage","Un socle de pierre",
   "Des tuyaux de cuivre","Une horloge au fronton","Une éolienne sur le toit","Un automate gardien","Lumières bleues et toit doré"},
  {"Un chantier","La bibliothèque","Des piles de livres","Un lutrin et son grimoire","Des lanternes bleues","Un banc de lecture","Un socle de pierre","Une tourelle d'étude",
   "Une lunette astronomique","Une rosace en vitrail","Un globe céleste","Une flèche dorée","Des runes lumineuses"},
  {"Un chantier","La trésorerie","Un coffre à la porte","Des sacs d'or","Des lanternes dorées","Une grille en fer forgé","Un socle de marbre","Des colonnes de marbre",
   "Un garde en armure","Une pile de lingots","Un dôme doré","Des bannières pourpres","Un trésor qui scintille"},
  {"Un chantier","Le puits","Des pièces au fond de l'eau","Une margelle de marbre","Des fleurs tout autour","Une lanterne suspendue","Une eau lumineuse","Le puits enchanté"},
  {"Un chantier","Le sanctuaire","Deux braseros","L'orbe éveillé"},
  {"Un chantier","La tour de guet","Un grand étendard","Des torches aux créneaux","Un étage de plus","Un balcon de bois","Une cloche d'alarme","Encore un étage","Le phare de Fort-Rebond"},
  {"Un chantier","Le laboratoire","Un alambic de cuivre","Une serre lumineuse","Le laboratoire des arcanes"},
  {"Un chantier","L'autel","Deux statues de héros","Des bannières aux piliers","Une flamme plus haute et des braseros","L'autel des légendes"},
  {"Un chantier","L'armurerie","Un mannequin d'entraînement","Un râtelier de lances","Des boucliers aux murs","Un socle de pierre","Une meule à aiguiser","Une armure exposée",
   "Des bannières de guerre","Une tour d'archers","L'arsenal royal"},
  {"Un chantier","La serre","Des parterres de fleurs","Des pots et un arrosoir","Des lanternes vertes","Un pommier","Une aile vitrée",
   "Une fontaine d'eau claire","La serre enchantée"},
  {"Un chantier","L'observatoire","Une lunette sous la coupole","Des lanternes bleues","Une sphère armillaire","Un étage de plus",
   "Un balcon circulaire","Une coupole dorée","Le grand télescope"},
  {"Un chantier","Le creuset légendaire","Des piliers runiques","Un soufflet et du charbon","Une cheminée à étincelles","Un rebord doré",
   "Des éclats de cristal en orbite","Les bannières de la fusion","La couronne de lumière"},
  {"Un chantier","Le cabinet des curiosités","Une urne sur le seuil","Une malle à trésors","Des lanternes mauves","Un crâne au-dessus de la porte",
   "Une étoile sous cloche","Un cabinet à l'étage","L'enseigne dorée"},
};
const char *bld_visual(int b,int lvl){
  b=clampi(b,0,BLD_COUNT-1); lvl=clampi(lvl,0,BUILDINGS[b].max);
  const char *s=VIS[b][lvl]; return s?s:"";
}

/* ---------------- planche de controle (tests) ---------------- */
static void put_img(uint8_t *img,int IW,int IH,const Sprite *s,int x0,int y0,int scale){
  for(int y=0;y<s->h;y++) for(int x=0;x<s->w;x++){ uint32_t c=s->px[y*s->w+x]; if(!(c>>24)) continue;
    for(int j=0;j<scale;j++) for(int i=0;i<scale;i++){ int X=x0+x*scale+i, Y=y0+y*scale+j; if(X<0||Y<0||X>=IW||Y>=IH) continue; uint8_t *p=&img[(Y*IW+X)*3]; p[0]=(uint8_t)cr(c); p[1]=(uint8_t)cg(c); p[2]=(uint8_t)cb(c); } }
}
static void sheet_levels(const char *path,int b){
  int TW=240, TH=220, cols=4, n=BUILDINGS[b].max+1, rows=(n+cols-1)/cols, IW=TW*cols, IH=TH*rows;
  uint8_t *img=(uint8_t*)malloc((size_t)IW*IH*3);
  for(int i=0;i<IW*IH;i++){ img[i*3]=0x1a; img[i*3+1]=0x12; img[i*3+2]=0x1c; }
  Sprite out=spr_alloc(TW,TH); out.em=(uint32_t*)calloc((size_t)TW*TH,4);
  VoxModel m={0}, mx={0}; bld_model(b,BUILDINGS[b].max,true,&mx); float sc,cy; vox_fit(&mx,0.5f,TW-12,TH-12,3.6f,&sc,&cy); vox_free(&mx);
  for(int L=0;L<n;L++){ bld_model(b,L,true,&m); VoxCam cam={-0.6f,0.5f,sc,TW*0.5f,TH*0.5f+cy}; vox_render(&m,1.0f,0,&cam,&out);
    put_img(img,IW,IH,&out,(L%cols)*TW,(L/cols)*TH,1); }
  FILE *f=fopen(path,"wb"); if(f){ fprintf(f,"P6 %d %d 255\n",IW,IH); fwrite(img,1,(size_t)IW*IH*3,f); fclose(f); }
  free(img); free(out.px); free(out.em); vox_free(&m);
}
void vox_debug_sheet(const char *path){
  { const char *e=getenv("VOXB"); if(e){ sheet_levels(path,atoi(e)); return; } }
  /* planche 1 : sprites du campement, tous niveaux */
  int CW=70, CH=78, IW=16*CW, IH=BLD_COUNT*CH;
  uint8_t *img=(uint8_t*)malloc((size_t)IW*IH*3);
  for(int i=0;i<IW*IH;i++){ img[i*3]=0x22; img[i*3+1]=0x2a; img[i*3+2]=0x1e; }
  VoxModel m={0}; Sprite s={0};
  for(int b=0;b<BLD_COUNT;b++) for(int L=0;L<=BUILDINGS[b].max;L++){
    bld_model(b,L,false,&m); int ax,ay; vox_camp_sprite(&m,&s,&ax,&ay);
    int cx=L*CW+CW/2, by=b*CH+CH-8;
    put_img(img,IW,IH,&s,cx-ax,by-ay,1);
    for(int x=-2;x<=2;x++){ int X=cx+x, Y=by; if(X>=0&&X<IW&&Y<IH){ img[(Y*IW+X)*3]=255; img[(Y*IW+X)*3+1]=0; img[(Y*IW+X)*3+2]=0; } }
  }
  char p[512]; snprintf(p,sizeof p,"%s_camp.ppm",path);
  FILE *f=fopen(p,"wb"); if(f){ fprintf(f,"P6 %d %d 255\n",IW,IH); fwrite(img,1,(size_t)IW*IH*3,f); fclose(f); }
  free(img);
  /* planche 2 : maquettes tournantes, niveau max, 3 angles */
  int TW=230, TH=250; IW=TW*6; IH=TH*((BLD_COUNT*3+5)/6);
  img=(uint8_t*)malloc((size_t)IW*IH*3);
  for(int i=0;i<IW*IH;i++){ img[i*3]=0x1a; img[i*3+1]=0x12; img[i*3+2]=0x1c; }
  Sprite out=spr_alloc(TW,TH); out.em=(uint32_t*)calloc((size_t)TW*TH,4);
  for(int b=0;b<BLD_COUNT;b++) for(int k=0;k<3;k++){
    int L = k==2 ? 1 : BUILDINGS[b].max;
    bld_model(b,L,true,&m);
    VoxModel mx={0}; bld_model(b,BUILDINGS[b].max,true,&mx);
    float sc,cy; vox_fit(&mx,0.50f,TW-16,TH-16,3.6f,&sc,&cy); vox_free(&mx);
    VoxCam cam={k==1?2.4f:-0.55f,0.50f,sc,TW*0.5f,TH*0.5f+cy};
    vox_render(&m,1.0f,0,&cam,&out);
    int cell=b*3+k; int X0=(cell%6)*TW, Y0=(cell/6)*TH;
    put_img(img,IW,IH,&out,X0,Y0,1);
  }
  snprintf(p,sizeof p,"%s_3d.ppm",path);
  f=fopen(p,"wb"); if(f){ fprintf(f,"P6 %d %d 255\n",IW,IH); fwrite(img,1,(size_t)IW*IH*3,f); fclose(f); }
  free(img); free(out.px); free(out.em); free(s.px); free(s.nm); free(s.em); vox_free(&m);
}
