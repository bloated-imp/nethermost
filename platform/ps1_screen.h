/* The PlayStation title and the background of the menu over it: the game fills
   this in (f_firetitle.c) and the renderer draws it as a 256x240 picture
   (scene3d/ps1_screen.cpp). */
#ifndef DOOM_PS1_SCREEN_H
#define DOOM_PS1_SCREEN_H
#ifdef __cplusplus
extern "C" {
#endif
#define PS1_FIRE_W 64
#define PS1_FIRE_H 128
typedef struct {
    int mode;                   /* 0 nothing this frame, 1 the title, 2 the menus' background */
    const unsigned char *heat;  /* the title's fire, PS1_FIRE_W by PS1_FIRE_H, 0 to 36 */
    int logoY;                  /* where the title picture is, 0 at rest */
    int menuLogo;               /* the main menu is up: the PlayStation's logo in place of the WAD's */
} ps1_screen_t;
extern ps1_screen_t ps1screen;
extern int ps1screens;          /* the art is there and the title and menu are switched on */
extern float ps1firespeed;      /* the fire's updates against the PlayStation's 30 a second */
#ifdef __cplusplus
}
#endif
#endif
