// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:  Head up display
//
//-----------------------------------------------------------------------------

#ifndef __HU_STUFF_H__
#define __HU_STUFF_H__

#include "d_event.h"


//
// Globally visible constants.
//
#define HU_FONTSTART	'!'	// the first font characters
#define HU_FONTEND	'_'	// the last font characters

// Calculate # of glyphs in font.
#define HU_FONTSIZE	(HU_FONTEND - HU_FONTSTART + 1)	

#define HU_BROADCAST	5

#define HU_MSGREFRESH	KEY_ENTER
#define HU_MSGX		0
#define HU_MSGY		0
#define HU_MSGWIDTH	64	// in characters
#define HU_MSGHEIGHT	1	// in lines

#define HU_MSGTIMEOUT	(4*TICRATE)

// F4 Nethermost, PS1 message position: just above the status bar, as the
// PlayStation drew it (y 193 on its 256x240 screen, the bar at y 200, so 7
// rows above the bar at y 168), its letters in line with
// the bar's AMMO label, as there: the label's bright letters start at column
// 11 on a status bar pack's bar and 8 on DOOM's, the font's a column in.
#define HU_PS1MSGX_PACK	10
#define HU_PS1MSGX	7
#define HU_PS1MSGY	161
extern boolean hu_ps1messages;

//
// HEADS UP TEXT
//

void HU_Init(void);
void HU_Start(void);

boolean HU_Responder(event_t* ev);

void HU_Ticker(void);
void HU_Drawer(void);
char HU_dequeueChatChar(void);
void HU_Erase(void);

// The current level's name as the automap shows it ("E1M1: HANGAR").
const char *HU_LevelTitle(void);


#endif
//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
