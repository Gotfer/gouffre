/* GOUFFRE - conversion des manettes "DirectInput / WinMM" (PlayStation, Nintendo, generiques)
   vers l'etat de manette du jeu. Sans dependance Windows : testable sous Linux. */
#ifndef PADMAP_H
#define PADMAP_H
#include "platform.h"
#include <string.h>
#include <stdio.h>

#define VID_MICROSOFT 0x045E
#define VID_SONY      0x054C
#define VID_NINTENDO  0x057E

enum { JL_GENERIC, JL_SONY, JL_NINTENDO };
enum { JA_X, JA_Y, JA_Z, JA_R, JA_U, JA_V, JA_COUNT };

typedef struct {
  unsigned ax[JA_COUNT];          /* positions brutes */
  unsigned mn[JA_COUNT], mx[JA_COUNT];
  bool has[JA_COUNT];             /* axe present */
  unsigned buttons;               /* bit 0 = bouton 1 */
  unsigned pov;                   /* centiemes de degre, 0xFFFF = centre */
  bool has_pov;
} JoyRaw;

static inline int joy_layout_for(unsigned short vid){
  if(vid==VID_SONY) return JL_SONY;
  if(vid==VID_NINTENDO) return JL_NINTENDO;
  return JL_GENERIC;
}
static inline int joy_pad_type(int layout){
  return layout==JL_SONY ? PADT_PS : (layout==JL_NINTENDO ? PADT_NINTENDO : PADT_GENERIC);
}
static inline float joy_norm(unsigned v,unsigned mn,unsigned mx){
  if(mx<=mn) return 0;
  float c=((float)mn+(float)mx)*0.5f, h=((float)mx-(float)mn)*0.5f;
  float f=((float)v-c)/h;
  return f<-1?-1:(f>1?1:f);
}
static inline float joy_trig(unsigned v,unsigned mn,unsigned mx){
  if(mx<=mn) return 0;
  float f=((float)v-(float)mn)/((float)mx-(float)mn);
  return f<0?0:(f>1?1:f);
}

/* indices des boutons (base 0) par disposition */
static const signed char JOY_BTN[3][PAD_COUNT] = {
  /*            A  B  X  Y  START BACK LB RB  (croix via le POV) */
  [JL_GENERIC]  = {0, 1, 2, 3, 7, 6, 4, 5, -1,-1,-1,-1},
  [JL_SONY]     = {1, 2, 0, 3, 9, 8, 4, 5, -1,-1,-1,-1},   /* Croix, Rond, Carre, Triangle, Options, Creer */
  [JL_NINTENDO] = {1, 0, 3, 2, 9, 8, 4, 5, -1,-1,-1,-1},   /* A a droite, B en bas (positions Nintendo) */
};

/* remplit l'etat manette de 'in' (sticks, boutons, fronts) a partir des valeurs brutes */
static inline void joy_to_input(const JoyRaw *j,int layout,Input *in){
  float a[JA_COUNT];
  for(int i=0;i<JA_COUNT;i++) a[i] = j->has[i] ? joy_norm(j->ax[i],j->mn[i],j->mx[i]) : 0.0f;
  in->lx=a[JA_X]; in->ly=a[JA_Y];
  /* stick droit : Z/R (PlayStation et la plupart des manettes), sinon U/V */
  if(j->has[JA_Z] && j->has[JA_R]){ in->rx=a[JA_Z]; in->ry=a[JA_R]; }
  else if(j->has[JA_U] && j->has[JA_V]){ in->rx=a[JA_U]; in->ry=a[JA_V]; }
  else { in->rx=0; in->ry=0; }
  if(layout==JL_SONY){
    in->lt = j->has[JA_U] ? joy_trig(j->ax[JA_U],j->mn[JA_U],j->mx[JA_U]) : 0;
    in->rt = j->has[JA_V] ? joy_trig(j->ax[JA_V],j->mn[JA_V],j->mx[JA_V]) : 0;
  } else { in->lt=0; in->rt=0; }
  bool now[PAD_COUNT]; memset(now,0,sizeof now);
  for(int b=0;b<PAD_COUNT;b++){ int idx=JOY_BTN[layout][b]; if(idx>=0) now[b]=((j->buttons>>idx)&1u)!=0; }
  if(layout==JL_SONY && ((j->buttons>>13)&1u)) now[PAD_BACK]=true;   /* pave tactile = Creer */
  if(j->has_pov && j->pov<=36000u){
    float d=(float)j->pov/100.0f;
    now[PAD_UP]    = d>=292.5f || d<=67.5f;
    now[PAD_RIGHT] = d>=22.5f && d<=157.5f;
    now[PAD_DOWN]  = d>=112.5f && d<=247.5f;
    now[PAD_LEFT]  = d>=202.5f && d<=337.5f;
  }
  for(int b=0;b<PAD_COUNT;b++){ if(now[b] && !in->pad[b]) in->pad_pressed[b]=true; in->pad[b]=now[b]; }
}

/* ---------- lecture directe des manettes PlayStation (rapports HID bruts) ----------
   DualSense USB : rapport 0x01 de 64 octets (format complet)
   DualSense Bluetooth : rapport 0x01 "simple" (78 octets lus sous Windows) ou 0x31 (complet)
   DualShock 4 : rapport 0x01 (USB/Bluetooth simple) ou 0x11 (Bluetooth complet) */
typedef struct { unsigned char lx,ly,rx,ry,l2,r2,b0,b1,b2; } SonyState;
static inline bool sony_is_ds5(unsigned short pid){ return pid==0x0CE6 || pid==0x0DF2; }
static inline bool sony_parse(const unsigned char *d,int len,bool ds5,SonyState *s){
  const unsigned char *p; bool full;
  if(len<10) return false;
  if(d[0]==0x01){ p=d+1; full = ds5 && len==64; }
  else if(d[0]==0x31 && ds5 && len>=12){ p=d+2; full=true; }
  else if(d[0]==0x11 && !ds5 && len>=13){ p=d+3; full=false; }
  else return false;
  s->lx=p[0]; s->ly=p[1]; s->rx=p[2]; s->ry=p[3];
  if(full){ s->l2=p[4]; s->r2=p[5]; s->b0=p[7]; s->b1=p[8]; s->b2=p[9]; }
  else    { s->b0=p[4]; s->b1=p[5]; s->b2=p[6]; s->l2=p[7]; s->r2=p[8]; }
  return true;
}
static inline float sony_axis(unsigned char v){ float f=((float)v-128.0f)/127.5f; return f<-1?-1:(f>1?1:f); }
static inline void sony_to_input(const SonyState *s,Input *in){
  in->lx=sony_axis(s->lx); in->ly=sony_axis(s->ly); in->rx=sony_axis(s->rx); in->ry=sony_axis(s->ry);
  in->lt=s->l2/255.0f; in->rt=s->r2/255.0f;
  int hat=s->b0&0x0F;
  bool now[PAD_COUNT]; memset(now,0,sizeof now);
  now[PAD_X]=(s->b0&0x10)!=0;  /* Carre */
  now[PAD_A]=(s->b0&0x20)!=0;  /* Croix */
  now[PAD_B]=(s->b0&0x40)!=0;  /* Rond */
  now[PAD_Y]=(s->b0&0x80)!=0;  /* Triangle */
  now[PAD_LB]=(s->b1&0x01)!=0; now[PAD_RB]=(s->b1&0x02)!=0;
  now[PAD_BACK]=(s->b1&0x10)!=0 || (s->b2&0x02)!=0;   /* Creer / pave tactile */
  now[PAD_START]=(s->b1&0x20)!=0;                      /* Options */
  now[PAD_UP]= hat==7||hat==0||hat==1; now[PAD_RIGHT]= hat>=1&&hat<=3;
  now[PAD_DOWN]= hat>=3&&hat<=5;       now[PAD_LEFT]= hat>=5&&hat<=7;
  for(int b=0;b<PAD_COUNT;b++){ if(now[b] && !in->pad[b]) in->pad_pressed[b]=true; in->pad[b]=now[b]; }
}

static inline void joy_friendly_name(unsigned short vid,unsigned short pid,const char *raw,char *out,int n){
  if(vid==VID_SONY){
    if(pid==0x0CE6||pid==0x0DF2) snprintf(out,(size_t)n,"DualSense (PS5)");
    else if(pid==0x05C4||pid==0x09CC||pid==0x0BA0) snprintf(out,(size_t)n,"DualShock 4 (PS4)");
    else snprintf(out,(size_t)n,"Manette PlayStation");
  } else if(vid==VID_NINTENDO) snprintf(out,(size_t)n,"Manette Nintendo");
  else snprintf(out,(size_t)n,"%s",(raw&&raw[0])?raw:"Manette");
}
#endif
