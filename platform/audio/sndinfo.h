/* SNDINFO, the sound list ZDoom-family ports read from a sound pack: names
   such as imp/pain given a sound file, $random sets and $alias. Only what
   picks a sound is read; volume, pitch, limits and player sounds are skipped.
   Used by user_sounds.c. */
#ifndef DOOM_AUDIO_SNDINFO_H
#define DOOM_AUDIO_SNDINFO_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct sndinfo_s sndinfo_t;

sndinfo_t *SndInfo_New(void);
void SndInfo_Free(sndinfo_t *info);
/* Adds a SNDINFO text's definitions; a name defined again, here or by a
   later text, takes the later definition. Returns how many it read. */
int SndInfo_Parse(sndinfo_t *info, const char *text, size_t length);
/* The sound a name plays, as the file name it gives without folder or
   extension (DSBGPAI2), through aliases and $random sets, *seed picking
   among a set's members. NULL when the name isn't defined. */
const char *SndInfo_Resolve(const sndinfo_t *info, const char *name, unsigned *seed);
int SndInfo_Count(const sndinfo_t *info);

#ifdef __cplusplus
}
#endif
#endif
