/* GOUFFRE - interface entre le jeu et la plateforme (Windows / test Linux) */
#ifndef PLATFORM_H
#define PLATFORM_H
#include <stdint.h>
#include <stdbool.h>

#define SCREEN_W 640
#define SCREEN_H 360
#define AUDIO_RATE 44100

enum {
  KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT,   /* ZQSD / WASD (position physique) + fleches */
  KEY_ENTER, KEY_ESCAPE, KEY_SPACE, KEY_TAB,
  KEY_1, KEY_2, KEY_3, KEY_4,
  KEY_R, KEY_E, KEY_M, KEY_P, KEY_F11, KEY_F1, KEY_BACKSPACE,
  KEY_T,                                   /* v6.6 : entrainement des heros (campement) */
  KEY_COUNT
};
enum { MOUSE_LEFT, MOUSE_RIGHT, MOUSE_MIDDLE, MOUSE_COUNT };
enum { PAD_A, PAD_B, PAD_X, PAD_Y, PAD_START, PAD_BACK, PAD_LB, PAD_RB,
       PAD_UP, PAD_DOWN, PAD_LEFT, PAD_RIGHT, PAD_COUNT };
enum { PADT_NONE, PADT_XBOX, PADT_PS, PADT_NINTENDO, PADT_GENERIC };

typedef struct {
  bool key[KEY_COUNT], key_pressed[KEY_COUNT];
  bool mb[MOUSE_COUNT], mb_pressed[MOUSE_COUNT];
  float mx, my;          /* souris en pixels du jeu (640x360) */
  bool mouse_moved;
  bool mouse_inside;
  int wheel;
  bool pad_on;
  int pad_type;          /* PADT_* */
  char pad_name[64];     /* nom affichable de la manette */
  char pad_info[64];     /* methode de lecture (diagnostic) */
  float lx, ly, rx, ry;  /* sticks -1..1, y vers le bas */
  float lt, rt;
  bool pad[PAD_COUNT], pad_pressed[PAD_COUNT];
  /* ecran tactile (iPhone) : sticks virtuels */
  bool touch;            /* la derniere commande vient du doigt */
  float tmx, tmy;        /* stick de deplacement -1..1 */
  bool taim; float tax, tay;   /* stick de visee (direction) */
} Input;

/* --- fourni par le jeu --- */
void game_init(void);
void game_frame(Input *in, float dt);          /* mise a jour + rendu */
const uint32_t *game_framebuffer(void);        /* 640x360, 0xAARRGGBB */
void game_audio_mix(int16_t *out, int frames); /* thread audio, stereo entrelace */
bool game_should_quit(void);
int  game_fullscreen_request(void);            /* -1 rien, sinon 0/1 voulu */
void game_fullscreen_changed(bool fs);
bool game_pixel_perfect(void);
void game_focus_lost(void);
void game_shutdown(void);
void game_bot_input(Input *in, float dt);      /* pilote automatique (tests) */

/* --- fourni par la plateforme --- */
const char *plat_save_path(void);
bool plat_thread_start(void (*fn)(void*),void *arg);   /* tache de fond (faux si indisponible) */
void plat_rumble(float strength,float seconds);   /* vibration (si la manette le permet) */
#endif
