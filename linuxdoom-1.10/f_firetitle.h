// The PlayStation-style title (F4 Gameplay, PS1 title screen): fire burning
// up from the bottom of a black screen, the WAD's DOOM logo (M_DOOM) rising
// into place from behind the flames. Drawing only; it replaces TITLEPIC on
// the title page (d_main.c) while ps1title is on.
#ifndef __F_FIRETITLE__
#define __F_FIRETITLE__

#include "doomtype.h"

extern boolean ps1title;

// The title page begins: no fire yet, the logo below the screen.
void F_FireTitleStart (void);
// A tic: the fire climbs, the logo rises.
void F_FireTitleTicker (void);
// Into screens[0], all of it.
void F_FireTitleDrawer (void);

#endif
