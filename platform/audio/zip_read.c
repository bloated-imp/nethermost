/* ZIP reading for zip_read.h: the central directory, then each entry's
   local header and data, inflated by stb_image's zlib decoder. */
#include "zip_read.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Only its zlib inflate is used, kept private to this file. */
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_NO_FAILURE_STRINGS
#include "stb_image.h"

static uint32_t le16(const unsigned char *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8; }
static uint32_t le32(const unsigned char *p) { return le16(p) | le16(p + 2) << 16; }

int Zip_List(const unsigned char *data, size_t size, zip_entry_t *entries, int most)
{
    /* The end record: 22 bytes plus a comment of up to 64K, at the very end. */
    if (!data || size < 22) return -1;
    size_t end = size - 22, stop = size > 22 + 65535 ? size - 22 - 65535 : 0;
    for (;; --end)
    {
	if (le32(data + end) == 0x06054b50) break;
	if (end == stop) return -1;
    }
    uint32_t count = le16(data + end + 10), at = le32(data + end + 16);
    int found = 0;
    for (uint32_t i = 0; i < count; ++i)
    {
	if ((size_t)at + 46 > size || le32(data + at) != 0x02014b50) return found;
	const unsigned char *h = data + at;
	uint32_t method = le16(h + 10), packed = le32(h + 20), whole = le32(h + 24);
	uint32_t namelen = le16(h + 28), extra = le16(h + 30), comment = le16(h + 32), local = le32(h + 42);
	if ((size_t)at + 46 + namelen > size) return found;
	at += 46 + namelen + extra + comment;
	/* Folders end in '/'; encrypted entries (flag bit 0) can't be read. */
	if (!namelen || h[46 + namelen - 1] == '/' || (le16(h + 8) & 1) || (method != 0 && method != 8)) continue;
	if (found == most || namelen >= sizeof(entries[0].name)) continue;
	zip_entry_t *e = &entries[found++];
	memcpy(e->name, h + 46, namelen);
	e->name[namelen] = 0;
	for (char *c = e->name; *c; ++c) if (*c == '\\') *c = '/';
	e->offset = local; e->packed = packed; e->size = whole; e->method = (int)method;
    }
    return found;
}

unsigned char *Zip_Read(const unsigned char *data, size_t size, const zip_entry_t *entry, size_t *length)
{
    size_t at = entry->offset;
    if (at + 30 > size || le32(data + at) != 0x04034b50) return NULL;
    at += 30 + le16(data + at + 26) + le16(data + at + 28);
    if (at > size || entry->packed > size - at || entry->size > 256u << 20) return NULL;
    unsigned char *out;
    if (entry->method == 0)
    {
	if (entry->packed != entry->size) return NULL;
	out = malloc(entry->size ? entry->size : 1);
	if (out) memcpy(out, data + at, entry->size);
    }
    else
    {
	out = malloc(entry->size ? entry->size : 1);
	if (!out) return NULL;
	int got = stbi_zlib_decode_noheader_buffer((char *)out, (int)entry->size, (const char *)data + at, (int)entry->packed);
	if (got != (int)entry->size) { free(out); return NULL; }
    }
    if (out && length) *length = entry->size;
    return out;
}
