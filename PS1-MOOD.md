# Nethermost: PS1 Mood

A fork of [DERP, the DOOM Engine Remaster Project](https://github.com/robongrui/derp)
that leans toward the feel of DOOM on the PlayStation: slower, darker and more
like a horror game, with Aubrey Hodges' ambient soundtrack in place of the MIDI
scores. Everything DERP does is still here; this fork adds to it.

## Your Own Music Tracks

Recorded music can stand in for any map's or screen's MIDI score. No music
ships with the game: players bring their own files.

1. Put audio files (FLAC, Ogg Vorbis, MP3 or WAV) in the `music` folder beside
   the game, or in the one in the game's save folder.
2. Name them after the map (`E1M1.flac`, `MAP01.ogg`) or the song
   (`D_INTER.mp3`), or list them in `music/playlist.txt`.

The playlist that comes with the game maps each Episode 1 level, and the
title, intermission and finale screens, to the track the PlayStation version
played there. An entry only needs the track's title: the game plays the first
file whose name contains it, so files keep their downloaded names. A map with
no track plays the WAD's score as before. `-nousermusic` turns it all off.

A music WAD made for ZDoom-family ports, whose songs are recordings rather
than MIDI (such as the 2007 PSX music WAD, in WMA), can be turned into such
files once: `python3 scripts/wad_music_to_files.py psxdoom.wad <save
folder>/music` writes D_E1M1.ogg and the rest, converting with ffmpeg what
the game can't read. Files named after the song play for every map using it.
A playlist entry wins over them, so the official album can be added later
beside them with a `playlist.txt`, and a map whose album track isn't found
keeps the WAD's.

Tracks stream from disk as they play, loop without a gap, are resampled to the
mixer's rate and follow the Music Volume slider. Code:
`platform/audio/track_stream.c`, `platform/user_music.c`; tests:
`tests/user_music.c`.

## Movement Tuning

**F4 → Nethermost → Movement** has seven sliders for finding a slower,
heavier feel by playing. Each is a multiplier; 1.00 leaves the Movement choice
above as it is.

- **Walk speed** and **Run speed**.
- **Turn speed**: the turn keys and the joystick; the mouse turns as before.
- **Turn ramp-up**: how long those take to reach full turning speed. 1.00 is
  DOOM's short slow start and jump; other values ease into full speed, 0 at once.
- **Momentum**: how long you take to get up to speed and to stop. The top
  speed stays the same.
- **View bob** and **Bob rate**: how much and how fast the view and the
  weapon bob as you move; the flashlight sways at the same pace.

They are saved with the other settings, and are off in demos and netgames, so
recordings play back as they were made. DOOM only, not Heretic. Code:
`linuxdoom-1.10/p_movetune.h`; tests: `tests/move_tuning.c`.

## The PlayStation's Picture

Resolution and upscaling are in **F4 → Graphics**; the rest are in **F4 → Nethermost**:

- **Resolution: 240p** with **Upscaling: Crisp pixels** draws everything, walls,
  sprites, weapon and status bar, on one coarse grid of square pixels. A screen
  whose height is a whole multiple of 240 (720, 1440, 2160) keeps them even.
- **Picture → Aspect: 4:3** keeps the picture the shape of an old TV, centred with black
  bars beside it; at 240p it is 320 by 240.
- **Picture → PS1 dithering** cuts the colours to the console's 15 bits
  through its 4x4 ordered dither, so smooth shading and fog break into a fine
  grain. It is rounded rather than truncated, so rooms keep their brightness.

- **Picture → PS1 title screen** (on by default) opens on fire burning up
  from the bottom of a black screen, the DOOM logo rising into place from
  behind the flames, as on the PlayStation, and stays until a button is
  pressed, which opens the menu over it. It is drawn from the WAD's own
  logo (M_DOOM) and palette; off, the title is the WAD's TITLEPIC.

- **Picture → PS1 fire sky** (on by default) turns the Hell maps' sky (SKY3:
  DOOM's third episode, DOOM II's MAP21 on) into a wall of fire, as on the
  PlayStation: its own fire algorithm (P_FireSky, as reconstructed in
  PSXDOOM-RE) at about its 15 updates a second, in its 37 fire colours, drawn
  crisp with none of the painted sky's softening or haze.

- **Picture → Light level** scales the rooms' own light (0.40 to 1.20, 1.00
  DOOM's): walls, floors, monsters, the weapon and the face darken, while lamps,
  window light, muzzle flashes, projectiles, fullbright frames and the
  flashlight keep their strength, so lit spots stand out against the dark.
- **Picture → PS1 motion** draws monsters, projectiles, doors and lifts in
  steps about 15 a second, as the PlayStation's game tics moved them; the game
  still runs at 35 and your view stays smooth.

Code: `linuxdoom-1.10/f_firetitle.c`, `platform/scene3d/fire_sky.cpp`, `pictureBox` in `platform/gpu3d/frame.cpp`, `ps1Dither` in
`platform/shaders/present.frag`.

## Relit Face

Like the weapon, the face on the status bar is relit by the light around
you: its painted light taken out, its shape read from the artwork as
normals, then turned to look ahead, where Doomguy looks. A lamp to your
right lights his right cheek, the room ahead lights his face, a muzzle
flash flares over it, and in the dark it falls into shadow. It keeps its
painted pixels, takes light in the colormaps' 1/16 steps, stays matte like
skin, and coloured light only tints it. The bullet guns' flashes shine a
soft gunpowder orange. **F4 →
Nethermost → Weapon → Relit face** (with Relit weapon on). Code: the face in
`weaponDraws` (`platform/scene3d/weapon.cpp`), drawn over the status bar in
`drawOverlays` (`platform/gpu3d/draw.cpp`), `MODE_FACE` in
`platform/shaders/world/weapon.glsl`.

The relit weapon also catches your own muzzle flash coming back off the walls,
floor and ceiling around you: strong in a tight room or against a wall, little
out in the open. **F4 → Nethermost → Weapon → Muzzle flash bounce on weapon**
sets how much.

The relit weapon also treats each part as what it is made of. Grey metal mirrors
the room and takes sharper highlights; a hand or a wooden stock stays matte.
**F4 → Nethermost → Weapon → Separate skin and metal** turns that off, and
**Metal shine** sets how shiny the metal is. Reflections follow where each part
faces, so a glowing floor lights the underside of the gun and not its top.

A ceiling on how bright light can make the weapon keeps its artwork readable in
the brightest rooms.

Over a glowing liquid such as slime or lava the gun keeps its original painted
pixels and takes an even wash of the liquid's color with a brighter rim along its
edges, so the light reads as coming from below. With the pool off to one side only
the side of the gun toward it takes that light; the far side stays in the room's.

**F4 → Nethermost → Weapon → Weapons 2.0** is the master switch for all of this,
the muzzle flash's bounce, color and place, and what the Light level and the PS1
pickup flash's strength do to the gun: off shows the weapon exactly as in DERP.

**F4 → Nethermost → Weapon → BFG Melt:** a monster the BFG blows apart, by its ball
or its tracers, starts its gibs glowing in the projectile's green and eases back to
their own colors by the end of the animation. **BFG Melt: normal deaths** does the
same for an ordinary death. **God face: glowing eyes:** while you are invincible the
yellow eyes on the relit face light themselves and throw a little light around them,
with sliders for how bright the eyes are, how much light they throw, how far, and how
sharply it fades.
The rampage and grin faces, which the game shows over the god face, take the same
eyes while you are invincible.

**F4 → Nethermost → PS1 title and menu background:** the PlayStation's title, and its
brown background behind the PC's menus, from art you put in the `hud` folder:
`ps1_title.png` and `ps1_menu.png` (both 256x240). The fire is the PlayStation's own
algorithm and colors; **PS1 title fire speed** slows it.

## Your Own Sound Effects

Recorded sound effects can stand in for the WAD's, such as a PlayStation
sound pack. None ship with the game: players bring their own files.

1. Put them in the `sounds` folder in the game's save folder (beside
   doom.cfg), or in one beside the game.
2. Either loose files named after the WAD's sound (`DSPISTOL.ogg`,
   `DSPOPAIN.flac`), or a ZDoom-style pack (`.pk3` or `.zip`) as it is.

A pack's SNDINFO is read too, so its monsters get sounds of their own: an
imp its own pain cry instead of the zombies', a shotgun guy and a chaingunner
their own sight, pain and death sets, claws and fireballs picked from a few
recordings. FLAC, Ogg Vorbis, MP3 and WAV are read at any rate, and play with
their own treble; muffling, echoes and distance work as for the WAD's.

**Replacing one sound later**: drop a loose file of the same name into the
folder. A loose file always wins over one inside a pack, the save folder's
over those beside the game, and an earlier pack (by name) over a later one.
A loose `SNDINFO.txt` is read after the packs', so it can point any name
(`imp/pain  DSMYPAIN`) at another file. **F4 → Sound → Effects → Supplied
sound files** switches between them and the WAD's own; `-nousersounds`
ignores them, and `-checksounds` decodes every one at start and reports any
that can't be read.

Code: `platform/user_sounds.c`, `platform/audio/zip_read.c`,
`platform/audio/sndinfo.c`, the names monsters answer to in `Game_SoundName`
(`platform/games/doom/profile.c`); tests: `tests/user_sounds.c`.

## Message Position

**F4 → Nethermost → HUD → PS1 message position** (on by default) puts pickup
and other messages just above the status bar, as on the PlayStation, where a
message also takes the place of the automap's level name. Off, they are at the
top of the screen, as on PC.

## Loading Screen

**F4 → Nethermost → HUD → PS1 loading screen** (on by default) shows the LOADING
plaque between levels at its PlayStation position, over the last screen shown, in
place of the title and progress bar. The art is yours: a `loading.png` in the `hud`
folder beside doom.cfg. Without it, the progress bar stays.

## Screen Flashes

In **F4 → Nethermost → Flashes**, two switches keep a flash to the 3D
view and your weapon, as the PlayStation did, with the status bar, on-screen
text and sky left as they are; off, the whole screen flashes, as on PC.

- **PS1 pickup flash** (on by default), with a **screen** and a **weapon** slider
  for how strongly it tints each; the same number on both brightens them about
  equally.
- **PS1 damage flash** (off by default), which also covers the radiation suit.

## Status Bar Pack

A WAD that restyles DOOM's status bar can give it its look, such as Kippykip's
PSX Doom HUD for Vanilla (PlayStation-style art in DOOM's layout). None ships
with the game.

1. Put the WAD in a `hud` folder in the game's save folder (beside doom.cfg),
   or in one beside the game. The first `.wad` by name is used.
2. **F4 → Nethermost → HUD → Status bar pack** (on by default)
   switches between its bar and the game's own, at once.

Only its status bar graphics are used (STBAR, STARMS, the numbers, keys and
faces); its sounds, menus, fonts and DEHACKED are left out. **HUD → PS1
muted red text** (on by default) gives the game's red text (menus, messages,
intermission) the reds of the pack's digits, so it matches the bar, and menus and messages
a black outline like the bar's labels; off, DOOM's bright red. A `-file` mod's own
bar still wins. Code: `W_AddFile`'s `^` in `linuxdoom-1.10/w_wad.c`,
`ST_UseStatusBarPack` in `st_stuff.c`, `I_StatusBarPack`.

## PS1 Reverb

**F4 → Nethermost → Sound → PS1 reverb** swaps the measured room acoustics
for the PlayStation's own reverb: its sound chip's reverb unit, rebuilt from the
psx-spx reference's formula and preset registers, running at the chip's 22050
Hz. Each level takes the preset and depth PlayStation DOOM gave it (Space on
E1M1 and MAP01, the studios and Hall elsewhere, from PsyDoom's tables); levels
it didn't have take its usual Hall. Sounds made under open sky stay dry, and
music takes none, since the recorded PlayStation tracks carry their own. Echo
and reverb still sets how much. Code: `platform/audio/psx_reverb.c`,
`Game_PsxReverb` in `platform/games/doom/profile.c`; test: `tests/psx_reverb.c`.

## Your Own Footsteps and Other Made-Up Sounds

The sounds the game makes up (footsteps, splashes, breaking glass and so on) can
be swapped for recordings. Put files in the `sounds` folder, named after the
effect and a number: `splash-thick1.wav`, `splash-thick2.wav` for nukage, slime,
blood and lava; `splash-water`, `step-stone`, `step-metal`, `step-wood`,
`step-soft`, the `run-` equivalents, `glass-shatter` and the rest (the names are
those `doom-sfx-render` writes). A different one plays each time, at the loudness
of the sound it replaces. Supplied sound files (F4) switches them off.

Ambient loops work the same way, one file named after the kind: `computer.wav` for
the computer walls, `crt`, `lamp`, `nukage`, `fire` and so on (not wind or rain). They
are looped and brought to the loudness of the loop they replace.

## Footstep Volume

**F4 → Nethermost → Sound → Footstep volume** sets how loud footsteps and splashes
are (0 to 2, 1.00 as made).

## Planned

- Varied monsters: each monster of a kind drawn from its own sprite set,
  loaded from a WAD the player supplies.
- A slower "PS1" movement preset, from the tuning values that feel right, and
  an optional 30 FPS cap.

## Rights

This fork, like DERP, is GPL 3 with DERP's attribution term (see `NOTICE.txt`).
The DOOM engine code is open source; DOOM's game data, the PlayStation
soundtrack and the PlayStation sound effects are not, and none of them is in
this repository or its downloads.
