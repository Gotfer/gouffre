/* GOUFFRE - manettes sous Windows
   Ordre de priorite :
   1) XInput          : manettes Xbox (et manettes emulees par Steam / DS4Windows), avec vibrations
   2) Lecture directe : DualSense (PS5) et DualShock 4 (PS4) en USB ou Bluetooth, via les rapports HID bruts
   3) DirectInput     : toutes les autres manettes (Nintendo, generiques, et PlayStation en secours)
   4) WinMM           : dernier recours si DirectInput est indisponible
   Un fichier de diagnostic (manettes.txt, a cote de la sauvegarde) decrit ce qui est detecte. */
#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <mmsystem.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <dinput.h>
#include <stdio.h>
#include <string.h>
#include "platform.h"
#include "padmap.h"

static HWND g_hwnd;
static char g_diag_path[MAX_PATH*2];
static CRITICAL_SECTION g_cs;
static double tnow(void){ return (double)GetTickCount64()/1000.0; }

/* ---------------- diagnostic ---------------- */
static char sec_act[256]="Manette utilisee : aucune\n";
static char sec_xi[256]="(pas encore analyse)\n", sec_hid[2000]="(pas encore analyse)\n", sec_di[2000]="(pas encore analyse)\n", sec_mm[800]="(non utilise)\n";
static volatile LONG diag_dirty=1;
static void set_section(char *dst,size_t n,const char *src){
  EnterCriticalSection(&g_cs);
  if(strcmp(dst,src)){ snprintf(dst,n,"%s",src); InterlockedExchange(&diag_dirty,1); }
  LeaveCriticalSection(&g_cs);
}
void pads_diag_flush(void){
  if(!InterlockedExchange(&diag_dirty,0) || !g_diag_path[0]) return;
  FILE *f=fopen(g_diag_path,"w"); if(!f) return;
  EnterCriticalSection(&g_cs);
  SYSTEMTIME t; GetLocalTime(&t);
  fprintf(f,"GOUFFRE v1.2 - diagnostic des manettes (mis a jour le %02d/%02d/%04d a %02d:%02d:%02d)\n",t.wDay,t.wMonth,t.wYear,t.wHour,t.wMinute,t.wSecond);
  fprintf(f,"Ce fichier se met a jour tout seul pendant que le jeu tourne.\n\n%s\n",sec_act);
  fprintf(f,"[1. XInput - manettes Xbox]\n%s\n",sec_xi);
  fprintf(f,"[2. Lecture directe - manettes PlayStation]\n%s\n",sec_hid);
  fprintf(f,"[3. DirectInput - autres manettes]\n%s\n",sec_di);
  fprintf(f,"[4. WinMM]\n%s",sec_mm);
  LeaveCriticalSection(&g_cs);
  fclose(f);
}

/* ---------------- 1) XInput ---------------- */
typedef struct { WORD wButtons; BYTE bLeftTrigger, bRightTrigger; SHORT sThumbLX, sThumbLY, sThumbRX, sThumbRY; } XPadG;
typedef struct { DWORD dwPacketNumber; XPadG Gamepad; } XPadState;
typedef struct { WORD wLeftMotorSpeed, wRightMotorSpeed; } XPadVib;
typedef DWORD (WINAPI *XInputGetState_t)(DWORD,XPadState*);
typedef DWORD (WINAPI *XInputSetState_t)(DWORD,XPadVib*);
static XInputGetState_t pXInputGetState; static XInputSetState_t pXInputSetState;
static int xi_idx=-1; static double xi_scan=-10; static double rumble_end=0; static bool rumbling=false;

static void xi_init(void){
  const char *dl[]={"xinput1_4.dll","xinput1_3.dll","xinput9_1_0.dll"};
  char sec[256]; snprintf(sec,sizeof sec,"Bibliotheque XInput introuvable\n");
  for(int i=0;i<3 && !pXInputGetState;i++){
    HMODULE h=LoadLibraryA(dl[i]);
    if(h){
      pXInputGetState=(XInputGetState_t)(void*)GetProcAddress(h,"XInputGetState");
      pXInputSetState=(XInputSetState_t)(void*)GetProcAddress(h,"XInputSetState");
      if(pXInputGetState) snprintf(sec,sizeof sec,"%s chargee, aucune manette Xbox branchee\n",dl[i]);
    }
  }
  set_section(sec_xi,sizeof sec_xi,sec);
}
static float xstick(SHORT v){ float f=v/32767.0f; return f<-1?-1:(f>1?1:f); }
static bool xi_poll(Input *in){
  if(!pXInputGetState) return false;
  XPadState st; memset(&st,0,sizeof st);
  if(xi_idx<0){
    double t=tnow(); if(t-xi_scan<1.5) return false; xi_scan=t;
    for(int i=0;i<4;i++) if(pXInputGetState((DWORD)i,&st)==ERROR_SUCCESS){ xi_idx=i; break; }
    if(xi_idx<0) return false;
    char sec[128]; snprintf(sec,sizeof sec,"Manette Xbox branchee (emplacement %d)\n",xi_idx); set_section(sec_xi,sizeof sec_xi,sec);
    memset(in->pad,0,sizeof in->pad);
  }
  if(pXInputGetState((DWORD)xi_idx,&st)!=ERROR_SUCCESS){
    xi_idx=-1; rumbling=false; set_section(sec_xi,sizeof sec_xi,"Manette Xbox debranchee\n"); return false;
  }
  in->pad_type=PADT_XBOX; snprintf(in->pad_name,sizeof in->pad_name,"Manette Xbox"); snprintf(in->pad_info,sizeof in->pad_info,"XInput");
  WORD b=st.Gamepad.wButtons;
  bool now[PAD_COUNT]={ (b&0x1000)!=0,(b&0x2000)!=0,(b&0x4000)!=0,(b&0x8000)!=0,(b&0x0010)!=0,(b&0x0020)!=0,(b&0x0100)!=0,(b&0x0200)!=0,
                        (b&0x0001)!=0,(b&0x0002)!=0,(b&0x0004)!=0,(b&0x0008)!=0 };
  for(int i=0;i<PAD_COUNT;i++){ if(now[i] && !in->pad[i]) in->pad_pressed[i]=true; in->pad[i]=now[i]; }
  in->lx=xstick(st.Gamepad.sThumbLX); in->ly=-xstick(st.Gamepad.sThumbLY);
  in->rx=xstick(st.Gamepad.sThumbRX); in->ry=-xstick(st.Gamepad.sThumbRY);
  in->lt=st.Gamepad.bLeftTrigger/255.0f; in->rt=st.Gamepad.bRightTrigger/255.0f;
  return true;
}
void pads_rumble_stop(void){
  if(rumbling && xi_idx>=0 && pXInputSetState){ XPadVib v={0,0}; pXInputSetState((DWORD)xi_idx,&v); }
  rumbling=false;
}
void plat_rumble(float strength,float seconds){
  if(xi_idx<0 || !pXInputSetState) return;
  if(strength<0) strength=0; if(strength>1) strength=1;
  XPadVib v; v.wLeftMotorSpeed=(WORD)(strength*65535.0f); v.wRightMotorSpeed=(WORD)(strength*0.75f*65535.0f);
  pXInputSetState((DWORD)xi_idx,&v);
  double e=tnow()+seconds; if(!rumbling || e>rumble_end) rumble_end=e;
  rumbling=true;
}

/* ---------------- 2) lecture directe PlayStation (HID) ---------------- */
typedef struct { bool active, valid, ds5, bt; SonyState st; char name[64]; } HidShared;
static HidShared hid;

static DWORD WINAPI hid_thread(LPVOID p){
  (void)p;
  GUID hg; HidD_GetHidGuid(&hg);
  for(;;){
    HANDLE h=INVALID_HANDLE_VALUE; unsigned short pid=0; int inlen=0; char name[64]=""; bool bt=false;
    char sec[2000]; int sl=0; int ngp=0; sec[0]=0;
    HDEVINFO dv=SetupDiGetClassDevsW(&hg,NULL,NULL,DIGCF_PRESENT|DIGCF_DEVICEINTERFACE);
    if(dv!=INVALID_HANDLE_VALUE){
      SP_DEVICE_INTERFACE_DATA ifd; ifd.cbSize=sizeof ifd;
      for(DWORD i=0; i<512 && SetupDiEnumDeviceInterfaces(dv,NULL,&hg,i,&ifd); i++){
        DWORD need=0; SetupDiGetDeviceInterfaceDetailW(dv,&ifd,NULL,0,&need,NULL);
        if(need<sizeof(DWORD) || need>8192) continue;
        SP_DEVICE_INTERFACE_DETAIL_DATA_W *det=(SP_DEVICE_INTERFACE_DETAIL_DATA_W*)malloc(need);
        if(!det) continue;
        det->cbSize=sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        if(!SetupDiGetDeviceInterfaceDetailW(dv,&ifd,det,need,NULL,NULL)){ free(det); continue; }
        HANDLE q=CreateFileW(det->DevicePath,0,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);
        if(q!=INVALID_HANDLE_VALUE){
          USHORT up=0, us=0; int il=0;
          PHIDP_PREPARSED_DATA pp=NULL;
          if(HidD_GetPreparsedData(q,&pp)){ HIDP_CAPS caps; if(HidP_GetCaps(pp,&caps)==HIDP_STATUS_SUCCESS){ up=caps.UsagePage; us=caps.Usage; il=caps.InputReportByteLength; } HidD_FreePreparsedData(pp); }
          HIDD_ATTRIBUTES at; memset(&at,0,sizeof at); at.Size=sizeof at;
          if(up==0x01 && (us==0x04 || us==0x05) && HidD_GetAttributes(q,&at)){
            ngp++;
            WCHAR wn[128]; wn[0]=0; HidD_GetProductString(q,wn,sizeof wn);
            char nm[128]; nm[0]=0; WideCharToMultiByte(CP_UTF8,0,wn,-1,nm,(int)sizeof nm,NULL,NULL);
            if(sl<(int)sizeof sec-200) sl+=snprintf(sec+sl,sizeof sec-sl,"- \"%s\"  VID %04X  PID %04X  rapports de %d octets\n",nm,at.VendorID,at.ProductID,il);
            if(at.VendorID==VID_SONY && h==INVALID_HANDLE_VALUE){
              HANDLE r=CreateFileW(det->DevicePath,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);
              if(r==INVALID_HANDLE_VALUE) r=CreateFileW(det->DevicePath,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);
              if(r!=INVALID_HANDLE_VALUE){
                h=r; pid=at.ProductID; inlen=il; bt=(il!=64);
                joy_friendly_name(at.VendorID,at.ProductID,nm,name,(int)sizeof name);
                if(sl<(int)sizeof sec-120) sl+=snprintf(sec+sl,sizeof sec-sl,"  -> ouverte en lecture directe (%s)\n",bt?"Bluetooth":"USB");
              } else if(sl<(int)sizeof sec-120) sl+=snprintf(sec+sl,sizeof sec-sl,"  -> impossible de l'ouvrir (erreur %lu) : un autre logiciel l'utilise peut-etre\n",GetLastError());
            }
          }
          CloseHandle(q);
        }
        free(det);
      }
      SetupDiDestroyDeviceInfoList(dv);
    }
    if(ngp==0) snprintf(sec,sizeof sec,"Aucune manette HID trouvee\n");
    else if(h==INVALID_HANDLE_VALUE && sl<(int)sizeof sec-80) sl+=snprintf(sec+sl,sizeof sec-sl,"(aucune manette PlayStation parmi elles)\n");
    set_section(sec_hid,sizeof sec_hid,sec);
    if(h==INVALID_HANDLE_VALUE){ Sleep(1500); continue; }

    EnterCriticalSection(&g_cs);
    hid.active=true; hid.valid=false; hid.ds5=sony_is_ds5(pid); hid.bt=bt; snprintf(hid.name,sizeof hid.name,"%s",name);
    LeaveCriticalSection(&g_cs);
    unsigned char buf[1024]; int rl = (inlen>0 && inlen<=(int)sizeof buf) ? inlen : 78;
    bool ds5=sony_is_ds5(pid); unsigned long nrep=0;
    for(;;){
      DWORD n=0;
      if(!ReadFile(h,buf,(DWORD)rl,&n,NULL)) break;
      SonyState s;
      if(sony_parse(buf,(int)n,ds5,&s)){
        EnterCriticalSection(&g_cs); hid.st=s; hid.valid=true; LeaveCriticalSection(&g_cs);
        if(++nrep==1){ if(sl<(int)sizeof sec-80) snprintf(sec+sl,sizeof sec-sl,"  -> premiers rapports recus : la manette fonctionne\n"); set_section(sec_hid,sizeof sec_hid,sec); }
      }
    }
    CloseHandle(h);
    EnterCriticalSection(&g_cs); hid.active=false; hid.valid=false; LeaveCriticalSection(&g_cs);
    set_section(sec_hid,sizeof sec_hid,"La manette PlayStation a ete debranchee\n");
    Sleep(500);
  }
  return 0;
}
static bool hid_in_use(void){ EnterCriticalSection(&g_cs); bool a=hid.active&&hid.valid; LeaveCriticalSection(&g_cs); return a; }
static bool hid_poll(Input *in){
  EnterCriticalSection(&g_cs);
  bool ok=hid.active && hid.valid; SonyState s=hid.st; bool bt=hid.bt; char nm[64]; memcpy(nm,hid.name,sizeof nm);
  LeaveCriticalSection(&g_cs);
  if(!ok) return false;
  in->pad_type=PADT_PS; snprintf(in->pad_name,sizeof in->pad_name,"%s",nm);
  snprintf(in->pad_info,sizeof in->pad_info,"lecture directe %s",bt?"Bluetooth":"USB");
  sony_to_input(&s,in);
  return true;
}

/* ---------------- 3) DirectInput ---------------- */
typedef struct { bool active; JoyRaw jr; int layout; char name[64]; } DiShared;
static DiShared dis; static volatile LONG di_failed=0;
typedef struct { bool found; GUID guid; unsigned short vid,pid; char name[128]; char list[1600]; int ll; } DiEnum;

static BOOL CALLBACK di_cb(LPCDIDEVICEINSTANCEW inst,LPVOID ctx){
  DiEnum *c=(DiEnum*)ctx;
  DWORD d1=inst->guidProduct.Data1; unsigned short vid=LOWORD(d1), pid=HIWORD(d1);
  char nm[128]; nm[0]=0; WideCharToMultiByte(CP_UTF8,0,inst->tszProductName,-1,nm,(int)sizeof nm,NULL,NULL);
  const char *note="";
  bool skip=false;
  if(vid==VID_MICROSOFT){ skip=true; note=" (Xbox : geree par XInput)"; }
  else if(vid==VID_SONY && hid_in_use()){ skip=true; note=" (deja lue en direct)"; }
  if(c->ll<(int)sizeof c->list-200) c->ll+=snprintf(c->list+c->ll,sizeof c->list-c->ll,"- \"%s\"  VID %04X  PID %04X%s\n",nm,vid,pid,note);
  if(!skip && !c->found){ c->found=true; c->guid=inst->guidInstance; c->vid=vid; c->pid=pid; snprintf(c->name,sizeof c->name,"%s",nm); }
  return DIENUM_CONTINUE;
}
static bool di_has(LPDIRECTINPUTDEVICE8W dev,DWORD ofs){
  DIDEVICEOBJECTINSTANCEW oi; memset(&oi,0,sizeof oi); oi.dwSize=sizeof oi;
  return SUCCEEDED(dev->lpVtbl->GetObjectInfo(dev,&oi,ofs,DIPH_BYOFFSET));
}
static DWORD WINAPI di_thread(LPVOID p){
  (void)p;
  LPDIRECTINPUT8W di=NULL;
  if(FAILED(DirectInput8Create(GetModuleHandleW(NULL),DIRECTINPUT_VERSION,&IID_IDirectInput8W,(void**)&di,NULL)) || !di){
    set_section(sec_di,sizeof sec_di,"DirectInput indisponible sur ce PC\n"); InterlockedExchange(&di_failed,1); return 0;
  }
  for(;;){
    DiEnum c; memset(&c,0,sizeof c);
    di->lpVtbl->EnumDevices(di,DI8DEVCLASS_GAMECTRL,di_cb,&c,DIEDFL_ATTACHEDONLY);
    if(c.ll==0) snprintf(c.list,sizeof c.list,"Aucune manette vue par DirectInput\n");
    set_section(sec_di,sizeof sec_di,c.list);
    if(!c.found){ Sleep(2000); continue; }
    LPDIRECTINPUTDEVICE8W dev=NULL;
    if(FAILED(di->lpVtbl->CreateDevice(di,&c.guid,&dev,NULL)) || !dev){ Sleep(2000); continue; }
    dev->lpVtbl->SetDataFormat(dev,&c_dfDIJoystick2);
    dev->lpVtbl->SetCooperativeLevel(dev,g_hwnd,DISCL_BACKGROUND|DISCL_NONEXCLUSIVE);
    DIPROPRANGE pr; memset(&pr,0,sizeof pr);
    pr.diph.dwSize=sizeof pr; pr.diph.dwHeaderSize=sizeof pr.diph; pr.diph.dwHow=DIPH_DEVICE; pr.diph.dwObj=0; pr.lMin=0; pr.lMax=65535;
    dev->lpVtbl->SetProperty(dev,DIPROP_RANGE,&pr.diph);
    JoyRaw base; memset(&base,0,sizeof base);
    for(int i=0;i<JA_COUNT;i++){ base.mn[i]=0; base.mx[i]=65535; }
    base.has[JA_X]=di_has(dev,DIJOFS_X); base.has[JA_Y]=di_has(dev,DIJOFS_Y); base.has[JA_Z]=di_has(dev,DIJOFS_Z);
    base.has[JA_R]=di_has(dev,DIJOFS_RZ); base.has[JA_U]=di_has(dev,DIJOFS_RX); base.has[JA_V]=di_has(dev,DIJOFS_RY);
    base.has_pov=di_has(dev,DIJOFS_POV(0));
    int layout=joy_layout_for(c.vid); char name[64]; joy_friendly_name(c.vid,c.pid,c.name,name,(int)sizeof name);
    dev->lpVtbl->Acquire(dev);
    int fails=0;
    for(;;){
      dev->lpVtbl->Poll(dev);
      DIJOYSTATE2 st; memset(&st,0,sizeof st);
      HRESULT hr=dev->lpVtbl->GetDeviceState(dev,sizeof st,&st);
      if(hr==DIERR_INPUTLOST || hr==DIERR_NOTACQUIRED){ dev->lpVtbl->Acquire(dev); if(++fails>150) break; Sleep(15); continue; }
      if(FAILED(hr)) break;
      fails=0;
      JoyRaw jr=base;
      jr.ax[JA_X]=(unsigned)st.lX; jr.ax[JA_Y]=(unsigned)st.lY; jr.ax[JA_Z]=(unsigned)st.lZ;
      jr.ax[JA_R]=(unsigned)st.lRz; jr.ax[JA_U]=(unsigned)st.lRx; jr.ax[JA_V]=(unsigned)st.lRy;
      jr.buttons=0; for(int i=0;i<32;i++) if(st.rgbButtons[i]&0x80) jr.buttons|=1u<<i;
      jr.pov = (LOWORD(st.rgdwPOV[0])==0xFFFF) ? 0xFFFFu : (unsigned)st.rgdwPOV[0];
      EnterCriticalSection(&g_cs); dis.active=true; dis.jr=jr; dis.layout=layout; snprintf(dis.name,sizeof dis.name,"%s",name); LeaveCriticalSection(&g_cs);
      if(c.vid==VID_SONY && hid_in_use()) break;     /* la lecture directe a pris le relais */
      Sleep(4);
    }
    EnterCriticalSection(&g_cs); dis.active=false; LeaveCriticalSection(&g_cs);
    dev->lpVtbl->Unacquire(dev); dev->lpVtbl->Release(dev);
    Sleep(500);
  }
  return 0;
}
static bool di_poll(Input *in){
  EnterCriticalSection(&g_cs);
  bool ok=dis.active; JoyRaw jr=dis.jr; int layout=dis.layout; char nm[64]; memcpy(nm,dis.name,sizeof nm);
  LeaveCriticalSection(&g_cs);
  if(!ok) return false;
  in->pad_type=joy_pad_type(layout); snprintf(in->pad_name,sizeof in->pad_name,"%s",nm); snprintf(in->pad_info,sizeof in->pad_info,"DirectInput");
  joy_to_input(&jr,layout,in);
  return true;
}

/* ---------------- 4) WinMM (si DirectInput est indisponible) ---------------- */
static int mm_id=-1, mm_layout=JL_GENERIC, mm_scan_id=0, mm_tick=0; static JoyRaw mm_jr; static char mm_name[64];
static bool mm_read(UINT id,JoyRaw *jr){
  JOYINFOEX ji; memset(&ji,0,sizeof ji); ji.dwSize=sizeof ji; ji.dwFlags=JOY_RETURNALL;
  if(joyGetPosEx(id,&ji)!=JOYERR_NOERROR) return false;
  jr->ax[JA_X]=ji.dwXpos; jr->ax[JA_Y]=ji.dwYpos; jr->ax[JA_Z]=ji.dwZpos; jr->ax[JA_R]=ji.dwRpos; jr->ax[JA_U]=ji.dwUpos; jr->ax[JA_V]=ji.dwVpos;
  jr->buttons=ji.dwButtons; jr->pov=ji.dwPOV; return true;
}
static bool mm_open(UINT id){
  JOYCAPSW jc; memset(&jc,0,sizeof jc);
  if(joyGetDevCapsW(id,&jc,sizeof jc)!=JOYERR_NOERROR) return false;
  if(jc.wMid==VID_MICROSOFT && pXInputGetState) return false;
  if(jc.wNumButtons<2 && jc.wNumAxes<2) return false;
  JoyRaw jr; memset(&jr,0,sizeof jr);
  jr.mn[JA_X]=jc.wXmin; jr.mx[JA_X]=jc.wXmax; jr.has[JA_X]=true;
  jr.mn[JA_Y]=jc.wYmin; jr.mx[JA_Y]=jc.wYmax; jr.has[JA_Y]=true;
  jr.mn[JA_Z]=jc.wZmin; jr.mx[JA_Z]=jc.wZmax; jr.has[JA_Z]=(jc.wCaps&JOYCAPS_HASZ)!=0;
  jr.mn[JA_R]=jc.wRmin; jr.mx[JA_R]=jc.wRmax; jr.has[JA_R]=(jc.wCaps&JOYCAPS_HASR)!=0;
  jr.mn[JA_U]=jc.wUmin; jr.mx[JA_U]=jc.wUmax; jr.has[JA_U]=(jc.wCaps&JOYCAPS_HASU)!=0;
  jr.mn[JA_V]=jc.wVmin; jr.mx[JA_V]=jc.wVmax; jr.has[JA_V]=(jc.wCaps&JOYCAPS_HASV)!=0;
  jr.has_pov=(jc.wCaps&JOYCAPS_HASPOV)!=0;
  if(!mm_read(id,&jr)) return false;
  mm_jr=jr; mm_id=(int)id; mm_layout=joy_layout_for(jc.wMid);
  char raw[64]; raw[0]=0; WideCharToMultiByte(CP_UTF8,0,jc.szPname,-1,raw,(int)sizeof raw,NULL,NULL);
  joy_friendly_name(jc.wMid,jc.wPid,raw,mm_name,(int)sizeof mm_name);
  char sec[160]; snprintf(sec,sizeof sec,"Manette ouverte : \"%s\" (numero %u)\n",raw,id); set_section(sec_mm,sizeof sec_mm,sec);
  return true;
}
static bool mm_poll(Input *in){
  if(mm_id<0){
    if(++mm_tick<3) return false;
    mm_tick=0;
    UINT n=joyGetNumDevs(); if(n==0) return false; if(n>16) n=16;
    if(!mm_open((UINT)(mm_scan_id++ % (int)n))) return false;
  }
  if(!mm_read((UINT)mm_id,&mm_jr)){ mm_id=-1; return false; }
  in->pad_type=joy_pad_type(mm_layout); snprintf(in->pad_name,sizeof in->pad_name,"%s",mm_name); snprintf(in->pad_info,sizeof in->pad_info,"WinMM");
  joy_to_input(&mm_jr,mm_layout,in);
  return true;
}

/* ---------------- interface ---------------- */
void pads_init(HWND hwnd,const char *diag_path){
  g_hwnd=hwnd;
  snprintf(g_diag_path,sizeof g_diag_path,"%s",diag_path?diag_path:"");
  InitializeCriticalSection(&g_cs);
  xi_init();
  HANDLE t1=CreateThread(NULL,0,hid_thread,NULL,0,NULL); if(t1) CloseHandle(t1);
  HANDLE t2=CreateThread(NULL,0,di_thread,NULL,0,NULL); if(t2) CloseHandle(t2);
}
void pads_poll(Input *in){
  bool ok=xi_poll(in);
  if(!ok) ok=hid_poll(in);
  if(!ok) ok=di_poll(in);
  if(!ok && di_failed) ok=mm_poll(in);
  if(!ok){
    if(in->pad_on) memset(in->pad,0,sizeof in->pad);
    in->lx=in->ly=in->rx=in->ry=in->lt=in->rt=0; in->pad_type=PADT_NONE; in->pad_name[0]=0; in->pad_info[0]=0;
  }
  in->pad_on=ok;
  if(rumbling && tnow()>=rumble_end) pads_rumble_stop();
  char act[256];
  if(ok) snprintf(act,sizeof act,"Manette utilisee : %s (%s)\n",in->pad_name,in->pad_info);
  else snprintf(act,sizeof act,"Manette utilisee : aucune\n");
  set_section(sec_act,sizeof sec_act,act);
}
