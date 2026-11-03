/* GOUFFRE - voxels : maquettes 3D des batiments du campement
   - modelage dans une grille de travail (primitives : boites, briques, toits, cylindres...)
   - rendu en rotation (camera orthographique, eclairage de studio, occlusion, ombres portees)
   - rendu oblique des sprites du campement (face avant 1:1, dessus vu en biais)            */
#include "game.h"

/* ---------------- grille de travail ---------------- */
#define WX 84
#define WY 96
#define WZ 64
static uint32_t WG[WX*WY*WZ];
#define VX0 42          /* origine locale (centre du batiment, sol, facade avant) */
#define VY0 4
#define VZ0 16
#define WIDX(x,y,z) ((((y)*WZ)+(z))*WX+(x))
static VoxEmit wem[VOX_MAX_EMIT]; static int wnem=0;

void vx_begin(void){ memset(WG,0,sizeof WG); wnem=0; }
void vx_set(int x,int y,int z,uint32_t c){
  x+=VX0; y+=VY0; z+=VZ0;
  if((unsigned)x>=WX||(unsigned)y>=WY||(unsigned)z>=WZ) return;
  WG[WIDX(x,y,z)]=c;
}
uint32_t vx_get(int x,int y,int z){
  x+=VX0; y+=VY0; z+=VZ0;
  if((unsigned)x>=WX||(unsigned)y>=WY||(unsigned)z>=WZ) return 0;
  return WG[WIDX(x,y,z)];
}
/* ne repeint que les voxels deja pleins */
void vx_paint(int x,int y,int z,uint32_t c){ if(vx_get(x,y,z)) vx_set(x,y,z,c); }
void vx_emit(float x,float y,float z,int kind){
  if(wnem>=VOX_MAX_EMIT) return;
  wem[wnem].x=x+VX0+0.5f; wem[wnem].y=y+VY0+0.5f; wem[wnem].z=z+VZ0+0.5f; wem[wnem].kind=kind; wnem++;
}
static inline void srt(int *a,int *b){ if(*a>*b){ int t=*a; *a=*b; *b=t; } }
void vx_box(int x0,int y0,int z0,int x1,int y1,int z1,uint32_t c){
  srt(&x0,&x1); srt(&y0,&y1); srt(&z0,&z1);
  for(int y=y0;y<=y1;y++) for(int z=z0;z<=z1;z++) for(int x=x0;x<=x1;x++) vx_set(x,y,z,c);
}
uint32_t vhash(int x,int y,int z){ uint32_t h=((uint32_t)x*73856093u)^((uint32_t)y*19349663u)^((uint32_t)z*83492791u); h^=h>>13; h*=0x5bd1e995u; h^=h>>15; return h; }

/* murs de briques : joints creuses d'un voxel sur les quatre faces (coins pleins) */
void vx_bricks(int x0,int y0,int z0,int x1,int y1,int z1,uint32_t c,uint32_t cm){
  srt(&x0,&x1); srt(&y0,&y1); srt(&z0,&z1);
  vx_box(x0,y0,z0,x1,y1,z1,c);
  for(int y=y0;y<=y1;y++){
    int row=(y-y0); bool hj=(row%4)==3;
    for(int f=0;f<4;f++){
      int n = f<2 ? x1-x0+1 : z1-z0+1;
      for(int u=1;u<n-1;u++){
        int off=((row/4)&1)*3; bool vj=((u+off)%6)==0;
        if(!(hj||vj)) continue;
        int x,z,ix,iz;
        if(f==0){ x=x0+u; z=z0; ix=x; iz=z0+1; } else if(f==1){ x=x0+u; z=z1; ix=x; iz=z1-1; }
        else if(f==2){ x=x0; z=z0+u; ix=x0+1; iz=z; } else { x=x1; z=z0+u; ix=x1-1; iz=z; }
        if(!vx_get(x,y,z)) continue;
        vx_set(x,y,z,0); vx_set(ix,y,iz,cm);
      }
    }
  }
}
/* planches verticales : une rainure toutes les 4 colonnes */
void vx_planks(int x0,int y0,int z0,int x1,int y1,int z1,uint32_t c,uint32_t cd){
  srt(&x0,&x1); srt(&y0,&y1); srt(&z0,&z1);
  vx_box(x0,y0,z0,x1,y1,z1,c);
  for(int y=y0;y<=y1;y++){
    for(int u=2;u<x1-x0-1;u+=4){ if(vx_get(x0+u,y,z0)){ vx_set(x0+u,y,z0,0); vx_set(x0+u,y,z0+1,cd); } if(vx_get(x0+u,y,z1)){ vx_set(x0+u,y,z1,0); vx_set(x0+u,y,z1-1,cd); } }
    for(int u=2;u<z1-z0-1;u+=4){ if(vx_get(x0,y,z0+u)){ vx_set(x0,y,z0+u,0); vx_set(x0+1,y,z0+u,cd); } if(vx_get(x1,y,z0+u)){ vx_set(x1,y,z0+u,0); vx_set(x1-1,y,z0+u,cd); } }
  }
}
/* pierres taillees irregulieres (soubassements) */
void vx_stones(int x0,int y0,int z0,int x1,int y1,int z1,uint32_t c,uint32_t cd){
  srt(&x0,&x1); srt(&y0,&y1); srt(&z0,&z1);
  for(int y=y0;y<=y1;y++) for(int z=z0;z<=z1;z++) for(int x=x0;x<=x1;x++){
    int row=(y-y0)/2; uint32_t h=vhash((x+row*5)/5,row,(z+row*3)/5);
    bool joint = ((y-y0)%2==1 && y!=y1) || ((x+row*5)%5==0) || ((z+row*3)%5==0);
    uint32_t cc = joint ? cd : col_lerp(c,HEX(0xffffff),(h&7)*0.02f);
    vx_set(x,y,z,(c&0xFF000000u)|(cc&0xFFFFFF));
  }
}
/* toit a deux pans : axe=0 faitage selon x (pans vers l'avant et l'arriere), axe=1 faitage selon z (pignon en facade) */
void vx_roof(int x0,int x1,int y0,int z0,int z1,int h,int axe,uint32_t c,uint32_t cd,int over){
  srt(&x0,&x1); srt(&z0,&z1);
  float span = axe ? (x1-x0+1)*0.5f+over : (z1-z0+1)*0.5f+over;
  for(int k=0;k<h;k++){
    float hw = span*(1.0f-(float)k/h); if(hw<0.5f) hw=0.5f;
    uint32_t cc = (k%3==2) ? cd : c;
    if(k==0) cc=cd;
    if(axe){ float cx=(x0+x1+1)*0.5f; int a=(int)floorf(cx-hw+0.5f), b=(int)ceilf(cx+hw-0.5f)-1;
      for(int z=z0-over;z<=z1+over;z++) for(int x=a;x<=b;x++) vx_set(x,y0+k,z,cc); }
    else { float cz=(z0+z1+1)*0.5f; int a=(int)floorf(cz-hw+0.5f), b=(int)ceilf(cz+hw-0.5f)-1;
      for(int z=a;z<=b;z++) for(int x=x0-over;x<=x1+over;x++) vx_set(x,y0+k,z,cc); }
  }
}
/* toit a quatre pans (pyramide si carre) */
void vx_roof_hip(int x0,int x1,int y0,int z0,int z1,int h,uint32_t c,uint32_t cd,int over){
  srt(&x0,&x1); srt(&z0,&z1);
  float sx=(x1-x0+1)*0.5f+over, sz=(z1-z0+1)*0.5f+over, cx=(x0+x1+1)*0.5f, cz=(z0+z1+1)*0.5f;
  for(int k=0;k<h;k++){
    float f=1.0f-(float)k/h; float hx=maxf(0.5f,sx*f), hz=maxf(0.5f,sz*f);
    if(sx>sz) hx=maxf(0.5f,sx-(sz-hz)); else hz=maxf(0.5f,sz-(sx-hx));
    uint32_t cc=(k%3==2||k==0)?cd:c;
    for(int z=(int)floorf(cz-hz+0.5f);z<=(int)ceilf(cz+hz-0.5f)-1;z++) for(int x=(int)floorf(cx-hx+0.5f);x<=(int)ceilf(cx+hx-0.5f)-1;x++) vx_set(x,y0+k,z,cc);
  }
}
void vx_cyl(float cx,float cz,float r,int y0,int y1,uint32_t c){
  srt(&y0,&y1);
  for(int z=(int)floorf(cz-r);z<=(int)ceilf(cz+r);z++) for(int x=(int)floorf(cx-r);x<=(int)ceilf(cx+r);x++){
    float dx=x+0.5f-cx, dz=z+0.5f-cz; if(dx*dx+dz*dz>r*r) continue;
    for(int y=y0;y<=y1;y++) vx_set(x,y,z,c);
  }
}
/* cylindre de pierre : assises alternees */
void vx_cyl_stones(float cx,float cz,float r,int y0,int y1,uint32_t c,uint32_t cd){
  srt(&y0,&y1);
  int nseg=maxi(4,(int)(TAU*r/5.0f)); float jw=nseg/(TAU*r);
  for(int z=(int)floorf(cz-r);z<=(int)ceilf(cz+r);z++) for(int x=(int)floorf(cx-r);x<=(int)ceilf(cx+r);x++){
    float dx=x+0.5f-cx, dz=z+0.5f-cz; float d2=dx*dx+dz*dz; if(d2>r*r) continue;
    bool edge = d2>(r-1.5f)*(r-1.5f);
    float a=atan2f(dz,dx);
    for(int y=y0;y<=y1;y++){
      uint32_t cc=c;
      if(edge){ int row=(y-y0)/3; float q=(a+PI)/TAU*nseg+0.5f*(row&1); float fr=q-floorf(q);
        if((y-y0)%3==2 || fr<jw) cc=cd; }
      vx_set(x,y,z,(c&0xFF000000u)|(cc&0xFFFFFF));
    }
  }
}
void vx_cone(float cx,float cz,float r,int y0,int h,uint32_t c,uint32_t cd){
  for(int k=0;k<h;k++){ float rr=r*(1.0f-(float)k/h); if(rr<0.6f) rr=0.6f; vx_cyl(cx,cz,rr,y0+k,y0+k,(k%3==2)?cd:c); }
}
void vx_sphere(float cx,float cy,float cz,float r,uint32_t c){
  for(int y=(int)floorf(cy-r);y<=(int)ceilf(cy+r);y++) for(int z=(int)floorf(cz-r);z<=(int)ceilf(cz+r);z++) for(int x=(int)floorf(cx-r);x<=(int)ceilf(cx+r);x++){
    float dx=x+0.5f-cx, dy=y+0.5f-cy, dz=z+0.5f-cz; if(dx*dx+dy*dy+dz*dz<=r*r) vx_set(x,y,z,c); }
}
void vx_blob(float cx,float cy,float cz,float rx,float ry,float rz,uint32_t c,uint32_t c2){
  for(int y=(int)floorf(cy-ry);y<=(int)ceilf(cy+ry);y++) for(int z=(int)floorf(cz-rz);z<=(int)ceilf(cz+rz);z++) for(int x=(int)floorf(cx-rx);x<=(int)ceilf(cx+rx);x++){
    float dx=(x+0.5f-cx)/rx, dy=(y+0.5f-cy)/ry, dz=(z+0.5f-cz)/rz; float d=dx*dx+dy*dy+dz*dz;
    uint32_t h=vhash(x,y,z); if(d>1.0f-(h&3)*0.05f) continue;
    vx_set(x,y,z,(h>>5)%3==0?c2:c); }
}
/* ligne de voxels (poteaux obliques, cordes, lames) */
void vx_line(float x0,float y0,float z0,float x1,float y1,float z1,uint32_t c){
  float dx=x1-x0, dy=y1-y0, dz=z1-z0; int n=(int)ceilf(maxf(fabsf(dx),maxf(fabsf(dy),fabsf(dz))))+1;
  for(int i=0;i<=n;i++){ float t=(float)i/n; vx_set((int)floorf(x0+dx*t+0.5f),(int)floorf(y0+dy*t+0.5f),(int)floorf(z0+dz*t+0.5f),c); }
}

/* faces d'une boite : u horizontal (de gauche a droite vu de face), d profondeur (0 = surface, >0 vers l'interieur, <0 en saillie) */
void vx_face(const VBox *b,int face,int u,int y,int d,uint32_t c){
  int x,z;
  switch(face){
    case F_FRONT: x=b->x0+u; z=b->z0+d; break;
    case F_BACK:  x=b->x1-u; z=b->z1-d; break;
    case F_LEFT:  x=b->x0+d; z=b->z1-u; break;
    default:      x=b->x1-d; z=b->z0+u; break;
  }
  vx_set(x,y,z,c);
}
int vx_face_w(const VBox *b,int face){ return face<=F_BACK ? b->x1-b->x0+1 : b->z1-b->z0+1; }
void vx_face_rect(const VBox *b,int face,int u0,int y0,int u1,int y1,int d,uint32_t c){
  srt(&u0,&u1); srt(&y0,&y1);
  for(int y=y0;y<=y1;y++) for(int u=u0;u<=u1;u++) vx_face(b,face,u,y,d,c);
}
/* fenetre : cadre de bois, vitre eclairee en retrait, croisillon, appui en saillie */
void vx_window(const VBox *b,int face,int u,int y,int w,int h,uint32_t glass,uint32_t frame){
  vx_face_rect(b,face,u,y,u+w-1,y+h-1,0,frame);
  vx_face_rect(b,face,u+1,y+1,u+w-2,y+h-2,0,0);
  vx_face_rect(b,face,u+1,y+1,u+w-2,y+h-2,1,glass);
  if(w>=5){ int m=u+w/2; for(int yy=y+1;yy<=y+h-2;yy++) vx_face(b,face,m,yy,1,frame); }
  if(h>=6){ int m=y+h/2; for(int uu=u+1;uu<=u+w-2;uu++) vx_face(b,face,uu,m,1,frame); }
  vx_face_rect(b,face,u,y-1,u+w-1,y-1,-1,frame);
}
/* porte en retrait, planches, poignee doree */
void vx_door(const VBox *b,int face,int u,int y,int w,int h,uint32_t c,uint32_t cd){
  vx_face_rect(b,face,u-1,y,u+w,y+h,0,cd);
  vx_face_rect(b,face,u,y,u+w-1,y+h-1,0,0);
  for(int yy=y;yy<y+h;yy++) for(int uu=u;uu<u+w;uu++) vx_face(b,face,uu,yy,1,((uu-u)%3==2)?cd:c);
  vx_face(b,face,u+w-2,y+h/2-1,0,VC(VM_METAL,0xe0b040));
}

/* boite englobante de la grille de travail (coordonnees locales) ; faux si vide */
bool vx_bounds(int *x0,int *y0,int *z0,int *x1,int *y1,int *z1){
  int a=WX,b=WY,c=WZ,d=-1,e=-1,f=-1;
  for(int y=0;y<WY;y++) for(int z=0;z<WZ;z++) for(int x=0;x<WX;x++) if(WG[WIDX(x,y,z)]){
    if(x<a)a=x; if(x>d)d=x; if(y<b)b=y; if(y>e)e=y; if(z<c)c=z; if(z>f)f=z; }
  if(d<0) return false;
  *x0=a-VX0; *y0=b-VY0; *z0=c-VZ0; *x1=d-VX0; *y1=e-VY0; *z1=f-VZ0; return true;
}

/* ---------------- compactage, normales lissees ---------------- */
void vox_free(VoxModel *m){ free(m->c); free(m->n); free(m->blk); m->c=NULL; m->n=NULL; m->blk=NULL; m->nx=m->ny=m->nz=0; m->cap=0; m->bcap=0; }
static void build_blocks(VoxModel *m){
  m->bnx=(m->nx+3)/4; m->bny=(m->ny+3)/4; m->bnz=(m->nz+3)/4; int nb=m->bnx*m->bny*m->bnz;
  if(!m->blk || m->bcap<nb){ free(m->blk); m->bcap=nb; m->blk=(uint8_t*)malloc((size_t)nb); }
  memset(m->blk,0,(size_t)nb);
  for(int y=0;y<m->ny;y++) for(int z=0;z<m->nz;z++){ const uint32_t *row=&m->c[((y*m->nz)+z)*m->nx]; uint8_t *br=&m->blk[((y>>2)*m->bnz+(z>>2))*m->bnx];
    for(int x=0;x<m->nx;x++) if(row[x]) br[x>>2]=1; }
}
static inline bool wfull(int x,int y,int z){ if((unsigned)x>=WX||(unsigned)y>=WY||(unsigned)z>=WZ) return false; return WG[WIDX(x,y,z)]!=0; }
static void vox_normals(VoxModel *m);
void vx_end(VoxModel *m,float pivx,float pivz){
  int x0=WX,y0=WY,z0=WZ,x1=-1,y1=-1,z1=-1;
  for(int y=0;y<WY;y++) for(int z=0;z<WZ;z++) for(int x=0;x<WX;x++) if(WG[WIDX(x,y,z)]){
    if(x<x0)x0=x; if(x>x1)x1=x; if(y<y0)y0=y; if(y>y1)y1=y; if(z<z0)z0=z; if(z>z1)z1=z; }
  if(x1<0){ x0=y0=z0=0; x1=y1=z1=0; }
  int nx=x1-x0+1, ny=y1-y0+1, nz=z1-z0+1;
  if(!m->c || m->cap<nx*ny*nz){ free(m->c); free(m->n); m->cap=nx*ny*nz; m->c=(uint32_t*)malloc((size_t)m->cap*4); m->n=(uint32_t*)malloc((size_t)m->cap*4); }
  m->x0=x0; m->y0=y0; m->z0=z0; m->nx=nx; m->ny=ny; m->nz=nz;
  for(int y=0;y<ny;y++) for(int z=0;z<nz;z++) memcpy(&m->c[((y*nz)+z)*nx],&WG[WIDX(x0,y0+y,z0+z)],(size_t)nx*4);
  m->px=VX0+pivx; m->pz=VZ0+pivz; m->py=VY0; m->fz=VZ0;
  m->nem=wnem; memcpy(m->em,wem,sizeof(VoxEmit)*wnem);
  vox_normals(m);
  build_blocks(m);
}
static inline uint32_t mget(const VoxModel *m,int x,int y,int z){
  if((unsigned)x>=(unsigned)m->nx||(unsigned)y>=(unsigned)m->ny||(unsigned)z>=(unsigned)m->nz) return 0;
  return m->c[((y*m->nz)+z)*m->nx+x];
}
static void vox_normals(VoxModel *m){
  int nx=m->nx, ny=m->ny, nz=m->nz;
  static float W5[5][5][5]; static bool wok=false;
  if(!wok){ for(int a=0;a<5;a++) for(int b=0;b<5;b++) for(int c=0;c<5;c++){ int d2=(a-2)*(a-2)+(b-2)*(b-2)+(c-2)*(c-2); W5[a][b][c]= d2 ? 1.0f/(float)d2 : 0; } wok=true; }
  for(int y=0;y<ny;y++) for(int z=0;z<nz;z++) for(int x=0;x<nx;x++){
    int i=((y*nz)+z)*nx+x; m->n[i]=0;
    if(!m->c[i]) continue;
    if(mget(m,x-1,y,z)&&mget(m,x+1,y,z)&&mget(m,x,y-1,z)&&mget(m,x,y+1,z)&&mget(m,x,y,z-1)&&mget(m,x,y,z+1)) continue;
    float gx=0,gy=0,gz=0; int empt=0;
    for(int dy=-2;dy<=2;dy++) for(int dz=-2;dz<=2;dz++) for(int dx=-2;dx<=2;dx++){
      if(!dx&&!dy&&!dz) continue;
      if(mget(m,x+dx,y+dy,z+dz)) continue;
      float w=W5[dx+2][dy+2][dz+2]; gx+=dx*w; gy+=dy*w; gz+=dz*w;
      if(dx>=-1&&dx<=1&&dy>=-1&&dy<=1&&dz>=-1&&dz<=1) empt++;
    }
    float l=sqrtf(gx*gx+gy*gy+gz*gz);
    if(l<1e-3f){ gx=(float)(!mget(m,x+1,y,z))-(float)(!mget(m,x-1,y,z)); gy=(float)(!mget(m,x,y+1,z))-(float)(!mget(m,x,y-1,z)); gz=(float)(!mget(m,x,y,z+1))-(float)(!mget(m,x,y,z-1)); l=sqrtf(gx*gx+gy*gy+gz*gz); if(l<1e-3f){ gy=1; l=1; } }
    gx/=l; gy/=l; gz/=l;
    int open=clampi(empt*255/26,0,255);
    m->n[i]=(uint32_t)(uint8_t)(int8_t)(gx*127)|((uint32_t)(uint8_t)(int8_t)(gy*127)<<8)|((uint32_t)(uint8_t)(int8_t)(gz*127)<<16)|((uint32_t)open<<24);
  }
}
/* voxels absents (ou differents) du modele de reference : marques comme nouveaux */
void vox_mark_new(VoxModel *m,const VoxModel *ref){
  for(int y=0;y<m->ny;y++) for(int z=0;z<m->nz;z++) for(int x=0;x<m->nx;x++){
    int i=((y*m->nz)+z)*m->nx+x; uint32_t c=m->c[i]; if(!c) continue;
    uint32_t r=mget(ref,x+m->x0-ref->x0,y+m->y0-ref->y0,z+m->z0-ref->z0);
    if(!r || ((r^c)&0x0FFFFFFFu)) m->c[i]=c|0x80000000u;
  }
}
/* balayage de construction : sous le plan y=sweep le nouveau modele, au-dessus l'ancien */
void vox_compose(const VoxModel *old,const VoxModel *nw,float sweep,VoxModel *out){
  int x0=mini(old->x0,nw->x0), y0=mini(old->y0,nw->y0), z0=mini(old->z0,nw->z0);
  int x1=maxi(old->x0+old->nx,nw->x0+nw->nx), y1=maxi(old->y0+old->ny,nw->y0+nw->ny), z1=maxi(old->z0+old->nz,nw->z0+nw->nz);
  int nx=x1-x0, ny=y1-y0, nz=z1-z0;
  if(!out->c || out->cap<nx*ny*nz){ free(out->c); free(out->n); out->cap=nx*ny*nz; out->c=(uint32_t*)malloc((size_t)out->cap*4); out->n=(uint32_t*)malloc((size_t)out->cap*4); }
  out->x0=x0; out->y0=y0; out->z0=z0; out->nx=nx; out->ny=ny; out->nz=nz;
  out->px=nw->px; out->py=nw->py; out->pz=nw->pz; out->fz=nw->fz; out->nem=nw->nem; memcpy(out->em,nw->em,sizeof(VoxEmit)*nw->nem);
  for(int y=0;y<ny;y++){
    int Y=y+y0; bool usenew = Y < sweep; bool band = fabsf(Y+0.5f-sweep)<0.75f;
    const VoxModel *s = usenew ? nw : old;
    for(int z=0;z<nz;z++) for(int x=0;x<nx;x++){
      int i=((y*nz)+z)*nx+x; int sx=x+x0-s->x0, sy=Y-s->y0, sz=z+z0-s->z0;
      if((unsigned)sx<(unsigned)s->nx && (unsigned)sy<(unsigned)s->ny && (unsigned)sz<(unsigned)s->nz){
        int j=((sy*s->nz)+sz)*s->nx+sx; uint32_t c=s->c[j];
        if(c && band && ((c>>24)&15)!=VM_GLOW) c=(c&0x80000000u)|VC(VM_GLOW,col_lerp(c|0xFF000000u,HEX(0xffc860),0.45f));
        out->c[i]=c; out->n[i]=s->n[j];
      } else { out->c[i]=0; out->n[i]=0; }
    }
  }
  build_blocks(out);
}

/* ---------------- lancer de rayons ---------------- */
static inline int ifl(float f){ int i=(int)f; return i-(f<(float)i); }
typedef struct { float dx,dy,dz, ix,iy,iz, tdx,tdy,tdz; int sx,sy,sz; } RayP;
static void rayp(RayP *p,float dx,float dy,float dz){
  /* composante quasi nulle : rayon parallele a cet axe (sinon signe et inverse ne s'accordent plus) */
  if(fabsf(dx)<=1e-6f) dx=0; if(fabsf(dy)<=1e-6f) dy=0; if(fabsf(dz)<=1e-6f) dz=0;
  p->dx=dx; p->dy=dy; p->dz=dz;
  p->ix= fabsf(dx)>1e-6f ? 1.0f/dx : 1e30f; p->iy= fabsf(dy)>1e-6f ? 1.0f/dy : 1e30f; p->iz= fabsf(dz)>1e-6f ? 1.0f/dz : 1e30f;
  p->sx= dx>0?1:(dx<0?-1:0); p->sy= dy>0?1:(dy<0?-1:0); p->sz= dz>0?1:(dz<0?-1:0);
  p->tdx=fabsf(p->ix); p->tdy=fabsf(p->iy); p->tdz=fabsf(p->iz);
}
typedef struct { int x,y,z,axis,sgn,idx; float t; uint32_t c; } Hit;
#ifdef VOXSTAT
long long g_vsteps=0, g_vskips=0, g_vcasts=0;
#define VST(x) (x)++
#else
#define VST(x)
#endif
static bool cast(const VoxModel *m,float ox,float oy,float oz,const RayP *p,Hit *h,int skip_glow){
  VST(g_vcasts);
  int nx=m->nx, ny=m->ny, nz=m->nz;
  float t0=-1e30f, t1=1e30f; int ax=1;
  if(p->sx){ float a=(0-ox)*p->ix, b=(nx-ox)*p->ix; if(a>b){ float t=a; a=b; b=t; } if(a>t0){ t0=a; ax=0; } if(b<t1) t1=b; } else if(ox<0||ox>=nx) return false;
  if(p->sy){ float a=(0-oy)*p->iy, b=(ny-oy)*p->iy; if(a>b){ float t=a; a=b; b=t; } if(a>t0){ t0=a; ax=1; } if(b<t1) t1=b; } else if(oy<0||oy>=ny) return false;
  if(p->sz){ float a=(0-oz)*p->iz, b=(nz-oz)*p->iz; if(a>b){ float t=a; a=b; b=t; } if(a>t0){ t0=a; ax=2; } if(b<t1) t1=b; } else if(oz<0||oz>=nz) return false;
  if(t0>t1 || t1<=0) return false;
  if(t0<0){ t0=0; }
  float t=t0+1e-4f;
  int x=clampi(ifl(ox+p->dx*t),0,nx-1), y=clampi(ifl(oy+p->dy*t),0,ny-1), z=clampi(ifl(oz+p->dz*t),0,nz-1);
  float tmx = p->sx>0 ? (x+1-ox)*p->ix : (p->sx<0 ? (x-ox)*p->ix : 1e30f);
  float tmy = p->sy>0 ? (y+1-oy)*p->iy : (p->sy<0 ? (y-oy)*p->iy : 1e30f);
  float tmz = p->sz>0 ? (z+1-oz)*p->iz : (p->sz<0 ? (z-oz)*p->iz : 1e30f);
  int sxy=nx*nz; int idx=((y*nz)+z)*nx+x;
  float tc=t0;
  for(;;){
    if(!m->blk[((y>>2)*m->bnz+(z>>2))*m->bnx+(x>>2)]){
      /* bloc vide : on saute directement a sa sortie */
      VST(g_vskips);
      int bx=x>>2, by=y>>2, bz=z>>2;
      float ex = p->sx>0 ? ((bx+1)*4-ox)*p->ix : (p->sx<0 ? (bx*4-ox)*p->ix : 1e30f);
      float ey = p->sy>0 ? ((by+1)*4-oy)*p->iy : (p->sy<0 ? (by*4-oy)*p->iy : 1e30f);
      float ez = p->sz>0 ? ((bz+1)*4-oz)*p->iz : (p->sz<0 ? (bz*4-oz)*p->iz : 1e30f);
      float te; int na;
      if(ex<=ey && ex<=ez){ te=ex; na=0; } else if(ey<=ez){ te=ey; na=1; } else { te=ez; na=2; }
      if(te>=t1) return false;
      tc=te; ax=na;
      x=ifl(ox+p->dx*te); y=ifl(oy+p->dy*te); z=ifl(oz+p->dz*te);
      if(na==0) x= p->sx>0 ? (bx+1)*4 : bx*4-1; else x=clampi(x,bx*4,bx*4+3);
      if(na==1) y= p->sy>0 ? (by+1)*4 : by*4-1; else y=clampi(y,by*4,by*4+3);
      if(na==2) z= p->sz>0 ? (bz+1)*4 : bz*4-1; else z=clampi(z,bz*4,bz*4+3);
      if((unsigned)x>=(unsigned)nx||(unsigned)y>=(unsigned)ny||(unsigned)z>=(unsigned)nz) return false;
      tmx = p->sx>0 ? (x+1-ox)*p->ix : (p->sx<0 ? (x-ox)*p->ix : 1e30f);
      tmy = p->sy>0 ? (y+1-oy)*p->iy : (p->sy<0 ? (y-oy)*p->iy : 1e30f);
      tmz = p->sz>0 ? (z+1-oz)*p->iz : (p->sz<0 ? (z-oz)*p->iz : 1e30f);
      idx=((y*nz)+z)*nx+x;
      continue;
    }
    VST(g_vsteps);
    uint32_t c=m->c[idx];
    if(c && !(skip_glow && ((c>>24)&15)==VM_GLOW)){ h->x=x; h->y=y; h->z=z; h->axis=ax; h->idx=idx; h->t=tc; h->c=c;
      h->sgn = ax==0 ? -p->sx : (ax==1 ? -p->sy : -p->sz); return true; }
    if(tmx<tmy){
      if(tmx<tmz){ x+=p->sx; if((unsigned)x>=(unsigned)nx) return false; idx+=p->sx; tc=tmx; tmx+=p->tdx; ax=0; }
      else { z+=p->sz; if((unsigned)z>=(unsigned)nz) return false; idx+=p->sz*nx; tc=tmz; tmz+=p->tdz; ax=2; }
    } else {
      if(tmy<tmz){ y+=p->sy; if((unsigned)y>=(unsigned)ny) return false; idx+=p->sy*sxy; tc=tmy; tmy+=p->tdy; ax=1; }
      else { z+=p->sz; if((unsigned)z>=(unsigned)nz) return false; idx+=p->sz*nx; tc=tmz; tmz+=p->tdz; ax=2; }
    }
  }
}
static inline bool solid(const VoxModel *m,int x,int y,int z){ uint32_t c=mget(m,x,y,z); return c && ((c>>24)&15)!=VM_GLOW; }
/* occlusion aux coins de la face touchee, interpolee */
static float corner_ao(const VoxModel *m,const Hit *h,float hx,float hy,float hz){
  int o[3]={h->x,h->y,h->z}; o[h->axis]+=h->sgn;
  int t1=(h->axis+1)%3, t2=(h->axis+2)%3;
  float P[3]={hx,hy,hz}; int V[3]={h->x,h->y,h->z};
  float a=clampf(P[t1]-V[t1],0,1), b=clampf(P[t2]-V[t2],0,1);
  float v[4];
  for(int k=0;k<4;k++){
    int s1=(k&1)?1:-1, s2=(k&2)?1:-1;
    int q1[3]={o[0],o[1],o[2]}; q1[t1]+=s1;
    int q2[3]={o[0],o[1],o[2]}; q2[t2]+=s2;
    int q3[3]={o[0],o[1],o[2]}; q3[t1]+=s1; q3[t2]+=s2;
    int s_1=solid(m,q1[0],q1[1],q1[2]), s_2=solid(m,q2[0],q2[1],q2[2]), s_3=solid(m,q3[0],q3[1],q3[2]);
    int occ = (s_1&&s_2) ? 3 : s_1+s_2+s_3;
    static const float L[4]={1.0f,0.80f,0.66f,0.54f};
    v[k]=L[occ];
  }
  return lerpf(lerpf(v[0],v[1],a),lerpf(v[2],v[3],a),b);
}

/* ombres : un rayon par face de voxel, memorise pour l'image en cours */
#define SHC 16384
static uint32_t sh_key[SHC]; static uint16_t sh_stamp[SHC]; static uint8_t sh_val[SHC]; static uint16_t sh_cur=0;
static bool shadowed(const VoxModel *m,const Hit *h,const RayP *lp){
  uint32_t key=(uint32_t)h->idx*6u+(uint32_t)(h->axis*2+(h->sgn>0));
  uint32_t s=(key*2654435761u)>>18;
  for(int k=0;k<8;k++){ uint32_t j=(s+k)&(SHC-1);
    if(sh_stamp[j]==sh_cur && sh_key[j]==key) return sh_val[j];
    if(sh_stamp[j]!=sh_cur){ float o[3]={h->x+0.5f,h->y+0.5f,h->z+0.5f}; o[h->axis]+=h->sgn*0.98f;
      Hit hh; bool r=cast(m,o[0],o[1],o[2],lp,&hh,1);
      sh_stamp[j]=sh_cur; sh_key[j]=key; sh_val[j]=r; return r; } }
  float o[3]={h->x+0.5f,h->y+0.5f,h->z+0.5f}; o[h->axis]+=h->sgn*0.98f; Hit hh; return cast(m,o[0],o[1],o[2],lp,&hh,1);
}

static inline void unpack_n(uint32_t n,float *x,float *y,float *z,float *open){
  *x=(int8_t)(n&255)/127.0f; *y=(int8_t)((n>>8)&255)/127.0f; *z=(int8_t)((n>>16)&255)/127.0f; *open=(n>>24)/255.0f;
}
static inline uint32_t mat_color(uint32_t c,int x,int y,int z,float time,float *glow){
  int mat=(c>>24)&15; uint32_t col=c&0xFFFFFF; *glow=0;
  uint32_t h=vhash(x,y,z);
  switch(mat){
    case VM_GLOW: *glow=1; return col;
    case VM_FIRE: { float f=0.5f+0.5f*sinf(time*11.0f+(h&255)*0.05f+y*0.9f); *glow=1; return col_lerp(HEX(0xff6a1a),HEX(0xffe070),f*((h>>8)&1?1.0f:0.6f))&0xFFFFFF; }
    case VM_WATER: { float f=0.5f+0.5f*sinf(time*2.4f+x*0.8f+z*0.6f); *glow=0.35f; return col_lerp(col|0xFF000000u,HEX(0xa8f0ff),f*0.35f)&0xFFFFFF; }
    case VM_FLAT: case VM_METAL: return col;
    case VM_LEAF: { float j=((h&15)-7.5f)*0.022f; return col_scale(col|0xFF000000u,1.0f+j)&0xFFFFFF; }
    default: { float j=((h&15)-7.5f)*0.007f; return col_scale(col|0xFF000000u,1.0f+j)&0xFFFFFF; }
  }
}

/* ---------------- rendu en rotation ---------------- */
void vox_basis(const VoxCam *cam,float f[3],float r[3],float u[3]){
  float ca=cosf(cam->yaw), sa=sinf(cam->yaw), cp=cosf(cam->pitch), sp=sinf(cam->pitch);
  f[0]=sa*cp; f[1]=-sp; f[2]=ca*cp;
  r[0]=ca; r[1]=0; r[2]=-sa;
  u[0]=f[1]*r[2]-f[2]*r[1]; u[1]=f[2]*r[0]-f[0]*r[2]; u[2]=f[0]*r[1]-f[1]*r[0];
}
/* position ecran d'un point du repere (emetteurs de fumee...) ; renvoie la profondeur */
float vox_project(const VoxModel *m,const VoxCam *cam,float X,float Y,float Z,float *sx,float *sy){
  float f[3],r[3],u[3]; vox_basis(cam,f,r,u);
  float dx=X-m->px, dy=Y-m->py, dz=Z-m->pz;
  *sx=cam->cx+(dx*r[0]+dy*r[1]+dz*r[2])*cam->scale;
  *sy=cam->cy-(dx*u[0]+dy*u[1]+dz*u[2])*cam->scale;
  return dx*f[0]+dy*f[1]+dz*f[2];
}
/* enveloppe : pour chaque carre de 2x2 colonnes, les coins hauts et bas (repere de travail) */
int vox_hull(const VoxModel *m,float **pts){
  int cx=(m->nx+1)/2, cz=(m->nz+1)/2, n=0;
  float *p=(float*)malloc((size_t)cx*cz*8*3*sizeof(float));
  for(int j=0;j<cz;j++) for(int i=0;i<cx;i++){
    int lo=1<<30, hi=-1;
    for(int dz=0;dz<2;dz++) for(int dx=0;dx<2;dx++){ int x=i*2+dx, z=j*2+dz; if(x>=m->nx||z>=m->nz) continue;
      for(int y=0;y<m->ny;y++) if(m->c[((y*m->nz)+z)*m->nx+x]){ if(y<lo) lo=y; if(y>hi) hi=y; } }
    if(hi<0) continue;
    for(int k=0;k<8;k++){ p[n*3]=(float)(m->x0+i*2+((k&1)?2:0)); p[n*3+1]=(float)(m->y0+((k&2)?hi+1:lo)); p[n*3+2]=(float)(m->z0+j*2+((k&4)?2:0)); n++; }
  }
  *pts=p; return n;
}
/* echelle et decalage vertical pour que la maquette tienne dans w x h quelle que soit l'orientation */
void vox_fit_pts(const VoxModel *m,const float *p,int n,float pitch,int w,int h,float maxscale,float *scale,float *cyoff){
  float minx=1e9f,maxx=-1e9f,miny=1e9f,maxy=-1e9f;
  for(int a=0;a<16;a++){
    VoxCam c={a*TAU/16+0.1f,pitch,1,0,0}; float f[3],r[3],u[3]; vox_basis(&c,f,r,u);
    for(int k=0;k<n;k++){ float dx=p[k*3]-m->px, dy=p[k*3+1]-m->py, dz=p[k*3+2]-m->pz;
      float sx=dx*r[0]+dy*r[1]+dz*r[2], sy=-(dx*u[0]+dy*u[1]+dz*u[2]);
      if(sx<minx)minx=sx; if(sx>maxx)maxx=sx; if(sy<miny)miny=sy; if(sy>maxy)maxy=sy; }
  }
  float s=minf((w)/(maxx-minx),(h)/(maxy-miny)); if(s>maxscale) s=maxscale;
  *scale=s; *cyoff=-(miny+maxy)*0.5f*s;
}
void vox_fit(const VoxModel *m,float pitch,int w,int h,float maxscale,float *scale,float *cyoff){
  float *p; int n=vox_hull(m,&p); vox_fit_pts(m,p,n,pitch,w,h,maxscale,scale,cyoff); free(p);
}

/* lumiere de studio (repere de la camera : x droite, y haut, z vers le spectateur) ; (0,0,0) : celle des maquettes */
static float g_vlight[3]={-0.58f,0.74f,0.36f};
void vox_set_light(float x,float y,float z){
  if(x==0 && y==0 && z==0){ g_vlight[0]=-0.58f; g_vlight[1]=0.74f; g_vlight[2]=0.36f; return; }
  g_vlight[0]=x; g_vlight[1]=y; g_vlight[2]=z;
}
void vox_render(const VoxModel *m,float time,int flags,const VoxCam *cam,Sprite *out){
  int W=out->w, H=out->h;
  memset(out->px,0,(size_t)W*H*4); if(out->em) memset(out->em,0,(size_t)W*H*4);
  if(!m->c || m->nx<=0) return;
  float f[3],r[3],u[3]; vox_basis(cam,f,r,u);
  /* lumiere de studio (fixe par rapport a la camera) : haut gauche, un peu de face */
  float lc[3]={g_vlight[0],g_vlight[1],g_vlight[2]}; { float l=sqrtf(lc[0]*lc[0]+lc[1]*lc[1]+lc[2]*lc[2]); lc[0]/=l; lc[1]/=l; lc[2]/=l; }
  float L[3]; for(int k=0;k<3;k++) L[k]=r[k]*lc[0]+u[k]*lc[1]-f[k]*lc[2];
  RayP rp, lp; rayp(&rp,f[0],f[1],f[2]); rayp(&lp,L[0],L[1],L[2]);
  if(++sh_cur==0){ memset(sh_stamp,0,sizeof sh_stamp); sh_cur=1; }
  float s=cam->scale, T=400.0f;
  float pvx=m->px-m->x0, pvy=m->py-m->y0, pvz=m->pz-m->z0;
  /* boite a l'ecran */
  int bx0=W,bx1=0,by0=H,by1=0;
  for(int k=0;k<8;k++){ float X=(k&1)?m->x0+m->nx:m->x0, Y=(k&2)?m->y0+m->ny:m->y0, Z=(k&4)?m->z0+m->nz:m->z0; float sx,sy; vox_project(m,cam,X,Y,Z,&sx,&sy);
    bx0=mini(bx0,(int)sx-1); bx1=maxi(bx1,(int)sx+1); by0=mini(by0,(int)sy-1); by1=maxi(by1,(int)sy+1); }
  bx0=clampi(bx0,0,W); bx1=clampi(bx1,0,W); by0=clampi(by0,0,H); by1=clampi(by1,0,H);
  /* tuiles de 8x8 pixels touchees par la silhouette (colonnes de 2x2 voxels projetees) */
  int TW=(W+7)/8, TH=(H+7)/8; static uint8_t *tmask=NULL; static int tcap=0;
  if(tcap<TW*TH){ free(tmask); tcap=TW*TH; tmask=(uint8_t*)malloc((size_t)tcap); }
  memset(tmask,0,(size_t)TW*TH);
  { float hw=1.6f*s+2.0f;
    for(int j=0;j<m->nz;j+=2) for(int i=0;i<m->nx;i+=2){
      int lo=1<<30, hi=-1;
      for(int dz=0;dz<2 && j+dz<m->nz;dz++) for(int dx=0;dx<2 && i+dx<m->nx;dx++){ int x=i+dx, z=j+dz;
        for(int y=0;y<m->ny;y++) if(m->c[((y*m->nz)+z)*m->nx+x]){ if(y<lo) lo=y; break; }
        for(int y=m->ny-1;y>=0;y--) if(m->c[((y*m->nz)+z)*m->nx+x]){ if(y>hi) hi=y; break; } }
      if(hi<0) continue;
      float X=m->x0+i+1.0f, Z=m->z0+j+1.0f, sx0,sy0,sx1,sy1;
      vox_project(m,cam,X,(float)(m->y0+lo),Z,&sx0,&sy0); vox_project(m,cam,X,(float)(m->y0+hi+1),Z,&sx1,&sy1);
      int tx0=ifl((minf(sx0,sx1)-hw)/8), tx1=ifl((maxf(sx0,sx1)+hw)/8), ty0=ifl((minf(sy0,sy1)-hw)/8), ty1=ifl((maxf(sy0,sy1)+hw)/8);
      tx0=clampi(tx0,0,TW-1); tx1=clampi(tx1,0,TW-1); ty0=clampi(ty0,0,TH-1); ty1=clampi(ty1,0,TH-1);
      for(int ty=ty0;ty<=ty1;ty++) memset(&tmask[ty*TW+tx0],1,(size_t)(tx1-tx0+1));
    } }
  float pulse=0.5f+0.5f*sinf(time*5.0f);
  for(int py=by0;py<by1;py++){
    float vs=(py+0.5f-cam->cy)/s;
    const uint8_t *trow=&tmask[(py>>3)*TW];
    for(int px=bx0;px<bx1;px++){
      if(!trow[px>>3]){ px|=7; continue; }
      float us=(px+0.5f-cam->cx)/s;
      float ox=pvx+r[0]*us-u[0]*vs-f[0]*T, oy=pvy+r[1]*us-u[1]*vs-f[1]*T, oz=pvz+r[2]*us-u[2]*vs-f[2]*T;
      Hit h; if(!cast(m,ox,oy,oz,&rp,&h,0)) continue;
      float glow; uint32_t col=mat_color(h.c,h.x+m->x0,h.y+m->y0,h.z+m->z0,time,&glow);
      int mat=(h.c>>24)&15;
      float R=cr(col)/255.0f, G=cg(col)/255.0f, B=cb(col)/255.0f, eR=0,eG=0,eB=0;
      if(glow<1.0f){
        float nf[3]={0,0,0}; nf[h.axis]=(float)h.sgn;
        float nsx,nsy,nsz,open; unpack_n(m->n[h.idx],&nsx,&nsy,&nsz,&open);
        float wf=(flags&VR_SMOOTH)?0.12f:0.6f, ws=1.0f-wf;
        float n0=nf[0]*wf+nsx*ws, n1=nf[1]*wf+nsy*ws, n2=nf[2]*wf+nsz*ws; float nl=sqrtf(n0*n0+n1*n1+n2*n2); n0/=nl; n1/=nl; n2/=nl;
        float hx=ox+f[0]*h.t, hy=oy+f[1]*h.t, hz=oz+f[2]*h.t;
        float ao=corner_ao(m,&h,hx,hy,hz)*(0.72f+0.28f*open);
        if(flags&VR_SMOOTH) ao=1.0f-(1.0f-ao)*0.35f;
        float ndl=n0*L[0]+n1*L[1]+n2*L[2];
        float key= ndl>0 ? ndl : 0; if(key>0 && shadowed(m,&h,&lp)) key*=0.12f;
        float hemi=0.5f+0.5f*n1;
        float amb=(0.30f+0.20f*hemi)*ao;
        float kr=amb*0.86f+key*0.98f, kg=amb*0.88f+key*0.90f, kb=amb*1.06f+key*0.76f;
        float vd=-(n0*f[0]+n1*f[1]+n2*f[2]); float rv=clampf(1.0f-vd,0,1); float rim=rv*rv*rv*0.20f;
        R=R*kr+rim*0.55f; G=G*kg+rim*0.6f; B=B*kb+rim*0.9f;
        if(mat==VM_METAL){ float rx=2*ndl*n0-L[0], ry=2*ndl*n1-L[1], rz=2*ndl*n2-L[2]; float sp=-(rx*f[0]+ry*f[1]+rz*f[2]); if(sp>0){ float s2=sp*sp, s4=s2*s2, s8=s4*s4; sp=s8*s4*s2*0.75f*(key>0.05f?1.0f:0.2f); R+=sp; G+=sp; B+=sp*0.9f; } }
        if(glow>0){ float k=glow; R=R*(1-k)+cr(col)/255.0f*k*1.05f; G=G*(1-k)+cg(col)/255.0f*k*1.05f; B=B*(1-k)+cb(col)/255.0f*k*1.05f; eR=R*0.25f; eG=G*0.25f; eB=B*0.3f; }
      } else { eR=R*0.85f; eG=G*0.85f; eB=B*0.85f; }
      if((flags&VR_PREVIEW) && (h.c&0x80000000u)){
        float k=0.35f+0.3f*pulse; R=lerpf(R,0.55f,k); G=lerpf(G,1.0f,k); B=lerpf(B,1.0f,k); eR+=0.12f*k; eG+=0.35f*k; eB+=0.4f*k; }
      int i=py*W+px;
      out->px[i]=rgb((int)(R*255),(int)(G*255),(int)(B*255));
      if(out->em && (eR+eG+eB)>0.01f) out->em[i]=rgb((int)(eR*255),(int)(eG*255),(int)(eB*255));
    }
  }
  /* contour sombre (alpha 0xFE : ne se propage pas) */
  for(int py=maxi(by0-1,0);py<mini(by1+1,H);py++) for(int px=maxi(bx0-1,0);px<mini(bx1+1,W);px++){
    int i=py*W+px; if(out->px[i]>>24) continue;
    int rr=0,gg=0,bb=0,n=0; uint32_t c;
    if(px>0 && ((c=out->px[i-1])>>24)==0xFF){ rr+=cr(c); gg+=cg(c); bb+=cb(c); n++; }
    if(px<W-1 && ((c=out->px[i+1])>>24)==0xFF){ rr+=cr(c); gg+=cg(c); bb+=cb(c); n++; }
    if(py>0 && ((c=out->px[i-W])>>24)==0xFF){ rr+=cr(c); gg+=cg(c); bb+=cb(c); n++; }
    if(py<H-1 && ((c=out->px[i+W])>>24)==0xFF){ rr+=cr(c); gg+=cg(c); bb+=cb(c); n++; }
    if(n) out->px[i]=0xFE000000u|(rgb(18+(rr/n)*12/100,14+(gg/n)*12/100,24+(bb/n)*14/100)&0xFFFFFF);
  }
}

/* copie dans la cible courante + halo des parties lumineuses (flou a demi-resolution) */
void vox_blit(const Sprite *img,int x0,int y0,float glow){
  int W=img->w, H=img->h;
  for(int y=0;y<H;y++){ int py=y0+y; if(py<gT->cy0||py>=gT->cy1) continue;
    for(int x=0;x<W;x++){ int px=x0+x; if(px<gT->cx0||px>=gT->cx1) continue;
      uint32_t c=img->px[y*W+x]; if(c>>24) gT->px[py*gT->w+px]=c|0xFF000000u; } }
  if(glow<=0 || !img->em) return;
  int w2=(W+1)/2, h2=(H+1)/2;
  static float *A=NULL,*Bf=NULL; static int cap=0;
  if(cap<w2*h2*3){ free(A); free(Bf); cap=w2*h2*3; A=(float*)malloc((size_t)cap*4); Bf=(float*)malloc((size_t)cap*4); }
  memset(A,0,(size_t)w2*h2*12);
  bool any=false;
  for(int y=0;y<H;y++) for(int x=0;x<W;x++){ uint32_t e=img->em[y*W+x]; if(!e) continue; any=true; int j=((y/2)*w2+(x/2))*3; A[j]+=cr(e)*0.25f; A[j+1]+=cg(e)*0.25f; A[j+2]+=cb(e)*0.25f; }
  if(!any) return;
  for(int pass=0;pass<2;pass++){
    const int R=3;
    for(int y=0;y<h2;y++) for(int x=0;x<w2;x++){ float s0=0,s1=0,s2=0; for(int k=-R;k<=R;k++){ int xx=clampi(x+k,0,w2-1); int j=(y*w2+xx)*3; s0+=A[j]; s1+=A[j+1]; s2+=A[j+2]; } int j=(y*w2+x)*3; Bf[j]=s0/(2*R+1); Bf[j+1]=s1/(2*R+1); Bf[j+2]=s2/(2*R+1); }
    for(int y=0;y<h2;y++) for(int x=0;x<w2;x++){ float s0=0,s1=0,s2=0; for(int k=-R;k<=R;k++){ int yy=clampi(y+k,0,h2-1); int j=(yy*w2+x)*3; s0+=Bf[j]; s1+=Bf[j+1]; s2+=Bf[j+2]; } int j=(y*w2+x)*3; A[j]=s0/(2*R+1); A[j+1]=s1/(2*R+1); A[j+2]=s2/(2*R+1); }
  }
  float k=glow*1.6f;
  for(int y=0;y<H;y++){ int py=y0+y; if(py<gT->cy0||py>=gT->cy1) continue;
    for(int x=0;x<W;x++){ int px=x0+x; if(px<gT->cx0||px>=gT->cx1) continue;
      int j=((y/2)*w2+(x/2))*3; float a=A[j]*k, b=A[j+1]*k, c=A[j+2]*k; if(a+b+c<2) continue;
      uint32_t *d=&gT->px[py*gT->w+px]; *d=rgb(cr(*d)+(int)a,cg(*d)+(int)b,cb(*d)+(int)c); } }
}

/* ---------------- sprite du campement : vue oblique (face 1:1, dessus a 1/2) ---------------- */
#define OBL 0.5f
void vox_camp_sprite(const VoxModel *m,Sprite *s,int *ax,int *ay){
  float Ytop=m->ny+OBL*m->nz;
  int W=m->nx+2, H=(int)ceilf(Ytop)+2;
  if(s->px){ free(s->px); free(s->nm); free(s->em); }
  *s=spr_alloc(W,H); s->nm=(uint32_t*)calloc((size_t)W*H,4);
  RayP rp; rayp(&rp,0,-OBL,1);
  float vn=1.0f/sqrtf(1+OBL*OBL);
  bool anyem=false;
  for(int sy=0;sy<H;sy++) for(int sx=0;sx<W;sx++){
    int x=sx-1; if(x<0||x>=m->nx) continue;
    float syc=sy-1+0.25f; float oz=-0.01f, oy=Ytop-syc-OBL*oz, ox=x+0.5f;
    Hit h; if(!cast(m,ox,oy,oz,&rp,&h,0)) continue;
    float glow; uint32_t col=mat_color(h.c,h.x+m->x0,h.y+m->y0,h.z+m->z0,0,&glow);
    int mat=(h.c>>24)&15;
    float nsx,nsy,nsz,open; unpack_n(m->n[h.idx],&nsx,&nsy,&nsz,&open);
    float nf[3]={0,0,0}; nf[h.axis]=(float)h.sgn;
    float n0=nf[0]*0.55f+nsx*0.45f, n1=nf[1]*0.55f+nsy*0.45f, n2=nf[2]*0.55f+nsz*0.45f; float nl=sqrtf(n0*n0+n1*n1+n2*n2); n0/=nl; n1/=nl; n2/=nl;
    uint32_t out;
    if(glow>=1.0f){ out=col|0xFF000000u; }
    else {
      float hx=ox, hy=oy-OBL*h.t, hz=oz+h.t;
      float ao=corner_ao(m,&h,hx,hy,hz)*(0.8f+0.2f*open);
      float tone = h.axis==1 ? 1.10f : 0.94f;
      tone *= 1.0f-0.07f*n0;
      tone *= ao;
      out=col_scale(col|0xFF000000u,tone);
      if(glow>0) out=col_lerp(out,col|0xFF000000u,glow);
    }
    int i=sy*W+sx; s->px[i]=out;
    if(glow>0){ if(!s->em) s->em=(uint32_t*)calloc((size_t)W*H,4); s->em[i]=col_scale(col|0xFF000000u,glow>=1?0.9f:0.4f); anyem=true; }
    /* normale a l'ecran : x droite, y bas, z vers le spectateur */
    float U[3]={0,vn,OBL*vn}, V[3]={0,OBL*vn,-vn};
    float ex=n0, ey=-(n0*U[0]+n1*U[1]+n2*U[2]), ez=n0*V[0]+n1*V[1]+n2*V[2];
    ez=ez*0.7f+0.3f; { float l=sqrtf(ex*ex+ey*ey+ez*ez); ex/=l; ey/=l; ez/=l; }
    int spec = mat==VM_METAL ? 190 : (glow>0 ? 0 : 22);
    s->nm[i]=((uint32_t)spec<<24)|((uint32_t)clampi((int)(ex*127.5f+128),0,255)<<16)|((uint32_t)clampi((int)(ey*127.5f+128),0,255)<<8)|(uint32_t)clampi((int)(ez*127.5f+128),0,255);
  }
  (void)anyem;
  /* contour */
  uint8_t *mk=(uint8_t*)calloc((size_t)W*H,1);
  for(int y=0;y<H;y++) for(int x=0;x<W;x++){ int i=y*W+x; if(s->px[i]>>24) continue;
    if((x>0&&(s->px[i-1]>>24))||(x<W-1&&(s->px[i+1]>>24))||(y>0&&(s->px[i-W]>>24))||(y<H-1&&(s->px[i+W]>>24))) mk[i]=1; }
  for(int i=0;i<W*H;i++) if(mk[i]){ s->px[i]=HEX(0x120e18); s->nm[i]=0x108080FFu; }
  free(mk);
  *ax=(int)floorf(m->px-m->x0)+1;
  *ay=(int)floorf(Ytop-(m->py-m->y0)-OBL*(m->fz-m->z0)+0.5f)+1;
}
/* position d'un point du repere dans le sprite du campement */
void vox_camp_point(const VoxModel *m,float X,float Y,float Z,float *sx,float *sy){
  float Ytop=m->ny+OBL*m->nz;
  *sx=(X-m->x0)+1; *sy=Ytop-(Y-m->y0)-OBL*(Z-m->z0)+1;
}
