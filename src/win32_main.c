/* GOUFFRE - couche Windows : fenetre, rendu (OpenGL ou GDI), clavier, souris, manette, son */
#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <mmsystem.h>
#include <GL/gl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "platform.h"
#include "gpu.h"

#ifndef GL_BGRA_EXT
#define GL_BGRA_EXT 0x80E1
#endif
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

static HWND g_hwnd; static HDC g_dc; static BITMAPINFO g_bmi;
static bool g_running=true, g_fs=false, g_active=true;
static WINDOWPLACEMENT g_wp={sizeof(WINDOWPLACEMENT)};
static Input g_in;
static int g_dx=0,g_dy=0,g_dw=SCREEN_W,g_dh=SCREEN_H;
static char g_save[MAX_PATH*2];
static char g_diag[MAX_PATH*2];
static char g_gfxlog[MAX_PATH*2];
static LARGE_INTEGER g_freq;

const char *plat_save_path(void){ return g_save; }

static double now_s(void){ LARGE_INTEGER c; QueryPerformanceCounter(&c); return (double)c.QuadPart/(double)g_freq.QuadPart; }

static void init_save_path(void){
  char base[MAX_PATH*2]; DWORD n=GetEnvironmentVariableA("APPDATA",base,sizeof base);
  if(n==0 || n>=sizeof base){
    GetModuleFileNameA(NULL,base,sizeof base); char *s=strrchr(base,'\\'); if(s) *s=0;
    snprintf(g_save,sizeof g_save,"%s\\gouffre_sauvegarde.txt",base);
    snprintf(g_diag,sizeof g_diag,"%s\\gouffre_manettes.txt",base);
    snprintf(g_gfxlog,sizeof g_gfxlog,"%s\\gouffre_graphismes.txt",base); return;
  }
  char dir[MAX_PATH*2]; snprintf(dir,sizeof dir,"%s\\Gouffre",base); CreateDirectoryA(dir,NULL);
  snprintf(g_save,sizeof g_save,"%s\\sauvegarde.txt",dir);
  snprintf(g_diag,sizeof g_diag,"%s\\manettes.txt",dir);
  snprintf(g_gfxlog,sizeof g_gfxlog,"%s\\graphismes.txt",dir);
}

/* ---------------- rendu ---------------- */
typedef BOOL (WINAPI *PFNSWAPINTERVAL)(int);
static HGLRC g_rc; static GLuint g_tex; static bool g_gl=false, g_vsync=false;
static HRESULT (WINAPI *pDwmFlush)(void);

static void compute_rect(int cw,int ch){
  int dw,dh;
  int s=cw/SCREEN_W; if(ch/SCREEN_H<s) s=ch/SCREEN_H;
  if(game_pixel_perfect() && s>=1){ dw=SCREEN_W*s; dh=SCREEN_H*s; }
  else if((long long)cw*SCREEN_H > (long long)ch*SCREEN_W){ dh=ch; dw=ch*SCREEN_W/SCREEN_H; }
  else { dw=cw; dh=cw*SCREEN_H/SCREEN_W; }
  if(dw<1) dw=1; if(dh<1) dh=1;
  g_dx=(cw-dw)/2; g_dy=(ch-dh)/2; g_dw=dw; g_dh=dh;
}

static bool gl_init(void){
  PIXELFORMATDESCRIPTOR pfd; memset(&pfd,0,sizeof pfd);
  pfd.nSize=sizeof pfd; pfd.nVersion=1; pfd.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER;
  pfd.iPixelType=PFD_TYPE_RGBA; pfd.cColorBits=32; pfd.cAlphaBits=8; pfd.iLayerType=PFD_MAIN_PLANE;
  int pf=ChoosePixelFormat(g_dc,&pfd); if(!pf) return false;
  if(!SetPixelFormat(g_dc,pf,&pfd)) return false;
  g_rc=wglCreateContext(g_dc); if(!g_rc) return false;
  if(!wglMakeCurrent(g_dc,g_rc)){ wglDeleteContext(g_rc); g_rc=NULL; return false; }
  const char *ren=(const char*)glGetString(GL_RENDERER);
  if(!ren || strstr(ren,"GDI Generic")){ wglMakeCurrent(NULL,NULL); wglDeleteContext(g_rc); g_rc=NULL; return false; }
  glGenTextures(1,&g_tex); glBindTexture(GL_TEXTURE_2D,g_tex);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,1024,512,0,GL_BGRA_EXT,GL_UNSIGNED_BYTE,NULL);
  if(glGetError()!=GL_NO_ERROR){ wglMakeCurrent(NULL,NULL); wglDeleteContext(g_rc); g_rc=NULL; return false; }
  glDisable(GL_DEPTH_TEST); glDisable(GL_BLEND); glEnable(GL_TEXTURE_2D);
  PFNSWAPINTERVAL si=(PFNSWAPINTERVAL)(void*)wglGetProcAddress("wglSwapIntervalEXT");
  if(si && si(1)) g_vsync=true;
  return true;
}
/* ---- rendu GPU (OpenGL 2.1 + shaders) ---- */
static HMODULE g_ogl32;
static void *win_getproc(const char *name){
  void *p=(void*)wglGetProcAddress(name);
  if(p==NULL || p==(void*)1 || p==(void*)2 || p==(void*)3 || p==(void*)-1){ p = g_ogl32 ? (void*)GetProcAddress(g_ogl32,name) : NULL; }
  return p;
}
static void gfx_log_write(const char *txt,bool append){
  FILE *f=fopen(g_gfxlog,append?"a":"w"); if(!f) return; fputs(txt,f); fclose(f);
}
/* tache de fond (generation des icones) */
typedef struct { void (*fn)(void*); void *arg; } ThrArg;
static DWORD WINAPI thr_entry(LPVOID p){ ThrArg a=*(ThrArg*)p; free(p); a.fn(a.arg); return 0; }
bool plat_thread_start(void (*fn)(void*),void *arg){
  ThrArg *a=(ThrArg*)malloc(sizeof *a); if(!a) return false; a->fn=fn; a->arg=arg;
  HANDLE h=CreateThread(NULL,0,thr_entry,a,0,NULL); if(!h){ free(a); return false; }
  SetThreadPriority(h,THREAD_PRIORITY_BELOW_NORMAL); CloseHandle(h); return true;
}
static void gpu_start(void){
  static char log[8192]; log[0]=0;
  bool forced_cpu = strstr(GetCommandLineA(),"--logiciel")!=NULL || strstr(GetCommandLineA(),"--cpu")!=NULL;
  if(!g_gl){ gfx_log_write("OpenGL indisponible : rendu logiciel (GDI).\n",false); return; }
  if(forced_cpu){ gfx_log_write("Option --logiciel : rendu logiciel.\n",false); return; }
  g_ogl32=GetModuleHandleA("opengl32.dll");
  g_gpu_ok = gpu_init(win_getproc,log,sizeof log);
  char head[256]; snprintf(head,sizeof head,"GOUFFRE 10.2 - rendu %s\n",g_gpu_ok?"GPU (eclairage HD, HDR, bloom, ombres)":"logiciel");
  gfx_log_write(head,false); gfx_log_write(log,true);
  if(!g_gpu_ok){ gpu_reset_state(); glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D,g_tex); }
}
static void present(void){
  RECT rc; GetClientRect(g_hwnd,&rc); int cw=rc.right, ch=rc.bottom; if(cw<=0||ch<=0) return;
  compute_rect(cw,ch);
  extern bool game_uses_gpu(void);
  static bool last_gpu=false;
  if(g_gl && g_gpu_ok && game_uses_gpu()){
    last_gpu=true;
    gpu_render(0,cw,ch,g_dx,ch-g_dy-g_dh,g_dw,g_dh);
    if(gpu_failed()){
      /* le pilote a refuse quelque chose : on bascule definitivement en rendu logiciel */
      g_gpu_ok=false; gpu_reset_state();
      gfx_log_write("Erreur pendant le rendu GPU : bascule en rendu logiciel.\n",true);
      glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D,g_tex);
    } else { SwapBuffers(g_dc); return; }
  }
  if(g_gl && last_gpu){ gpu_reset_state(); glEnable(GL_TEXTURE_2D); last_gpu=false; }
  if(g_gl){
    glViewport(0,0,cw,ch); glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT);
    glViewport(g_dx,ch-g_dy-g_dh,g_dw,g_dh);
    glBindTexture(GL_TEXTURE_2D,g_tex);
    glTexSubImage2D(GL_TEXTURE_2D,0,0,0,SCREEN_W,SCREEN_H,GL_BGRA_EXT,GL_UNSIGNED_BYTE,game_framebuffer());
    float u=SCREEN_W/1024.0f, v=SCREEN_H/512.0f;
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glColor4f(1,1,1,1);
    glBegin(GL_QUADS);
    glTexCoord2f(0,0); glVertex2f(-1,1); glTexCoord2f(u,0); glVertex2f(1,1);
    glTexCoord2f(u,v); glVertex2f(1,-1); glTexCoord2f(0,v); glVertex2f(-1,-1);
    glEnd();
    SwapBuffers(g_dc);
  } else {
    if(g_dy>0){ PatBlt(g_dc,0,0,cw,g_dy,BLACKNESS); PatBlt(g_dc,0,g_dy+g_dh,cw,ch-g_dy-g_dh,BLACKNESS); }
    if(g_dx>0){ PatBlt(g_dc,0,g_dy,g_dx,g_dh,BLACKNESS); PatBlt(g_dc,g_dx+g_dw,g_dy,cw-g_dx-g_dw,g_dh,BLACKNESS); }
    SetStretchBltMode(g_dc,COLORONCOLOR);
    StretchDIBits(g_dc,g_dx,g_dy,g_dw,g_dh,0,0,SCREEN_W,SCREEN_H,game_framebuffer(),&g_bmi,DIB_RGB_COLORS,SRCCOPY);
    GdiFlush();
  }
}

/* ---------------- plein ecran ---------------- */
static void set_fullscreen(bool fs){
  DWORD style=(DWORD)GetWindowLongW(g_hwnd,GWL_STYLE);
  if(fs && !g_fs){
    MONITORINFO mi={sizeof(MONITORINFO)};
    if(GetWindowPlacement(g_hwnd,&g_wp) && GetMonitorInfoW(MonitorFromWindow(g_hwnd,MONITOR_DEFAULTTOPRIMARY),&mi)){
      SetWindowLongW(g_hwnd,GWL_STYLE,(LONG)(style & ~WS_OVERLAPPEDWINDOW));
      SetWindowPos(g_hwnd,HWND_TOP,mi.rcMonitor.left,mi.rcMonitor.top,mi.rcMonitor.right-mi.rcMonitor.left,mi.rcMonitor.bottom-mi.rcMonitor.top,SWP_NOOWNERZORDER|SWP_FRAMECHANGED);
      g_fs=true;
    }
  } else if(!fs && g_fs){
    SetWindowLongW(g_hwnd,GWL_STYLE,(LONG)(style|WS_OVERLAPPEDWINDOW));
    SetWindowPlacement(g_hwnd,&g_wp);
    SetWindowPos(g_hwnd,NULL,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOOWNERZORDER|SWP_FRAMECHANGED);
    g_fs=false;
  }
  game_fullscreen_changed(g_fs);
}

/* ---------------- clavier / souris ---------------- */
static void set_key(int k,bool down){
  if(k<0||k>=KEY_COUNT) return;
  if(down && !g_in.key[k]) g_in.key_pressed[k]=true;
  g_in.key[k]=down;
}
static int map_key(WPARAM vk,LPARAM lp){
  int sc=(int)((lp>>16)&0xFF); bool ext=((lp>>24)&1)!=0;
  switch(vk){
    case VK_UP: return KEY_UP; case VK_DOWN: return KEY_DOWN; case VK_LEFT: return KEY_LEFT; case VK_RIGHT: return KEY_RIGHT;
    case VK_RETURN: return KEY_ENTER; case VK_ESCAPE: return KEY_ESCAPE; case VK_SPACE: return KEY_SPACE; case VK_TAB: return KEY_TAB;
    case VK_F11: return KEY_F11; case VK_F1: return KEY_F1; case VK_BACK: return KEY_BACKSPACE;
    case '1': case VK_NUMPAD1: return KEY_1; case '2': case VK_NUMPAD2: return KEY_2;
    case '3': case VK_NUMPAD3: return KEY_3; case '4': case VK_NUMPAD4: return KEY_4;
    default: break;
  }
  if(!ext){
    /* deplacement par position physique : ZQSD (AZERTY) = WASD (QWERTY) */
    switch(sc){ case 0x11: return KEY_UP; case 0x1E: return KEY_LEFT; case 0x1F: return KEY_DOWN; case 0x20: return KEY_RIGHT; default: break; }
  }
  switch(vk){ case 'R': return KEY_R; case 'E': return KEY_E; case 'M': return KEY_M; case 'P': return KEY_P; case 'T': return KEY_T; default: break; }
  return -1;
}
static void clear_input(void){
  memset(g_in.key,0,sizeof g_in.key); memset(g_in.mb,0,sizeof g_in.mb);
}
static void update_mouse(void){
  POINT p; if(!GetCursorPos(&p)) return; ScreenToClient(g_hwnd,&p);
  RECT rc; GetClientRect(g_hwnd,&rc);
  g_in.mouse_inside = p.x>=0 && p.y>=0 && p.x<rc.right && p.y<rc.bottom && g_active;
  float mx=(float)(p.x-g_dx)*SCREEN_W/(float)g_dw, my=(float)(p.y-g_dy)*SCREEN_H/(float)g_dh;
  if(mx<0) mx=0; if(my<0) my=0; if(mx>SCREEN_W-1) mx=SCREEN_W-1; if(my>SCREEN_H-1) my=SCREEN_H-1;
  if(fabsf(mx-g_in.mx)>0.01f || fabsf(my-g_in.my)>0.01f) g_in.mouse_moved=true;
  g_in.mx=mx; g_in.my=my;
}

/* ---------------- manettes (voir win32_pads.c) ---------------- */
void pads_init(HWND hwnd,const char *diag_path);
void pads_poll(Input *in);
void pads_rumble_stop(void);
void pads_diag_flush(void);
/* ---------------- son (waveOut, thread dedie) ---------------- */
#define AUD_BUFS 6
#define AUD_FRAMES 512
static HWAVEOUT g_wo; static WAVEHDR g_hdr[AUD_BUFS]; static int16_t g_abuf[AUD_BUFS][AUD_FRAMES*2];
static HANDLE g_aev, g_athr; static volatile LONG g_aud_run=0;
static DWORD WINAPI audio_thread(LPVOID p){
  (void)p;
  while(g_aud_run){
    WaitForSingleObject(g_aev,20);
    for(int i=0;i<AUD_BUFS;i++){
      if(g_hdr[i].dwFlags & WHDR_DONE){
        game_audio_mix(g_abuf[i],AUD_FRAMES);
        g_hdr[i].dwFlags &= ~WHDR_DONE;
        waveOutWrite(g_wo,&g_hdr[i],sizeof(WAVEHDR));
      }
    }
  }
  return 0;
}
static void audio_start(void){
  WAVEFORMATEX wf; memset(&wf,0,sizeof wf);
  wf.wFormatTag=WAVE_FORMAT_PCM; wf.nChannels=2; wf.nSamplesPerSec=AUDIO_RATE; wf.wBitsPerSample=16; wf.nBlockAlign=4; wf.nAvgBytesPerSec=AUDIO_RATE*4;
  g_aev=CreateEventW(NULL,FALSE,FALSE,NULL);
  if(waveOutOpen(&g_wo,WAVE_MAPPER,&wf,(DWORD_PTR)g_aev,0,CALLBACK_EVENT)!=MMSYSERR_NOERROR){ g_wo=NULL; return; }
  for(int i=0;i<AUD_BUFS;i++){
    memset(&g_hdr[i],0,sizeof(WAVEHDR)); g_hdr[i].lpData=(LPSTR)g_abuf[i]; g_hdr[i].dwBufferLength=AUD_FRAMES*4;
    waveOutPrepareHeader(g_wo,&g_hdr[i],sizeof(WAVEHDR));
    game_audio_mix(g_abuf[i],AUD_FRAMES);
    waveOutWrite(g_wo,&g_hdr[i],sizeof(WAVEHDR));
  }
  g_aud_run=1;
  g_athr=CreateThread(NULL,0,audio_thread,NULL,0,NULL);
  if(g_athr) SetThreadPriority(g_athr,THREAD_PRIORITY_TIME_CRITICAL);
}
static void audio_stop(void){
  if(!g_wo) return;
  g_aud_run=0; if(g_athr){ SetEvent(g_aev); WaitForSingleObject(g_athr,500); CloseHandle(g_athr); }
  waveOutReset(g_wo);
  for(int i=0;i<AUD_BUFS;i++) waveOutUnprepareHeader(g_wo,&g_hdr[i],sizeof(WAVEHDR));
  waveOutClose(g_wo); g_wo=NULL;
}

/* ---------------- fenetre ---------------- */
static LRESULT CALLBACK wnd_proc(HWND h,UINT m,WPARAM w,LPARAM l){
  switch(m){
    case WM_CLOSE: g_running=false; return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: { PAINTSTRUCT ps; BeginPaint(h,&ps); EndPaint(h,&ps); return 0; }
    case WM_SETCURSOR: if(LOWORD(l)==HTCLIENT){ SetCursor(NULL); return TRUE; } break;
    case WM_ACTIVATEAPP:
      g_active = w!=0;
      if(!w){ clear_input(); pads_rumble_stop(); game_focus_lost(); }
      return 0;
    case WM_KILLFOCUS: clear_input(); return 0;
    case WM_SYSCOMMAND: if((w&0xFFF0)==SC_KEYMENU) return 0; break;
    case WM_SYSKEYDOWN:
      if(w==VK_RETURN && (l&(1<<29))){ if(!(l&(1<<30))) set_fullscreen(!g_fs); return 0; }
      if(w==VK_F4) break;
      /* fallthrough */
    case WM_KEYDOWN:
      if(w==VK_F11 && !(l&(1<<30))){ set_fullscreen(!g_fs); return 0; }
      if(!(l&(1<<30))) set_key(map_key(w,l),true);
      return 0;
    case WM_SYSKEYUP: case WM_KEYUP: set_key(map_key(w,l),false); return 0;
    case WM_LBUTTONDOWN: g_in.mb[MOUSE_LEFT]=true; g_in.mb_pressed[MOUSE_LEFT]=true; SetCapture(h); return 0;
    case WM_LBUTTONUP: g_in.mb[MOUSE_LEFT]=false; ReleaseCapture(); return 0;
    case WM_RBUTTONDOWN: g_in.mb[MOUSE_RIGHT]=true; g_in.mb_pressed[MOUSE_RIGHT]=true; return 0;
    case WM_RBUTTONUP: g_in.mb[MOUSE_RIGHT]=false; return 0;
    case WM_MBUTTONDOWN: g_in.mb[MOUSE_MIDDLE]=true; g_in.mb_pressed[MOUSE_MIDDLE]=true; return 0;
    case WM_MBUTTONUP: g_in.mb[MOUSE_MIDDLE]=false; return 0;
    case WM_MOUSEWHEEL: g_in.wheel += GET_WHEEL_DELTA_WPARAM(w)>0?1:-1; return 0;
    case WM_GETMINMAXINFO: { MINMAXINFO *mm=(MINMAXINFO*)l; mm->ptMinTrackSize.x=SCREEN_W/2; mm->ptMinTrackSize.y=SCREEN_H/2; return 0; }
    default: break;
  }
  return DefWindowProcW(h,m,w,l);
}

static void dpi_aware(void){
  HMODULE u=GetModuleHandleA("user32.dll");
  typedef BOOL (WINAPI *Ctx_t)(HANDLE);
  Ctx_t ctx = u ? (Ctx_t)(void*)GetProcAddress(u,"SetProcessDpiAwarenessContext") : NULL;
  if(ctx && ctx((HANDLE)(intptr_t)-4)) return;
  typedef BOOL (WINAPI *Aware_t)(void);
  Aware_t aw = u ? (Aware_t)(void*)GetProcAddress(u,"SetProcessDPIAware") : NULL;
  if(aw) aw();
}

int WINAPI WinMain(HINSTANCE hi,HINSTANCE hp,LPSTR cmd,int show){
  (void)hp; (void)cmd; (void)show;
  QueryPerformanceFrequency(&g_freq);
  dpi_aware();
  timeBeginPeriod(1);
  init_save_path();

  WNDCLASSEXW wc; memset(&wc,0,sizeof wc);
  wc.cbSize=sizeof wc; wc.style=CS_OWNDC|CS_HREDRAW|CS_VREDRAW; wc.lpfnWndProc=wnd_proc; wc.hInstance=hi;
  wc.hIcon=LoadIconW(hi,MAKEINTRESOURCEW(1)); wc.hIconSm=wc.hIcon; wc.hCursor=LoadCursor(NULL,IDC_ARROW);
  wc.lpszClassName=L"GouffreFenetre";
  if(!RegisterClassExW(&wc)){ MessageBoxW(NULL,L"Impossible de créer la fenêtre.",L"Gouffre",MB_ICONERROR); return 1; }

  RECT wa; SystemParametersInfoW(SPI_GETWORKAREA,0,&wa,0);
  int aw=wa.right-wa.left, ah=wa.bottom-wa.top, sc=1;
  while(SCREEN_W*(sc+1)<=aw-60 && SCREEN_H*(sc+1)<=ah-90) sc++;
  RECT r={0,0,SCREEN_W*sc,SCREEN_H*sc}; AdjustWindowRect(&r,WS_OVERLAPPEDWINDOW,FALSE);
  int ww=r.right-r.left, wh=r.bottom-r.top;
  g_hwnd=CreateWindowExW(0,wc.lpszClassName,L"GOUFFRE",WS_OVERLAPPEDWINDOW,wa.left+(aw-ww)/2,wa.top+(ah-wh)/2,ww,wh,NULL,NULL,hi,NULL);
  if(!g_hwnd){ MessageBoxW(NULL,L"Impossible de créer la fenêtre.",L"Gouffre",MB_ICONERROR); return 1; }
  g_dc=GetDC(g_hwnd);

  memset(&g_bmi,0,sizeof g_bmi);
  g_bmi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER); g_bmi.bmiHeader.biWidth=SCREEN_W; g_bmi.bmiHeader.biHeight=-SCREEN_H;
  g_bmi.bmiHeader.biPlanes=1; g_bmi.bmiHeader.biBitCount=32; g_bmi.bmiHeader.biCompression=BI_RGB;

  g_gl=gl_init();
  gpu_start();
  HMODULE dwm=LoadLibraryA("dwmapi.dll"); if(dwm) pDwmFlush=(HRESULT (WINAPI*)(void))(void*)GetProcAddress(dwm,"DwmFlush");

  ShowWindow(g_hwnd,SW_SHOW); UpdateWindow(g_hwnd);
  if(g_gl){ glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT); SwapBuffers(g_dc); }

  game_init();
  pads_init(g_hwnd,g_diag);
  audio_start();
  memset(&g_in,0,sizeof g_in);
  { extern bool game_wants_fullscreen_at_start(void); if(game_wants_fullscreen_at_start()) set_fullscreen(true); }

  update_mouse(); g_in.mouse_moved=false;      /* pas de faux mouvement de souris au demarrage */
  double last=now_s();
  while(g_running){
    MSG msg;
    while(PeekMessageW(&msg,NULL,0,0,PM_REMOVE)){
      if(msg.message==WM_QUIT){ g_running=false; break; }
      TranslateMessage(&msg); DispatchMessageW(&msg);
    }
    if(!g_running) break;
    double t0=now_s(); float dt=(float)(t0-last); last=t0; if(dt>0.1f) dt=0.1f;
    update_mouse();
    pads_poll(&g_in);
    { static double next_diag=0; if(t0>=next_diag){ next_diag=t0+2.0; pads_diag_flush(); } }
    game_frame(&g_in,dt);
    memset(g_in.key_pressed,0,sizeof g_in.key_pressed); memset(g_in.mb_pressed,0,sizeof g_in.mb_pressed);
    memset(g_in.pad_pressed,0,sizeof g_in.pad_pressed); g_in.mouse_moved=false; g_in.wheel=0;
    int fr=game_fullscreen_request(); if(fr>=0) set_fullscreen(fr!=0);
    if(game_should_quit()) break;
    if(IsIconic(g_hwnd)){ Sleep(30); continue; }
    present();
    /* cadence : vsync si dispo, sinon compositeur, sinon ~60 i/s */
    double el=now_s()-t0;
    if(!(g_gl && g_vsync)){
      if(!g_gl && pDwmFlush && pDwmFlush()==S_OK){ /* synchro sur l'ecran */ }
      else { double target=1.0/60.0; while(el<target){ if(target-el>0.002) Sleep(1); el=now_s()-t0; } }
    }
    if(now_s()-t0<0.002) Sleep(1);
  }
  pads_rumble_stop();
  pads_diag_flush();
  game_shutdown();
  audio_stop();
  if(g_rc){ gpu_shutdown(); wglMakeCurrent(NULL,NULL); wglDeleteContext(g_rc); }
  ReleaseDC(g_hwnd,g_dc);
  DestroyWindow(g_hwnd);
  timeEndPeriod(1);
  return 0;
}
