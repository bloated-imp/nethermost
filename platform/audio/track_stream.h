/* Recorded music tracks the player supplies (FLAC, Ogg Vorbis, MP3 or WAV),
   streamed from disk and decoded as they play, resampled to the mixer's rate.
   None ship with the game: see platform/user_music.c for where they are
   looked for. Opening reads the file's header on the caller's thread; mixing
   runs on the audio thread with the music lock held. */
#ifndef DOOM_AUDIO_TRACK_STREAM_H
#define DOOM_AUDIO_TRACK_STREAM_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct track_stream_s track_stream_t;

/* NULL when the file is missing or not a format it reads. */
track_stream_t *Track_Open(const char *path);
void Track_Close(track_stream_t *track);
/* From the start; loop plays it round without a gap, else it ends once. */
void Track_Restart(track_stream_t *track, int loop);
/* Adds frames of interleaved stereo at rate, scaled by gain. Returns 0 once a
   track that doesn't loop has ended. */
int Track_Mix(track_stream_t *track, float *stereo, int frames, int rate, float gain);
/* The file's own sample rate and length in seconds (0 when it can't tell). */
int Track_Rate(const track_stream_t *track);
double Track_Seconds(const track_stream_t *track);

/* A whole sound file held in memory (FLAC, Ogg Vorbis, MP3 or WAV, told by
   its first bytes, so a file without an extension reads too), decoded to
   mono floats at its own rate; *mono is malloc'd, free() it. 0 when it
   isn't one of those. */
int Track_DecodeMemory(const void *data, size_t size, float **mono, int *frames, int *rate);

#ifdef __cplusplus
}
#endif
#endif
