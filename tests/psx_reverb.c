/* The PlayStation reverb (platform/audio/psx_reverb.c): a click through each
   preset PlayStation DOOM used rings out (an answer some tens of
   milliseconds on), stays finite and within range, decays, and the presets
   last in the order their buffers suggest: the small studio shortest, the
   Space echo longest. Off is silent. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "psx_reverb.h"

#define RATE 44100
#define SECONDS 6

static int failures;
static void expect(int ok, const char *what, const char *preset)
{
    if (!ok) { fprintf(stderr, "FAIL %s: %s\n", preset, what); ++failures; }
}

/* Seconds until the tail's energy (in 50 ms windows) falls 40 dB below its peak; -1 if it never does. */
static double ring(int preset, const char *name, double *peak_out)
{
    psx_reverb_t *r = PsxReverb_New();
    if (!r) { fprintf(stderr, "no memory\n"); exit(1); }
    PsxReverb_Set(r, preset, 0x2FFF);
    int frames = RATE * SECONDS, window = RATE / 20;
    float *left = calloc((size_t)frames, sizeof *left), *right = calloc((size_t)frames, sizeof *right);
    float *out = calloc((size_t)frames * 2, sizeof *out);
    left[0] = right[0] = left[1] = right[1] = 0.8f;
    /* In blocks, as the mixer calls it. */
    for (int at = 0; at < frames; at += 512)
    {
        int count = frames - at < 512 ? frames - at : 512;
        PsxReverb_Process(r, left + at, right + at, out + 2 * at, count, 1);
    }
    double peak = 0, decayed = -1, first = -1, largest = 0;
    int finite = 1;
    for (int w = 0; w + window <= frames; w += window)
    {
        double energy = 0;
        for (int i = w; i < w + window; ++i)
        {
            float a = out[2 * i], b = out[2 * i + 1];
            if (!isfinite(a) || !isfinite(b)) finite = 0;
            largest = fmax(largest, fmax(fabs(a), fabs(b)));
            energy += (double)a * a + (double)b * b;
        }
        if (energy > 1e-12 && first < 0) first = (double)w / RATE;
        if (energy > peak) peak = energy;
        else if (decayed < 0 && peak > 0 && energy < peak * 1e-4) decayed = (double)w / RATE;
    }
    expect(finite, "output is finite", name);
    expect(largest <= 1.0, "output stays within range", name);
    expect(peak > 1e-8, "it rings", name);
    expect(decayed > 0, "it decays 40 dB within the run", name);
    printf("%-10s answers after %.2f s, 40 dB down after %.2f s, loudest %.3f\n", name, first, decayed, largest);
    *peak_out = peak;
    free(left); free(right); free(out);
    PsxReverb_Free(r);
    return decayed;
}

int main(void)
{
    double peak;
    double studio_a = ring(PSX_REVERB_STUDIO_A, "studio A", &peak);
    double studio_b = ring(PSX_REVERB_STUDIO_B, "studio B", &peak);
    double studio_c = ring(PSX_REVERB_STUDIO_C, "studio C", &peak);
    double hall = ring(PSX_REVERB_HALL, "hall", &peak);
    double space = ring(PSX_REVERB_SPACE, "space", &peak);
    expect(studio_a <= studio_c, "the small studio no longer than the large", "studio");
    expect(studio_b <= hall && hall <= space, "medium studio, hall, space in rising length", "lengths");

    /* Off: nothing at all. */
    psx_reverb_t *r = PsxReverb_New();
    float in[512] = {0.8f}, out[1024] = {0};
    PsxReverb_Set(r, PSX_REVERB_OFF, 0x2FFF);
    PsxReverb_Process(r, in, in, out, 512, 1);
    double sum = 0;
    for (int i = 0; i < 1024; ++i) sum += fabs(out[i]);
    expect(sum == 0, "off adds nothing", "off");
    PsxReverb_Free(r);
    if (failures) return 1;
    printf("PS1 reverb: all presets ring and decay.\n");
    return 0;
}
