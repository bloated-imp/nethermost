/* Sound effects the player brings: recorded files that replace the WAD's
   own sounds, such as a PlayStation sound pack. The game ships none of them.
   They are looked for in a `sounds` folder in the game's save folder (beside
   doom.cfg) and in one beside the game itself.

   A sound is a file named after the WAD's sound lump: sounds/DSPISTOL.ogg
   replaces the pistol. FLAC, Ogg Vorbis, MP3 and WAV are read, at any rate.
   A ZDoom-style pack (.pk3 or .zip) can go in the folder as it is: its
   sounds/ folder is read, and its SNDINFO, which gives single monsters
   sounds of their own (imp/pain = DSBGPAI2; see Game_SoundName for the
   names DOOM's monsters answer to).

   Which wins: a loose file always beats one in a pack, so a better recording
   of one sound only needs dropping into the folder beside the pack; the save
   folder's beat those beside the game; packs go in name order, the first
   holding a sound giving it. SNDINFO files are read in the opposite order,
   so the winning one's definitions are read last and stand: a loose
   sounds/SNDINFO.txt can change any name a pack gives. -nousersounds (or F4
   Sound, Supplied sound files) ignores them all. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
/* DOOM's headers before SDL's: its boolean is an enum of false and true. */
#include "doomdef.h"
#include "m_argv.h"
#include "w_wad.h"
#include "z_zone.h"
#include <SDL3/SDL.h>
#include "audio/sndinfo.h"
#include "audio/track_stream.h"
#include "audio/zip_read.h"
#include "user_sounds.h"

#define KEY_SIZE 64
#define MOST_ENTRIES 4096

typedef struct
{
    char key[KEY_SIZE];     /* the file's name without folder or extension, upper case */
    char path[1024];        /* a loose file */
    int archive;            /* else the pack holding it */
    zip_entry_t entry;
    int tried;              /* decoded (or failed) already */
    float *pcm;
    int frames, rate;
} source_t;

typedef struct
{
    unsigned char *data;
    size_t size;
    char name[256];
} archive_t;

static source_t *sources;
static int source_count, source_room;
static archive_t archives[64];
static int archive_count;
static sndinfo_t *info;
static unsigned seed = 0x2545f491u;
static int ready;

static void make_key(char *key, const char *name)
{
    const char *slash = strrchr(name, '/');
    const char *base = slash ? slash + 1 : name;
    size_t n = 0;
    for (; base[n] && base[n] != '.' && n < KEY_SIZE - 1; ++n) key[n] = (char)toupper((unsigned char)base[n]);
    key[n] = 0;
}

static source_t *find(const char *key)
{
    char want[KEY_SIZE];
    make_key(want, key);
    for (int i = 0; i < source_count; ++i)
	if (!strcmp(sources[i].key, want)) return &sources[i];
    return NULL;
}

/* A new source, unless an earlier (winning) one has its name. */
static source_t *add(const char *name)
{
    char key[KEY_SIZE];
    make_key(key, name);
    if (!*key || find(key)) return NULL;
    if (source_count == source_room)
    {
	int room = source_room ? source_room * 2 : 256;
	source_t *grown = realloc(sources, (size_t)room * sizeof(source_t));
	if (!grown) return NULL;
	sources = grown; source_room = room;
    }
    source_t *s = &sources[source_count++];
    memset(s, 0, sizeof(*s));
    strcpy(s->key, key);
    s->archive = -1;
    return s;
}

static int has_extension(const char *name, const char *const *list)
{
    const char *dot = strrchr(name, '.');
    if (!dot) return 0;
    for (; *list; ++list)
	if (!SDL_strcasecmp(dot, *list)) return 1;
    return 0;
}

static const char *const audio_types[] = {".ogg", ".flac", ".mp3", ".wav", NULL};
static const char *const pack_types[] = {".pk3", ".zip", NULL};

/* SNDINFO or SNDINFO.txt (any extension), at a pack's top level or loose in the folder. */
static int is_sndinfo(const char *name)
{
    return !strchr(name, '/') && !SDL_strncasecmp(name, "SNDINFO", 7) && (!name[7] || name[7] == '.');
}

static int compare_names(const void *a, const void *b)
{
    return SDL_strcasecmp(*(char *const *)a, *(char *const *)b);
}

/* The folder's file names, in name order (SDL_free the list). */
static char **listing(const char *folder, int *count)
{
    char **names = SDL_GlobDirectory(folder, "*", SDL_GLOB_CASEINSENSITIVE, count);
    if (names) qsort(names, (size_t)*count, sizeof(char *), compare_names);
    return names;
}

static void read_sndinfo(const char *text, size_t length, const char *where)
{
    int read = SndInfo_Parse(info, text, length);
    fprintf(stderr, "Sounds: SNDINFO in %s, %d names.\n", where, read);
}

static int decoded(source_t *s, user_sound_t *out);

/* -checksounds: every supplied sound decoded at once, the ones that can't
   be named, so a pack or a new file can be checked without playing it all. */
static void check_all(void)
{
    int good = 0;
    double seconds = 0;
    for (int i = 0; i < source_count; ++i)
    {
	user_sound_t u;
	if (decoded(&sources[i], &u)) { ++good; seconds += (double)u.length / u.rate; }
    }
    fprintf(stderr, "Sounds: %d of %d decode, %.1f seconds in all.\n", good, source_count, seconds);
}

void UserSounds_Init(void)
{
    if (ready) return;
    ready = 1;
    info = SndInfo_New();
    if (M_CheckParm("-nousersounds") || !info) return;
    char folders[2][1024];
    int folder_count = 0;
    snprintf(folders[folder_count++], sizeof(folders[0]), "sounds");
    const char *base = SDL_GetBasePath();
    if (base) snprintf(folders[folder_count++], sizeof(folders[0]), "%ssounds", base);

    /* Loose files first: they win. */
    int loose = 0;
    for (int f = 0; f < folder_count; ++f)
    {
	int count = 0;
	char **names = listing(folders[f], &count);
	for (int i = 0; names && i < count; ++i)
	{
	    if (!has_extension(names[i], audio_types)) continue;
	    source_t *s = add(names[i]);
	    if (s) { snprintf(s->path, sizeof(s->path), "%s/%s", folders[f], names[i]); ++loose; }
	}
	SDL_free(names);
    }
    /* Then the packs, in folder and then name order. */
    for (int f = 0; f < folder_count; ++f)
    {
	int count = 0;
	char **names = listing(folders[f], &count);
	for (int i = 0; names && i < count && archive_count < (int)SDL_arraysize(archives); ++i)
	{
	    if (!has_extension(names[i], pack_types)) continue;
	    char path[1024];
	    snprintf(path, sizeof(path), "%s/%s", folders[f], names[i]);
	    size_t size = 0;
	    unsigned char *data = SDL_LoadFile(path, &size);
	    static zip_entry_t entries[MOST_ENTRIES];
	    int found = data ? Zip_List(data, size, entries, MOST_ENTRIES) : -1;
	    if (found < 0) { fprintf(stderr, "Sounds: %s is not a pack that can be read.\n", path); SDL_free(data); continue; }
	    archive_t *a = &archives[archive_count];
	    a->data = data; a->size = size;
	    snprintf(a->name, sizeof(a->name), "%s", path);
	    int added = 0;
	    for (int e = 0; e < found; ++e)
	    {
		if (SDL_strncasecmp(entries[e].name, "sounds/", 7)) continue;
		source_t *s = add(entries[e].name);
		if (s) { s->archive = archive_count; s->entry = entries[e]; ++added; }
	    }
	    fprintf(stderr, "Sounds: %d from %s.\n", added, path);
	    ++archive_count;
	}
	SDL_free(names);
    }
    if (loose) fprintf(stderr, "Sounds: %d loose files.\n", loose);

    /* SNDINFO, losing first: the packs from last to first, then the loose
       ones, the game's folder before the save folder. */
    for (int a = archive_count - 1; a >= 0; --a)
    {
	static zip_entry_t entries[MOST_ENTRIES];
	int found = Zip_List(archives[a].data, archives[a].size, entries, MOST_ENTRIES);
	for (int e = 0; e < found; ++e)
	{
	    if (!is_sndinfo(entries[e].name)) continue;
	    size_t length = 0;
	    unsigned char *text = Zip_Read(archives[a].data, archives[a].size, &entries[e], &length);
	    if (text) read_sndinfo((const char *)text, length, archives[a].name);
	    free(text);
	}
    }
    for (int f = folder_count - 1; f >= 0; --f)
    {
	int count = 0;
	char **names = listing(folders[f], &count);
	for (int i = 0; names && i < count; ++i)
	{
	    if (!is_sndinfo(names[i])) continue;
	    char path[1024];
	    snprintf(path, sizeof(path), "%s/%s", folders[f], names[i]);
	    size_t length = 0;
	    char *text = SDL_LoadFile(path, &length);
	    if (text) read_sndinfo(text, length, path);
	    SDL_free(text);
	}
	SDL_free(names);
    }
    if (M_CheckParm("-checksounds")) check_all();
}

/* Its samples, decoded on first use and kept. */
static int decoded(source_t *s, user_sound_t *out)
{
    if (!s->tried)
    {
	s->tried = 1;
	size_t size = 0;
	unsigned char *bytes = NULL;
	if (s->archive >= 0)
	{
	    const archive_t *a = &archives[s->archive];
	    bytes = Zip_Read(a->data, a->size, &s->entry, &size);
	}
	else bytes = SDL_LoadFile(s->path, &size);
	if (bytes && !Track_DecodeMemory(bytes, size, &s->pcm, &s->frames, &s->rate))
	    fprintf(stderr, "Sounds: %s (%s) is no sound file that can be read.\n", s->key,
		    s->archive >= 0 ? archives[s->archive].name : s->path);
	if (s->archive >= 0) free(bytes); else SDL_free(bytes);
    }
    if (!s->pcm || s->frames <= 0 || s->rate <= 0) return 0;
    out->pcm = s->pcm; out->dmx = NULL; out->length = s->frames; out->rate = s->rate;
    return 1;
}

int UserSounds_Lump(const char *lump, user_sound_t *out)
{
    UserSounds_Init();
    source_t *s = lump ? find(lump) : NULL;
    return s && decoded(s, out);
}

/* The WAD's own sound lump of that name. */
static int wad_lump(const char *name, user_sound_t *out)
{
    if (strlen(name) > 8) return 0;
    int lump = W_CheckNumForName((char *)name);
    if (lump < 0) return 0;
    int length = W_LumpLength(lump);
    const unsigned char *data = W_CacheLumpNum(lump, PU_STATIC);
    if (length < 8 || (data[0] | data[1] << 8) != 3) return 0;
    unsigned size = (unsigned)data[4] | (unsigned)data[5] << 8 | (unsigned)data[6] << 16 | (unsigned)data[7] << 24;
    if (!size || size > (unsigned)(length - 8)) return 0;
    out->pcm = NULL; out->dmx = data + 8; out->length = (int)size; out->rate = data[2] | data[3] << 8;
    return 1;
}

int UserSounds_Named(const char *name, user_sound_t *out)
{
    UserSounds_Init();
    const char *sound = SndInfo_Resolve(info, name, &seed);
    if (!sound) return 0;
    return UserSounds_Lump(sound, out) || wad_lump(sound, out);
}
