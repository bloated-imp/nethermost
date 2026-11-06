/* Music tracks the player brings: recorded files that replace a map's or a
   screen's MIDI score. The game ships none of them. They are looked for in a
   `music` folder in the game's save folder (beside doom.cfg) and in one
   beside the game itself, in that order.

   A track is found by name, first through `music/playlist.txt`, then as a
   file named after the map or the song:

       # playlist.txt: a key, '=', then a file in this folder (or a full path)
       E1M1    = 02 Hangar.flac
       MAP01   = Entryway                  # any file whose name has this in it
       D_INTER = 30 Level Complete.mp3     # a song's lump name: screens too

   or simply music/E1M1.flac, music/D_INTER.ogg and so on. A playlist entry
   can be the file's whole name, its name without the extension, or just a
   part of it such as the track's title: the first file in the folder (in
   name order) whose name holds it is played, so downloaded files keep their
   numbers and artist names. Keys and names don't care about case. FLAC, Ogg
   Vorbis, MP3 and WAV are read. A map without a track plays the WAD's own
   score as before; -nousermusic ignores them all. */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <SDL3/SDL.h>
#include "m_argv.h"
#include "user_music.h"

static const char *const extensions[] = {".flac", ".ogg", ".mp3", ".wav"};

static int same(const char *a, const char *b)
{
    for (; *a && *b; ++a, ++b)
	if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
    return *a == *b;
}

static int readable(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (file) fclose(file);
    return file != NULL;
}

static char *trim(char *s)
{
    while (*s && isspace((unsigned char)*s)) ++s;
    char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) --end;
    *end = 0;
    return s;
}

static int absolute(const char *path)
{
    return path[0] == '/' || path[0] == '\\' || (path[0] && path[1] == ':');
}

static int playable(const char *name)
{
    size_t n = strlen(name);
    for (size_t e = 0; e < sizeof(extensions) / sizeof(*extensions); ++e)
    {
	size_t m = strlen(extensions[e]);
	if (n > m && same(name + n - m, extensions[e])) return 1;
    }
    return 0;
}

static int contains(const char *haystack, const char *needle)
{
    size_t n = strlen(needle);
    for (const char *h = haystack; *h; ++h)
    {
	size_t i = 0;
	while (i < n && h[i] && tolower((unsigned char)h[i]) == tolower((unsigned char)needle[i])) ++i;
	if (i == n) return 1;
    }
    return 0;
}

/* A playlist entry's file: as written, with an extension added, or the first
   playable file in folder (by name) whose name holds it. */
static int resolve(const char *folder, const char *target, char *path, size_t size)
{
    if (absolute(target)) snprintf(path, size, "%s", target);
    else snprintf(path, size, "%s/%s", folder, target);
    if (readable(path) && playable(path)) return 1;
    if (absolute(target)) return 0;
    for (size_t e = 0; e < sizeof(extensions) / sizeof(*extensions); ++e)
    {
	snprintf(path, size, "%s/%s%s", folder, target, extensions[e]);
	if (readable(path)) return 1;
    }
    int count = 0, found = 0;
    char **names = SDL_GlobDirectory(folder, "*", SDL_GLOB_CASEINSENSITIVE, &count);
    const char *first = NULL;
    for (int i = 0; names && i < count; ++i)
	if (playable(names[i]) && contains(names[i], target) && (!first || SDL_strcasecmp(names[i], first) < 0))
	    first = names[i];
    if (first)
    {
	snprintf(path, size, "%s/%s", folder, first);
	found = readable(path);
    }
    SDL_free(names);
    return found;
}

/* The playlist's file for key, in folder. */
static int from_playlist(const char *folder, const char *key, char *path, size_t size)
{
    char list[4200], line[1024];
    snprintf(list, sizeof(list), "%s/playlist.txt", folder);
    FILE *file = fopen(list, "r");
    if (!file) return 0;
    int found = 0;
    while (!found && fgets(line, sizeof(line), file))
    {
	char *hash = strchr(line, '#');
	if (hash) *hash = 0;
	char *equals = strchr(line, '=');
	if (!equals) continue;
	*equals = 0;
	char *name = trim(line), *target = trim(equals + 1);
	if (!*target || !same(name, key)) continue;
	found = resolve(folder, target, path, size);
	if (!found) fprintf(stderr, "Music: no file in %s matches \"%s\" (for %s).\n", folder, target, key);
    }
    fclose(file);
    return found;
}

static int named_file(const char *folder, const char *key, char *path, size_t size)
{
    for (size_t e = 0; e < sizeof(extensions) / sizeof(*extensions); ++e)
    {
	/* Try the key as written and in upper and lower case: Linux minds. */
	char upper[64], lower[64];
	size_t n = strlen(key) < sizeof(upper) - 1 ? strlen(key) : sizeof(upper) - 1;
	for (size_t i = 0; i < n; ++i)
	{
	    upper[i] = (char)toupper((unsigned char)key[i]);
	    lower[i] = (char)tolower((unsigned char)key[i]);
	}
	upper[n] = lower[n] = 0;
	const char *spellings[] = {upper, lower};
	for (int s = 0; s < 2; ++s)
	{
	    snprintf(path, size, "%s/%s%s", folder, spellings[s], extensions[e]);
	    if (readable(path)) return 1;
	}
    }
    return 0;
}

int UserMusic_Find(const char *map, const char *song, char *path, size_t size)
{
    if (M_CheckParm("-nousermusic")) return 0;
    char folders[2][4096];
    int count = 0;
    snprintf(folders[count++], sizeof(folders[0]), "music");
    const char *base = SDL_GetBasePath();
    if (base) snprintf(folders[count++], sizeof(folders[0]), "%smusic", base);

    char lump[16] = "";
    if (song && *song) snprintf(lump, sizeof(lump), "D_%s", song);
    const char *keys[2] = {map && *map ? map : NULL, *lump ? lump : NULL};
    /* The playlist first (in every folder), then files named by key. */
    for (int f = 0; f < count; ++f)
	for (int k = 0; k < 2; ++k)
	    if (keys[k] && from_playlist(folders[f], keys[k], path, size)) return 1;
    for (int f = 0; f < count; ++f)
	for (int k = 0; k < 2; ++k)
	    if (keys[k] && named_file(folders[f], keys[k], path, size)) return 1;
    return 0;
}
