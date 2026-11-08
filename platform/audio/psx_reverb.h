/* The PlayStation's reverb (F4 Nethermost, PS1 reverb): the SPU's own reverb unit,
 * as the psx-spx reference documents it, with its preset register sets. It
 * runs at the SPU's 22050 Hz on its own echo buffer, in place of the
 * measured rooms (reverb.h), with each map's preset and depth from the
 * PlayStation's (Game_PsxReverb). Runs in the mixer with the audio lock held. */
#ifndef DOOM_AUDIO_PSX_REVERB_H
#define DOOM_AUDIO_PSX_REVERB_H

/* The SPU's presets, as PlayStation DOOM names them (SPU_REV_MODE_*). */
enum {
    PSX_REVERB_OFF, PSX_REVERB_ROOM, PSX_REVERB_STUDIO_A, PSX_REVERB_STUDIO_B, PSX_REVERB_STUDIO_C,
    PSX_REVERB_HALL, PSX_REVERB_SPACE, PSX_REVERB_ECHO, PSX_REVERB_DELAY, PSX_REVERB_HALF_ECHO,
    PSX_REVERB_PRESETS
};

typedef struct psx_reverb_s psx_reverb_t;

/* A reverb ringing nothing, preset off; NULL without memory. */
psx_reverb_t *PsxReverb_New(void);
void PsxReverb_Free(psx_reverb_t *reverb);
/* A preset (PSX_REVERB_*) and its depth, the SPU's reverb output volume
   (0-0x7FFF, as PlayStation DOOM's maps give it). A new preset starts from
   an empty buffer, as the SPU's does when the game sets it. */
void PsxReverb_Set(psx_reverb_t *reverb, int preset, int depth);
/* Forgets everything still ringing. */
void PsxReverb_Clear(psx_reverb_t *reverb);
/* Adds the reverb of a stereo send (left, right; at the 44100 Hz output
   rate) to interleaved stereo, scaled by amount. */
void PsxReverb_Process(psx_reverb_t *reverb, const float *left, const float *right, float *stereo, int frames,
                       float amount);
#endif
