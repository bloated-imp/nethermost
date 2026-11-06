/* Sound effects the player supplies (user_sounds.c). */
#ifndef DOOM_USER_SOUNDS_H
#define DOOM_USER_SOUNDS_H

/* A sound to play: pcm (mono floats, a file the player supplied) or dmx (the
   WAD's own 8-bit lump data), length samples at rate. */
typedef struct
{
    const float *pcm;
    const unsigned char *dmx;
    int length, rate;
} user_sound_t;

/* Looks through the sound folders; once, before the others. */
void UserSounds_Init(void);
/* A file the player supplies for a sound lump (DSPISTOL); 0 when there is none. */
int UserSounds_Lump(const char *lump, user_sound_t *out);
/* A name from the packs' SNDINFO (imp/pain), resolved (a $random set picks
   one) to a supplied file, or else to the WAD's lump of that name; 0 when
   no SNDINFO defines it or what it names can't be found. */
int UserSounds_Named(const char *name, user_sound_t *out);
#endif
