/* Test unitaire de la conversion des manettes (src/padmap.h) */
#include <stdio.h>
#include <math.h>
#include "../src/padmap.h"
static int fails=0, checks=0;
#define CHECK(c,msg) do{ checks++; if(!(c)){ fails++; printf("ECHEC : %s\n",msg); } }while(0)
static JoyRaw rest(bool hasR,bool hasUV){
  JoyRaw j; memset(&j,0,sizeof j);
  for(int i=0;i<JA_COUNT;i++){ j.mn[i]=0; j.mx[i]=65535; j.ax[i]=32767; }
  j.has[JA_X]=j.has[JA_Y]=j.has[JA_Z]=true; j.has[JA_R]=hasR; j.has[JA_U]=j.has[JA_V]=hasUV;
  j.ax[JA_U]=0; j.ax[JA_V]=0; /* gachettes relachees */
  j.pov=0xFFFF; j.has_pov=true; return j;
}
int main(void){
  Input in; memset(&in,0,sizeof in);
  /* DualSense au repos */
  JoyRaw j=rest(true,true);
  joy_to_input(&j,JL_SONY,&in);
  CHECK(fabsf(in.lx)<0.01f && fabsf(in.ly)<0.01f && fabsf(in.rx)<0.01f && fabsf(in.ry)<0.01f,"sticks centres au repos");
  int any=0; for(int b=0;b<PAD_COUNT;b++) any|=in.pad[b]; CHECK(!any,"aucun bouton au repos");
  CHECK(in.lt<0.01f && in.rt<0.01f,"gachettes relachees");
  /* Croix -> A (front montant puis plus de front) */
  memset(in.pad_pressed,0,sizeof in.pad_pressed); j.buttons=1u<<1; joy_to_input(&j,JL_SONY,&in);
  CHECK(in.pad[PAD_A] && in.pad_pressed[PAD_A],"Croix = valider (A)");
  memset(in.pad_pressed,0,sizeof in.pad_pressed); joy_to_input(&j,JL_SONY,&in);
  CHECK(in.pad[PAD_A] && !in.pad_pressed[PAD_A],"pas de double appui si on reste appuye");
  memset(in.pad_pressed,0,sizeof in.pad_pressed); j.buttons=1u<<2; joy_to_input(&j,JL_SONY,&in);
  CHECK(in.pad_pressed[PAD_B] && !in.pad[PAD_A],"Rond = retour (B)");
  memset(in.pad_pressed,0,sizeof in.pad_pressed); j.buttons=1u<<3; joy_to_input(&j,JL_SONY,&in);
  CHECK(in.pad_pressed[PAD_Y],"Triangle = Y");
  memset(in.pad_pressed,0,sizeof in.pad_pressed); j.buttons=1u<<0; joy_to_input(&j,JL_SONY,&in);
  CHECK(in.pad_pressed[PAD_X],"Carre = X");
  memset(in.pad_pressed,0,sizeof in.pad_pressed); j.buttons=1u<<9; joy_to_input(&j,JL_SONY,&in);
  CHECK(in.pad_pressed[PAD_START],"Options = Start");
  memset(in.pad_pressed,0,sizeof in.pad_pressed); j.buttons=1u<<8; joy_to_input(&j,JL_SONY,&in);
  CHECK(in.pad_pressed[PAD_BACK],"Creer = Back");
  memset(in.pad_pressed,0,sizeof in.pad_pressed); j.buttons=0; joy_to_input(&j,JL_SONY,&in);
  memset(in.pad_pressed,0,sizeof in.pad_pressed); j.buttons=1u<<13; joy_to_input(&j,JL_SONY,&in);
  CHECK(in.pad_pressed[PAD_BACK],"pave tactile = Back");
  j.buttons=0;
  /* croix directionnelle (POV) */
  struct { unsigned pov; bool u,r,d,l; const char *n; } P[]={
    {0,1,0,0,0,"haut"},{9000,0,1,0,0,"droite"},{18000,0,0,1,0,"bas"},{27000,0,0,0,1,"gauche"},
    {4500,1,1,0,0,"haut-droite"},{31500,1,0,0,1,"haut-gauche"},{0xFFFF,0,0,0,0,"centre"} };
  for(int i=0;i<7;i++){
    j.pov=P[i].pov; joy_to_input(&j,JL_SONY,&in);
    char m[64]; snprintf(m,sizeof m,"POV %s",P[i].n);
    CHECK(in.pad[PAD_UP]==P[i].u && in.pad[PAD_RIGHT]==P[i].r && in.pad[PAD_DOWN]==P[i].d && in.pad[PAD_LEFT]==P[i].l,m);
  }
  j.pov=0xFFFF;
  /* sticks : Y vers le bas positif, stick droit sur Z / R */
  j.ax[JA_X]=0; j.ax[JA_Y]=65535; j.ax[JA_Z]=65535; j.ax[JA_R]=0; joy_to_input(&j,JL_SONY,&in);
  CHECK(in.lx<-0.99f && in.ly>0.99f,"stick gauche bas-gauche");
  CHECK(in.rx>0.99f && in.ry<-0.99f,"stick droit haut-droite (Z/R)");
  j.ax[JA_U]=65535; joy_to_input(&j,JL_SONY,&in); CHECK(in.lt>0.99f,"L2 enfoncee");
  /* manette generique sans axe R : stick droit sur U/V */
  JoyRaw g=rest(false,true); g.ax[JA_U]=65535; g.ax[JA_V]=32767; Input gi; memset(&gi,0,sizeof gi);
  joy_to_input(&g,JL_GENERIC,&gi); CHECK(gi.rx>0.99f && fabsf(gi.ry)<0.01f,"generique : stick droit sur U/V");
  g.buttons=1u<<0; joy_to_input(&g,JL_GENERIC,&gi); CHECK(gi.pad[PAD_A],"generique : bouton 1 = A");
  /* Nintendo : A a droite (bouton 2), B en bas (bouton 1) */
  JoyRaw nt=rest(true,false); Input ni; memset(&ni,0,sizeof ni);
  nt.buttons=1u<<1; joy_to_input(&nt,JL_NINTENDO,&ni); CHECK(ni.pad[PAD_A] && !ni.pad[PAD_B],"Nintendo : A");
  nt.buttons=1u<<0; joy_to_input(&nt,JL_NINTENDO,&ni); CHECK(ni.pad[PAD_B] && !ni.pad[PAD_A],"Nintendo : B");
  /* noms et types */
  char nm[64];
  joy_friendly_name(0x054C,0x0CE6,"Wireless Controller",nm,64); CHECK(!strcmp(nm,"DualSense (PS5)"),"nom DualSense");
  joy_friendly_name(0x054C,0x09CC,"Wireless Controller",nm,64); CHECK(!strcmp(nm,"DualShock 4 (PS4)"),"nom DualShock 4");
  joy_friendly_name(0x1234,0x0001,"Ma manette",nm,64); CHECK(!strcmp(nm,"Ma manette"),"nom generique");
  CHECK(joy_layout_for(0x054C)==JL_SONY && joy_pad_type(JL_SONY)==PADT_PS,"Sony = PlayStation");
  CHECK(joy_layout_for(0x057E)==JL_NINTENDO,"Nintendo");
  CHECK(joy_layout_for(0x2DC8)==JL_GENERIC && joy_pad_type(JL_GENERIC)==PADT_GENERIC,"autre = generique");
  /* ---------- lecture directe PlayStation ---------- */
  {
    unsigned char r[78]; SonyState st; Input si; memset(&si,0,sizeof si);
    /* DualSense USB : rapport 0x01, 64 octets, format complet */
    memset(r,0,sizeof r); r[0]=0x01; r[1]=128; r[2]=128; r[3]=255; r[4]=0; r[5]=10; r[6]=200; r[8]=0x08|0x20; r[9]=0x20; r[10]=0;
    CHECK(sony_parse(r,64,true,&st),"DualSense USB lu");
    sony_to_input(&st,&si);
    CHECK(si.pad[PAD_A] && !si.pad[PAD_B] && !si.pad[PAD_X] && !si.pad[PAD_Y],"DualSense USB : Croix seule");
    CHECK(si.pad[PAD_START],"DualSense USB : Options");
    CHECK(!si.pad[PAD_UP]&&!si.pad[PAD_DOWN]&&!si.pad[PAD_LEFT]&&!si.pad[PAD_RIGHT],"DualSense USB : croix relachee (8)");
    CHECK(fabsf(si.lx)<0.01f && si.rx>0.99f && si.ry<-0.99f,"DualSense USB : sticks");
    CHECK(si.rt>0.75f && si.lt<0.05f,"DualSense USB : gachettes");
    /* DualSense Bluetooth simple : rapport 0x01, lu en 78 octets */
    memset(r,0,sizeof r); r[0]=0x01; r[1]=0; r[2]=128; r[3]=128; r[4]=128; r[5]=0x02|0x40; r[6]=0x01; r[7]=0x02; r[8]=0; r[9]=0;
    memset(&si,0,sizeof si); CHECK(sony_parse(r,78,true,&st),"DualSense Bluetooth simple lu");
    sony_to_input(&st,&si);
    CHECK(si.pad[PAD_B] && si.pad[PAD_RIGHT] && !si.pad[PAD_UP],"DualSense BT : Rond + droite");
    CHECK(si.pad[PAD_LB] && si.pad[PAD_BACK],"DualSense BT : L1 + pave tactile");
    CHECK(si.lx<-0.99f,"DualSense BT : stick gauche a gauche");
    /* DualSense Bluetooth complet : rapport 0x31 */
    memset(r,0,sizeof r); r[0]=0x31; r[1]=0x00; r[2]=128; r[3]=0; r[4]=128; r[5]=128; r[9]=0x80|0x06; r[10]=0; r[11]=0;
    memset(&si,0,sizeof si); CHECK(sony_parse(r,78,true,&st),"DualSense BT complet lu");
    sony_to_input(&st,&si);
    CHECK(si.pad[PAD_Y] && si.pad[PAD_LEFT] && si.ly<-0.99f,"DualSense BT complet : Triangle + gauche + stick haut");
    /* DualShock 4 USB : rapport 0x01, format simple */
    memset(r,0,sizeof r); r[0]=0x01; r[1]=128; r[2]=128; r[3]=128; r[4]=128; r[5]=0x10|0x04; r[6]=0x02; r[7]=0; r[8]=0; r[9]=255;
    memset(&si,0,sizeof si); CHECK(sony_parse(r,64,false,&st),"DualShock 4 USB lu");
    sony_to_input(&st,&si);
    CHECK(si.pad[PAD_X] && si.pad[PAD_DOWN] && si.pad[PAD_RB] && si.rt>0.99f,"DualShock 4 : Carre + bas + R1 + R2");
    /* rapports ignores */
    r[0]=0x05; CHECK(!sony_parse(r,64,true,&st),"autre rapport ignore");
    CHECK(!sony_parse(r,4,true,&st),"rapport trop court ignore");
  }
  printf("%d verifications, %d echec(s)\n",checks,fails);
  return fails?1:0;
}
