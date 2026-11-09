// The PlayStation-style title, f_firetitle.h.
//
// The fire is a field of heat in cells of FIRE_CELL screen pixels. The bottom
// row burns at full heat; each tic every cell above takes the heat of the
// cell below it, a little cooler at random and pushed a cell sideways at
// random, so flames lick upward, flicker and die out as they climb. Heat is
// coloured black, deep red, orange, yellow to white, each step the WAD
// palette's nearest colour, so it stays in the game's pixels and colours.
//
// The logo is M_DOOM at twice its size, rising from below the screen to its
// place over the first seconds. It is drawn first and the fire over it, so it
// comes up from behind the flames. Its own random numbers: the game's, and
// demos, are untouched.

#include <string.h>

#include "doomdef.h"
#include "m_swap.h"
#include "r_defs.h"
#include "v_video.h"
#include "w_wad.h"
#include "z_zone.h"

#include "f_firetitle.h"
#include "../platform/ps1_screen.h"

double I_GetTimeExact (void);

boolean		ps1title = true;
ps1_screen_t	ps1screen;
int		ps1screens;
float		ps1firespeed = 1;

extern boolean	menuactive;

#define FIRE_LEVELS	37		// heat 0 (none) to 36 (the burning floor)
#define FIRE_CELL	2		// screen pixels a side: chunky flames that hang together
#define FIRE_W		(SCREENWIDTH/FIRE_CELL)
#define FIRE_H		(SCREENHEIGHT/FIRE_CELL)
#define LOGO_SCALE	2
#define LOGO_TOP	14		// where the logo comes to rest
#define RISE_DELAY	35		// tics of fire alone before the logo shows
#define RISE_TICS	200		// how long it takes to rise (about 5.7 s)

static byte	heat[FIRE_W*FIRE_H];
static byte	colors[FIRE_LEVELS];
static boolean	colored;
static int	tics;
static unsigned	seed = 0x6d2b79f5u;

static unsigned Rand (void)
{
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    return seed;
}

// Each heat level's colour: a ramp through deep red, orange and yellow to
// white, matched to the palette.
static void ColorFire (void)
{
    const byte*	palette = W_CacheLumpName ("PLAYPAL", PU_CACHE);
    int		level, i;

    for (level = 0; level < FIRE_LEVELS; level++)
    {
	float	t = level / (float)(FIRE_LEVELS - 1), r, g, b;
	int	best = 0, bestdist = 1 << 30;

	if (t < 0.35f) { r = t/0.35f*0.75f; g = 0.02f; b = 0.02f; }
	else if (t < 0.65f) { r = 0.75f + (t - 0.35f)/0.30f*0.25f; g = (t - 0.35f)/0.30f*0.45f; b = 0.02f; }
	else if (t < 0.9f) { r = 1; g = 0.45f + (t - 0.65f)/0.25f*0.45f; b = 0.05f; }
	else { r = 1; g = 0.9f + (t - 0.9f)/0.1f*0.1f; b = (t - 0.9f)/0.1f*0.8f; }
	if (!level) r = g = b = 0;
	for (i = 0; i < 256; i++)
	{
	    int dr = palette[i*3] - (int)(r*255), dg = palette[i*3+1] - (int)(g*255), db = palette[i*3+2] - (int)(b*255);
	    int dist = 3*dr*dr + 4*dg*dg + 2*db*db;
	    if (dist < bestdist) { bestdist = dist; best = i; }
	}
	colors[level] = (byte)best;
    }
    colored = true;
}

// With the PlayStation's art (ps1_screen.h), its title exactly: a 64x128 field
// of heat, tiled four across from row 116 of its 256x240 screen, over a picture
// rising from below it a row at a time. Both moved every second vblank, 30 a
// second. Near the top each even row lowers the bottom row's heat, so the fire
// burns itself out and leaves the picture. (The PsyDoom reconstruction's ti_main.)
#define PS1_LOGO_START	250		// SCREEN_H + 10
#define PS1_MOVES	30		// a second
static byte	psheat[PS1_FIRE_W*PS1_FIRE_H];
static float	fireclock, frametime;
static int	framespermove = 2, framecount;
static int	logoy;
static boolean	psready;
static double	lastdraw = -1;

static void PsFireStart (void)
{
    memset (psheat, 0, sizeof(psheat));
    memset (psheat + (PS1_FIRE_H-1)*PS1_FIRE_W, FIRE_LEVELS-1, PS1_FIRE_W);
    logoy = PS1_LOGO_START;
    fireclock = 0;
    framecount = 0;
    lastdraw = -1;
    psready = true;
}

static void PsFireBurn (void)
{
    int		x, y;
    byte*	bottom = psheat + (PS1_FIRE_H-1)*PS1_FIRE_W;

    if (logoy < 50 && !(logoy & 1) && bottom[0])
	memset (bottom, bottom[0]-1, PS1_FIRE_W);
    for (x = 0; x < PS1_FIRE_W; x++)
	for (y = 1; y < PS1_FIRE_H; y++)
	{
	    byte	h = psheat[y*PS1_FIRE_W + x];
	    unsigned	r;

	    if (!h)
	    {
		psheat[(y-1)*PS1_FIRE_W + x] = 0;
		continue;
	    }
	    r = Rand ();
	    psheat[(y-1)*PS1_FIRE_W + ((x + 1 - (int)(r & 3)) & (PS1_FIRE_W-1))] = (byte)(h - ((r >> 2) & 1));
	}
}

// The picture moves in step with the display, not the game's tics: frames here
// come at the display's rate, and neither 35 tics a second nor a clock running at
// 30 fall evenly into those at 144 Hz, one move 4 frames after the last and the
// next 5. So a move comes every whole number of frames, the number nearest 30
// moves a second (5 at 144 Hz, 28.8 a second; 2 at 60), from the frame time seen.
static void PsFireAdvance (void)
{
    double	now = I_GetTimeExact ();	// in tics
    float	ideal, off;

    if (lastdraw >= 0)
    {
	float seconds = (float)((now - lastdraw)/TICRATE);

	if (seconds > 0 && seconds < 0.1f)	// not a pause or a load
	    frametime = frametime > 0 ? frametime + (seconds - frametime)*0.05f : seconds;
    }
    lastdraw = now;
    if (frametime <= 0)
	return;
    ideal = 1/(PS1_MOVES*frametime);
    off = ideal - framespermove;
    if (off > 0.75f || off < -0.75f)	// not on every wobble of the measurement
    {
	framespermove = (int)(ideal + 0.5f);
	if (framespermove < 1)
	    framespermove = 1;
    }
    if (++framecount < framespermove)
	return;
    framecount = 0;
    if (logoy > 0)
	logoy--;
    for (fireclock += ps1firespeed; fireclock >= 1; fireclock -= 1)
	PsFireBurn ();
}

void F_FireTitleStart (void)
{
    PsFireStart ();
    memset (heat, 0, sizeof(heat));
    memset (heat + (FIRE_H-1)*FIRE_W, FIRE_LEVELS-1, FIRE_W);
    tics = 0;
}

void F_FireTitleTicker (void)
{
    int		x, y;

    if (ps1screens)
    {
	return;		// moved as it is drawn (PsFireAdvance)
    }
    tics++;
    for (x = 0; x < FIRE_W; x++)
	for (y = 1; y < FIRE_H; y++)
	{
	    int		from = y*FIRE_W + x;
	    unsigned	r = Rand ();
	    int		to = from - FIRE_W + (int)(r % 3) - 1;
	    // Cooling 0, 1 or 2 a row: flames reach about a third of the way up.
	    int		cooled = heat[from] - (int)((r >> 2) % 3);

	    if (to < 0)
		continue;
	    heat[to] = (byte)(cooled > 0 ? cooled : 0);
	}
}

// The logo's place now: below the screen, then easing up to rest.
static int LogoTop (void)
{
    float	t = (tics - RISE_DELAY) / (float)RISE_TICS;

    if (t <= 0)
	return SCREENHEIGHT;
    if (t >= 1)
	return LOGO_TOP;
    t = 1 - (1 - t)*(1 - t)*(1 - t);	// slowing as it arrives
    return SCREENHEIGHT + (int)((LOGO_TOP - SCREENHEIGHT)*t);
}

// M_DOOM at LOGO_SCALE, clipped to the screen.
static void DrawLogo (void)
{
    int		lump = W_CheckNumForName ("M_DOOM");
    patch_t*	patch;
    int		width, left, top = LogoTop (), col;

    if (lump < 0 || top >= SCREENHEIGHT)
	return;
    patch = W_CacheLumpNum (lump, PU_CACHE);
    width = SHORT(patch->width);
    left = (SCREENWIDTH - width*LOGO_SCALE)/2;
    for (col = 0; col < width; col++)
    {
	column_t*	column = (column_t *)((byte *)patch + LONG(patch->columnofs[col]));

	while (column->topdelta != 0xff)
	{
	    byte*	source = (byte *)column + 3;
	    int		i, sx, sy;

	    for (i = 0; i < column->length; i++)
		for (sy = 0; sy < LOGO_SCALE; sy++)
		{
		    int y = top + (column->topdelta + i)*LOGO_SCALE + sy;
		    if (y < 0 || y >= SCREENHEIGHT)
			continue;
		    for (sx = 0; sx < LOGO_SCALE; sx++)
		    {
			int x = left + col*LOGO_SCALE + sx;
			if (x >= 0 && x < SCREENWIDTH)
			    screens[0][y*SCREENWIDTH + x] = source[i];
		    }
		}
	    column = (column_t *)((byte *)column + column->length + 4);
	}
    }
}

void F_FireTitleDrawer (void)
{
    int		i;

    if (ps1screens)
    {
	if (!psready)
	    PsFireStart ();
	PsFireAdvance ();
	// The picture is the renderer's; the menus drawn over it mark what they cover.
	ps1screen.mode = menuactive ? 2 : 1;
	ps1screen.heat = psheat;
	ps1screen.logoY = logoy;
	ps1screen.menuLogo = 0;
	memset (v_overlaymask, 0, sizeof(v_overlaymask));
	v_overlayactive = true;
	return;
    }
    if (!colored)
	ColorFire ();
    memset (screens[0], colors[0], SCREENWIDTH*SCREENHEIGHT);
    DrawLogo ();
    for (i = 0; i < SCREENWIDTH*SCREENHEIGHT; i++)
    {
	byte h = heat[(i/SCREENWIDTH/FIRE_CELL)*FIRE_W + (i%SCREENWIDTH)/FIRE_CELL];
	if (h)
	    screens[0][i] = colors[h];
    }
}
