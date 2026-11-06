/* Music tracks the player supplies: finding them (user_music.c) and
   streaming them (audio/track_stream.c). Builds its own WAV files; give it
   more files (FLAC, Ogg, MP3) on the command line to check those decoders. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#define chdir _chdir
#else
#include <unistd.h>
#endif
#include <SDL3/SDL.h>
#include "m_argv.h"
#include "track_stream.h"
#include "../platform/user_music.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

int myargc;
char **myargv;
int M_CheckParm(char *check)
{
    for (int i = 1; i < myargc; ++i)
	if (!SDL_strcasecmp(check, myargv[i])) return i;
    return 0;
}

static int failures;
#define CHECK(condition, ...) do { if (!(condition)) { ++failures; printf("FAIL: " __VA_ARGS__); printf("\n"); } } while (0)

/* A 16-bit PCM WAV of a sine at hz, seconds long. */
static void write_wav(const char *path, int rate, int channels, double seconds, double hz)
{
    FILE *file = fopen(path, "wb");
    if (!file) { printf("FAIL: can't write %s\n", path); ++failures; return; }
    int frames = (int)(rate * seconds), bytes = frames * channels * 2;
    unsigned char header[44];
    memcpy(header, "RIFF", 4);
    unsigned v = 36 + bytes; memcpy(header + 4, &v, 4);
    memcpy(header + 8, "WAVEfmt ", 8);
    v = 16; memcpy(header + 16, &v, 4);
    unsigned short s = 1; memcpy(header + 20, &s, 2);
    s = (unsigned short)channels; memcpy(header + 22, &s, 2);
    v = (unsigned)rate; memcpy(header + 24, &v, 4);
    v = (unsigned)(rate * channels * 2); memcpy(header + 28, &v, 4);
    s = (unsigned short)(channels * 2); memcpy(header + 32, &s, 2);
    s = 16; memcpy(header + 34, &s, 2);
    memcpy(header + 36, "data", 4);
    v = (unsigned)bytes; memcpy(header + 40, &v, 4);
    fwrite(header, 1, 44, file);
    for (int i = 0; i < frames; ++i)
	for (int c = 0; c < channels; ++c)
	{
	    short sample = (short)(16000 * sin(2 * M_PI * hz * i / rate));
	    fwrite(&sample, 2, 1, file);
	}
    fclose(file);
}

/* Mixes the track at 44.1 kHz until it ends or limit frames pass: the frames
   played, and the loudest sample. */
static int play(track_stream_t *track, int limit, float *peak)
{
    float buffer[512 * 2];
    int played = 0;
    *peak = 0;
    while (played < limit)
    {
	memset(buffer, 0, sizeof(buffer));
	int alive = Track_Mix(track, buffer, 512, 44100, 1.0f);
	for (int i = 0; i < 1024; ++i) if (fabsf(buffer[i]) > *peak) *peak = fabsf(buffer[i]);
	played += 512;
	if (!alive) break;
    }
    return played;
}

static void check_file(const char *path, double expect_seconds, float min_peak, float max_peak)
{
    track_stream_t *track = Track_Open(path);
    CHECK(track != NULL, "%s doesn't open", path);
    if (!track) return;
    Track_Restart(track, 0);
    float peak;
    int played = play(track, 44100 * 30, &peak);
    double seconds = played / 44100.0;
    CHECK(fabs(seconds - expect_seconds) < 0.1, "%s played %.3f s, expected %.3f", path, seconds, expect_seconds);
    CHECK(peak > min_peak && peak < max_peak, "%s peak %.3f", path, peak);
    /* Looping goes round without ending. */
    Track_Restart(track, 1);
    played = play(track, (int)(44100 * (expect_seconds * 2.5)), &peak);
    CHECK(played >= (int)(44100 * expect_seconds * 2.4), "%s stopped looping after %d frames", path, played);
    printf("%s: %d Hz, %.2f s\n", path, Track_Rate(track), expect_seconds);
    Track_Close(track);
}

static void write_text(const char *path, const char *text)
{
    FILE *file = fopen(path, "w");
    if (file) { fputs(text, file); fclose(file); }
}

int main(int argc, char **argv)
{
    myargc = argc;
    myargv = argv;
    char *base = SDL_GetPrefPath("DERP tests", "user-music");
    if (!base) { printf("FAIL: no temp folder\n"); return 1; }
    if (chdir(base)) { printf("FAIL: can't enter %s\n", base); return 1; }
    SDL_CreateDirectory("music");

    /* Streaming: a 48 kHz mono file comes out at 44.1 kHz, as long as it is. */
    write_wav("music/E1M1.wav", 48000, 1, 1.5, 220);
    write_wav("music/hangar.wav", 44100, 2, 2.0, 440);
    check_file("music/E1M1.wav", 1.5, 0.45f, 0.52f);   /* 16000/32768 */
    check_file("music/hangar.wav", 2.0, 0.45f, 0.52f);
    CHECK(Track_Open("music/missing.ogg") == NULL, "a missing file opened");
    CHECK(Track_Open("music/E1M1.txt") == NULL, "a .txt opened");

    /* Finding: a file named after the map. */
    char path[4096];
    CHECK(UserMusic_Find("E1M1", "e1m1", path, sizeof(path)) && strstr(path, "E1M1.wav"), "E1M1 not found by name (%s)", path);
    CHECK(UserMusic_Find("e1m1", NULL, path, sizeof(path)), "the map name minds its case");
    CHECK(!UserMusic_Find("E1M2", "e1m2", path, sizeof(path)), "E1M2 found with no track (%s)", path);
    CHECK(!UserMusic_Find(NULL, NULL, path, sizeof(path)), "nothing asked, something found");

    /* The playlist wins over names, by map and by song, comments and spaces allowed. */
    write_text("music/playlist.txt",
	"# tracks\n"
	"  e1m1 =  hangar.wav   # the first map\n"
	"D_INTER = hangar.wav\n"
	"E1M3 = gone.flac\n");
    CHECK(UserMusic_Find("E1M1", "e1m1", path, sizeof(path)) && strstr(path, "hangar.wav"), "playlist ignored for E1M1 (%s)", path);
    CHECK(UserMusic_Find(NULL, "inter", path, sizeof(path)) && strstr(path, "hangar.wav"), "song key D_INTER not found");
    CHECK(!UserMusic_Find("E1M3", "e1m3", path, sizeof(path)), "a playlist file that isn't there was used");

    /* A title finds a downloaded file by part of its name, whatever its
       number, artist or format; the first by name order wins; no match, none. */
    write_wav("music/Aubrey Hodges - 03 Retribution Dawns.wav", 44100, 2, 0.2, 440);
    write_wav("music/Aubrey Hodges - 12 Retribution Dawns (Alt).wav", 44100, 2, 0.2, 440);
    write_text("music/playlist.txt",
	"E1M2 = retribution dawns\n"
	"E1M4 = hangar\n"
	"E1M5 = The Broken Ones\n");
    CHECK(UserMusic_Find("E1M2", "e1m2", path, sizeof(path)) && strstr(path, "03 Retribution Dawns.wav"), "title not matched (%s)", path);
    CHECK(UserMusic_Find("E1M4", "e1m4", path, sizeof(path)) && strstr(path, "hangar.wav"), "name without extension not found (%s)", path);
    CHECK(!UserMusic_Find("E1M5", "e1m5", path, sizeof(path)), "a missing title matched (%s)", path);
    remove("music/Aubrey Hodges - 03 Retribution Dawns.wav");
    remove("music/Aubrey Hodges - 12 Retribution Dawns (Alt).wav");

    /* -nousermusic turns them all off. */
    char *off[] = {argv[0], "-nousermusic"};
    myargc = 2; myargv = off;
    CHECK(!UserMusic_Find("E1M1", "e1m1", path, sizeof(path)), "-nousermusic still found a track");
    myargc = argc; myargv = argv;

    remove("music/playlist.txt");
    remove("music/E1M1.wav");
    remove("music/hangar.wav");

    /* Any further files: other formats, each must play and loop. */
    for (int i = 1; i + 1 < argc; i += 2) check_file(argv[i], atof(argv[i + 1]), 0.01f, 1.01f);

    SDL_free(base);
    if (failures) { printf("%d failures\n", failures); return 1; }
    printf("user music: all checks passed\n");
    return 0;
}
