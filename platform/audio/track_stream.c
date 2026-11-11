/* Streams a recorded track (track_stream.h). The decoders are dr_flac,
   dr_mp3 and dr_wav (David Reid, public domain or MIT-0) and stb_vorbis
   (Sean Barrett, public domain or MIT), downloaded by CMakeLists.txt. */
#include "track_stream.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define DR_FLAC_IMPLEMENTATION
#define DR_FLAC_NO_STDIO_WIDE
#include "dr_flac.h"
#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"
#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"
#define STB_VORBIS_NO_PUSHDATA_API
/* stb_vorbis uses alloca but includes its header only on Linux and Windows;
   Apple's clang rejects the undeclared call (the Mac build). */
#if !defined(_WIN32) && !defined(__linux__)
#include <alloca.h>
#endif
#include "stb_vorbis.c"

enum { KIND_FLAC, KIND_MP3, KIND_WAV, KIND_VORBIS };
#define CHUNK 1024			/* frames decoded at a time */
#define FADE_IN_SECONDS 0.02f		/* no click where it starts */

struct track_stream_s
{
    int kind, channels, rate, loop, ended;
    unsigned long long total;		/* frames, 0 unknown */
    drflac *flac; drmp3 mp3; drwav wav; stb_vorbis *vorbis;
    float decoded[CHUNK * 2];		/* stereo, from the file */
    int have, at;			/* frames in decoded, the next one */
    float previous[2], current[2];	/* the two frames read between */
    double phase;			/* position between previous and current, 0-1 */
    int primed;
    float fade;				/* fade-in gain, rising to 1 */
};

static int ends_with(const char *path, const char *suffix)
{
    size_t a = strlen(path), b = strlen(suffix);
    if (a < b) return 0;
    for (size_t i = 0; i < b; ++i)
    {
	char c = path[a - b + i];
	if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
	if (c != suffix[i]) return 0;
    }
    return 1;
}

track_stream_t *Track_Open(const char *path)
{
    track_stream_t *track = calloc(1, sizeof(*track));
    if (!track) return NULL;
    int ok = 0;
    if (ends_with(path, ".flac"))
    {
	track->kind = KIND_FLAC;
	track->flac = drflac_open_file(path, NULL);
	if ((ok = track->flac != NULL))
	{
	    track->channels = track->flac->channels;
	    track->rate = (int)track->flac->sampleRate;
	    track->total = track->flac->totalPCMFrameCount;
	}
    }
    else if (ends_with(path, ".mp3"))
    {
	track->kind = KIND_MP3;
	if ((ok = drmp3_init_file(&track->mp3, path, NULL)))
	{
	    track->channels = (int)track->mp3.channels;
	    track->rate = (int)track->mp3.sampleRate;
	}
    }
    else if (ends_with(path, ".wav"))
    {
	track->kind = KIND_WAV;
	if ((ok = drwav_init_file(&track->wav, path, NULL)))
	{
	    track->channels = track->wav.channels;
	    track->rate = (int)track->wav.sampleRate;
	    track->total = track->wav.totalPCMFrameCount;
	}
    }
    else if (ends_with(path, ".ogg"))
    {
	int error = 0;
	track->kind = KIND_VORBIS;
	track->vorbis = stb_vorbis_open_filename(path, &error, NULL);
	if ((ok = track->vorbis != NULL))
	{
	    stb_vorbis_info info = stb_vorbis_get_info(track->vorbis);
	    track->channels = info.channels;
	    track->rate = (int)info.sample_rate;
	    track->total = stb_vorbis_stream_length_in_samples(track->vorbis);
	}
    }
    if (!ok || track->channels < 1 || track->rate < 8000)
    {
	if (ok) { track->rate = 0; Track_Close(track); }
	else free(track);
	return NULL;
    }
    Track_Restart(track, 1);
    return track;
}

void Track_Close(track_stream_t *track)
{
    if (!track) return;
    switch (track->kind)
    {
    case KIND_FLAC: drflac_close(track->flac); break;
    case KIND_MP3: drmp3_uninit(&track->mp3); break;
    case KIND_WAV: drwav_uninit(&track->wav); break;
    case KIND_VORBIS: stb_vorbis_close(track->vorbis); break;
    }
    free(track);
}

static void seek_start(track_stream_t *track)
{
    switch (track->kind)
    {
    case KIND_FLAC: drflac_seek_to_pcm_frame(track->flac, 0); break;
    case KIND_MP3: drmp3_seek_to_pcm_frame(&track->mp3, 0); break;
    case KIND_WAV: drwav_seek_to_pcm_frame(&track->wav, 0); break;
    case KIND_VORBIS: stb_vorbis_seek_start(track->vorbis); break;
    }
}

void Track_Restart(track_stream_t *track, int loop)
{
    seek_start(track);
    track->loop = loop;
    track->ended = 0;
    track->have = track->at = 0;
    track->primed = 0;
    track->phase = 0;
    track->fade = 0;
}

/* Fills decoded with up to CHUNK stereo frames from the file. */
static int decode(track_stream_t *track)
{
    float raw[CHUNK * 8];
    int channels = track->channels > 8 ? 8 : track->channels;
    long got = 0;
    switch (track->kind)
    {
    case KIND_FLAC:
	if (track->channels > 8) return 0;
	got = (long)drflac_read_pcm_frames_f32(track->flac, CHUNK, raw);
	break;
    case KIND_MP3:
	if (track->channels > 8) return 0;
	got = (long)drmp3_read_pcm_frames_f32(&track->mp3, CHUNK, raw);
	break;
    case KIND_WAV:
	if (track->channels > 8) return 0;
	got = (long)drwav_read_pcm_frames_f32(&track->wav, CHUNK, raw);
	break;
    case KIND_VORBIS:
	got = stb_vorbis_get_samples_float_interleaved(track->vorbis, channels, raw, CHUNK * channels);
	break;
    }
    for (long i = 0; i < got; ++i)
    {
	float left = raw[i * channels], right = channels > 1 ? raw[i * channels + 1] : left;
	track->decoded[i * 2] = left;
	track->decoded[i * 2 + 1] = right;
    }
    track->have = (int)got;
    track->at = 0;
    return (int)got;
}

/* The next frame from the file, round to the start when it loops. */
static int next_frame(track_stream_t *track, float out[2])
{
    if (track->at >= track->have)
    {
	if (!decode(track))
	{
	    if (!track->loop) return 0;
	    seek_start(track);
	    if (!decode(track)) return 0;
	}
    }
    out[0] = track->decoded[track->at * 2];
    out[1] = track->decoded[track->at * 2 + 1];
    ++track->at;
    return 1;
}

int Track_Mix(track_stream_t *track, float *stereo, int frames, int rate, float gain)
{
    if (!track || track->ended) return 0;
    double step = (double)track->rate / rate;
    float fade_step = 1.0f / (FADE_IN_SECONDS * rate);
    if (!track->primed)
    {
	if (!next_frame(track, track->previous) || !next_frame(track, track->current))
	{
	    track->ended = 1;
	    return 0;
	}
	track->primed = 1;
    }
    for (int i = 0; i < frames; ++i)
    {
	/* Linear between the two frames either side: tracks are almost
	   always 44.1 kHz already, where it reads the file frame by frame. */
	float t = (float)track->phase;
	float left = track->previous[0] + (track->current[0] - track->previous[0]) * t;
	float right = track->previous[1] + (track->current[1] - track->previous[1]) * t;
	if (track->fade < 1) track->fade = fminf(1, track->fade + fade_step);
	stereo[i * 2] += left * gain * track->fade;
	stereo[i * 2 + 1] += right * gain * track->fade;
	track->phase += step;
	while (track->phase >= 1)
	{
	    track->phase -= 1;
	    track->previous[0] = track->current[0];
	    track->previous[1] = track->current[1];
	    if (!next_frame(track, track->current))
	    {
		track->ended = 1;
		return 0;
	    }
	}
    }
    return 1;
}

int Track_Rate(const track_stream_t *track) { return track ? track->rate : 0; }

double Track_Seconds(const track_stream_t *track)
{
    return track && track->total && track->rate ? (double)track->total / track->rate : 0;
}

static void decoded_free(int kind, float *pcm)
{
    if (!pcm) return;
    if (kind == KIND_FLAC) drflac_free(pcm, NULL);
    else if (kind == KIND_WAV) drwav_free(pcm, NULL);
    else drmp3_free(pcm, NULL);
}

/* A whole short file in memory, for sound effects (user_sounds.c): told by
   its first bytes, so files without an extension read too. */
int Track_DecodeMemory(const void *data, size_t size, float **mono, int *frames, int *rate)
{
    const unsigned char *b = data;
    float *mixed = NULL;
    unsigned int channels = 0, sr = 0;
    drflac_uint64 count = 0;
    *mono = NULL; *frames = 0; *rate = 0;
    if (!data || size < 12) return 0;
    if (!memcmp(b, "OggS", 4))
    {
	int ch = 0, hz = 0;
	short *pcm = NULL;
	int got = stb_vorbis_decode_memory(b, (int)size, &ch, &hz, &pcm);
	if (got <= 0 || ch <= 0 || !pcm) { free(pcm); return 0; }
	mixed = malloc((size_t)got * sizeof(float));
	if (mixed)
	    for (int i = 0; i < got; ++i)
	    {
		float sum = 0;
		for (int c = 0; c < ch; ++c) sum += pcm[(size_t)i * ch + c];
		mixed[i] = sum / (32768.0f * ch);
	    }
	free(pcm);
	if (!mixed) return 0;
	*mono = mixed; *frames = got; *rate = hz;
	return 1;
    }
    float *pcm = NULL;
    int kind;
    if (!memcmp(b, "fLaC", 4))
    {
	kind = KIND_FLAC;
	pcm = drflac_open_memory_and_read_pcm_frames_f32(b, size, &channels, &sr, &count, NULL);
    }
    else if (!memcmp(b, "RIFF", 4) && !memcmp(b + 8, "WAVE", 4))
    {
	kind = KIND_WAV;
	pcm = drwav_open_memory_and_read_pcm_frames_f32(b, size, &channels, &sr, (drwav_uint64 *)&count, NULL);
    }
    else
    {
	drmp3_config config = {0};
	kind = KIND_MP3;
	pcm = drmp3_open_memory_and_read_pcm_frames_f32(b, size, &config, (drmp3_uint64 *)&count, NULL);
	channels = config.channels; sr = config.sampleRate;
    }
    if (!pcm || !channels || !sr || !count || count > (1u << 26)) { decoded_free(kind, pcm); return 0; }
    mixed = malloc((size_t)count * sizeof(float));
    if (mixed)
	for (drflac_uint64 i = 0; i < count; ++i)
	{
	    float sum = 0;
	    for (unsigned c = 0; c < channels; ++c) sum += pcm[i * channels + c];
	    mixed[i] = sum / channels;
	}
    decoded_free(kind, pcm);
    if (!mixed) return 0;
    *mono = mixed; *frames = (int)count; *rate = (int)sr;
    return 1;
}
