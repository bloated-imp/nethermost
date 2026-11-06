/* Reading files out of a ZIP archive held in memory (a .pk3 or .zip of sounds
   the player supplies, user_sounds.c). Stored and deflated entries; no
   encryption, no ZIP64, no spanning. */
#ifndef DOOM_AUDIO_ZIP_READ_H
#define DOOM_AUDIO_ZIP_READ_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char name[256];          /* the path inside the archive, '/' between folders */
    size_t offset;           /* of its local header */
    size_t packed, size;     /* compressed and whole sizes */
    int method;              /* 0 stored, 8 deflated */
} zip_entry_t;

/* The archive's files (not its folders), from its central directory, into
   entries (at most most). Returns how many, or -1 when data is no ZIP. */
int Zip_List(const unsigned char *data, size_t size, zip_entry_t *entries, int most);
/* One entry's contents, malloc'd (free() it); NULL when it can't be read. */
unsigned char *Zip_Read(const unsigned char *data, size_t size, const zip_entry_t *entry, size_t *length);

#ifdef __cplusplus
}
#endif
#endif
