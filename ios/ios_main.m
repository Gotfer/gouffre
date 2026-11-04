/* GOUFFRE - couche iPhone / iPad (UIKit)
   - image : rendu GPU OpenGL ES 3 (eclairage, HDR, bloom, decors HD 4K si le dossier
     Documents/donnees est present) ; a defaut, rendu logiciel 640x360 en pixels nets
   - son : AudioUnit (RemoteIO), 44,1 kHz stereo
   - manette : GameController (DualSense, DualShock, Xbox, MFi), vibrations par CoreHaptics
   - tactile : au doigt, comme une souris dans les menus ; pendant une descente,
     moitie gauche = stick virtuel, moitie droite = visee ; deux doigts = pause */
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
#define MAXT 8
@interface GameView : UIView
@end
@implementation GameView {
  CADisplayLink *_link; double _last; Input _in;
  bool _prevKey[KEY_COUNT], _prevPad[PAD_COUNT], _prevMb[MOUSE_COUNT];
  /* tactile */
  UITouch *_stick; CGPoint _stick0, _stickP;   /* stick virtuel (moitie gauche, pendant une descente) */
  UITouch *_aim;                               /* visee / souris */
  bool _tap, _mouseDown, _escape; float _tx, _ty; bool _moved;
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
    _cpu.layer.magnificationFilter=kCAFilterNearest; _cpu.layer.contentsGravity=kCAGravityResizeAspect;
    [self addSubview:_cpu];
    _cs=CGColorSpaceCreateDeviceRGB();
    memset(&_in,0,sizeof _in); _tx=SCREEN_W/2; _ty=SCREEN_H/2;
    _link=[CADisplayLink displayLinkWithTarget:self selector:@selector(tick:)];
    if(@available(iOS 15.0, *)) _link.preferredFrameRateRange=CAFrameRateRangeMake(60,60,60);
    [_link addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];
  }
  return self;
}
- (void)layoutSubviews {
  [super layoutSubviews];
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
  float k=fminf((float)_bw/SCREEN_W,(float)_bh/SCREEN_H);
  int dw=(int)(SCREEN_W*k+0.5f), dh=(int)(SCREEN_H*k+0.5f), dx=(_bw-dw)/2, dy=(_bh-dh)/2;
  gpu_render(_fb,_bw,_bh,dx,_bh-dy-dh,dw,dh);
  if(gpu_failed()){ g_gpu_ok=false; gpu_reset_state(); NSLog(@"Gouffre : erreur GPU, bascule en rendu logiciel"); return false; }
  glBindRenderbuffer(GL_RENDERBUFFER,_rb);
  [g_ctx presentRenderbuffer:GL_RENDERBUFFER];
  return true;
}
/* rectangle de l'image (proportions conservees) */
- (CGRect)gameRect {
  CGSize b=self.bounds.size; float k=fminf(b.width/SCREEN_W,b.height/SCREEN_H);
  float w=SCREEN_W*k, h=SCREEN_H*k; return CGRectMake((b.width-w)/2,(b.height-h)/2,w,h);
}
- (CGPoint)toGame:(CGPoint)p { CGRect r=[self gameRect]; return CGPointMake((p.x-r.origin.x)/r.size.width*SCREEN_W,(p.y-r.origin.y)/r.size.height*SCREEN_H); }
- (bool)inPlay { return g_state==GS_RUN && R.sub==RS_PLAY; }
- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)ev {
  if(ev.allTouches.count>=2 && !_stick){ _escape=true; }   /* deux doigts : pause / retour */
  for(UITouch *t in touches){ CGPoint g=[self toGame:[t locationInView:self]];
    if([self inPlay] && g.x<SCREEN_W*0.45f && !_stick){ _stick=t; _stick0=g; _stickP=g; continue; }
    if(!_aim){ _aim=t; _tx=g.x; _ty=g.y; _moved=true; _tap=true; _mouseDown=true; } }
}
- (void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)ev {
  for(UITouch *t in touches){ CGPoint g=[self toGame:[t locationInView:self]];
    if(t==_stick) _stickP=g; else if(t==_aim){ _tx=g.x; _ty=g.y; _moved=true; } }
}
- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)ev {
  for(UITouch *t in touches){ if(t==_stick) _stick=nil; if(t==_aim){ _aim=nil; _mouseDown=false; } }
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
- (void)tick:(CADisplayLink*)l {
  double now=CACurrentMediaTime(); float dt= _last>0 ? (float)(now-_last) : 1.0f/60; _last=now; if(dt>0.1f) dt=0.1f;
  [self pollPad];
  /* clavier virtuel : le stick tactile devient des fleches */
  for(int i=0;i<KEY_COUNT;i++) _in.key[i]=false;
  if(_stick && [self inPlay]){ float dx=_stickP.x-_stick0.x, dy=_stickP.y-_stick0.y; const float dz=7;
    _in.key[KEY_LEFT]=dx<-dz; _in.key[KEY_RIGHT]=dx>dz; _in.key[KEY_UP]=dy<-dz; _in.key[KEY_DOWN]=dy>dz; }
  if(_escape){ _in.key[KEY_ESCAPE]=true; _escape=false; }
  _in.mx=_tx; _in.my=_ty; _in.mouse_inside=true; _in.mouse_moved=_moved; _moved=false;
  _in.mb[MOUSE_LEFT]=_mouseDown || _tap; _tap=false;
  for(int i=0;i<KEY_COUNT;i++){ _in.key_pressed[i]=_in.key[i] && !_prevKey[i]; _prevKey[i]=_in.key[i]; }
  for(int i=0;i<PAD_COUNT;i++){ _in.pad_pressed[i]=_in.pad[i] && !_prevPad[i]; _prevPad[i]=_in.pad[i]; }
  for(int i=0;i<MOUSE_COUNT;i++){ _in.mb_pressed[i]=_in.mb[i] && !_prevMb[i]; _prevMb[i]=_in.mb[i]; }
  _in.wheel=0;
  game_frame(&_in,dt);
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
