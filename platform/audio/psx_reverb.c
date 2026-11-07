/* The PlayStation SPU's reverb, psx_reverb.h. From the psx-spx reference
 * ("SPU Reverb Formula", "SPU Reverb Examples"): every 22050 Hz cycle,
 *
 *   [mLSAME] = (Lin + [dLSAME]*vWALL - [mLSAME-2])*vIIR + [mLSAME-2]   L to L
 *   [mRSAME] = (Rin + [dRSAME]*vWALL - [mRSAME-2])*vIIR + [mRSAME-2]   R to R
 *   [mLDIFF] = (Lin + [dRDIFF]*vWALL - [mLDIFF-2])*vIIR + [mLDIFF-2]   R to L
 *   [mRDIFF] = (Rin + [dLDIFF]*vWALL - [mRDIFF-2])*vIIR + [mRDIFF-2]   L to R
 *   Lout = vCOMB1*[mLCOMB1] + vCOMB2*[mLCOMB2] + vCOMB3*[mLCOMB3] + vCOMB4*[mLCOMB4]
 *   Lout = Lout - vAPF1*[mLAPF1-dAPF1], [mLAPF1] = Lout, Lout = Lout*vAPF1 + [mLAPF1-dAPF1]
 *   Lout = Lout - vAPF2*[mLAPF2-dAPF2], [mLAPF2] = Lout, Lout = Lout*vAPF2 + [mLAPF2-dAPF2]
 *   (Rout the same), out = Lout*EVOLL, Rout*EVOLR; the buffer address steps on
 *
 * with Lin = vLIN*left input, Rin = vRIN*right input. Addresses are in
 * 8-byte units relative to the moving buffer address, so a register's value
 * is 4 samples; "-2" (bytes) is the sample before. The buffer is the
 * preset's size at the top of the SPU's sound RAM, wrapping. Volumes are
 * signed 16-bit over 0x8000; memory saturates as 16-bit samples do.
 *
 * The SPU halves its input to 22050 Hz and doubles its output back through
 * FIR filters; here a pair average in and a linear step out. */
#include <stdlib.h>
#include <string.h>
#include "psx_reverb.h"

/* Register order (1F801DC0h on): */
enum {
    dAPF1, dAPF2, vIIR, vCOMB1, vCOMB2, vCOMB3, vCOMB4, vWALL, vAPF1, vAPF2,
    mLSAME, mRSAME, mLCOMB1, mRCOMB1, mLCOMB2, mRCOMB2, dLSAME, dRSAME, mLDIFF, mRDIFF,
    mLCOMB3, mRCOMB3, mLCOMB4, mRCOMB4, dLDIFF, dRDIFF, mLAPF1, mRAPF1, mLAPF2, mRAPF2, vLIN, vRIN,
    REGISTERS
};
typedef struct { unsigned size; unsigned short r[REGISTERS]; } preset_t;
/* psx-spx's presets, size in bytes. PlayStation DOOM's STUDIO_A/B/C are the
   small, medium and large studios, SPACE the space echo. */
static const preset_t presets[PSX_REVERB_PRESETS] = {
    [PSX_REVERB_OFF] = { 0x10, {0,0,0,0,0,0,0,0,0,0, 1,1,1,1,1,1,0,0,1,1, 1,1,1,1,0,0,1,1,1,1,0,0} },
    [PSX_REVERB_ROOM] = { 0x26C0, {0x007D,0x005B,0x6D80,0x54B8,0xBED0,0x0000,0x0000,0xBA80,0x5800,0x5300,
        0x04D6,0x0333,0x03F0,0x0227,0x0374,0x01EF,0x0334,0x01B5,0x0000,0x0000,
        0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x01B4,0x0136,0x00B8,0x005C,0x8000,0x8000} },
    [PSX_REVERB_STUDIO_A] = { 0x1F40, {0x0033,0x0025,0x70F0,0x4FA8,0xBCE0,0x4410,0xC0F0,0x9C00,0x5280,0x4EC0,
        0x03E4,0x031B,0x03A4,0x02AF,0x0372,0x0266,0x031C,0x025D,0x025C,0x018E,
        0x022F,0x0135,0x01D2,0x00B7,0x018F,0x00B5,0x00B4,0x0080,0x004C,0x0026,0x8000,0x8000} },
    [PSX_REVERB_STUDIO_B] = { 0x4840, {0x00B1,0x007F,0x70F0,0x4FA8,0xBCE0,0x4510,0xBEF0,0xB4C0,0x5280,0x4EC0,
        0x0904,0x076B,0x0824,0x065F,0x07A2,0x0616,0x076C,0x05ED,0x05EC,0x042E,
        0x050F,0x0305,0x0462,0x02B7,0x042F,0x0265,0x0264,0x01B2,0x0100,0x0080,0x8000,0x8000} },
    [PSX_REVERB_STUDIO_C] = { 0x6FE0, {0x00E3,0x00A9,0x6F60,0x4FA8,0xBCE0,0x4510,0xBEF0,0xA680,0x5680,0x52C0,
        0x0DFB,0x0B58,0x0D09,0x0A3C,0x0BD9,0x0973,0x0B59,0x08DA,0x08D9,0x05E9,
        0x07EC,0x04B0,0x06EF,0x03D2,0x05EA,0x031D,0x031C,0x0238,0x0154,0x00AA,0x8000,0x8000} },
    [PSX_REVERB_HALL] = { 0xADE0, {0x01A5,0x0139,0x6000,0x5000,0x4C00,0xB800,0xBC00,0xC000,0x6000,0x5C00,
        0x15BA,0x11BB,0x14C2,0x10BD,0x11BC,0x0DC1,0x11C0,0x0DC3,0x0DC0,0x09C1,
        0x0BC4,0x07C1,0x0A00,0x06CD,0x09C2,0x05C1,0x05C0,0x041A,0x0274,0x013A,0x8000,0x8000} },
    [PSX_REVERB_SPACE] = { 0xF6C0, {0x033D,0x0231,0x7E00,0x5000,0xB400,0xB000,0x4C00,0xB000,0x6000,0x5400,
        0x1ED6,0x1A31,0x1D14,0x183B,0x1BC2,0x16B2,0x1A32,0x15EF,0x15EE,0x1055,
        0x1334,0x0F2D,0x11F6,0x0C5D,0x1056,0x0AE1,0x0AE0,0x07A2,0x0464,0x0232,0x8000,0x8000} },
    [PSX_REVERB_ECHO] = { 0x18040, {0x0001,0x0001,0x7FFF,0x7FFF,0x0000,0x0000,0x0000,0x8100,0x0000,0x0000,
        0x1FFF,0x0FFF,0x1005,0x0005,0x0000,0x0000,0x1005,0x0005,0x0000,0x0000,
        0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x1004,0x1002,0x0004,0x0002,0x8000,0x8000} },
    [PSX_REVERB_DELAY] = { 0x18040, {0x0001,0x0001,0x7FFF,0x7FFF,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
        0x1FFF,0x0FFF,0x1005,0x0005,0x0000,0x0000,0x1005,0x0005,0x0000,0x0000,
        0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x1004,0x1002,0x0004,0x0002,0x8000,0x8000} },
    [PSX_REVERB_HALF_ECHO] = { 0x3C00, {0x0017,0x0013,0x70F0,0x4FA8,0xBCE0,0x4510,0xBEF0,0x8500,0x5F80,0x54C0,
        0x0371,0x02AF,0x02E5,0x01DF,0x02B0,0x01D7,0x0358,0x026A,0x01D6,0x011E,
        0x012D,0x00B1,0x011F,0x0059,0x01A0,0x00E3,0x0058,0x0040,0x0028,0x0014,0x8000,0x8000} },
};
/* The largest preset's buffer, in samples. */
#define BUFFER_SAMPLES (0x18040 / 2)

struct psx_reverb_s {
    float buffer[BUFFER_SAMPLES];
    unsigned length, at;         /* the preset's buffer in samples, the moving address */
    float v[REGISTERS];          /* volumes as fractions */
    unsigned m[REGISTERS];       /* addresses in samples */
    float evol;                  /* the depth: reverb output volume */
    int preset;
    /* Halving and doubling the rate: the first input sample of a pair, and
       the last two outputs to step between. */
    int half;
    float held_left, held_right, last_left, last_right, out_left, out_right;
};

static float clamp16(float x) { return x < -1 ? -1 : x > 32767.0f / 32768.0f ? 32767.0f / 32768.0f : x; }
static float *cell(psx_reverb_t *r, unsigned offset) { return &r->buffer[(r->at + offset) % r->length]; }
static float get(psx_reverb_t *r, unsigned offset) { return *cell(r, offset); }
/* [address-2]: the sample before. */
static float before(psx_reverb_t *r, unsigned offset) { return *cell(r, offset + r->length - 1); }

psx_reverb_t *PsxReverb_New(void)
{
    psx_reverb_t *r = calloc(1, sizeof *r);
    if (r) { r->preset = -1; PsxReverb_Set(r, PSX_REVERB_OFF, 0); }
    return r;
}
void PsxReverb_Free(psx_reverb_t *reverb) { free(reverb); }
void PsxReverb_Clear(psx_reverb_t *r)
{
    memset(r->buffer, 0, sizeof r->buffer);
    r->at = 0; r->half = 0;
    r->held_left = r->held_right = r->last_left = r->last_right = r->out_left = r->out_right = 0;
}
void PsxReverb_Set(psx_reverb_t *r, int preset, int depth)
{
    if (preset < 0 || preset >= PSX_REVERB_PRESETS) preset = PSX_REVERB_OFF;
    r->evol = (depth < 0 ? 0 : depth > 0x7FFF ? 0x7FFF : depth) / 32768.0f;
    if (preset == r->preset) return;
    r->preset = preset;
    const preset_t *p = &presets[preset];
    r->length = p->size / 2;
    for (int i = 0; i < REGISTERS; ++i)
    {
        r->v[i] = (short)p->r[i] / 32768.0f;
        r->m[i] = ((unsigned)p->r[i] * 4u) % r->length;
    }
    PsxReverb_Clear(r);
}
/* One 22050 Hz cycle. */
static void cycle(psx_reverb_t *r, float in_left, float in_right, float *out_left, float *out_right)
{
    const float *v = r->v; const unsigned *m = r->m;
    float lin = v[vLIN] * in_left, rin = v[vRIN] * in_right;
    float lsame = clamp16((lin + get(r, m[dLSAME]) * v[vWALL] - before(r, m[mLSAME])) * v[vIIR] + before(r, m[mLSAME]));
    float rsame = clamp16((rin + get(r, m[dRSAME]) * v[vWALL] - before(r, m[mRSAME])) * v[vIIR] + before(r, m[mRSAME]));
    float ldiff = clamp16((lin + get(r, m[dRDIFF]) * v[vWALL] - before(r, m[mLDIFF])) * v[vIIR] + before(r, m[mLDIFF]));
    float rdiff = clamp16((rin + get(r, m[dLDIFF]) * v[vWALL] - before(r, m[mRDIFF])) * v[vIIR] + before(r, m[mRDIFF]));
    *cell(r, m[mLSAME]) = lsame; *cell(r, m[mRSAME]) = rsame;
    *cell(r, m[mLDIFF]) = ldiff; *cell(r, m[mRDIFF]) = rdiff;
    float l = v[vCOMB1] * get(r, m[mLCOMB1]) + v[vCOMB2] * get(r, m[mLCOMB2]) + v[vCOMB3] * get(r, m[mLCOMB3]) + v[vCOMB4] * get(r, m[mLCOMB4]);
    float rr = v[vCOMB1] * get(r, m[mRCOMB1]) + v[vCOMB2] * get(r, m[mRCOMB2]) + v[vCOMB3] * get(r, m[mRCOMB3]) + v[vCOMB4] * get(r, m[mRCOMB4]);
    unsigned length = r->length;
    float t;
    t = get(r, (m[mLAPF1] + length - m[dAPF1]) % length); l = clamp16(l - v[vAPF1] * t); *cell(r, m[mLAPF1]) = l; l = l * v[vAPF1] + t;
    t = get(r, (m[mRAPF1] + length - m[dAPF1]) % length); rr = clamp16(rr - v[vAPF1] * t); *cell(r, m[mRAPF1]) = rr; rr = rr * v[vAPF1] + t;
    t = get(r, (m[mLAPF2] + length - m[dAPF2]) % length); l = clamp16(l - v[vAPF2] * t); *cell(r, m[mLAPF2]) = l; l = l * v[vAPF2] + t;
    t = get(r, (m[mRAPF2] + length - m[dAPF2]) % length); rr = clamp16(rr - v[vAPF2] * t); *cell(r, m[mRAPF2]) = rr; rr = rr * v[vAPF2] + t;
    *out_left = clamp16(l) * r->evol;
    *out_right = clamp16(rr) * r->evol;
    r->at = (r->at + 1) % length;
}
void PsxReverb_Process(psx_reverb_t *r, const float *left, const float *right, float *stereo, int frames, float amount)
{
    if (r->preset == PSX_REVERB_OFF || r->evol <= 0) return;
    for (int i = 0; i < frames; ++i)
    {
        if (!r->half) { r->held_left = left[i]; r->held_right = right[i]; }
        else
        {
            r->last_left = r->out_left; r->last_right = r->out_right;
            cycle(r, 0.5f * (r->held_left + left[i]), 0.5f * (r->held_right + right[i]), &r->out_left, &r->out_right);
        }
        /* Halfway between the last two outputs on the first of a pair, on the newest on the second. */
        float t = r->half ? 1.0f : 0.5f;
        stereo[2 * i] += amount * (r->last_left + (r->out_left - r->last_left) * t);
        stereo[2 * i + 1] += amount * (r->last_right + (r->out_right - r->last_right) * t);
        r->half ^= 1;
    }
}
