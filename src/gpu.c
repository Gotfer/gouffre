/* GOUFFRE - rendu GPU (OpenGL 2.1 + FBO)
   Le CPU dessine le pixel art en couches 640x360 (scene, emission, interface, normales) ;
   la carte graphique compose l'image a la resolution de l'ecran :
   eclairage par pixel avec normales et reflets, HDR, brouillard volumetrique eclaire,
   effets lisses (billes, trainees, particules, rayons), ondes de choc, air brulant,
   bloom multi-echelle, flou de profondeur, aberration chromatique, etalonnage et grain. */
#include "game.h"
#include <stddef.h>

#ifdef _WIN32
#define GLCC __stdcall
#else
#define GLCC
#endif
typedef unsigned int GLenum, GLuint, GLbitfield; typedef int GLint, GLsizei; typedef float GLfloat;
typedef unsigned char GLboolean, GLubyte; typedef char GLchar; typedef ptrdiff_t GLsizeiptr, GLintptr;

#define GL_NO_ERROR 0
#define GL_TRIANGLES 0x0004
#define GL_TEXTURE_2D 0x0DE1
#define GL_UNSIGNED_BYTE 0x1401
#define GL_FLOAT 0x1406
#define GL_RGB 0x1907
#define GL_RGBA 0x1908
#define GL_BGRA 0x80E1
#define GL_RGBA8 0x8058
#define GL_RGBA16F 0x881A
#define GL_R11F_G11F_B10F 0x8C3A
#define GL_NEAREST 0x2600
#define GL_LINEAR 0x2601
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_REPEAT 0x2901
#define GL_TEXTURE0 0x84C0
#define GL_BLEND 0x0BE2
#define GL_ZERO 0
#define GL_ONE 1
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_DEPTH_TEST 0x0B71
#define GL_CULL_FACE 0x0B44
#define GL_SCISSOR_TEST 0x0C11
#define GL_COLOR_BUFFER_BIT 0x4000
#define GL_ARRAY_BUFFER 0x8892
#define GL_STREAM_DRAW 0x88E0
#define GL_STATIC_DRAW 0x88E4
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_FRAMEBUFFER 0x8D40
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_VENDOR 0x1F00
#define GL_RENDERER 0x1F01
#define GL_VERSION 0x1F02
#define GL_SHADING_LANGUAGE_VERSION 0x8B8C
#define GL_UNPACK_ALIGNMENT 0x0CF5
#define GL_PACK_ALIGNMENT 0x0D05
#define GL_MAX_TEXTURE_SIZE 0x0D33
#define GL_LUMINANCE 0x1909
#define GL_LUMINANCE8 0x8040
#define GL_LUMINANCE_ALPHA 0x190A
#define GL_LUMINANCE8_ALPHA8 0x8045
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#define GL_TEXTURE_MAX_LEVEL 0x813D
#define GL_TEXTURE_SWIZZLE_R 0x8E42
#define GL_TEXTURE_SWIZZLE_B 0x8E44
#define GL_RED 0x1903
#define GL_BLUE 0x1905

static struct {
  const GLubyte *(GLCC *GetString)(GLenum);
  GLenum (GLCC *GetError)(void);
  void (GLCC *Viewport)(GLint,GLint,GLsizei,GLsizei);
  void (GLCC *ClearColor)(GLfloat,GLfloat,GLfloat,GLfloat);
  void (GLCC *Clear)(GLbitfield);
  void (GLCC *Enable)(GLenum);
  void (GLCC *Disable)(GLenum);
  void (GLCC *BlendFunc)(GLenum,GLenum);
  void (GLCC *GenTextures)(GLsizei,GLuint*);
  void (GLCC *DeleteTextures)(GLsizei,const GLuint*);
  void (GLCC *BindTexture)(GLenum,GLuint);
  void (GLCC *TexParameteri)(GLenum,GLenum,GLint);
  void (GLCC *TexImage2D)(GLenum,GLint,GLint,GLsizei,GLsizei,GLint,GLenum,GLenum,const void*);
  void (GLCC *TexSubImage2D)(GLenum,GLint,GLint,GLint,GLsizei,GLsizei,GLenum,GLenum,const void*);
  void (GLCC *PixelStorei)(GLenum,GLint);
  void (GLCC *ReadPixels)(GLint,GLint,GLsizei,GLsizei,GLenum,GLenum,void*);
  void (GLCC *DrawArrays)(GLenum,GLint,GLsizei);
  void (GLCC *GetIntegerv)(GLenum,GLint*);
  void (GLCC *Finish)(void);
  void (GLCC *ActiveTexture)(GLenum);
  void (GLCC *GenBuffers)(GLsizei,GLuint*);
  void (GLCC *DeleteBuffers)(GLsizei,const GLuint*);
  void (GLCC *BindBuffer)(GLenum,GLuint);
  void (GLCC *BufferData)(GLenum,GLsizeiptr,const void*,GLenum);
  GLuint (GLCC *CreateShader)(GLenum);
  void (GLCC *ShaderSource)(GLuint,GLsizei,const GLchar*const*,const GLint*);
  void (GLCC *CompileShader)(GLuint);
  void (GLCC *GetShaderiv)(GLuint,GLenum,GLint*);
  void (GLCC *GetShaderInfoLog)(GLuint,GLsizei,GLsizei*,GLchar*);
  void (GLCC *DeleteShader)(GLuint);
  GLuint (GLCC *CreateProgram)(void);
  void (GLCC *AttachShader)(GLuint,GLuint);
  void (GLCC *BindAttribLocation)(GLuint,GLuint,const GLchar*);
  void (GLCC *LinkProgram)(GLuint);
  void (GLCC *GetProgramiv)(GLuint,GLenum,GLint*);
  void (GLCC *GetProgramInfoLog)(GLuint,GLsizei,GLsizei*,GLchar*);
  void (GLCC *DeleteProgram)(GLuint);
  void (GLCC *UseProgram)(GLuint);
  GLint (GLCC *GetUniformLocation)(GLuint,const GLchar*);
  void (GLCC *Uniform1i)(GLint,GLint);
  void (GLCC *Uniform1f)(GLint,GLfloat);
  void (GLCC *Uniform2f)(GLint,GLfloat,GLfloat);
  void (GLCC *Uniform3f)(GLint,GLfloat,GLfloat,GLfloat);
  void (GLCC *Uniform4f)(GLint,GLfloat,GLfloat,GLfloat,GLfloat);
  void (GLCC *Uniform4fv)(GLint,GLsizei,const GLfloat*);
  void (GLCC *EnableVertexAttribArray)(GLuint);
  void (GLCC *DisableVertexAttribArray)(GLuint);
  void (GLCC *VertexAttribPointer)(GLuint,GLint,GLenum,GLboolean,GLsizei,const void*);
  void (GLCC *GenFramebuffers)(GLsizei,GLuint*);
  void (GLCC *DeleteFramebuffers)(GLsizei,const GLuint*);
  void (GLCC *BindFramebuffer)(GLenum,GLuint);
  void (GLCC *FramebufferTexture2D)(GLenum,GLenum,GLenum,GLuint,GLint);
  GLenum (GLCC *CheckFramebufferStatus)(GLenum);
} gl;

int g_gpu_quality=2;
static bool G_ok=false, G_failed=false; static int G_frames=0;
static char G_name[256]="";
static char *G_log; static int G_logn;
static void logf_(const char *fmt,const char *a,const char *b){
  if(!G_log || G_logn<=1) return; size_t l=strlen(G_log); if((int)l>=G_logn-1) return;
  snprintf(G_log+l,(size_t)G_logn-l,fmt,a?a:"",b?b:"");
}
const char *gpu_renderer_name(void){ return G_name; }
static int ico_ver_pub(void);
bool gpu_icons_active(void){ return G_ok && !G_failed && ico_ver_pub()>0; }
bool gpu_failed(void){ return G_failed; }

/* ======================================================================= */
/*                                 SHADERS                                 */
/* ======================================================================= */
#define GLSL(...) "#version 120\n" #__VA_ARGS__

static const char *VS_FULL = GLSL(
  attribute vec2 aPos; varying vec2 vUV;
  void main(){ vUV = aPos*0.5+0.5; gl_Position = vec4(aPos,0.0,1.0); }
);

/* ---- v8 : decors HD precalcules. L'octet alpha de la scene porte un code de region (1..7) ;
   hdmap() donne la position dans l'atlas HD et dans sa version basse definition ---- */
#define GLSLB(...) #__VA_ARGS__
#define HDGLSL GLSLB( \
  uniform vec4 uHdM[8]; uniform vec4 uHdP[8]; uniform vec4 uHdG; uniform vec2 uHdS; \
  bool hdmap(float code, vec2 g, out vec2 ua, out vec2 ul){ \
    vec4 M = vec4(0.0); vec4 P = vec4(0.0); \
    for(int i = 1; i < 8; i++){ if(float(i) == code){ M = uHdM[i]; P = uHdP[i]; } } \
    if(M.z <= 0.0) return false; \
    float tc = M.x*(floor(g.x) + 0.5) + M.y; \
    if(tc < 0.0 || tc >= M.z) return false; \
    float tx = clamp(M.x*g.x + M.y, 0.02, M.z - 0.02); \
    float ty = g.y - uHdS.x*M.w; \
    float tyc = floor(g.y) + 0.5 - uHdS.x*M.w; \
    ua = vec2((P.x + tx)/uHdG.x, ty/uHdG.y); \
    ul = vec2((P.y + floor(tc) + 0.5)/uHdG.z, (mod(floor(tyc), P.z) + 0.5)/uHdG.w); \
    return true; \
  } \
)

/* ---- lumieres : un quad par lumiere, accumulation additive (espace du jeu) ---- */
static const char *VS_LIGHT = GLSL(
  attribute vec2 aPos; attribute vec4 aL0; attribute vec4 aL1; attribute vec2 aL2;
  uniform vec2 uGame;
  varying vec2 vG; varying vec4 vL0; varying vec4 vL1; varying vec2 vL2;
  void main(){
    vG = aPos; vL0 = aL0; vL1 = aL1; vL2 = aL2;
    gl_Position = vec4(aPos.x/uGame.x*2.0-1.0, 1.0-aPos.y/uGame.y*2.0, 0.0, 1.0);
  }
);
static const char *FS_LIGHT = "#version 120\n" HDGLSL "\n" GLSLB(
  uniform sampler2D uNrm; uniform sampler2D uOcc; uniform vec2 uGame; uniform float uLScale; uniform vec2 uLP;   /* force du relief, des reflets */
  uniform sampler2D uScn; uniform sampler2D uHdN;
  varying vec2 vG; varying vec4 vL0; varying vec4 vL1; varying vec2 vL2;
  float occ(vec2 p){ return texture2D(uOcc, (floor(p) + 0.5)/uGame).r*255.0 - 1.0; }
  void main(){
    vec2 d = vL0.xy - vG;
    float q = dot(d,d)/(vL0.z*vL0.z);
    if(q >= 1.0) discard;
    float f = 1.0 - q; f *= f;
    /* ombres portees : on remonte vers la lumiere ; un pixel de sprite plus haut que
       le rayon a cet endroit arrete la lumiere (champ de hauteur en espace ecran) */
    if(vL2.x > 0.0 && occ(vG) < 0.0){
      float dl = length(d);
      float n = clamp(dl*0.6, 4.0, 48.0);
      float s = 0.0;
      for(int i = 1; i <= 48; i++){
        if(float(i) >= n) break;
        float t = float(i)/n;
        if(dl*(1.0 - t) < 11.0) break;      /* ce qui porte la lumiere ne l'arrete pas */
        float hs = occ(vG + d*t);
        if(hs >= 0.0) s = max(s, clamp((hs - vL0.w*t)*0.7 + 0.8, 0.0, 1.0));
      }
      f *= 1.0 - s*vL2.x;
    }
    vec4 n4 = texture2D(uNrm, (floor(vG)+0.5)/uGame);
    if(uHdS.y > 0.5){                       /* relief HD du decor */
      float code = floor(texture2D(uScn, (floor(vG)+0.5)/uGame).a*255.0 + 0.5);
      vec2 ua, ul;
      if(code >= 0.5 && code <= 6.5 && hdmap(code, vG, ua, ul)) n4 = texture2D(uHdN, ua);
    }
    vec3 N = normalize(n4.rgb*2.0 - 1.0);
    vec3 L = normalize(vec3(d, vL0.w));
    float k = mix(1.0, clamp(1.0 + (dot(N, L) - L.z)*1.7, 0.12, 2.3), vL1.w*uLP.x);
    vec3 H = normalize(L + vec3(0.0, 0.32, 0.947));
    float sp = pow(max(dot(N, H), 0.0), 24.0)*n4.a*1.7*uLP.y;
    vec3 c = vL1.rgb*f;
    gl_FragColor = vec4(c*k, dot(c, vec3(0.3,0.59,0.11))*sp)*uLScale;
  }
);

/* ---- composition de la scene : camera, deformations, eclairage, brouillard, emission ---- */
static const char *FS_SCENE = "#version 120\n" HDGLSL "\n" GLSLB(
  uniform sampler2D uScene; uniform sampler2D uEmis; uniform sampler2D uLight; uniform sampler2D uNoise;
  uniform sampler2D uHdA; uniform sampler2D uHdL; uniform float uHdSky;
  uniform vec2 uGame;
  uniform vec4 uCam;      /* decalage x,y ; zoom ; pixels du jeu par pixel de sortie */
  uniform vec4 uCtr;      /* centre du zoom */
  uniform vec4 uView;     /* zone de scene (pixels du jeu) */
  uniform vec4 uLit;      /* zone eclairee */
  uniform vec4 uAmb;      /* ambiance rvb, eclairage actif */
  uniform vec4 uShock[8];
  uniform vec4 uMisc;     /* air brulant, temps, gain d'emission, defilement */
  uniform vec4 uFog;      /* couleur, densite */
  uniform vec4 uFog2;     /* voile sombre du haut, vitesse, 1/echelle lumiere, - */
  uniform vec4 uP25;      /* vue 2.5D : inclinaison K, axe du puits, demi-largeur du puits */
  varying vec2 vUV;
  vec3 sharp(sampler2D t, vec2 g, float w){
    vec2 p = g - 0.5; vec2 c = floor(p); vec2 f = p - c;
    f = clamp((f - 0.5)/w + 0.5, 0.0, 1.0);
    return texture2D(t, (c + 0.5 + f)/uGame).rgb;
  }
  void main(){
    vec2 s = vec2(vUV.x*uGame.x, (1.0 - vUV.y)*uGame.y);
    if(s.x < uView.x || s.y < uView.y || s.x >= uView.z || s.y >= uView.w){ gl_FragColor = vec4(0.0,0.0,0.0,1.0); return; }
    float ls = 1.0;
    if(uP25.x > 0.0){   /* vue 2.5D : ecran -> jeu */
      float K = uP25.x, H = uGame.y, a = K/H, b = 1.0 - K;
      float yy = (-b + sqrt(max(b*b + 4.0*a*s.y, 0.0)))/(2.0*a);
      ls = 1.0 - K + 2.0*K*yy/H;
      float cx = uP25.y, hw = uP25.z, fw = uP25.w, x0 = cx - hw, x1 = cx + hw, W = uGame.x;
      float xl = cx - hw*ls, xr = cx + hw*ls;
      float F = fw*(1.0 + (0.55 + 1.5*yy/H - 1.0)*(K/0.13));   /* paroi du puits */
      float a0 = x0 - fw, a1 = x1 + fw;
      float xx;
      if(s.x < xl - F) xx = s.x*a0/(xl - F);
      else if(s.x < xl) xx = a0 + (s.x - (xl - F))*fw/F;
      else if(s.x <= xr) xx = cx + (s.x - cx)/ls;
      else if(s.x < xr + F) xx = x1 + (s.x - xr)*fw/F;
      else xx = a1 + (s.x - (xr + F))*(W - a1)/(W - (xr + F));
      s = vec2(xx, yy);
    }
    float pw = uCam.w/ls;
    vec2 g = (s - uCtr.xy)/uCam.z + uCtr.xy + uCam.xy;
    float ring = 0.0;
    for(int i=0;i<8;i++){
      vec4 sh = uShock[i];
      if(sh.w > 0.0){
        vec2 v = g - sh.xy; float d = length(v); float e = abs(d - sh.z);
        if(e < 7.0 && d > 0.5){
          float k = 1.0 - e/7.0; k = k*k*sh.w;
          g -= v/d*(d < sh.z ? -k : k)*3.0;
          ring += k;
        }
      }
    }
    bool lit = uAmb.w > 0.5 && g.x >= uLit.x && g.x < uLit.z && g.y >= uLit.y && g.y < uLit.w;
    if(uMisc.x > 0.0 && lit){
      g.x += sin(g.y*0.21 + uMisc.y*3.3)*sin(g.y*0.045 - uMisc.y*0.9)*uMisc.x;
      g.y += sin(g.x*0.13 + uMisc.y*2.1)*0.25*uMisc.x;
    }
    g = clamp(g, uView.xy + 0.5, uView.zw - 0.5);
    vec3 alb = sharp(uScene, g, pw);
    float emk = 1.0; float hdc = 0.0; bool sky = false;
    if(uHdS.y > 0.5){                       /* decor HD : detail de l'atlas, ombres et voiles du jeu conserves */
      vec4 sc = texture2D(uScene, (floor(g)+0.5)/uGame);
      float code = floor(sc.a*255.0 + 0.5);
      vec2 ua, ul;
      if(code >= 0.5 && code <= 7.5 && hdmap(code, g, ua, ul)){
        vec4 A = texture2D(uHdA, ua); vec4 Lo = texture2D(uHdL, ul);
        float lc = dot(sc.rgb, vec3(0.3,0.59,0.11)), ll = dot(Lo.rgb, vec3(0.3,0.59,0.11));
        float k = clamp(lc/max(ll, 0.02), 0.0, 1.6);
        hdc = code;
        if(code > 6.5){ alb = A.rgb*k*uHdSky; sky = true; }
        else { alb = max(sc.rgb + (A.rgb - Lo.rgb)*k, vec3(0.0)); emk = clamp(A.a/max(Lo.a, 0.08), 0.0, 4.0); }
      }
    }
    vec3 col = alb;
    if(lit && !sky){
      vec4 L = texture2D(uLight, vec2(g.x/uGame.x, 1.0 - g.y/uGame.y))*uFog2.z;
      vec3 il = min(uAmb.rgb + L.rgb, vec3(3.5));
      float m = max(L.r, max(L.g, L.b));
      vec3 tint = m > 0.001 ? L.rgb/m : vec3(1.0);
      col = alb*il + tint*min(L.a, 0.8);
      if(uFog2.x > 0.0){
        float top = clamp(1.0 - (g.y - uLit.y)/64.0, 0.0, 1.0);
        col *= 1.0 - uFog2.x*pow(top, 1.5);
      }
      if(uFog.w > 0.0){
        vec2 q = vec2(g.x, g.y - uMisc.w)/uGame.y;
        float t = uMisc.y*uFog2.y;
        float n1 = texture2D(uNoise, q*vec2(0.85,1.3) + vec2(t*0.013, t*0.004)).r;
        float n2 = texture2D(uNoise, q*vec2(2.3,2.9) - vec2(t*0.021, -t*0.009)).g;
        float n = n1*0.62 + n2*0.38;
        float dens = uFog.w*smoothstep(0.28, 0.82, n);
        vec3 fl = uFog.rgb*(uAmb.rgb*0.3 + L.rgb*1.25);
        col = mix(col, fl, clamp(dens, 0.0, 0.85));
      }
    }
    if(hdc > 0.5) col += texture2D(uEmis, (floor(g)+0.5)/uGame).rgb*emk*uMisc.z;
    else col += sharp(uEmis, g, pw)*uMisc.z;
    col += vec3(ring*0.16);
    if(uFog2.w > 0.5) col = texture2D(uLight, vec2(g.x/uGame.x, 1.0 - g.y/uGame.y)).rgb*uFog2.z*0.6;
    gl_FragColor = vec4(col, 1.0);
  }
);

/* ---- effets lisses (glows, disques, anneaux, trainees, billes, fumee, rayons, etoiles, faisceaux) ---- */
static const char *VS_FX = GLSL(
  attribute vec2 aPos; attribute vec4 aA; attribute vec4 aB; attribute vec4 aC; attribute vec4 aE; attribute vec4 aD;
  uniform vec2 uGame; uniform vec4 uCam; uniform vec4 uCtr; uniform vec4 uP25;
  varying vec2 vG; varying vec4 vA; varying vec4 vB; varying vec4 vC; varying vec4 vE; varying vec4 vD; varying float vS;
  void main(){
    vG = aPos; vA = aA; vB = aB; vC = aC; vE = aE; vD = aD;
    vec2 s = (aPos - uCam.xy - uCtr.xy)*uCam.z + uCtr.xy;
    vS = 1.0;
    if(uP25.x > 0.0){   /* vue 2.5D : jeu -> ecran */
      float K = uP25.x, H = uGame.y, cx = uP25.y, hw = uP25.z, fw = uP25.w, x0 = cx - hw, x1 = cx + hw, W = uGame.x;
      float ls = 1.0 - K + 2.0*K*s.y/H;
      float Y = (1.0 - K)*s.y + K*s.y*s.y/H;
      float xl = cx - hw*ls, xr = cx + hw*ls;
      float F = fw*(1.0 + (0.55 + 1.5*s.y/H - 1.0)*(K/0.13));
      float a0 = x0 - fw, a1 = x1 + fw;
      float X;
      if(s.x < a0) X = s.x*(xl - F)/a0;
      else if(s.x < x0) X = xl - F + (s.x - a0)*F/fw;
      else if(s.x <= x1) X = cx + (s.x - cx)*ls;
      else if(s.x < a1) X = xr + (s.x - x1)*F/fw;
      else X = xr + F + (s.x - a1)*(W - (xr + F))/(W - a1);
      s = vec2(X, Y); vS = max(ls, 0.2);
    }
    gl_Position = vec4(s.x/uGame.x*2.0-1.0, 1.0-s.y/uGame.y*2.0, 0.0, 1.0);
  }
);
static const char *FS_FX = GLSL(
  uniform sampler2D uNoise; uniform sampler2D uLight; uniform vec4 uCam; uniform float uTime; uniform vec4 uAmbL;   /* ambiance, 1/echelle lumiere */
  varying vec2 vG; varying vec4 vA; varying vec4 vB; varying vec4 vC; varying vec4 vE; varying vec4 vD; varying float vS;
  vec3 hue(float h){ return clamp(abs(fract(h + vec3(0.0, 0.6667, 0.3333))*6.0 - 3.0) - 1.0, 0.0, 1.0); }
  vec4 ball(vec2 v, float r, float st, float glowf, vec3 c, vec3 c2, float evo, float px){
    vec4 o = vec4(0.0);
    float d = length(v);
    if(glowf > 0.5){ float q = d/(r*2.6 + 2.0); if(q < 1.0){ float f = 1.0 - q*q; f *= f; o.rgb += c*f*0.32; } }
    float shape = d;
    if(st > 1.5 && st < 2.5) shape = (abs(v.x) + abs(v.y))*0.8;
    float cov = clamp((r - shape)/px + 0.5, 0.0, 1.0);
    if(cov <= 0.0) return o;
    vec2 u = v/max(r, 0.5);
    vec3 n = vec3(u, sqrt(max(0.0, 1.0 - dot(u, u))));
    vec3 Ld = normalize(vec3(-0.55, -0.65, 0.55));
    float dif = max(dot(n, Ld), 0.0);
    vec3 base = mix(c2, c, smoothstep(0.05, 0.85, dif*0.8 + 0.2));
    float t = uTime; float alpha = 1.0;
    if(st > 0.5 && st < 1.5){
      float w = texture2D(uNoise, v*0.08 + vec2(t*0.2, -t*0.3)).r;
      alpha = 0.5 + 0.3*w; base = mix(c2, c, 0.55 + 0.45*w);
    } else if(st > 1.5 && st < 2.5){
      vec2 sg = sign(v + 0.0001);
      n = normalize(vec3(sg*0.55, 0.75)); dif = max(dot(n, Ld), 0.0);
      base = mix(c2, c, dif) + vec3(0.3)*step(v.x, 0.0)*step(v.y, 0.0);
    } else if(st > 2.5 && st < 3.5){
      float a = atan(v.y, v.x)/6.2832 + t*0.35;
      base = mix(hue(a), vec3(1.0), 0.15)*(0.65 + 0.5*dif);
    } else if(st > 3.5 && st < 4.5){
      if(v.x < 0.0) base = c2*(0.45 + 0.65*dif);
    } else if(st > 4.5 && st < 5.5){
      base = mix(c, vec3(1.0), 0.45 + 0.25*sin(t*9.0));
    } else if(st > 5.5 && st < 6.5){
      float k = texture2D(uNoise, v*0.23 + vec2(floor(t*12.0)*0.37, floor(t*12.0)*0.11)).b;
      base += vec3(1.0, 1.0, 0.6)*smoothstep(0.55, 0.7, k)*1.6;
    } else if(st > 6.5 && st < 7.5){
      float a = atan(v.y, v.x) + d*0.7 - t*10.0;
      base = mix(base, vec3(1.0), smoothstep(0.55, 1.0, sin(a*3.0))*0.6);
    } else if(st > 7.5 && st < 8.5){
      float k = texture2D(uNoise, v*0.11 + vec2(t*0.05, t*0.9)).g;
      base = mix(c2, c, k) + vec3(1.0, 0.75, 0.35)*smoothstep(0.5, 0.9, k)*0.9;
    } else if(st > 8.5 && st < 9.5){
      base = mix(base, vec3(0.86, 0.97, 1.0), 0.25 + 0.4*(1.0 - n.z));
    } else if(st > 10.5 && st < 11.5){   /* echo : ondes concentriques */
      float w = 0.5 + 0.5*sin((d/max(r, 0.5))*9.0 - t*10.0);
      base = mix(c2, c, 0.25 + 0.75*w)*(0.7 + 0.45*dif);
    } else if(st > 11.5 && st < 12.5){   /* or poli */
      vec3 R2 = reflect(vec3(0.0, 0.0, -1.0), n);
      float band = smoothstep(-0.15, 0.3, -R2.y);
      base = mix(c2*0.75, c*1.3, band) + vec3(1.0, 0.95, 0.7)*pow(max(dot(n, normalize(vec3(-0.4, -0.6, 0.7))), 0.0), 30.0)*1.3;
    } else if(st > 12.5 && st < 13.5){   /* racines : bois et feuilles */
      float g = texture2D(uNoise, v*0.18 + vec2(0.3, 0.7)).r;
      float ring = 0.5 + 0.5*sin(d*5.0 + g*4.0);
      base = mix(c2, c, ring*0.6 + 0.2)*(0.6 + 0.55*dif);
      float leaf = smoothstep(0.62, 0.72, texture2D(uNoise, v*0.35 + vec2(0.11, 0.5)).g);
      base = mix(base, vec3(0.45, 0.82, 0.25), leaf*0.85);
    } else if(st > 13.5){   /* acide : bulles qui montent */
      float k = texture2D(uNoise, v*0.2 + vec2(0.0, t*0.6)).g;
      base = mix(c2, c, 0.4 + 0.6*k) + vec3(0.9, 1.0, 0.6)*smoothstep(0.7, 0.85, k)*0.8;
    } else if(st > 9.5){
      float e = smoothstep(r - 1.3*px - 0.7, r - 0.1, d);
      vec3 b2 = mix(c*1.35, vec3(1.0), smoothstep(0.55, 0.0, d/r));
      o.rgb = o.rgb*(1.0 - cov) + mix(b2, c2, e)*cov; o.a = cov;
      return o;
    }
    vec3 R = reflect(-Ld, n);
    float spec = pow(max(R.z, 0.0), 22.0);
    float rim = pow(1.0 - n.z, 2.2);
    vec3 body = base*(0.55 + 0.65*dif) + vec3(spec)*0.85 + c*rim*0.7;
    if(evo > 0.5) body += vec3(1.0)*smoothstep(r - 1.4*px - 0.6, r - 0.15, shape)*0.7;
    body *= 1.12;
    body = mix(body, c2*0.22, smoothstep(r - 1.1*px - 0.35, r, shape)*0.75);   /* liseré sombre : la bille se detache du sol clair */
    float al = cov*alpha;
    o.rgb = o.rgb*(1.0 - al) + body*al;
    o.a = al;
    return o;
  }
  void main(){
    if(vG.x < vD.x || vG.y < vD.y || vG.x >= vD.z || vG.y >= vD.w) discard;
    float tp = vB.w; float px = uCam.w/vS;
    vec3 c = vC.rgb; float a = vC.a;
    vec4 o = vec4(0.0);
    if(tp < 0.5){                                 /* lueur ; poussiere (p>0) : visible seulement dans la lumiere */
      vec2 v = vG - vA.xy; float q = dot(v, v)/(vB.x*vB.x);
      if(q >= 1.0) discard;
      float f = 1.0 - q; f *= f;
      if(vB.y > 0.5){
        vec3 li = uAmbL.rgb*0.35 + texture2D(uLight, vec2(vA.x/640.0, 1.0 - vA.y/360.0)).rgb*uAmbL.w;
        c *= clamp(li*1.4, 0.0, 2.0);
      }
      o = vec4(c*f*a, 0.0);
    } else if(tp < 1.5){                          /* disque doux */
      float d = length(vG - vA.xy);
      float f = clamp((vB.x - d)/px + 0.5, 0.0, 1.0);
      float core = 0.75 + 0.6*(1.0 - clamp(d/max(vB.x, 0.3), 0.0, 1.0));
      f = max(f, clamp(1.0 - (d - vB.x)/(vB.x*0.8 + 0.6), 0.0, 1.0)*0.25);
      o = vec4(c*a*f*core, 0.0);
    } else if(tp < 2.5){                          /* anneau */
      float d = length(vG - vA.xy);
      float w = max(vB.y*0.5, px*0.75);
      float e = abs(d - vB.x);
      float f = clamp((w - e)/px + 0.5, 0.0, 1.0);
      f = max(f, (1.0 - clamp(e/(w*3.0 + 1.0), 0.0, 1.0))*0.3);
      o = vec4(c*a*f, 0.0);
    } else if(tp < 3.5){                          /* capsule (trainees, eclairs) */
      vec2 pa = vG - vA.xy, ba = vA.zw - vA.xy;
      float h = clamp(dot(pa, ba)/max(dot(ba, ba), 0.0001), 0.0, 1.0);
      float d = length(pa - ba*h); float r = vB.x;
      float f = clamp(1.0 - d/(r + 0.6), 0.0, 1.0); f = f*f*(3.0 - 2.0*f);
      float core = clamp((r*0.45 - d)/px + 0.5, 0.0, 1.0);
      o = vec4(c*a*(f*0.85 + core*0.6), 0.0);
    } else if(tp < 4.5){                          /* bille */
      o = ball(vG - vA.xy, vB.x, vB.y, vB.z, c, vE.rgb, vE.a, px);
    } else if(tp < 5.5){                          /* fumee (alpha) */
      float d = length(vG - vA.xy)/vB.x;
      if(d >= 1.0) discard;
      float n = texture2D(uNoise, vG*0.045 + vec2(uTime*0.03, -uTime*0.05)).r;
      float f = 1.0 - d*d; f = f*f*(0.45 + 1.0*n);
      float al = clamp(f*a, 0.0, 1.0);
      o = vec4(c*al, al);
    } else if(tp < 6.5){                          /* rayon de lumiere */
      vec2 ba = vA.zw - vA.xy; float len = max(length(ba), 0.001); vec2 dir = ba/len; vec2 nr = vec2(-dir.y, dir.x);
      vec2 pa = vG - vA.xy; float al = dot(pa, dir)/len; float ac = dot(pa, nr)/(vB.x*0.5);
      if(al < 0.0 || al > 1.0 || abs(ac) > 1.0) discard;
      float f = cos(ac*1.5708); f *= f;
      f *= smoothstep(0.0, 0.1, al)*(1.0 - al)*(1.0 - al);
      float dust = texture2D(uNoise, vG*0.025 + vec2(uTime*0.004, -uTime*0.012)).r;
      f *= 0.55 + 0.9*dust;
      o = vec4(c*a*f, 0.0);
    } else if(tp < 7.5){                          /* etoile scintillante */
      vec2 v = vG - vA.xy; float r = vB.x; float w = px*1.2 + 0.3;
      float lx = max(0.0, 1.0 - abs(v.x)/r)*max(0.0, 1.0 - abs(v.y)/w);
      float ly = max(0.0, 1.0 - abs(v.y)/r)*max(0.0, 1.0 - abs(v.x)/w);
      float g = max(0.0, 1.0 - length(v)/(r*0.5)); g *= g;
      o = vec4(c*a*(lx + ly + g*1.2), 0.0);
    } else {                                      /* faisceau vertical */
      float d = abs(vG.x - vA.x); float r = vB.x;
      float core = clamp((r*0.35 - d)/px + 0.5, 0.0, 1.0);
      float body = clamp(1.0 - d/(r + 0.5), 0.0, 1.0); body *= body;
      float halo = clamp(1.0 - d/(r*2.6 + 1.0), 0.0, 1.0); halo *= halo;
      float ends = smoothstep(vA.y, vA.y + 3.0, vG.y)*smoothstep(vA.w, vA.w - 3.0, vG.y);
      float flick = 0.85 + 0.15*texture2D(uNoise, vec2(vG.y*0.02 - uTime*1.7, vA.x*0.01)).r;
      o = vec4((c*(body*0.9 + halo*0.35) + vec3(1.0)*core*0.8)*a*ends*flick, 0.0);
    }
    gl_FragColor = o;
  }
);

/* ---- bloom / flou : reduction 13 points (avec seuil doux et moyenne de Karis), agrandissement en tente ---- */
static const char *FS_DOWN = GLSL(
  uniform sampler2D uSrc; uniform vec2 uTexel; uniform vec4 uP;   /* mode, seuil, genou, plafond */
  varying vec2 vUV;
  vec3 S(vec2 o){ return min(texture2D(uSrc, vUV + o*uTexel).rgb, vec3(uP.w)); }
  float lum(vec3 c){ return dot(c, vec3(0.2126, 0.7152, 0.0722)); }
  void main(){
    vec3 a = S(vec2(-2.0,-2.0)), b = S(vec2(0.0,-2.0)), c = S(vec2(2.0,-2.0));
    vec3 d = S(vec2(-1.0,-1.0)), e = S(vec2(1.0,-1.0));
    vec3 f = S(vec2(-2.0,0.0)), g = S(vec2(0.0,0.0)), h = S(vec2(2.0,0.0));
    vec3 i = S(vec2(-1.0,1.0)), j = S(vec2(1.0,1.0));
    vec3 k = S(vec2(-2.0,2.0)), l = S(vec2(0.0,2.0)), m = S(vec2(2.0,2.0));
    vec3 r;
    if(uP.x > 0.5){
      vec3 g0 = (d+e+i+j)*0.25, g1 = (a+b+f+g)*0.25, g2 = (b+c+g+h)*0.25, g3 = (f+g+k+l)*0.25, g4 = (g+h+l+m)*0.25;
      float w0 = 0.5/(1.0+lum(g0)), w1 = 0.125/(1.0+lum(g1)), w2 = 0.125/(1.0+lum(g2)), w3 = 0.125/(1.0+lum(g3)), w4 = 0.125/(1.0+lum(g4));
      r = (g0*w0 + g1*w1 + g2*w2 + g3*w3 + g4*w4)/(w0 + w1 + w2 + w3 + w4);
      float br = max(r.r, max(r.g, r.b));
      float soft = clamp(br - uP.y + uP.z, 0.0, 2.0*uP.z);
      soft = soft*soft/(4.0*uP.z + 0.0001);
      r *= max(soft, br - uP.y)/max(br, 0.0001);
    } else {
      r = (d+e+i+j)*0.125 + (a+c+k+m)*0.03125 + (b+f+h+l)*0.0625 + g*0.125;
    }
    gl_FragColor = vec4(r, 1.0);
  }
);
static const char *FS_UP = GLSL(
  uniform sampler2D uSrc; uniform vec2 uTexel; uniform float uW; varying vec2 vUV;
  void main(){
    vec4 d = uTexel.xyxy*vec4(1.0, 1.0, -1.0, 0.0);
    vec3 s = texture2D(uSrc, vUV - d.xy).rgb;
    s += texture2D(uSrc, vUV - d.wy).rgb*2.0;
    s += texture2D(uSrc, vUV - d.zy).rgb;
    s += texture2D(uSrc, vUV + d.zw).rgb*2.0;
    s += texture2D(uSrc, vUV).rgb*4.0;
    s += texture2D(uSrc, vUV + d.xw).rgb*2.0;
    s += texture2D(uSrc, vUV + d.zy).rgb;
    s += texture2D(uSrc, vUV + d.wy).rgb*2.0;
    s += texture2D(uSrc, vUV + d.xy).rgb;
    gl_FragColor = vec4(s*(uW/16.0), 1.0);
  }
);

/* ---- image finale : aberration, profondeur de champ, bloom, exposition, courbe, etalonnage, vignette, grain, interface ---- */
static const char *FS_FINAL = GLSL(
  uniform sampler2D uHDR; uniform sampler2D uBloom; uniform sampler2D uBlur; uniform sampler2D uUI; uniform sampler2D uNoise;
  uniform vec2 uGame;
  uniform vec4 uP0;    /* bloom, exposition, saturation, contraste */
  uniform vec4 uP1;    /* vignette, aberration, grain, temps */
  uniform vec4 uP2;    /* dof y0, dof y1, dof, flou total */
  uniform vec4 uP3;    /* faiblesse, desaturation, teinte, pixels du jeu par pixel (interface) */
  uniform vec4 uFlash; uniform vec3 uSh; uniform vec3 uHi; uniform vec4 uRect;
  uniform vec4 uOn;    /* bloom actif, flou actif */
  uniform sampler2D uIcoA; uniform sampler2D uIcoM; uniform sampler2D uIcoI; uniform float uIcoOn;
  varying vec2 vUV;
  vec3 tm(vec3 x){
    vec3 hi = 0.82 + 0.18*(1.0 - exp(-(x - 0.82)/0.18));
    return mix(x, hi, step(0.82, x));
  }
  vec4 sharp4(sampler2D t, vec2 g, float w){
    vec2 p = g - 0.5; vec2 c = floor(p); vec2 f = p - c;
    f = clamp((f - 0.5)/w + 0.5, 0.0, 1.0);
    return texture2D(t, (c + 0.5 + f)/uGame);
  }
  void main(){
    vec2 uv = vUV;
    vec2 dc = uv - 0.5;
    float ca = uP1.y*(0.35 + dot(dc, dc)*4.0);
    vec3 col;
    if(ca > 0.00001){
      col.r = texture2D(uHDR, uv - dc*ca).r;
      col.g = texture2D(uHDR, uv).g;
      col.b = texture2D(uHDR, uv + dc*ca).b;
    } else col = texture2D(uHDR, uv).rgb;
    vec2 s = vec2(uv.x*uGame.x, (1.0 - uv.y)*uGame.y);
    if(uOn.y > 0.5){
      float m = uP2.z*clamp((uP2.y - s.y)/max(uP2.y - uP2.x, 1.0), 0.0, 1.0);
      m = max(m, uP2.w);
      col = mix(col, texture2D(uBlur, uv).rgb, m);
    }
    if(uOn.x > 0.5) col += texture2D(uBloom, uv).rgb*uP0.x;
    col += uFlash.rgb*uFlash.a;
    col *= uP0.y;
    col = tm(max(col, vec3(0.0)));
    col = (col - 0.5)*uP0.w + 0.5;
    float l = dot(col, vec3(0.299, 0.587, 0.114));
    col = mix(vec3(l), col, uP0.z*(1.0 - uP3.y));
    col += uSh*(1.0 - clamp(l, 0.0, 1.0))*uP3.z;
    col *= mix(vec3(1.0), uHi, smoothstep(0.35, 1.0, l));
    vec2 rc = (uRect.xy + uRect.zw)*0.5, rh = max((uRect.zw - uRect.xy)*0.5, vec2(1.0));
    vec2 q = (s - rc)/rh;
    col *= max(1.0 - uP1.x*(q.x*q.x*0.55 + q.y*q.y*0.75), 0.0);
    if(uP3.x > 0.0){
      float e = min(min(s.x - uRect.x, uRect.z - s.x), min(s.y - uRect.y, uRect.w - s.y)*1.6);
      col += vec3(1.0, 0.07, 0.05)*exp(-max(e, 0.0)/9.0)*uP3.x;
    }
    vec4 nz = texture2D(uNoise, gl_FragCoord.xy/256.0 + vec2(fract(uP1.w*7.31), fract(uP1.w*3.17)));
    col += (nz.a - 0.5)*uP1.z*(0.6 + 0.4*(1.0 - l));
    vec4 ui = sharp4(uUI, s, uP3.w);
    if(uIcoOn > 0.5){   /* icones des billes et objets a la resolution de l'ecran */
      vec4 im = texture2D(uIcoM, (floor(s) + 0.5)/uGame);
      float id = floor(im.r*255.0 + 0.5);
      float ix = (id + 0.5)/256.0;
      vec4 d0 = texture2D(uIcoI, vec2(ix, 0.125));
      vec4 d1 = texture2D(uIcoI, vec2(ix, 0.375));
      vec2 o = vec2(d0.r*65280.0 + d0.g*255.0, d0.b*65280.0 + d0.a*255.0)/16.0 - 512.0;
      float sz = (d1.r*65280.0 + d1.g*255.0)/16.0;
      float cell = floor(d1.b*255.0 + 0.5);
      vec2 l = clamp((s - o)/max(sz, 0.01), 0.0, 1.0);
      vec2 auv = (vec2(mod(cell, 8.0), floor(cell/8.0)) + l)*vec2(0.125, 1.0/11.0);   /* atlas 8 x 11 */
      vec4 ic = texture2D(uIcoA, auv, -0.35);
      if(im.a > 0.0){
        vec4 d2 = texture2D(uIcoI, vec2(ix, 0.625));
        vec4 d3 = texture2D(uIcoI, vec2(ix, 0.875));
        ic.rgb = mix(ic.rgb, d2.rgb*ic.a, d2.a);
        ic.rgb *= d3.rgb;
        ic *= d1.a;
        float k = im.a;
        ui.rgb = ic.rgb*k + ui.rgb*(1.0 - ic.a);
        ui.a = ic.a*k + ui.a*(1.0 - ic.a*k);
      }
    }
    col = ui.rgb + clamp(col, 0.0, 1.0)*(1.0 - ui.a);
    if(uOn.z > 0.0){   /* lignes de balayage : au moins 3 pixels d'ecran par ligne */
      float n = max(1.0, floor(1.0/(uP3.w*3.0)));
      col *= 1.0 - uOn.z*(0.5 - 0.5*cos(6.2832*s.y*n));
    }
    col += (fract(nz.a*7.13 + nz.r*3.71) - 0.5)/255.0;
    gl_FragColor = vec4(col, 1.0);
  }
);

/* ======================================================================= */
/*                             OBJETS OPENGL                               */
/* ======================================================================= */
typedef struct { GLuint fb, tex; int w, h; } RT;
static RT rt_light, rt_hdr, rt_bl[7], rt_dof[3];
static int n_bl=0;
static GLuint tx_scene, tx_emis, tx_ui, tx_nrm, tx_noise, tx_occ;
static GLuint tx_icoA, tx_icoM, tx_icoI;          /* icones : atlas, masque, donnees */
static int ico_ver=0, ico_frame=-1, ico_n=0;
static uint8_t ico_mask[SCREEN_W*SCREEN_H*2], ico_idata[256*4*4];
static GLuint vbo_full, vbo_dyn;
static GLint fmt_light=GL_RGBA16F, fmt_hdr=GL_RGBA16F;
static float lscale=1.0f;
static int cur_vw=-1, cur_vh=-1, cur_q=-1, IW=0, IH=0;
static int max_tex=4096;

enum { A_POS=0, A_1, A_2, A_3, A_4, A_5 };
static GLuint p_light, p_scene, p_fx, p_down, p_up, p_final;
static struct { GLint nrm, occ, game, lsc, lp, scn, hdN, hdM, hdP, hdG, hdS; } ul;
static struct { GLint scene, emis, light, noise, game, cam, ctr, view, lit, amb, shock, misc, fog, fog2, p25, hdA, hdL, hdM, hdP, hdG, hdS, sky; } us;
static GLuint tx_hdA, tx_hdN, tx_hdL; static int hd_ver=0;
static struct { GLint noise, light, ambl, game, cam, ctr, time, p25; } uf;
static struct { GLint src, texel, p; } ud;
static struct { GLint src, texel, w; } uu;
static struct { GLint hdr, bloom, blur, ui, noise, game, p0, p1, p2, p3, flash, sh, hi, rect, on, icoA, icoM, icoI, icoOn; } un;

/* OpenGL ES 3 (iPhone) : les shaders GLSL 1.20 sont traduits a la volee en GLSL ES 3.00 */
static bool G_es=false;
static bool idch(char c){ return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'; }
static char *es_translate(const char *src,bool frag){
  size_t n=strlen(src); char *o=(char*)malloc(n*2+512); if(!o) return NULL; size_t k=0;
  const char *hdr = frag ? "#version 300 es\nprecision highp float;\nprecision highp int;\nprecision highp sampler2D;\nout vec4 oFragColor;\n"
                         : "#version 300 es\nprecision highp float;\nprecision highp int;\nprecision highp sampler2D;\n";
  const char *p=src; if(!strncmp(p,"#version 120\n",13)) p+=13;
  k=(size_t)sprintf(o,"%s",hdr);
  static const char *const FROM[]={"attribute","varying","texture2D","gl_FragColor"};
  while(*p){
    if(idch(*p) && (p==src || !idch(p[-1]))){
      const char *e=p; while(idch(*e)) e++; size_t l=(size_t)(e-p); const char *rep=NULL;
      for(int i=0;i<4;i++) if(strlen(FROM[i])==l && !strncmp(p,FROM[i],l)){
        rep = i==0 ? "in" : i==1 ? (frag?"in":"out") : i==2 ? "texture" : "oFragColor"; }
      if(rep){ size_t rl=strlen(rep); memcpy(o+k,rep,rl); k+=rl; } else { memcpy(o+k,p,l); k+=l; }
      p=e; continue;
    }
    o[k++]=*p++;
  }
  o[k]=0; return o;
}
static GLuint sh_compile(GLenum type,const char *src0,const char *name){
  char *tr = G_es ? es_translate(src0,type==GL_FRAGMENT_SHADER) : NULL;
  const char *src = tr ? tr : src0;
  GLuint s=gl.CreateShader(type); gl.ShaderSource(s,1,&src,NULL); gl.CompileShader(s); free(tr);
  GLint ok=0; gl.GetShaderiv(s,GL_COMPILE_STATUS,&ok);
  if(!ok){ char b[2048]; b[0]=0; gl.GetShaderInfoLog(s,sizeof b,NULL,b); logf_("shader %s : %s\n",name,b); gl.DeleteShader(s); return 0; }
  return s;
}
static GLuint prog_make(const char *vs,const char *fs,const char *const *attrs,int na,const char *name){
  GLuint v=sh_compile(GL_VERTEX_SHADER,vs,name); if(!v) return 0;
  GLuint f=sh_compile(GL_FRAGMENT_SHADER,fs,name); if(!f){ gl.DeleteShader(v); return 0; }
  GLuint p=gl.CreateProgram(); gl.AttachShader(p,v); gl.AttachShader(p,f);
  for(int i=0;i<na;i++) gl.BindAttribLocation(p,(GLuint)i,attrs[i]);
  gl.LinkProgram(p);
  GLint ok=0; gl.GetProgramiv(p,GL_LINK_STATUS,&ok);
  gl.DeleteShader(v); gl.DeleteShader(f);
  if(!ok){ char b[2048]; b[0]=0; gl.GetProgramInfoLog(p,sizeof b,NULL,b); logf_("programme %s : %s\n",name,b); gl.DeleteProgram(p); return 0; }
  return p;
}

static GLuint tex_make(int w,int h,GLint ifmt,GLint filter,GLint wrap,const void *data){
  GLuint t=0; gl.GenTextures(1,&t); gl.BindTexture(GL_TEXTURE_2D,t);
  gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,filter); gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,filter);
  gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,wrap); gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,wrap);
  if(G_es){ /* pas de BGRA garanti en ES : on echange rouge et bleu a la lecture */
    gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_SWIZZLE_R,GL_BLUE); gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_SWIZZLE_B,GL_RED);
    gl.TexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,data);
  } else gl.TexImage2D(GL_TEXTURE_2D,0,ifmt,w,h,0,GL_BGRA,GL_UNSIGNED_BYTE,data);
  return t;
}
static void rt_free(RT *r){ if(r->fb) gl.DeleteFramebuffers(1,&r->fb); if(r->tex) gl.DeleteTextures(1,&r->tex); r->fb=r->tex=0; r->w=r->h=0; }
static bool rt_try(RT *r,int w,int h,GLint fmt){
  while(gl.GetError()!=GL_NO_ERROR){}
  gl.GenTextures(1,&r->tex); gl.BindTexture(GL_TEXTURE_2D,r->tex);
  gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR); gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
  gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE); gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  gl.TexImage2D(GL_TEXTURE_2D,0,fmt,w,h,0,(G_es&&fmt==GL_R11F_G11F_B10F)?GL_RGB:GL_RGBA,fmt==GL_RGBA8?GL_UNSIGNED_BYTE:GL_FLOAT,NULL);
  if(gl.GetError()!=GL_NO_ERROR){ gl.DeleteTextures(1,&r->tex); r->tex=0; return false; }
  gl.GenFramebuffers(1,&r->fb); gl.BindFramebuffer(GL_FRAMEBUFFER,r->fb);
  gl.FramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,r->tex,0);
  bool ok = gl.CheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE && gl.GetError()==GL_NO_ERROR;
  gl.BindFramebuffer(GL_FRAMEBUFFER,0);
  if(!ok){ rt_free(r); return false; }
  r->w=w; r->h=h;
  gl.BindFramebuffer(GL_FRAMEBUFFER,r->fb); gl.Viewport(0,0,w,h); gl.ClearColor(0,0,0,0); gl.Clear(GL_COLOR_BUFFER_BIT); gl.BindFramebuffer(GL_FRAMEBUFFER,0);
  return true;
}
static bool rt_make(RT *r,int w,int h,const GLint *fmts,int nf,GLint *chosen){
  rt_free(r);
  if(w<1) w=1; if(h<1) h=1;
  for(int i=0;i<nf;i++) if(rt_try(r,w,h,fmts[i])){ if(chosen) *chosen=fmts[i]; return true; }
  return false;
}

/* ---- texture de bruit raccordable (brouillard, fumee, poussiere, grain) ---- */
static uint32_t nh(int x,int y,int s){ uint32_t h=(uint32_t)x*374761393u+(uint32_t)y*668265263u+(uint32_t)s*2246822519u; h=(h^(h>>13))*1274126177u; return h^(h>>16); }
static float tn(float x,float y,int per,int seed){
  int ix=(int)floorf(x), iy=(int)floorf(y); float fx=x-ix, fy=y-iy; fx=fx*fx*(3-2*fx); fy=fy*fy*(3-2*fy);
  int x0=((ix%per)+per)%per, y0=((iy%per)+per)%per, x1=(x0+1)%per, y1=(y0+1)%per;
  float a=(nh(x0,y0,seed)&0xFFFF)/65535.0f, b=(nh(x1,y0,seed)&0xFFFF)/65535.0f, c=(nh(x0,y1,seed)&0xFFFF)/65535.0f, d=(nh(x1,y1,seed)&0xFFFF)/65535.0f;
  return lerpf(lerpf(a,b,fx),lerpf(c,d,fx),fy);
}
static float tfbm(float u,float v,int per,int seed){ float s=0,a=0.5f,n=0; for(int o=0;o<4;o++){ s+=a*tn(u*per,v*per,per,seed+o*31); n+=a; a*=0.5f; per*=2; } return s/n; }
static GLuint make_noise(void){
  enum { N=256 };
  uint32_t *px=(uint32_t*)malloc(N*N*4);
  for(int y=0;y<N;y++) for(int x=0;x<N;x++){
    float u=(float)x/N, v=(float)y/N;
    float r=clampf((tfbm(u,v,4,11)-0.5f)*1.9f+0.5f,0,1);
    float g=clampf((tfbm(u,v,8,57)-0.5f)*1.9f+0.5f,0,1);
    float b=tn(u*32,v*32,32,91); b=clampf((b-0.35f)*1.6f,0,1);
    float a=(nh(x,y,7)&0xFF)/255.0f;
    px[y*N+x]=((uint32_t)(a*255)<<24)|((uint32_t)(r*255)<<16)|((uint32_t)(g*255)<<8)|(uint32_t)(b*255);
  }
  GLuint t=tex_make(N,N,GL_RGBA8,GL_LINEAR,GL_REPEAT,px);
  free(px); return t;
}

/* ======================================================================= */
/*                              INITIALISATION                             */
/* ======================================================================= */
static GpuGetProc G_gp;
static void *gp1(const char *n){ return G_gp(n); }
static bool load(void *dst,const char *n,const char *alt){
  void *p=gp1(n); if(!p && alt) p=gp1(alt);
  memcpy(dst,&p,sizeof p);
  if(!p){ logf_("fonction OpenGL absente : %s%s\n",n,""); return false; }
  return true;
}
bool gpu_init(GpuGetProc gp,char *log,int logn){
  G_log=log; G_logn=logn; if(log && logn>0) log[0]=0;
  G_gp=gp; G_ok=false; G_failed=false; G_frames=0;
  bool ok=true;
#define LD(f,n) ok &= load(&gl.f,n,NULL)
#define LX(f,n,alt) ok &= load(&gl.f,n,alt)
  LD(GetString,"glGetString"); LD(GetError,"glGetError"); LD(Viewport,"glViewport"); LD(ClearColor,"glClearColor"); LD(Clear,"glClear");
  LD(Enable,"glEnable"); LD(Disable,"glDisable"); LD(BlendFunc,"glBlendFunc"); LD(GenTextures,"glGenTextures"); LD(DeleteTextures,"glDeleteTextures");
  LD(BindTexture,"glBindTexture"); LD(TexParameteri,"glTexParameteri"); LD(TexImage2D,"glTexImage2D"); LD(TexSubImage2D,"glTexSubImage2D");
  LD(PixelStorei,"glPixelStorei"); LD(ReadPixels,"glReadPixels"); LD(DrawArrays,"glDrawArrays"); LD(GetIntegerv,"glGetIntegerv"); LD(Finish,"glFinish");
  LX(ActiveTexture,"glActiveTexture","glActiveTextureARB");
  LX(GenBuffers,"glGenBuffers","glGenBuffersARB"); LX(DeleteBuffers,"glDeleteBuffers","glDeleteBuffersARB"); LX(BindBuffer,"glBindBuffer","glBindBufferARB"); LX(BufferData,"glBufferData","glBufferDataARB");
  LD(CreateShader,"glCreateShader"); LD(ShaderSource,"glShaderSource"); LD(CompileShader,"glCompileShader"); LD(GetShaderiv,"glGetShaderiv");
  LD(GetShaderInfoLog,"glGetShaderInfoLog"); LD(DeleteShader,"glDeleteShader"); LD(CreateProgram,"glCreateProgram"); LD(AttachShader,"glAttachShader");
  LD(BindAttribLocation,"glBindAttribLocation"); LD(LinkProgram,"glLinkProgram"); LD(GetProgramiv,"glGetProgramiv"); LD(GetProgramInfoLog,"glGetProgramInfoLog");
  LD(DeleteProgram,"glDeleteProgram"); LD(UseProgram,"glUseProgram"); LD(GetUniformLocation,"glGetUniformLocation");
  LD(Uniform1i,"glUniform1i"); LD(Uniform1f,"glUniform1f"); LD(Uniform2f,"glUniform2f"); LD(Uniform3f,"glUniform3f"); LD(Uniform4f,"glUniform4f"); LD(Uniform4fv,"glUniform4fv");
  LD(EnableVertexAttribArray,"glEnableVertexAttribArray"); LD(DisableVertexAttribArray,"glDisableVertexAttribArray"); LD(VertexAttribPointer,"glVertexAttribPointer");
  LX(GenFramebuffers,"glGenFramebuffers","glGenFramebuffersEXT"); LX(DeleteFramebuffers,"glDeleteFramebuffers","glDeleteFramebuffersEXT");
  LX(BindFramebuffer,"glBindFramebuffer","glBindFramebufferEXT"); LX(FramebufferTexture2D,"glFramebufferTexture2D","glFramebufferTexture2DEXT");
  LX(CheckFramebufferStatus,"glCheckFramebufferStatus","glCheckFramebufferStatusEXT");
#undef LD
#undef LX
  if(!ok){ logf_("OpenGL 2.1 + FBO indisponible : rendu logiciel.\n%s%s","",""); return false; }
  const char *ven=(const char*)gl.GetString(GL_VENDOR), *ren=(const char*)gl.GetString(GL_RENDERER), *ver=(const char*)gl.GetString(GL_VERSION), *sl=(const char*)gl.GetString(GL_SHADING_LANGUAGE_VERSION);
  snprintf(G_name,sizeof G_name,"%s | %s",ren?ren:"?",ver?ver:"?");
  logf_("Carte graphique : %s (%s)\n",ren,ven); logf_("OpenGL %s | GLSL %s\n",ver,sl);
  G_es = ver && !strncmp(ver,"OpenGL ES",9);
  if(G_es && ver[10]<'3'){ logf_("OpenGL ES 3 requis : rendu logiciel.\n%s%s","",""); return false; }
  if(!ver || !sl || (!G_es && ver[0]<'2')){ logf_("OpenGL trop ancien (2.1 requis) : rendu logiciel.\n%s%s","",""); return false; }
  while(gl.GetError()!=GL_NO_ERROR){}
  gl.GetIntegerv(GL_MAX_TEXTURE_SIZE,&max_tex); if(max_tex<2048) max_tex=2048;

  static const char *const AT_FULL[]={"aPos"};
  static const char *const AT_LIGHT[]={"aPos","aL0","aL1","aL2"};
  static const char *const AT_FX[]={"aPos","aA","aB","aC","aE","aD"};
  p_light=prog_make(VS_LIGHT,FS_LIGHT,AT_LIGHT,4,"lumieres");
  p_scene=prog_make(VS_FULL,FS_SCENE,AT_FULL,1,"scene");
  p_fx=prog_make(VS_FX,FS_FX,AT_FX,6,"effets");
  p_down=prog_make(VS_FULL,FS_DOWN,AT_FULL,1,"reduction");
  p_up=prog_make(VS_FULL,FS_UP,AT_FULL,1,"agrandissement");
  p_final=prog_make(VS_FULL,FS_FINAL,AT_FULL,1,"final");
  if(!p_light||!p_scene||!p_fx||!p_down||!p_up||!p_final){ logf_("Echec des shaders : rendu logiciel.\n%s%s","",""); return false; }
#define UL(p,n) gl.GetUniformLocation(p,n)
  ul.nrm=UL(p_light,"uNrm"); ul.occ=UL(p_light,"uOcc"); ul.game=UL(p_light,"uGame"); ul.lsc=UL(p_light,"uLScale"); ul.lp=UL(p_light,"uLP");
  ul.scn=UL(p_light,"uScn"); ul.hdN=UL(p_light,"uHdN"); ul.hdM=UL(p_light,"uHdM"); ul.hdP=UL(p_light,"uHdP"); ul.hdG=UL(p_light,"uHdG"); ul.hdS=UL(p_light,"uHdS");
  us.hdA=UL(p_scene,"uHdA"); us.hdL=UL(p_scene,"uHdL"); us.hdM=UL(p_scene,"uHdM"); us.hdP=UL(p_scene,"uHdP"); us.hdG=UL(p_scene,"uHdG"); us.hdS=UL(p_scene,"uHdS"); us.sky=UL(p_scene,"uHdSky");
  us.scene=UL(p_scene,"uScene"); us.emis=UL(p_scene,"uEmis"); us.light=UL(p_scene,"uLight"); us.noise=UL(p_scene,"uNoise");
  us.game=UL(p_scene,"uGame"); us.cam=UL(p_scene,"uCam"); us.ctr=UL(p_scene,"uCtr"); us.view=UL(p_scene,"uView"); us.lit=UL(p_scene,"uLit");
  us.amb=UL(p_scene,"uAmb"); us.shock=UL(p_scene,"uShock"); us.misc=UL(p_scene,"uMisc"); us.fog=UL(p_scene,"uFog"); us.fog2=UL(p_scene,"uFog2"); us.p25=UL(p_scene,"uP25");
  uf.noise=UL(p_fx,"uNoise"); uf.light=UL(p_fx,"uLight"); uf.ambl=UL(p_fx,"uAmbL"); uf.game=UL(p_fx,"uGame"); uf.cam=UL(p_fx,"uCam"); uf.ctr=UL(p_fx,"uCtr"); uf.time=UL(p_fx,"uTime"); uf.p25=UL(p_fx,"uP25");
  ud.src=UL(p_down,"uSrc"); ud.texel=UL(p_down,"uTexel"); ud.p=UL(p_down,"uP");
  uu.src=UL(p_up,"uSrc"); uu.texel=UL(p_up,"uTexel"); uu.w=UL(p_up,"uW");
  un.hdr=UL(p_final,"uHDR"); un.bloom=UL(p_final,"uBloom"); un.blur=UL(p_final,"uBlur"); un.ui=UL(p_final,"uUI"); un.noise=UL(p_final,"uNoise");
  un.game=UL(p_final,"uGame"); un.p0=UL(p_final,"uP0"); un.p1=UL(p_final,"uP1"); un.p2=UL(p_final,"uP2"); un.p3=UL(p_final,"uP3");
  un.flash=UL(p_final,"uFlash"); un.sh=UL(p_final,"uSh"); un.hi=UL(p_final,"uHi"); un.rect=UL(p_final,"uRect"); un.on=UL(p_final,"uOn");
  un.icoA=UL(p_final,"uIcoA"); un.icoM=UL(p_final,"uIcoM"); un.icoI=UL(p_final,"uIcoI"); un.icoOn=UL(p_final,"uIcoOn");
#undef UL
  gl.PixelStorei(GL_UNPACK_ALIGNMENT,4); gl.PixelStorei(GL_PACK_ALIGNMENT,4);
  tx_scene=tex_make(SCREEN_W,SCREEN_H,GL_RGBA8,GL_LINEAR,GL_CLAMP_TO_EDGE,NULL);
  tx_emis=tex_make(SCREEN_W,SCREEN_H,GL_RGBA8,GL_LINEAR,GL_CLAMP_TO_EDGE,NULL);
  tx_ui=tex_make(SCREEN_W,SCREEN_H,GL_RGBA8,GL_LINEAR,GL_CLAMP_TO_EDGE,NULL);
  tx_nrm=tex_make(SCREEN_W,SCREEN_H,GL_RGBA8,GL_NEAREST,GL_CLAMP_TO_EDGE,NULL);
  tx_noise=make_noise();
  /* icones : masque (numero, voile) et donnees par icone ; l'atlas arrive quand il est pret */
  { gl.GenTextures(1,&tx_icoM); gl.BindTexture(GL_TEXTURE_2D,tx_icoM);
    gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST); gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE); gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    gl.TexImage2D(GL_TEXTURE_2D,0,G_es?GL_LUMINANCE_ALPHA:GL_LUMINANCE8_ALPHA8,SCREEN_W,SCREEN_H,0,GL_LUMINANCE_ALPHA,GL_UNSIGNED_BYTE,NULL);
    gl.GenTextures(1,&tx_icoI); gl.BindTexture(GL_TEXTURE_2D,tx_icoI);
    gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST); gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE); gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    gl.TexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,256,4,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
    if(gl.GetError()!=GL_NO_ERROR){ tx_icoM=tx_icoI=0; logf_("Icones HD indisponibles.\n%s%s","",""); } }
  { GLuint t=0; gl.GenTextures(1,&t); gl.BindTexture(GL_TEXTURE_2D,t);
    gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST); gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE); gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    gl.PixelStorei(GL_UNPACK_ALIGNMENT,1);
    gl.TexImage2D(GL_TEXTURE_2D,0,G_es?GL_LUMINANCE:GL_LUMINANCE8,SCREEN_W,SCREEN_H,0,GL_LUMINANCE,GL_UNSIGNED_BYTE,NULL);
    gl.PixelStorei(GL_UNPACK_ALIGNMENT,4); tx_occ=t; }
  static const float FULL[6]={-1,-1, 3,-1, -1,3};
  gl.GenBuffers(1,&vbo_full); gl.BindBuffer(GL_ARRAY_BUFFER,vbo_full); gl.BufferData(GL_ARRAY_BUFFER,sizeof FULL,FULL,GL_STATIC_DRAW);
  gl.GenBuffers(1,&vbo_dyn); gl.BindBuffer(GL_ARRAY_BUFFER,0);
  if(gl.GetError()!=GL_NO_ERROR){ logf_("Erreur OpenGL a l'initialisation : rendu logiciel.\n%s%s","",""); return false; }
  /* formats de rendu : flottants si possible */
  static const GLint F_LIGHT[]={GL_RGBA16F,GL_RGBA8};
  static const GLint F_HDR[]={GL_R11F_G11F_B10F,GL_RGBA16F,GL_RGBA8};
  RT tst={0,0,0,0};
  if(!rt_make(&tst,64,64,F_LIGHT,2,&fmt_light)){ logf_("Aucun format de rendu utilisable : rendu logiciel.\n%s%s","",""); return false; }
  rt_free(&tst);
  if(!rt_make(&tst,64,64,F_HDR,3,&fmt_hdr)){ logf_("Aucun format HDR utilisable : rendu logiciel.\n%s%s","",""); return false; }
  rt_free(&tst);
  lscale = fmt_light==GL_RGBA8 ? 0.25f : 1.0f;
  logf_("Formats : lumiere %s, image %s\n",fmt_light==GL_RGBA16F?"RGBA16F":"RGBA8",fmt_hdr==GL_R11F_G11F_B10F?"R11G11B10F":(fmt_hdr==GL_RGBA16F?"RGBA16F":"RGBA8"));
  cur_vw=cur_vh=-1;
  G_ok=true;
  return true;
}
static int ico_ver_pub(void){ return (tx_icoA && tx_icoM && tx_icoI && un.icoOn>=0) ? ico_ver : 0; }
void gpu_shutdown(void){
  if(!G_ok) return;
  rt_free(&rt_light); rt_free(&rt_hdr); for(int i=0;i<7;i++) rt_free(&rt_bl[i]); for(int i=0;i<3;i++) rt_free(&rt_dof[i]);
  G_ok=false;
}

static bool ensure_targets(int vw,int vh){
  if(vw==cur_vw && vh==cur_vh && g_gpu_quality==cur_q) return rt_hdr.fb!=0;
  cur_vw=vw; cur_vh=vh; cur_q=g_gpu_quality;
  int cap = g_gpu_quality<=0 ? 1920 : (g_gpu_quality==1 ? 3840 : 16384);
  if(cap>max_tex) cap=max_tex;
  int iw=vw, ih=vh; if(iw>cap){ ih=(int)((long long)ih*cap/iw); iw=cap; } if(ih>max_tex){ iw=(int)((long long)iw*max_tex/ih); ih=max_tex; }
  if(iw<SCREEN_W/2) iw=SCREEN_W/2; if(ih<SCREEN_H/2) ih=SCREEN_H/2;
  IW=iw; IH=ih;
  int ls = iw>=1600 ? 3 : (iw>=960 ? 2 : 1); if(g_gpu_quality<=0 && ls>2) ls=2;
  static const GLint F_LIGHT[]={GL_RGBA16F,GL_RGBA8};
  GLint fh[1]={fmt_hdr}, fl[1]={fmt_light};
  (void)F_LIGHT;
  bool ok = rt_make(&rt_light,SCREEN_W*ls,SCREEN_H*ls,fl,1,NULL) && rt_make(&rt_hdr,iw,ih,fh,1,NULL);
  int bw=iw/2, bh=ih/2; if(bw>1280){ bh=bh*1280/bw; bw=1280; }
  n_bl=0;
  for(int i=0;i<7 && ok;i++){
    if(bw<12||bh<8) break;
    ok = rt_make(&rt_bl[i],bw,bh,fh,1,NULL); n_bl++;
    bw=(bw+1)/2; bh=(bh+1)/2;
  }
  for(int i=0;i<3 && ok;i++) ok = rt_make(&rt_dof[i],SCREEN_W>>i,SCREEN_H>>i,fh,1,NULL);
  if(!ok){ logf_("Impossible de creer les tampons %s%s\n","",""); G_failed=true; return false; }
  return true;
}

/* ======================================================================= */
/*                                  RENDU                                  */
/* ======================================================================= */
static float vbuf[GPU_MAX_FX*6*22];

static void bind_rt(const RT *r){ gl.BindFramebuffer(GL_FRAMEBUFFER,r->fb); gl.Viewport(0,0,r->w,r->h); }
static void tex_unit(int u,GLuint t){ gl.ActiveTexture(GL_TEXTURE0+(GLenum)u); gl.BindTexture(GL_TEXTURE_2D,t); }
static void draw_full(void){
  gl.BindBuffer(GL_ARRAY_BUFFER,vbo_full);
  gl.EnableVertexAttribArray(A_POS); gl.VertexAttribPointer(A_POS,2,GL_FLOAT,0,8,(const void*)0);
  gl.DrawArrays(GL_TRIANGLES,0,3);
}
static void attribs(int n,const int *sizes,int stride){
  int off=0;
  for(int i=0;i<n;i++){ gl.EnableVertexAttribArray((GLuint)i); gl.VertexAttribPointer((GLuint)i,sizes[i],GL_FLOAT,0,stride*4,(const void*)(intptr_t)(off*4)); off+=sizes[i]; }
}
static void attribs_off(int n){ for(int i=1;i<n;i++) gl.DisableVertexAttribArray((GLuint)i); }
static void upload(GLuint t,const uint32_t *px){ gl.BindTexture(GL_TEXTURE_2D,t); gl.TexSubImage2D(GL_TEXTURE_2D,0,0,0,SCREEN_W,SCREEN_H,G_es?GL_RGBA:GL_BGRA,GL_UNSIGNED_BYTE,px); }

static int build_lights(void){
  float *v=vbuf; int n=0;
  float lx0=GP.lit[0], ly0=GP.lit[1], lx1=GP.lit[2], ly1=GP.lit[3];
  for(int i=0;i<g_nlights;i++){
    const GLightRec *L=&g_lights[i];
    float x0=fmaxf(L->x-L->r,lx0), x1=fminf(L->x+L->r,lx1), y0=fmaxf(L->y-L->r,ly0), y1=fminf(L->y+L->r,ly1);
    if(x1<=x0||y1<=y0) continue;
    const float q[6][2]={{x0,y0},{x1,y0},{x1,y1},{x0,y0},{x1,y1},{x0,y1}};
    for(int k=0;k<6;k++){ *v++=q[k][0]; *v++=q[k][1]; *v++=L->x; *v++=L->y; *v++=L->r; *v++=L->h; *v++=L->cr; *v++=L->cg; *v++=L->cb; *v++=L->namt; *v++=L->sh; *v++=0; }
    n+=6;
  }
  return n;
}
static float *emit6(float *v,const float c[4][2],const float *at){
  static const int IDX[6]={0,1,2,0,2,3};
  for(int k=0;k<6;k++){ const float *p=c[IDX[k]]; *v++=p[0]; *v++=p[1]; memcpy(v,at,20*sizeof(float)); v+=20; }
  return v;
}
static int build_fx(void){
  float *v=vbuf; int n=0;
  for(int i=0;i<g_nfx;i++){
    const FxItem *f=&g_fx[i];
    float at[20]={ f->x0,f->y0,f->x1,f->y1, f->r,f->p,f->q,(float)f->type,
      cr(f->c)/255.0f,cg(f->c)/255.0f,cb(f->c)/255.0f,f->a,
      cr(f->c2)/255.0f,cg(f->c2)/255.0f,cb(f->c2)/255.0f,(float)((f->c2>>24)&1u),
      f->clip[0],f->clip[1],f->clip[2],f->clip[3] };
    float c[4][2];
    float e;
    switch(f->type){
      case FX_CAPS: case FX_SHAFT: {
        float dx=f->x1-f->x0, dy=f->y1-f->y0, l=sqrtf(dx*dx+dy*dy);
        float ux= l>0.001f ? dx/l : 1, uy= l>0.001f ? dy/l : 0;
        float hw = f->type==FX_CAPS ? f->r+1.2f : f->r*0.5f+0.5f, ext = f->type==FX_CAPS ? f->r+1.2f : 0.5f;
        float nx=-uy*hw, ny=ux*hw;
        float ax=f->x0-ux*ext, ay=f->y0-uy*ext, bx=f->x1+ux*ext, by=f->y1+uy*ext;
        c[0][0]=ax+nx; c[0][1]=ay+ny; c[1][0]=bx+nx; c[1][1]=by+ny; c[2][0]=bx-nx; c[2][1]=by-ny; c[3][0]=ax-nx; c[3][1]=ay-ny;
      } break;
      case FX_BEAM: {
        e=f->r*2.6f+1.5f;
        c[0][0]=f->x0-e; c[0][1]=f->y0; c[1][0]=f->x0+e; c[1][1]=f->y0; c[2][0]=f->x0+e; c[2][1]=f->y1; c[3][0]=f->x0-e; c[3][1]=f->y1;
      } break;
      default: {
        switch(f->type){
          case FX_RING: e=f->r+f->p*1.6f+2.5f; break;
          case FX_BALL: e= f->q>0.5f ? f->r*3.4f+3.0f : f->r+1.5f; break;
          case FX_DISC: e=f->r*1.8f+1.5f; break;
          default: e=f->r+0.5f; break;
        }
        c[0][0]=f->x0-e; c[0][1]=f->y0-e; c[1][0]=f->x0+e; c[1][1]=f->y0-e; c[2][0]=f->x0+e; c[2][1]=f->y0+e; c[3][0]=f->x0-e; c[3][1]=f->y0+e;
      } break;
    }
    v=emit6(v,(const float (*)[2])c,at); n+=6;
  }
  return n;
}

static void set_cam(GLint ucam,GLint uctr){
  float zoom = GP.cam_zoom>0.05f ? GP.cam_zoom : 1.0f;
  gl.Uniform4f(ucam,GP.cam_x,GP.cam_y,zoom,((float)SCREEN_W/IW)/zoom);
  gl.Uniform4f(uctr,GP.cam_cx,GP.cam_cy,0,0);
}

void gpu_reset_state(void){
  if(!G_ok) return;
  gl.UseProgram(0); gl.BindBuffer(GL_ARRAY_BUFFER,0);
  for(int i=0;i<6;i++) gl.DisableVertexAttribArray((GLuint)i);
  for(int u=7;u>=0;u--){ gl.ActiveTexture(GL_TEXTURE0+(GLenum)u); gl.BindTexture(GL_TEXTURE_2D,0); }
  gl.BindFramebuffer(GL_FRAMEBUFFER,0); gl.Disable(GL_BLEND);
}

void gpu_render(unsigned fbo,int win_w,int win_h,int vx,int vy,int vw,int vh){
  if(!G_ok || G_failed) return;
  while(gl.GetError()!=GL_NO_ERROR){}
  if(vw<16||vh<16) return;
  if(!ensure_targets(vw,vh)) return;
  gl.Disable(GL_DEPTH_TEST); gl.Disable(GL_CULL_FACE); gl.Disable(GL_SCISSOR_TEST); gl.Disable(GL_BLEND);
  gl.ActiveTexture(GL_TEXTURE0);
  /* icones en pleine definition : atlas (une fois), puis masque et donnees de l'image */
  if(tx_icoM && tx_icoI && un.icoOn>=0){
    int v=icons_atlas_version();
    if(v>0 && v!=ico_ver){
      if(!tx_icoA) gl.GenTextures(1,&tx_icoA);
      gl.BindTexture(GL_TEXTURE_2D,tx_icoA);
      int nl=icons_atlas_levels();
      gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR); gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
      gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE); gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
      gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAX_LEVEL,nl-1);
      gl.PixelStorei(GL_UNPACK_ALIGNMENT,1);
      for(int l=0;l<nl;l++){ int w,h; const uint8_t *d=icons_atlas_level(l,&w,&h); if(d) gl.TexImage2D(GL_TEXTURE_2D,l,GL_RGBA8,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,d); }
      gl.PixelStorei(GL_UNPACK_ALIGNMENT,4);
      ico_ver=v;
    }
    int fid=icons_frame_id();
    if(fid!=ico_frame){
      ico_frame=fid; ico_n= ico_ver>0 ? icons_gpu_prepare(ico_mask,ico_idata) : 0;
      if(ico_n>0){
        gl.BindTexture(GL_TEXTURE_2D,tx_icoM); gl.PixelStorei(GL_UNPACK_ALIGNMENT,1);
        gl.TexSubImage2D(GL_TEXTURE_2D,0,0,0,SCREEN_W,SCREEN_H,GL_LUMINANCE_ALPHA,GL_UNSIGNED_BYTE,ico_mask);
        gl.BindTexture(GL_TEXTURE_2D,tx_icoI);
        gl.TexSubImage2D(GL_TEXTURE_2D,0,0,0,256,4,GL_RGBA,GL_UNSIGNED_BYTE,ico_idata);
        gl.PixelStorei(GL_UNPACK_ALIGNMENT,4);
      }
    }
  }
  /* v8 : decor HD (une fois par niveau) */
  { int hv=hd_gpu_version();
    if(hv>0 && hv!=hd_ver){
      if(!tx_hdA){ gl.GenTextures(1,&tx_hdA); gl.GenTextures(1,&tx_hdN); gl.GenTextures(1,&tx_hdL); }
      int nl=hd_gpu_levels(), l0=0; gl.PixelStorei(GL_UNPACK_ALIGNMENT,1);
      /* v9 : atlas 4K ; on commence au niveau de reduction que la carte accepte (et a ~2K en mode Performance) */
      while(l0<nl-1){ int w,h; (void)hd_gpu_level(0,l0,&w,&h); bool big= w>max_tex || h>max_tex, perf= l0==0 && g_gpu_quality<=0 && w>2400; if(!big && !perf) break; l0++; }
      for(int k=0;k<2;k++){ GLuint t= k==0 ? tx_hdA : tx_hdN; gl.BindTexture(GL_TEXTURE_2D,t);
        gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR); gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE); gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
        gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAX_LEVEL,nl-1-l0);
        for(int l=l0;l<nl;l++){ int w,h; const uint8_t *d=hd_gpu_level(k,l,&w,&h); if(d) gl.TexImage2D(GL_TEXTURE_2D,l-l0,GL_RGBA8,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,d); } }
      { int w,h; const uint8_t *d=hd_gpu_low(&w,&h); gl.BindTexture(GL_TEXTURE_2D,tx_hdL);
        gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST); gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE); gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
        gl.TexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,d); }
      gl.PixelStorei(GL_UNPACK_ALIGNMENT,4);
      if(gl.GetError()!=GL_NO_ERROR) logf_("Textures HD : erreur d'envoi%s%s\n","","");
      hd_gpu_release_cpu(); hd_ver=hv;
    }
  }
  bool hd_on = tx_hdA && hd_ver>0 && hd_gpu_version()==hd_ver && hd_gpu_key()>=0;
  float hdM[8][4], hdP[8][4], hdG[4], hdSc=0; if(hd_on) hd_gpu_params(hdM,hdP,hdG,&hdSc);
  upload(tx_scene,g_screen); upload(tx_emis,g_emis); upload(tx_ui,g_ui);
  if(GP.lit_on){ upload(tx_nrm,g_nrm);
    gl.BindTexture(GL_TEXTURE_2D,tx_occ); gl.PixelStorei(GL_UNPACK_ALIGNMENT,1);
    gl.TexSubImage2D(GL_TEXTURE_2D,0,0,0,SCREEN_W,SCREEN_H,GL_LUMINANCE,GL_UNSIGNED_BYTE,g_occ); gl.PixelStorei(GL_UNPACK_ALIGNMENT,4); }

  /* 1. lumieres */
  bind_rt(&rt_light); gl.ClearColor(0,0,0,0); gl.Clear(GL_COLOR_BUFFER_BIT);
  if(GP.lit_on && g_nlights>0){
    int nv=build_lights();
    if(nv>0){
      gl.UseProgram(p_light);
      tex_unit(0,tx_nrm); tex_unit(1,tx_occ); gl.Uniform1i(ul.nrm,0); gl.Uniform1i(ul.occ,1); gl.Uniform2f(ul.game,SCREEN_W,SCREEN_H); gl.Uniform1f(ul.lsc,lscale);
      tex_unit(2,tx_scene); tex_unit(3,hd_on?tx_hdN:tx_nrm); gl.Uniform1i(ul.scn,2); gl.Uniform1i(ul.hdN,3);
      gl.Uniform2f(ul.hdS,hdSc,hd_on?1.0f:0.0f);
      if(hd_on){ gl.Uniform4fv(ul.hdM,8,&hdM[0][0]); gl.Uniform4fv(ul.hdP,8,&hdP[0][0]); gl.Uniform4f(ul.hdG,hdG[0],hdG[1],hdG[2],hdG[3]); }
      { extern int g_dbg; gl.Uniform2f(ul.lp,(g_dbg&4)?0.0f:1.0f,(g_dbg&32)?0.0f:GP.spec); }
      gl.BindBuffer(GL_ARRAY_BUFFER,vbo_dyn); gl.BufferData(GL_ARRAY_BUFFER,(GLsizeiptr)nv*12*4,vbuf,GL_STREAM_DRAW);
      static const int SZ[4]={2,4,4,2}; attribs(4,SZ,12);
      gl.Enable(GL_BLEND); gl.BlendFunc(GL_ONE,GL_ONE);
      gl.DrawArrays(GL_TRIANGLES,0,nv);
      gl.Disable(GL_BLEND); attribs_off(4); gl.ActiveTexture(GL_TEXTURE0);
    }
  }

  /* 2. composition de la scene (HDR) */
  bind_rt(&rt_hdr);
  gl.UseProgram(p_scene);
  tex_unit(0,tx_scene); tex_unit(1,tx_emis); tex_unit(2,rt_light.tex); tex_unit(3,tx_noise);
  gl.Uniform1i(us.scene,0); gl.Uniform1i(us.emis,1); gl.Uniform1i(us.light,2); gl.Uniform1i(us.noise,3);
  tex_unit(4,hd_on?tx_hdA:tx_noise); tex_unit(5,hd_on?tx_hdL:tx_noise); gl.Uniform1i(us.hdA,4); gl.Uniform1i(us.hdL,5);
  gl.Uniform2f(us.hdS,hdSc,hd_on?1.0f:0.0f); gl.Uniform1f(us.sky,1.0f);
  if(hd_on){ gl.Uniform4fv(us.hdM,8,&hdM[0][0]); gl.Uniform4fv(us.hdP,8,&hdP[0][0]); gl.Uniform4f(us.hdG,hdG[0],hdG[1],hdG[2],hdG[3]); }
  gl.Uniform2f(us.game,SCREEN_W,SCREEN_H); set_cam(us.cam,us.ctr);
  gl.Uniform4f(us.view,GP.view[0],GP.view[1],GP.view[2],GP.view[3]);
  gl.Uniform4f(us.lit,GP.lit[0],GP.lit[1],GP.lit[2],GP.lit[3]);
  gl.Uniform4f(us.amb,GP.amb[0],GP.amb[1],GP.amb[2],GP.lit_on?1.0f:0.0f);
  gl.Uniform4fv(us.shock,8,&GP.shock[0][0]);
  gl.Uniform4f(us.misc,GP.haze,GP.time,GP.emis_gain,GP.scroll);
  gl.Uniform4f(us.fog,GP.fog[0],GP.fog[1],GP.fog[2],GP.fog[3]);
  { extern int g_dbg; gl.Uniform4f(us.fog2,GP.fog_dark,GP.fog_speed,1.0f/lscale,(g_dbg&64)?1.0f:0.0f); }
  gl.Uniform4f(us.p25,P25_K*GP.p25,(PIT_X0+PIT_X1)*0.5f,(PIT_X1-PIT_X0)*0.5f,(float)SC_FW);
  draw_full();

  /* 3. effets */
  if(g_nfx>0){
    int nv=build_fx();
    gl.UseProgram(p_fx);
    tex_unit(0,tx_noise); tex_unit(1,rt_light.tex); gl.Uniform1i(uf.noise,0); gl.Uniform1i(uf.light,1); gl.Uniform2f(uf.game,SCREEN_W,SCREEN_H); set_cam(uf.cam,uf.ctr); gl.Uniform1f(uf.time,GP.time); gl.Uniform4f(uf.p25,P25_K*GP.p25,(PIT_X0+PIT_X1)*0.5f,(PIT_X1-PIT_X0)*0.5f,(float)SC_FW);
    if(GP.lit_on) gl.Uniform4f(uf.ambl,GP.amb[0],GP.amb[1],GP.amb[2],1.0f/lscale); else gl.Uniform4f(uf.ambl,2.0f,2.0f,2.0f,0.0f);
    gl.BindBuffer(GL_ARRAY_BUFFER,vbo_dyn); gl.BufferData(GL_ARRAY_BUFFER,(GLsizeiptr)nv*22*4,vbuf,GL_STREAM_DRAW);
    static const int SZ[6]={2,4,4,4,4,4}; attribs(6,SZ,22);
    gl.Enable(GL_BLEND); gl.BlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
    gl.DrawArrays(GL_TRIANGLES,0,nv);
    gl.Disable(GL_BLEND); attribs_off(6);
  }

  /* 4. bloom */
  bool bloom_on = GP.bloom>0.001f && n_bl>=2;
  if(bloom_on){
    gl.UseProgram(p_down); gl.Uniform1i(ud.src,0);
    tex_unit(0,rt_hdr.tex);
    bind_rt(&rt_bl[0]); gl.Uniform2f(ud.texel,1.0f/rt_hdr.w,1.0f/rt_hdr.h); gl.Uniform4f(ud.p,1,GP.bloom_thr,GP.bloom_thr*0.5f+0.05f,24.0f); draw_full();
    for(int i=1;i<n_bl;i++){
      tex_unit(0,rt_bl[i-1].tex); bind_rt(&rt_bl[i]);
      gl.Uniform2f(ud.texel,1.0f/rt_bl[i-1].w,1.0f/rt_bl[i-1].h); gl.Uniform4f(ud.p,0,0,0,64.0f); draw_full();
    }
    gl.UseProgram(p_up); gl.Uniform1i(uu.src,0);
    gl.Enable(GL_BLEND); gl.BlendFunc(GL_ONE,GL_ONE);
    for(int i=n_bl-1;i>=1;i--){
      tex_unit(0,rt_bl[i].tex); bind_rt(&rt_bl[i-1]);
      gl.Uniform2f(uu.texel,1.0f/rt_bl[i].w,1.0f/rt_bl[i].h); gl.Uniform1f(uu.w,i>=n_bl-2?1.0f:0.85f); draw_full();
    }
    gl.Disable(GL_BLEND);
  }
  /* 5. flou (profondeur de champ, menus) */
  bool blur_on = GP.dof>0.001f || GP.blur_all>0.001f;
  if(blur_on){
    gl.UseProgram(p_down); gl.Uniform1i(ud.src,0);
    tex_unit(0,rt_hdr.tex); bind_rt(&rt_dof[0]); gl.Uniform2f(ud.texel,1.0f/rt_hdr.w*(rt_hdr.w/(float)rt_dof[0].w)*0.5f,1.0f/rt_hdr.h*(rt_hdr.h/(float)rt_dof[0].h)*0.5f); gl.Uniform4f(ud.p,0,0,0,8.0f); draw_full();
    for(int i=1;i<3;i++){ tex_unit(0,rt_dof[i-1].tex); bind_rt(&rt_dof[i]); gl.Uniform2f(ud.texel,1.0f/rt_dof[i-1].w,1.0f/rt_dof[i-1].h); gl.Uniform4f(ud.p,0,0,0,8.0f); draw_full(); }
    gl.UseProgram(p_up); gl.Uniform1i(uu.src,0);
    for(int i=2;i>=1;i--){ tex_unit(0,rt_dof[i].tex); bind_rt(&rt_dof[i-1]); gl.Uniform2f(uu.texel,1.0f/rt_dof[i].w,1.0f/rt_dof[i].h); gl.Uniform1f(uu.w,1.0f); draw_full(); }
  }

  /* 6. image finale */
  gl.BindFramebuffer(GL_FRAMEBUFFER,fbo);
  gl.Viewport(0,0,win_w,win_h); gl.ClearColor(0,0,0,1); gl.Clear(GL_COLOR_BUFFER_BIT);
  gl.Viewport(vx,vy,vw,vh);
  gl.UseProgram(p_final);
  tex_unit(0,rt_hdr.tex); tex_unit(1,bloom_on?rt_bl[0].tex:rt_hdr.tex); tex_unit(2,blur_on?rt_dof[0].tex:rt_hdr.tex); tex_unit(3,tx_ui); tex_unit(4,tx_noise);
  gl.Uniform1i(un.hdr,0); gl.Uniform1i(un.bloom,1); gl.Uniform1i(un.blur,2); gl.Uniform1i(un.ui,3); gl.Uniform1i(un.noise,4);
  gl.Uniform2f(un.game,SCREEN_W,SCREEN_H);
  gl.Uniform4f(un.p0,GP.bloom/(float)(n_bl>0?n_bl:1),GP.exposure,GP.sat,GP.con);
  gl.Uniform4f(un.p1,GP.vig,GP.chroma*0.01f,GP.grain,GP.time);
  gl.Uniform4f(un.p2,GP.dof_y0,GP.dof_y1,GP.dof,GP.blur_all);
  gl.Uniform4f(un.p3,GP.lowhp,GP.desat,GP.tint_amt,(float)SCREEN_W/vw);
  gl.Uniform4f(un.flash,GP.flash[0],GP.flash[1],GP.flash[2],GP.flash[3]);
  gl.Uniform3f(un.sh,GP.sh_tint[0],GP.sh_tint[1],GP.sh_tint[2]); gl.Uniform3f(un.hi,GP.hi_tint[0],GP.hi_tint[1],GP.hi_tint[2]);
  gl.Uniform4f(un.rect,GP.view[0],GP.view[1],GP.view[2],GP.view[3]);
  gl.Uniform4f(un.on,bloom_on?1.0f:0.0f,blur_on?1.0f:0.0f,GP.scan,0);
  { bool io=ico_n>0 && tx_icoA && ico_ver>0;
    if(io){ tex_unit(5,tx_icoA); tex_unit(6,tx_icoM); tex_unit(7,tx_icoI); gl.Uniform1i(un.icoA,5); gl.Uniform1i(un.icoM,6); gl.Uniform1i(un.icoI,7); }
    else { gl.Uniform1i(un.icoA,4); gl.Uniform1i(un.icoM,4); gl.Uniform1i(un.icoI,4); }
    gl.Uniform1f(un.icoOn,io?1.0f:0.0f); }
  draw_full();
  gl.UseProgram(0); gl.ActiveTexture(GL_TEXTURE0); gl.BindBuffer(GL_ARRAY_BUFFER,0);
  GLenum err=gl.GetError();
  if(err!=GL_NO_ERROR){ char b[64]; snprintf(b,sizeof b,"0x%04x",err); logf_("Erreur OpenGL pendant le rendu : %s%s\n",b,""); if(G_frames<120) G_failed=true; }
  G_frames++;
}

/* ---- banc de test ---- */
static RT rt_test;
unsigned gpu_test_target(int w,int h){
  if(!G_ok) return 0;
  static const GLint F8[1]={GL_RGBA8};
  if(rt_test.w!=w || rt_test.h!=h) rt_make(&rt_test,w,h,F8,1,NULL);
  return rt_test.fb;
}
void gpu_test_read(int w,int h,uint8_t *rgb){
  if(!G_ok) return;
  uint8_t *tmp=(uint8_t*)malloc((size_t)w*h*4);
  gl.BindFramebuffer(GL_FRAMEBUFFER,rt_test.fb); gl.Finish();
  gl.ReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,tmp);
  for(int y=0;y<h;y++){ const uint8_t *s=&tmp[(size_t)(h-1-y)*w*4]; uint8_t *d=&rgb[(size_t)y*w*3]; for(int x=0;x<w;x++){ d[x*3]=s[x*4]; d[x*3+1]=s[x*4+1]; d[x*3+2]=s[x*4+2]; } }
  free(tmp);
  gl.BindFramebuffer(GL_FRAMEBUFFER,0);
}
