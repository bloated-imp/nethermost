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

**F4 → Game → Play → Tuning** has seven sliders for finding a slower,
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

In **F4 → Graphics**:

- **Resolution: 240p** with **Upscaling: Crisp pixels** draws everything, walls,
  sprites, weapon and status bar, on one coarse grid of square pixels. A screen
  whose height is a whole multiple of 240 (720, 1440, 2160) keeps them even.
- **Aspect: 4:3** keeps the picture the shape of an old TV, centred with black
  bars beside it; at 240p it is 320 by 240.
- **Retro look → PS1 dithering** cuts the colours to the console's 15 bits
  through its 4x4 ordered dither, so smooth shading and fog break into a fine
  grain. It is rounded rather than truncated, so rooms keep their brightness.

- **Retro look â†’ PS1 title screen** (on by default) opens on fire burning up
  from the bottom of a black screen, the DOOM logo rising into place from
  behind the flames, as on the PlayStation, and stays until a button is
  pressed, which opens the menu over it. It is drawn from the WAD's own
  logo (M_DOOM) and palette; off, the title is the WAD's TITLEPIC.

- **Retro look → PS1 fire sky** (on by default) turns the Hell maps' sky (SKY3:
  DOOM's third episode, DOOM II's MAP21 on) into a wall of fire, as on the
  PlayStation: its own fire algorithm (P_FireSky, as reconstructed in
  PSXDOOM-RE) at about its 15 updates a second, in its 37 fire colours, drawn
  crisp with none of the painted sky's softening or haze.

- **Retro look → Light level** scales the rooms' own light (0.40 to 1.20, 1.00
  DOOM's): walls, floors, monsters, the weapon and the face darken, while lamps,
  window light, muzzle flashes, projectiles, fullbright frames and the
  flashlight keep their strength, so lit spots stand out against the dark.
- **Retro look → PS1 motion** draws monsters, projectiles, doors and lifts in
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
soft gunpowder orange. **F4 â†’
Graphics â†’ Relit face** (with Relit weapon on). Code: the face in
`weaponDraws` (`platform/scene3d/weapon.cpp`), drawn over the status bar in
`drawOverlays` (`platform/gpu3d/draw.cpp`), `MODE_FACE` in
`platform/shaders/world/weapon.glsl`.

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
(`imp/pain  DSMYPAIN`) at another file. **F4 â†’ Sound â†’ Effects â†’ Supplied
sound files** switches between them and the WAD's own; `-nousersounds`
ignores them, and `-checksounds` decodes every one at start and reports any
that can't be read.

Code: `platform/user_sounds.c`, `platform/audio/zip_read.c`,
`platform/audio/sndinfo.c`, the names monsters answer to in `Game_SoundName`
(`platform/games/doom/profile.c`); tests: `tests/user_sounds.c`.

## Footstep Volume

**F4 → Sound → Effects → Footstep volume** sets how loud footsteps and splashes
are (0 to 2, 1.00 as made).

## Planned

- Varied monsters: each monster of a kind drawn from its own sprite set,
  loaded from a WAD the player supplies.
- A slower "PS1" movement preset, from the tuning values that feel right, and
  an optional 30 FPS cap.
- Reverb tuned toward the PlayStation's.

## Rights

This fork, like DERP, is GPL 3 with DERP's attribution term (see `NOTICE.txt`).
The DOOM engine code is open source; DOOM's game data, the PlayStation
soundtrack and the PlayStation sound effects are not, and none of them is in
this repository or its downloads.
