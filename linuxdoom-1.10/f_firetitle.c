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

boolean		ps1title = true;

#define FIRE_LEVELS	37		// heat 0 (none) to 36 (the burning floor)
#define FIRE_CELL	2		// screen pixels a side: chunky flames that hang together
#define FIRE_W		(SCREENWIDTH/FIRE_CELL)
#define FIRE_H		(SCREENHEIGHT/FIRE_CELL)
#define LOGO_SCALE	2
#define LOGO_TOP	14		// where the logo comes to rest
#define RISE_DELAY	35		// tics of fire alone before the logo shows
#define RISE_TICS	(35*4)		// how long it takes to rise

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

void F_FireTitleStart (void)
{
    memset (heat, 0, sizeof(heat));
    memset (heat + (FIRE_H-1)*FIRE_W, FIRE_LEVELS-1, FIRE_W);
    tics = 0;
}

void F_FireTitleTicker (void)
{
    int		x, y;

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
