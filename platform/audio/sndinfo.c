/* SNDINFO reading for sndinfo.h. Line by line: `name sound`, `$alias name
   other`, `$random name { a b c }` (its braces may run over lines); other
   $ commands are skipped with their line (and braces). // and block
   comments are ignored, and a name or sound may be "quoted". */
#include "sndinfo.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define NAME_SIZE 64
#define SET_MOST 16

enum { KIND_SOUND, KIND_ALIAS, KIND_RANDOM };

typedef struct
{
    char name[NAME_SIZE];
    int kind, count;
    char target[SET_MOST][NAME_SIZE];   /* the sound, the alias's name, or the set */
} entry_t;

struct sndinfo_s
{
    entry_t *entries;
    int count, room;
};

sndinfo_t *SndInfo_New(void) { return calloc(1, sizeof(sndinfo_t)); }

void SndInfo_Free(sndinfo_t *info)
{
    if (!info) return;
    free(info->entries);
    free(info);
}

int SndInfo_Count(const sndinfo_t *info) { return info ? info->count : 0; }

static int same(const char *a, const char *b)
{
    for (; *a && *b; ++a, ++b)
	if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
    return *a == *b;
}

static entry_t *find(const sndinfo_t *info, const char *name)
{
    for (int i = 0; i < info->count; ++i)
	if (same(info->entries[i].name, name)) return &info->entries[i];
    return NULL;
}

/* The entry for name, new or replaced. */
static entry_t *define(sndinfo_t *info, const char *name, int kind)
{
    entry_t *e = find(info, name);
    if (!e)
    {
	if (info->count == info->room)
	{
	    int room = info->room ? info->room * 2 : 64;
	    entry_t *grown = realloc(info->entries, (size_t)room * sizeof(entry_t));
	    if (!grown) return NULL;
	    info->entries = grown; info->room = room;
	}
	e = &info->entries[info->count++];
    }
    memset(e, 0, sizeof(*e));
    strncpy(e->name, name, NAME_SIZE - 1);
    e->kind = kind;
    return e;
}

/* A sound file's name as it is looked up: no folder, no extension. */
static void sound_key(char *out, const char *path)
{
    const char *slash = strrchr(path, '/'), *back = strrchr(path, '\\');
    if (back > slash) slash = back;
    const char *base = slash ? slash + 1 : path;
    size_t n = 0;
    for (; base[n] && base[n] != '.' && n < NAME_SIZE - 1; ++n) out[n] = (char)toupper((unsigned char)base[n]);
    out[n] = 0;
}

typedef struct { const char *at, *end; } reader_t;

/* The next token on this line ("{" and "}" are tokens of their own); 0 at
   the line's end, which skip_line then passes. Comments are spaces. */
static int token(reader_t *r, char *out, size_t size, int cross_lines)
{
    for (;;)
    {
	while (r->at < r->end && (*r->at == ' ' || *r->at == '\t' || *r->at == '\r' || (cross_lines && *r->at == '\n'))) ++r->at;
	if (r->at + 1 < r->end && r->at[0] == '/' && r->at[1] == '/')
	{
	    while (r->at < r->end && *r->at != '\n') ++r->at;
	    continue;
	}
	if (r->at + 1 < r->end && r->at[0] == '/' && r->at[1] == '*')
	{
	    r->at += 2;
	    while (r->at + 1 < r->end && !(r->at[0] == '*' && r->at[1] == '/')) ++r->at;
	    r->at = r->at + 1 < r->end ? r->at + 2 : r->end;
	    continue;
	}
	break;
    }
    if (r->at >= r->end || *r->at == '\n') return 0;
    size_t n = 0;
    if (*r->at == '{' || *r->at == '}') out[n++] = *r->at++;
    else if (*r->at == '"')
    {
	for (++r->at; r->at < r->end && *r->at != '"' && *r->at != '\n'; ++r->at) if (n + 1 < size) out[n++] = *r->at;
	if (r->at < r->end && *r->at == '"') ++r->at;
    }
    else
	for (; r->at < r->end && !strchr(" \t\r\n{}\"", *r->at); ++r->at)
	{
	    if (r->at + 1 < r->end && r->at[0] == '/' && (r->at[1] == '/' || r->at[1] == '*')) break;
	    if (n + 1 < size) out[n++] = *r->at;
	}
    out[n] = 0;
    return 1;
}

static void skip_line(reader_t *r)
{
    while (r->at < r->end && *r->at != '\n') ++r->at;
    if (r->at < r->end) ++r->at;
}

/* Past a brace pair that opens on this line, if one does. */
static void skip_braces(reader_t *r)
{
    char word[NAME_SIZE];
    const char *at = r->at;
    while (token(r, word, sizeof(word), 0))
	if (!strcmp(word, "{"))
	{
	    while (token(r, word, sizeof(word), 1) && strcmp(word, "}")) {}
	    return;
	}
    r->at = at;
}

int SndInfo_Parse(sndinfo_t *info, const char *text, size_t length)
{
    if (!info || !text) return 0;
    reader_t r = { text, text + length };
    char word[NAME_SIZE], name[NAME_SIZE], other[NAME_SIZE];
    int read = 0;
    while (r.at < r.end)
    {
	if (!token(&r, word, sizeof(word), 0)) { skip_line(&r); continue; }
	if (word[0] == '$')
	{
	    if (same(word, "$random") && token(&r, name, sizeof(name), 0) && token(&r, other, sizeof(other), 1) && !strcmp(other, "{"))
	    {
		entry_t *e = define(info, name, KIND_RANDOM);
		while (token(&r, other, sizeof(other), 1) && strcmp(other, "}"))
		    if (e && e->count < SET_MOST) strncpy(e->target[e->count++], other, NAME_SIZE - 1);
		read += e != NULL;
	    }
	    else if (same(word, "$alias") && token(&r, name, sizeof(name), 0) && token(&r, other, sizeof(other), 0))
	    {
		entry_t *e = define(info, name, KIND_ALIAS);
		if (e) { strncpy(e->target[0], other, NAME_SIZE - 1); e->count = 1; ++read; }
	    }
	    else skip_braces(&r);
	    skip_line(&r);
	    continue;
	}
	if (token(&r, other, sizeof(other), 0) && strcmp(other, "{") && strcmp(other, "}"))
	{
	    entry_t *e = define(info, word, KIND_SOUND);
	    if (e) { sound_key(e->target[0], other); e->count = 1; ++read; }
	}
	skip_line(&r);
    }
    return read;
}

const char *SndInfo_Resolve(const sndinfo_t *info, const char *name, unsigned *seed)
{
    if (!info || !name) return NULL;
    static char key[NAME_SIZE];
    /* A set's member that isn't a name of its own is taken as a sound. */
    for (int depth = 0; depth < 8; ++depth)
    {
	const entry_t *e = find(info, name);
	if (!e)
	{
	    if (!depth) return NULL;
	    sound_key(key, name);
	    return key;
	}
	if (e->kind == KIND_SOUND) return e->target[0];
	if (!e->count) return NULL;
	int pick = 0;
	if (e->kind == KIND_RANDOM && e->count > 1)
	{
	    unsigned x = seed && *seed ? *seed : 0x9e3779b9u;
	    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
	    if (seed) *seed = x;
	    pick = (int)(x % (unsigned)e->count);
	}
	name = e->target[pick];
    }
    return NULL;
}
