/* GOUFFRE - couche iPhone / iPad (UIKit)
   - image : rendu GPU OpenGL ES 3 (eclairage, HDR, bloom, decors HD 4K si le dossier
     Documents/donnees est present) ; a defaut, rendu logiciel 640x360 en pixels nets
   - son : AudioUnit (RemoteIO), 44,1 kHz stereo
   - manette : GameController (DualSense, DualShock, Xbox, MFi), vibrations par CoreHaptics
   - tactile : au doigt, comme une souris dans les menus (glisser = defiler) ; pendant une
     descente, deux sticks virtuels flottants (pouce gauche : deplacement analogique,
     pouce droit : visee) ; boutons Pause / Vitesse / Visee dans les bandes sur les cotes */
#import <UIKit/UIKit.h>
#import <GameController/GameController.h>
#import <CoreHaptics/CoreHaptics.h>
#import <AVFoundation/AVFoundation.h>
#import <AudioToolbox/AudioToolbox.h>
#define GLES_SILENCE_DEPRECATION 1
#import <OpenGLES/EAGL.h>
#import <OpenGLES/ES3/gl.h>
#include <dlfcn.h>
#include <pthread.h>
#include <mach/mach_time.h>
#include "game.h"

/* ---------------- plateforme ---------------- */
static char g_save[1024];
const char *plat_save_path(void){ return g_save; }
typedef struct { void (*fn)(void*); void *arg; } ThrArg;
static void *thr_entry(void *p){ ThrArg a=*(ThrArg*)p; free(p); a.fn(a.arg); return NULL; }
bool plat_thread_start(void (*fn)(void*),void *arg){
  ThrArg *a=(ThrArg*)malloc(sizeof *a); if(!a) return false; a->fn=fn; a->arg=arg;
  pthread_t t; pthread_attr_t at; pthread_attr_init(&at); pthread_attr_setdetachstate(&at,PTHREAD_CREATE_DETACHED);
  int r=pthread_create(&t,&at,thr_entry,a); pthread_attr_destroy(&at);
  if(r!=0){ free(a); return false; }
  return true;
}
static CHHapticEngine *g_haptic API_AVAILABLE(ios(14.0));
void plat_rumble(float strength,float seconds){
  if(@available(iOS 14.0, *)){
    if(!g_haptic) return;
    NSError *err=nil;
    CHHapticEventParameter *in=[[CHHapticEventParameter alloc] initWithParameterID:CHHapticEventParameterIDHapticIntensity value:fminf(1,fmaxf(0,strength))];
    CHHapticEvent *ev=[[CHHapticEvent alloc] initWithEventType:CHHapticEventTypeHapticContinuous parameters:@[in] relativeTime:0 duration:fmaxf(0.05f,seconds)];
    CHHapticPattern *pat=[[CHHapticPattern alloc] initWithEvents:@[ev] parameters:@[] error:&err];
    if(!pat) return;
    id<CHHapticPatternPlayer> pl=[g_haptic createPlayerWithPattern:pat error:&err];
    [pl startAtTime:CHHapticTimeImmediate error:&err];
  }
}

/* ---------------- son ---------------- */
static AudioComponentInstance g_au;
static OSStatus au_render(void *ref,AudioUnitRenderActionFlags *flags,const AudioTimeStamp *ts,UInt32 bus,UInt32 frames,AudioBufferList *io){
  (void)ref; (void)flags; (void)ts; (void)bus;
  int16_t *out=(int16_t*)io->mBuffers[0].mData;
  game_audio_mix(out,(int)frames);
  return noErr;
}
static void audio_start(void){
  AVAudioSession *s=[AVAudioSession sharedInstance];
  [s setCategory:AVAudioSessionCategoryAmbient error:nil];
  [s setPreferredSampleRate:AUDIO_RATE error:nil];
  [s setActive:YES error:nil];
  AudioComponentDescription d={0}; d.componentType=kAudioUnitType_Output; d.componentSubType=kAudioUnitSubType_RemoteIO; d.componentManufacturer=kAudioUnitManufacturer_Apple;
  AudioComponent c=AudioComponentFindNext(NULL,&d); if(!c) return;
  if(AudioComponentInstanceNew(c,&g_au)!=noErr) return;
  AudioStreamBasicDescription f={0};
  f.mSampleRate=AUDIO_RATE; f.mFormatID=kAudioFormatLinearPCM; f.mFormatFlags=kAudioFormatFlagIsSignedInteger|kAudioFormatFlagIsPacked;
  f.mChannelsPerFrame=2; f.mBitsPerChannel=16; f.mBytesPerFrame=4; f.mFramesPerPacket=1; f.mBytesPerPacket=4;
  AudioUnitSetProperty(g_au,kAudioUnitProperty_StreamFormat,kAudioUnitScope_Input,0,&f,sizeof f);
  AURenderCallbackStruct cb={au_render,NULL};
  AudioUnitSetProperty(g_au,kAudioUnitProperty_SetRenderCallback,kAudioUnitScope_Input,0,&cb,sizeof cb);
  if(AudioUnitInitialize(g_au)==noErr) AudioOutputUnitStart(g_au);
}

/* ---------------- carte graphique ---------------- */
static EAGLContext *g_ctx;
extern bool game_uses_gpu(void);
static void *ios_getproc(const char *n){ return dlsym(RTLD_DEFAULT,n); }
static void gl_start(void){
  g_ctx=[[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3];
  if(!g_ctx || ![EAGLContext setCurrentContext:g_ctx]){ g_ctx=nil; NSLog(@"Gouffre : OpenGL ES 3 indisponible, rendu logiciel"); return; }
  static char log[8192]; log[0]=0;
  g_gpu_ok=gpu_init(ios_getproc,log,sizeof log);
  NSLog(@"Gouffre : rendu %s\n%s",g_gpu_ok?"GPU":"logiciel",log);
}

/* ---------------- vue du jeu ---------------- */
/* boutons tactiles, ranges dans les bandes noires sur les cotes de l'image */
enum { TB_PAUSE, TB_SPEED, TB_AIM, TB_N };
static const int TB_KEY[TB_N]={KEY_ESCAPE,KEY_R,KEY_TAB};
static const bool TB_PLAY[TB_N]={false,true,true};
#define STICK_R 54.0f      /* rayon du stick virtuel (points) */

@interface GameView : UIView
@end
@implementation GameView {
  CADisplayLink *_link; double _last; Input _in;
  bool _prevKey[KEY_COUNT], _prevPad[PAD_COUNT], _prevMb[MOUSE_COUNT];
  /* tactile : menus (doigt = souris) */
  UITouch *_aim; bool _tap, _mouseDown; float _tx, _ty; bool _moved; float _scrollAcc, _lastGY; int _wheel;
  /* tactile : partie (deux sticks virtuels flottants) */
  UITouch *_mv; CGPoint _mv0, _mvP;
  UITouch *_am; CGPoint _am0, _amP; double _amT; float _amMax;
  /* boutons */
  UIView *_btn[TB_N]; UILabel *_btnL[TB_N]; UITouch *_btnT[TB_N]; bool _btnPress[TB_N];
  UIView *_ovl; CAShapeLayer *_sb[2], *_sk[2];
  /* interface agrandie : zoom (pincer) et deplacement (glisser) dans les menus */
  float _zoom, _menuZoom, _panX, _panY; int _scrKey;
  UITouch *_t1, *_t2; CGPoint _t1s, _t1p; double _t1T; bool _panning, _pinched;
  float _pinD0, _pinZ0; CGPoint _pinG0;
  UIView *_scrH, *_scrV;                       /* barres de defilement */
  CGColorSpaceRef _cs;
  UIView *_cpu;                                /* image du rendu logiciel (si pas de GPU) */
  GLuint _fb, _rb; GLint _bw, _bh;             /* image GPU : tampon de l'ecran */
}
+ (Class)layerClass { return [CAEAGLLayer class]; }
- (instancetype)initWithFrame:(CGRect)fr {
  if((self=[super initWithFrame:fr])){
    self.multipleTouchEnabled=YES; self.backgroundColor=[UIColor blackColor];
    CAEAGLLayer *el=(CAEAGLLayer*)self.layer; el.opaque=YES;
    el.drawableProperties=@{kEAGLDrawablePropertyRetainedBacking:@NO, kEAGLDrawablePropertyColorFormat:kEAGLColorFormatRGBA8};
    self.contentScaleFactor=[UIScreen mainScreen].nativeScale;
    if(g_ctx){ [EAGLContext setCurrentContext:g_ctx]; glGenFramebuffers(1,&_fb); glGenRenderbuffers(1,&_rb); }
    _cpu=[[UIView alloc] initWithFrame:self.bounds]; _cpu.autoresizingMask=UIViewAutoresizingFlexibleWidth|UIViewAutoresizingFlexibleHeight;
    _cpu.userInteractionEnabled=NO; _cpu.backgroundColor=[UIColor blackColor];
    _cpu.layer.magnificationFilter=kCAFilterNearest; _cpu.layer.contentsGravity=kCAGravityResize; _cpu.autoresizingMask=UIViewAutoresizingNone;
    [self addSubview:_cpu];
    /* calque des commandes tactiles (dessin seulement : les doigts sont lus par cette vue) */
    _ovl=[[UIView alloc] initWithFrame:self.bounds]; _ovl.autoresizingMask=UIViewAutoresizingFlexibleWidth|UIViewAutoresizingFlexibleHeight;
    _ovl.userInteractionEnabled=NO; _ovl.backgroundColor=[UIColor clearColor]; [self addSubview:_ovl];
    for(int i=0;i<2;i++){
      _sb[i]=[CAShapeLayer layer]; _sb[i].path=[UIBezierPath bezierPathWithOvalInRect:CGRectMake(-STICK_R,-STICK_R,STICK_R*2,STICK_R*2)].CGPath;
      _sb[i].fillColor=[UIColor colorWithWhite:1 alpha:0.07].CGColor; _sb[i].strokeColor=[UIColor colorWithWhite:1 alpha:0.35].CGColor; _sb[i].lineWidth=2; _sb[i].hidden=YES;
      _sk[i]=[CAShapeLayer layer]; _sk[i].path=[UIBezierPath bezierPathWithOvalInRect:CGRectMake(-23,-23,46,46)].CGPath;
      _sk[i].fillColor=(i==0?[UIColor colorWithWhite:1 alpha:0.32]:[UIColor colorWithRed:1 green:0.85 blue:0.3 alpha:0.38]).CGColor; _sk[i].hidden=YES;
      [_ovl.layer addSublayer:_sb[i]]; [_ovl.layer addSublayer:_sk[i]];
    }
    for(int i=0;i<TB_N;i++){
      UIView *b=[[UIView alloc] initWithFrame:CGRectMake(0,0,60,60)]; b.userInteractionEnabled=NO;
      b.backgroundColor=[UIColor colorWithWhite:0.08 alpha:0.55]; b.layer.borderWidth=1.5; b.layer.borderColor=[UIColor colorWithRed:0.78 green:0.58 blue:0.35 alpha:0.7].CGColor;
      UILabel *l=[[UILabel alloc] initWithFrame:b.bounds]; l.autoresizingMask=UIViewAutoresizingFlexibleWidth|UIViewAutoresizingFlexibleHeight;
      l.textAlignment=NSTextAlignmentCenter; l.numberOfLines=2; l.adjustsFontSizeToFitWidth=YES; l.minimumScaleFactor=0.6;
      l.textColor=[UIColor colorWithRed:0.98 green:0.93 blue:0.85 alpha:1]; l.font=[UIFont boldSystemFontOfSize:13];
      [b addSubview:l]; [_ovl addSubview:b]; _btn[i]=b; _btnL[i]=l;
    }
    _btnL[TB_SPEED].text=@"⏩\nVitesse"; _btnL[TB_AIM].text=@"◎\nVisée"; _btnL[TB_PAUSE].text=@"❚❚\nPause";
    _cs=CGColorSpaceCreateDeviceRGB();
    { float z=[[NSUserDefaults standardUserDefaults] floatForKey:@"zoomMenus"]; _menuZoom = z>=1.0f ? z : 1.75f; _zoom=1; _scrKey=-1; }
    for(int i=0;i<2;i++){ UIView *v=[[UIView alloc] initWithFrame:CGRectZero]; v.userInteractionEnabled=NO; v.backgroundColor=[UIColor colorWithWhite:1 alpha:0.45]; v.layer.cornerRadius=2; v.hidden=YES; [_ovl addSubview:v]; if(i==0) _scrH=v; else _scrV=v; }
    memset(&_in,0,sizeof _in); _tx=SCREEN_W/2; _ty=SCREEN_H/2;
    _link=[CADisplayLink displayLinkWithTarget:self selector:@selector(tick:)];
    if(@available(iOS 15.0, *)) _link.preferredFrameRateRange=CAFrameRateRangeMake(60,60,60);
    [_link addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];
  }
  return self;
}
/* rectangle de l'image (proportions conservees) */
- (CGRect)fitRect {
  CGSize b=self.bounds.size; float k=fminf(b.width/SCREEN_W,b.height/SCREEN_H);
  float w=SCREEN_W*k, h=SCREEN_H*k; return CGRectMake((b.width-w)/2,(b.height-h)/2,w,h);
}
- (float)scale { CGSize b=self.bounds.size; return fminf(b.width/SCREEN_W,b.height/SCREEN_H)*_zoom; }   /* points par pixel du jeu */
- (void)clampPan {
  CGSize b=self.bounds.size; float s=[self scale]; float vw=b.width/s, vh=b.height/s;
  _panX = vw>=SCREEN_W ? 0 : fminf(fmaxf(_panX,0),SCREEN_W-vw);
  _panY = vh>=SCREEN_H ? 0 : fminf(fmaxf(_panY,0),SCREEN_H-vh);
}
/* rectangle de l'image a l'ecran (agrandie et deplacee dans les menus) */
- (CGRect)gameRect {
  CGSize b=self.bounds.size; float s=[self scale]; float w=SCREEN_W*s, h=SCREEN_H*s;
  float x = w<=b.width ? (b.width-w)/2 : -_panX*s, y = h<=b.height ? (b.height-h)/2 : -_panY*s;
  return CGRectMake(x,y,w,h);
}
- (void)layoutButtons {
  CGRect g=[self fitRect]; CGSize B=self.bounds.size; float bar=g.origin.x;
  if(bar>=50){   /* iPhone : bandes noires sur les cotes */
    float s=fminf(bar-10,68), lx=bar/2, rx=B.width-bar/2, yt=g.origin.y+g.size.height*0.20f, yb=g.origin.y+g.size.height*0.80f;
    _btn[TB_PAUSE].center=CGPointMake(lx,yt); _btn[TB_SPEED].center=CGPointMake(lx,yb); _btn[TB_AIM].center=CGPointMake(rx,yt);
    for(int i=0;i<TB_N;i++){ _btn[i].bounds=CGRectMake(0,0,s,s); _btn[i].layer.cornerRadius=s*0.26f; _btn[i].alpha=1; }
  } else {       /* iPad : sur l'image, discrets */
    float s=56;
    _btn[TB_PAUSE].center=CGPointMake(g.origin.x+s*0.5f+8,g.origin.y+g.size.height*0.45f);
    _btn[TB_SPEED].center=CGPointMake(g.origin.x+s*0.5f+8,g.origin.y+g.size.height*0.62f);
    _btn[TB_AIM].center=CGPointMake(CGRectGetMaxX(g)-s*0.5f-8,g.origin.y+g.size.height*0.62f);
    for(int i=0;i<TB_N;i++){ _btn[i].bounds=CGRectMake(0,0,s,s); _btn[i].layer.cornerRadius=s*0.26f; _btn[i].alpha=0.75f; }
  }
}
- (void)layoutSubviews {
  [super layoutSubviews]; [self layoutButtons];
  if(!g_ctx || !_rb) return;
  [EAGLContext setCurrentContext:g_ctx];
  glBindRenderbuffer(GL_RENDERBUFFER,_rb);
  [g_ctx renderbufferStorage:GL_RENDERBUFFER fromDrawable:(CAEAGLLayer*)self.layer];
  glBindFramebuffer(GL_FRAMEBUFFER,_fb);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_RENDERBUFFER,_rb);
  glGetRenderbufferParameteriv(GL_RENDERBUFFER,GL_RENDERBUFFER_WIDTH,&_bw);
  glGetRenderbufferParameteriv(GL_RENDERBUFFER,GL_RENDERBUFFER_HEIGHT,&_bh);
  if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE){ _bw=_bh=0; }
  glBindFramebuffer(GL_FRAMEBUFFER,0);
}
/* image GPU : rend dans le tampon de l'ecran ; faux si le rendu logiciel doit prendre le relais */
- (bool)drawGPU {
  if(!g_ctx || !g_gpu_ok || !game_uses_gpu() || _bw<=0 || _bh<=0) return false;
  if([UIApplication sharedApplication].applicationState==UIApplicationStateBackground) return true;   /* pas de GL en arriere-plan */
  [EAGLContext setCurrentContext:g_ctx];
  CGRect g=[self gameRect]; float px=(float)_bw/self.bounds.size.width;
  int dx=(int)lroundf(g.origin.x*px), dy=(int)lroundf(g.origin.y*px), dw=(int)lroundf(g.size.width*px), dh=(int)lroundf(g.size.height*px);
  gpu_render(_fb,_bw,_bh,dx,_bh-dy-dh,dw,dh);
  if(gpu_failed()){ g_gpu_ok=false; gpu_reset_state(); NSLog(@"Gouffre : erreur GPU, bascule en rendu logiciel"); return false; }
  glBindRenderbuffer(GL_RENDERBUFFER,_rb);
  [g_ctx presentRenderbuffer:GL_RENDERBUFFER];
  return true;
}
/* rectangle de l'image (proportions conservees) */
- (CGPoint)toGame:(CGPoint)p { CGRect r=[self gameRect]; return CGPointMake((p.x-r.origin.x)/r.size.width*SCREEN_W,(p.y-r.origin.y)/r.size.height*SCREEN_H); }
- (bool)inPlay { return g_state==GS_RUN && (R.sub==RS_PLAY || R.sub==RS_INTRO); }
- (int)buttonAt:(CGPoint)p {
  for(int i=0;i<TB_N;i++){ if(_btn[i].hidden) continue; CGRect f=CGRectInset(_btn[i].frame,-10,-10); if(CGRectContainsPoint(f,p)) return i; }
  return -1;
}
- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)ev {
  for(UITouch *t in touches){ CGPoint p=[t locationInView:self];
    int b=[self buttonAt:p];
    if(b>=0 && !_btnT[b]){ _btnT[b]=t; _btnPress[b]=true; continue; }
    if([self inPlay]){
      if(p.x<self.bounds.size.width*0.5f){ if(!_mv){ _mv=t; _mv0=p; _mvP=p; } }
      else if(!_am){ _am=t; _am0=p; _amP=p; _amT=CACurrentMediaTime(); _amMax=0; }
      continue;
    }
    /* menus : un doigt = toucher ou faire glisser l'interface, deux doigts = zoom */
    if(!_t1){ _t1=t; _t1s=p; _t1p=p; _t1T=CACurrentMediaTime(); _panning=false; _pinched=false; _scrollAcc=0;
      if(g_state==GS_CINE) _mouseDown=true; }   /* cinematique : maintenir pour passer */
    else if(!_t2){ _t2=t; _pinched=true; _panning=true;
      CGPoint a=[_t1 locationInView:self], c=p; _pinD0=fmaxf(20,hypotf(a.x-c.x,a.y-c.y)); _pinZ0=_zoom;
      _pinG0=[self toGame:CGPointMake((a.x+c.x)/2,(a.y+c.y)/2)]; }
  }
}
- (void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)ev {
  for(UITouch *t in touches){ CGPoint p=[t locationInView:self];
    if(t==_mv) _mvP=p;
    else if(t==_am){ _amP=p; _amMax=fmaxf(_amMax,hypotf(p.x-_am0.x,p.y-_am0.y)); }
    else if((t==_t1||t==_t2) && _t2){   /* pincer : zoom autour du point entre les doigts */
      CGPoint a=[_t1 locationInView:self], c=[_t2 locationInView:self]; float d=hypotf(a.x-c.x,a.y-c.y);
      _zoom=fminf(fmaxf(_pinZ0*d/_pinD0,1.0f),3.0f); _menuZoom=_zoom;
      float s=[self scale]; CGPoint m=CGPointMake((a.x+c.x)/2,(a.y+c.y)/2);
      _panX=_pinG0.x-m.x/s; _panY=_pinG0.y-m.y/s; [self clampPan]; }
    else if(t==_t1){
      if(!_panning && hypotf(p.x-_t1s.x,p.y-_t1s.y)>10){ _panning=true; }
      if(_panning){ float s=[self scale]; float dx=(p.x-_t1p.x)/s, dy=(p.y-_t1p.y)/s;
        float oy=_panY; _panX-=dx; _panY-=dy; [self clampPan];
        /* au bord de l'ecran, le glissement vertical fait defiler les listes (molette) */
        float rest=dy+(_panY-oy); _scrollAcc+=rest;
        while(_scrollAcc>14){ _wheel++; _scrollAcc-=14; } while(_scrollAcc<-14){ _wheel--; _scrollAcc+=14; } }
      _t1p=p; } }
}
- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)ev {
  for(UITouch *t in touches){
    for(int i=0;i<TB_N;i++) if(_btnT[i]==t) _btnT[i]=nil;
    if(t==_mv) _mv=nil;
    if(t==_am){ /* touche breve sans glisser : c'est un appui (boutons de vitesse, etc.) */
      if(CACurrentMediaTime()-_amT<0.3 && _amMax<14){ CGPoint g=[self toGame:_am0]; _tx=g.x; _ty=g.y; _moved=true; _tap=true; }
      _am=nil; }
    if(t==_t1 || t==_t2){
      if(t==_t1 && !_panning && !_pinched && CACurrentMediaTime()-_t1T<0.6){   /* simple toucher : clic */
        CGPoint g=[self toGame:[t locationInView:self]]; _tx=g.x; _ty=g.y; _moved=true; _tap=true; }
      if(_pinched) [[NSUserDefaults standardUserDefaults] setFloat:_menuZoom forKey:@"zoomMenus"];
      if(t==_t1){ _t1=_t2; _t2=nil; if(_t1){ _t1s=_t1p=[_t1 locationInView:self]; } } else _t2=nil;
      if(!_t1) _mouseDown=false; } }
}
- (void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)ev { [self touchesEnded:touches withEvent:ev]; }

static float btnv(GCControllerButtonInput *b){ return b ? b.value : 0; }
- (void)pollPad {
  GCController *c=[GCController current]; if(!c) c=[GCController controllers].firstObject;
  GCExtendedGamepad *g=c.extendedGamepad;
  bool on= g!=nil;
  _in.pad_on=on;
  for(int i=0;i<PAD_COUNT;i++) _in.pad[i]=false;
  _in.lx=_in.ly=_in.rx=_in.ry=_in.lt=_in.rt=0;
  if(!on) return;
  NSString *cat=c.productCategory?:@"";
  _in.pad_type= ([cat rangeOfString:@"DualSense"].location!=NSNotFound || [cat rangeOfString:@"DualShock"].location!=NSNotFound) ? PADT_PS :
                ([cat rangeOfString:@"Switch"].location!=NSNotFound ? PADT_NINTENDO : PADT_XBOX);
  snprintf(_in.pad_name,sizeof _in.pad_name,"%s",(c.vendorName?:@"manette").UTF8String);
  _in.lx=g.leftThumbstick.xAxis.value; _in.ly=-g.leftThumbstick.yAxis.value;
  _in.rx=g.rightThumbstick.xAxis.value; _in.ry=-g.rightThumbstick.yAxis.value;
  _in.lt=btnv(g.leftTrigger); _in.rt=btnv(g.rightTrigger);
  _in.pad[PAD_A]=g.buttonA.pressed; _in.pad[PAD_B]=g.buttonB.pressed; _in.pad[PAD_X]=g.buttonX.pressed; _in.pad[PAD_Y]=g.buttonY.pressed;
  _in.pad[PAD_START]=g.buttonMenu.pressed; _in.pad[PAD_BACK]=g.buttonOptions ? g.buttonOptions.pressed : false;
  _in.pad[PAD_LB]=g.leftShoulder.pressed; _in.pad[PAD_RB]=g.rightShoulder.pressed;
  _in.pad[PAD_UP]=g.dpad.up.pressed; _in.pad[PAD_DOWN]=g.dpad.down.pressed; _in.pad[PAD_LEFT]=g.dpad.left.pressed; _in.pad[PAD_RIGHT]=g.dpad.right.pressed;
  if(@available(iOS 14.0, *)){ if(!g_haptic && c.haptics){ g_haptic=[c.haptics createEngineWithLocality:GCHapticsLocalityDefault]; [g_haptic startAndReturnError:nil]; } }
}
- (void)updateOverlay {
  [CATransaction begin]; [CATransaction setDisableActions:YES];
  bool play=[self inPlay];
  for(int i=0;i<TB_N;i++){
    bool show = TB_PLAY[i] ? play : (g_state!=GS_CINE && g_state!=GS_TITLE);
    _btn[i].hidden=!show;
    bool down=_btnT[i]!=nil;
    _btn[i].backgroundColor=[UIColor colorWithWhite:down?0.3:0.08 alpha:down?0.75:0.55];
  }
  NSString *pl = play ? @"❚❚\nPause" : @"◀\nRetour";
  if(![_btnL[TB_PAUSE].text isEqualToString:pl]) _btnL[TB_PAUSE].text=pl;
  NSString *vs = S.game_speed==0 ? @"⏩\nx1" : (S.game_speed==1 ? @"⏩\nx1,5" : @"⏩\nx2");
  if(![_btnL[TB_SPEED].text isEqualToString:vs]) _btnL[TB_SPEED].text=vs;
  NSString *am = S.autoaim ? @"◎\nAuto" : @"◎\nDoigt";
  if(![_btnL[TB_AIM].text isEqualToString:am]) _btnL[TB_AIM].text=am;
  /* barres de defilement quand l'interface est agrandie */
  { CGSize B=self.bounds.size; float sc=[self scale]; float vw=B.width/sc, vh=B.height/sc;
    _scrH.hidden = vw>=SCREEN_W-0.5f; _scrV.hidden = vh>=SCREEN_H-0.5f;
    if(!_scrH.hidden){ float L=B.width-80; _scrH.frame=CGRectMake(40+_panX/SCREEN_W*L,B.height-7,vw/SCREEN_W*L,4); }
    if(!_scrV.hidden){ float L=B.height-40; _scrV.frame=CGRectMake(B.width-7,20+_panY/SCREEN_H*L,4,vh/SCREEN_H*L); } }
  /* sticks : visibles sous le doigt */
  UITouch *tt[2]={_mv,_am}; CGPoint o[2]={_mv0,_am0}, q[2]={_mvP,_amP};
  for(int i=0;i<2;i++){
    bool on = tt[i]!=nil && play;
    _sb[i].hidden=!on; _sk[i].hidden=!on;
    if(on){ float dx=q[i].x-o[i].x, dy=q[i].y-o[i].y, d=hypotf(dx,dy); if(d>STICK_R){ dx*=STICK_R/d; dy*=STICK_R/d; }
      _sb[i].position=o[i]; _sk[i].position=CGPointMake(o[i].x+dx,o[i].y+dy); }
  }
  [CATransaction commit];
}
- (void)tick:(CADisplayLink*)l {
  double now=CACurrentMediaTime(); float dt= _last>0 ? (float)(now-_last) : 1.0f/60; _last=now; if(dt>0.1f) dt=0.1f;
  [self pollPad];
  bool play=[self inPlay];
  for(int i=0;i<KEY_COUNT;i++) _in.key[i]=false;
  /* boutons tactiles : touches tenues tant que le doigt reste dessus */
  for(int i=0;i<TB_N;i++){ if(_btnT[i]||_btnPress[i]) _in.key[TB_KEY[i]]=true; _btnPress[i]=false; }
  /* sticks virtuels (analogiques, flottants : la base suit le pouce s'il depasse le bord) */
  _in.touch=true; _in.tmx=_in.tmy=0; _in.taim=false; _in.tax=_in.tay=0;
  if(_mv && play){
    float dx=_mvP.x-_mv0.x, dy=_mvP.y-_mv0.y, d=hypotf(dx,dy);
    if(d>STICK_R){ _mv0.x=_mvP.x-dx/d*STICK_R; _mv0.y=_mvP.y-dy/d*STICK_R; dx=_mvP.x-_mv0.x; dy=_mvP.y-_mv0.y; d=STICK_R; }
    float m=d/STICK_R; const float dz=0.14f;
    if(m>dz){ float k=(m-dz)/(1-dz)/m; _in.tmx=dx/STICK_R*k; _in.tmy=dy/STICK_R*k; }
  }
  if(_am && play){
    float dx=_amP.x-_am0.x, dy=_amP.y-_am0.y, d=hypotf(dx,dy);
    if(d>STICK_R*1.5f){ _am0.x=_amP.x-dx/d*STICK_R*1.5f; _am0.y=_amP.y-dy/d*STICK_R*1.5f; }
    if(d>12){ _in.taim=true; _in.tax=dx/d; _in.tay=dy/d; }
  }
  _in.mx=_tx; _in.my=_ty; _in.mouse_inside=true; _in.mouse_moved=_moved; _moved=false;
  _in.mb[MOUSE_LEFT]=_mouseDown || _tap; _tap=false;
  for(int i=0;i<KEY_COUNT;i++){ _in.key_pressed[i]=_in.key[i] && !_prevKey[i]; _prevKey[i]=_in.key[i]; }
  for(int i=0;i<PAD_COUNT;i++){ _in.pad_pressed[i]=_in.pad[i] && !_prevPad[i]; _prevPad[i]=_in.pad[i]; }
  for(int i=0;i<MOUSE_COUNT;i++){ _in.mb_pressed[i]=_in.mb[i] && !_prevMb[i]; _prevMb[i]=_in.mb[i]; }
  _in.wheel=_wheel; _wheel=0;
  game_frame(&_in,dt);
  /* zoom : image entiere pendant l'action et les cinematiques, interface agrandie ailleurs */
  { bool full=[self inPlay] || g_state==GS_CINE;
    int key=g_state*100+(g_state==GS_RUN?R.sub:0);
    if(!_t2) _zoom = full ? 1.0f : _menuZoom;
    if(key!=_scrKey){ _scrKey=key; CGSize B=self.bounds.size; float sc=[self scale]; _panX=(SCREEN_W-B.width/sc)/2; _panY=(SCREEN_H-B.height/sc)/2; }
    [self clampPan];
    CGRect gr=[self gameRect]; if(!CGRectEqualToRect(_cpu.frame,gr)) _cpu.frame=gr; }
  [self updateOverlay];
  if([self drawGPU]){ _cpu.hidden=YES; return; }
  _cpu.hidden=NO;
  /* image 640x360 (0xAARRGGBB) -> calque */
  const uint32_t *fb=game_framebuffer();
  CGDataProviderRef dp=CGDataProviderCreateWithData(NULL,fb,(size_t)SCREEN_W*SCREEN_H*4,NULL);
  CGImageRef img=CGImageCreate(SCREEN_W,SCREEN_H,8,32,SCREEN_W*4,_cs,kCGBitmapByteOrder32Little|kCGImageAlphaNoneSkipFirst,dp,NULL,false,kCGRenderingIntentDefault);
  /* copie : le jeu reecrit son image a chaque frame */
  CGContextRef ctx=CGBitmapContextCreate(NULL,SCREEN_W,SCREEN_H,8,SCREEN_W*4,_cs,kCGBitmapByteOrder32Little|kCGImageAlphaNoneSkipFirst);
  CGContextDrawImage(ctx,CGRectMake(0,0,SCREEN_W,SCREEN_H),img);
  CGImageRef copy=CGBitmapContextCreateImage(ctx);
  _cpu.layer.contents=(__bridge id)copy;
  CGImageRelease(copy); CGContextRelease(ctx); CGImageRelease(img); CGDataProviderRelease(dp);
}
@end

/* ---------------- application ---------------- */
@interface GameVC : UIViewController
@end
@implementation GameVC
- (void)loadView { self.view=[[GameView alloc] initWithFrame:[UIScreen mainScreen].bounds]; }
- (BOOL)prefersStatusBarHidden { return YES; }
- (BOOL)prefersHomeIndicatorAutoHidden { return YES; }
- (UIRectEdge)preferredScreenEdgesDeferringSystemGestures { return UIRectEdgeAll; }
- (UIInterfaceOrientationMask)supportedInterfaceOrientations { return UIInterfaceOrientationMaskLandscape; }
@end

@interface AppDelegate : UIResponder <UIApplicationDelegate>
@property (strong,nonatomic) UIWindow *window;
@end
@implementation AppDelegate
- (BOOL)application:(UIApplication*)app didFinishLaunchingWithOptions:(NSDictionary*)opt {
  NSString *doc=NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,NSUserDomainMask,YES).firstObject;
  snprintf(g_save,sizeof g_save,"%s/sauvegarde.txt",doc.UTF8String);
  /* donnees facultatives (decors HD 4K + ambiances, ~5 Go) a copier dans Documents/donnees */
  /* le dossier est cherche ou qu'il ait ete depose ("donnees", "donnees 2", Gouffre/donnees...) */
  NSFileManager *fm=[NSFileManager defaultManager];
  NSString *data=[doc stringByAppendingPathComponent:@"donnees"];
  BOOL (^has)(NSString*)=^BOOL(NSString *d){ return [fm fileExistsAtPath:[d stringByAppendingPathComponent:@"niveau_01.gx"]] || [fm fileExistsAtPath:[d stringByAppendingPathComponent:@"campement_0.gx"]] || [fm fileExistsAtPath:[d stringByAppendingPathComponent:@"ambiances"]]; };
  if(!has(data)){
    if(has(doc)) data=doc;
    else { NSDirectoryEnumerator *en=[fm enumeratorAtPath:doc]; NSString *rel;
      while((rel=[en nextObject])){ if(en.level>3){ [en skipDescendants]; continue; }
        NSString *d=[doc stringByAppendingPathComponent:rel]; BOOL dir=NO;
        if([fm fileExistsAtPath:d isDirectory:&dir] && dir && has(d)){ data=d; break; } } }
  }
  setenv("GOUFFRE_DONNEES",data.UTF8String,0);
  NSLog(@"Gouffre : donnees = %@",data);
  bool first=![[NSFileManager defaultManager] fileExistsAtPath:[NSString stringWithUTF8String:g_save]];
  gl_start();
  game_init();
  if(first){ S.autoaim=true; save_write(); }   /* au doigt, la visee automatique aide beaucoup (reglable dans les options) */
  audio_start();
  self.window=[[UIWindow alloc] initWithFrame:[UIScreen mainScreen].bounds];
  self.window.rootViewController=[GameVC new];
  [self.window makeKeyAndVisible];
  return YES;
}
- (void)applicationWillResignActive:(UIApplication*)app { game_focus_lost(); save_write(); if(g_ctx){ [EAGLContext setCurrentContext:g_ctx]; glFinish(); } }
- (void)applicationDidEnterBackground:(UIApplication*)app { save_write(); }
- (void)applicationWillTerminate:(UIApplication*)app { game_shutdown(); }
@end

int main(int argc,char *argv[]){
  @autoreleasepool { return UIApplicationMain(argc,argv,nil,NSStringFromClass([AppDelegate class])); }
}
