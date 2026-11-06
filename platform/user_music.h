/* Music tracks the player supplies (user_music.c). */
#ifndef DOOM_USER_MUSIC_H
#define DOOM_USER_MUSIC_H
#include <stddef.h>

/* map: the map lump (E1M1, MAP01) or NULL off a map; song: the music's name
   without its D_ (E1M1, INTER). Writes the track's path; 0 when there is none. */
int UserMusic_Find(const char *map, const char *song, char *path, size_t size);
#endif
