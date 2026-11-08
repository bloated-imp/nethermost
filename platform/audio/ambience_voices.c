/* Plays the ambient loops: each voice reads its own take of its kind's loop
 * from the made bank, wrapping round its end, through the same two-stage
 * lowpass the effects use for muffling. A kind's layers differ by their
 * takes and where in them they start, not by pitch, which would beat. Gains
 * glide over a twentieth of a second. */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "ambience_voices.h"
#include "made_bank.h"
#include "resample.h"
#include "wind_voice.h"

#define GLIDE_SECONDS 0.05f
#define MUFFLE_GLIDE_SECONDS 0.08f
#define COMPUTER_FADE_SECONDS 0.12f
/* Split the capped computer presence before the common palette level. */
#define COMPUTER_BODY_SHARE 0.55f
#define COMPUTER_ACTIVITY_SHARE 0.83f
/* The original bleep playback level underplays a continuous machine texture.
   Raise only this palette by 8 dB, after the shared presence cap is applied. */
#define COMPUTER_PLAYBACK_GAIN 2.51188643f
#define PI_F 3.14159265f

typedef struct {
    double position;
    float left, right, send, cutoff;              /* where it is */
    float to_left, to_right, to_send, to_cutoff;  /* where it goes */
    float low[2];
    int started;
    int step, to_step;   /* wind and rain: the strength's take it plays, and the one it is to */
} voice_t;
static voice_t voices[AMBIENT_KINDS][AMBIENCE_LAYERS];
static voice_t computer_activity[AMBIENCE_LAYERS], computer_body;
static int alternative_computers;
/* A kind's loop the player supplies (AmbienceVoices_SupplyLoop), and whether
   those are in use. */
static made_sample_t supplied_loops[AMBIENT_KINDS];
static int use_supplied = 1;
static float computer_blend;

static void target(voice_t *v, const ambience_voice_t *t, float scale)
{
    v->to_left = scale * t->left; v->to_right = scale * t->right; v->to_send = scale * t->send;
    if (t->cutoff > 0) v->to_cutoff = t->cutoff;
}

void AmbienceVoices_Set(const ambience_voice_t targets[AMBIENT_KINDS][AMBIENCE_LAYERS])
{
    for (int k = 0; k < AMBIENT_KINDS; ++k)
        for (int l = 0; l < AMBIENCE_LAYERS; ++l)
        {
            voice_t *v = &voices[k][l];
            const ambience_voice_t *t = &targets[k][l];
            v->to_left = t->left; v->to_right = t->right; v->to_send = t->send;
            if (t->cutoff > 0) v->to_cutoff = t->cutoff;
            v->to_step = t->step;
            if (k == AMBIENT_COMPUTER) target(&computer_activity[l], t, COMPUTER_ACTIVITY_SHARE * COMPUTER_PLAYBACK_GAIN);
        }
}

void AmbienceVoices_SetComputerBody(const ambience_voice_t *body)
{
    ambience_voice_t silent = {0};
    target(&computer_body, body ? body : &silent, COMPUTER_BODY_SHARE * COMPUTER_PLAYBACK_GAIN);
}

void AmbienceVoices_AlternativeComputer(int enabled) { alternative_computers = enabled != 0; }

void AmbienceVoices_UseSupplied(int enabled) { use_supplied = enabled != 0; }

/* The bus runs at the loops' rate, so a recording is averaged down to it;
   its ends are crossfaded so the loop has no seam; and it is brought to the
   loudness of the loop it replaces, so the balance holds. */
void AmbienceVoices_SupplyLoop(int kind, const float *pcm, int length, int rate)
{
    if (kind < 0 || kind >= AMBIENT_KINDS || !pcm || length < 2 || rate <= 0) return;
    double step = rate / (double)MADE_RATE;
    int frames = (int)(length / step);
    float *out = malloc((size_t)frames * sizeof *out);
    if (!out) return;
    for (int i = 0; i < frames; ++i)
    {
        int from = (int)(i * step), to = (int)((i + 1) * step);
        if (to > length) to = length;
        if (to <= from) to = from + 1;
        float sum = 0;
        for (int j = from; j < to; ++j) sum += pcm[j];
        out[i] = sum / (to - from);
    }
    int fade = MADE_RATE / 10;
    if (fade * 2 < frames)
    {
        for (int i = 0; i < fade; ++i)
        {
            float in = sqrtf((float)i / fade), away = sqrtf(1 - (float)i / fade);
            out[i] = out[i] * in + out[frames - fade + i] * away;
        }
        frames -= fade;
    }
    float bank = 0, own = 0, peak = 0;
    int counted = 0;
    for (int l = 0; l < AMBIENCE_LAYERS; ++l)
    {
        const made_sample_t *loop = MadeBank_Loop(kind, l);
        for (int i = 0; loop && loop->data && i < loop->length; ++i)
        {
            float value = (loop->data[i] - 128) / 128.0f;
            bank += value * value;
            ++counted;
        }
    }
    for (int i = 0; i < frames; ++i) { own += out[i] * out[i]; peak = fmaxf(peak, fabsf(out[i])); }
    if (counted && own > 0)
    {
        float gain = sqrtf((bank / counted) / (own / frames));
        if (kind == AMBIENT_COMPUTER) gain *= 1.3f;   /* a steady sound under the bleeps' average sat too low */
        if (peak * gain > 0.98f) gain = 0.98f / peak;
        for (int i = 0; i < frames; ++i) out[i] *= gain;
    }
    supplied_loops[kind] = (made_sample_t){ NULL, frames, MADE_RATE, out };
}

static void silence(voice_t *v)
{
    v->left = v->right = v->send = v->to_left = v->to_right = v->to_send = 0;
}

void AmbienceVoices_Clear(void)
{
    for (int k = 0; k < AMBIENT_KINDS; ++k)
        for (int l = 0; l < AMBIENCE_LAYERS; ++l)
        {
            silence(&voices[k][l]);
        }
    for (int l = 0; l < AMBIENCE_LAYERS; ++l) silence(&computer_activity[l]);
    silence(&computer_body);
    WindVoice_Clear();
}

/* The ambience is mixed at the loops' own rate, then the sum - left, right
   and reverb send - is read up to the output rate once, the way the chosen
   effect playback reads a sample. Four times fewer samples per voice, and a
   sinc reading three channels instead of every voice's. The wind has three of
   its own, always read clean. */
#define BUS 2048
#define BUS_CHANNELS 6
static float bus[BUS_CHANNELS][BUS];
static int filled;
static double read_at = RESAMPLE_REACH;

/* Hidden palettes keep their cursor but must not retain the loudness or
   muffling of a computer the listener has since left or destroyed. Their
   next audible transition is already covered by the palette crossfade. */
static void follow_hidden(voice_t *v)
{
    v->left = v->to_left; v->right = v->to_right; v->send = v->to_send;
    v->cutoff = v->to_cutoff > 0 ? v->to_cutoff : 20000;
    v->low[0] = v->low[1] = 0;
}

/* Runs a voice for n samples at the loops' rate, adding it to the bus from
   at. Open, it skips the muffling filter, which can't be quite open at 11 kHz. */
static void voice_run(voice_t *v, const made_sample_t *loop, int at, int n, float weight_from, float weight_to)
{
    if (weight_from + weight_to <= 0) { follow_hidden(v); return; }
    if (v->left + v->right + v->to_left + v->to_right < 1e-5f) { v->left = v->right = v->send = 0; return; }
    if (v->to_cutoff <= 0) v->to_cutoff = 20000;
    float glide = 1 - expf(-n / (GLIDE_SECONDS * MADE_RATE)), muffle_glide = 1 - expf(-n / (MUFFLE_GLIDE_SECONDS * MADE_RATE));
    /* Linear across the run toward the glided value: no steps. */
    float left = v->left + (v->to_left - v->left) * glide, right = v->right + (v->to_right - v->right) * glide;
    float sendto = v->send + (v->to_send - v->send) * glide;
    v->cutoff *= powf(v->to_cutoff / v->cutoff, muffle_glide);
    int open = v->cutoff >= 0.45f * MADE_RATE;
    float keep = expf(-2 * PI_F * fminf(v->cutoff, 0.45f * MADE_RATE) / MADE_RATE);
    int position = (int)v->position % loop->length;
    for (int i = 0; i < n; ++i)
    {
        float x = (i + 1) / (float)n, value = loop->pcm ? loop->pcm[position] : (loop->data[position] - 128) / 128.0f;
        v->low[0] = value + (v->low[0] - value) * keep;
        v->low[1] = v->low[0] + (v->low[1] - v->low[0]) * keep;
        if (!open) value = v->low[1];
        value *= weight_from + (weight_to - weight_from) * x;
        bus[0][at + i] += value * (v->left + (left - v->left) * x);
        bus[1][at + i] += value * (v->right + (right - v->right) * x);
        bus[2][at + i] += value * (v->send + (sendto - v->send) * x);
        if (++position >= loop->length) position = 0;
    }
    v->position = position;
    v->left = left; v->right = right; v->send = sendto;
}

static void start(voice_t *v, const made_sample_t *loop, int offset)
{
    if (v->started) return;
    v->position = offset % loop->length;
    v->cutoff = v->to_cutoff > 0 ? v->to_cutoff : 20000;
    v->started = 1;
}

/* Every voice for n more samples at the end of the bus. */
static void bus_run(int n)
{
    for (int c = 0; c < BUS_CHANNELS; ++c) memset(bus[c] + filled, 0, (size_t)n * sizeof(float));
    WindVoice_Run(bus[3] + filled, bus[4] + filled, bus[5] + filled, n);
    float next_blend = alternative_computers ? fminf(1, computer_blend + n / (COMPUTER_FADE_SECONDS * MADE_RATE))
                                            : fmaxf(0, computer_blend - n / (COMPUTER_FADE_SECONDS * MADE_RATE));
    for (int k = 0; k < AMBIENT_KINDS; ++k)
        for (int l = 0; l < AMBIENCE_LAYERS; ++l)
        {
            voice_t *v = &voices[k][l];
            /* Wind and rain: a layer takes up another strength's take only
               once it has faded out (ambience.c plays each where it is silent
               as the strength passes; a jump in strength fades it out first). */
            int stepped = Ambient_Stepped(k), swapping = stepped && v->step != v->to_step;
            if (swapping && v->left + v->right + v->send < 1e-3f) { v->step = v->to_step; v->started = 0; swapping = 0; }
            const made_sample_t *loop = stepped ? MadeBank_StepLoop(k, v->step)
                                     : use_supplied && supplied_loops[k].pcm ? &supplied_loops[k] : MadeBank_Loop(k, l);
            if (!loop) continue;
            /* Each starts somewhere else in its loop; muffling starts where it
               should be. Layers of one recording start far apart, or two
               nearby machines playing it almost together would comb. */
            start(v, loop, loop->pcm ? l * (loop->length / AMBIENCE_LAYERS) : k * 7919 + l * 3571);
            float to[3] = { v->to_left, v->to_right, v->to_send };
            if (swapping) v->to_left = v->to_right = v->to_send = 0;
            voice_run(v, loop, filled, n, k == AMBIENT_COMPUTER ? 1 - computer_blend : 1,
                                        k == AMBIENT_COMPUTER ? 1 - next_blend : 1);
            if (swapping) { v->to_left = to[0]; v->to_right = to[1]; v->to_send = to[2]; }
        }
    if (computer_blend + next_blend > 0)
    {
        for (int l = 0; l < AMBIENCE_LAYERS; ++l)
        {
            const made_sample_t *loop = MadeBank_ComputerActivity(l);
            if (!loop) continue;
            start(&computer_activity[l], loop, AMBIENT_COMPUTER * 7919 + l * 3571);
            voice_run(&computer_activity[l], loop, filled, n, computer_blend, next_blend);
        }
        const made_sample_t *body = MadeBank_ComputerBody();
        if (body)
        {
            start(&computer_body, body, AMBIENT_COMPUTER * 7919);
            voice_run(&computer_body, body, filled, n, computer_blend, next_blend);
        }
    }
    else
    {
        for (int l = 0; l < AMBIENCE_LAYERS; ++l) follow_hidden(&computer_activity[l]);
        follow_hidden(&computer_body);
    }
    computer_blend = next_blend;
    filled += n;
}

void AmbienceVoices_Mix(float *stereo, float *send, int frames, int rate, int resampling, float gain, float duck)
{
    double ratio = MADE_RATE / (double)rate;
    for (int done = 0; done < frames;)
    {
        int count = frames - done < 256 ? frames - done : 256;
        /* Mixed far enough ahead for the reading's reach past the last frame. */
        int need = (int)(read_at + count * ratio) + RESAMPLE_REACH + 2;
        if (need > filled) bus_run(need - filled);
        int wind = WindVoice_Sounding();
        for (int i = 0; i < count; ++i)
        {
            double at = read_at + i * ratio;
            float scale = gain * duck;
            stereo[2 * (done + i)] += scale * Resample_AtFloat(bus[0], filled, at, resampling);
            stereo[2 * (done + i) + 1] += scale * Resample_AtFloat(bus[1], filled, at, resampling);
            send[done + i] += scale * Resample_AtFloat(bus[2], filled, at, resampling);
            if (!wind) continue;
            stereo[2 * (done + i)] += scale * Resample_AtFloat(bus[3], filled, at, RESAMPLE_CLEAN);
            stereo[2 * (done + i) + 1] += scale * Resample_AtFloat(bus[4], filled, at, RESAMPLE_CLEAN);
            send[done + i] += scale * Resample_AtFloat(bus[5], filled, at, RESAMPLE_CLEAN);
        }
        read_at += count * ratio;
        /* Keep only what the next readings reach back to. */
        int drop = (int)read_at - RESAMPLE_REACH - 1;
        if (drop > 0)
        {
            for (int c = 0; c < BUS_CHANNELS; ++c) memmove(bus[c], bus[c] + drop, (size_t)(filled - drop) * sizeof(float));
            filled -= drop; read_at -= drop;
        }
        done += count;
    }
}
